/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_net.h"
#include "br_netif_mock.h"
#include "br_shell.h"

#define PEER_IP    BR_IP4(192, 168, 7, 1)
#define LOCAL_IP   BR_IP4(192, 168, 7, 2)

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static bool str_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint8_t  pattern[2000];
static uint8_t  readback[2000];

static void fill_pattern(void)
{
    for (size_t i = 0; i < sizeof(pattern); i++) {
        pattern[i] = (uint8_t)(i * 13 + 5);
    }
}

static void run_init_tests(void)
{
    br_uart_puts("Test 1: Stack start\n");

    print_result(br_net_init() == BR_OK, "Test 1a: init");
    print_result(br_net_init() == BR_ERR_BUSY, "Test 1b: second init is rejected");

    br_net_ifinfo_t info;
    print_result(br_net_ifinfo(0, &info) == BR_OK && str_equal(info.name, "lo0") &&
                 info.ip == BR_IP4(127, 0, 0, 1) && info.up,
                 "Test 1c: the loopback interface exists");
    print_result(br_net_ifinfo(9, &info) == BR_ERR_NOENT, "Test 1d: unknown interface index");
}

static void run_udp_tests(void)
{
    br_uart_puts("Test 2: UDP over loopback\n");

    int server = br_net_socket(BR_NET_DGRAM);
    int client = br_net_socket(BR_NET_DGRAM);
    print_result(server >= 0 && client >= 0, "Test 2a: create sockets");

    print_result(br_net_bind(server, BR_IP4(127, 0, 0, 1), 7000) == BR_OK, "Test 2b: bind");

    int dup = br_net_socket(BR_NET_DGRAM);
    print_result(br_net_bind(dup, BR_IP4(127, 0, 0, 1), 7000) == BR_ERR_BUSY,
                 "Test 2c: binding a used port is rejected");
    br_net_close(dup);

    br_net_bind(client, BR_IP4(127, 0, 0, 1), 7001);

    print_result(br_net_sendto(client, "hello", 5, BR_IP4(127, 0, 0, 1), 7000) == 5,
                 "Test 2d: send");

    char buf[16];
    uint32_t from_ip = 0;
    uint16_t from_port = 0;
    int n = br_net_recvfrom(server, buf, sizeof(buf), BR_MSEC(500), &from_ip, &from_port);
    print_result(n == 5 && buf[0] == 'h' && buf[4] == 'o' &&
                 from_ip == BR_IP4(127, 0, 0, 1) && from_port == 7001,
                 "Test 2e: receive with the sender address");

    br_net_sendto(server, "reply", 5, from_ip, from_port);
    n = br_net_recv(client, buf, sizeof(buf), BR_MSEC(500));
    print_result(n == 5 && buf[0] == 'r', "Test 2f: the reply arrives");

    br_net_sendto(client, "0123456789", 10, BR_IP4(127, 0, 0, 1), 7000);
    n = br_net_recv(server, buf, 4, BR_MSEC(500));
    print_result(n == 4 && br_net_recv(server, buf, sizeof(buf), BR_MSEC(50)) == BR_ERR_TIMEOUT,
                 "Test 2g: a short buffer cuts the datagram");

    print_result(br_net_recv(server, buf, sizeof(buf), BR_MSEC(50)) == BR_ERR_TIMEOUT,
                 "Test 2h: receive times out");

    br_net_close(server);
    br_net_close(client);
}

static uint8_t stack_echo[2048];
static volatile int echo_fd = -1;

static void tcp_echo_task(void *arg)
{
    (void)arg;

    int conn = br_net_accept(echo_fd, BR_SEC(5), NULL, NULL);
    if (conn < 0) {
        return;
    }

    uint8_t buf[200];
    int n;
    while ((n = br_net_recv(conn, buf, sizeof(buf), BR_SEC(5))) > 0) {
        size_t sent = 0;
        while (sent < (size_t)n) {
            int w = br_net_send(conn, buf + sent, (size_t)n - sent);
            if (w <= 0) {
                break;
            }
            sent += (size_t)w;
        }
    }
    br_net_close(conn);
    br_task_suspend(br_task_self());
}

static void run_tcp_tests(void)
{
    br_uart_puts("Test 3: TCP over loopback\n");

    fill_pattern();

    int listener = br_net_socket(BR_NET_STREAM);
    print_result(br_net_bind(listener, BR_IP4(127, 0, 0, 1), 7002) == BR_OK &&
                 br_net_listen(listener, 1) == BR_OK,
                 "Test 3a: listen");
    echo_fd = listener;

    br_tid_t tid;
    br_task_create(&tid, "echo", tcp_echo_task, NULL, 4, stack_echo, sizeof(stack_echo));

    int client = br_net_socket(BR_NET_STREAM);
    print_result(br_net_connect(client, BR_IP4(127, 0, 0, 1), 7002) == BR_OK,
                 "Test 3b: connect");

    bool sent_ok = true;
    for (size_t off = 0; off < sizeof(pattern) && sent_ok; off += 400) {
        sent_ok = br_net_send(client, pattern + off, 400) == 400;
    }
    print_result(sent_ok, "Test 3c: send 2000 bytes");

    size_t got = 0;
    while (got < sizeof(readback)) {
        int n = br_net_recv(client, readback + got, sizeof(readback) - got, BR_SEC(5));
        if (n <= 0) {
            break;
        }
        got += (size_t)n;
    }
    bool same = got == sizeof(pattern);
    for (size_t i = 0; same && i < got; i++) {
        same = readback[i] == pattern[i];
    }
    print_result(same, "Test 3d: the echo is complete and correct");

    br_net_close(client);
    br_sleep_ms(100);
    br_net_close(listener);

    int bad = br_net_socket(BR_NET_STREAM);
    print_result(br_net_connect(bad, BR_IP4(127, 0, 0, 1), 7099) != BR_OK,
                 "Test 3e: connecting to a closed port fails");
    br_net_close(bad);
}

static void run_limit_tests(void)
{
    br_uart_puts("Test 4: Limits\n");

    int fds[BR_NET_MAX_SOCKETS];
    int n = 0;
    while (n < BR_NET_MAX_SOCKETS) {
        fds[n] = br_net_socket(BR_NET_DGRAM);
        if (fds[n] < 0) {
            break;
        }
        n++;
    }
    print_result(n == BR_NET_MAX_SOCKETS && br_net_socket(BR_NET_DGRAM) == BR_ERR_NOMEM,
                 "Test 4a: the socket table is limited");
    for (int i = 0; i < n; i++) {
        br_net_close(fds[i]);
    }

    char c;
    print_result(br_net_socket(7) == BR_ERR_INVALID && br_net_recv(-1, &c, 1, 0) == BR_ERR_INVALID &&
                 br_net_close(99) == BR_ERR_INVALID && br_net_send(0, &c, 1) == BR_ERR_INVALID,
                 "Test 4b: bad arguments are rejected");
}

static br_netif_mock_t mock;

static const uint8_t local_mac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
static const uint8_t peer_mac[6]  = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

static uint16_t checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2) {
        sum += (uint32_t)((data[i] << 8) | data[i + 1]);
    }
    if (len & 1) {
        sum += (uint32_t)(data[len - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static size_t build_arp_request(uint8_t *f)
{
    static const uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    for (int i = 0; i < 6; i++) {
        f[i] = bcast[i];
        f[6 + i] = peer_mac[i];
    }
    f[12] = 0x08; f[13] = 0x06;
    f[14] = 0; f[15] = 1; f[16] = 0x08; f[17] = 0; f[18] = 6; f[19] = 4; f[20] = 0; f[21] = 1;
    for (int i = 0; i < 6; i++) {
        f[22 + i] = peer_mac[i];
        f[32 + i] = 0;
    }
    f[28] = 192; f[29] = 168; f[30] = 7; f[31] = 1;
    f[38] = 192; f[39] = 168; f[40] = 7; f[41] = 2;
    return 42;
}

static size_t build_echo_request(uint8_t *f)
{
    for (int i = 0; i < 6; i++) {
        f[i] = local_mac[i];
        f[6 + i] = peer_mac[i];
    }
    f[12] = 0x08; f[13] = 0x00;
    uint8_t *ip = f + 14;
    ip[0] = 0x45; ip[1] = 0; ip[2] = 0; ip[3] = 28; ip[4] = 0x12; ip[5] = 0x34;
    ip[6] = 0; ip[7] = 0; ip[8] = 64; ip[9] = 1; ip[10] = 0; ip[11] = 0;
    ip[12] = 192; ip[13] = 168; ip[14] = 7; ip[15] = 1;
    ip[16] = 192; ip[17] = 168; ip[18] = 7; ip[19] = 2;
    uint16_t c = checksum(ip, 20);
    ip[10] = (uint8_t)(c >> 8); ip[11] = (uint8_t)c;
    uint8_t *icmp = ip + 20;
    icmp[0] = 8; icmp[1] = 0; icmp[2] = 0; icmp[3] = 0;
    icmp[4] = 0xAB; icmp[5] = 0xCD; icmp[6] = 0; icmp[7] = 9;
    c = checksum(icmp, 8);
    icmp[2] = (uint8_t)(c >> 8); icmp[3] = (uint8_t)c;
    return 14 + 28;
}

static int take_frame(uint8_t *buf, size_t size, br_time_t wait)
{
    br_time_t start = br_uptime_us();
    while (br_uptime_us() - start < wait) {
        int n = br_netif_mock_take(&mock, buf, size);
        if (n != 0) {
            return n;
        }
        br_sleep_ms(5);
    }
    return 0;
}

static uint8_t frame[BR_NETIF_FRAME_MAX];

static void run_wire_tests(void)
{
    br_uart_puts("Test 5: Ethernet interface\n");

    br_netif_mock_init(&mock, "eth0", local_mac);
    print_result(br_net_netif_add(&mock.netif, LOCAL_IP, BR_IP4(255, 255, 255, 0), PEER_IP) == BR_OK,
                 "Test 5a: add an interface with a static address");

    br_net_ifinfo_t info;
    bool found = false;
    for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
        if (str_equal(info.name, "eth0")) {
            found = info.ip == LOCAL_IP && info.gw == PEER_IP && info.mac[5] == 2 && info.up;
            break;
        }
    }
    print_result(found, "Test 5b: interface information");

    br_sleep_ms(50);
    while (br_netif_mock_take(&mock, frame, sizeof(frame)) != 0) { }

    size_t len = build_arp_request(frame);
    br_netif_mock_inject(&mock, frame, len);
    int n = take_frame(frame, sizeof(frame), BR_MSEC(500));
    print_result(n >= 42 && frame[12] == 0x08 && frame[13] == 0x06 && frame[21] == 2 &&
                 frame[22] == local_mac[0] && frame[27] == 2 &&
                 frame[28] == 192 && frame[31] == 2 &&
                 frame[0] == peer_mac[0] && frame[5] == peer_mac[5],
                 "Test 5c: an ARP request for our address is answered");

    len = build_echo_request(frame);
    br_netif_mock_inject(&mock, frame, len);
    n = take_frame(frame, sizeof(frame), BR_MSEC(500));
    print_result(n >= 42 && frame[12] == 0x08 && frame[13] == 0x00 && frame[23] == 1 &&
                 frame[34] == 0 && frame[38] == 0xAB && frame[39] == 0xCD &&
                 frame[26] == 192 && frame[29] == 2 && frame[0] == peer_mac[0],
                 "Test 5d: an ICMP echo request is answered");
}

static uint8_t stack_peer[2048];

static void peer_task(void *arg)
{
    (void)arg;

    static uint8_t f[BR_NETIF_FRAME_MAX];

    while (1) {
        int n = br_netif_mock_take(&mock, f, sizeof(f));
        if (n < 42) {
            br_sleep_ms(2);
            continue;
        }

        bool is_ip = f[12] == 0x08 && f[13] == 0x00 && f[23] == 1 && f[34] == 8;
        bool to_peer = f[30] == 192 && f[31] == 168 && f[32] == 7 && f[33] == 1;
        if (!is_ip || !to_peer) {
            continue;
        }

        for (int i = 0; i < 6; i++) {
            f[i] = local_mac[i];
            f[6 + i] = peer_mac[i];
        }
        for (int i = 0; i < 4; i++) {
            uint8_t t = f[26 + i];
            f[26 + i] = f[30 + i];
            f[30 + i] = t;
        }
        size_t icmp_len = (size_t)n - 34;
        f[34] = 0;
        f[36] = 0;
        f[37] = 0;
        uint16_t c = checksum(f + 34, icmp_len);
        f[36] = (uint8_t)(c >> 8);
        f[37] = (uint8_t)c;
        br_netif_mock_inject(&mock, f, (size_t)n);
    }
}

static void run_ping_tests(void)
{
    br_uart_puts("Test 6: Ping\n");

    br_tid_t tid;
    br_task_create(&tid, "peer", peer_task, NULL, 1, stack_peer, sizeof(stack_peer));

    uint32_t rtt = 0;
    print_result(br_net_ping(BR_IP4(127, 0, 0, 1), BR_MSEC(500), &rtt) == BR_OK,
                 "Test 6a: ping the loopback address");

    br_err_t err = br_net_ping(PEER_IP, BR_SEC(2), &rtt);
    print_result(err == BR_OK && rtt > 0, "Test 6b: ping a peer through the interface");

    print_result(br_net_ping(BR_IP4(192, 168, 7, 99), BR_MSEC(300), &rtt) == BR_ERR_TIMEOUT,
                 "Test 6c: ping of a silent address times out");

    br_task_suspend(tid);
}

static void run_shell_tests(void)
{
    br_uart_puts("Test 7: Shell commands\n");

    br_shell_register_net();

    char ifconfig_line[] = "ifconfig";
    print_result(br_shell_execute(ifconfig_line) == 0, "Test 7a: ifconfig");

    char ping_line[] = "ping 127.0.0.1 2";
    print_result(br_shell_execute(ping_line) == 0, "Test 7b: ping");

    char bad_ip[] = "ping 300.1.1.1";
    char bad_count[] = "ping 127.0.0.1 0";
    char no_arg[] = "ping";
    char short_ip[] = "ping 1.2.3";
    print_result(br_shell_execute(bad_ip) == -1 && br_shell_execute(bad_count) == -1 &&
                 br_shell_execute(no_arg) == -1 && br_shell_execute(short_ip) == -1,
                 "Test 7c: bad ping arguments are rejected");

    char silent[] = "ping 192.168.7.99 1";
    print_result(br_shell_execute(silent) == -1, "Test 7d: ping of a silent address fails");
}

static uint8_t stack_supervisor[3072];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Network Test ===\n\n");

    run_init_tests();
    run_udp_tests();
    run_tcp_tests();
    run_limit_tests();
    run_wire_tests();
    run_ping_tests();
    run_shell_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Network Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

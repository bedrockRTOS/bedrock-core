/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_net.h"
#include "br_netif_mock.h"
#include "br_shell.h"

#define PEER_IP    BR_IP4(192, 168, 7, 1)
#define LOCAL_IP   BR_IP4(192, 168, 7, 2)
#define LOOP4      BR_IP4(127, 0, 0, 1)

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
                 br_ip_equal(&info.ip, &BR_IP4(127, 0, 0, 1)) && info.up,
                 "Test 1c: the loopback interface exists");
    print_result(br_net_ifinfo(9, &info) == BR_ERR_NOENT, "Test 1d: unknown interface index");
}

static void run_udp_tests(void)
{
    br_uart_puts("Test 2: UDP over loopback\n");

    int server = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
    int client = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
    print_result(server >= 0 && client >= 0, "Test 2a: create sockets");

    print_result(br_net_bind(server, &LOOP4, 7000) == BR_OK, "Test 2b: bind");

    int dup = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
    print_result(br_net_bind(dup, &LOOP4, 7000) == BR_ERR_BUSY,
                 "Test 2c: binding a used port is rejected");
    br_net_close(dup);

    br_net_bind(client, &LOOP4, 7001);

    print_result(br_net_sendto(client, "hello", 5, &LOOP4, 7000) == 5,
                 "Test 2d: send");

    char buf[16];
    br_ip_t  from_ip;
    uint16_t from_port = 0;
    int n = br_net_recvfrom(server, buf, sizeof(buf), BR_MSEC(500), &from_ip, &from_port);
    print_result(n == 5 && buf[0] == 'h' && buf[4] == 'o' &&
                 br_ip_equal(&from_ip, &LOOP4) && from_port == 7001,
                 "Test 2e: receive with the sender address");

    br_net_sendto(server, "reply", 5, &from_ip, from_port);
    n = br_net_recv(client, buf, sizeof(buf), BR_MSEC(500));
    print_result(n == 5 && buf[0] == 'r', "Test 2f: the reply arrives");

    br_net_sendto(client, "0123456789", 10, &LOOP4, 7000);
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
static br_tid_t     echo_tid;

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

    int listener = br_net_socket(BR_AF_INET, BR_NET_STREAM);
    print_result(br_net_bind(listener, &LOOP4, 7002) == BR_OK &&
                 br_net_listen(listener, 1) == BR_OK,
                 "Test 3a: listen");
    echo_fd = listener;

    br_task_create(&echo_tid, "echo", tcp_echo_task, NULL, 4, stack_echo, sizeof(stack_echo));

    int client = br_net_socket(BR_AF_INET, BR_NET_STREAM);
    print_result(br_net_connect(client, &LOOP4, 7002) == BR_OK,
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
    br_task_delete(echo_tid);

    int bad = br_net_socket(BR_AF_INET, BR_NET_STREAM);
    print_result(br_net_connect(bad, &LOOP4, 7099) != BR_OK,
                 "Test 3e: connecting to a closed port fails");
    br_net_close(bad);
}

static void run_limit_tests(void)
{
    br_uart_puts("Test 4: Limits\n");

    int fds[BR_NET_MAX_SOCKETS];
    int n = 0;
    while (n < BR_NET_MAX_SOCKETS) {
        fds[n] = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
        if (fds[n] < 0) {
            break;
        }
        n++;
    }
    print_result(n == BR_NET_MAX_SOCKETS && br_net_socket(BR_AF_INET, BR_NET_DGRAM) == BR_ERR_NOMEM,
                 "Test 4a: the socket table is limited");
    for (int i = 0; i < n; i++) {
        br_net_close(fds[i]);
    }

    char c;
    print_result(br_net_socket(BR_AF_INET, 7) == BR_ERR_INVALID && br_net_recv(-1, &c, 1, 0) == BR_ERR_INVALID &&
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
        if (n != 0 && !(n >= 14 && buf[12] == 0x86 && buf[13] == 0xDD)) {
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
    print_result(br_net_netif_add(&mock.netif, &LOCAL_IP, &BR_IP4(255, 255, 255, 0), &PEER_IP) == BR_OK,
                 "Test 5a: add an interface with a static address");

    br_net_ifinfo_t info;
    bool found = false;
    for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
        if (str_equal(info.name, "eth0")) {
            found = br_ip_equal(&info.ip, &LOCAL_IP) && br_ip_equal(&info.gw, &PEER_IP) && info.mac[5] == 2 && info.up;
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
static br_tid_t peer_tid;
static volatile int dns_queries;

static bool dns_name_is(const uint8_t *q, const char *label1, const char *label2)
{
    size_t l1 = 0;
    while (label1[l1] != '\0') {
        l1++;
    }
    size_t l2 = 0;
    while (label2[l2] != '\0') {
        l2++;
    }
    if (q[0] != l1 || q[1 + l1] != l2 || q[2 + l1 + l2] != 0) {
        return false;
    }
    for (size_t i = 0; i < l1; i++) {
        if (q[1 + i] != (uint8_t)label1[i]) {
            return false;
        }
    }
    for (size_t i = 0; i < l2; i++) {
        if (q[2 + l1 + i] != (uint8_t)label2[i]) {
            return false;
        }
    }
    return true;
}

static void answer_dns(uint8_t *f, int n)
{
    uint8_t *dns = f + 42;
    size_t   qlen = 0;
    while (dns[12 + qlen] != 0) {
        qlen += (size_t)dns[12 + qlen] + 1;
    }
    qlen += 1;

    size_t question_end = 12 + qlen + 4;
    bool   known = dns_name_is(dns + 12, "bedrock", "test");

    dns_queries++;

    dns[2] = 0x81;
    dns[3] = known ? 0x80 : 0x83;
    dns[6] = 0;
    dns[7] = known ? 1 : 0;
    dns[8] = dns[9] = dns[10] = dns[11] = 0;

    size_t dns_len = question_end;
    if (known) {
        uint8_t *a = dns + question_end;
        a[0] = 0xC0; a[1] = 0x0C;
        a[2] = 0; a[3] = 1;
        a[4] = 0; a[5] = 1;
        a[6] = 0; a[7] = 0; a[8] = 0; a[9] = 60;
        a[10] = 0; a[11] = 4;
        a[12] = 192; a[13] = 168; a[14] = 7; a[15] = 50;
        dns_len += 16;
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
    uint8_t p0 = f[34], p1 = f[35];
    f[34] = f[36]; f[35] = f[37];
    f[36] = p0; f[37] = p1;

    size_t ip_len = 20 + 8 + dns_len;
    f[16] = (uint8_t)(ip_len >> 8); f[17] = (uint8_t)ip_len;
    f[24] = 0; f[25] = 0;
    uint16_t c = checksum(f + 14, 20);
    f[24] = (uint8_t)(c >> 8); f[25] = (uint8_t)c;

    size_t udp_len = 8 + dns_len;
    f[38] = (uint8_t)(udp_len >> 8); f[39] = (uint8_t)udp_len;
    f[40] = 0; f[41] = 0;

    (void)n;
    br_netif_mock_inject(&mock, f, 42 + dns_len);
}


/* ---- IPv6 peer ---- */

static const uint8_t peer_ll[16]     = { 0xFE, 0x80, [15] = 1 };
static const uint8_t peer_global[16] = { 0x20, 0x01, 0x0D, 0xB8, 0, 7, [15] = 1 };
static const uint8_t peer_answer[16] = { 0x20, 0x01, 0x0D, 0xB8, 0, 7, [15] = 0x50 };
static const uint8_t all_nodes[16]   = { 0xFF, 0x02, [15] = 1 };

static volatile int dns6_queries;
static volatile int router_adverts;

static uint32_t sum_add(uint32_t sum, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i + 1 < len; i += 2) {
        sum += (uint32_t)((data[i] << 8) | data[i + 1]);
    }
    if (len & 1) {
        sum += (uint32_t)(data[len - 1] << 8);
    }
    return sum;
}

static uint16_t checksum6(const uint8_t *src, const uint8_t *dst, uint8_t nh,
                          const uint8_t *data, size_t len)
{
    uint8_t tail[8] = { 0, 0, (uint8_t)(len >> 8), (uint8_t)len, 0, 0, 0, nh };
    uint32_t sum = sum_add(0, src, 16);
    sum = sum_add(sum, dst, 16);
    sum = sum_add(sum, tail, 8);
    sum = sum_add(sum, data, len);
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static bool is_peer_addr(const uint8_t *a)
{
    bool ll = true;
    bool gl = true;
    for (int i = 0; i < 16; i++) {
        ll = ll && a[i] == peer_ll[i];
        gl = gl && a[i] == peer_global[i];
    }
    return ll || gl;
}

static void put_eth_ip6(uint8_t *f, const uint8_t *src, const uint8_t *dst, uint8_t nh, size_t len)
{
    for (int i = 0; i < 6; i++) {
        f[i] = local_mac[i];
        f[6 + i] = peer_mac[i];
    }
    f[12] = 0x86; f[13] = 0xDD;
    f[14] = 0x60; f[15] = 0; f[16] = 0; f[17] = 0;
    f[18] = (uint8_t)(len >> 8); f[19] = (uint8_t)len;
    f[20] = nh; f[21] = 255;
    for (int i = 0; i < 16; i++) {
        f[22 + i] = src[i];
        f[38 + i] = dst[i];
    }
}

static void finish_icmp6(uint8_t *f, size_t len)
{
    uint8_t *icmp = f + 54;
    icmp[2] = 0; icmp[3] = 0;
    uint16_t c = checksum6(f + 22, f + 38, 58, icmp, len);
    icmp[2] = (uint8_t)(c >> 8); icmp[3] = (uint8_t)c;
    br_netif_mock_inject(&mock, f, 54 + len);
}

static void answer_ns(uint8_t *f)
{
    uint8_t target[16];
    uint8_t to[16];
    for (int i = 0; i < 16; i++) {
        target[i] = f[54 + 8 + i];
        to[i] = f[22 + i];
    }

    bool unspecified = true;
    for (int i = 0; i < 16; i++) {
        unspecified = unspecified && to[i] == 0;
    }
    if (unspecified || !is_peer_addr(target)) {
        return;
    }

    put_eth_ip6(f, target, to, 58, 32);
    uint8_t *icmp = f + 54;
    icmp[0] = 136; icmp[1] = 0;
    icmp[4] = 0x60; icmp[5] = icmp[6] = icmp[7] = 0;
    for (int i = 0; i < 16; i++) {
        icmp[8 + i] = target[i];
    }
    icmp[24] = 2; icmp[25] = 1;
    for (int i = 0; i < 6; i++) {
        icmp[26 + i] = peer_mac[i];
    }
    finish_icmp6(f, 32);
}

static void answer_rs(uint8_t *f)
{
    put_eth_ip6(f, peer_ll, all_nodes, 58, 56);
    f[0] = 0x33; f[1] = 0x33; f[2] = 0; f[3] = 0; f[4] = 0; f[5] = 1;
    uint8_t *icmp = f + 54;
    icmp[0] = 134; icmp[1] = 0;
    icmp[4] = 64; icmp[5] = 0;
    icmp[6] = 0x07; icmp[7] = 0x08;
    for (int i = 8; i < 16; i++) {
        icmp[i] = 0;
    }
    icmp[16] = 1; icmp[17] = 1;
    for (int i = 0; i < 6; i++) {
        icmp[18 + i] = peer_mac[i];
    }
    icmp[24] = 3; icmp[25] = 4; icmp[26] = 64; icmp[27] = 0xC0;
    icmp[28] = 0; icmp[29] = 1; icmp[30] = 0x51; icmp[31] = 0x80;
    icmp[32] = 0; icmp[33] = 0; icmp[34] = 0x38; icmp[35] = 0x40;
    icmp[36] = icmp[37] = icmp[38] = icmp[39] = 0;
    for (int i = 0; i < 16; i++) {
        icmp[40 + i] = i < 6 ? peer_global[i] : 0;
    }
    router_adverts++;
    finish_icmp6(f, 56);
}

static void answer_echo6(uint8_t *f, size_t len)
{
    uint8_t src[16];
    uint8_t dst[16];
    for (int i = 0; i < 16; i++) {
        src[i] = f[38 + i];
        dst[i] = f[22 + i];
    }
    put_eth_ip6(f, src, dst, 58, len);
    f[54] = 129;
    finish_icmp6(f, len);
}

static void answer_dns6(uint8_t *f)
{
    uint8_t *udp = f + 54;
    uint8_t *dns = udp + 8;
    size_t qlen = 0;
    while (dns[12 + qlen] != 0) {
        qlen += (size_t)dns[12 + qlen] + 1;
    }
    qlen += 1;

    size_t question_end = 12 + qlen + 4;
    bool known = dns_name_is(dns + 12, "bedrock", "test") && dns[12 + qlen] == 0 &&
                 dns[12 + qlen + 1] == 28;
    dns6_queries++;

    dns[2] = 0x81;
    dns[3] = known ? 0x80 : 0x83;
    dns[6] = 0;
    dns[7] = known ? 1 : 0;
    dns[8] = dns[9] = dns[10] = dns[11] = 0;

    size_t dns_len = question_end;
    if (known) {
        uint8_t *a = dns + question_end;
        a[0] = 0xC0; a[1] = 0x0C;
        a[2] = 0; a[3] = 28;
        a[4] = 0; a[5] = 1;
        a[6] = 0; a[7] = 0; a[8] = 0; a[9] = 60;
        a[10] = 0; a[11] = 16;
        for (int i = 0; i < 16; i++) {
            a[12 + i] = peer_answer[i];
        }
        dns_len += 28;
    }

    uint8_t src[16];
    uint8_t dst[16];
    for (int i = 0; i < 16; i++) {
        src[i] = f[38 + i];
        dst[i] = f[22 + i];
    }
    uint8_t p0 = udp[0], p1 = udp[1];
    udp[0] = udp[2]; udp[1] = udp[3];
    udp[2] = p0; udp[3] = p1;

    size_t udp_len = 8 + dns_len;
    udp[4] = (uint8_t)(udp_len >> 8); udp[5] = (uint8_t)udp_len;
    udp[6] = 0; udp[7] = 0;
    put_eth_ip6(f, src, dst, 17, udp_len);
    uint16_t c = checksum6(src, dst, 17, udp, udp_len);
    udp[6] = (uint8_t)(c >> 8); udp[7] = (uint8_t)c;
    br_netif_mock_inject(&mock, f, 54 + udp_len);
}

static void peer_ipv6(uint8_t *f, int n)
{
    if (n < 54) {
        return;
    }

    if (f[20] == 58) {
        switch (f[54]) {
        case 133: answer_rs(f); break;
        case 135: if (n >= 54 + 24) { answer_ns(f); } break;
        case 128:
            if (is_peer_addr(f + 38)) {
                answer_echo6(f, (size_t)n - 54);
            }
            break;
        default: break;
        }
    } else if (f[20] == 17 && n >= 54 + 8 + 12 && f[56] == 0 && f[57] == 53 && is_peer_addr(f + 38)) {
        answer_dns6(f);
    }
}

static void peer_task(void *arg)
{
    (void)arg;

    static uint8_t f[BR_NETIF_FRAME_MAX];

    while (1) {
        int n = br_netif_mock_take(&mock, f, sizeof(f));
        if (n >= 54 && f[12] == 0x86 && f[13] == 0xDD) {
            peer_ipv6(f, n);
            continue;
        }
        if (n < 42) {
            br_sleep_ms(2);
            continue;
        }

        if (f[12] == 0x08 && f[13] == 0x00 && f[23] == 17 && f[36] == 0 && f[37] == 53 &&
            f[30] == 192 && f[31] == 168 && f[32] == 7 && f[33] == 1) {
            answer_dns(f, n);
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

    br_task_create(&peer_tid, "peer", peer_task, NULL, 1, stack_peer, sizeof(stack_peer));

    uint32_t rtt = 0;
    print_result(br_net_ping(&LOOP4, BR_MSEC(500), &rtt) == BR_OK,
                 "Test 6a: ping the loopback address");

    br_err_t err = br_net_ping(&PEER_IP, BR_SEC(2), &rtt);
    print_result(err == BR_OK && rtt > 0, "Test 6b: ping a peer through the interface");

    print_result(br_net_ping(&BR_IP4(192, 168, 7, 99), BR_MSEC(300), &rtt) == BR_ERR_TIMEOUT,
                 "Test 6c: ping of a silent address times out");

}

static void run_dns_tests(void)
{
    br_uart_puts("Test 8: DNS\n");

    br_ip_t ip;
    print_result(br_net_dns(&ip) == BR_ERR_NOENT && ip.family == BR_AF_ANY,
                 "Test 8a: no DNS server before one is set");

    br_net_set_dns(&PEER_IP);
    print_result(br_net_dns(&ip) == BR_OK && br_ip_equal(&ip, &PEER_IP), "Test 8b: set the DNS server");

    br_ip_t want = BR_IP4(192, 168, 7, 50);
    ip = br_ip_any(BR_AF_ANY);
    print_result(br_net_resolve("bedrock.test", BR_AF_INET, &ip) == BR_OK && br_ip_equal(&ip, &want) &&
                 dns_queries == 1,
                 "Test 8c: resolve a name through the interface");

    ip = br_ip_any(BR_AF_ANY);
    print_result(br_net_resolve("bedrock.test", BR_AF_INET, &ip) == BR_OK && br_ip_equal(&ip, &want) &&
                 dns_queries == 1,
                 "Test 8d: a second lookup comes from the cache");

    want = BR_IP4(10, 1, 2, 3);
    print_result(br_net_resolve("10.1.2.3", BR_AF_INET, &ip) == BR_OK && br_ip_equal(&ip, &want) &&
                 dns_queries == 1,
                 "Test 8e: a dotted address needs no query");

    print_result(br_net_resolve("nosuch.test", BR_AF_INET, &ip) == BR_ERR_NOENT && dns_queries == 2,
                 "Test 8f: an unknown name is reported");

    char longname[80];
    for (int i = 0; i < 79; i++) {
        longname[i] = 'a';
    }
    longname[79] = '\0';
    print_result(br_net_resolve(NULL, BR_AF_INET, &ip) == BR_ERR_INVALID &&
                 br_net_resolve("", BR_AF_INET, &ip) == BR_ERR_INVALID &&
                 br_net_resolve(longname, BR_AF_INET, &ip) == BR_ERR_INVALID &&
                 br_net_resolve("a.test", BR_AF_INET, NULL) == BR_ERR_INVALID &&
                 br_net_resolve("a.test", 5, &ip) == BR_ERR_INVALID,
                 "Test 8g: bad arguments are rejected");

    char found[] = "nslookup bedrock.test 4";
    char missing[] = "nslookup nosuch.test 4";
    char usage[] = "nslookup";
    char bad_family[] = "nslookup bedrock.test 5";
    print_result(br_shell_execute(found) == 0 && br_shell_execute(missing) == -1 &&
                 br_shell_execute(usage) == -1 && br_shell_execute(bad_family) == -1,
                 "Test 8h: nslookup");
}


static bool contains_ip6(const br_net_ifinfo_t *info, const br_ip_t *want)
{
    for (uint8_t i = 0; i < info->ip6_count; i++) {
        if (br_ip_equal(&info->ip6[i], want)) {
            return true;
        }
    }
    return false;
}

static bool has_prefix(const br_net_ifinfo_t *info, const uint8_t *prefix, size_t n)
{
    for (uint8_t i = 0; i < info->ip6_count; i++) {
        size_t k = 0;
        while (k < n && info->ip6[i].addr[k] == prefix[k]) {
            k++;
        }
        if (k == n) {
            return true;
        }
    }
    return false;
}

static bool eth0_info(br_net_ifinfo_t *info)
{
    for (uint32_t i = 0; br_net_ifinfo(i, info) == BR_OK; i++) {
        if (str_equal(info->name, "eth0")) {
            return true;
        }
    }
    return false;
}

static void run_ipv6_address_tests(void)
{
    br_uart_puts("Test 9: IPv6 addresses\n");

    static const char *const good[][2] = {
        { "::1", "::1" }, { "::", "::" }, { "1::", "1::" },
        { "2001:db8::1", "2001:db8::1" },
        { "2001:0DB8:0000:0000:0000:0000:0000:0001", "2001:db8::1" },
        { "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8" },
        { "1:0:0:2:0:0:0:3", "1:0:0:2::3" },
        { "1:0:2:3:4:5:6:7", "1:0:2:3:4:5:6:7" },
        { "fe80::ff:fe00:2", "fe80::ff:fe00:2" },
        { "::ffff:1.2.3.4", "::ffff:102:304" },
        { "192.168.1.2", "192.168.1.2" },
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); i++) {
        br_ip_t ip, again;
        char text[48];
        bool this_ok = br_ip_parse(good[i][0], &ip) && br_ip_format(&ip, text, sizeof(text)) > 0 &&
                       str_equal(text, good[i][1]) && br_ip_parse(text, &again) && br_ip_equal(&ip, &again);
        if (!this_ok) {
            br_uart_puts("  address rejected or mangled: ");
            br_uart_puts(good[i][0]);
            br_uart_puts("\n");
        }
        ok = ok && this_ok;
    }
    print_result(ok, "Test 9a: addresses are parsed and formatted");

    static const char *const bad[] = {
        "", ":::", "1::2::3", "12345::", "1:2:3:4:5:6:7:8:9", ":1", "1:", "g::", "1.2.3",
        "256.1.1.1", "::1.2.3", "1:2:3:4:5:6:7::8", "1.2.3.4.5", "::1:", " ::1", "fe80::1%", "fe80::1%nosuch",
        "1.2.3.4%1", "::1%0", "%1", "fe80::1%70000", "fe80::1%2%3",
    };
    ok = true;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        br_ip_t ip;
        if (br_ip_parse(bad[i], &ip)) {
            br_uart_puts("  accepted: ");
            br_uart_puts(bad[i]);
            br_uart_puts("\n");
            ok = false;
        }
    }
    print_result(ok && !br_ip_parse(NULL, &(br_ip_t){ 0 }), "Test 9b: malformed addresses are rejected");

    char tiny[3];
    br_ip_t v6 = BR_IP6_LOOPBACK;
    br_ip_t none = br_ip_any(BR_AF_ANY);
    char big[8];
    print_result(br_ip_format(&v6, tiny, sizeof(tiny)) == BR_ERR_NOMEM && tiny[2] == '\0' &&
                 br_ip_format(&none, big, sizeof(big)) == BR_ERR_INVALID &&
                 br_ip_format(&v6, big, 0) == BR_ERR_INVALID,
                 "Test 9c: formatting checks its buffer");

    br_ip_t a = BR_IP4(1, 2, 3, 4);
    br_ip_t b = BR_IP4(1, 2, 3, 4);
    br_ip_t c = BR_IP4(1, 2, 3, 5);
    br_ip_t zero4 = br_ip_any(BR_AF_INET);
    br_ip_t zero6 = br_ip_any(BR_AF_INET6);
    print_result(br_ip_equal(&a, &b) && !br_ip_equal(&a, &c) && !br_ip_equal(&zero4, &zero6) &&
                 br_ip_is_any(&zero4) && br_ip_is_any(&zero6) && !br_ip_is_any(&a) && !br_ip_is_any(&v6),
                 "Test 9d: comparing addresses");
}

static void run_ipv6_scope_tests(void)
{
    br_net_ifinfo_t info;
    print_result(eth0_info(&info) && info.scope != 0, "Test 9e: an interface has a scope number");

    br_ip_t by_name, by_number, ll = { .family = BR_AF_INET6, .addr = { 0xFE, 0x80, [15] = 1 } };
    char text[48], number[24] = "fe80::1%";
    char digits[6];
    size_t n = 8;
    int k = 0;
    uint32_t v = info.scope;
    do {
        digits[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    while (k > 0) {
        number[n++] = digits[--k];
    }
    number[n] = '\0';

    print_result(br_ip_parse("fe80::1%eth0", &by_name) && br_ip_parse(number, &by_number) &&
                 by_name.scope == info.scope && br_ip_equal(&by_name, &by_number) &&
                 br_ip_equal(&by_name, &ll) &&
                 br_ip_format(&by_name, text, sizeof(text)) > 0 && str_equal(text, number),
                 "Test 9f: a scope is parsed by name and by number and formatted");

    br_ip_t other = by_name;
    other.scope = (uint16_t)(info.scope + 1);
    print_result(!br_ip_equal(&by_name, &other),
                 "Test 9g: addresses with different scopes differ");
}

static void run_ipv6_loopback_tests(void)
{
    br_uart_puts("Test 10: IPv6 over loopback\n");

    br_net_ifinfo_t info;
    br_ip_t lo6 = BR_IP6_LOOPBACK;
    bool lo_ok = false;
    for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
        if (str_equal(info.name, "lo0")) {
            lo_ok = contains_ip6(&info, &lo6);
        }
    }
    print_result(lo_ok, "Test 10a: the loopback interface has ::1");

    int server = br_net_socket(BR_AF_INET6, BR_NET_DGRAM);
    int client = br_net_socket(BR_AF_INET6, BR_NET_DGRAM);
    print_result(server >= 0 && client >= 0 && br_net_bind(server, &lo6, 7100) == BR_OK &&
                 br_net_bind(client, NULL, 7101) == BR_OK,
                 "Test 10b: bind IPv6 sockets");

    print_result(br_net_sendto(client, "hello6", 6, &lo6, 7100) == 6, "Test 10c: send");

    char buf[16];
    br_ip_t from;
    uint16_t from_port = 0;
    int n = br_net_recvfrom(server, buf, sizeof(buf), BR_MSEC(500), &from, &from_port);
    print_result(n == 6 && buf[0] == 'h' && buf[5] == '6' && from.family == BR_AF_INET6 &&
                 br_ip_equal(&from, &lo6) && from_port == 7101,
                 "Test 10d: receive with the sender address");

    br_net_sendto(server, "back", 4, &from, from_port);
    print_result(br_net_recv(client, buf, sizeof(buf), BR_MSEC(500)) == 4 && buf[0] == 'b',
                 "Test 10e: the reply arrives");
    br_net_close(server);
    br_net_close(client);

    int v4 = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
    int v6 = br_net_socket(BR_AF_INET6, BR_NET_DGRAM);
    br_ip_t ip4 = BR_IP4(127, 0, 0, 1);
    print_result(br_net_bind(v4, &lo6, 7102) == BR_ERR_INVALID &&
                 br_net_sendto(v6, "x", 1, &ip4, 7100) == BR_ERR_INVALID &&
                 br_net_sendto(v4, "x", 1, &lo6, 7100) == BR_ERR_INVALID &&
                 br_net_sendto(v4, "x", 1, NULL, 7100) == BR_ERR_INVALID &&
                 br_net_connect(v6, NULL, 7100) == BR_ERR_INVALID,
                 "Test 10f: an address of another family is rejected");
    br_net_close(v4);
    br_net_close(v6);

    int any4 = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
    int any6 = br_net_socket(BR_AF_INET6, BR_NET_DGRAM);
    print_result(br_net_bind(any4, NULL, 7200) == BR_OK && br_net_bind(any6, NULL, 7200) == BR_OK,
                 "Test 10l: an IPv4 and an IPv6 socket can use the same port");
    br_net_close(any4);
    br_net_close(any6);

    print_result(br_net_socket(BR_AF_ANY, BR_NET_DGRAM) == BR_ERR_INVALID &&
                 br_net_socket(5, BR_NET_DGRAM) == BR_ERR_INVALID,
                 "Test 10g: the family must be IPv4 or IPv6");

    fill_pattern();
    int listener = br_net_socket(BR_AF_INET6, BR_NET_STREAM);
    print_result(br_net_bind(listener, &lo6, 7103) == BR_OK && br_net_listen(listener, 1) == BR_OK,
                 "Test 10h: listen on ::1");
    echo_fd = listener;
    br_task_create(&echo_tid, "echo6", tcp_echo_task, NULL, 4, stack_echo, sizeof(stack_echo));

    int tcp = br_net_socket(BR_AF_INET6, BR_NET_STREAM);
    print_result(br_net_connect(tcp, &lo6, 7103) == BR_OK, "Test 10i: connect");

    bool sent_ok = true;
    for (size_t off = 0; off < 800 && sent_ok; off += 400) {
        sent_ok = br_net_send(tcp, pattern + off, 400) == 400;
    }
    size_t got = 0;
    while (got < 800) {
        int r = br_net_recv(tcp, readback + got, 800 - got, BR_SEC(5));
        if (r <= 0) {
            break;
        }
        got += (size_t)r;
    }
    bool same = sent_ok && got == 800;
    for (size_t i = 0; same && i < got; i++) {
        same = readback[i] == pattern[i];
    }
    print_result(same, "Test 10j: the TCP echo is complete and correct");
    br_net_close(tcp);
    br_sleep_ms(100);
    br_net_close(listener);
    br_task_delete(echo_tid);

    uint32_t rtt = 0;
    br_ip_t none = br_ip_any(BR_AF_ANY);
    print_result(br_net_ping(&lo6, BR_MSEC(500), &rtt) == BR_OK && br_net_ping(&none, BR_MSEC(50), &rtt) == BR_ERR_INVALID &&
                 br_net_ping(NULL, BR_MSEC(50), &rtt) == BR_ERR_INVALID,
                 "Test 10k: ping ::1");
}

static void run_ipv6_wire_tests(void)
{
    br_uart_puts("Test 11: IPv6 on the Ethernet interface\n");

    br_ip_t ll = { .family = BR_AF_INET6, .addr = { 0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFE, 0, 0, 2 } };
    br_net_ifinfo_t info;

    print_result(br_net_wait_ip6(&mock.netif, BR_SEC(6)) == BR_OK && eth0_info(&info) &&
                 contains_ip6(&info, &ll),
                 "Test 11a: the link-local address comes from the MAC address");

    static const uint8_t prefix[8] = { 0x20, 0x01, 0x0D, 0xB8, 0, 7, 0, 0 };
    bool slaac = false;
    for (int i = 0; i < 100 && !slaac; i++) {
        slaac = eth0_info(&info) && has_prefix(&info, prefix, sizeof(prefix));
        if (!slaac) {
            br_sleep_ms(100);
        }
    }
    print_result(slaac && router_adverts > 0,
                 "Test 11b: a router advertisement gives a global address");

    uint32_t rtt = 0;
    br_ip_t peer_local = { .family = BR_AF_INET6, .addr = { 0xFE, 0x80, [15] = 1 } };
    br_ip_t peer_glob = { .family = BR_AF_INET6, .addr = { 0x20, 0x01, 0x0D, 0xB8, 0, 7, [15] = 1 } };
    print_result(br_net_ping(&peer_local, BR_SEC(2), &rtt) == BR_OK && rtt > 0,
                 "Test 11c: ping the peer by its link-local address");
    print_result(br_net_ping(&peer_glob, BR_SEC(2), &rtt) == BR_OK && rtt > 0,
                 "Test 11d: ping the peer by its global address");

    br_ip_t silent = { .family = BR_AF_INET6, .addr = { 0xFE, 0x80, [15] = 0x99 } };
    print_result(br_net_ping(&silent, BR_MSEC(300), &rtt) == BR_ERR_TIMEOUT,
                 "Test 11e: ping of a silent address times out");

    br_ip_t stat = { .family = BR_AF_INET6, .addr = { 0xFD, 0, [15] = 2 } };
    br_ip_t v4 = BR_IP4(1, 2, 3, 4);
    print_result(br_net_ip6_add(&mock.netif, &stat) == BR_OK && br_net_ip6_add(&mock.netif, &v4) == BR_ERR_INVALID &&
                 br_net_ip6_add(NULL, &stat) == BR_ERR_INVALID,
                 "Test 11f: add a static address");

    bool listed = false;
    for (int i = 0; i < 40 && !listed; i++) {
        listed = eth0_info(&info) && contains_ip6(&info, &stat);
        if (!listed) {
            br_sleep_ms(100);
        }
    }
    print_result(listed, "Test 11g: the static address is used after duplicate detection");

    br_ip_t another = { .family = BR_AF_INET6, .addr = { 0xFD, 0, [15] = 3 } };
    print_result(br_net_ip6_add(&mock.netif, &another) == BR_ERR_NOMEM,
                 "Test 11h: the address table is limited");

    char ping_ll[] = "ping fe80::1 1";
    char ping_silent[] = "ping fe80::99 1";
    char ping_lo[] = "ping ::1 1";
    char ifconfig_line[] = "ifconfig";
    print_result(br_shell_execute(ping_ll) == 0 && br_shell_execute(ping_silent) == -1 &&
                 br_shell_execute(ping_lo) == 0 && br_shell_execute(ifconfig_line) == 0,
                 "Test 11i: shell commands with IPv6 addresses");
}

static void run_ipv6_dns_tests(void)
{
    br_uart_puts("Test 12: DNS over IPv6\n");

    br_ip_t server = { .family = BR_AF_INET6, .addr = { 0x20, 0x01, 0x0D, 0xB8, 0, 7, [15] = 1 } };
    br_ip_t ip;
    br_ip_t want = { .family = BR_AF_INET6, .addr = { 0x20, 0x01, 0x0D, 0xB8, 0, 7, [15] = 0x50 } };

    print_result(br_net_set_dns(&server) == BR_OK && br_net_dns(&ip) == BR_OK && br_ip_equal(&ip, &server),
                 "Test 12a: an IPv6 DNS server");

    ip = br_ip_any(BR_AF_ANY);
    print_result(br_net_resolve("bedrock.test", BR_AF_INET6, &ip) == BR_OK && br_ip_equal(&ip, &want) &&
                 dns6_queries == 1,
                 "Test 12b: resolve an AAAA record");
    print_result(br_net_resolve("bedrock.test", BR_AF_INET6, &ip) == BR_OK && dns6_queries == 1,
                 "Test 12c: the answer is cached");
    print_result(br_net_resolve("nosuch.test", BR_AF_INET6, &ip) == BR_ERR_NOENT && dns6_queries == 2,
                 "Test 12d: an unknown name is reported");

    br_ip_t lit = { .family = BR_AF_INET6, .addr = { [15] = 1 } };
    print_result(br_net_resolve("::1", BR_AF_INET6, &ip) == BR_OK && br_ip_equal(&ip, &lit) &&
                 br_net_resolve("::1", BR_AF_INET, &ip) == BR_ERR_NOENT && dns6_queries == 2,
                 "Test 12e: an IPv6 literal needs no query and has its family");

    char six[] = "nslookup bedrock.test 6";
    print_result(br_shell_execute(six) == 0, "Test 12f: nslookup with a family");
}

static void stop_peer(void)
{
    br_task_suspend(peer_tid);
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
    run_dns_tests();
    run_ipv6_address_tests();
    run_ipv6_scope_tests();
    run_ipv6_loopback_tests();
    run_ipv6_wire_tests();
    run_ipv6_dns_tests();
    stop_peer();

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

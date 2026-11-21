/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_net.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static void run_tests(void)
{
    br_uart_puts("Test 1: QEMU user network\n");

    br_netif_t *dev = br_hal_netif_default();
    if (dev == NULL) {
        br_uart_puts("Test 1: SKIP, no network interface on this target\n");
        return;
    }

    print_result(br_net_init() == BR_OK, "Test 1a: init");
    print_result(br_net_netif_add(dev, NULL, NULL, NULL) == BR_OK, "Test 1b: add the interface with DHCP");

    br_err_t err = br_net_wait_ip(dev, BR_SEC(10));
    print_result(err == BR_OK, "Test 1c: DHCP gives an address");

    br_net_ifinfo_t info;
    bool found = false;
    for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
        if (info.has_mac && !br_ip_is_any(&info.ip)) {
            found = br_ip_equal(&info.ip, &BR_IP4(10, 0, 2, 15)) &&
                    br_ip_equal(&info.gw, &BR_IP4(10, 0, 2, 2)) &&
                    br_ip_equal(&info.mask, &BR_IP4(255, 255, 255, 0));
        }
    }
    print_result(found, "Test 1d: the lease is 10.0.2.15 with gateway 10.0.2.2");

    br_ip_t dns;
    print_result(br_net_dns(&dns) == BR_OK && br_ip_equal(&dns, &BR_IP4(10, 0, 2, 3)),
                 "Test 1h: DHCP provides the DNS server");

    uint32_t rtt = 0;
    print_result(br_net_ping(&BR_IP4(10, 0, 2, 2), BR_SEC(3), &rtt) == BR_OK && rtt > 0,
                 "Test 1e: ping the gateway");
    print_result(dev->irq && br_net_rx_wakeups() > 0 && rtt < BR_MSEC(100),
                 "Test 1f: frames wake the receive task by interrupt");

    int fd = br_net_socket(BR_AF_INET, BR_NET_DGRAM);
    print_result(fd >= 0 && br_net_sendto(fd, "bedrock", 7, &BR_IP4(10, 0, 2, 2), 9) == 7,
                 "Test 1g: send a datagram through the gateway");
    br_net_close(fd);

    print_result(br_net_wait_ip6(dev, BR_SEC(10)) == BR_OK, "Test 1i: an IPv6 link-local address");

    /* QEMU's user network sends a router advertisement for fec0::/64. */
    static const uint8_t prefix[8] = { 0xFE, 0xC0 };
    bool global = false;
    for (int i = 0; i < 100 && !global; i++) {
        global = false;
        for (uint32_t k = 0; br_net_ifinfo(k, &info) == BR_OK; k++) {
            for (uint8_t a = 0; a < info.ip6_count; a++) {
                global = global || (info.ip6[a].addr[0] == prefix[0] && info.ip6[a].addr[1] == prefix[1]);
            }
        }
        if (!global) {
            br_sleep_ms(100);
        }
    }
    print_result(global, "Test 1j: a router advertisement gives a global address");

    br_ip_t gw6 = { .family = BR_AF_INET6, .addr = { 0xFE, 0xC0, [15] = 2 } };
    print_result(br_net_ping(&gw6, BR_SEC(3), &rtt) == BR_OK && rtt > 0,
                 "Test 1k: ping the IPv6 gateway");
}

static uint8_t stack_supervisor[3072];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== QEMU Network Test ===\n\n");

    run_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - QEMU Network Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

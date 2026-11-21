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
    print_result(br_net_netif_add(dev, 0, 0, 0) == BR_OK, "Test 1b: add the interface with DHCP");

    br_err_t err = br_net_wait_ip(dev, BR_SEC(10));
    print_result(err == BR_OK, "Test 1c: DHCP gives an address");

    br_net_ifinfo_t info;
    bool found = false;
    for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
        if (info.has_mac && info.ip != 0) {
            found = info.ip == BR_IP4(10, 0, 2, 15) && info.gw == BR_IP4(10, 0, 2, 2) &&
                    info.mask == BR_IP4(255, 255, 255, 0);
        }
    }
    print_result(found, "Test 1d: the lease is 10.0.2.15 with gateway 10.0.2.2");

    uint32_t rtt = 0;
    print_result(br_net_ping(BR_IP4(10, 0, 2, 2), BR_SEC(3), &rtt) == BR_OK && rtt > 0,
                 "Test 1e: ping the gateway");

    int fd = br_net_socket(BR_NET_DGRAM);
    print_result(fd >= 0 && br_net_sendto(fd, "bedrock", 7, BR_IP4(10, 0, 2, 2), 9) == 7,
                 "Test 1f: send a datagram through the gateway");
    br_net_close(fd);
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

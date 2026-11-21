/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_net.h"
#include "br_shell.h"
#include "br_console.h"

#define ECHO_PORT  7

static uint8_t stack_echo4[2048];
static uint8_t stack_echo6[2048];
static uint8_t stack_shell[2048];

static void echo_task(void *arg)
{
    int family = (int)(uintptr_t)arg;

    int fd = br_net_socket(family, BR_NET_DGRAM);
    if (fd < 0 || br_net_bind(fd, NULL, ECHO_PORT) != BR_OK) {
        br_printf("echo: cannot bind IPv%d port %d\n", family, ECHO_PORT);
        br_task_suspend(br_task_self());
    }

    uint8_t buf[512];
    while (1) {
        br_ip_t  ip;
        uint16_t port;
        int n = br_net_recvfrom(fd, buf, sizeof(buf), BR_TIME_INFINITE, &ip, &port);
        if (n > 0) {
            br_net_sendto(fd, buf, (size_t)n, &ip, port);
        }
    }
}

static void shell_task(void *arg)
{
    (void)arg;

    br_shell_register_builtin();
    br_shell_register_net();
    br_shell_run();
}

static uint8_t stack_main[3072];

static void main_task(void *arg)
{
    (void)arg;

    br_netif_t *dev = br_hal_netif_default();
    br_net_init();

    if (dev == NULL || br_net_netif_add(dev, NULL, NULL, NULL) != BR_OK) {
        br_printf("no network interface\n");
    } else {
        br_printf("waiting for an address\n");
        if (br_net_wait_ip(dev, BR_SEC(10)) == BR_OK) {
            br_net_ifinfo_t info;
            for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
                if (info.has_mac) {
                    char text[48];
                    br_ip_format(&info.ip, text, sizeof(text));
                    br_printf("%s: %s\n", info.name, text);
                }
            }
        } else {
            br_printf("no address from DHCP\n");
        }
    }

    br_tid_t tid;
    br_task_create(&tid, "echo4", echo_task, (void *)(uintptr_t)BR_AF_INET, 4, stack_echo4, sizeof(stack_echo4));
    br_task_create(&tid, "echo6", echo_task, (void *)(uintptr_t)BR_AF_INET6, 4, stack_echo6, sizeof(stack_echo6));
    br_task_create(&tid, "shell", shell_task, NULL, 5, stack_shell, sizeof(stack_shell));
    br_task_suspend(br_task_self());
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - UDP echo\n");

    br_tid_t tid;
    br_task_create(&tid, "main", main_task, NULL, 0, stack_main, sizeof(stack_main));

    br_kernel_start();
}

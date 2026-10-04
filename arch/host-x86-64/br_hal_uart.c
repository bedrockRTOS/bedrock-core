/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

extern void br_uart_rx_push(char c);

static bool stdin_closed;

void br_uart_putc(char c)
{
    putchar((unsigned char)c);
}

void br_uart_puts(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
}

void br_host_uart_poll(void)
{
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };

    while (!stdin_closed && poll(&pfd, 1, 0) > 0) {
        char buf[32];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) {
            stdin_closed = true;
            break;
        }
        for (ssize_t i = 0; i < n; i++) {
            br_uart_rx_push(buf[i]);
        }
    }
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define UART0_DR   (*(volatile uint32_t *)0x4000C000)
#define UART0_FR   (*(volatile uint32_t *)0x4000C018)
#define UART0_FR_TXFF  (1 << 5)

void br_uart_putc(char c)
{
    while (UART0_FR & UART0_FR_TXFF) { }
    UART0_DR = c;
}

void br_uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            br_uart_putc('\r');
        }
        br_uart_putc(*s++);
    }
}

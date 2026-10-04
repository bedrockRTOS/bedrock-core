/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define UART0_THR       (*(volatile uint8_t *)0x10000000UL)
#define UART0_LSR       (*(volatile uint8_t *)0x10000005UL)
#define UART0_LSR_THRE  (1 << 5)

void br_uart_putc(char c)
{
    while (!(UART0_LSR & UART0_LSR_THRE)) { }
    UART0_THR = (uint8_t)c;
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

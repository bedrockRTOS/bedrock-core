/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE-GPL-3.0.md for the full terms and runtime exception.
 *
 * UART0 driver for LM3S6965 (QEMU).
 * Minimal polled output -- no interrupts, no RX.
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

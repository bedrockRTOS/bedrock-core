/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Host x86-64 UART HAL -- maps to stdout.
 */

#include <stdio.h>

void br_uart_putc(char c)
{
    putchar((unsigned char)c);
}

void br_uart_puts(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
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

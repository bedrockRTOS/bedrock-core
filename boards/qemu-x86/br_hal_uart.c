/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#include "x86_io.h"

#define COM1             0x3F8
#define COM1_IRQ         4

#define UART_DATA        (COM1 + 0)
#define UART_IER         (COM1 + 1)
#define UART_FCR         (COM1 + 2)
#define UART_LCR         (COM1 + 3)
#define UART_MCR         (COM1 + 4)
#define UART_LSR         (COM1 + 5)

#define UART_LSR_DR      0x01
#define UART_LSR_THRE    0x20

extern void br_uart_rx_push(char c);

void br_uart_init(void)
{
    outb(UART_IER, 0x00);
    outb(UART_LCR, 0x80);
    outb(UART_DATA, 0x01);
    outb(UART_IER, 0x00);
    outb(UART_LCR, 0x03);
    outb(UART_FCR, 0xC7);
    outb(UART_MCR, 0x0B);
    outb(UART_IER, 0x01);

    outb(PIC1_DATA, (uint8_t)(inb(PIC1_DATA) & ~(1U << COM1_IRQ)));
}

void br_hal_external_isr(uint32_t irq)
{
    if (irq == COM1_IRQ) {
        while (inb(UART_LSR) & UART_LSR_DR) {
            br_uart_rx_push((char)inb(UART_DATA));
        }
    }
}

void br_uart_putc(char c)
{
    while (!(inb(UART_LSR) & UART_LSR_THRE)) { }
    outb(UART_DATA, (uint8_t)c);
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

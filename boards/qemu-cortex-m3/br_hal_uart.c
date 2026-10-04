/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define UART0_DR        (*(volatile uint32_t *)0x4000C000UL)
#define UART0_FR        (*(volatile uint32_t *)0x4000C018UL)
#define UART0_IMSC      (*(volatile uint32_t *)0x4000C038UL)
#define UART0_ICR       (*(volatile uint32_t *)0x4000C044UL)
#define NVIC_ISER0      (*(volatile uint32_t *)0xE000E100UL)

#define UART_FR_RXFE    (1UL << 4)
#define UART_FR_TXFF    (1UL << 5)
#define UART_INT_RX     (1UL << 4)
#define UART_INT_RT     (1UL << 6)
#define UART0_IRQ       5

extern void br_uart_rx_push(char c);

void br_uart_init(void)
{
    UART0_IMSC = UART_INT_RX | UART_INT_RT;
    NVIC_ISER0 = 1UL << UART0_IRQ;
}

void IRQ5_Handler(void)
{
    while (!(UART0_FR & UART_FR_RXFE)) {
        br_uart_rx_push((char)(UART0_DR & 0xFF));
    }
    UART0_ICR = UART_INT_RX | UART_INT_RT;
}

void br_uart_putc(char c)
{
    while (UART0_FR & UART_FR_TXFF) { }
    UART0_DR = (uint32_t)(uint8_t)c;
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

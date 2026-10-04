/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define UART0_DR        (*(volatile uint32_t *)0x40034000UL)
#define UART0_FR        (*(volatile uint32_t *)0x40034018UL)
#define UART0_IBRD      (*(volatile uint32_t *)0x40034024UL)
#define UART0_FBRD      (*(volatile uint32_t *)0x40034028UL)
#define UART0_LCR_H     (*(volatile uint32_t *)0x4003402CUL)
#define UART0_CR        (*(volatile uint32_t *)0x40034030UL)

#define IO_BANK0_GPIO0_CTRL  (*(volatile uint32_t *)0x40014004UL)
#define IO_BANK0_GPIO1_CTRL  (*(volatile uint32_t *)0x4001400CUL)

#define GPIO_FUNC_UART  2
#define UART_FR_TXFF    (1UL << 5)
#define UART_LCR_H_FEN  (1UL << 4)
#define UART_LCR_H_8BIT (3UL << 5)
#define UART_CR_UARTEN  (1UL << 0)
#define UART_CR_TXE     (1UL << 8)
#define UART_CR_RXE     (1UL << 9)

void br_uart_init(void)
{
    IO_BANK0_GPIO0_CTRL = GPIO_FUNC_UART;
    IO_BANK0_GPIO1_CTRL = GPIO_FUNC_UART;

    UART0_IBRD  = 6;
    UART0_FBRD  = 33;
    UART0_LCR_H = UART_LCR_H_8BIT | UART_LCR_H_FEN;
    UART0_CR    = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void br_uart_putc(char c)
{
    while (UART0_FR & UART_FR_TXFF) { }
    UART0_DR = (uint8_t)c;
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

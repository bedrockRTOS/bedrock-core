/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define USART1_SR       (*(volatile uint32_t *)0x40013800UL)
#define USART1_DR       (*(volatile uint32_t *)0x40013804UL)
#define USART1_BRR      (*(volatile uint32_t *)0x40013808UL)
#define USART1_CR1      (*(volatile uint32_t *)0x4001380CUL)
#define NVIC_ISER1      (*(volatile uint32_t *)0xE000E104UL)

#define USART_SR_RXNE   (1UL << 5)
#define USART_SR_TXE    (1UL << 7)
#define USART_CR1_RE    (1UL << 2)
#define USART_CR1_TE    (1UL << 3)
#define USART_CR1_RXNEIE (1UL << 5)
#define USART_CR1_UE    (1UL << 13)

#define USART1_BRR_115200_AT_72MHZ  625
#define USART1_IRQ      37

extern void br_uart_rx_push(char c);

void br_uart_init(void)
{
    USART1_BRR = USART1_BRR_115200_AT_72MHZ;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_ISER1 = 1UL << (USART1_IRQ - 32);
}

void IRQ37_Handler(void)
{
    while (USART1_SR & USART_SR_RXNE) {
        br_uart_rx_push((char)(USART1_DR & 0xFF));
    }
}

void br_uart_putc(char c)
{
    while (!(USART1_SR & USART_SR_TXE)) { }
    USART1_DR = (uint8_t)c;
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

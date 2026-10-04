/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define UART0_RBR       (*(volatile uint8_t *)0x10000000UL)
#define UART0_THR       (*(volatile uint8_t *)0x10000000UL)
#define UART0_IER       (*(volatile uint8_t *)0x10000001UL)
#define UART0_LSR       (*(volatile uint8_t *)0x10000005UL)

#define PLIC_PRIORITY(irq)  (*(volatile uint32_t *)(0x0C000000UL + 4UL * (irq)))
#define PLIC_ENABLE_CTX0    (*(volatile uint32_t *)0x0C002000UL)
#define PLIC_THRESHOLD_CTX0 (*(volatile uint32_t *)0x0C200000UL)
#define PLIC_CLAIM_CTX0     (*(volatile uint32_t *)0x0C200004UL)

#define UART0_IRQ       10
#define UART_IER_RDA    (1 << 0)
#define UART_LSR_DR     (1 << 0)
#define UART_LSR_THRE   (1 << 5)
#define MIE_MEIE        (1UL << 11)

extern void br_uart_rx_push(char c);

void br_uart_init(void)
{
    PLIC_PRIORITY(UART0_IRQ) = 1;
    PLIC_THRESHOLD_CTX0 = 0;
    PLIC_ENABLE_CTX0 = 1UL << UART0_IRQ;

    UART0_IER = UART_IER_RDA;

    __asm volatile ("csrs mie, %0" :: "r" (MIE_MEIE));
}

void br_hal_external_isr(void)
{
    uint32_t irq = PLIC_CLAIM_CTX0;

    if (irq == UART0_IRQ) {
        while (UART0_LSR & UART_LSR_DR) {
            br_uart_rx_push((char)UART0_RBR);
        }
    }

    PLIC_CLAIM_CTX0 = irq;
}

void br_uart_putc(char c)
{
    while (!(UART0_LSR & UART_LSR_THRE)) { }
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

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>

#include "bedrock/br_hal.h"
#include "br_avr_isr.h"

#define BAUD  115200UL

/* Double speed mode, UBRR rounded to the nearest value */
#define UBRR_VALUE  ((BR_HAL_SYS_CLOCK_HZ + 4UL * BAUD) / (8UL * BAUD) - 1UL)

extern void br_uart_rx_push(char c);

void br_uart_init(void)
{
    UCSR0A = _BV(U2X0);
    UBRR0  = (uint16_t)UBRR_VALUE;
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
}

/* The interrupt stays pending while there is a received byte, so one byte is
 * read per interrupt. */
void br_avr_usart_rx_isr(void)
{
    char c = (char)UDR0;
    br_uart_rx_push(c);
}

BR_AVR_KERNEL_ISR(USART_RX_vect_num, br_avr_usart_rx_isr);

void br_uart_putc(char c)
{
    while (!(UCSR0A & _BV(UDRE0))) { }
    UDR0 = (uint8_t)c;
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

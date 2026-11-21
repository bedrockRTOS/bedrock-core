/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>

#include "bedrock/br_hal.h"

/*
 * Port 0 is PORTB (PB0..PB7), port 1 is PORTC (PC0..PC6) and port 2 is
 * PORTD (PD0..PD7).
 */

#define PORT_B  0
#define PORT_C  1
#define PORT_D  2

static bool pin_valid(uint8_t port, uint8_t pin)
{
    if (port == PORT_C) {
        return pin < 7;
    }
    return (port == PORT_B || port == PORT_D) && pin < 8;
}

static void set_bit(volatile uint8_t *reg, uint8_t mask, bool on)
{
    uint8_t sreg = SREG;
    __asm__ volatile ("cli" ::: "memory");
    if (on) {
        *reg |= mask;
    } else {
        *reg &= (uint8_t)~mask;
    }
    SREG = sreg;
}

static volatile uint8_t *ddr(uint8_t port)
{
    return port == PORT_B ? &DDRB : port == PORT_C ? &DDRC : &DDRD;
}

static volatile uint8_t *out(uint8_t port)
{
    return port == PORT_B ? &PORTB : port == PORT_C ? &PORTC : &PORTD;
}

static volatile uint8_t *in(uint8_t port)
{
    return port == PORT_B ? &PINB : port == PORT_C ? &PINC : &PIND;
}

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode)
{
    if (!pin_valid(port, pin)) {
        return BR_ERR_INVALID;
    }

    uint8_t mask = (uint8_t)(1U << pin);

    switch (mode) {
    case BR_GPIO_OUTPUT:
        set_bit(ddr(port), mask, true);
        if (!(*ddr(port) & mask)) {
            /* The port does not respond, as in QEMU */
            return BR_ERR_IO;
        }
        break;
    case BR_GPIO_INPUT:
        set_bit(ddr(port), mask, false);
        set_bit(out(port), mask, false);
        break;
    case BR_GPIO_INPUT_PULLUP:
        set_bit(ddr(port), mask, false);
        set_bit(out(port), mask, true);
        break;
    default:
        return BR_ERR_INVALID;
    }
    return BR_OK;
}

br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level)
{
    if (!pin_valid(port, pin)) {
        return BR_ERR_INVALID;
    }

    set_bit(out(port), (uint8_t)(1U << pin), level);
    return BR_OK;
}

br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level)
{
    if (!pin_valid(port, pin) || level == NULL) {
        return BR_ERR_INVALID;
    }

    /* An output reads back the level it drives, an input the level of the pin */
    uint8_t mask = (uint8_t)(1U << pin);
    volatile uint8_t *reg = (*ddr(port) & mask) ? out(port) : in(port);

    *level = (*reg & mask) != 0;
    return BR_OK;
}

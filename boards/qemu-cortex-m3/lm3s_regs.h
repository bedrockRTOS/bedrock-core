/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LM3S_REGS_H
#define LM3S_REGS_H

#include <stdint.h>

#define LM3S_REG(a)         (*(volatile uint32_t *)(a))

#define SYSCTL_RCC          LM3S_REG(0x400FE060UL)
#define SYSCTL_RCGC0        LM3S_REG(0x400FE100UL)
#define SYSCTL_RCGC1        LM3S_REG(0x400FE104UL)
#define SYSCTL_RCGC2        LM3S_REG(0x400FE108UL)

#define GPIO_DIR            0x400UL
#define GPIO_AFSEL          0x420UL
#define GPIO_DR2R           0x500UL
#define GPIO_ODR            0x50CUL
#define GPIO_PUR            0x510UL
#define GPIO_PDR            0x514UL
#define GPIO_DEN            0x51CUL

#define LM3S_GPIO_PORTS     7
#define LM3S_GPIO_PINS      8

#define LM3S_WAIT_LIMIT     1000000UL

static inline uint32_t lm3s_gpio_base(uint8_t port)
{
    static const uint32_t base[LM3S_GPIO_PORTS] = {
        0x40004000UL, 0x40005000UL, 0x40006000UL, 0x40007000UL,
        0x40024000UL, 0x40025000UL, 0x40026000UL,
    };
    return base[port];
}

static inline void lm3s_gpio_clock(uint8_t port)
{
    SYSCTL_RCGC2 |= 1UL << port;
    (void)SYSCTL_RCGC2;
}

static inline void lm3s_pin_function(uint8_t port, uint8_t pin, int open_drain)
{
    uint32_t base = lm3s_gpio_base(port);
    uint32_t bit  = 1UL << pin;

    lm3s_gpio_clock(port);
    LM3S_REG(base + GPIO_AFSEL) |= bit;
    LM3S_REG(base + GPIO_DEN)   |= bit;
    if (open_drain) {
        LM3S_REG(base + GPIO_ODR) |= bit;
    }
}

#endif

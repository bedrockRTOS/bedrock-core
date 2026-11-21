/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef RP2040_REGS_H
#define RP2040_REGS_H

#include <stdint.h>

#define RP_REG(a)           (*(volatile uint32_t *)(a))

#define RP_CLK_PERI_HZ      12000000UL

#define RESETS_RESET_CLR_REG  RP_REG(0x4000F000UL)
#define RESETS_RESET_DONE_REG RP_REG(0x4000C008UL)

#define RESET_I2C0          (1UL << 3)
#define RESET_PWM           (1UL << 14)
#define RESET_SPI0          (1UL << 16)

#define SIO_GPIO_IN         RP_REG(0xD0000004UL)
#define SIO_GPIO_OUT_SET    RP_REG(0xD0000014UL)
#define SIO_GPIO_OUT_CLR    RP_REG(0xD0000018UL)
#define SIO_GPIO_OE_SET     RP_REG(0xD0000024UL)
#define SIO_GPIO_OE_CLR     RP_REG(0xD0000028UL)

#define IO_BANK0_BASE       0x40014000UL
#define IO_BANK0_CTRL(n)    RP_REG(IO_BANK0_BASE + 0x04UL + 8UL * (n))
#define PADS_BANK0_GPIO(n)  RP_REG(0x4001C000UL + 0x04UL + 4UL * (n))

#define GPIO_FUNC_SPI       1UL
#define GPIO_FUNC_I2C       3UL
#define GPIO_FUNC_PWM       4UL
#define GPIO_FUNC_SIO       5UL

#define PAD_SCHMITT         (1UL << 1)
#define PAD_PDE             (1UL << 2)
#define PAD_PUE             (1UL << 3)
#define PAD_IE              (1UL << 6)

#define RP_GPIO_COUNT       30
#define RP_WAIT_LIMIT       1000000UL

static inline void rp_unreset(uint32_t mask)
{
    RESETS_RESET_CLR_REG = mask;
    while ((RESETS_RESET_DONE_REG & mask) != mask) { }
}

static inline void rp_pin_function(uint8_t gpio, uint32_t func, uint32_t pad)
{
    PADS_BANK0_GPIO(gpio) = pad | PAD_IE | PAD_SCHMITT;
    IO_BANK0_CTRL(gpio)   = func;
}

#endif

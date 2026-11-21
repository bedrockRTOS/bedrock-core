/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef STM32_REGS_H
#define STM32_REGS_H

#include <stdint.h>

#define STM32_REG(a)        (*(volatile uint32_t *)(a))

#define RCC_APB2ENR_REG     STM32_REG(0x40021018UL)
#define RCC_APB1ENR_REG     STM32_REG(0x4002101CUL)

#define RCC_APB2ENR_IOPEN(p) (1UL << (2 + (p)))
#define RCC_APB2ENR_SPI1EN   (1UL << 12)
#define RCC_APB1ENR_TIM2EN   (1UL << 0)
#define RCC_APB1ENR_TIM3EN   (1UL << 1)
#define RCC_APB1ENR_I2C1EN   (1UL << 21)

#define STM32_GPIO_PORTS    5
#define STM32_GPIO_PINS     16

#define GPIO_CRL            0x00UL
#define GPIO_CRH            0x04UL
#define GPIO_IDR            0x08UL
#define GPIO_ODR            0x0CUL
#define GPIO_BSRR           0x10UL

#define GPIO_CFG_INPUT_FLOAT   0x4UL
#define GPIO_CFG_INPUT_PULL    0x8UL
#define GPIO_CFG_OUTPUT_PP_2M  0x2UL
#define GPIO_CFG_AF_PP_50M     0xBUL
#define GPIO_CFG_AF_OD_50M     0xFUL

#define STM32_WAIT_LIMIT    1000000UL

#define STM32_APB1_HZ       (BR_HAL_SYS_CLOCK_HZ / 2UL)
#define STM32_APB2_HZ       BR_HAL_SYS_CLOCK_HZ
#define STM32_TIM_HZ        BR_HAL_SYS_CLOCK_HZ

static inline uint32_t stm32_gpio_base(uint8_t port)
{
    return 0x40010800UL + 0x400UL * port;
}

static inline void stm32_gpio_clock(uint8_t port)
{
    RCC_APB2ENR_REG |= RCC_APB2ENR_IOPEN(port);
    (void)RCC_APB2ENR_REG;
}

static inline void stm32_gpio_cfg(uint8_t port, uint8_t pin, uint32_t cfg)
{
    uint32_t reg   = stm32_gpio_base(port) + (pin < 8 ? GPIO_CRL : GPIO_CRH);
    uint32_t shift = (pin & 7U) * 4U;

    stm32_gpio_clock(port);
    STM32_REG(reg) = (STM32_REG(reg) & ~(0xFUL << shift)) | (cfg << shift);
}

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef STM32F4_REGS_H
#define STM32F4_REGS_H

#include <stdint.h>

#define STM32_REG(a)        (*(volatile uint32_t *)(a))

#define RCC_AHB1ENR_REG     STM32_REG(0x40023830UL)
#define RCC_APB1ENR_REG     STM32_REG(0x40023840UL)
#define RCC_APB2ENR_REG     STM32_REG(0x40023844UL)

#define RCC_APB2ENR_USART1EN (1UL << 4)
#define RCC_APB2ENR_SPI1EN   (1UL << 12)
#define RCC_APB1ENR_TIM2EN   (1UL << 0)
#define RCC_APB1ENR_TIM3EN   (1UL << 1)
#define RCC_APB1ENR_I2C1EN   (1UL << 21)

#define STM32_GPIO_PORTS    9
#define STM32_GPIO_PINS     16

#define GPIO_MODER          0x00UL
#define GPIO_OTYPER         0x04UL
#define GPIO_OSPEEDR        0x08UL
#define GPIO_PUPDR          0x0CUL
#define GPIO_IDR            0x10UL
#define GPIO_ODR            0x14UL
#define GPIO_BSRR           0x18UL
#define GPIO_AFRL           0x20UL
#define GPIO_AFRH           0x24UL

#define GPIO_MODE_INPUT     0UL
#define GPIO_MODE_OUTPUT    1UL
#define GPIO_MODE_AF        2UL

#define GPIO_PULL_NONE      0UL
#define GPIO_PULL_UP        1UL
#define GPIO_PULL_DOWN      2UL

#define GPIO_SPEED_HIGH     2UL

#define STM32_WAIT_LIMIT    1000000UL

#define STM32_APB1_HZ       BR_HAL_SYS_CLOCK_HZ
#define STM32_APB2_HZ       BR_HAL_SYS_CLOCK_HZ
#define STM32_TIM_HZ        BR_HAL_SYS_CLOCK_HZ

static inline uint32_t stm32_gpio_base(uint8_t port)
{
    return 0x40020000UL + 0x400UL * port;
}

static inline void stm32_field_set(uint32_t reg, uint32_t shift, uint32_t width_mask, uint32_t value)
{
    STM32_REG(reg) = (STM32_REG(reg) & ~(width_mask << shift)) | (value << shift);
}

static inline void stm32_gpio_pin(uint8_t port, uint8_t pin, uint32_t mode, uint32_t pull,
                                  uint32_t af, int open_drain)
{
    uint32_t base = stm32_gpio_base(port);

    RCC_AHB1ENR_REG |= 1UL << port;
    (void)RCC_AHB1ENR_REG;

    if (mode == GPIO_MODE_AF) {
        uint32_t reg = base + (pin < 8 ? GPIO_AFRL : GPIO_AFRH);
        stm32_field_set(reg, (pin & 7U) * 4U, 0xFUL, af);
    }
    stm32_field_set(base + GPIO_OSPEEDR, pin * 2U, 3UL, GPIO_SPEED_HIGH);
    stm32_field_set(base + GPIO_PUPDR, pin * 2U, 3UL, pull);
    stm32_field_set(base + GPIO_OTYPER, pin, 1UL, open_drain ? 1UL : 0UL);
    stm32_field_set(base + GPIO_MODER, pin * 2U, 3UL, mode);
}

#endif

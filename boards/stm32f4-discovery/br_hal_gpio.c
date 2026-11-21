/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "stm32f4_regs.h"

static bool pin_valid(uint8_t port, uint8_t pin)
{
    return port < STM32_GPIO_PORTS && pin < STM32_GPIO_PINS;
}

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode)
{
    if (!pin_valid(port, pin) || mode > BR_GPIO_OUTPUT) {
        return BR_ERR_INVALID;
    }

    switch (mode) {
    case BR_GPIO_INPUT:
        stm32_gpio_pin(port, pin, GPIO_MODE_INPUT, GPIO_PULL_NONE, 0, 0);
        break;
    case BR_GPIO_INPUT_PULLUP:
        stm32_gpio_pin(port, pin, GPIO_MODE_INPUT, GPIO_PULL_UP, 0, 0);
        break;
    case BR_GPIO_INPUT_PULLDOWN:
        stm32_gpio_pin(port, pin, GPIO_MODE_INPUT, GPIO_PULL_DOWN, 0, 0);
        break;
    default:
        stm32_gpio_pin(port, pin, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0, 0);
        break;
    }
    return BR_OK;
}

br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level)
{
    if (!pin_valid(port, pin)) {
        return BR_ERR_INVALID;
    }
    STM32_REG(stm32_gpio_base(port) + GPIO_BSRR) = 1UL << (level ? pin : pin + 16);
    return BR_OK;
}

br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level)
{
    if (!pin_valid(port, pin) || level == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t base = stm32_gpio_base(port);
    uint32_t reg  = ((STM32_REG(base + GPIO_MODER) >> (pin * 2U)) & 3UL) == GPIO_MODE_OUTPUT
                        ? GPIO_ODR : GPIO_IDR;
    *level = (STM32_REG(base + reg) & (1UL << pin)) != 0;
    return BR_OK;
}

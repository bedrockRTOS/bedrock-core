/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "lm3s_regs.h"

static bool pin_valid(uint8_t port, uint8_t pin)
{
    return port < LM3S_GPIO_PORTS && pin < LM3S_GPIO_PINS;
}

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode)
{
    if (!pin_valid(port, pin) || mode > BR_GPIO_OUTPUT) {
        return BR_ERR_INVALID;
    }

    uint32_t base = lm3s_gpio_base(port);
    uint32_t bit  = 1UL << pin;

    lm3s_gpio_clock(port);

    LM3S_REG(base + GPIO_AFSEL) &= ~bit;

    if (mode == BR_GPIO_OUTPUT) {
        LM3S_REG(base + GPIO_DR2R) |= bit;
        LM3S_REG(base + GPIO_DIR)  |= bit;
    } else {
        LM3S_REG(base + GPIO_DIR) &= ~bit;
    }

    if (mode == BR_GPIO_INPUT_PULLUP) {
        LM3S_REG(base + GPIO_PDR) &= ~bit;
        LM3S_REG(base + GPIO_PUR) |= bit;
    } else if (mode == BR_GPIO_INPUT_PULLDOWN) {
        LM3S_REG(base + GPIO_PUR) &= ~bit;
        LM3S_REG(base + GPIO_PDR) |= bit;
    } else {
        LM3S_REG(base + GPIO_PUR) &= ~bit;
        LM3S_REG(base + GPIO_PDR) &= ~bit;
    }

    LM3S_REG(base + GPIO_DEN) |= bit;
    return BR_OK;
}

br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level)
{
    if (!pin_valid(port, pin)) {
        return BR_ERR_INVALID;
    }

    uint32_t bit = 1UL << pin;
    LM3S_REG(lm3s_gpio_base(port) + (bit << 2)) = level ? bit : 0;
    return BR_OK;
}

br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level)
{
    if (!pin_valid(port, pin) || level == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t bit = 1UL << pin;
    *level = (LM3S_REG(lm3s_gpio_base(port) + (bit << 2)) & bit) != 0;
    return BR_OK;
}

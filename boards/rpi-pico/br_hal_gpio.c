/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "rp2040_regs.h"

static bool pin_valid(uint8_t port, uint8_t pin)
{
    return port == 0 && pin < RP_GPIO_COUNT;
}

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode)
{
    if (!pin_valid(port, pin) || mode > BR_GPIO_OUTPUT) {
        return BR_ERR_INVALID;
    }

    uint32_t pad = 0;
    if (mode == BR_GPIO_INPUT_PULLUP) {
        pad = PAD_PUE;
    } else if (mode == BR_GPIO_INPUT_PULLDOWN) {
        pad = PAD_PDE;
    }

    rp_pin_function(pin, GPIO_FUNC_SIO, pad);

    if (mode == BR_GPIO_OUTPUT) {
        SIO_GPIO_OE_SET = 1UL << pin;
    } else {
        SIO_GPIO_OE_CLR = 1UL << pin;
    }
    return BR_OK;
}

br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level)
{
    if (!pin_valid(port, pin)) {
        return BR_ERR_INVALID;
    }
    if (level) {
        SIO_GPIO_OUT_SET = 1UL << pin;
    } else {
        SIO_GPIO_OUT_CLR = 1UL << pin;
    }
    return BR_OK;
}

br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level)
{
    if (!pin_valid(port, pin) || level == NULL) {
        return BR_ERR_INVALID;
    }
    *level = (SIO_GPIO_IN & (1UL << pin)) != 0;
    return BR_OK;
}

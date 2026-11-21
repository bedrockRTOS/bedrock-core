/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define HOST_GPIO_PORTS  2
#define HOST_GPIO_PINS   32

static br_gpio_mode_t mode[HOST_GPIO_PORTS][HOST_GPIO_PINS];
static bool           latch[HOST_GPIO_PORTS][HOST_GPIO_PINS];

static bool pin_valid(uint8_t port, uint8_t pin)
{
    return port < HOST_GPIO_PORTS && pin < HOST_GPIO_PINS;
}

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t m)
{
    if (!pin_valid(port, pin) || m > BR_GPIO_OUTPUT) {
        return BR_ERR_INVALID;
    }
    mode[port][pin] = m;
    return BR_OK;
}

br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level)
{
    if (!pin_valid(port, pin)) {
        return BR_ERR_INVALID;
    }
    latch[port][pin] = level;
    return BR_OK;
}

br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level)
{
    if (!pin_valid(port, pin) || level == NULL) {
        return BR_ERR_INVALID;
    }

    switch (mode[port][pin]) {
    case BR_GPIO_OUTPUT:         *level = latch[port][pin]; break;
    case BR_GPIO_INPUT_PULLUP:   *level = true;             break;
    default:                     *level = false;            break;
    }
    return BR_OK;
}

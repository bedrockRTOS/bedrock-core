/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode)
{
    (void)port;
    (void)pin;
    (void)mode;
    return BR_ERR_INVALID;
}

br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level)
{
    (void)port;
    (void)pin;
    (void)level;
    return BR_ERR_INVALID;
}

br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level)
{
    (void)port;
    (void)pin;
    (void)level;
    return BR_ERR_INVALID;
}

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode)
{
    (void)bus;
    (void)hz;
    (void)mode;
    return BR_ERR_INVALID;
}

br_err_t br_hal_spi_transfer(uint8_t bus, const uint8_t *tx, uint8_t *rx,
                             size_t len)
{
    (void)bus;
    (void)tx;
    (void)rx;
    (void)len;
    return BR_ERR_INVALID;
}

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz)
{
    (void)bus;
    (void)hz;
    return BR_ERR_INVALID;
}

br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data,
                          size_t len, bool stop)
{
    (void)bus;
    (void)addr;
    (void)data;
    (void)len;
    (void)stop;
    return BR_ERR_INVALID;
}

br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    (void)bus;
    (void)addr;
    (void)data;
    (void)len;
    return BR_ERR_INVALID;
}

br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille)
{
    (void)ch;
    (void)freq_hz;
    (void)duty_permille;
    return BR_ERR_INVALID;
}

br_err_t br_hal_pwm_stop(uint8_t ch)
{
    (void)ch;
    return BR_ERR_INVALID;
}

br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge)
{
    (void)ch;
    (void)edge;
    return BR_ERR_INVALID;
}

br_err_t br_hal_capture_stop(uint8_t ch)
{
    (void)ch;
    return BR_ERR_INVALID;
}

br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us)
{
    (void)ch;
    (void)period_us;
    return BR_ERR_INVALID;
}

struct br_netif *br_hal_netif_default(void)
{
    return NULL;
}

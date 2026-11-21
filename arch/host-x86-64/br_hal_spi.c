/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define HOST_SPI_IDLE_BYTE  0xFF

static bool    initialized;
static uint8_t spi_mode;

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode)
{
    if (bus != 0 || hz == 0 || (mode & ~(BR_SPI_CPHA | BR_SPI_CPOL | BR_SPI_LOOP))) {
        return BR_ERR_INVALID;
    }
    spi_mode    = mode;
    initialized = true;
    return BR_OK;
}

br_err_t br_hal_spi_transfer(uint8_t bus, const uint8_t *tx, uint8_t *rx,
                             size_t len)
{
    if (bus != 0 || !initialized) {
        return BR_ERR_INVALID;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t out = tx != NULL ? tx[i] : HOST_SPI_IDLE_BYTE;
        if (rx != NULL) {
            rx[i] = (spi_mode & BR_SPI_LOOP) ? out : HOST_SPI_IDLE_BYTE;
        }
    }
    return BR_OK;
}

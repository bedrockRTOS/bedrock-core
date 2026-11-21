/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_SPI_H
#define BR_SPI_H

#include "bedrock/bedrock.h"

#define BR_SPI_IOCTL_SET_SPEED  0x0201
#define BR_SPI_IOCTL_SET_MODE   0x0202
#define BR_SPI_IOCTL_TRANSFER   0x0203

typedef struct {
    const uint8_t *tx;
    uint8_t       *rx;
    size_t         len;
} br_spi_xfer_t;

typedef struct {
    br_device_t dev;
    uint8_t     bus;
    uint32_t    hz;
    uint8_t     mode;
    br_mutex_t  lock;
} br_spi_dev_t;

br_err_t br_spi_register(br_spi_dev_t *spi, const char *name, uint8_t bus);
br_err_t br_spi_set_speed(br_device_t *dev, uint32_t hz);
br_err_t br_spi_set_mode(br_device_t *dev, uint8_t mode);
br_err_t br_spi_transfer(br_device_t *dev, const void *tx, void *rx, size_t len);

#endif

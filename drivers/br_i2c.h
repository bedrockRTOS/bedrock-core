/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_I2C_H
#define BR_I2C_H

#include "bedrock/bedrock.h"

#define BR_I2C_IOCTL_SET_ADDR    0x0301
#define BR_I2C_IOCTL_SET_SPEED   0x0302
#define BR_I2C_IOCTL_WRITE_READ  0x0303

typedef struct {
    uint8_t        addr;
    const uint8_t *wbuf;
    size_t         wlen;
    uint8_t       *rbuf;
    size_t         rlen;
} br_i2c_xfer_t;

typedef struct {
    br_device_t dev;
    uint8_t     bus;
    uint8_t     addr;
    uint32_t    hz;
    br_mutex_t  lock;
} br_i2c_dev_t;

br_err_t br_i2c_register(br_i2c_dev_t *i2c, const char *name, uint8_t bus);
br_err_t br_i2c_set_addr(br_device_t *dev, uint8_t addr);
br_err_t br_i2c_set_speed(br_device_t *dev, uint32_t hz);
br_err_t br_i2c_write_read(br_device_t *dev, uint8_t addr,
                           const void *wbuf, size_t wlen,
                           void *rbuf, size_t rlen);

#endif

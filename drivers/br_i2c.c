/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_i2c.h"

#define I2C_DEFAULT_HZ  100000U
#define I2C_ADDR_MAX    0x7F

static br_err_t i2c_open(br_device_t *dev)
{
    br_i2c_dev_t *i2c = dev->priv;
    return br_hal_i2c_init(i2c->bus, i2c->hz);
}

static br_err_t i2c_xfer(br_i2c_dev_t *i2c, const br_i2c_xfer_t *x,
                         br_time_t timeout)
{
    if (x->addr > I2C_ADDR_MAX) {
        return BR_ERR_INVALID;
    }

    br_err_t err = br_mutex_lock(&i2c->lock, timeout);
    if (err != BR_OK) {
        return err;
    }

    err = BR_OK;
    if (x->wlen > 0) {
        err = br_hal_i2c_write(i2c->bus, x->addr, x->wbuf, x->wlen, x->rlen == 0);
    }
    if (err == BR_OK && x->rlen > 0) {
        err = br_hal_i2c_read(i2c->bus, x->addr, x->rbuf, x->rlen);
    }

    br_mutex_unlock(&i2c->lock);
    return err;
}

static int i2c_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    br_i2c_dev_t *i2c = dev->priv;
    br_i2c_xfer_t x = { .addr = i2c->addr, .rbuf = buf, .rlen = len };
    br_err_t err = i2c_xfer(i2c, &x, timeout);
    return err == BR_OK ? (int)len : (int)err;
}

static int i2c_write(br_device_t *dev, const void *buf, size_t len,
                     br_time_t timeout)
{
    br_i2c_dev_t *i2c = dev->priv;
    br_i2c_xfer_t x = { .addr = i2c->addr, .wbuf = buf, .wlen = len };
    br_err_t err = i2c_xfer(i2c, &x, timeout);
    return err == BR_OK ? (int)len : (int)err;
}

static br_err_t i2c_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    br_i2c_dev_t *i2c = dev->priv;

    if (arg == NULL) {
        return BR_ERR_INVALID;
    }

    switch (cmd) {
    case BR_I2C_IOCTL_SET_ADDR: {
        uint8_t addr = *(uint8_t *)arg;
        if (addr > I2C_ADDR_MAX) {
            return BR_ERR_INVALID;
        }
        i2c->addr = addr;
        return BR_OK;
    }
    case BR_I2C_IOCTL_SET_SPEED: {
        uint32_t hz = *(uint32_t *)arg;
        br_err_t err = br_mutex_lock(&i2c->lock, BR_TIME_INFINITE);
        if (err != BR_OK) {
            return err;
        }
        err = br_hal_i2c_init(i2c->bus, hz);
        if (err == BR_OK) {
            i2c->hz = hz;
        }
        br_mutex_unlock(&i2c->lock);
        return err;
    }
    case BR_I2C_IOCTL_WRITE_READ:
        return i2c_xfer(i2c, arg, BR_TIME_INFINITE);
    default:
        return BR_ERR_INVALID;
    }
}

static const br_device_ops_t i2c_ops = {
    .open  = i2c_open,
    .read  = i2c_read,
    .write = i2c_write,
    .ioctl = i2c_ioctl,
};

br_err_t br_i2c_register(br_i2c_dev_t *i2c, const char *name, uint8_t bus)
{
    if (i2c == NULL) {
        return BR_ERR_INVALID;
    }
    i2c->bus  = bus;
    i2c->addr = 0;
    i2c->hz   = I2C_DEFAULT_HZ;
    br_mutex_init(&i2c->lock);
    return br_device_register(&i2c->dev, name, &i2c_ops, i2c);
}

br_err_t br_i2c_set_addr(br_device_t *dev, uint8_t addr)
{
    return br_device_ioctl(dev, BR_I2C_IOCTL_SET_ADDR, &addr);
}

br_err_t br_i2c_set_speed(br_device_t *dev, uint32_t hz)
{
    return br_device_ioctl(dev, BR_I2C_IOCTL_SET_SPEED, &hz);
}

br_err_t br_i2c_write_read(br_device_t *dev, uint8_t addr,
                           const void *wbuf, size_t wlen,
                           void *rbuf, size_t rlen)
{
    br_i2c_xfer_t x = {
        .addr = addr, .wbuf = wbuf, .wlen = wlen, .rbuf = rbuf, .rlen = rlen,
    };
    return br_device_ioctl(dev, BR_I2C_IOCTL_WRITE_READ, &x);
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_spi.h"

#define SPI_DEFAULT_HZ  1000000U

static br_err_t spi_open(br_device_t *dev)
{
    br_spi_dev_t *spi = dev->priv;
    return br_hal_spi_init(spi->bus, spi->hz, spi->mode);
}

static br_err_t spi_run(br_spi_dev_t *spi, const uint8_t *tx, uint8_t *rx,
                        size_t len, br_time_t timeout)
{
    br_err_t err = br_mutex_lock(&spi->lock, timeout);
    if (err != BR_OK) {
        return err;
    }
    err = br_hal_spi_transfer(spi->bus, tx, rx, len);
    br_mutex_unlock(&spi->lock);
    return err;
}

static int spi_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    br_err_t err = spi_run(dev->priv, NULL, buf, len, timeout);
    return err == BR_OK ? (int)len : (int)err;
}

static int spi_write(br_device_t *dev, const void *buf, size_t len,
                     br_time_t timeout)
{
    br_err_t err = spi_run(dev->priv, buf, NULL, len, timeout);
    return err == BR_OK ? (int)len : (int)err;
}

static br_err_t spi_reconfigure(br_spi_dev_t *spi, uint32_t hz, uint8_t mode)
{
    br_err_t err = br_mutex_lock(&spi->lock, BR_TIME_INFINITE);
    if (err != BR_OK) {
        return err;
    }
    err = br_hal_spi_init(spi->bus, hz, mode);
    if (err == BR_OK) {
        spi->hz   = hz;
        spi->mode = mode;
    }
    br_mutex_unlock(&spi->lock);
    return err;
}

static br_err_t spi_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    br_spi_dev_t *spi = dev->priv;

    if (arg == NULL) {
        return BR_ERR_INVALID;
    }

    switch (cmd) {
    case BR_SPI_IOCTL_SET_SPEED:
        return spi_reconfigure(spi, *(uint32_t *)arg, spi->mode);
    case BR_SPI_IOCTL_SET_MODE:
        return spi_reconfigure(spi, spi->hz, *(uint8_t *)arg);
    case BR_SPI_IOCTL_TRANSFER: {
        br_spi_xfer_t *x = arg;
        return spi_run(spi, x->tx, x->rx, x->len, BR_TIME_INFINITE);
    }
    default:
        return BR_ERR_INVALID;
    }
}

static const br_device_ops_t spi_ops = {
    .open  = spi_open,
    .read  = spi_read,
    .write = spi_write,
    .ioctl = spi_ioctl,
};

br_err_t br_spi_register(br_spi_dev_t *spi, const char *name, uint8_t bus)
{
    if (spi == NULL) {
        return BR_ERR_INVALID;
    }
    spi->bus  = bus;
    spi->hz   = SPI_DEFAULT_HZ;
    spi->mode = 0;
    br_mutex_init(&spi->lock);
    br_device_user_allow_ioctl(BR_SPI_IOCTL_SET_SPEED, sizeof(uint32_t));
    br_device_user_allow_ioctl(BR_SPI_IOCTL_SET_MODE, sizeof(uint8_t));
    return br_device_register(&spi->dev, name, &spi_ops, spi);
}

br_err_t br_spi_set_speed(br_device_t *dev, uint32_t hz)
{
    return br_device_ioctl(dev, BR_SPI_IOCTL_SET_SPEED, &hz);
}

br_err_t br_spi_set_mode(br_device_t *dev, uint8_t mode)
{
    return br_device_ioctl(dev, BR_SPI_IOCTL_SET_MODE, &mode);
}

br_err_t br_spi_transfer(br_device_t *dev, const void *tx, void *rx, size_t len)
{
    br_spi_xfer_t x = { .tx = tx, .rx = rx, .len = len };
    return br_device_ioctl(dev, BR_SPI_IOCTL_TRANSFER, &x);
}

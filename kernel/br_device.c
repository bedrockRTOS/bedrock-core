/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

static br_device_t *device_list;

static bool name_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

br_err_t br_device_register(br_device_t *dev, const char *name,
                            const br_device_ops_t *ops, void *priv)
{
    if (dev == NULL || name == NULL || ops == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    for (br_device_t *d = device_list; d != NULL; d = d->next) {
        if (d == dev || name_equal(d->name, name)) {
            br_hal_irq_restore(key);
            return BR_ERR_BUSY;
        }
    }

    dev->name       = name;
    dev->ops        = ops;
    dev->priv       = priv;
    dev->open_count = 0;
    dev->next       = device_list;
    device_list     = dev;

    br_hal_irq_restore(key);
    return BR_OK;
}

br_device_t *br_device_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    uint32_t key = br_hal_irq_disable();

    br_device_t *d = device_list;
    while (d != NULL && !name_equal(d->name, name)) {
        d = d->next;
    }

    br_hal_irq_restore(key);
    return d;
}

br_err_t br_device_open(br_device_t *dev)
{
    if (dev == NULL || br_hal_in_isr()) {
        return BR_ERR_INVALID;
    }
    if (dev->open_count == UINT8_MAX) {
        return BR_ERR_OVERFLOW;
    }

    if (dev->open_count == 0 && dev->ops->open != NULL) {
        br_err_t err = dev->ops->open(dev);
        if (err != BR_OK) {
            return err;
        }
    }

    dev->open_count++;
    return BR_OK;
}

br_err_t br_device_close(br_device_t *dev)
{
    if (dev == NULL || dev->open_count == 0) {
        return BR_ERR_INVALID;
    }

    if (dev->open_count == 1 && dev->ops->close != NULL) {
        br_err_t err = dev->ops->close(dev);
        if (err != BR_OK) {
            return err;
        }
    }

    dev->open_count--;
    return BR_OK;
}

int br_device_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    if (dev == NULL || buf == NULL || dev->open_count == 0 ||
        dev->ops->read == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->read(dev, buf, len, timeout);
}

int br_device_write(br_device_t *dev, const void *buf, size_t len,
                    br_time_t timeout)
{
    if (dev == NULL || buf == NULL || dev->open_count == 0 ||
        dev->ops->write == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->write(dev, buf, len, timeout);
}

br_err_t br_device_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    if (dev == NULL || dev->open_count == 0 || dev->ops->ioctl == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->ioctl(dev, cmd, arg);
}

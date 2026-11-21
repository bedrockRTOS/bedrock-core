/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_DEVICE_H
#define BR_DEVICE_H

#include "br_types.h"

struct br_device;

typedef struct {
    br_err_t (*open)(struct br_device *dev);
    br_err_t (*close)(struct br_device *dev);
    int      (*read)(struct br_device *dev, void *buf, size_t len,
                     br_time_t timeout);
    int      (*write)(struct br_device *dev, const void *buf, size_t len,
                      br_time_t timeout);
    br_err_t (*ioctl)(struct br_device *dev, uint32_t cmd, void *arg);
} br_device_ops_t;

typedef struct br_device {
    const char            *name;
    const br_device_ops_t *ops;
    void                  *priv;
    uint8_t                open_count;
    bool                   user_ok;
    struct br_device      *next;
} br_device_t;

#endif

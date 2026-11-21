/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_BLKDEV_H
#define BR_BLKDEV_H

#include "br_types.h"

struct br_blkdev;

typedef struct {
    br_err_t (*read)(struct br_blkdev *dev, uint32_t block, uint32_t off,
                     void *buf, size_t len);
    br_err_t (*write)(struct br_blkdev *dev, uint32_t block, uint32_t off,
                      const void *buf, size_t len);
    br_err_t (*erase)(struct br_blkdev *dev, uint32_t block);
    br_err_t (*sync)(struct br_blkdev *dev);
} br_blkdev_ops_t;

typedef struct br_blkdev {
    const br_blkdev_ops_t *ops;
    void                  *priv;
    uint32_t               block_size;
    uint32_t               block_count;
    uint32_t               prog_size;
} br_blkdev_t;

static inline br_err_t br_blkdev_read(br_blkdev_t *dev, uint32_t block,
                                      uint32_t off, void *buf, size_t len)
{
    if (dev == NULL || buf == NULL || block >= dev->block_count ||
        off > dev->block_size || len > dev->block_size - off) {
        return BR_ERR_INVALID;
    }
    return dev->ops->read(dev, block, off, buf, len);
}

static inline br_err_t br_blkdev_write(br_blkdev_t *dev, uint32_t block,
                                       uint32_t off, const void *buf, size_t len)
{
    if (dev == NULL || buf == NULL || block >= dev->block_count ||
        off > dev->block_size || len > dev->block_size - off ||
        off % dev->prog_size != 0 || len % dev->prog_size != 0) {
        return BR_ERR_INVALID;
    }
    return dev->ops->write(dev, block, off, buf, len);
}

static inline br_err_t br_blkdev_erase(br_blkdev_t *dev, uint32_t block)
{
    if (dev == NULL || block >= dev->block_count) {
        return BR_ERR_INVALID;
    }
    return dev->ops->erase(dev, block);
}

static inline br_err_t br_blkdev_sync(br_blkdev_t *dev)
{
    if (dev == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->sync(dev);
}

#endif

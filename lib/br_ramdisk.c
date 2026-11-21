/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_ramdisk.h"

static br_err_t ram_read(br_blkdev_t *dev, uint32_t block, uint32_t off,
                         void *buf, size_t len)
{
    br_ramdisk_t *rd = dev->priv;
    const uint8_t *src = rd->mem + (size_t)block * dev->block_size + off;
    uint8_t *dst = buf;

    for (size_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    return BR_OK;
}

static br_err_t ram_write(br_blkdev_t *dev, uint32_t block, uint32_t off,
                          const void *buf, size_t len)
{
    br_ramdisk_t *rd = dev->priv;

    if (rd->writes_until_fail == 0) {
        return BR_ERR_IO;
    }
    if (rd->writes_until_fail > 0) {
        rd->writes_until_fail--;
    }

    uint8_t *dst = rd->mem + (size_t)block * dev->block_size + off;
    const uint8_t *src = buf;

    for (size_t i = 0; i < len; i++) {
        dst[i] &= src[i];
    }
    return BR_OK;
}

static br_err_t ram_erase(br_blkdev_t *dev, uint32_t block)
{
    br_ramdisk_t *rd = dev->priv;
    uint8_t *dst = rd->mem + (size_t)block * dev->block_size;

    for (uint32_t i = 0; i < dev->block_size; i++) {
        dst[i] = 0xFF;
    }
    return BR_OK;
}

static br_err_t ram_sync(br_blkdev_t *dev)
{
    (void)dev;
    return BR_OK;
}

static const br_blkdev_ops_t ram_ops = {
    .read  = ram_read,
    .write = ram_write,
    .erase = ram_erase,
    .sync  = ram_sync,
};

br_err_t br_ramdisk_init(br_ramdisk_t *rd, void *mem, uint32_t block_size,
                         uint32_t block_count)
{
    if (rd == NULL || mem == NULL || block_count == 0 ||
        block_size < BR_RAMDISK_PROG_SIZE ||
        block_size % BR_RAMDISK_PROG_SIZE != 0) {
        return BR_ERR_INVALID;
    }

    rd->mem                 = mem;
    rd->writes_until_fail   = -1;
    rd->dev.ops             = &ram_ops;
    rd->dev.priv            = rd;
    rd->dev.block_size      = block_size;
    rd->dev.block_count     = block_count;
    rd->dev.prog_size       = BR_RAMDISK_PROG_SIZE;

    for (uint32_t b = 0; b < block_count; b++) {
        ram_erase(&rd->dev, b);
    }
    return BR_OK;
}

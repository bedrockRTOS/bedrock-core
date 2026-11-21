/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_RAMDISK_H
#define BR_RAMDISK_H

#include "bedrock/bedrock.h"
#include "bedrock/br_blkdev.h"

#define BR_RAMDISK_PROG_SIZE  4

typedef struct {
    br_blkdev_t  dev;
    uint8_t     *mem;
    int32_t      writes_until_fail;
} br_ramdisk_t;

br_err_t br_ramdisk_init(br_ramdisk_t *rd, void *mem, uint32_t block_size,
                         uint32_t block_count);

#endif

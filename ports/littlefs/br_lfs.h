/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_LFS_H
#define BR_LFS_H

#ifndef LFS_DEFINES
#define LFS_DEFINES br_lfs_config.h
#endif

#include "lfs.h"
#include "bedrock/bedrock.h"
#include "bedrock/br_blkdev.h"

#ifndef BR_LFS_CACHE_SIZE
#define BR_LFS_CACHE_SIZE      64
#endif

#ifndef BR_LFS_LOOKAHEAD_SIZE
#define BR_LFS_LOOKAHEAD_SIZE  16
#endif

typedef struct {
    lfs_t              lfs;
    struct lfs_config  cfg;
    br_blkdev_t       *dev;
    br_mutex_t         lock;
    uint8_t            read_buf[BR_LFS_CACHE_SIZE];
    uint8_t            prog_buf[BR_LFS_CACHE_SIZE];
    uint8_t            lookahead_buf[BR_LFS_LOOKAHEAD_SIZE] __attribute__((aligned(4)));
} br_lfs_t;

typedef struct {
    lfs_file_t              file;
    struct lfs_file_config  cfg;
    uint8_t                 buf[BR_LFS_CACHE_SIZE];
} br_lfs_file_t;

br_err_t br_lfs_init(br_lfs_t *l, br_blkdev_t *dev);
int      br_lfs_file_open(br_lfs_t *l, br_lfs_file_t *f, const char *path, int flags);

#endif

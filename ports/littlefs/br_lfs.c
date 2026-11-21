/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_lfs.h"

#define BR_LFS_BLOCK_CYCLES  100

static int lfs_bd_read(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, void *buffer, lfs_size_t size)
{
    br_lfs_t *l = c->context;
    return br_blkdev_read(l->dev, block, off, buffer, size) == BR_OK ? 0 : LFS_ERR_IO;
}

static int lfs_bd_prog(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, const void *buffer, lfs_size_t size)
{
    br_lfs_t *l = c->context;
    return br_blkdev_write(l->dev, block, off, buffer, size) == BR_OK ? 0 : LFS_ERR_IO;
}

static int lfs_bd_erase(const struct lfs_config *c, lfs_block_t block)
{
    br_lfs_t *l = c->context;
    return br_blkdev_erase(l->dev, block) == BR_OK ? 0 : LFS_ERR_IO;
}

static int lfs_bd_sync(const struct lfs_config *c)
{
    br_lfs_t *l = c->context;
    return br_blkdev_sync(l->dev) == BR_OK ? 0 : LFS_ERR_IO;
}

static int lfs_bd_lock(const struct lfs_config *c)
{
    br_lfs_t *l = c->context;
    return br_mutex_lock(&l->lock, BR_TIME_INFINITE) == BR_OK ? 0 : LFS_ERR_IO;
}

static int lfs_bd_unlock(const struct lfs_config *c)
{
    br_lfs_t *l = c->context;
    return br_mutex_unlock(&l->lock) == BR_OK ? 0 : LFS_ERR_IO;
}

br_err_t br_lfs_init(br_lfs_t *l, br_blkdev_t *dev)
{
    if (l == NULL || dev == NULL || dev->block_count < 2 ||
        BR_LFS_CACHE_SIZE % dev->prog_size != 0 ||
        dev->block_size % BR_LFS_CACHE_SIZE != 0) {
        return BR_ERR_INVALID;
    }

    l->dev = dev;
    br_mutex_init(&l->lock);

    l->cfg = (struct lfs_config){
        .context          = l,
        .read             = lfs_bd_read,
        .prog             = lfs_bd_prog,
        .erase            = lfs_bd_erase,
        .sync             = lfs_bd_sync,
        .lock             = lfs_bd_lock,
        .unlock           = lfs_bd_unlock,
        .read_size        = dev->prog_size,
        .prog_size        = dev->prog_size,
        .block_size       = dev->block_size,
        .block_count      = dev->block_count,
        .block_cycles     = BR_LFS_BLOCK_CYCLES,
        .cache_size       = BR_LFS_CACHE_SIZE,
        .lookahead_size   = BR_LFS_LOOKAHEAD_SIZE,
        .read_buffer      = l->read_buf,
        .prog_buffer      = l->prog_buf,
        .lookahead_buffer = l->lookahead_buf,
    };
    return BR_OK;
}

int br_lfs_file_open(br_lfs_t *l, br_lfs_file_t *f, const char *path, int flags)
{
    f->cfg = (struct lfs_file_config){
        .buffer = f->buf,
    };
    return lfs_file_opencfg(&l->lfs, &f->file, path, flags, &f->cfg);
}

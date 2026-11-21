/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_FS_H
#define BR_FS_H

#include "bedrock/bedrock.h"
#include "bedrock/br_blkdev.h"

#define BR_FS_READ   0x01
#define BR_FS_WRITE  0x02
#define BR_FS_APPEND 0x04

typedef struct {
    char   name[CONFIG_FS_NAME_MAX];
    size_t size;
    bool   dir;
} br_fs_info_t;

typedef struct {
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
} br_fs_usage_t;

br_err_t br_fs_format(br_blkdev_t *dev);
br_err_t br_fs_mount(br_blkdev_t *dev);
br_err_t br_fs_unmount(void);

int      br_fs_open(const char *name, uint8_t flags);
int      br_fs_read(int fd, void *buf, size_t len);
br_err_t br_fs_seek(int fd, size_t offset);
int      br_fs_write(int fd, const void *buf, size_t len);
br_err_t br_fs_close(int fd);

br_err_t br_fs_remove(const char *name);
br_err_t br_fs_mkdir(const char *path);
br_err_t br_fs_rmdir(const char *path);
br_err_t br_fs_info(const char *path, br_fs_info_t *info);
br_err_t br_fs_readdir(const char *dir, uint32_t index, br_fs_info_t *info);
br_err_t br_fs_stat(const char *name, size_t *size);
br_err_t br_fs_list(uint32_t index, br_fs_info_t *info);
br_err_t br_fs_usage(br_fs_usage_t *usage);

#if CONFIG_MPU
br_err_t br_fs_enable_user_calls(void);
#endif

#endif

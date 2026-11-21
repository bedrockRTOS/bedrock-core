/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ff.h"
#include "diskio.h"
#include "br_ff.h"

static br_blkdev_t *disk;
static br_mutex_t   mutex[FF_VOLUMES + 1];

br_err_t br_ff_attach(br_blkdev_t *dev)
{
    if (dev == NULL || dev->block_size != FF_MIN_SS ||
        FF_MIN_SS % dev->prog_size != 0) {
        return BR_ERR_INVALID;
    }
    disk = dev;
    return BR_OK;
}

void br_ff_detach(void)
{
    disk = NULL;
}

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0 && disk != NULL) ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || disk == NULL) {
        return RES_NOTRDY;
    }

    for (UINT i = 0; i < count; i++) {
        if (br_blkdev_read(disk, (uint32_t)sector + i, 0, buff + (size_t)i * FF_MIN_SS,
                           FF_MIN_SS) != BR_OK) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || disk == NULL) {
        return RES_NOTRDY;
    }

    for (UINT i = 0; i < count; i++) {
        uint32_t block = (uint32_t)sector + i;
        if (br_blkdev_erase(disk, block) != BR_OK ||
            br_blkdev_write(disk, block, 0, buff + (size_t)i * FF_MIN_SS,
                            FF_MIN_SS) != BR_OK) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0 || disk == NULL) {
        return RES_NOTRDY;
    }

    switch (cmd) {
    case CTRL_SYNC:
        return br_blkdev_sync(disk) == BR_OK ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = disk->block_count;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

void *ff_memalloc(UINT msize)
{
    (void)msize;
    return NULL;
}

void ff_memfree(void *mblock)
{
    (void)mblock;
}

int ff_mutex_create(int vol)
{
    br_mutex_init(&mutex[vol]);
    return 1;
}

void ff_mutex_delete(int vol)
{
    (void)vol;
}

int ff_mutex_take(int vol)
{
    return br_mutex_lock(&mutex[vol], BR_MSEC(FF_FS_TIMEOUT)) == BR_OK;
}

void ff_mutex_give(int vol)
{
    br_mutex_unlock(&mutex[vol]);
}

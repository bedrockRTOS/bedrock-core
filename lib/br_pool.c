/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: LGPL-2.1
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 */

#include "br_pool.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * Static memory pool allocator
 *
 * Manages a caller-provided buffer as a pool of fixed-size
 * blocks. Zero dynamic allocation -- the buffer, block size,
 * and count are all determined at init time.
 */

#define POOL_ALIGN       (sizeof(void *))
#define POOL_ALIGN_UP(x) (((x) + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1))

struct br_pool {
    uint8_t  *buffer;
    size_t    block_size;
    size_t    total;
    size_t    used;
    void     *free_head;
    bool      initialized;
};

#ifndef BR_POOL_MAX_POOLS
#define BR_POOL_MAX_POOLS 8
#endif

static struct br_pool pools[BR_POOL_MAX_POOLS];
static uint8_t pool_count;

br_pool_handle_t br_pool_create(void *buffer, size_t buf_size,
                                size_t block_size)
{
    if (buffer == NULL || block_size == 0 || pool_count >= BR_POOL_MAX_POOLS) {
        return NULL;
    }

    size_t aligned = POOL_ALIGN_UP(block_size);
    if (aligned < sizeof(void *)) {
        aligned = sizeof(void *);
    }

    size_t count = buf_size / aligned;
    if (count == 0) {
        return NULL;
    }

    struct br_pool *pool = &pools[pool_count++];
    pool->buffer     = (uint8_t *)buffer;
    pool->block_size = aligned;
    pool->total      = count;
    pool->used       = 0;
    pool->initialized = true;

    pool->free_head = NULL;
    for (size_t i = 0; i < count; i++) {
        void *block = pool->buffer + (i * aligned);
        *(void **)block = pool->free_head;
        pool->free_head = block;
    }

    return (br_pool_handle_t)pool;
}

void *br_pool_alloc(br_pool_handle_t handle)
{
    struct br_pool *pool = (struct br_pool *)handle;
    if (pool == NULL || !pool->initialized || pool->free_head == NULL) {
        return NULL;
    }

    void *block = pool->free_head;
    pool->free_head = *(void **)block;
    pool->used++;

    memset(block, 0, pool->block_size);
    return block;
}

void br_pool_free(br_pool_handle_t handle, void *block)
{
    struct br_pool *pool = (struct br_pool *)handle;
    if (pool == NULL || !pool->initialized || block == NULL) {
        return;
    }

    uint8_t *b = (uint8_t *)block;
    if (b < pool->buffer || b >= pool->buffer + (pool->total * pool->block_size)) {
        return;
    }

    *(void **)block = pool->free_head;
    pool->free_head = block;
    if (pool->used > 0) {
        pool->used--;
    }
}

size_t br_pool_available(br_pool_handle_t handle)
{
    struct br_pool *pool = (struct br_pool *)handle;
    if (pool == NULL || !pool->initialized) {
        return 0;
    }
    return pool->total - pool->used;
}

size_t br_pool_total(br_pool_handle_t handle)
{
    struct br_pool *pool = (struct br_pool *)handle;
    if (pool == NULL || !pool->initialized) {
        return 0;
    }
    return pool->total;
}

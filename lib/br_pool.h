/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: LGPL-2.1
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef BR_POOL_H
#define BR_POOL_H

#include <stddef.h>

/* Opaque handle to a memory pool */
typedef void *br_pool_handle_t;

/* Create a pool from a caller-provided buffer.
 * Returns NULL on failure. */
br_pool_handle_t br_pool_create(void *buffer, size_t buf_size,
                                size_t block_size);

/* Allocate one block (returns NULL if pool is exhausted) */
void *br_pool_alloc(br_pool_handle_t handle);

/* Return a block to the pool */
void br_pool_free(br_pool_handle_t handle, void *block);

/* Query: free blocks remaining */
size_t br_pool_available(br_pool_handle_t handle);

/* Query: total blocks in pool */
size_t br_pool_total(br_pool_handle_t handle);

#endif /* BR_POOL_H */

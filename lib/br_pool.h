/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE-GPL-3.0.md for the full terms and runtime exception.
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

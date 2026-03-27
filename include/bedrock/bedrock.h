/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE.md for the full terms and runtime exception.
 */

#ifndef BEDROCK_H
#define BEDROCK_H

#include "br_types.h"
#include "br_hal.h"
#include "br_assert.h"

/*
 * API stability annotations
 *
 * BR_STABLE      — signature will not change within a minor version series.
 *                  Safe to use in application code.
 *
 * BR_EXPERIMENTAL — may change or be removed in a future minor version.
 *                   Compiles cleanly by default; define BR_WARN_EXPERIMENTAL
 *                   at build time to turn usage into compiler warnings.
 *
 * BR_INTERNAL    — not part of the application-facing API.
 *                  Called by the kernel or HAL layer only.
 */

#define BR_STABLE

#ifdef BR_WARN_EXPERIMENTAL
#  define BR_EXPERIMENTAL \
    __attribute__((deprecated("experimental API — may change in a future version")))
#else
#  define BR_EXPERIMENTAL
#endif

#ifdef BR_WARN_INTERNAL
#  define BR_INTERNAL \
    __attribute__((deprecated("internal API — not for application use")))
#else
#  define BR_INTERNAL
#endif

/* Kernel version */

#define BEDROCK_VERSION_MAJOR  0
#define BEDROCK_VERSION_MINOR  0
#define BEDROCK_VERSION_PATCH  3

#define BEDROCK_VERSION \
    ((BEDROCK_VERSION_MAJOR << 16) | \
     (BEDROCK_VERSION_MINOR <<  8) | \
      BEDROCK_VERSION_PATCH)

/* Kernel lifecycle */

BR_STABLE void br_kernel_init(void);
BR_STABLE void br_kernel_start(void) __attribute__((noreturn));

/* Task management */

BR_STABLE br_err_t br_task_create(br_tid_t *tid,
                                   const char *name,
                                   br_task_entry_t entry,
                                   void *arg,
                                   uint8_t priority,
                                   void *stack,
                                   size_t stack_size);

BR_STABLE br_err_t br_task_suspend(br_tid_t tid);
BR_STABLE br_err_t br_task_resume(br_tid_t tid);
BR_STABLE void     br_task_yield(void);
BR_STABLE br_tid_t br_task_self(void);

/*
 * br_task_delete — return a task's slot to the pool.
 *
 * EXPERIMENTAL: cleanup semantics for tasks that hold kernel objects
 * (mutexes, semaphores, queue slots) are still being defined and may
 * require a different signature (e.g. a cleanup callback) in a future
 * version.
 */
BR_EXPERIMENTAL br_err_t br_task_delete(br_tid_t tid);

/* Time services */

BR_STABLE void      br_sleep_us(br_time_t us);
BR_STABLE br_time_t br_uptime_us(void);

static inline void br_sleep_ms(uint32_t ms) { br_sleep_us(BR_MSEC(ms)); }
static inline void br_sleep_s(uint32_t s)   { br_sleep_us(BR_SEC(s));   }

/*
 * br_time_alarm_handler — invoked by the HAL timer ISR on alarm expiry.
 *
 * INTERNAL: application code must never call this directly.
 * Exposed here so the HAL implementation can reference it without a
 * separate internal header.
 */
BR_INTERNAL void br_time_alarm_handler(void);

/* Semaphore */

BR_STABLE br_err_t br_sem_init(br_sem_t *sem, int32_t initial, int32_t max);
BR_STABLE br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout);
BR_STABLE br_err_t br_sem_give(br_sem_t *sem);

/* Mutex (with priority inheritance) */

BR_STABLE br_err_t br_mutex_init(br_mutex_t *mtx);
BR_STABLE br_err_t br_mutex_lock(br_mutex_t *mtx, br_time_t timeout);
BR_STABLE br_err_t br_mutex_unlock(br_mutex_t *mtx);

/* Message queue */

BR_STABLE br_err_t br_mqueue_init(br_mqueue_t *mq, void *buffer,
                                   size_t msg_size, size_t max_msgs);
BR_STABLE br_err_t br_mqueue_send(br_mqueue_t *mq, const void *msg,
                                   br_time_t timeout);
BR_STABLE br_err_t br_mqueue_recv(br_mqueue_t *mq, void *msg,
                                   br_time_t timeout);

/* Panic and assertions — see br_assert.h for BR_PANIC() and br_assert() */

BR_STABLE void br_set_panic_handler(br_panic_handler_t handler);

#endif /* BEDROCK_H */

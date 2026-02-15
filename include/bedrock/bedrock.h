/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 */

#ifndef BEDROCK_H
#define BEDROCK_H

#include "br_types.h"
#include "br_hal.h"

/* bedrock[RTOS] Public API */

/* Kernel lifecycle */

void br_kernel_init(void);
void br_kernel_start(void) __attribute__((noreturn));

/* Task management */

br_err_t br_task_create(br_tid_t *tid,
                        const char *name,
                        br_task_entry_t entry,
                        void *arg,
                        uint8_t priority,
                        void *stack,
                        size_t stack_size);

br_err_t br_task_suspend(br_tid_t tid);
br_err_t br_task_resume(br_tid_t tid);
br_err_t br_task_delete(br_tid_t tid);
void br_task_yield(void);
br_tid_t br_task_self(void);

/* Time services */

void br_sleep_us(br_time_t us);

static inline void br_sleep_ms(uint32_t ms) { br_sleep_us(BR_MSEC(ms)); }
static inline void br_sleep_s(uint32_t s)   { br_sleep_us(BR_SEC(s));   }

br_time_t br_uptime_us(void);
void br_time_alarm_handler(void);

/* Semaphore */

br_err_t br_sem_init(br_sem_t *sem, int32_t initial, int32_t max);
br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout);
br_err_t br_sem_give(br_sem_t *sem);

/* Mutex (with priority inheritance) */

br_err_t br_mutex_init(br_mutex_t *mtx);
br_err_t br_mutex_lock(br_mutex_t *mtx, br_time_t timeout);
br_err_t br_mutex_unlock(br_mutex_t *mtx);

/* Message Queue */

br_err_t br_mqueue_init(br_mqueue_t *mq, void *buffer,
                        size_t msg_size, size_t max_msgs);
br_err_t br_mqueue_send(br_mqueue_t *mq, const void *msg, br_time_t timeout);
br_err_t br_mqueue_recv(br_mqueue_t *mq, void *msg, br_time_t timeout);

#endif /* BEDROCK_H */

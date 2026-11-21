/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BEDROCK_H
#define BEDROCK_H

#include "br_types.h"
#include "br_device.h"
#include "br_hal.h"
#include "br_assert.h"
#include "br_version.h"

#define BR_STABLE

#ifdef BR_WARN_EXPERIMENTAL
#  define BR_EXPERIMENTAL \
    __attribute__((deprecated("experimental API, may change in a future version")))
#else
#  define BR_EXPERIMENTAL
#endif

#ifdef BR_WARN_INTERNAL
#  define BR_INTERNAL \
    __attribute__((deprecated("internal API, not for application use")))
#else
#  define BR_INTERNAL
#endif


BR_STABLE void br_kernel_init(void);
BR_STABLE void br_kernel_start(void) __attribute__((noreturn));

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

BR_EXPERIMENTAL br_err_t br_task_delete(br_tid_t tid);
BR_EXPERIMENTAL br_err_t br_task_get_info(br_tid_t tid, br_task_info_t *info);
BR_EXPERIMENTAL void     br_hook_idle(void);

BR_STABLE void      br_sleep_us(br_time_t us);
BR_STABLE br_time_t br_uptime_us(void);

static inline void br_sleep_ms(uint32_t ms) { br_sleep_us(BR_MSEC(ms)); }
static inline void br_sleep_s(uint32_t s)   { br_sleep_us(BR_SEC(s));   }

BR_INTERNAL void br_time_alarm_handler(void);

BR_STABLE br_err_t br_sem_init(br_sem_t *sem, int32_t initial, int32_t max);
BR_STABLE br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout);
BR_STABLE br_err_t br_sem_give(br_sem_t *sem);

BR_STABLE br_err_t br_mutex_init(br_mutex_t *mtx);
BR_STABLE br_err_t br_mutex_lock(br_mutex_t *mtx, br_time_t timeout);
BR_STABLE br_err_t br_mutex_unlock(br_mutex_t *mtx);

BR_STABLE br_err_t br_mqueue_init(br_mqueue_t *mq, void *buffer,
                                   size_t msg_size, size_t max_msgs);
BR_STABLE br_err_t br_mqueue_send(br_mqueue_t *mq, const void *msg,
                                   br_time_t timeout);
BR_STABLE br_err_t br_mqueue_recv(br_mqueue_t *mq, void *msg,
                                   br_time_t timeout);

BR_STABLE void br_set_panic_handler(br_panic_handler_t handler);

BR_EXPERIMENTAL void     br_uart_putc(char c);
BR_EXPERIMENTAL void     br_uart_puts(const char *s);
BR_EXPERIMENTAL br_err_t br_uart_getc(char *c, br_time_t timeout);

BR_EXPERIMENTAL br_err_t     br_device_register(br_device_t *dev, const char *name,
                                                const br_device_ops_t *ops, void *priv);
BR_EXPERIMENTAL br_device_t *br_device_find(const char *name);
BR_EXPERIMENTAL br_err_t     br_device_open(br_device_t *dev);
BR_EXPERIMENTAL br_err_t     br_device_close(br_device_t *dev);
BR_EXPERIMENTAL int          br_device_read(br_device_t *dev, void *buf, size_t len,
                                            br_time_t timeout);
BR_EXPERIMENTAL int          br_device_write(br_device_t *dev, const void *buf, size_t len,
                                             br_time_t timeout);
BR_EXPERIMENTAL br_err_t     br_device_ioctl(br_device_t *dev, uint32_t cmd, void *arg);

#endif /* BEDROCK_H */

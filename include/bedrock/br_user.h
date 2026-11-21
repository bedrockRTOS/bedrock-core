/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_USER_H
#define BR_USER_H

#include "br_types.h"

#if CONFIG_MPU

#define BR_USER_BSS  __attribute__((section(".user_bss")))

#define BR_USER_STACK(name, size) \
    static uint8_t name[size] __attribute__((aligned(size)))

br_tid_t br_user_self(void);
void     br_user_yield(void);
void     br_user_sleep_us(br_time_t us);
br_time_t br_user_uptime_us(void);
void     br_user_exit(void) __attribute__((noreturn));

int      br_user_sem_create(int32_t initial, int32_t max);
br_err_t br_user_sem_take(int sem, br_time_t timeout);
br_err_t br_user_sem_give(int sem);

int      br_user_mutex_create(void);
br_err_t br_user_mutex_lock(int mutex, br_time_t timeout);
br_err_t br_user_mutex_unlock(int mutex);

int      br_user_mqueue_create(size_t msg_size, size_t max_msgs);
br_err_t br_user_mqueue_send(int mq, const void *msg, br_time_t timeout);
br_err_t br_user_mqueue_recv(int mq, void *msg, br_time_t timeout);

void     br_user_uart_putc(char c);
void     br_user_uart_puts(const char *s);
br_err_t br_user_uart_getc(char *c, br_time_t timeout);

#endif

#endif

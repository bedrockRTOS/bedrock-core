/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "bedrock/br_user.h"
#include "bedrock/br_syscall.h"

#if CONFIG_MPU

#define LO(t)  ((uint32_t)(t))
#define HI(t)  ((uint32_t)((t) >> 32))

br_tid_t br_user_self(void)
{
    return (br_tid_t)br_hal_syscall(BR_SYS_SELF, 0, 0, 0, 0);
}

void br_user_yield(void)
{
    br_hal_syscall(BR_SYS_YIELD, 0, 0, 0, 0);
}

void br_user_sleep_us(br_time_t us)
{
    br_hal_syscall(BR_SYS_SLEEP_US, LO(us), HI(us), 0, 0);
}

br_time_t br_user_uptime_us(void)
{
    uint32_t out[2] = { 0, 0 };

    br_hal_syscall(BR_SYS_UPTIME_US, (uint32_t)(uintptr_t)out, 0, 0, 0);
    return ((br_time_t)out[1] << 32) | out[0];
}

void br_user_exit(void)
{
    br_hal_syscall(BR_SYS_EXIT, 0, 0, 0, 0);
    while (1) { }
}

int br_user_sem_create(int32_t initial, int32_t max)
{
    return (int)br_hal_syscall(BR_SYS_SEM_CREATE, (uint32_t)initial, (uint32_t)max, 0, 0);
}

br_err_t br_user_sem_take(int sem, br_time_t timeout)
{
    return (br_err_t)br_hal_syscall(BR_SYS_SEM_TAKE, (uint32_t)sem, LO(timeout), HI(timeout), 0);
}

br_err_t br_user_sem_give(int sem)
{
    return (br_err_t)br_hal_syscall(BR_SYS_SEM_GIVE, (uint32_t)sem, 0, 0, 0);
}

int br_user_mutex_create(void)
{
    return (int)br_hal_syscall(BR_SYS_MUTEX_CREATE, 0, 0, 0, 0);
}

br_err_t br_user_mutex_lock(int mutex, br_time_t timeout)
{
    return (br_err_t)br_hal_syscall(BR_SYS_MUTEX_LOCK, (uint32_t)mutex, LO(timeout), HI(timeout), 0);
}

br_err_t br_user_mutex_unlock(int mutex)
{
    return (br_err_t)br_hal_syscall(BR_SYS_MUTEX_UNLOCK, (uint32_t)mutex, 0, 0, 0);
}

int br_user_mqueue_create(size_t msg_size, size_t max_msgs)
{
    return (int)br_hal_syscall(BR_SYS_MQ_CREATE, (uint32_t)msg_size, (uint32_t)max_msgs, 0, 0);
}

br_err_t br_user_mqueue_send(int mq, const void *msg, br_time_t timeout)
{
    return (br_err_t)br_hal_syscall(BR_SYS_MQ_SEND, (uint32_t)mq, (uint32_t)(uintptr_t)msg,
                                    LO(timeout), HI(timeout));
}

br_err_t br_user_mqueue_recv(int mq, void *msg, br_time_t timeout)
{
    return (br_err_t)br_hal_syscall(BR_SYS_MQ_RECV, (uint32_t)mq, (uint32_t)(uintptr_t)msg,
                                    LO(timeout), HI(timeout));
}

void br_user_uart_putc(char c)
{
    br_hal_syscall(BR_SYS_UART_PUTC, (uint32_t)(uint8_t)c, 0, 0, 0);
}

void br_user_uart_puts(const char *s)
{
    br_hal_syscall(BR_SYS_UART_PUTS, (uint32_t)(uintptr_t)s, 0, 0, 0);
}

br_err_t br_user_uart_getc(char *c, br_time_t timeout)
{
    return (br_err_t)br_hal_syscall(BR_SYS_UART_GETC, (uint32_t)(uintptr_t)c, LO(timeout),
                                    HI(timeout), 0);
}

#endif

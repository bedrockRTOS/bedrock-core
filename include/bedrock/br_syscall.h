/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_SYSCALL_H
#define BR_SYSCALL_H

enum {
    BR_SYS_SELF         = 0,
    BR_SYS_YIELD        = 1,
    BR_SYS_SLEEP_US     = 2,
    BR_SYS_UPTIME_US    = 3,
    BR_SYS_EXIT         = 4,
    BR_SYS_SEM_CREATE   = 5,
    BR_SYS_SEM_TAKE     = 6,
    BR_SYS_SEM_GIVE     = 7,
    BR_SYS_MUTEX_CREATE = 8,
    BR_SYS_MUTEX_LOCK   = 9,
    BR_SYS_MUTEX_UNLOCK = 10,
    BR_SYS_MQ_CREATE    = 11,
    BR_SYS_MQ_SEND      = 12,
    BR_SYS_MQ_RECV      = 13,
    BR_SYS_UART_PUTC    = 14,
    BR_SYS_UART_PUTS    = 15,
    BR_SYS_UART_GETC    = 16,
    BR_SYS_COUNT
};

#endif

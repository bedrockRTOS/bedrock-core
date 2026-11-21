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
    BR_SYS_COUNT,

    BR_SYS_DEV_OPEN     = 32,
    BR_SYS_DEV_CLOSE    = 33,
    BR_SYS_DEV_READ     = 34,
    BR_SYS_DEV_WRITE    = 35,
    BR_SYS_DEV_IOCTL    = 36,

    BR_SYS_FS_OPEN      = 48,
    BR_SYS_FS_READ      = 49,
    BR_SYS_FS_WRITE     = 50,
    BR_SYS_FS_SEEK      = 51,
    BR_SYS_FS_CLOSE     = 52,
    BR_SYS_FS_REMOVE    = 53,
    BR_SYS_FS_STAT      = 54,
    BR_SYS_FS_MKDIR     = 55,
    BR_SYS_FS_RMDIR     = 56,
    BR_SYS_FS_INFO      = 57,
    BR_SYS_FS_READDIR   = 58,
    BR_SYS_FS_USAGE     = 59,

    BR_SYS_NET_SOCKET   = 64,
    BR_SYS_NET_BIND     = 65,
    BR_SYS_NET_LISTEN   = 66,
    BR_SYS_NET_ACCEPT   = 67,
    BR_SYS_NET_CONNECT  = 68,
    BR_SYS_NET_SEND     = 69,
    BR_SYS_NET_RECV     = 70,
    BR_SYS_NET_SENDTO   = 71,
    BR_SYS_NET_RECVFROM = 72,
    BR_SYS_NET_CLOSE    = 73,
    BR_SYS_NET_PING     = 74,
    BR_SYS_NET_RESOLVE  = 75,
    BR_SYS_NET_IFINFO   = 76
};

#endif

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

int br_user_device_open(const char *name)
{
    return (int)br_hal_syscall(BR_SYS_DEV_OPEN, (uint32_t)(uintptr_t)name, 0, 0, 0);
}

br_err_t br_user_device_close(int dev)
{
    return (br_err_t)br_hal_syscall(BR_SYS_DEV_CLOSE, (uint32_t)dev, 0, 0, 0);
}

int br_user_device_read(int dev, void *buf, size_t len, br_time_t timeout)
{
    uint32_t block[5] = { (uint32_t)dev, (uint32_t)(uintptr_t)buf, (uint32_t)len, LO(timeout), HI(timeout) };
    return (int)br_hal_syscall(BR_SYS_DEV_READ, (uint32_t)(uintptr_t)block, 0, 0, 0);
}

int br_user_device_write(int dev, const void *buf, size_t len, br_time_t timeout)
{
    uint32_t block[5] = { (uint32_t)dev, (uint32_t)(uintptr_t)buf, (uint32_t)len, LO(timeout), HI(timeout) };
    return (int)br_hal_syscall(BR_SYS_DEV_WRITE, (uint32_t)(uintptr_t)block, 0, 0, 0);
}

br_err_t br_user_device_ioctl(int dev, uint32_t cmd, void *arg, size_t size)
{
    return (br_err_t)br_hal_syscall(BR_SYS_DEV_IOCTL, (uint32_t)dev, cmd, (uint32_t)(uintptr_t)arg,
                                    (uint32_t)size);
}

#define P(x) ((uint32_t)(uintptr_t)(x))
#define SYS4(n, a, b, c, d) br_hal_syscall((n), (a), (b), (c), (d))

int br_user_fs_open(const char *name, uint8_t flags) { return (int)SYS4(BR_SYS_FS_OPEN, P(name), flags, 0, 0); }
int br_user_fs_read(int fd, void *buf, size_t len) { return (int)SYS4(BR_SYS_FS_READ, (uint32_t)fd, P(buf), (uint32_t)len, 0); }
int br_user_fs_write(int fd, const void *buf, size_t len) { return (int)SYS4(BR_SYS_FS_WRITE, (uint32_t)fd, P(buf), (uint32_t)len, 0); }
br_err_t br_user_fs_seek(int fd, size_t offset) { return (br_err_t)SYS4(BR_SYS_FS_SEEK, (uint32_t)fd, (uint32_t)offset, 0, 0); }
br_err_t br_user_fs_close(int fd) { return (br_err_t)SYS4(BR_SYS_FS_CLOSE, (uint32_t)fd, 0, 0, 0); }
br_err_t br_user_fs_remove(const char *name) { return (br_err_t)SYS4(BR_SYS_FS_REMOVE, P(name), 0, 0, 0); }
br_err_t br_user_fs_mkdir(const char *path) { return (br_err_t)SYS4(BR_SYS_FS_MKDIR, P(path), 0, 0, 0); }
br_err_t br_user_fs_rmdir(const char *path) { return (br_err_t)SYS4(BR_SYS_FS_RMDIR, P(path), 0, 0, 0); }
br_err_t br_user_fs_stat(const char *name, uint32_t *size) { return (br_err_t)SYS4(BR_SYS_FS_STAT, P(name), P(size), 0, 0); }
br_err_t br_user_fs_info(const char *path, void *info) { return (br_err_t)SYS4(BR_SYS_FS_INFO, P(path), P(info), 0, 0); }
br_err_t br_user_fs_readdir(const char *dir, uint32_t index, void *info) { return (br_err_t)SYS4(BR_SYS_FS_READDIR, P(dir), index, P(info), 0); }
br_err_t br_user_fs_usage(void *usage) { return (br_err_t)SYS4(BR_SYS_FS_USAGE, P(usage), 0, 0, 0); }

int br_user_net_socket(int family, int type) { return (int)SYS4(BR_SYS_NET_SOCKET, (uint32_t)family, (uint32_t)type, 0, 0); }
br_err_t br_user_net_bind(int fd, const br_ip_t *ip, uint16_t port) { return (br_err_t)SYS4(BR_SYS_NET_BIND, (uint32_t)fd, P(ip), port, 0); }
br_err_t br_user_net_listen(int fd, int backlog) { return (br_err_t)SYS4(BR_SYS_NET_LISTEN, (uint32_t)fd, (uint32_t)backlog, 0, 0); }
br_err_t br_user_net_connect(int fd, const br_ip_t *ip, uint16_t port) { return (br_err_t)SYS4(BR_SYS_NET_CONNECT, (uint32_t)fd, P(ip), port, 0); }
int br_user_net_send(int fd, const void *buf, size_t len) { return (int)SYS4(BR_SYS_NET_SEND, (uint32_t)fd, P(buf), (uint32_t)len, 0); }
br_err_t br_user_net_close(int fd) { return (br_err_t)SYS4(BR_SYS_NET_CLOSE, (uint32_t)fd, 0, 0, 0); }
br_err_t br_user_net_resolve(const char *name, int family, br_ip_t *ip) { return (br_err_t)SYS4(BR_SYS_NET_RESOLVE, P(name), (uint32_t)family, P(ip), 0); }
br_err_t br_user_net_ifinfo(uint32_t index, void *info) { return (br_err_t)SYS4(BR_SYS_NET_IFINFO, index, P(info), 0, 0); }

int br_user_net_accept(int fd, br_time_t timeout, br_ip_t *ip, uint32_t *port)
{
    uint32_t b[5] = { (uint32_t)fd, LO(timeout), HI(timeout), P(ip), P(port) };
    return (int)SYS4(BR_SYS_NET_ACCEPT, P(b), 0, 0, 0);
}

int br_user_net_recv(int fd, void *buf, size_t len, br_time_t timeout)
{
    uint32_t b[5] = { (uint32_t)fd, P(buf), (uint32_t)len, LO(timeout), HI(timeout) };
    return (int)SYS4(BR_SYS_NET_RECV, P(b), 0, 0, 0);
}

int br_user_net_sendto(int fd, const void *buf, size_t len, const br_ip_t *ip, uint16_t port)
{
    uint32_t b[5] = { (uint32_t)fd, P(buf), (uint32_t)len, P(ip), port };
    return (int)SYS4(BR_SYS_NET_SENDTO, P(b), 0, 0, 0);
}

int br_user_net_recvfrom(int fd, void *buf, size_t len, br_time_t timeout, br_ip_t *ip, uint32_t *port)
{
    uint32_t b[7] = { (uint32_t)fd, P(buf), (uint32_t)len, LO(timeout), HI(timeout), P(ip), P(port) };
    return (int)SYS4(BR_SYS_NET_RECVFROM, P(b), 0, 0, 0);
}

br_err_t br_user_net_ping(const br_ip_t *ip, br_time_t timeout, uint32_t *rtt_us)
{
    uint32_t b[4] = { P(ip), LO(timeout), HI(timeout), P(rtt_us) };
    return (br_err_t)SYS4(BR_SYS_NET_PING, P(b), 0, 0, 0);
}

#endif

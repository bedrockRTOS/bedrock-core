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

int      br_user_device_open(const char *name);
br_err_t br_user_device_close(int dev);
int      br_user_device_read(int dev, void *buf, size_t len, br_time_t timeout);
int      br_user_device_write(int dev, const void *buf, size_t len, br_time_t timeout);
br_err_t br_user_device_ioctl(int dev, uint32_t cmd, void *arg, size_t size);

int      br_user_fs_open(const char *name, uint8_t flags);
int      br_user_fs_read(int fd, void *buf, size_t len);
int      br_user_fs_write(int fd, const void *buf, size_t len);
br_err_t br_user_fs_seek(int fd, size_t offset);
br_err_t br_user_fs_close(int fd);
br_err_t br_user_fs_remove(const char *name);
br_err_t br_user_fs_mkdir(const char *path);
br_err_t br_user_fs_rmdir(const char *path);
br_err_t br_user_fs_stat(const char *name, uint32_t *size);
br_err_t br_user_fs_info(const char *path, void *info);
br_err_t br_user_fs_readdir(const char *dir, uint32_t index, void *info);
br_err_t br_user_fs_usage(void *usage);

int      br_user_net_socket(int type);
br_err_t br_user_net_bind(int fd, uint32_t ip, uint16_t port);
br_err_t br_user_net_listen(int fd, int backlog);
int      br_user_net_accept(int fd, br_time_t timeout, uint32_t *ip, uint32_t *port);
br_err_t br_user_net_connect(int fd, uint32_t ip, uint16_t port);
int      br_user_net_send(int fd, const void *buf, size_t len);
int      br_user_net_recv(int fd, void *buf, size_t len, br_time_t timeout);
int      br_user_net_sendto(int fd, const void *buf, size_t len, uint32_t ip, uint16_t port);
int      br_user_net_recvfrom(int fd, void *buf, size_t len, br_time_t timeout,
                              uint32_t *ip, uint32_t *port);
br_err_t br_user_net_close(int fd);
br_err_t br_user_net_ping(uint32_t ip, br_time_t timeout, uint32_t *rtt_us);
br_err_t br_user_net_resolve(const char *name, uint32_t *ip);
br_err_t br_user_net_ifinfo(uint32_t index, void *info);

#endif

#endif

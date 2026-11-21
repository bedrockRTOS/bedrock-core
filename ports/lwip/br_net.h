/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_NET_H
#define BR_NET_H

#include "bedrock/bedrock.h"
#include "bedrock/br_netif.h"

#define BR_NET_MAX_SOCKETS   6
#define BR_NET_MAX_NETIFS    2
#define BR_NET_POLL_MS       5

#define BR_NET_DGRAM         1
#define BR_NET_STREAM        2

#define BR_IP4(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

typedef struct {
    char     name[8];
    uint8_t  mac[BR_NETIF_MAC_LEN];
    uint32_t ip;
    uint32_t mask;
    uint32_t gw;
    bool     up;
    bool     has_mac;
} br_net_ifinfo_t;

br_err_t br_net_init(void);
br_err_t br_net_netif_add(br_netif_t *netif, uint32_t ip, uint32_t mask, uint32_t gw);
br_err_t br_net_wait_ip(br_netif_t *netif, br_time_t timeout);
br_err_t br_net_ifinfo(uint32_t index, br_net_ifinfo_t *info);

int      br_net_socket(int type);
br_err_t br_net_bind(int fd, uint32_t ip, uint16_t port);
br_err_t br_net_listen(int fd, int backlog);
int      br_net_accept(int fd, br_time_t timeout, uint32_t *ip, uint16_t *port);
br_err_t br_net_connect(int fd, uint32_t ip, uint16_t port);
int      br_net_send(int fd, const void *buf, size_t len);
int      br_net_recv(int fd, void *buf, size_t len, br_time_t timeout);
int      br_net_sendto(int fd, const void *buf, size_t len, uint32_t ip, uint16_t port);
int      br_net_recvfrom(int fd, void *buf, size_t len, br_time_t timeout,
                         uint32_t *ip, uint16_t *port);
br_err_t br_net_close(int fd);

br_err_t br_net_ping(uint32_t ip, br_time_t timeout, uint32_t *rtt_us);

void     br_shell_register_net(void);

#endif

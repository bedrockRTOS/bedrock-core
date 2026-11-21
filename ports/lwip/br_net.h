/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_NET_H
#define BR_NET_H

#include "bedrock/bedrock.h"
#include "bedrock/br_ip.h"
#include "bedrock/br_netif.h"

#define BR_NET_MAX_SOCKETS   6
#define BR_NET_MAX_NETIFS    2
#define BR_NET_POLL_MS       5
#define BR_NET_IRQ_FALLBACK_MS  200

#define BR_NET_DGRAM         1
#define BR_NET_STREAM        2

#define BR_NET_IP6_MAX       3

typedef struct {
    char     name[8];
    uint8_t  mac[BR_NETIF_MAC_LEN];
    br_ip_t  ip;
    br_ip_t  mask;
    br_ip_t  gw;
    br_ip_t  ip6[BR_NET_IP6_MAX];
    uint8_t  ip6_count;
    uint16_t scope;
    bool     up;
    bool     has_mac;
} br_net_ifinfo_t;

bool     br_ip_equal(const br_ip_t *a, const br_ip_t *b);
bool     br_ip_is_any(const br_ip_t *ip);
br_ip_t  br_ip_any(int family);
bool     br_ip_parse(const char *text, br_ip_t *ip);
int      br_ip_format(const br_ip_t *ip, char *buf, size_t size);

br_err_t br_net_init(void);
br_err_t br_net_netif_add(br_netif_t *netif, const br_ip_t *ip, const br_ip_t *mask,
                          const br_ip_t *gw);
br_err_t br_net_ip6_add(br_netif_t *netif, const br_ip_t *addr);
br_err_t br_net_wait_ip(br_netif_t *netif, br_time_t timeout);
br_err_t br_net_wait_ip6(br_netif_t *netif, br_time_t timeout);
br_err_t br_net_ifinfo(uint32_t index, br_net_ifinfo_t *info);

int      br_net_socket(int family, int type);
br_err_t br_net_bind(int fd, const br_ip_t *ip, uint16_t port);
br_err_t br_net_listen(int fd, int backlog);
int      br_net_accept(int fd, br_time_t timeout, br_ip_t *ip, uint16_t *port);
br_err_t br_net_connect(int fd, const br_ip_t *ip, uint16_t port);
int      br_net_send(int fd, const void *buf, size_t len);
int      br_net_recv(int fd, void *buf, size_t len, br_time_t timeout);
int      br_net_sendto(int fd, const void *buf, size_t len, const br_ip_t *ip, uint16_t port);
int      br_net_recvfrom(int fd, void *buf, size_t len, br_time_t timeout,
                         br_ip_t *ip, uint16_t *port);
br_err_t br_net_close(int fd);

br_err_t br_net_ping(const br_ip_t *ip, br_time_t timeout, uint32_t *rtt_us);

br_err_t br_net_resolve(const char *name, int family, br_ip_t *ip);
br_err_t br_net_set_dns(const br_ip_t *ip);
br_err_t br_net_dns(br_ip_t *ip);

uint32_t br_net_rx_wakeups(void);

#if CONFIG_MPU
br_err_t br_net_enable_user_calls(void);
#endif

void     br_shell_register_net(void);

#endif

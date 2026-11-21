/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_NETIF_H
#define BR_NETIF_H

#include "br_types.h"

#define BR_NETIF_MAC_LEN    6
#define BR_NETIF_FRAME_MAX  1536

struct br_netif;

typedef struct {
    br_err_t (*send)(struct br_netif *netif, const void *frame, size_t len);
    int      (*recv)(struct br_netif *netif, void *buf, size_t size);
    bool     (*link_up)(struct br_netif *netif);
} br_netif_ops_t;

typedef struct br_netif {
    const char            *name;
    uint8_t                mac[BR_NETIF_MAC_LEN];
    uint16_t               mtu;
    const br_netif_ops_t  *ops;
    void                  *priv;
} br_netif_t;

#endif

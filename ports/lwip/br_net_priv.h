/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_NET_PRIV_H
#define BR_NET_PRIV_H

#include "br_net.h"
#include "lwip/ip_addr.h"

bool br_net_to_lwip(ip_addr_t *dst, const br_ip_t *src);
void br_net_from_lwip(br_ip_t *dst, const ip_addr_t *src);

#endif

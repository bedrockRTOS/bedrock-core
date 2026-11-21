/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_IP_H
#define BR_IP_H

#include "br_types.h"

#define BR_AF_ANY            0
#define BR_AF_INET           4
#define BR_AF_INET6          6

/* An IPv4 or IPv6 address. The bytes are in network order, an IPv4 address
 * uses addr[0..3]. A family of BR_AF_ANY means "no address".
 *
 * scope is the interface a link-local IPv6 address belongs to (the "scope"
 * field of br_net_ifinfo_t). 0 means the default interface. */
typedef struct {
    _Alignas(4) uint16_t family;
    uint16_t scope;
    uint8_t  addr[16];
} br_ip_t;

#define BR_IP4(a, b, c, d) \
    ((br_ip_t){ .family = BR_AF_INET, .addr = { (a), (b), (c), (d) } })

#define BR_IP6_LOOPBACK \
    ((br_ip_t){ .family = BR_AF_INET6, .addr = { [15] = 1 } })

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_NETIF_MOCK_H
#define BR_NETIF_MOCK_H

#include "bedrock/bedrock.h"
#include "bedrock/br_netif.h"

#define BR_NETIF_MOCK_DEPTH  2

typedef struct {
    uint8_t  data[BR_NETIF_FRAME_MAX];
    uint16_t len;
} br_netif_mock_frame_t;

typedef struct {
    br_netif_t            netif;
    br_netif_mock_frame_t tx[BR_NETIF_MOCK_DEPTH];
    br_netif_mock_frame_t rx[BR_NETIF_MOCK_DEPTH];
    uint8_t               tx_head;
    uint8_t               tx_count;
    uint8_t               rx_head;
    uint8_t               rx_count;
    bool                  link;
} br_netif_mock_t;

br_err_t br_netif_mock_init(br_netif_mock_t *mock, const char *name,
                            const uint8_t mac[BR_NETIF_MAC_LEN]);
br_err_t br_netif_mock_inject(br_netif_mock_t *mock, const void *frame, size_t len);
int      br_netif_mock_take(br_netif_mock_t *mock, void *buf, size_t size);

#endif

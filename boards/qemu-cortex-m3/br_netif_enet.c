/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "bedrock/br_netif.h"
#include "lm3s_regs.h"

#define MAC_BASE        0x40048000UL
#define MAC_RCTL        LM3S_REG(MAC_BASE + 0x008UL)
#define MAC_TCTL        LM3S_REG(MAC_BASE + 0x00CUL)
#define MAC_DATA        LM3S_REG(MAC_BASE + 0x010UL)
#define MAC_IA0         LM3S_REG(MAC_BASE + 0x014UL)
#define MAC_IA1         LM3S_REG(MAC_BASE + 0x018UL)
#define MAC_NP          LM3S_REG(MAC_BASE + 0x034UL)
#define MAC_TR          LM3S_REG(MAC_BASE + 0x038UL)

#define SYSCTL_RCGC2_EMAC0   (1UL << 28)
#define SYSCTL_RCGC2_EPHY0   (1UL << 30)

#define MAC_RCTL_RXEN        (1UL << 0)
#define MAC_RCTL_AMUL        (1UL << 1)
#define MAC_RCTL_RSTFIFO     (1UL << 4)
#define MAC_TCTL_TXEN        (1UL << 0)
#define MAC_TCTL_PADEN       (1UL << 1)
#define MAC_TCTL_CRC         (1UL << 2)
#define MAC_TCTL_DUPLEX      (1UL << 4)
#define MAC_TR_NEWTX         (1UL << 0)

#define ETH_HEADER_LEN       14U
#define RX_OVERHEAD          6U

static br_netif_t netif;
static bool       ready;

static br_err_t enet_send(br_netif_t *dev, const void *frame, size_t len)
{
    (void)dev;
    const uint8_t *f = frame;

    if (len < ETH_HEADER_LEN || len > BR_NETIF_FRAME_MAX) {
        return BR_ERR_INVALID;
    }

    uint32_t spins = LM3S_WAIT_LIMIT;
    while (MAC_TR & MAC_TR_NEWTX) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }

    MAC_DATA = (uint32_t)(len - ETH_HEADER_LEN) | ((uint32_t)f[0] << 16) | ((uint32_t)f[1] << 24);

    for (size_t i = 2; i < len; i += 4) {
        uint32_t w = 0;
        for (size_t b = 0; b < 4 && i + b < len; b++) {
            w |= (uint32_t)f[i + b] << (8 * b);
        }
        MAC_DATA = w;
    }

    MAC_TR = MAC_TR_NEWTX;
    return BR_OK;
}

static int enet_recv(br_netif_t *dev, void *buf, size_t size)
{
    (void)dev;
    uint8_t *out = buf;

    if (MAC_NP == 0) {
        return 0;
    }

    uint32_t w = MAC_DATA;
    uint32_t total = w & 0xFFFFU;
    if (total < RX_OVERHEAD + ETH_HEADER_LEN) {
        MAC_RCTL |= MAC_RCTL_RSTFIFO;
        return BR_ERR_IO;
    }

    uint32_t len = total - RX_OVERHEAD;
    uint32_t keep = len <= size ? len : 0;

    if (keep != 0) {
        out[0] = (uint8_t)(w >> 16);
        out[1] = (uint8_t)(w >> 24);
    }

    uint32_t pos = 2;
    while (pos < len + 4U) {
        w = MAC_DATA;
        for (uint32_t b = 0; b < 4; b++, pos++) {
            if (keep != 0 && pos < len) {
                out[pos] = (uint8_t)(w >> (8 * b));
            }
        }
    }
    return keep != 0 ? (int)len : BR_ERR_OVERFLOW;
}

static bool enet_link_up(br_netif_t *dev)
{
    (void)dev;
    return true;
}

static const br_netif_ops_t enet_ops = {
    .send    = enet_send,
    .recv    = enet_recv,
    .link_up = enet_link_up,
};

br_netif_t *br_hal_netif_default(void)
{
    if (!ready) {
        SYSCTL_RCGC2 |= SYSCTL_RCGC2_EMAC0 | SYSCTL_RCGC2_EPHY0;
        (void)SYSCTL_RCGC2;

        MAC_RCTL = MAC_RCTL_RSTFIFO;
        MAC_TCTL = MAC_TCTL_TXEN | MAC_TCTL_PADEN | MAC_TCTL_CRC | MAC_TCTL_DUPLEX;
        MAC_RCTL = MAC_RCTL_RXEN | MAC_RCTL_AMUL;

        uint32_t ia0 = MAC_IA0;
        uint32_t ia1 = MAC_IA1;
        for (int i = 0; i < 4; i++) {
            netif.mac[i] = (uint8_t)(ia0 >> (8 * i));
        }
        netif.mac[4] = (uint8_t)ia1;
        netif.mac[5] = (uint8_t)(ia1 >> 8);

        netif.name = "eth0";
        netif.mtu  = 1500;
        netif.ops  = &enet_ops;
        netif.priv = NULL;
        ready      = true;
    }
    return &netif;
}

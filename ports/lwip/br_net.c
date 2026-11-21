/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_net.h"

#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"

#define BR_NET_RX_STACK  3072

typedef struct {
    struct netif  lwip;
    br_netif_t   *dev;
    bool          used;
} net_entry_t;

static net_entry_t entries[BR_NET_MAX_NETIFS];
static bool        initialized;
static uint8_t     rx_stack[BR_NET_RX_STACK] __attribute__((aligned(8)));
static uint8_t     rx_frame[BR_NETIF_FRAME_MAX];
static uint8_t     tx_frame[BR_NETIF_FRAME_MAX];

extern br_mutex_t br_net_slot_lock;
extern void       br_net_slots_init(void);

static err_t link_output(struct netif *netif, struct pbuf *p)
{
    net_entry_t *e = netif->state;

    if (p->tot_len > sizeof(tx_frame)) {
        return ERR_BUF;
    }
    pbuf_copy_partial(p, tx_frame, p->tot_len, 0);
    return e->dev->ops->send(e->dev, tx_frame, p->tot_len) == BR_OK ? ERR_OK : ERR_IF;
}

static err_t netif_setup(struct netif *netif)
{
    net_entry_t *e = netif->state;

    netif->name[0]    = e->dev->name[0];
    netif->name[1]    = e->dev->name[1] != '\0' ? e->dev->name[1] : 'x';
    netif->output     = etharp_output;
    netif->linkoutput = link_output;
    netif->mtu        = e->dev->mtu;
    netif->hwaddr_len = BR_NETIF_MAC_LEN;
    for (int i = 0; i < BR_NETIF_MAC_LEN; i++) {
        netif->hwaddr[i] = e->dev->mac[i];
    }
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    return ERR_OK;
}

static void rx_task(void *arg)
{
    (void)arg;

    while (1) {
        bool any = false;

        for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
            net_entry_t *e = &entries[i];
            if (!e->used) {
                continue;
            }

            int len = e->dev->ops->recv(e->dev, rx_frame, sizeof(rx_frame));
            if (len <= 0) {
                continue;
            }
            any = true;

            struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
            if (p == NULL) {
                continue;
            }
            pbuf_take(p, rx_frame, (u16_t)len);
            if (e->lwip.input(p, &e->lwip) != ERR_OK) {
                pbuf_free(p);
            }
        }

        if (!any) {
            br_sleep_ms(BR_NET_POLL_MS);
        }
    }
}

br_err_t br_net_init(void)
{
    if (initialized) {
        return BR_ERR_BUSY;
    }

    br_net_slots_init();
    tcpip_init(NULL, NULL);

    br_tid_t tid;
    br_err_t err = br_task_create(&tid, "netrx", rx_task, NULL, 3, rx_stack, sizeof(rx_stack));
    if (err != BR_OK) {
        return err;
    }
    initialized = true;
    return BR_OK;
}

br_err_t br_net_netif_add(br_netif_t *dev, uint32_t ip, uint32_t mask, uint32_t gw)
{
    if (!initialized || dev == NULL || dev->ops == NULL || dev->name == NULL) {
        return BR_ERR_INVALID;
    }

    net_entry_t *e = NULL;
    for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
        if (!entries[i].used) {
            e = &entries[i];
            break;
        }
    }
    if (e == NULL) {
        return BR_ERR_NOMEM;
    }

    ip4_addr_t a, m, g;
    ip4_addr_set_u32(&a, lwip_htonl(ip));
    ip4_addr_set_u32(&m, lwip_htonl(mask));
    ip4_addr_set_u32(&g, lwip_htonl(gw));

    e->dev = dev;

    LOCK_TCPIP_CORE();
    struct netif *added = netif_add(&e->lwip, &a, &m, &g, e, netif_setup, tcpip_input);
    if (added != NULL) {
        if (netif_default == NULL || netif_default == netif_list) {
            netif_set_default(&e->lwip);
        }
        netif_set_up(&e->lwip);
        if (ip == 0) {
            dhcp_start(&e->lwip);
        }
    }
    UNLOCK_TCPIP_CORE();

    if (added == NULL) {
        return BR_ERR_INVALID;
    }
    e->used = true;
    return BR_OK;
}

br_err_t br_net_wait_ip(br_netif_t *dev, br_time_t timeout)
{
    br_time_t start = br_uptime_us();

    while (1) {
        for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
            if (entries[i].used && entries[i].dev == dev &&
                !ip4_addr_isany_val(*netif_ip4_addr(&entries[i].lwip))) {
                return BR_OK;
            }
        }
        if (timeout != BR_TIME_INFINITE && br_uptime_us() - start >= timeout) {
            return BR_ERR_TIMEOUT;
        }
        br_sleep_ms(20);
    }
}

br_err_t br_net_ifinfo(uint32_t index, br_net_ifinfo_t *info)
{
    if (!initialized || info == NULL) {
        return BR_ERR_INVALID;
    }

    br_err_t err = BR_ERR_NOENT;
    uint32_t n = 0;

    LOCK_TCPIP_CORE();
    struct netif *netif;
    NETIF_FOREACH(netif) {
        if (n++ != index) {
            continue;
        }

        net_entry_t *e = netif->state;
        bool ours = false;
        for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
            ours = ours || (e == &entries[i] && entries[i].used);
        }

        if (ours) {
            size_t k = 0;
            for (; k + 1 < sizeof(info->name) && e->dev->name[k] != '\0'; k++) {
                info->name[k] = e->dev->name[k];
            }
            info->name[k] = '\0';
        } else {
            info->name[0] = netif->name[0];
            info->name[1] = netif->name[1];
            info->name[2] = (char)('0' + netif->num % 10);
            info->name[3] = '\0';
        }
        info->has_mac = netif->hwaddr_len == BR_NETIF_MAC_LEN;
        for (int i = 0; i < BR_NETIF_MAC_LEN; i++) {
            info->mac[i] = info->has_mac ? netif->hwaddr[i] : 0;
        }
        info->ip   = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(netif)));
        info->mask = lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(netif)));
        info->gw   = lwip_ntohl(ip4_addr_get_u32(netif_ip4_gw(netif)));
        info->up   = netif_is_up(netif) && netif_is_link_up(netif);
        err = BR_OK;
        break;
    }
    UNLOCK_TCPIP_CORE();
    return err;
}

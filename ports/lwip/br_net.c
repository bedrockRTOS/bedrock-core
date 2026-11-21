/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_net_priv.h"

#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/pbuf.h"
#include "lwip/ethip6.h"
#include "lwip/ip6_addr.h"
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
static br_sem_t    rx_sem;
static volatile uint32_t rx_wakeups;

static void rx_notify(br_netif_t *netif)
{
    (void)netif;
    br_sem_give(&rx_sem);
}

uint32_t br_net_rx_wakeups(void)
{
    return rx_wakeups;
}

/* ---- addresses ---- */

bool br_ip_equal(const br_ip_t *a, const br_ip_t *b)
{
    if (a->family != b->family) {
        return false;
    }
    if (a->family == BR_AF_INET6 && a->scope != 0 && b->scope != 0 && a->scope != b->scope) {
        return false;
    }
    size_t n = a->family == BR_AF_INET ? 4 : a->family == BR_AF_INET6 ? 16 : 0;
    for (size_t i = 0; i < n; i++) {
        if (a->addr[i] != b->addr[i]) {
            return false;
        }
    }
    return true;
}

br_ip_t br_ip_any(int family)
{
    br_ip_t ip;
    ip.family = (uint16_t)family;
    ip.scope  = 0;
    for (int i = 0; i < 16; i++) {
        ip.addr[i] = 0;
    }
    return ip;
}

bool br_ip_is_any(const br_ip_t *ip)
{
    size_t n = ip->family == BR_AF_INET ? 4 : ip->family == BR_AF_INET6 ? 16 : 0;
    for (size_t i = 0; i < n; i++) {
        if (ip->addr[i] != 0) {
            return false;
        }
    }
    return true;
}

static bool parse_dec(const char *s, size_t n, uint32_t max, uint32_t *out)
{
    if (n == 0 || n > 3) {
        return false;
    }
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        v = v * 10 + (uint32_t)(s[i] - '0');
    }
    if (v > max) {
        return false;
    }
    *out = v;
    return true;
}

static bool parse4(const char *s, uint8_t out[4])
{
    for (int part = 0; part < 4; part++) {
        size_t n = 0;
        while (s[n] != '\0' && s[n] != '.') {
            n++;
        }
        uint32_t v;
        if (!parse_dec(s, n, 255, &v)) {
            return false;
        }
        out[part] = (uint8_t)v;
        s += n;
        if (part < 3) {
            if (*s != '.') {
                return false;
            }
            s++;
        }
    }
    return *s == '\0';
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse6(const char *s, uint8_t out[16])
{
    uint16_t w[8];
    int n = 0;
    int gap = -1;

    if (s[0] == ':') {
        if (s[1] != ':') {
            return false;
        }
        gap = 0;
        s += 2;
    }

    while (*s != '\0') {
        const char *start = s;
        uint32_t v = 0;
        int digits = 0;
        while (hex_value(*s) >= 0) {
            if (++digits > 4) {
                return false;
            }
            v = (v << 4) | (uint32_t)hex_value(*s);
            s++;
        }
        if (*s == '.') {
            uint8_t b[4];
            if (n > 6 || !parse4(start, b)) {
                return false;
            }
            w[n++] = (uint16_t)((b[0] << 8) | b[1]);
            w[n++] = (uint16_t)((b[2] << 8) | b[3]);
            s = "";
            break;
        }
        if (digits == 0 || n >= 8) {
            return false;
        }
        w[n++] = (uint16_t)v;
        if (*s == '\0') {
            break;
        }
        if (*s != ':') {
            return false;
        }
        s++;
        if (*s == ':') {
            if (gap >= 0) {
                return false;
            }
            gap = n;
            s++;
        } else if (*s == '\0') {
            return false;
        }
    }

    if (gap < 0 ? n != 8 : n >= 8) {
        return false;
    }

    uint16_t full[8];
    int missing = 8 - n;
    for (int i = 0, k = 0; i < 8; i++) {
        if (gap >= 0 && i >= gap && i < gap + missing) {
            full[i] = 0;
        } else {
            full[i] = w[k++];
        }
    }
    for (int i = 0; i < 8; i++) {
        out[2 * i]     = (uint8_t)(full[i] >> 8);
        out[2 * i + 1] = (uint8_t)full[i];
    }
    return true;
}

bool br_ip_parse(const char *text, br_ip_t *ip)
{
    if (text == NULL || ip == NULL) {
        return false;
    }

    bool        v6 = false;
    const char *pct = NULL;
    for (const char *c = text; *c != '\0'; c++) {
        v6 = v6 || *c == ':';
        if (*c == '%' && pct == NULL) {
            pct = c;
        }
    }

    br_ip_t out = br_ip_any(v6 ? BR_AF_INET6 : BR_AF_INET);
    char    addr[40];

    if (pct != NULL) {
        size_t n = (size_t)(pct - text);
        if (!v6 || n == 0 || n >= sizeof(addr) || pct[1] == '\0') {
            return false;
        }
        for (size_t i = 0; i < n; i++) {
            addr[i] = text[i];
        }
        addr[n] = '\0';

        uint32_t scope = 0;
        bool numeric = true;
        for (const char *c = pct + 1; *c != '\0'; c++) {
            numeric = numeric && *c >= '0' && *c <= '9';
            if (numeric) {
                scope = scope * 10 + (uint32_t)(*c - '0');
                if (scope > 0xFFFF) {
                    return false;
                }
            }
        }
        if (!numeric) {
            scope = 0;
            br_net_ifinfo_t info;
            for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
                const char *a = info.name;
                const char *b = pct + 1;
                while (*a != '\0' && *a == *b) {
                    a++;
                    b++;
                }
                if (*a == '\0' && *b == '\0') {
                    scope = info.scope;
                    break;
                }
            }
        }
        if (scope == 0 || !parse6(addr, out.addr)) {
            return false;
        }
        out.scope = (uint16_t)scope;
        *ip = out;
        return true;
    }

    if (!(v6 ? parse6(text, out.addr) : parse4(text, out.addr))) {
        return false;
    }
    *ip = out;
    return true;
}

typedef struct {
    char  *buf;
    size_t size;
    size_t len;
} out_t;

static void out_char(out_t *o, char c)
{
    if (o->len + 1 < o->size) {
        o->buf[o->len] = c;
    }
    o->len++;
}

static void out_number(out_t *o, uint32_t v, uint32_t base)
{
    char tmp[8];
    int n = 0;
    do {
        uint32_t d = v % base;
        tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= base;
    } while (v != 0);
    while (n > 0) {
        out_char(o, tmp[--n]);
    }
}

int br_ip_format(const br_ip_t *ip, char *buf, size_t size)
{
    if (ip == NULL || buf == NULL || size == 0 ||
        (ip->family != BR_AF_INET && ip->family != BR_AF_INET6)) {
        return BR_ERR_INVALID;
    }

    out_t o = { buf, size, 0 };

    if (ip->family == BR_AF_INET) {
        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                out_char(&o, '.');
            }
            out_number(&o, ip->addr[i], 10);
        }
    } else {
        uint16_t w[8];
        for (int i = 0; i < 8; i++) {
            w[i] = (uint16_t)((ip->addr[2 * i] << 8) | ip->addr[2 * i + 1]);
        }

        int best = -1;
        int best_len = 1;
        for (int i = 0; i < 8;) {
            if (w[i] != 0) {
                i++;
                continue;
            }
            int j = i;
            while (j < 8 && w[j] == 0) {
                j++;
            }
            if (j - i > best_len) {
                best = i;
                best_len = j - i;
            }
            i = j;
        }

        for (int i = 0; i < 8; i++) {
            if (i == best) {
                out_char(&o, ':');
                out_char(&o, ':');
                i += best_len - 1;
                continue;
            }
            if (i > 0 && i != best + best_len) {
                out_char(&o, ':');
            }
            out_number(&o, w[i], 16);
        }
        if (ip->scope != 0) {
            out_char(&o, '%');
            out_number(&o, ip->scope, 10);
        }
    }

    buf[o.len < size ? o.len : size - 1] = '\0';
    return o.len < size ? (int)o.len : BR_ERR_NOMEM;
}

bool br_net_to_lwip(ip_addr_t *dst, const br_ip_t *src)
{
    if (src->family == BR_AF_INET) {
        IP_ADDR4(dst, src->addr[0], src->addr[1], src->addr[2], src->addr[3]);
        return true;
    }
    if (src->family == BR_AF_INET6) {
        ip6_addr_t *a6 = ip_2_ip6(dst);
        for (int i = 0; i < 4; i++) {
            uint32_t w = (uint32_t)src->addr[4 * i] | ((uint32_t)src->addr[4 * i + 1] << 8) |
                         ((uint32_t)src->addr[4 * i + 2] << 16) | ((uint32_t)src->addr[4 * i + 3] << 24);
            a6->addr[i] = w;
        }
        IP_SET_TYPE(dst, IPADDR_TYPE_V6);
        ip6_addr_clear_zone(a6);
        if (ip6_addr_has_scope(a6, IP6_UNKNOWN)) {
            if (src->scope != 0) {
                ip6_addr_set_zone(a6, src->scope);
            } else if (netif_default != NULL) {
                ip6_addr_assign_zone(a6, IP6_UNKNOWN, netif_default);
            }
        }
        return true;
    }
    return false;
}

static void from_lwip6(br_ip_t *dst, const ip6_addr_t *a6)
{
    dst->family = BR_AF_INET6;
    dst->scope  = (uint16_t)ip6_addr_zone(a6);
    for (int i = 0; i < 4; i++) {
        uint32_t w = a6->addr[i];
        dst->addr[4 * i]     = (uint8_t)w;
        dst->addr[4 * i + 1] = (uint8_t)(w >> 8);
        dst->addr[4 * i + 2] = (uint8_t)(w >> 16);
        dst->addr[4 * i + 3] = (uint8_t)(w >> 24);
    }
}

static void from_lwip4(br_ip_t *dst, const ip4_addr_t *a4)
{
    uint32_t w = ip4_addr_get_u32(a4);
    *dst = br_ip_any(BR_AF_INET);
    dst->addr[0] = (uint8_t)w;
    dst->addr[1] = (uint8_t)(w >> 8);
    dst->addr[2] = (uint8_t)(w >> 16);
    dst->addr[3] = (uint8_t)(w >> 24);
}

void br_net_from_lwip(br_ip_t *dst, const ip_addr_t *src)
{
    *dst = br_ip_any(BR_AF_INET);
    if (IP_IS_V6(src)) {
        from_lwip6(dst, ip_2_ip6(src));
    } else {
        from_lwip4(dst, ip_2_ip4(src));
    }
}

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
    netif->output_ip6 = ethip6_output;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif_create_ip6_linklocal_address(netif, 1);
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

        if (any) {
            continue;
        }

        bool polled = false;
        for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
            polled = polled || (entries[i].used && !entries[i].dev->irq);
        }

        if (polled) {
            br_sleep_ms(BR_NET_POLL_MS);
        } else if (br_sem_take(&rx_sem, BR_NET_IRQ_FALLBACK_MS * 1000ULL) == BR_OK) {
            rx_wakeups++;
        }
    }
}

br_err_t br_net_init(void)
{
    if (initialized) {
        return BR_ERR_BUSY;
    }

    br_net_slots_init();
    br_sem_init(&rx_sem, 0, 1);
    tcpip_init(NULL, NULL);

    br_tid_t tid;
    br_err_t err = br_task_create(&tid, "netrx", rx_task, NULL, 3, rx_stack, sizeof(rx_stack));
    if (err != BR_OK) {
        return err;
    }
    initialized = true;
    return BR_OK;
}

br_err_t br_net_netif_add(br_netif_t *dev, const br_ip_t *ip, const br_ip_t *mask,
                          const br_ip_t *gw)
{
    if (!initialized || dev == NULL || dev->ops == NULL || dev->name == NULL) {
        return BR_ERR_INVALID;
    }
    if ((ip != NULL && ip->family != BR_AF_INET && ip->family != BR_AF_ANY) ||
        (mask != NULL && mask->family != BR_AF_INET && mask->family != BR_AF_ANY) ||
        (gw != NULL && gw->family != BR_AF_INET && gw->family != BR_AF_ANY)) {
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

    ip_addr_t a, m, g;
    br_ip_t any = br_ip_any(BR_AF_INET);
    br_net_to_lwip(&a, ip != NULL && ip->family == BR_AF_INET ? ip : &any);
    br_net_to_lwip(&m, mask != NULL && mask->family == BR_AF_INET ? mask : &any);
    br_net_to_lwip(&g, gw != NULL && gw->family == BR_AF_INET ? gw : &any);
    bool dhcp = ip4_addr_isany_val(*ip_2_ip4(&a));

    e->dev = dev;
    if (dev->irq) {
        dev->rx_notify = rx_notify;
    }

    LOCK_TCPIP_CORE();
    struct netif *added = netif_add(&e->lwip, ip_2_ip4(&a), ip_2_ip4(&m), ip_2_ip4(&g), e,
                                    netif_setup, tcpip_input);
    if (added != NULL) {
        if (netif_default == NULL || netif_default == netif_list) {
            netif_set_default(&e->lwip);
        }
        netif_set_up(&e->lwip);
        if (dhcp) {
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

br_err_t br_net_ip6_add(br_netif_t *dev, const br_ip_t *addr)
{
    if (!initialized || dev == NULL || addr == NULL || addr->family != BR_AF_INET6) {
        return BR_ERR_INVALID;
    }

    br_err_t result = BR_ERR_NOENT;

    for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
        if (!entries[i].used || entries[i].dev != dev) {
            continue;
        }

        ip_addr_t a;
        br_net_to_lwip(&a, addr);

        s8_t idx;
        LOCK_TCPIP_CORE();
        err_t err = netif_add_ip6_address(&entries[i].lwip, ip_2_ip6(&a), &idx);
        UNLOCK_TCPIP_CORE();
        result = err == ERR_OK ? BR_OK : BR_ERR_NOMEM;
        break;
    }
    return result;
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

br_err_t br_net_wait_ip6(br_netif_t *dev, br_time_t timeout)
{
    br_time_t start = br_uptime_us();

    while (1) {
        for (int i = 0; i < BR_NET_MAX_NETIFS; i++) {
            if (!entries[i].used || entries[i].dev != dev) {
                continue;
            }
            for (int k = 0; k < LWIP_IPV6_NUM_ADDRESSES; k++) {
                if (ip6_addr_ispreferred(netif_ip6_addr_state(&entries[i].lwip, k))) {
                    return BR_OK;
                }
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
        from_lwip4(&info->ip, netif_ip4_addr(netif));
        from_lwip4(&info->mask, netif_ip4_netmask(netif));
        from_lwip4(&info->gw, netif_ip4_gw(netif));
        info->scope = netif_get_index(netif);
        info->ip6_count = 0;
        for (int k = 0; k < LWIP_IPV6_NUM_ADDRESSES && info->ip6_count < BR_NET_IP6_MAX; k++) {
            if (ip6_addr_isvalid(netif_ip6_addr_state(netif, k))) {
                from_lwip6(&info->ip6[info->ip6_count++], netif_ip6_addr(netif, k));
            }
        }
        info->up   = netif_is_up(netif) && netif_is_link_up(netif);
        err = BR_OK;
        break;
    }
    UNLOCK_TCPIP_CORE();
    return err;
}

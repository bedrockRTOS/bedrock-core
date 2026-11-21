/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_net.h"

#include "lwip/api.h"
#include "lwip/dns.h"
#include "lwip/tcpip.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/prot/ip.h"
#include "lwip/prot/ip4.h"

#define PING_PAYLOAD  32

typedef struct {
    bool           used;
    int            type;
    struct netconn *conn;
    struct netbuf  *nb;
    size_t         off;
} slot_t;

static slot_t   slots[BR_NET_MAX_SOCKETS];
br_mutex_t      br_net_slot_lock;
static uint16_t ping_seq;

void br_net_slots_init(void)
{
    br_mutex_init(&br_net_slot_lock);
}

static br_err_t map_err(err_t err)
{
    switch (err) {
    case ERR_OK:       return BR_OK;
    case ERR_TIMEOUT:  return BR_ERR_TIMEOUT;
    case ERR_MEM:
    case ERR_BUF:      return BR_ERR_NOMEM;
    case ERR_USE:
    case ERR_ISCONN:
    case ERR_INPROGRESS:
    case ERR_ALREADY:  return BR_ERR_BUSY;
    case ERR_ARG:
    case ERR_VAL:
    case ERR_IF:
    case ERR_RTE:      return BR_ERR_INVALID;
    default:           return BR_ERR_IO;
    }
}

static slot_t *slot_get(int fd)
{
    if (fd < 0 || fd >= BR_NET_MAX_SOCKETS || !slots[fd].used) {
        return NULL;
    }
    return &slots[fd];
}

static int slot_alloc(struct netconn *conn, int type)
{
    br_mutex_lock(&br_net_slot_lock, BR_TIME_INFINITE);

    int fd = BR_ERR_NOMEM;
    for (int i = 0; i < BR_NET_MAX_SOCKETS; i++) {
        if (!slots[i].used) {
            slots[i].used = true;
            slots[i].type = type;
            slots[i].conn = conn;
            slots[i].nb   = NULL;
            slots[i].off  = 0;
            fd = i;
            break;
        }
    }

    br_mutex_unlock(&br_net_slot_lock);
    return fd;
}

static void to_lwip_addr(ip_addr_t *dst, uint32_t ip)
{
    IP_ADDR4(dst, (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
}

static uint32_t from_lwip_addr(const ip_addr_t *a)
{
    return lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(a)));
}

static int timeout_ms(br_time_t timeout)
{
    if (timeout == BR_TIME_INFINITE) {
        return 0;
    }
    br_time_t ms = (timeout + 999U) / 1000U;
    if (ms == 0) {
        ms = 1;
    }
    return ms > 0x7FFFFFFFU ? 0x7FFFFFFF : (int)ms;
}

int br_net_socket(int type)
{
    if (br_hal_in_isr() || (type != BR_NET_DGRAM && type != BR_NET_STREAM)) {
        return BR_ERR_INVALID;
    }

    struct netconn *conn = netconn_new(type == BR_NET_DGRAM ? NETCONN_UDP : NETCONN_TCP);
    if (conn == NULL) {
        return BR_ERR_NOMEM;
    }

    int fd = slot_alloc(conn, type);
    if (fd < 0) {
        netconn_delete(conn);
    }
    return fd;
}

br_err_t br_net_bind(int fd, uint32_t ip, uint16_t port)
{
    slot_t *s = slot_get(fd);
    if (s == NULL) {
        return BR_ERR_INVALID;
    }

    ip_addr_t addr;
    to_lwip_addr(&addr, ip);
    return map_err(netconn_bind(s->conn, &addr, port));
}

br_err_t br_net_listen(int fd, int backlog)
{
    (void)backlog;

    slot_t *s = slot_get(fd);
    if (s == NULL || s->type != BR_NET_STREAM) {
        return BR_ERR_INVALID;
    }
    return map_err(netconn_listen(s->conn));
}

int br_net_accept(int fd, br_time_t timeout, uint32_t *ip, uint16_t *port)
{
    slot_t *s = slot_get(fd);
    if (s == NULL || s->type != BR_NET_STREAM) {
        return BR_ERR_INVALID;
    }

    netconn_set_recvtimeout(s->conn, timeout_ms(timeout));

    struct netconn *nc;
    err_t err = netconn_accept(s->conn, &nc);
    if (err != ERR_OK) {
        return map_err(err);
    }

    int nfd = slot_alloc(nc, BR_NET_STREAM);
    if (nfd < 0) {
        netconn_delete(nc);
        return nfd;
    }

    ip_addr_t addr;
    u16_t peer_port = 0;
    if (netconn_peer(nc, &addr, &peer_port) == ERR_OK) {
        if (ip != NULL) {
            *ip = from_lwip_addr(&addr);
        }
        if (port != NULL) {
            *port = peer_port;
        }
    }
    return nfd;
}

br_err_t br_net_connect(int fd, uint32_t ip, uint16_t port)
{
    slot_t *s = slot_get(fd);
    if (s == NULL) {
        return BR_ERR_INVALID;
    }

    ip_addr_t addr;
    to_lwip_addr(&addr, ip);
    return map_err(netconn_connect(s->conn, &addr, port));
}

static int udp_send(slot_t *s, const void *buf, size_t len, const ip_addr_t *to, uint16_t port)
{
    if (len > 0xFFFF) {
        return BR_ERR_INVALID;
    }

    struct netbuf *nb = netbuf_new();
    if (nb == NULL) {
        return BR_ERR_NOMEM;
    }
    if (netbuf_ref(nb, buf, (u16_t)len) != ERR_OK) {
        netbuf_delete(nb);
        return BR_ERR_NOMEM;
    }

    err_t err = to != NULL ? netconn_sendto(s->conn, nb, to, port) : netconn_send(s->conn, nb);
    netbuf_delete(nb);
    return err == ERR_OK ? (int)len : (int)map_err(err);
}

int br_net_send(int fd, const void *buf, size_t len)
{
    slot_t *s = slot_get(fd);
    if (s == NULL || buf == NULL) {
        return BR_ERR_INVALID;
    }

    if (s->type == BR_NET_DGRAM) {
        return udp_send(s, buf, len, NULL, 0);
    }

    size_t written = 0;
    err_t err = netconn_write_partly(s->conn, buf, len, NETCONN_COPY, &written);
    if (err != ERR_OK) {
        return map_err(err);
    }
    return (int)written;
}

int br_net_sendto(int fd, const void *buf, size_t len, uint32_t ip, uint16_t port)
{
    slot_t *s = slot_get(fd);
    if (s == NULL || buf == NULL || s->type != BR_NET_DGRAM) {
        return BR_ERR_INVALID;
    }

    ip_addr_t addr;
    to_lwip_addr(&addr, ip);
    return udp_send(s, buf, len, &addr, port);
}

int br_net_recvfrom(int fd, void *buf, size_t len, br_time_t timeout,
                    uint32_t *ip, uint16_t *port)
{
    slot_t *s = slot_get(fd);
    if (s == NULL || buf == NULL) {
        return BR_ERR_INVALID;
    }

    if (s->nb == NULL) {
        netconn_set_recvtimeout(s->conn, timeout_ms(timeout));

        struct netbuf *nb;
        err_t err = netconn_recv(s->conn, &nb);
        if (err == ERR_CLSD && s->type == BR_NET_STREAM) {
            return 0;
        }
        if (err != ERR_OK) {
            return map_err(err);
        }
        s->nb  = nb;
        s->off = 0;
    }

    if (ip != NULL) {
        *ip = from_lwip_addr(netbuf_fromaddr(s->nb));
    }
    if (port != NULL) {
        *port = netbuf_fromport(s->nb);
    }

    size_t total = netbuf_len(s->nb);
    size_t n = total - s->off;
    if (n > len) {
        n = len;
    }
    netbuf_copy_partial(s->nb, buf, (u16_t)n, (u16_t)s->off);
    s->off += n;

    if (s->type == BR_NET_DGRAM || s->off >= total) {
        netbuf_delete(s->nb);
        s->nb  = NULL;
        s->off = 0;
    }
    return (int)n;
}

int br_net_recv(int fd, void *buf, size_t len, br_time_t timeout)
{
    return br_net_recvfrom(fd, buf, len, timeout, NULL, NULL);
}

br_err_t br_net_close(int fd)
{
    slot_t *s = slot_get(fd);
    if (s == NULL) {
        return BR_ERR_INVALID;
    }

    if (s->nb != NULL) {
        netbuf_delete(s->nb);
        s->nb = NULL;
    }
    if (s->type == BR_NET_STREAM) {
        netconn_close(s->conn);
    }
    err_t err = netconn_delete(s->conn);

    br_mutex_lock(&br_net_slot_lock, BR_TIME_INFINITE);
    s->used = false;
    s->conn = NULL;
    br_mutex_unlock(&br_net_slot_lock);
    return map_err(err);
}

br_err_t br_net_ping(uint32_t ip, br_time_t timeout, uint32_t *rtt_us)
{
    struct netconn *conn = netconn_new_with_proto_and_callback(NETCONN_RAW, IP_PROTO_ICMP, NULL);
    if (conn == NULL) {
        return BR_ERR_NOMEM;
    }

    uint8_t packet[sizeof(struct icmp_echo_hdr) + PING_PAYLOAD];
    struct icmp_echo_hdr *echo = (struct icmp_echo_hdr *)packet;
    uint16_t seq = ++ping_seq;

    ICMPH_TYPE_SET(echo, ICMP_ECHO);
    ICMPH_CODE_SET(echo, 0);
    echo->chksum = 0;
    echo->id     = lwip_htons(0xB3D0);
    echo->seqno  = lwip_htons(seq);
    for (int i = 0; i < PING_PAYLOAD; i++) {
        packet[sizeof(*echo) + i] = (uint8_t)('a' + i % 23);
    }
    echo->chksum = inet_chksum(packet, sizeof(packet));

    struct netbuf *nb = netbuf_new();
    if (nb == NULL || netbuf_ref(nb, packet, sizeof(packet)) != ERR_OK) {
        if (nb != NULL) {
            netbuf_delete(nb);
        }
        netconn_delete(conn);
        return BR_ERR_NOMEM;
    }

    ip_addr_t dest;
    to_lwip_addr(&dest, ip);

    br_time_t start = br_uptime_us();
    err_t err = netconn_sendto(conn, nb, &dest, 0);
    netbuf_delete(nb);
    if (err != ERR_OK) {
        netconn_delete(conn);
        return map_err(err);
    }

    br_err_t result = BR_ERR_TIMEOUT;

    while (result == BR_ERR_TIMEOUT) {
        br_time_t spent = br_uptime_us() - start;
        if (spent >= timeout) {
            break;
        }
        netconn_set_recvtimeout(conn, timeout_ms(timeout - spent));

        struct netbuf *reply;
        if (netconn_recv(conn, &reply) != ERR_OK) {
            break;
        }

        uint8_t head[sizeof(struct ip_hdr)];
        if (netbuf_len(reply) >= sizeof(head) + sizeof(struct icmp_echo_hdr)) {
            netbuf_copy_partial(reply, head, sizeof(head), 0);
            size_t ihl = (size_t)(head[0] & 0x0F) * 4;
            struct icmp_echo_hdr r;
            if (netbuf_len(reply) >= ihl + sizeof(r)) {
                netbuf_copy_partial(reply, &r, sizeof(r), (u16_t)ihl);
                if (ICMPH_TYPE(&r) == ICMP_ER && r.id == lwip_htons(0xB3D0) &&
                    r.seqno == lwip_htons(seq) &&
                    from_lwip_addr(netbuf_fromaddr(reply)) == ip) {
                    if (rtt_us != NULL) {
                        *rtt_us = (uint32_t)(br_uptime_us() - start);
                    }
                    result = BR_OK;
                }
            }
        }
        netbuf_delete(reply);
    }

    netconn_delete(conn);
    return result;
}

br_err_t br_net_resolve(const char *name, uint32_t *ip)
{
    if (name == NULL || ip == NULL || br_hal_in_isr()) {
        return BR_ERR_INVALID;
    }

    size_t n = 0;
    while (name[n] != '\0') {
        n++;
    }
    if (n == 0 || n >= DNS_MAX_NAME_LENGTH) {
        return BR_ERR_INVALID;
    }

    ip_addr_t addr;
    err_t err = netconn_gethostbyname_addrtype(name, &addr, NETCONN_DNS_IPV4);
    if (err == ERR_OK) {
        *ip = from_lwip_addr(&addr);
        return BR_OK;
    }
    return err == ERR_VAL ? BR_ERR_NOENT : map_err(err);
}

br_err_t br_net_set_dns(uint32_t ip)
{
    ip_addr_t addr;
    to_lwip_addr(&addr, ip);

    LOCK_TCPIP_CORE();
    dns_setserver(0, &addr);
    UNLOCK_TCPIP_CORE();
    return BR_OK;
}

uint32_t br_net_dns(void)
{
    LOCK_TCPIP_CORE();
    const ip_addr_t *addr = dns_getserver(0);
    uint32_t ip = ip_addr_isany(addr) ? 0 : from_lwip_addr(addr);
    UNLOCK_TCPIP_CORE();
    return ip;
}

#if CONFIG_MPU

#include "bedrock/br_syscall.h"

#define USER_NAME_MAX  64
#define BLOCK_MAX      8

static struct {
    br_tid_t owner;
    bool     used;
} sock_owner[BR_NET_MAX_SOCKETS];

static bool sock_ok(const br_tcb_t *t, uint32_t fd)
{
    return fd < BR_NET_MAX_SOCKETS && sock_owner[fd].used && sock_owner[fd].owner == t->id;
}

static void sock_claim(const br_tcb_t *t, int fd)
{
    if (fd >= 0 && fd < BR_NET_MAX_SOCKETS) {
        sock_owner[fd].owner = t->id;
        sock_owner[fd].used  = true;
    }
}

static bool out_ok(const br_tcb_t *t, uint32_t ptr, size_t size)
{
    return ptr % 4 == 0 && br_user_access_ok(t, (const void *)ptr, size, BR_MEM_W);
}

static bool block_get(const br_tcb_t *t, uint32_t ptr, uint32_t *out, size_t words)
{
    if (ptr % 4 != 0 || !br_user_access_ok(t, (const void *)ptr, words * 4, BR_MEM_R)) {
        return false;
    }
    for (size_t i = 0; i < words; i++) {
        out[i] = ((const uint32_t *)ptr)[i];
    }
    return true;
}

static br_time_t to_time(uint32_t lo, uint32_t hi)
{
    return ((br_time_t)hi << 32) | lo;
}

static uint32_t net_handler(br_tcb_t *t, const uint32_t *a)
{
    uint32_t b[BLOCK_MAX];
    int32_t  r = BR_ERR_INVALID;

    switch (a[0]) {
    case BR_SYS_NET_SOCKET:
        r = br_net_socket((int)a[1]);
        sock_claim(t, r);
        break;

    case BR_SYS_NET_BIND:
        r = sock_ok(t, a[1]) ? br_net_bind((int)a[1], a[2], (uint16_t)a[3]) : BR_ERR_INVALID;
        break;

    case BR_SYS_NET_LISTEN:
        r = sock_ok(t, a[1]) ? br_net_listen((int)a[1], (int)a[2]) : BR_ERR_INVALID;
        break;

    case BR_SYS_NET_CONNECT:
        r = sock_ok(t, a[1]) ? br_net_connect((int)a[1], a[2], (uint16_t)a[3]) : BR_ERR_INVALID;
        break;

    case BR_SYS_NET_CLOSE:
        if (sock_ok(t, a[1])) {
            r = br_net_close((int)a[1]);
            sock_owner[a[1]].used = false;
        }
        break;

    case BR_SYS_NET_SEND:
        if (!sock_ok(t, a[1])) {
            r = BR_ERR_INVALID;
        } else if (!br_user_access_ok(t, (const void *)a[2], a[3], BR_MEM_R)) {
            r = BR_ERR_PERM;
        } else {
            r = br_net_send((int)a[1], (const void *)a[2], a[3]);
        }
        break;

    case BR_SYS_NET_SENDTO:
        if (!block_get(t, a[1], b, 5)) {
            r = BR_ERR_PERM;
        } else if (!sock_ok(t, b[0])) {
            r = BR_ERR_INVALID;
        } else if (!br_user_access_ok(t, (const void *)b[1], b[2], BR_MEM_R)) {
            r = BR_ERR_PERM;
        } else {
            r = br_net_sendto((int)b[0], (const void *)b[1], b[2], b[3], (uint16_t)b[4]);
        }
        break;

    case BR_SYS_NET_RECV:
        if (!block_get(t, a[1], b, 5)) {
            r = BR_ERR_PERM;
        } else if (!sock_ok(t, b[0])) {
            r = BR_ERR_INVALID;
        } else if (!br_user_access_ok(t, (const void *)b[1], b[2], BR_MEM_W)) {
            r = BR_ERR_PERM;
        } else {
            r = br_net_recv((int)b[0], (void *)b[1], b[2], to_time(b[3], b[4]));
        }
        break;

    case BR_SYS_NET_RECVFROM:
        if (!block_get(t, a[1], b, 7)) {
            r = BR_ERR_PERM;
        } else if (!sock_ok(t, b[0])) {
            r = BR_ERR_INVALID;
        } else if (!br_user_access_ok(t, (const void *)b[1], b[2], BR_MEM_W) ||
                   !out_ok(t, b[5], sizeof(uint32_t)) || !out_ok(t, b[6], sizeof(uint32_t))) {
            r = BR_ERR_PERM;
        } else {
            uint32_t ip = 0;
            uint16_t port = 0;
            r = br_net_recvfrom((int)b[0], (void *)b[1], b[2], to_time(b[3], b[4]), &ip, &port);
            if (r >= 0) {
                *(uint32_t *)b[5] = ip;
                *(uint32_t *)b[6] = port;
            }
        }
        break;

    case BR_SYS_NET_ACCEPT:
        if (!block_get(t, a[1], b, 5)) {
            r = BR_ERR_PERM;
        } else if (!sock_ok(t, b[0])) {
            r = BR_ERR_INVALID;
        } else if (!out_ok(t, b[3], sizeof(uint32_t)) || !out_ok(t, b[4], sizeof(uint32_t))) {
            r = BR_ERR_PERM;
        } else {
            uint32_t ip = 0;
            uint16_t port = 0;
            r = br_net_accept((int)b[0], to_time(b[1], b[2]), &ip, &port);
            if (r >= 0) {
                sock_claim(t, r);
                *(uint32_t *)b[3] = ip;
                *(uint32_t *)b[4] = port;
            }
        }
        break;

    case BR_SYS_NET_PING:
        if (!block_get(t, a[1], b, 4)) {
            r = BR_ERR_PERM;
        } else if (!out_ok(t, b[3], sizeof(uint32_t))) {
            r = BR_ERR_PERM;
        } else {
            uint32_t rtt = 0;
            r = br_net_ping(b[0], to_time(b[1], b[2]), &rtt);
            if (r == BR_OK) {
                *(uint32_t *)b[3] = rtt;
            }
        }
        break;

    case BR_SYS_NET_RESOLVE: {
        char name[USER_NAME_MAX];
        r = br_user_copy_string(t, (const void *)a[1], name, sizeof(name));
        if (r >= 0 && !out_ok(t, a[2], sizeof(uint32_t))) {
            r = BR_ERR_PERM;
        } else if (r >= 0) {
            uint32_t ip = 0;
            r = br_net_resolve(name, &ip);
            if (r == BR_OK) {
                *(uint32_t *)a[2] = ip;
            }
        }
        break;
    }

    case BR_SYS_NET_IFINFO: {
        br_net_ifinfo_t info;
        if (!out_ok(t, a[2], sizeof(info))) {
            r = BR_ERR_PERM;
        } else {
            r = br_net_ifinfo(a[1], &info);
            if (r == BR_OK) {
                *(br_net_ifinfo_t *)a[2] = info;
            }
        }
        break;
    }

    default:
        break;
    }

    return (uint32_t)r;
}

static void net_release(br_tid_t tid)
{
    for (int i = 0; i < BR_NET_MAX_SOCKETS; i++) {
        if (sock_owner[i].used && sock_owner[i].owner == tid) {
            br_net_close(i);
            sock_owner[i].used = false;
        }
    }
}

br_err_t br_net_enable_user_calls(void)
{
    br_err_t err = br_syscall_register(BR_SYS_NET_SOCKET, 13, net_handler);
    if (err == BR_OK) {
        err = br_user_release_register(net_release);
    }
    return err;
}

#endif

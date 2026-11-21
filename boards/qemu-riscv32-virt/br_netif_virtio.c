/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#include "bedrock/br_hal.h"
#include "bedrock/br_netif.h"

#define VIRTIO_MMIO_BASE     0x10001000UL
#define VIRTIO_MMIO_STRIDE   0x1000UL
#define VIRTIO_MMIO_SLOTS    8

#define VIRTIO_REG(base, off)  (*(volatile uint32_t *)((base) + (off)))

#define VIRTIO_MAGIC            0x74726976UL
#define VIRTIO_ID_NET           1UL

#define REG_MAGIC               0x000UL
#define REG_VERSION             0x004UL
#define REG_DEVICE_ID           0x008UL
#define REG_DEVICE_FEATURES     0x010UL
#define REG_DEVICE_FEATURES_SEL 0x014UL
#define REG_DRIVER_FEATURES     0x020UL
#define REG_DRIVER_FEATURES_SEL 0x024UL
#define REG_QUEUE_SEL           0x030UL
#define REG_QUEUE_NUM_MAX       0x034UL
#define REG_QUEUE_NUM           0x038UL
#define REG_QUEUE_READY         0x044UL
#define REG_QUEUE_NOTIFY        0x050UL
#define REG_INTERRUPT_STATUS    0x060UL
#define REG_INTERRUPT_ACK       0x064UL
#define REG_STATUS              0x070UL
#define REG_QUEUE_DESC_LOW      0x080UL
#define REG_QUEUE_DESC_HIGH     0x084UL
#define REG_QUEUE_AVAIL_LOW     0x090UL
#define REG_QUEUE_AVAIL_HIGH    0x094UL
#define REG_QUEUE_USED_LOW      0x0A0UL
#define REG_QUEUE_USED_HIGH     0x0A4UL
#define REG_CONFIG              0x100UL

#define STATUS_ACK              1UL
#define STATUS_DRIVER           2UL
#define STATUS_DRIVER_OK        4UL
#define STATUS_FEATURES_OK      8UL

#define FEATURE_NET_MAC_LOW     (1UL << 5)
#define FEATURE_VERSION_1_HIGH  (1UL << 0)

#define QUEUE_RX                0
#define QUEUE_TX                1
#define QUEUE_SIZE              8
#define BUF_SIZE                (BR_NETIF_FRAME_MAX + 12)

#define DESC_F_WRITE            2

typedef struct {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
    uint16_t used_event;
} vring_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct {
    uint16_t          flags;
    uint16_t          idx;
    vring_used_elem_t ring[QUEUE_SIZE];
    uint16_t          avail_event;
} vring_used_t;

typedef struct {
    vring_desc_t  desc[QUEUE_SIZE] __attribute__((aligned(16)));
    vring_avail_t avail            __attribute__((aligned(2)));
    vring_used_t  used             __attribute__((aligned(4)));
    uint16_t      last_used;
} virtqueue_t;

static virtqueue_t rxq __attribute__((aligned(4096)));
static virtqueue_t txq __attribute__((aligned(4096)));
static uint8_t     rx_buf[QUEUE_SIZE][BUF_SIZE] __attribute__((aligned(16)));
static uint8_t     tx_buf[BUF_SIZE] __attribute__((aligned(16)));

#define PLIC_PRIORITY(irq)  (*(volatile uint32_t *)(0x0C000000UL + 4UL * (irq)))
#define PLIC_ENABLE_CTX0    (*(volatile uint32_t *)0x0C002000UL)

static br_netif_t netif;
static uintptr_t  base;
static uint32_t   irq_number;
static bool       ready;

bool br_hal_netif_irq(uint32_t irq);

bool br_hal_netif_irq(uint32_t irq)
{
    if (!ready || irq != irq_number) {
        return false;
    }

    VIRTIO_REG(base, REG_INTERRUPT_ACK) = VIRTIO_REG(base, REG_INTERRUPT_STATUS);

    if (netif.rx_notify != NULL) {
        netif.rx_notify(&netif);
    }
    return true;
}

static void barrier(void)
{
    __sync_synchronize();
}

static void queue_setup(uint32_t index, virtqueue_t *q)
{
    VIRTIO_REG(base, REG_QUEUE_SEL) = index;
    VIRTIO_REG(base, REG_QUEUE_NUM) = QUEUE_SIZE;
    VIRTIO_REG(base, REG_QUEUE_DESC_LOW)   = (uint32_t)(uintptr_t)q->desc;
    VIRTIO_REG(base, REG_QUEUE_DESC_HIGH)  = 0;
    VIRTIO_REG(base, REG_QUEUE_AVAIL_LOW)  = (uint32_t)(uintptr_t)&q->avail;
    VIRTIO_REG(base, REG_QUEUE_AVAIL_HIGH) = 0;
    VIRTIO_REG(base, REG_QUEUE_USED_LOW)   = (uint32_t)(uintptr_t)&q->used;
    VIRTIO_REG(base, REG_QUEUE_USED_HIGH)  = 0;
    VIRTIO_REG(base, REG_QUEUE_READY)      = 1;
}

static void rx_post(uint16_t id)
{
    rxq.desc[id].addr_lo = (uint32_t)(uintptr_t)rx_buf[id];
    rxq.desc[id].addr_hi = 0;
    rxq.desc[id].len     = BUF_SIZE;
    rxq.desc[id].flags   = DESC_F_WRITE;
    rxq.desc[id].next    = 0;

    rxq.avail.ring[rxq.avail.idx % QUEUE_SIZE] = id;
    barrier();
    rxq.avail.idx++;
    barrier();
    VIRTIO_REG(base, REG_QUEUE_NOTIFY) = QUEUE_RX;
}

static br_err_t virtio_send(br_netif_t *dev, const void *frame, size_t len)
{
    (void)dev;
    const uint8_t *f = frame;

    if (len == 0 || len > BR_NETIF_FRAME_MAX) {
        return BR_ERR_INVALID;
    }

    for (int i = 0; i < 12; i++) {
        tx_buf[i] = 0;
    }
    for (size_t i = 0; i < len; i++) {
        tx_buf[12 + i] = f[i];
    }

    txq.desc[0].addr_lo = (uint32_t)(uintptr_t)tx_buf;
    txq.desc[0].addr_hi = 0;
    txq.desc[0].len     = (uint32_t)(len + 12);
    txq.desc[0].flags   = 0;
    txq.desc[0].next    = 0;

    txq.avail.ring[txq.avail.idx % QUEUE_SIZE] = 0;
    barrier();
    txq.avail.idx++;
    barrier();
    VIRTIO_REG(base, REG_QUEUE_NOTIFY) = QUEUE_TX;

    uint32_t spins = 1000000UL;
    while (*(volatile uint16_t *)&txq.used.idx == txq.last_used) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    barrier();
    txq.last_used++;
    return BR_OK;
}

static int virtio_recv(br_netif_t *dev, void *buf, size_t size)
{
    (void)dev;

    if (*(volatile uint16_t *)&rxq.used.idx == rxq.last_used) {
        return 0;
    }
    barrier();

    vring_used_elem_t e = rxq.used.ring[rxq.last_used % QUEUE_SIZE];
    rxq.last_used++;

    int result;
    if (e.len <= 12 || e.id >= QUEUE_SIZE) {
        result = BR_ERR_IO;
    } else if (e.len - 12 > size) {
        result = BR_ERR_OVERFLOW;
    } else {
        uint8_t *out = buf;
        for (uint32_t i = 0; i < e.len - 12; i++) {
            out[i] = rx_buf[e.id][12 + i];
        }
        result = (int)(e.len - 12);
    }

    if (e.id < QUEUE_SIZE) {
        rx_post((uint16_t)e.id);
    }
    return result;
}

static bool virtio_link_up(br_netif_t *dev)
{
    (void)dev;
    return true;
}

static const br_netif_ops_t virtio_ops = {
    .send    = virtio_send,
    .recv    = virtio_recv,
    .link_up = virtio_link_up,
};

static bool find_device(void)
{
    for (uint32_t i = 0; i < VIRTIO_MMIO_SLOTS; i++) {
        uintptr_t b = VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_STRIDE;
        if (VIRTIO_REG(b, REG_MAGIC) == VIRTIO_MAGIC &&
            VIRTIO_REG(b, REG_DEVICE_ID) == VIRTIO_ID_NET &&
            VIRTIO_REG(b, REG_VERSION) == 2) {
            base       = b;
            irq_number = 1 + i;
            return true;
        }
    }
    return false;
}

br_netif_t *br_hal_netif_default(void)
{
    if (ready) {
        return &netif;
    }
    if (!find_device()) {
        return NULL;
    }

    VIRTIO_REG(base, REG_STATUS) = 0;
    VIRTIO_REG(base, REG_STATUS) = STATUS_ACK;
    VIRTIO_REG(base, REG_STATUS) = STATUS_ACK | STATUS_DRIVER;

    VIRTIO_REG(base, REG_DEVICE_FEATURES_SEL) = 0;
    uint32_t low = VIRTIO_REG(base, REG_DEVICE_FEATURES);
    VIRTIO_REG(base, REG_DEVICE_FEATURES_SEL) = 1;
    uint32_t high = VIRTIO_REG(base, REG_DEVICE_FEATURES);
    if (!(high & FEATURE_VERSION_1_HIGH)) {
        return NULL;
    }

    VIRTIO_REG(base, REG_DRIVER_FEATURES_SEL) = 0;
    VIRTIO_REG(base, REG_DRIVER_FEATURES) = low & FEATURE_NET_MAC_LOW;
    VIRTIO_REG(base, REG_DRIVER_FEATURES_SEL) = 1;
    VIRTIO_REG(base, REG_DRIVER_FEATURES) = FEATURE_VERSION_1_HIGH;

    VIRTIO_REG(base, REG_STATUS) = STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK;
    if (!(VIRTIO_REG(base, REG_STATUS) & STATUS_FEATURES_OK)) {
        return NULL;
    }

    queue_setup(QUEUE_RX, &rxq);
    queue_setup(QUEUE_TX, &txq);

    VIRTIO_REG(base, REG_STATUS) = STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK |
                                   STATUS_DRIVER_OK;

    for (uint16_t i = 0; i < QUEUE_SIZE; i++) {
        rx_post(i);
    }

    static const uint8_t default_mac[BR_NETIF_MAC_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    for (int i = 0; i < BR_NETIF_MAC_LEN; i++) {
        netif.mac[i] = (low & FEATURE_NET_MAC_LOW)
                           ? *(volatile uint8_t *)(base + REG_CONFIG + (uintptr_t)i)
                           : default_mac[i];
    }

    netif.name = "eth0";
    netif.mtu  = 1500;
    netif.ops  = &virtio_ops;
    netif.priv = NULL;
    netif.irq  = true;
    ready      = true;

    PLIC_PRIORITY(irq_number) = 1;
    PLIC_ENABLE_CTX0 |= 1UL << irq_number;
    return &netif;
}

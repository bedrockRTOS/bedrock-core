/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_netif_mock.h"

static void copy(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static br_err_t mock_send(br_netif_t *netif, const void *frame, size_t len)
{
    br_netif_mock_t *m = netif->priv;

    if (len > BR_NETIF_FRAME_MAX) {
        return BR_ERR_OVERFLOW;
    }

    uint32_t key = br_hal_irq_disable();
    if (m->tx_count == BR_NETIF_MOCK_DEPTH) {
        br_hal_irq_restore(key);
        return BR_ERR_NOMEM;
    }

    br_netif_mock_frame_t *f = &m->tx[(m->tx_head + m->tx_count) % BR_NETIF_MOCK_DEPTH];
    copy(f->data, frame, len);
    f->len = (uint16_t)len;
    m->tx_count++;
    br_hal_irq_restore(key);
    return BR_OK;
}

static int mock_recv(br_netif_t *netif, void *buf, size_t size)
{
    br_netif_mock_t *m = netif->priv;

    uint32_t key = br_hal_irq_disable();
    if (m->rx_count == 0) {
        br_hal_irq_restore(key);
        return 0;
    }

    br_netif_mock_frame_t *f = &m->rx[m->rx_head];
    int len = f->len;
    if ((size_t)len > size) {
        len = BR_ERR_OVERFLOW;
    } else {
        copy(buf, f->data, (size_t)len);
    }
    m->rx_head = (uint8_t)((m->rx_head + 1) % BR_NETIF_MOCK_DEPTH);
    m->rx_count--;
    br_hal_irq_restore(key);
    return len;
}

static bool mock_link_up(br_netif_t *netif)
{
    br_netif_mock_t *m = netif->priv;
    return m->link;
}

static const br_netif_ops_t mock_ops = {
    .send    = mock_send,
    .recv    = mock_recv,
    .link_up = mock_link_up,
};

br_err_t br_netif_mock_init(br_netif_mock_t *mock, const char *name,
                            const uint8_t mac[BR_NETIF_MAC_LEN])
{
    if (mock == NULL || name == NULL || mac == NULL) {
        return BR_ERR_INVALID;
    }

    mock->tx_head  = 0;
    mock->tx_count = 0;
    mock->rx_head  = 0;
    mock->rx_count = 0;
    mock->link     = true;

    mock->netif.name      = name;
    mock->netif.mtu       = 1500;
    mock->netif.ops       = &mock_ops;
    mock->netif.priv      = mock;
    mock->netif.irq       = false;
    mock->netif.rx_notify = NULL;
    copy(mock->netif.mac, mac, BR_NETIF_MAC_LEN);
    return BR_OK;
}

br_err_t br_netif_mock_inject(br_netif_mock_t *mock, const void *frame, size_t len)
{
    if (mock == NULL || frame == NULL || len > BR_NETIF_FRAME_MAX) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    if (mock->rx_count == BR_NETIF_MOCK_DEPTH) {
        br_hal_irq_restore(key);
        return BR_ERR_NOMEM;
    }

    br_netif_mock_frame_t *f = &mock->rx[(mock->rx_head + mock->rx_count) % BR_NETIF_MOCK_DEPTH];
    copy(f->data, frame, len);
    f->len = (uint16_t)len;
    mock->rx_count++;
    br_hal_irq_restore(key);
    return BR_OK;
}

int br_netif_mock_take(br_netif_mock_t *mock, void *buf, size_t size)
{
    if (mock == NULL || buf == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    if (mock->tx_count == 0) {
        br_hal_irq_restore(key);
        return 0;
    }

    br_netif_mock_frame_t *f = &mock->tx[mock->tx_head];
    int len = f->len;
    if ((size_t)len > size) {
        len = BR_ERR_OVERFLOW;
    } else {
        copy(buf, f->data, (size_t)len);
    }
    mock->tx_head = (uint8_t)((mock->tx_head + 1) % BR_NETIF_MOCK_DEPTH);
    mock->tx_count--;
    br_hal_irq_restore(key);
    return len;
}

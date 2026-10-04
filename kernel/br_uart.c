/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

void br_uart_rx_push(char c);

static char   rx_buf[CONFIG_UART_RX_BUF_SIZE];
static size_t rx_head;
static size_t rx_tail;
static size_t rx_count;

static br_sem_t rx_sem = {
    .count      = 0,
    .max_count  = CONFIG_UART_RX_BUF_SIZE,
    .wait_queue = NULL,
};

void br_uart_rx_push(char c)
{
    uint32_t key = br_hal_irq_disable();

    if (rx_count == CONFIG_UART_RX_BUF_SIZE) {
        br_hal_irq_restore(key);
        return;
    }

    rx_buf[rx_head] = c;
    rx_head = (rx_head + 1) % CONFIG_UART_RX_BUF_SIZE;
    rx_count++;

    br_hal_irq_restore(key);
    br_sem_give(&rx_sem);
}

br_err_t br_uart_getc(char *c, br_time_t timeout)
{
    if (c == NULL) {
        return BR_ERR_INVALID;
    }

    br_err_t err = br_sem_take(&rx_sem, timeout);
    if (err != BR_OK) {
        return err;
    }

    uint32_t key = br_hal_irq_disable();

    *c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % CONFIG_UART_RX_BUF_SIZE;
    rx_count--;

    br_hal_irq_restore(key);
    return BR_OK;
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_uart_dev.h"

static int uart_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    (void)dev;
    char *out = buf;
    size_t n = 0;

    while (n < len) {
        br_err_t err = br_uart_getc(&out[n], n == 0 ? timeout : 0);
        if (err != BR_OK) {
            if (n > 0 || err == BR_ERR_TIMEOUT) {
                break;
            }
            return (int)err;
        }
        n++;
    }

    if (n == 0 && len > 0) {
        return BR_ERR_TIMEOUT;
    }
    return (int)n;
}

static int uart_write(br_device_t *dev, const void *buf, size_t len,
                      br_time_t timeout)
{
    (void)dev;
    (void)timeout;
    const char *in = buf;

    for (size_t i = 0; i < len; i++) {
        br_uart_putc(in[i]);
    }
    return (int)len;
}

static const br_device_ops_t uart_ops = {
    .read  = uart_read,
    .write = uart_write,
};

br_err_t br_uart_dev_register(br_uart_dev_t *uart, const char *name)
{
    if (uart == NULL) {
        return BR_ERR_INVALID;
    }
    return br_device_register(&uart->dev, name, &uart_ops, uart);
}

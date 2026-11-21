/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_UART_DEV_H
#define BR_UART_DEV_H

#include "bedrock/bedrock.h"

typedef struct {
    br_device_t dev;
} br_uart_dev_t;

br_err_t br_uart_dev_register(br_uart_dev_t *uart, const char *name);

#endif

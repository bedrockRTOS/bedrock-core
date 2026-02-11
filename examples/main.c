/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "bedrock/bedrock.h"

/* UART output (defined in arch/arm-cortex-m/br_hal_uart.c) */
extern void br_uart_puts(const char *s);

static uint8_t stack_task_a[1024];
static uint8_t stack_task_b[1024];

static void task_a(void *arg)
{
    (void)arg;
    while (1) {
        br_uart_puts("[A] tick\n");
        br_sleep_ms(500);
    }
}

static void task_b(void *arg)
{
    (void)arg;
    while (1) {
        br_uart_puts("[B] tock\n");
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("bedrock[RTOS] v0.0.1 booting...\n");

    br_tid_t tid_a, tid_b;

    br_task_create(&tid_a, "task_a", task_a, NULL,
                   1, stack_task_a, sizeof(stack_task_a));

    br_task_create(&tid_b, "task_b", task_b, NULL,
                   2, stack_task_b, sizeof(stack_task_b));

    br_uart_puts("Starting scheduler\n");
    br_kernel_start();
}

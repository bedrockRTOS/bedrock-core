/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE-GPL-3.0.md for the full terms and runtime exception.
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

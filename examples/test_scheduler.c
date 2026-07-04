/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

extern void br_uart_puts(const char *s);
extern void br_uart_putc(char c);

static void uart_put_uint(uint32_t v)
{
    char buf[10];
    int len = 0;

    if (v == 0) {
        br_uart_putc('0');
        return;
    }

    while (v > 0 && len < (int)sizeof(buf)) {
        buf[len++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (len > 0) {
        br_uart_putc(buf[--len]);
    }
}

/* Test 1: priority ordering */

static uint8_t stack_prio_low[512];
static uint8_t stack_prio_high[512];

static char    prio_log[3];
static int     prio_log_len;

static void prio_low_task(void *arg)
{
    (void)arg;
    prio_log[prio_log_len++] = 'L';
    br_task_suspend(br_task_self());
}

static void prio_high_task(void *arg)
{
    (void)arg;
    prio_log[prio_log_len++] = 'H';
    br_task_suspend(br_task_self());
}

static void run_priority_test(void)
{
    br_uart_puts("Test 1: Priority ordering\n");

    prio_log_len = 0;

    br_tid_t low_tid, high_tid;
    br_task_create(&low_tid, "prio_low", prio_low_task, NULL, 6,
                   stack_prio_low, sizeof(stack_prio_low));
    br_task_create(&high_tid, "prio_high", prio_high_task, NULL, 2,
                   stack_prio_high, sizeof(stack_prio_high));

    br_sleep_ms(20);

    if (prio_log_len == 2 && prio_log[0] == 'H' && prio_log[1] == 'L') {
        br_uart_puts("Test 1: PASS - higher priority task ran first\n\n");
    } else {
        br_uart_puts("Test 1: FAIL - order was: ");
        for (int i = 0; i < prio_log_len; i++) {
            br_uart_putc(prio_log[i]);
        }
        br_uart_puts("\n\n");
    }
}

/* Test 2: round-robin */

#define RR_TASK_COUNT 3

static uint8_t          stack_rr[RR_TASK_COUNT][512];
static volatile uint32_t rr_counter[RR_TASK_COUNT];
static br_tid_t          rr_tid[RR_TASK_COUNT];

static void rr_task(void *arg)
{
    int idx = (int)(uintptr_t)arg;
    while (1) {
        rr_counter[idx]++;
    }
}

static void run_round_robin_test(void)
{
    br_uart_puts("Test 2: Round-robin at equal priority\n");

    for (int i = 0; i < RR_TASK_COUNT; i++) {
        rr_counter[i] = 0;
        br_task_create(&rr_tid[i], "rr", rr_task, (void *)(uintptr_t)i, 4,
                       stack_rr[i], sizeof(stack_rr[i]));
    }

    br_sleep_ms(80);

    for (int i = 0; i < RR_TASK_COUNT; i++) {
        br_task_suspend(rr_tid[i]);
    }

    bool all_ran = true;
    for (int i = 0; i < RR_TASK_COUNT; i++) {
        br_uart_puts("  task ");
        uart_put_uint((uint32_t)i);
        br_uart_puts(" count = ");
        uart_put_uint(rr_counter[i]);
        br_uart_puts("\n");
        if (rr_counter[i] == 0) {
            all_ran = false;
        }
    }

    if (all_ran) {
        br_uart_puts("Test 2: PASS - all equal-priority tasks made progress\n\n");
    } else {
        br_uart_puts("Test 2: FAIL - at least one task was starved\n\n");
    }
}

/* Test 3: preemption */

static uint8_t           stack_preempt_low[512];
static uint8_t           stack_preempt_high[512];
static volatile uint32_t preempt_low_counter;
static volatile bool     preempt_high_ran;
static volatile uint32_t preempt_low_counter_at_high_start;

static void preempt_low_task(void *arg)
{
    (void)arg;
    while (1) {
        preempt_low_counter++;
    }
}

static void preempt_high_task(void *arg)
{
    (void)arg;
    br_sleep_ms(30);
    preempt_low_counter_at_high_start = preempt_low_counter;
    preempt_high_ran = true;
    br_task_suspend(br_task_self());
}

static void run_preemption_test(void)
{
    br_uart_puts("Test 3: Preemption of a non-yielding low-priority task\n");

    preempt_low_counter = 0;
    preempt_high_ran = false;
    preempt_low_counter_at_high_start = 0;

    br_tid_t low_tid, high_tid;
    br_task_create(&high_tid, "preempt_high", preempt_high_task, NULL, 1,
                   stack_preempt_high, sizeof(stack_preempt_high));
    br_task_create(&low_tid, "preempt_low", preempt_low_task, NULL, 6,
                   stack_preempt_low, sizeof(stack_preempt_low));

    br_sleep_ms(60);

    br_task_suspend(low_tid);

    if (preempt_high_ran && preempt_low_counter_at_high_start > 0) {
        br_uart_puts("Test 3: PASS - high task preempted the busy low task\n\n");
    } else {
        br_uart_puts("Test 3: FAIL - preemption did not happen as expected\n\n");
    }
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Scheduler Test ===\n\n");

    run_priority_test();
    run_round_robin_test();
    run_preemption_test();

    br_uart_puts("=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Scheduler Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

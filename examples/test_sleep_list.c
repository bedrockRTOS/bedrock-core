/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

extern void br_uart_puts(const char *s);

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

/* Test 1: tasks inserted out of chronological order must wake up in
 * ascending wake-time order, not creation order. */

static uint8_t stack_order_a[512];
static uint8_t stack_order_b[512];
static uint8_t stack_order_c[512];

static char order_log[3];
static int  order_log_len;

static void make_order_task(char tag, br_time_t sleep_ms)
{
    br_sleep_ms(sleep_ms);
    order_log[order_log_len++] = tag;
    br_task_suspend(br_task_self());
}

static void order_task_a(void *arg) { (void)arg; make_order_task('A', 30); }
static void order_task_b(void *arg) { (void)arg; make_order_task('B', 10); }
static void order_task_c(void *arg) { (void)arg; make_order_task('C', 20); }

static void run_ordering_test(void)
{
    br_uart_puts("Test 1: Sleep list ordering\n");

    order_log_len = 0;

    br_tid_t tid;
    br_task_create(&tid, "order_a", order_task_a, NULL, 3,
                   stack_order_a, sizeof(stack_order_a));
    br_task_create(&tid, "order_c", order_task_c, NULL, 3,
                   stack_order_c, sizeof(stack_order_c));
    br_task_create(&tid, "order_b", order_task_b, NULL, 3,
                   stack_order_b, sizeof(stack_order_b));

    br_sleep_ms(60);

    bool order_ok = order_log_len == 3 &&
                    order_log[0] == 'B' &&
                    order_log[1] == 'C' &&
                    order_log[2] == 'A';

    br_uart_puts("  wake order: ");
    for (int i = 0; i < order_log_len; i++) {
        char s[2] = { order_log[i], '\0' };
        br_uart_puts(s);
    }
    br_uart_puts("\n");

    print_result(order_ok, "Test 1: tasks woke in ascending wake-time order");
}

/* Test 2: multiple tasks due at (approximately) the same time must all
 * be woken by a single alarm handler pass, not just the list head. */

#define BATCH_TASK_COUNT 3

static uint8_t           stack_batch[BATCH_TASK_COUNT][512];
static volatile uint32_t batch_woken_count;

static void batch_task(void *arg)
{
    (void)arg;
    br_sleep_ms(20);
    batch_woken_count++;
    br_task_suspend(br_task_self());
}

static void run_batch_wakeup_test(void)
{
    br_uart_puts("Test 2: Batch alarm wakeup\n");

    batch_woken_count = 0;

    br_tid_t tid;
    for (int i = 0; i < BATCH_TASK_COUNT; i++) {
        br_task_create(&tid, "batch", batch_task, NULL, 3,
                       stack_batch[i], sizeof(stack_batch[i]));
    }

    br_sleep_ms(60);

    print_result(batch_woken_count == BATCH_TASK_COUNT,
                 "Test 2: all same-deadline tasks woke from one alarm");
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Sleep List Test ===\n\n");

    run_ordering_test();
    run_batch_wakeup_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Sleep List Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

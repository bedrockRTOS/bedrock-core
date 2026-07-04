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

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

/* Test 1: basic take/give (non-blocking fast path) */

static void run_basic_test(void)
{
    br_uart_puts("Test 1: Basic take/give\n");

    br_sem_t sem;
    br_sem_init(&sem, 0, 1);

    br_err_t give_err = br_sem_give(&sem);
    br_err_t take_err = br_sem_take(&sem, 0);

    print_result(give_err == BR_OK && take_err == BR_OK,
                 "Test 1: give then take succeed");
}

/* Test 2: timeout */

static uint8_t           stack_timeout_waiter[512];
static volatile br_err_t timeout_waiter_result = BR_OK;
static volatile br_time_t timeout_waiter_elapsed_us;
static br_sem_t           timeout_sem;

static void timeout_waiter_task(void *arg)
{
    (void)arg;
    br_time_t start = br_uptime_us();
    timeout_waiter_result = br_sem_take(&timeout_sem, BR_MSEC(30));
    timeout_waiter_elapsed_us = br_uptime_us() - start;
    br_task_suspend(br_task_self());
}

static void run_timeout_test(void)
{
    br_uart_puts("Test 2: Timeout\n");

    br_sem_t sem;
    br_sem_init(&sem, 0, 1);

    br_err_t immediate_err = br_sem_take(&sem, 0);
    print_result(immediate_err == BR_ERR_TIMEOUT,
                 "Test 2a: zero timeout returns immediately");

    br_sem_init(&timeout_sem, 0, 1);
    timeout_waiter_result = BR_OK;
    timeout_waiter_elapsed_us = 0;

    br_tid_t waiter_tid;
    br_task_create(&waiter_tid, "sem_timeout_waiter", timeout_waiter_task,
                   NULL, 3, stack_timeout_waiter, sizeof(stack_timeout_waiter));

    br_sleep_ms(60);

    br_uart_puts("  elapsed us = ");
    uart_put_uint((uint32_t)timeout_waiter_elapsed_us);
    br_uart_puts("\n");

    print_result(timeout_waiter_result == BR_ERR_TIMEOUT &&
                 timeout_waiter_elapsed_us >= BR_MSEC(30),
                 "Test 2b: blocked take times out after the requested duration");
}

/* Test 3: overflow */

static void run_overflow_test(void)
{
    br_uart_puts("Test 3: Overflow\n");

    br_sem_t sem;
    br_sem_init(&sem, 0, 2);

    br_err_t err1 = br_sem_give(&sem);
    br_err_t err2 = br_sem_give(&sem);
    br_err_t err3 = br_sem_give(&sem);

    print_result(err1 == BR_OK && err2 == BR_OK && err3 == BR_ERR_OVERFLOW,
                 "Test 3: give beyond max_count is rejected");
}

/* Test 4: give wakes a blocked waiter directly, without touching count */

static uint8_t            stack_wake_waiter[512];
static volatile br_err_t  wake_waiter_result = BR_ERR_INVALID;
static volatile bool      wake_waiter_done;
static br_sem_t           wake_sem;

static void wake_waiter_task(void *arg)
{
    (void)arg;
    wake_waiter_result = br_sem_take(&wake_sem, BR_TIME_INFINITE);
    wake_waiter_done = true;
    br_task_suspend(br_task_self());
}

static void run_wakeup_test(void)
{
    br_uart_puts("Test 4: Give wakes a blocked waiter\n");

    br_sem_init(&wake_sem, 0, 1);
    wake_waiter_result = BR_ERR_INVALID;
    wake_waiter_done = false;

    br_tid_t waiter_tid;
    br_task_create(&waiter_tid, "sem_wake_waiter", wake_waiter_task,
                   NULL, 3, stack_wake_waiter, sizeof(stack_wake_waiter));

    br_sleep_ms(10);

    br_err_t give_err = br_sem_give(&wake_sem);

    br_sleep_ms(10);

    print_result(give_err == BR_OK && wake_waiter_done &&
                 wake_waiter_result == BR_OK,
                 "Test 4: waiter woken by give");
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Semaphore Test ===\n\n");

    run_basic_test();
    run_timeout_test();
    run_overflow_test();
    run_wakeup_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Semaphore Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

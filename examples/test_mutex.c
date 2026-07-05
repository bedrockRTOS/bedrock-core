/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

extern void br_uart_puts(const char *s);

#ifdef BR_HOST_TEST
extern void br_hal_test_force_isr(bool v);
#endif

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

/* Test 1: basic lock/unlock */

static void run_basic_test(void)
{
    br_uart_puts("Test 1: Basic lock/unlock\n");

    br_mutex_t mtx;
    br_mutex_init(&mtx);

    br_err_t l1 = br_mutex_lock(&mtx, 0);
    br_err_t u1 = br_mutex_unlock(&mtx);
    br_err_t l2 = br_mutex_lock(&mtx, 0);
    br_err_t u2 = br_mutex_unlock(&mtx);

    print_result(l1 == BR_OK && u1 == BR_OK && l2 == BR_OK && u2 == BR_OK,
                 "Test 1: lock/unlock succeed when uncontended");
}

/* Test 2: timeout */

static br_mutex_t timeout_mtx;
static uint8_t    stack_timeout_holder[512];

static void timeout_holder_task(void *arg)
{
    (void)arg;
    br_mutex_lock(&timeout_mtx, BR_TIME_INFINITE);
    br_task_suspend(br_task_self());
}

static void run_timeout_test(void)
{
    br_uart_puts("Test 2: Timeout\n");

    br_mutex_init(&timeout_mtx);

    br_tid_t tid;
    br_task_create(&tid, "mtx_holder", timeout_holder_task, NULL, 3,
                   stack_timeout_holder, sizeof(stack_timeout_holder));

    br_sleep_ms(10);

    br_time_t start = br_uptime_us();
    br_err_t err = br_mutex_lock(&timeout_mtx, BR_MSEC(30));
    br_time_t elapsed = br_uptime_us() - start;

    print_result(err == BR_ERR_TIMEOUT && elapsed >= BR_MSEC(30),
                 "Test 2: lock on a held mutex times out");
}

/* Test 3: priority inheritance */

static br_mutex_t pi_mtx;

static uint8_t stack_pi_low[512];
static volatile br_err_t pi_low_lock_result = BR_ERR_INVALID;

static void pi_low_task(void *arg)
{
    (void)arg;
    pi_low_lock_result = br_mutex_lock(&pi_mtx, BR_TIME_INFINITE);
    br_time_t start = br_uptime_us();
    while (br_uptime_us() - start < 40000) { }
    br_mutex_unlock(&pi_mtx);
    br_task_suspend(br_task_self());
}

static uint8_t stack_pi_medium[512];
static volatile uint32_t pi_medium_counter;

static void pi_medium_task(void *arg)
{
    (void)arg;
    while (1) {
        pi_medium_counter++;
    }
}

static uint8_t           stack_pi_high[512];
static volatile br_err_t pi_high_result = BR_ERR_INVALID;

static void pi_high_task(void *arg)
{
    (void)arg;
    pi_high_result = br_mutex_lock(&pi_mtx, BR_MSEC(300));
    br_task_suspend(br_task_self());
}

static void run_priority_inheritance_test(void)
{
    br_uart_puts("Test 3: Priority inheritance\n");

    br_mutex_init(&pi_mtx);
    pi_low_lock_result = BR_ERR_INVALID;
    pi_high_result = BR_ERR_INVALID;
    pi_medium_counter = 0;

    br_tid_t low_tid;
    br_task_create(&low_tid, "pi_low", pi_low_task, NULL, 6,
                   stack_pi_low, sizeof(stack_pi_low));

    /* Let low run and lock the mutex before medium/high exist. */
    br_sleep_ms(5);

    br_tid_t med_tid, high_tid;
    br_task_create(&med_tid, "pi_medium", pi_medium_task, NULL, 4,
                   stack_pi_medium, sizeof(stack_pi_medium));
    br_task_create(&high_tid, "pi_high", pi_high_task, NULL, 1,
                   stack_pi_high, sizeof(stack_pi_high));

    br_sleep_ms(200);

    print_result(pi_low_lock_result == BR_OK && pi_high_result == BR_OK &&
                 pi_medium_counter > 0,
                 "Test 3: boosted low task hands off to high despite a busy medium task");
}

/* Test 4: ISR rejection (host test harness only) */

static void run_isr_rejection_test(void)
{
    br_uart_puts("Test 4: ISR rejection\n");

#ifdef BR_HOST_TEST
    br_mutex_t mtx;
    br_mutex_init(&mtx);

    br_hal_test_force_isr(true);
    br_err_t lock_err = br_mutex_lock(&mtx, BR_TIME_INFINITE);
    br_err_t unlock_err = br_mutex_unlock(&mtx);
    br_hal_test_force_isr(false);

    print_result(lock_err == BR_ERR_ISR && unlock_err == BR_ERR_ISR,
                 "Test 4: lock/unlock reject calls made from ISR context");
#else
    br_uart_puts("Test 4: SKIPPED (host-only)\n");
#endif
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Mutex Test ===\n\n");

    run_basic_test();
    run_timeout_test();
    run_priority_inheritance_test();
    run_isr_rejection_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Mutex Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

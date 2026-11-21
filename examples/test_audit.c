/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "test_stack.h"
#include "br_shell.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static uint8_t stack_a[TEST_STACK(1024)];
static uint8_t stack_b[TEST_STACK(1024)];
static uint8_t stack_c[TEST_STACK(1024)];
static uint8_t stack_supervisor[TEST_STACK(1536)];

static br_sem_t       sem;
static volatile int   sem_result = 99;

static void sleeper_task(void *arg)
{
    (void)arg;
    br_sleep_us(BR_TIME_INFINITE);
    br_task_suspend(br_task_self());
}

static void waiter_task(void *arg)
{
    (void)arg;
    sem_result = br_sem_take(&sem, BR_TIME_INFINITE - 1);
    br_task_suspend(br_task_self());
}

static void idle_entry(void *arg)
{
    (void)arg;
    br_task_suspend(br_task_self());
}

static void run_size_tests(void)
{
    br_uart_puts("Test 1: Size and range checks\n");

    uint32_t storage[4];
    br_mqueue_t mq;

    print_result(br_mqueue_init(&mq, storage, SIZE_MAX / 2, 4) == BR_ERR_INVALID,
                 "Test 1a: a message queue whose size overflows is rejected");
    print_result(br_mqueue_init(&mq, storage, 0, 4) == BR_ERR_INVALID &&
                 br_mqueue_init(&mq, storage, 4, 0) == BR_ERR_INVALID,
                 "Test 1b: empty queue dimensions are rejected");
    print_result(br_mqueue_init(&mq, storage, sizeof(uint32_t), 4) == BR_OK,
                 "Test 1c: a normal queue is accepted");

    br_tid_t tid;
    print_result(br_task_create(&tid, "tiny", idle_entry, NULL, 5, stack_c, 0) == BR_ERR_INVALID &&
                 br_task_create(&tid, "tiny", idle_entry, NULL, 5, stack_c, 64) == BR_ERR_INVALID &&
                 br_task_create(&tid, "tiny", idle_entry, NULL, 5, stack_c, BR_STACK_MIN - 1) == BR_ERR_INVALID,
                 "Test 1d: a stack that cannot hold the initial frame is rejected");

    char line[4];
    print_result(br_shell_readline(line, 0, 0) == BR_ERR_INVALID &&
                 br_shell_readline(NULL, 4, 0) == BR_ERR_INVALID,
                 "Test 1e: a line buffer of zero bytes is rejected");
}

static void run_time_tests(void)
{
    br_uart_puts("Test 2: Time arithmetic near the limit\n");

    br_tid_t tid;
    br_task_create(&tid, "sleeper", sleeper_task, NULL, 2, stack_a, sizeof(stack_a));
    br_sleep_ms(50);

    br_task_info_t info;
    br_task_get_info(tid, &info);
    print_result(info.state == BR_TASK_BLOCKED, "Test 2a: an endless sleep does not wrap around");
    br_task_delete(tid);

    br_sem_init(&sem, 0, 1);
    br_tid_t wid;
    br_task_create(&wid, "waiter", waiter_task, NULL, 2, stack_b, sizeof(stack_b));
    br_sleep_ms(50);

    br_task_get_info(wid, &info);
    bool still_waiting = info.state == BR_TASK_BLOCKED;
    br_sem_give(&sem);
    br_sleep_ms(20);
    print_result(still_waiting && sem_result == BR_OK,
                 "Test 2b: a very long timeout does not wrap around");
    br_task_delete(wid);
}

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Audit Test ===\n\n");

    run_size_tests();
    run_time_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Audit Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

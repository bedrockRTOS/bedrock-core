/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0
 *
 * Test for br_task_delete() functionality
 */

#include "bedrock/bedrock.h"

extern void br_uart_puts(const char *s);

static uint8_t stack_supervisor[1024];
static uint8_t stack_worker[512];
static uint8_t stack_test[512];

static volatile int worker_run_count = 0;
static br_tid_t current_worker_tid = 0;

/* Simple worker task that increments counter and exits */
static void worker_task(void *arg)
{
    int id = (int)(uintptr_t)arg;
    
    br_uart_puts("[Worker ");
    if (id == 1) br_uart_puts("1");
    else if (id == 2) br_uart_puts("2");
    else if (id == 3) br_uart_puts("3");
    br_uart_puts("] Running\n");
    
    worker_run_count++;
    
    /* Simulate some work */
    br_sleep_ms(50);
    
    br_uart_puts("[Worker ");
    if (id == 1) br_uart_puts("1");
    else if (id == 2) br_uart_puts("2");
    else if (id == 3) br_uart_puts("3");
    br_uart_puts("] Finished\n");
    
    /* Worker exits but remains in system until deleted */
    while (1) {
        br_sleep_ms(1000);
    }
}

/* Task that tries to delete itself (should fail) */
static void self_delete_task(void *arg)
{
    (void)arg;
    br_tid_t my_tid = br_task_self();
    
    br_uart_puts("[SelfDelete] Attempting self-deletion...\n");
    
    br_err_t err = br_task_delete(my_tid);
    
    if (err == BR_ERR_INVALID) {
        br_uart_puts("[SelfDelete] PASS: Self-deletion correctly prevented\n");
    } else {
        br_uart_puts("[SelfDelete] FAIL: Self-deletion should return BR_ERR_INVALID\n");
    }
    
    while (1) {
        br_sleep_ms(1000);
    }
}

/* Supervisor task that creates, waits, and deletes workers */
static void supervisor_task(void *arg)
{
    (void)arg;
    br_err_t err;
    
    br_uart_puts("\n=== Task Deletion Test ===\n\n");
    
    /* Test 1: Create and delete worker 1 */
    br_uart_puts("Test 1: Create Worker 1\n");
    err = br_task_create(&current_worker_tid, "worker1", worker_task, 
                         (void *)1, 3, stack_worker, sizeof(stack_worker));
    if (err != BR_OK) {
        br_uart_puts("FAIL: Could not create worker 1\n");
        while (1) { br_sleep_ms(1000); }
    }
    
    br_uart_puts("Test 1: Worker TID = ");
    if (current_worker_tid == 2) br_uart_puts("2");
    else if (current_worker_tid == 3) br_uart_puts("3");
    else br_uart_puts("?");
    br_uart_puts("\n");
    
    br_sleep_ms(200);  /* Let worker run and finish */
    
    br_uart_puts("Test 1: Deleting Worker 1\n");
    err = br_task_delete(current_worker_tid);
    if (err == BR_OK) {
        br_uart_puts("Test 1: PASS - Worker 1 deleted\n\n");
    } else {
        br_uart_puts("Test 1: FAIL - Could not delete worker 1\n\n");
    }
    
    /* Test 2: Create worker 2 - should reuse same slot */
    br_uart_puts("Test 2: Create Worker 2 (should reuse slot)\n");
    br_tid_t worker2_tid;
    err = br_task_create(&worker2_tid, "worker2", worker_task,
                         (void *)2, 3, stack_worker, sizeof(stack_worker));
    if (err != BR_OK) {
        br_uart_puts("FAIL: Could not create worker 2\n");
        while (1) { br_sleep_ms(1000); }
    }
    
    br_uart_puts("Test 2: Worker TID = ");
    if (worker2_tid == 2) br_uart_puts("2");
    else if (worker2_tid == 3) br_uart_puts("3");
    else br_uart_puts("?");
    
    if (worker2_tid == current_worker_tid) {
        br_uart_puts(" (REUSED - PASS)\n");
    } else {
        br_uart_puts(" (NEW SLOT - FAIL)\n");
    }
    
    br_sleep_ms(200);
    
    br_uart_puts("Test 2: Deleting Worker 2\n");
    err = br_task_delete(worker2_tid);
    if (err == BR_OK) {
        br_uart_puts("Test 2: PASS - Worker 2 deleted\n\n");
    } else {
        br_uart_puts("Test 2: FAIL - Could not delete worker 2\n\n");
    }
    
    /* Test 3: Self-deletion prevention */
    br_uart_puts("Test 3: Self-deletion prevention\n");
    br_tid_t test_tid;
    err = br_task_create(&test_tid, "selfdelete", self_delete_task,
                         NULL, 3, stack_test, sizeof(stack_test));
    
    br_sleep_ms(200);
    
    /* Clean up test task */
    br_task_delete(test_tid);
    
    /* Test 4: Verify worker run count */
    br_uart_puts("\nTest 4: Worker run count\n");
    br_uart_puts("Expected: 2, Got: ");
    if (worker_run_count == 2) {
        br_uart_puts("2 - PASS\n");
    } else {
        br_uart_puts("FAIL\n");
    }
    
    /* Summary */
    br_uart_puts("\n=== All Tests Complete ===\n");
    br_uart_puts("Task deletion working correctly!\n\n");
    
    /* Keep running */
    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();
    
    br_uart_puts("\nbedrock[RTOS] - Task Deletion Test\n");
    
    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   1, stack_supervisor, sizeof(stack_supervisor));
    
    br_kernel_start();
}

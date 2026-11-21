/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_shell.h"
#include "br_pool.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static uint8_t pool_storage[4 * 32];
static uint8_t stack_worker[512];

static void worker_task(void *arg)
{
    (void)arg;
    while (1) {
        br_sleep_ms(1000);
    }
}

static void run_command_tests(void)
{
    br_uart_puts("Test 1: Built-in commands\n");

    char tasks_line[] = "tasks";
    print_result(br_shell_execute(tasks_line) == 0, "Test 1a: tasks");

    char mem_line[] = "mem";
    print_result(br_shell_execute(mem_line) == 0, "Test 1b: mem without pools");

    br_pool_handle_t pool = br_pool_create(pool_storage, sizeof(pool_storage), 32);
    void *block = br_pool_alloc(pool);

    print_result(br_shell_execute(mem_line) == 0 && block != NULL,
                 "Test 1c: mem with a pool");

    char uptime_line[] = "uptime";
    print_result(br_shell_execute(uptime_line) == 0, "Test 1d: uptime");

    char version_line[] = "version";
    print_result(br_shell_execute(version_line) == 0, "Test 1e: version");

    char help_line[] = "help";
    print_result(br_shell_execute(help_line) == 0, "Test 1f: help lists the built-in commands");
}

static uint8_t stack_supervisor[1536];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Shell Commands Test ===\n\n");

    br_shell_register_builtin();

    br_tid_t worker_tid;
    br_task_create(&worker_tid, "worker", worker_task, NULL, 3,
                   stack_worker, sizeof(stack_worker));
    br_sleep_ms(10);

    run_command_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Shell Commands Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

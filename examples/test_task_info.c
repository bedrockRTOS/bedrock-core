/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "test_stack.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static bool str_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static volatile uint32_t idle_calls;

void br_hook_idle(void)
{
    idle_calls++;
}

/* Bytes of the probe stack the test writes: low in the stack, then higher */
#define PROBE_LOW   (sizeof(stack_probe) * 3 / 5)
#define PROBE_HIGH  (sizeof(stack_probe) / 5)

static uint8_t stack_probe[TEST_STACK(512)];
static uint8_t stack_busy[TEST_STACK(512)];

static void probe_task(void *arg)
{
    (void)arg;
    br_task_suspend(br_task_self());
}

static void busy_task(void *arg)
{
    (void)arg;

    br_time_t end = br_uptime_us() + BR_MSEC(50);
    while (br_uptime_us() < end) { }

    br_task_suspend(br_task_self());
}

static void run_info_tests(void)
{
    br_task_info_t info;

    br_uart_puts("Test 1: br_task_get_info\n");

    br_tid_t probe_tid;
    br_task_create(&probe_tid, "probe", probe_task, NULL, 5,
                   stack_probe, sizeof(stack_probe));

    br_err_t err = br_task_get_info(probe_tid, &info);
    print_result(err == BR_OK &&
                 str_equal(info.name, "probe") &&
                 info.priority == 5 &&
                 info.state == BR_TASK_READY &&
                 info.stack_size == sizeof(stack_probe),
                 "Test 1a: name, priority, state and stack size");

    print_result(br_task_get_info(CONFIG_MAX_TASKS, &info) == BR_ERR_INVALID,
                 "Test 1b: tid out of range is rejected");

    print_result(br_task_get_info(probe_tid, NULL) == BR_ERR_INVALID,
                 "Test 1c: NULL info is rejected");

    br_task_delete(probe_tid);
    print_result(br_task_get_info(probe_tid, &info) == BR_ERR_INVALID,
                 "Test 1d: deleted task is rejected");
}

static void run_stack_tests(void)
{
    br_task_info_t info;

    br_uart_puts("Test 2: Stack high-water mark\n");

    br_tid_t probe_tid;
    br_task_create(&probe_tid, "probe", probe_task, NULL, 5,
                   stack_probe, sizeof(stack_probe));

    br_task_get_info(probe_tid, &info);
    print_result(info.stack_used > 0 && info.stack_used < sizeof(stack_probe) / 2,
                 "Test 2a: fresh stack is mostly unused");

    stack_probe[PROBE_LOW] = 0;
    br_task_get_info(probe_tid, &info);
    print_result(info.stack_used == sizeof(stack_probe) - (PROBE_LOW - sizeof(uint32_t)),
                 "Test 2b: mark follows the lowest written byte");

    stack_probe[PROBE_HIGH] = 0;
    br_task_get_info(probe_tid, &info);
    print_result(info.stack_used == sizeof(stack_probe) - (PROBE_HIGH - sizeof(uint32_t)),
                 "Test 2c: mark moves down as the stack grows");

    br_task_delete(probe_tid);
}

static void run_stats_tests(void)
{
    br_uart_puts("Test 3: CPU time and idle hook\n");

    uint32_t idle_before = idle_calls;

    br_tid_t busy_tid;
    br_task_create(&busy_tid, "busy", busy_task, NULL, 2,
                   stack_busy, sizeof(stack_busy));

    br_sleep_ms(150);

    br_task_info_t info;
    br_err_t err = br_task_get_info(busy_tid, &info);
    print_result(err == BR_OK && info.state == BR_TASK_SUSPENDED,
                 "Test 3a: busy task finished");

#if CONFIG_TASK_STATS
    print_result(info.cpu_time_us >= BR_MSEC(30) && info.cpu_time_us <= br_uptime_us(),
                 "Test 3b: busy task CPU time covers its busy loop");

    br_task_info_t sup;
    br_task_get_info(br_task_self(), &sup);
    print_result(sup.cpu_time_us < info.cpu_time_us,
                 "Test 3c: sleeping task accumulates less CPU time than a busy one");

    bool idle_ran = false;
    for (int tid = 0; tid < CONFIG_MAX_TASKS; tid++) {
        br_task_info_t t;
        if (br_task_get_info((br_tid_t)tid, &t) == BR_OK && str_equal(t.name, "idle")) {
            idle_ran = t.cpu_time_us > 0;
        }
    }
    print_result(idle_ran, "Test 3d: idle task is accounted");
#else
    print_result(info.cpu_time_us == 0, "Test 3b: CPU time is 0 without CONFIG_TASK_STATS");
#endif

    print_result(idle_calls > idle_before, "Test 3e: br_hook_idle runs in the idle task");

    br_task_delete(busy_tid);
}

static uint8_t stack_supervisor[TEST_STACK(1024)];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Task Info Test ===\n\n");

    run_info_tests();
    run_stack_tests();
    run_stats_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Task Info Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

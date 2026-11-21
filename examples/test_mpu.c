/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

#if !CONFIG_MPU
#error "test_mpu needs CONFIG_MPU"
#endif

#ifndef TEST_UART_BASE
#define TEST_UART_BASE  0x4000C000UL
#endif
#ifndef TEST_CODE_ADDR
#define TEST_CODE_ADDR  0x100UL
#endif
#ifndef TEST_STACK_SIZE
#define TEST_STACK_SIZE  1024
#endif
#ifndef TEST_GRANT_BASE
#define TEST_GRANT_BASE  0x40010000UL
#endif
#define UART_BASE  TEST_UART_BASE

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

BR_USER_BSS static volatile uint32_t shared_progress;
BR_USER_BSS static volatile uint32_t shared_value;
BR_USER_BSS static volatile int32_t  shared_results[16];
BR_USER_BSS static int               sem_h;
BR_USER_BSS static int               mutex_h;
BR_USER_BSS static int               mq_h;

static volatile uint32_t kernel_secret = 0x5EC0DE;

BR_USER_STACK(stack_a, TEST_STACK_SIZE);
BR_USER_STACK(stack_b, TEST_STACK_SIZE);
BR_USER_STACK(stack_fault, TEST_STACK_SIZE);
static uint8_t stack_supervisor[2048];

static void wait_stopped(br_tid_t tid, br_time_t limit)
{
    br_time_t start = br_uptime_us();
    br_task_info_t info;

    while (br_uptime_us() - start < limit) {
        if (br_task_get_info(tid, &info) == BR_OK && info.state == BR_TASK_SUSPENDED) {
            return;
        }
        br_sleep_ms(5);
    }
}

static void user_a(void *arg)
{
    (void)arg;
    int i = 0;

    shared_results[i++] = br_user_self();

    sem_h   = br_user_sem_create(0, 4);
    mutex_h = br_user_mutex_create();
    mq_h    = br_user_mqueue_create(sizeof(uint32_t), 4);
    shared_results[i++] = (sem_h >= 0 && mutex_h >= 0 && mq_h >= 0) ? 1 : 0;

    shared_results[i++] = br_user_sem_take(sem_h, BR_MSEC(20));
    shared_results[i++] = br_user_mutex_lock(mutex_h, 0);
    shared_results[i++] = br_user_mutex_unlock(mutex_h);
    shared_results[i++] = br_user_sem_take(99, 0);

    uint32_t msg = 0xA5A5;
    shared_results[i++] = br_user_mqueue_send(mq_h, &msg, 0);
    uint32_t got = 0;
    shared_results[i++] = br_user_mqueue_recv(mq_h, &got, 0);
    shared_results[i++] = (got == 0xA5A5) ? 1 : 0;

    br_time_t t0 = br_user_uptime_us();
    br_user_sleep_us(BR_MSEC(10));
    br_time_t t1 = br_user_uptime_us();
    shared_results[i++] = (t1 - t0 >= BR_MSEC(9)) ? 1 : 0;

    shared_progress = 1;
    br_user_yield();

    shared_results[i++] = br_user_sem_take(sem_h, BR_SEC(2));
    uint32_t reply = 0;
    shared_results[i++] = br_user_mqueue_recv(mq_h, &reply, BR_SEC(2));
    shared_results[i++] = (reply == 0xB00B) ? 1 : 0;

    br_user_uart_puts("  user task prints through a system call\n");
    shared_progress = 2;
    br_user_exit();
}

static void user_b(void *arg)
{
    (void)arg;

    while (shared_progress < 1) {
        br_user_sleep_us(BR_MSEC(2));
    }

    uint32_t reply = 0xB00B;
    br_user_mqueue_send(mq_h, &reply, BR_SEC(1));
    br_user_sem_give(sem_h);

    br_user_exit();
}

BR_USER_BSS static volatile uint32_t *fault_target;
BR_USER_BSS static uintptr_t          fault_aux;

static void recurse(volatile uint32_t depth)
{
    volatile uint8_t pad[96];
    pad[0] = (uint8_t)depth;
    if (depth != 0xFFFFFFFFU) {
        recurse(depth + 1);
    }
    shared_value = pad[0];
}

static void fault_task(void *arg)
{
    uint32_t kind = (uint32_t)(uintptr_t)arg;

    switch (kind) {
    case 0:
        *fault_target = 0xBAD;
        break;
    case 1:
        shared_value = *fault_target;
        break;
    case 2:
        *(volatile uint8_t *)UART_BASE = 'X';
        break;
    case 3:
        *(volatile uint32_t *)fault_aux = 0;
        break;
    case 4:
        *(volatile uint8_t *)fault_aux = 0;
        break;
    case 5:
        recurse(0);
        break;
    case 6: {
        void (*fn)(void) = (void (*)(void))(fault_aux | 1UL);
        fn();
        break;
    }
    case 7:
        *(volatile uint8_t *)UART_BASE = 'G';
        shared_value = 77;
        break;
    default:
        break;
    }

    shared_progress = 100 + kind;
    br_user_exit();
}

static void run_syscall_tests(void)
{
    br_uart_puts("Test 1: System calls from unprivileged tasks\n");

    br_tid_t ta;
    br_tid_t tb;
    print_result(br_task_create_user(&ta, "user_a", user_a, NULL, 2, stack_a, sizeof(stack_a)) == BR_OK &&
                 br_task_create_user(&tb, "user_b", user_b, NULL, 3, stack_b, sizeof(stack_b)) == BR_OK,
                 "Test 1a: create unprivileged tasks");

    br_task_info_t info;
    br_task_get_info(ta, &info);
    print_result(info.user, "Test 1b: the task is marked unprivileged");

    wait_stopped(ta, BR_SEC(5));

    print_result(shared_progress == 2, "Test 1c: both tasks ran to the end");
    print_result(shared_results[0] == (int32_t)ta && shared_results[1] == 1,
                 "Test 1d: self id and object creation");
    print_result(shared_results[2] == BR_ERR_TIMEOUT, "Test 1e: semaphore take times out");
    print_result(shared_results[3] == BR_OK && shared_results[4] == BR_OK,
                 "Test 1f: mutex lock and unlock");
    print_result(shared_results[5] == BR_ERR_INVALID, "Test 1g: a bad handle is rejected");
    print_result(shared_results[6] == BR_OK && shared_results[7] == BR_OK && shared_results[8] == 1,
                 "Test 1h: message queue round trip");
    print_result(shared_results[9] == 1, "Test 1i: sleep and uptime");
    print_result(shared_results[10] == BR_OK && shared_results[11] == BR_OK && shared_results[12] == 1,
                 "Test 1j: blocking calls between two tasks");

    br_task_delete(ta);
    br_task_delete(tb);
}

static bool run_fault(uint32_t kind, volatile uint32_t *target, uintptr_t aux, br_task_info_t *info)
{
    fault_target    = target;
    fault_aux       = aux;
    shared_progress = 0;
    shared_value    = 0;

    br_tid_t tid;
    if (br_task_create_user(&tid, "faulty", fault_task, (void *)(uintptr_t)kind, 2,
                            stack_fault, sizeof(stack_fault)) != BR_OK) {
        return false;
    }
    wait_stopped(tid, BR_SEC(2));
    br_task_get_info(tid, info);
    br_task_delete(tid);
    return true;
}

static void run_violation_tests(void)
{
    br_uart_puts("Test 2: Memory violations\n");

    br_task_info_t info;
    uint32_t before = kernel_secret;

    bool ok = run_fault(0, &kernel_secret, 0, &info);
    print_result(ok && info.faulted && kernel_secret == before && shared_progress < 100,
                 "Test 2a: a write to kernel data is stopped");

    ok = run_fault(1, &kernel_secret, 0, &info);
    print_result(ok && info.faulted && shared_progress < 100,
                 "Test 2b: a read of kernel data is stopped");

    ok = run_fault(2, NULL, 0, &info);
    print_result(ok && info.faulted, "Test 2c: a peripheral access without a grant is stopped");

    ok = run_fault(3, NULL, TEST_CODE_ADDR, &info);
    print_result(ok && info.faulted, "Test 2d: a write to flash is stopped");

    ok = run_fault(4, NULL, (uintptr_t)&stack_b[16], &info);
    print_result(ok && info.faulted, "Test 2e: a write to another task's stack is stopped");

    ok = run_fault(5, NULL, 0, &info);
    print_result(ok && info.faulted && shared_progress < 100,
                 "Test 2f: a stack overflow is stopped");

    ok = run_fault(6, NULL, (uintptr_t)&shared_value, &info);
    print_result(ok && info.faulted, "Test 2g: running code from data is stopped");

    char hex[9];
    for (int i = 0; i < 8; i++) {
        hex[i] = "0123456789abcdef"[(kernel_secret >> (28 - 4 * i)) & 0xF];
    }
    hex[8] = '\0';
    br_uart_puts("secret now ");
    br_uart_puts(hex);
    br_uart_puts("\n");
    print_result(kernel_secret == before, "Test 2h: kernel data is intact after all violations");
}

static void run_grant_tests(void)
{
    br_uart_puts("Test 3: Granted regions\n");

    br_tid_t tid;
    br_task_create_user(&tid, "granted", fault_task, (void *)7, 2, stack_fault, sizeof(stack_fault));

    print_result(br_task_grant(tid, (void *)UART_BASE, 4096, BR_MEM_R | BR_MEM_W) == BR_OK,
                 "Test 3a: grant a peripheral");
    print_result(br_task_grant(tid, (void *)(UART_BASE + 8), 4096, BR_MEM_R) == BR_ERR_INVALID,
                 "Test 3b: a misaligned region is rejected");
    print_result(br_task_grant(tid, (void *)UART_BASE, 100, BR_MEM_R) == BR_ERR_INVALID,
                 "Test 3c: a size that is not a power of two is rejected");
    print_result(br_task_grant(tid, (void *)TEST_GRANT_BASE, 4096, 0) == BR_ERR_INVALID,
                 "Test 3d: a region without rights is rejected");

    br_task_info_t info;
    shared_progress = 0;
    shared_value    = 0;
    wait_stopped(tid, BR_SEC(2));
    br_task_get_info(tid, &info);
    print_result(!info.faulted && shared_value == 77, "Test 3e: the granted peripheral can be used");
    br_task_delete(tid);

    br_tid_t t2;
    br_task_create_user(&t2, "grants", user_b, NULL, 2, stack_fault, sizeof(stack_fault));
    br_task_grant(t2, (void *)(TEST_GRANT_BASE), 4096, BR_MEM_R);
    br_task_grant(t2, (void *)(TEST_GRANT_BASE + 0x1000), 4096, BR_MEM_R);
    br_task_grant(t2, (void *)(TEST_GRANT_BASE + 0x2000), 4096, BR_MEM_R);
    print_result(br_task_grant(t2, (void *)(TEST_GRANT_BASE + 0x3000), 4096, BR_MEM_R) == BR_ERR_NOMEM,
                 "Test 3f: the number of regions is limited");
    br_task_suspend(t2);
    br_task_delete(t2);
}

static void run_create_tests(void)
{
    br_uart_puts("Test 4: Creating tasks\n");

    static uint8_t odd_stack[1000] __attribute__((aligned(8)));
    br_tid_t tid;
    print_result(br_task_create_user(&tid, "odd", user_b, NULL, 2, odd_stack, sizeof(odd_stack)) == BR_ERR_INVALID,
                 "Test 4a: a stack that is not a power of two is rejected");
    print_result(br_task_create_user(&tid, "misaligned", user_b, NULL, 2, &stack_a[TEST_STACK_SIZE / 2], TEST_STACK_SIZE) == BR_ERR_INVALID,
                 "Test 4b: a stack that is not aligned to its size is rejected");

    br_tid_t kt;
    print_result(br_task_create(&kt, "kernel", user_b, NULL, 6, stack_supervisor, 0) == BR_ERR_INVALID,
                 "Test 4c: privileged tasks are created as before");
}

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== MPU Test ===\n\n");

    run_syscall_tests();
    run_violation_tests();
    run_grant_tests();
    run_create_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - MPU Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

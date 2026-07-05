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

/* Test 1: basic send/recv, FIFO order */

static void run_basic_test(void)
{
    br_uart_puts("Test 1: Basic send/recv\n");

    uint32_t buffer[2];
    br_mqueue_t mq;
    br_mqueue_init(&mq, buffer, sizeof(uint32_t), 2);

    uint32_t v1 = 11, v2 = 22;
    br_err_t s1 = br_mqueue_send(&mq, &v1, 0);
    br_err_t s2 = br_mqueue_send(&mq, &v2, 0);

    uint32_t r1 = 0, r2 = 0;
    br_err_t g1 = br_mqueue_recv(&mq, &r1, 0);
    br_err_t g2 = br_mqueue_recv(&mq, &r2, 0);

    print_result(s1 == BR_OK && s2 == BR_OK &&
                 g1 == BR_OK && g2 == BR_OK &&
                 r1 == 11 && r2 == 22,
                 "Test 1: messages come back out in FIFO order");
}

/* Test 2: full queue blocks a sender until a receiver makes room */

static uint32_t         full_buf[1];
static br_mqueue_t       full_mq;
static uint8_t           stack_full_sender[512];
static volatile br_err_t full_sender_result = BR_ERR_INVALID;
static volatile bool     full_sender_done;

static void full_sender_task(void *arg)
{
    (void)arg;
    uint32_t v = 20;
    full_sender_result = br_mqueue_send(&full_mq, &v, BR_TIME_INFINITE);
    full_sender_done = true;
    br_task_suspend(br_task_self());
}

static void run_full_blocking_test(void)
{
    br_uart_puts("Test 2a: Full queue blocks sender\n");

    br_mqueue_init(&full_mq, full_buf, sizeof(uint32_t), 1);
    full_sender_done = false;
    full_sender_result = BR_ERR_INVALID;

    uint32_t first = 10;
    br_mqueue_send(&full_mq, &first, 0);

    br_tid_t tid;
    br_task_create(&tid, "full_sender", full_sender_task, NULL, 3,
                   stack_full_sender, sizeof(stack_full_sender));

    br_sleep_ms(10);
    bool blocked_while_full = !full_sender_done;

    uint32_t drained = 0;
    br_mqueue_recv(&full_mq, &drained, 0);

    br_sleep_ms(10);

    uint32_t second = 0;
    br_err_t got_second = br_mqueue_recv(&full_mq, &second, 0);

    print_result(blocked_while_full && drained == 10 &&
                 full_sender_done && full_sender_result == BR_OK &&
                 got_second == BR_OK && second == 20,
                 "Test 2a: send unblocks once a slot frees up");
}

/* Test 2b: empty queue blocks a receiver until a sender delivers */

static uint32_t          empty_buf[1];
static br_mqueue_t        empty_mq;
static uint8_t            stack_empty_receiver[512];
static volatile br_err_t  empty_receiver_result = BR_ERR_INVALID;
static volatile uint32_t  empty_receiver_value;
static volatile bool      empty_receiver_done;

static void empty_receiver_task(void *arg)
{
    (void)arg;
    uint32_t v = 0;
    empty_receiver_result = br_mqueue_recv(&empty_mq, &v, BR_TIME_INFINITE);
    empty_receiver_value = v;
    empty_receiver_done = true;
    br_task_suspend(br_task_self());
}

static void run_empty_blocking_test(void)
{
    br_uart_puts("Test 2b: Empty queue blocks receiver\n");

    br_mqueue_init(&empty_mq, empty_buf, sizeof(uint32_t), 1);
    empty_receiver_done = false;
    empty_receiver_result = BR_ERR_INVALID;

    br_tid_t tid;
    br_task_create(&tid, "empty_receiver", empty_receiver_task, NULL, 3,
                   stack_empty_receiver, sizeof(stack_empty_receiver));

    br_sleep_ms(10);
    bool blocked_while_empty = !empty_receiver_done;

    uint32_t v = 99;
    br_mqueue_send(&empty_mq, &v, 0);

    br_sleep_ms(10);

    print_result(blocked_while_empty && empty_receiver_done &&
                 empty_receiver_result == BR_OK &&
                 empty_receiver_value == 99,
                 "Test 2b: recv unblocks once a message arrives");
}

/* Test 3: timeout */

static void run_timeout_test(void)
{
    br_uart_puts("Test 3: Timeout\n");

    uint32_t buffer[1];
    br_mqueue_t mq;
    br_mqueue_init(&mq, buffer, sizeof(uint32_t), 1);

    uint32_t v = 0;
    br_time_t start = br_uptime_us();
    br_err_t err = br_mqueue_recv(&mq, &v, BR_MSEC(30));
    br_time_t elapsed = br_uptime_us() - start;

    br_uart_puts("  elapsed us = ");
    uart_put_uint((uint32_t)elapsed);
    br_uart_puts("\n");

    print_result(err == BR_ERR_TIMEOUT && elapsed >= BR_MSEC(30),
                 "Test 3: recv on an empty queue times out");
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Message Queue Test ===\n\n");

    run_basic_test();
    run_full_blocking_test();
    run_empty_blocking_test();
    run_timeout_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Message Queue Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

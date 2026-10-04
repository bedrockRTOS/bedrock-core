/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

#define EXPECTED_LINE "bedrock"

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

static void run_line_test(void)
{
    br_uart_puts("Test 1: Receive a line\n");

    char line[16];
    size_t len = 0;
    br_err_t err = BR_OK;

    while (len < sizeof(line) - 1) {
        char c;
        err = br_uart_getc(&c, BR_SEC(2));
        if (err != BR_OK || c == '\n' || c == '\r') {
            break;
        }
        line[len++] = c;
    }
    line[len] = '\0';

    print_result(err == BR_OK && str_equal(line, EXPECTED_LINE),
                 "Test 1: received line matches \"" EXPECTED_LINE "\"");
}

static void run_timeout_test(void)
{
    br_uart_puts("Test 2: Timeout on empty buffer\n");

    char c;
    br_err_t err;
    br_time_t elapsed;

    do {
        br_time_t start = br_uptime_us();
        err = br_uart_getc(&c, BR_MSEC(50));
        elapsed = br_uptime_us() - start;
    } while (err == BR_OK && (c == '\n' || c == '\r'));

    print_result(err == BR_ERR_TIMEOUT && elapsed >= BR_MSEC(50),
                 "Test 2: getc times out when no data arrives");
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== UART RX Test ===\n\n");

    run_line_test();
    run_timeout_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - UART RX Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_pwm.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static br_pwm_dev_t     pwm;
static br_capture_dev_t capture;

static void run_pwm_tests(void)
{
    br_uart_puts("Test 1: PWM device\n");

    br_pwm_register(&pwm, "pwm0", 0);
    br_device_t *dev = br_device_find("pwm0");
    br_device_open(dev);

    br_err_t err = br_pwm_set(dev, 1000, 250);
    if (err == BR_ERR_INVALID) {
        br_uart_puts("Test 1: SKIP, no PWM on this target\n");
        br_device_close(dev);
        return;
    }
    print_result(err == BR_OK, "Test 1a: start 1 kHz at 25 percent");

    print_result(br_pwm_set(dev, 2000, 1000) == BR_OK, "Test 1b: 100 percent duty");
    print_result(br_pwm_set(dev, 2000, 0) == BR_OK, "Test 1c: 0 percent duty");
    print_result(br_pwm_set(dev, 1000, 1001) == BR_ERR_INVALID, "Test 1d: duty above 100 percent is rejected");
    print_result(br_pwm_set(dev, 0, 500) == BR_ERR_INVALID, "Test 1e: zero frequency is rejected");

    br_pwm_set(dev, 1000, 500);
    print_result(br_pwm_stop(dev) == BR_OK, "Test 1f: stop");
}

static void run_capture_tests(void)
{
    br_uart_puts("Test 2: Capture device\n");

    br_capture_register(&capture, "cap0", 0);
    br_device_t *cap = br_device_find("cap0");

    if (br_device_open(cap) != BR_OK) {
        br_uart_puts("Test 2: SKIP, no capture on this target\n");
        return;
    }

    br_device_t *dev = br_device_find("pwm0");
    br_pwm_set(dev, 1000, 500);

    uint32_t period = 0;
    br_err_t err = br_capture_read_period(cap, &period, BR_MSEC(50));
    if (err == BR_ERR_TIMEOUT) {
        br_uart_puts("Test 2a: SKIP, capture input is not wired to PWM\n");
    } else {
        print_result(err == BR_OK && period > 950 && period < 1050,
                     "Test 2a: captured period matches the 1 kHz PWM");
    }

    br_pwm_stop(dev);

    print_result(br_capture_set_edge(cap, BR_CAPTURE_FALLING) == BR_OK,
                 "Test 2b: select falling edge");

    print_result(br_capture_set_edge(cap, (br_capture_edge_t)5) == BR_ERR_INVALID,
                 "Test 2c: unknown edge is rejected");

    print_result(br_device_read(cap, &period, 2, 0) == BR_ERR_INVALID,
                 "Test 2d: short read buffer is rejected");

    br_device_close(cap);
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== PWM Test ===\n\n");

    run_pwm_tests();
    run_capture_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - PWM Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

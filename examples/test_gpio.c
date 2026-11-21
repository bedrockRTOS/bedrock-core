/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "test_stack.h"
#include "br_gpio.h"

#define TEST_PORT  0
/* PB5 is the LED of an Arduino Uno. PB6 and PB7 are the crystal pins. */
#ifdef __AVR__
#  define TEST_PIN   5
#else
#  define TEST_PIN   6
#endif

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static br_gpio_dev_t gpio;

static void run_gpio_tests(void)
{
    br_uart_puts("Test 1: GPIO device\n");

    br_gpio_register(&gpio, "gpio0", TEST_PORT);
    br_device_t *dev = br_device_find("gpio0");
    br_device_open(dev);

    br_err_t err = br_gpio_configure(dev, TEST_PIN, BR_GPIO_OUTPUT);
    if (err == BR_ERR_INVALID || err == BR_ERR_IO) {
        br_uart_puts("Test 1: SKIP, no GPIO on this target\n");
        return;
    }
    print_result(err == BR_OK, "Test 1a: configure as output");

    bool level = false;
    br_gpio_write(dev, TEST_PIN, true);
    err = br_gpio_read(dev, TEST_PIN, &level);
    print_result(err == BR_OK && level, "Test 1b: output high reads back high");

    br_gpio_write(dev, TEST_PIN, false);
    err = br_gpio_read(dev, TEST_PIN, &level);
    print_result(err == BR_OK && !level, "Test 1c: output low reads back low");

    br_gpio_toggle(dev, TEST_PIN);
    br_gpio_read(dev, TEST_PIN, &level);
    print_result(level, "Test 1d: toggle from low gives high");

    br_gpio_toggle(dev, TEST_PIN);
    br_gpio_read(dev, TEST_PIN, &level);
    print_result(!level, "Test 1e: toggle from high gives low");

    print_result(br_gpio_configure(dev, TEST_PIN, BR_GPIO_INPUT) == BR_OK,
                 "Test 1f: configure as input");

    print_result(br_gpio_configure(dev, 200, BR_GPIO_OUTPUT) == BR_ERR_INVALID,
                 "Test 1g: pin out of range is rejected");

    print_result(br_gpio_configure(dev, TEST_PIN, (br_gpio_mode_t)9) == BR_ERR_INVALID,
                 "Test 1h: unknown mode is rejected");

    br_device_close(dev);
}

static uint8_t stack_supervisor[TEST_STACK(1024)];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== GPIO Test ===\n\n");

    run_gpio_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - GPIO Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

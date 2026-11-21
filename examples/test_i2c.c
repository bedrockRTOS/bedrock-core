/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_i2c.h"

#define EEPROM_ADDR   0x50
#define ABSENT_ADDR   0x2A

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static br_i2c_dev_t i2c;

static void run_i2c_tests(void)
{
    br_uart_puts("Test 1: I2C device\n");

    br_i2c_register(&i2c, "i2c0", 0);
    br_device_t *dev = br_device_find("i2c0");

    if (br_device_open(dev) != BR_OK) {
        br_uart_puts("Test 1: SKIP, no I2C on this target\n");
        return;
    }

    print_result(br_i2c_set_addr(dev, 0x80) == BR_ERR_INVALID,
                 "Test 1a: 8-bit address is rejected");

    print_result(br_i2c_set_speed(dev, 400000) == BR_OK, "Test 1b: change speed");
    print_result(br_i2c_set_speed(dev, 0) == BR_ERR_INVALID, "Test 1c: zero speed is rejected");
    br_i2c_set_speed(dev, 100000);

    uint8_t probe = 0;
    br_i2c_set_addr(dev, ABSENT_ADDR);
    int n = br_device_write(dev, &probe, 1, BR_SEC(1));
    if (n == BR_ERR_TIMEOUT) {
        br_uart_puts("Test 1: SKIP, the I2C peripheral does not respond\n");
        br_device_close(dev);
        return;
    }
    print_result(n == BR_ERR_BUSY, "Test 1d: absent device is reported as NAK");

    br_i2c_set_addr(dev, EEPROM_ADDR);
    uint8_t out[4] = { 0x10, 0xAA, 0xBB, 0xCC };
    if (br_device_write(dev, out, sizeof(out), BR_SEC(1)) != (int)sizeof(out)) {
        br_uart_puts("Test 1e: SKIP, no device at the EEPROM address\n");
    } else {
        uint8_t in[3] = { 0 };
        uint8_t offset = 0x10;
        br_err_t err = br_i2c_write_read(dev, EEPROM_ADDR, &offset, 1, in, sizeof(in));
        print_result(err == BR_OK && in[0] == 0xAA && in[1] == 0xBB && in[2] == 0xCC,
                     "Test 1e: write then read with repeated start");

        offset = 0x11;
        br_device_write(dev, &offset, 1, BR_SEC(1));
        uint8_t one = 0;
        print_result(br_device_read(dev, &one, 1, BR_SEC(1)) == 1 && one == 0xBB,
                     "Test 1f: single byte read");
    }

    br_device_close(dev);
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== I2C Test ===\n\n");

    run_i2c_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - I2C Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

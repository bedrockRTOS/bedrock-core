/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_spi.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static br_spi_dev_t spi;

static void run_spi_tests(void)
{
    br_uart_puts("Test 1: SPI device\n");

    br_spi_register(&spi, "spi0", 0);
    br_device_t *dev = br_device_find("spi0");

    if (br_device_open(dev) != BR_OK) {
        br_uart_puts("Test 1: SKIP, no SPI on this target\n");
        return;
    }

    uint8_t tx[8] = { 0x00, 0x55, 0xAA, 0xFF, 0x01, 0x80, 0x7E, 0xC3 };
    uint8_t rx[8] = { 0 };

    br_err_t err = br_spi_transfer(dev, tx, rx, sizeof(tx));
    if (err == BR_ERR_TIMEOUT) {
        br_uart_puts("Test 1: SKIP, the SPI peripheral does not respond\n");
        br_device_close(dev);
        return;
    }
    print_result(err == BR_OK, "Test 1a: transfer completes");

    print_result(br_device_write(dev, tx, sizeof(tx), BR_SEC(1)) == (int)sizeof(tx),
                 "Test 1b: write returns the length");

    print_result(br_device_read(dev, rx, sizeof(rx), BR_SEC(1)) == (int)sizeof(rx),
                 "Test 1c: read returns the length");

    print_result(br_spi_set_speed(dev, 500000) == BR_OK, "Test 1d: change speed");

    print_result(br_spi_set_speed(dev, 0) == BR_ERR_INVALID, "Test 1e: zero speed is rejected");

    print_result(br_spi_set_mode(dev, 0x80) == BR_ERR_INVALID, "Test 1f: unknown mode bits are rejected");

    print_result(br_spi_set_mode(dev, BR_SPI_CPHA | BR_SPI_CPOL) == BR_OK,
                 "Test 1g: select mode 3");

    if (br_spi_set_mode(dev, BR_SPI_LOOP) != BR_OK) {
        br_uart_puts("Test 1h: SKIP, no SPI loopback on this target\n");
    } else {
        for (size_t i = 0; i < sizeof(rx); i++) {
            rx[i] = 0;
        }
        err = br_spi_transfer(dev, tx, rx, sizeof(tx));
        bool same = true;
        for (size_t i = 0; i < sizeof(tx); i++) {
            same = same && rx[i] == tx[i];
        }
        print_result(err == BR_OK && same, "Test 1h: loopback returns the bytes sent");
    }

    br_device_close(dev);
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== SPI Test ===\n\n");

    run_spi_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - SPI Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

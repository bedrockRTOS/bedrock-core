/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "test_stack.h"
#include "br_uart_dev.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static int  open_calls;
static int  close_calls;
static int  last_ioctl_cmd;
static char last_written[8];

static br_err_t fake_open(br_device_t *dev)
{
    (void)dev;
    open_calls++;
    return BR_OK;
}

static br_err_t fake_close(br_device_t *dev)
{
    (void)dev;
    close_calls++;
    return BR_OK;
}

static int fake_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    (void)dev;
    (void)timeout;
    uint8_t *p = buf;
    for (size_t i = 0; i < len; i++) {
        p[i] = (uint8_t)(0x40 + i);
    }
    return (int)len;
}

static int fake_write(br_device_t *dev, const void *buf, size_t len,
                      br_time_t timeout)
{
    (void)dev;
    (void)timeout;
    const char *p = buf;
    for (size_t i = 0; i < len && i < sizeof(last_written); i++) {
        last_written[i] = p[i];
    }
    return (int)len;
}

static br_err_t fake_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    (void)dev;
    last_ioctl_cmd = (int)cmd;
    if (arg != NULL) {
        *(int *)arg = 1234;
    }
    return BR_OK;
}

static const br_device_ops_t full_ops = {
    .open  = fake_open,
    .close = fake_close,
    .read  = fake_read,
    .write = fake_write,
    .ioctl = fake_ioctl,
};

static const br_device_ops_t empty_ops = { 0 };

static br_device_t full_dev;
static br_device_t empty_dev;
static br_device_t dup_dev;
static br_uart_dev_t uart_dev;

static void run_registry_tests(void)
{
    br_uart_puts("Test 1: Registry\n");

    print_result(br_device_register(&full_dev, "fake0", &full_ops, NULL) == BR_OK,
                 "Test 1a: register a device");

    print_result(br_device_register(&empty_dev, "fake1", &empty_ops, NULL) == BR_OK,
                 "Test 1b: register a second device");

    print_result(br_device_find("fake0") == &full_dev &&
                 br_device_find("fake1") == &empty_dev,
                 "Test 1c: find returns the registered devices");

    print_result(br_device_find("nosuchdev") == NULL, "Test 1d: unknown name returns NULL");

    print_result(br_device_register(&dup_dev, "fake0", &full_ops, NULL) == BR_ERR_BUSY,
                 "Test 1e: duplicate name is rejected");

    print_result(br_device_register(&full_dev, "fake2", &full_ops, NULL) == BR_ERR_BUSY,
                 "Test 1f: registering the same object twice is rejected");

    print_result(br_device_register(NULL, "x", &full_ops, NULL) == BR_ERR_INVALID &&
                 br_device_register(&dup_dev, NULL, &full_ops, NULL) == BR_ERR_INVALID &&
                 br_device_register(&dup_dev, "x", NULL, NULL) == BR_ERR_INVALID,
                 "Test 1g: NULL arguments are rejected");
}

static void run_open_tests(void)
{
    br_uart_puts("Test 2: Open and close\n");

    uint8_t b[2];
    print_result(br_device_read(&full_dev, b, sizeof(b), 0) == BR_ERR_INVALID,
                 "Test 2a: read before open is rejected");

    print_result(br_device_open(&full_dev) == BR_OK && open_calls == 1,
                 "Test 2b: first open calls the driver");

    print_result(br_device_open(&full_dev) == BR_OK && open_calls == 1,
                 "Test 2c: second open only counts");

    print_result(br_device_close(&full_dev) == BR_OK && close_calls == 0,
                 "Test 2d: close with another opener does not call the driver");

    print_result(br_device_close(&full_dev) == BR_OK && close_calls == 1,
                 "Test 2e: last close calls the driver");

    print_result(br_device_close(&full_dev) == BR_ERR_INVALID,
                 "Test 2f: close of a closed device is rejected");

    print_result(br_device_open(NULL) == BR_ERR_INVALID, "Test 2g: open of NULL is rejected");
}

static void run_io_tests(void)
{
    br_uart_puts("Test 3: Read, write and ioctl\n");

    br_device_open(&full_dev);

    uint8_t buf[3];
    int n = br_device_read(&full_dev, buf, sizeof(buf), 0);
    print_result(n == 3 && buf[0] == 0x40 && buf[2] == 0x42, "Test 3a: read reaches the driver");

    n = br_device_write(&full_dev, "abc", 3, 0);
    print_result(n == 3 && last_written[0] == 'a' && last_written[2] == 'c',
                 "Test 3b: write reaches the driver");

    int value = 0;
    print_result(br_device_ioctl(&full_dev, 0x77, &value) == BR_OK &&
                 last_ioctl_cmd == 0x77 && value == 1234,
                 "Test 3c: ioctl passes the command and argument");

    br_device_close(&full_dev);

    br_device_open(&empty_dev);
    print_result(br_device_read(&empty_dev, buf, sizeof(buf), 0) == BR_ERR_INVALID &&
                 br_device_write(&empty_dev, buf, sizeof(buf), 0) == BR_ERR_INVALID &&
                 br_device_ioctl(&empty_dev, 1, NULL) == BR_ERR_INVALID,
                 "Test 3d: missing operations are rejected");
    br_device_close(&empty_dev);
}

static void run_uart_tests(void)
{
    br_uart_puts("Test 4: UART device\n");

    print_result(br_uart_dev_register(&uart_dev, "uart0") == BR_OK &&
                 br_device_find("uart0") == &uart_dev.dev,
                 "Test 4a: register uart0");

    br_device_open(&uart_dev.dev);

    int n = br_device_write(&uart_dev.dev, "  uart0 write\n", 14, 0);
    print_result(n == 14, "Test 4b: write sends every byte");

    char c;
    while (br_device_read(&uart_dev.dev, &c, 1, BR_MSEC(100)) == 1) { }
    print_result(br_device_read(&uart_dev.dev, &c, 1, 0) == BR_ERR_TIMEOUT,
                 "Test 4c: read with no input times out");

    br_device_close(&uart_dev.dev);
}

static uint8_t stack_supervisor[TEST_STACK(1024)];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Device Test ===\n\n");

    run_registry_tests();
    run_open_tests();
    run_io_tests();
    run_uart_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Device Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

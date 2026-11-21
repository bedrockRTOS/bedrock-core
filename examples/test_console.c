/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_console.h"

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

static void run_snprintf_tests(void)
{
    char buf[64];
    int n;

    br_uart_puts("Test 1: br_snprintf formats\n");

    n = br_snprintf(buf, sizeof(buf), "%d %i", -42, 7);
    print_result(str_equal(buf, "-42 7") && n == 5, "Test 1a: signed decimal");

    n = br_snprintf(buf, sizeof(buf), "%u", 4000000000u);
    print_result(str_equal(buf, "4000000000") && n == 10, "Test 1b: unsigned decimal");

    br_snprintf(buf, sizeof(buf), "%x %X", 0xBEEFu, 0xBEEFu);
    print_result(str_equal(buf, "beef BEEF"), "Test 1c: hex lower and upper case");

    br_snprintf(buf, sizeof(buf), "%c%c", 'o', 'k');
    print_result(str_equal(buf, "ok"), "Test 1d: characters");

    const char *volatile null_str = NULL;
    br_snprintf(buf, sizeof(buf), "[%s] [%s]", "text", null_str);
    print_result(str_equal(buf, "[text] [(null)]"), "Test 1e: strings and NULL");

    br_snprintf(buf, sizeof(buf), "100%%");
    print_result(str_equal(buf, "100%"), "Test 1f: percent sign");

    br_snprintf(buf, sizeof(buf), "[%5d] [%-5d] [%05d] [%5s] [%-5s]",
                42, 42, 42, "ab", "ab");
    print_result(str_equal(buf, "[   42] [42   ] [00042] [   ab] [ab   ]"),
                 "Test 1g: width, left align, zero pad");

    br_snprintf(buf, sizeof(buf), "[%05d]", -42);
    print_result(str_equal(buf, "[-0042]"), "Test 1h: zero pad keeps the sign first");

    br_snprintf(buf, sizeof(buf), "%llu", (unsigned long long)BR_SEC(86400) * 365);
    print_result(str_equal(buf, "31536000000000"), "Test 1i: 64-bit unsigned");

    br_snprintf(buf, sizeof(buf), "%lld", -(long long)5000000000LL);
    print_result(str_equal(buf, "-5000000000"), "Test 1j: 64-bit signed");

    br_snprintf(buf, sizeof(buf), "%zu %lu", (size_t)1024, 70000ul);
    print_result(str_equal(buf, "1024 70000"), "Test 1k: size_t and long");

    br_snprintf(buf, sizeof(buf), "%p", (void *)(uintptr_t)0x1234);
    print_result(str_equal(buf, "0x1234"), "Test 1l: pointer");

    br_snprintf(buf, sizeof(buf), "%d", -2147483647 - 1);
    print_result(str_equal(buf, "-2147483648"), "Test 1m: INT_MIN");

    br_snprintf(buf, sizeof(buf), "%08x", 0xABCu);
    print_result(str_equal(buf, "00000abc"), "Test 1n: zero padded hex");
}

static void run_truncation_tests(void)
{
    char buf[8];
    int n;

    br_uart_puts("Test 2: br_snprintf truncation\n");

    n = br_snprintf(buf, 4, "abcdef");
    print_result(str_equal(buf, "abc") && n == 6, "Test 2a: output is cut and terminated, full length returned");

    n = br_snprintf(buf, 0, "abcdef");
    print_result(n == 6, "Test 2b: zero size only counts");

    n = br_snprintf(NULL, 0, "%d", 12345);
    print_result(n == 5, "Test 2c: NULL buffer only counts");

    n = br_snprintf(buf, sizeof(buf), "%s", "");
    print_result(str_equal(buf, "") && n == 0, "Test 2d: empty output");
}

static void run_printf_test(void)
{
    br_uart_puts("Test 3: br_printf\n");

    int n = br_printf("  printf: %s %d 0x%x\n", "line", 12, 255);
    print_result(n == 23, "Test 3: returns the number of characters written");
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Console Test ===\n\n");

    run_snprintf_tests();
    run_truncation_tests();
    run_printf_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Console Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

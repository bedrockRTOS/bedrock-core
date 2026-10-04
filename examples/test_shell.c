/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_shell.h"

static int  add_argc;
static int  bedrock_calls;

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static int parse_int(const char *s)
{
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s++ - '0');
    }
    return v;
}

static int add_handler(int argc, char **argv)
{
    add_argc = argc;
    int sum = 0;
    for (int i = 1; i < argc; i++) {
        sum += parse_int(argv[i]);
    }
    return sum;
}

static int bedrock_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    bedrock_calls++;
    return 0;
}

static br_shell_cmd_t add_cmd = {
    .name    = "add",
    .help    = "print the sum of the arguments",
    .handler = add_handler,
};

static br_shell_cmd_t bedrock_cmd = {
    .name    = "bedrock",
    .help    = "test command",
    .handler = bedrock_handler,
};

static void run_execute_tests(void)
{
    br_uart_puts("Test 1: Execute\n");

    char line1[] = "add 2 3";
    int r1 = br_shell_execute(line1);
    print_result(r1 == 5 && add_argc == 3, "Test 1a: command with arguments");

    char line2[] = "  add   10\t20  ";
    int r2 = br_shell_execute(line2);
    print_result(r2 == 30 && add_argc == 3, "Test 1b: extra whitespace is ignored");

    char line3[] = "nosuchcmd";
    print_result(br_shell_execute(line3) == -1, "Test 1c: unknown command returns -1");

    add_argc = 0;
    char line4[] = "   ";
    print_result(br_shell_execute(line4) == 0 && add_argc == 0, "Test 1d: empty line does nothing");

    char line5[] = "add 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1";
    print_result(br_shell_execute(line5) == -1, "Test 1e: too many arguments returns -1");

    char line6[] = "help";
    print_result(br_shell_execute(line6) == 0, "Test 1f: help lists commands");
}

static void run_readline_test(void)
{
    br_uart_puts("Test 2: Read and execute a line from UART\n");

    char line[CONFIG_SHELL_LINE_MAX];
    br_err_t err = br_shell_readline(line, sizeof(line), BR_SEC(2));
    if (err == BR_OK) {
        br_shell_execute(line);
    }

    print_result(err == BR_OK && bedrock_calls == 1, "Test 2: \"bedrock\" command ran");
}

static uint8_t stack_supervisor[1024];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Shell Test ===\n\n");

    br_shell_register(&add_cmd);
    br_shell_register(&bedrock_cmd);

    run_execute_tests();
    run_readline_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Shell Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_shell.h"
#include "br_fs.h"
#include "br_ramdisk.h"

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static uint8_t      disk[128 * 16];
static br_ramdisk_t ramdisk;

static void run_tests(void)
{
    br_uart_puts("Test 1: Filesystem commands\n");

    char ls_line[] = "ls";
    print_result(br_shell_execute(ls_line) == -1, "Test 1a: ls without a filesystem fails");

    br_ramdisk_init(&ramdisk, disk, 128, 16);
    br_fs_format(&ramdisk.dev);
    br_fs_mount(&ramdisk.dev);

    char write_line[] = "write motd hello from the shell";
    print_result(br_shell_execute(write_line) == 0, "Test 1b: write");

    size_t size = 0;
    print_result(br_fs_stat("motd", &size) == BR_OK && size == 20, "Test 1c: the file has the joined arguments");

    int fd = br_fs_open("motd", BR_FS_READ);
    char buf[24] = { 0 };
    br_fs_read(fd, buf, sizeof(buf) - 1);
    br_fs_close(fd);
    const char *want = "hello from the shell";
    bool same = true;
    for (int i = 0; i < 20; i++) {
        same = same && buf[i] == want[i];
    }
    print_result(same, "Test 1d: the content is right");

    char cat_line[] = "cat motd";
    print_result(br_shell_execute(cat_line) == 0, "Test 1e: cat");

    char cat_missing[] = "cat nothing";
    print_result(br_shell_execute(cat_missing) == -1, "Test 1f: cat of a missing file fails");

    char cat_usage[] = "cat";
    print_result(br_shell_execute(cat_usage) == -1, "Test 1g: cat without an argument fails");

    char write_empty[] = "write blank";
    print_result(br_shell_execute(write_empty) == 0 &&
                 br_fs_stat("blank", &size) == BR_OK && size == 0,
                 "Test 1h: write without text creates an empty file");

    print_result(br_shell_execute(ls_line) == 0, "Test 1i: ls");
}

static uint8_t stack_supervisor[2048];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Shell Filesystem Test ===\n\n");

    br_shell_register_fs();
    run_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Shell Filesystem Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

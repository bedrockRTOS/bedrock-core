/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_fs.h"
#include "br_ramdisk.h"

#define BLOCK_SIZE   128
#define BLOCK_COUNT  32

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static uint8_t      disk[BLOCK_SIZE * BLOCK_COUNT];
static br_ramdisk_t ramdisk;
static uint8_t      pattern[1200];
static uint8_t      readback[1200];

static void fill_pattern(uint8_t seed)
{
    for (size_t i = 0; i < sizeof(pattern); i++) {
        pattern[i] = (uint8_t)(seed + i * 7);
    }
}

static bool write_file(const char *name, const uint8_t *data, size_t len, size_t step)
{
    int fd = br_fs_open(name, BR_FS_WRITE);
    if (fd < 0) {
        return false;
    }
    bool ok = true;
    for (size_t off = 0; off < len && ok; off += step) {
        size_t n = len - off < step ? len - off : step;
        ok = br_fs_write(fd, data + off, n) == (int)n;
    }
    return br_fs_close(fd) == BR_OK && ok;
}

static bool read_file_equals(const char *name, const uint8_t *data, size_t len, size_t step)
{
    int fd = br_fs_open(name, BR_FS_READ);
    if (fd < 0) {
        return false;
    }
    size_t total = 0;
    int n;
    while (total < sizeof(readback) &&
           (n = br_fs_read(fd, readback + total, step)) > 0) {
        total += (size_t)n;
    }
    br_fs_close(fd);

    if (total != len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (readback[i] != data[i]) {
            return false;
        }
    }
    return true;
}

static uint32_t count_files(void)
{
    br_fs_info_t info;
    uint32_t n = 0;
    while (br_fs_list(n, &info) == BR_OK) {
        n++;
    }
    return n;
}

static uint32_t free_blocks(void)
{
    br_fs_usage_t u;
    br_fs_usage(&u);
    return u.free_blocks;
}

static void run_blkdev_tests(void)
{
    br_uart_puts("Test 1: Block device\n");

    br_err_t err = br_ramdisk_init(&ramdisk, disk, BLOCK_SIZE, BLOCK_COUNT);
    print_result(err == BR_OK, "Test 1a: init");

    br_blkdev_t *dev = &ramdisk.dev;
    uint8_t buf[8];

    print_result(br_blkdev_read(dev, 0, 0, buf, 8) == BR_OK &&
                 buf[0] == 0xFF && buf[7] == 0xFF,
                 "Test 1b: a new device is erased");

    uint8_t a[4] = { 0xF0, 0xF0, 0xF0, 0xF0 };
    uint8_t b[4] = { 0x0F, 0x0F, 0x0F, 0x0F };
    br_blkdev_write(dev, 1, 0, a, 4);
    br_blkdev_write(dev, 1, 0, b, 4);
    br_blkdev_read(dev, 1, 0, buf, 4);
    print_result(buf[0] == 0x00 && buf[3] == 0x00,
                 "Test 1c: programming only clears bits");

    br_blkdev_erase(dev, 1);
    br_blkdev_read(dev, 1, 0, buf, 4);
    print_result(buf[0] == 0xFF && buf[3] == 0xFF, "Test 1d: erase sets all bits");

    print_result(br_blkdev_read(dev, BLOCK_COUNT, 0, buf, 4) == BR_ERR_INVALID &&
                 br_blkdev_read(dev, 0, BLOCK_SIZE - 2, buf, 4) == BR_ERR_INVALID &&
                 br_blkdev_erase(dev, BLOCK_COUNT) == BR_ERR_INVALID,
                 "Test 1e: out of range access is rejected");

    print_result(br_blkdev_write(dev, 0, 2, a, 4) == BR_ERR_INVALID &&
                 br_blkdev_write(dev, 0, 0, a, 3) == BR_ERR_INVALID,
                 "Test 1f: unaligned program is rejected");
}

static void run_mount_tests(void)
{
    br_uart_puts("Test 2: Format and mount\n");

    print_result(br_fs_open("a", BR_FS_READ) == BR_ERR_INVALID,
                 "Test 2a: open before mount is rejected");

    print_result(br_fs_mount(&ramdisk.dev) == BR_ERR_IO,
                 "Test 2b: mounting an unformatted device fails");

    print_result(br_fs_format(&ramdisk.dev) == BR_OK, "Test 2c: format");
    print_result(br_fs_mount(&ramdisk.dev) == BR_OK, "Test 2d: mount");
    print_result(br_fs_mount(&ramdisk.dev) == BR_ERR_BUSY, "Test 2e: second mount is rejected");
    print_result(br_fs_format(&ramdisk.dev) == BR_ERR_BUSY, "Test 2f: format while mounted is rejected");

    br_fs_usage_t u;
    print_result(br_fs_usage(&u) == BR_OK && u.total_blocks == BLOCK_COUNT &&
                 u.free_blocks == BLOCK_COUNT - 1 && count_files() == 0,
                 "Test 2g: empty filesystem");
}

static void run_file_tests(void)
{
    br_uart_puts("Test 3: Files\n");

    uint32_t free_start = free_blocks();

    print_result(write_file("hello", (const uint8_t *)"hello", 5, 5), "Test 3a: write a small file");

    size_t size = 0;
    print_result(br_fs_stat("hello", &size) == BR_OK && size == 5, "Test 3b: stat");
    print_result(read_file_equals("hello", (const uint8_t *)"hello", 5, 16), "Test 3c: read back");

    int fd = br_fs_open("hello", BR_FS_READ);
    char tmp[8];
    br_fs_read(fd, tmp, sizeof(tmp));
    print_result(br_fs_read(fd, tmp, sizeof(tmp)) == 0, "Test 3d: read at end returns 0");
    br_fs_close(fd);

    print_result(write_file("empty", pattern, 0, 1) && br_fs_stat("empty", &size) == BR_OK && size == 0 &&
                 read_file_equals("empty", pattern, 0, 8),
                 "Test 3e: empty file");

    fill_pattern(3);
    print_result(write_file("big", pattern, sizeof(pattern), 37), "Test 3f: write a multi-block file");
    print_result(br_fs_stat("big", &size) == BR_OK && size == sizeof(pattern), "Test 3g: multi-block size");
    print_result(read_file_equals("big", pattern, sizeof(pattern), 50), "Test 3h: multi-block read back");
    print_result(read_file_equals("big", pattern, sizeof(pattern), 1), "Test 3i: byte by byte read back");

    br_fs_info_t info;
    print_result(count_files() == 3 && br_fs_list(0, &info) == BR_OK &&
                 br_fs_list(3, &info) == BR_ERR_NOENT,
                 "Test 3j: list");

    fill_pattern(9);
    print_result(write_file("big", pattern, 300, 100) &&
                 read_file_equals("big", pattern, 300, 64) && count_files() == 3,
                 "Test 3k: rewriting replaces the old content");

    print_result(br_fs_remove("big") == BR_OK && br_fs_remove("big") == BR_ERR_NOENT &&
                 br_fs_open("big", BR_FS_READ) == BR_ERR_NOENT,
                 "Test 3l: remove");

    br_fs_remove("hello");
    br_fs_remove("empty");
    print_result(count_files() == 0 && free_blocks() == free_start,
                 "Test 3m: removed files give their blocks back");
}

static void run_error_tests(void)
{
    br_uart_puts("Test 4: Errors and limits\n");

    char toolong[CONFIG_FS_NAME_MAX + 2];
    for (size_t i = 0; i < sizeof(toolong) - 1; i++) {
        toolong[i] = 'n';
    }
    toolong[sizeof(toolong) - 1] = '\0';

    print_result(br_fs_open("", BR_FS_READ) == BR_ERR_INVALID &&
                 br_fs_open(toolong, BR_FS_WRITE) == BR_ERR_INVALID &&
                 br_fs_open("a b", BR_FS_WRITE) == BR_ERR_INVALID &&
                 br_fs_open(NULL, BR_FS_READ) == BR_ERR_INVALID,
                 "Test 4a: bad names are rejected");

    print_result(br_fs_open("x", 0) == BR_ERR_INVALID &&
                 br_fs_open("x", BR_FS_READ | BR_FS_WRITE) == BR_ERR_INVALID,
                 "Test 4b: bad flags are rejected");

    print_result(br_fs_open("nothing", BR_FS_READ) == BR_ERR_NOENT, "Test 4c: missing file");

    write_file("f", (const uint8_t *)"data", 4, 4);

    int w = br_fs_open("g", BR_FS_WRITE);
    char c[4];
    print_result(br_fs_read(w, c, 4) == BR_ERR_INVALID, "Test 4d: read on a write descriptor");

    print_result(br_fs_open("h", BR_FS_WRITE) == BR_ERR_BUSY,
                 "Test 4e: a second writer is rejected");
    print_result(br_fs_open("g", BR_FS_READ) == BR_ERR_BUSY,
                 "Test 4f: reading a file that is being written is rejected");
    br_fs_close(w);

    int r = br_fs_open("f", BR_FS_READ);
    print_result(br_fs_write(r, "x", 1) == BR_ERR_INVALID, "Test 4g: write on a read descriptor");
    print_result(br_fs_open("f", BR_FS_WRITE) == BR_ERR_BUSY, "Test 4h: writing an open file is rejected");
    print_result(br_fs_remove("f") == BR_ERR_BUSY, "Test 4i: removing an open file is rejected");
    print_result(br_fs_unmount() == BR_ERR_BUSY, "Test 4j: unmount with open files is rejected");

    int fds[CONFIG_FS_MAX_OPEN];
    int opened = 1;
    fds[0] = r;
    while (opened < CONFIG_FS_MAX_OPEN) {
        fds[opened] = br_fs_open("f", BR_FS_READ);
        if (fds[opened] < 0) {
            break;
        }
        opened++;
    }
    print_result(opened == CONFIG_FS_MAX_OPEN && br_fs_open("f", BR_FS_READ) == BR_ERR_NOMEM,
                 "Test 4k: descriptor table is limited");
    for (int i = 0; i < opened; i++) {
        br_fs_close(fds[i]);
    }

    print_result(br_fs_close(r) == BR_ERR_INVALID && br_fs_read(99, c, 4) == BR_ERR_INVALID &&
                 br_fs_read(-1, c, 4) == BR_ERR_INVALID,
                 "Test 4l: bad descriptors are rejected");

    br_fs_remove("f");
    br_fs_remove("g");
}

static void run_space_tests(void)
{
    br_uart_puts("Test 5: Running out of space\n");

    fill_pattern(5);
    write_file("keep", pattern, 200, 50);
    uint32_t free_before = free_blocks();

    int fd = br_fs_open("huge", BR_FS_WRITE);
    bool failed = false;
    for (int i = 0; i < 40 && !failed; i++) {
        failed = br_fs_write(fd, pattern, 1000) != 1000;
    }
    print_result(failed, "Test 5a: a write beyond the capacity fails");
    print_result(br_fs_close(fd) == BR_ERR_IO, "Test 5b: close reports the failed file");

    print_result(br_fs_open("huge", BR_FS_READ) == BR_ERR_NOENT &&
                 read_file_equals("keep", pattern, 200, 40),
                 "Test 5c: the failed file does not exist and other files are intact");
    print_result(free_blocks() == free_before, "Test 5d: blocks of the failed file are free again");

    br_fs_remove("keep");
}

static void run_persistence_tests(void)
{
    br_uart_puts("Test 6: Persistence\n");

    fill_pattern(11);
    write_file("saved", pattern, 500, 90);
    write_file("note", (const uint8_t *)"text", 4, 4);

    print_result(br_fs_unmount() == BR_OK, "Test 6a: unmount");
    print_result(br_fs_open("saved", BR_FS_READ) == BR_ERR_INVALID, "Test 6b: no access after unmount");
    print_result(br_fs_mount(&ramdisk.dev) == BR_OK, "Test 6c: mount again");
    print_result(read_file_equals("saved", pattern, 500, 33) &&
                 read_file_equals("note", (const uint8_t *)"text", 4, 4) &&
                 count_files() == 2,
                 "Test 6d: files survive a remount");

    write_file("saved", pattern, 100, 100);
    br_fs_unmount();
    br_fs_mount(&ramdisk.dev);
    print_result(read_file_equals("saved", pattern, 100, 100) && count_files() == 2,
                 "Test 6e: the newest version wins after a remount");

    br_fs_remove("saved");
    br_fs_remove("note");
}

static void run_powerfail_tests(void)
{
    br_uart_puts("Test 7: Power loss during a rewrite\n");

    uint8_t old_data[300];
    uint8_t new_data[300];
    for (size_t i = 0; i < sizeof(old_data); i++) {
        old_data[i] = (uint8_t)(i * 3 + 1);
        new_data[i] = (uint8_t)(i * 5 + 2);
    }

    bool all_atomic = true;
    int  saw_old = 0;
    int  saw_new = 0;

    for (int cut = 0; cut < 14; cut++) {
        write_file("cfg", old_data, sizeof(old_data), 100);

        ramdisk.writes_until_fail = cut;
        write_file("cfg", new_data, sizeof(new_data), 100);
        ramdisk.writes_until_fail = -1;

        br_fs_unmount();
        br_fs_mount(&ramdisk.dev);

        bool is_old = read_file_equals("cfg", old_data, sizeof(old_data), 64);
        bool is_new = read_file_equals("cfg", new_data, sizeof(new_data), 64);

        if (is_old) {
            saw_old++;
        }
        if (is_new) {
            saw_new++;
        }
        if ((!is_old && !is_new) || count_files() != 1) {
            all_atomic = false;
        }
        br_fs_remove("cfg");
    }

    print_result(all_atomic, "Test 7a: the file is always either the old or the new version");
    print_result(saw_old > 0 && saw_new > 0, "Test 7b: both outcomes were exercised");
    print_result(free_blocks() == BLOCK_COUNT - 1, "Test 7c: no blocks are leaked");
}

static uint8_t stack_supervisor[2048];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== Filesystem Test ===\n\n");

    run_blkdev_tests();
    run_mount_tests();
    run_file_tests();
    run_error_tests();
    run_space_tests();
    run_persistence_tests();
    run_powerfail_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - Filesystem Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

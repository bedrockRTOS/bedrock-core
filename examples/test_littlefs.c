/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "br_lfs.h"
#include "br_ramdisk.h"

#define BLOCK_SIZE   256
#define BLOCK_COUNT  32

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static uint8_t       disk[BLOCK_SIZE * BLOCK_COUNT];
static br_ramdisk_t  ramdisk;
static br_lfs_t      fs;
static br_lfs_file_t file;
static uint8_t       pattern[900];
static uint8_t       readback[900];

static void fill_pattern(uint8_t seed, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        pattern[i] = (uint8_t)(seed + i * 7);
    }
}

static bool write_file(const char *path, const uint8_t *data, size_t len)
{
    if (br_lfs_file_open(&fs, &file, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) < 0) {
        return false;
    }
    bool ok = lfs_file_write(&fs.lfs, &file.file, data, len) == (lfs_ssize_t)len;
    return lfs_file_close(&fs.lfs, &file.file) == 0 && ok;
}

static bool file_equals(const char *path, const uint8_t *data, size_t len)
{
    if (br_lfs_file_open(&fs, &file, path, LFS_O_RDONLY) < 0) {
        return false;
    }
    lfs_ssize_t n = lfs_file_read(&fs.lfs, &file.file, readback, sizeof(readback));
    lfs_file_close(&fs.lfs, &file.file);

    if (n != (lfs_ssize_t)len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (readback[i] != data[i]) {
            return false;
        }
    }
    return true;
}

static int count_entries(const char *path)
{
    lfs_dir_t dir;
    struct lfs_info info;
    int n = 0;

    if (lfs_dir_open(&fs.lfs, &dir, path) < 0) {
        return -1;
    }
    while (lfs_dir_read(&fs.lfs, &dir, &info) > 0) {
        if (info.name[0] != '.') {
            n++;
        }
    }
    lfs_dir_close(&fs.lfs, &dir);
    return n;
}

static void run_setup_tests(void)
{
    br_uart_puts("Test 1: Port setup\n");

    br_ramdisk_init(&ramdisk, disk, BLOCK_SIZE, BLOCK_COUNT);

    print_result(br_lfs_init(&fs, &ramdisk.dev) == BR_OK, "Test 1a: init");
    print_result(lfs_mount(&fs.lfs, &fs.cfg) != 0, "Test 1b: mounting an unformatted device fails");
    print_result(lfs_format(&fs.lfs, &fs.cfg) == 0, "Test 1c: format");
    print_result(lfs_mount(&fs.lfs, &fs.cfg) == 0, "Test 1d: mount");

    static br_ramdisk_t other;
    static uint8_t other_disk[100 * 4];
    br_ramdisk_init(&other, other_disk, 100, 4);
    static br_lfs_t other_fs;
    print_result(br_lfs_init(&other_fs, &other.dev) == BR_ERR_INVALID,
                 "Test 1e: a block size that the cache does not divide is rejected");
}

static void run_file_tests(void)
{
    br_uart_puts("Test 2: Files and directories\n");

    print_result(write_file("hello", (const uint8_t *)"hello", 5) &&
                 file_equals("hello", (const uint8_t *)"hello", 5),
                 "Test 2a: write and read a file");

    struct lfs_info info;
    print_result(lfs_stat(&fs.lfs, "hello", &info) == 0 && info.size == 5 &&
                 info.type == LFS_TYPE_REG,
                 "Test 2b: stat");

    fill_pattern(3, sizeof(pattern));
    print_result(write_file("big", pattern, sizeof(pattern)) &&
                 file_equals("big", pattern, sizeof(pattern)),
                 "Test 2c: a file larger than a block");

    print_result(lfs_mkdir(&fs.lfs, "dir") == 0 &&
                 write_file("dir/a", (const uint8_t *)"in dir", 6) &&
                 file_equals("dir/a", (const uint8_t *)"in dir", 6),
                 "Test 2d: a file in a directory");

    print_result(count_entries("/") == 3 && count_entries("dir") == 1,
                 "Test 2e: directory listing");

    print_result(lfs_rename(&fs.lfs, "dir/a", "moved") == 0 &&
                 file_equals("moved", (const uint8_t *)"in dir", 6) &&
                 lfs_stat(&fs.lfs, "dir/a", &info) == LFS_ERR_NOENT,
                 "Test 2f: rename across directories");

    print_result(lfs_remove(&fs.lfs, "moved") == 0 &&
                 lfs_remove(&fs.lfs, "dir") == 0 &&
                 lfs_remove(&fs.lfs, "hello") == 0 &&
                 lfs_stat(&fs.lfs, "hello", &info) == LFS_ERR_NOENT &&
                 count_entries("/") == 1,
                 "Test 2g: remove");

    print_result(br_lfs_file_open(&fs, &file, "nothing", LFS_O_RDONLY) == LFS_ERR_NOENT,
                 "Test 2h: a missing file");

    fill_pattern(9, 200);
    write_file("big", pattern, 200);
    print_result(file_equals("big", pattern, 200), "Test 2i: a file is replaced with a shorter one");
    lfs_remove(&fs.lfs, "big");
}

static void run_persistence_tests(void)
{
    br_uart_puts("Test 3: Persistence\n");

    fill_pattern(11, 500);
    write_file("saved", pattern, 500);

    print_result(lfs_unmount(&fs.lfs) == 0 && lfs_mount(&fs.lfs, &fs.cfg) == 0,
                 "Test 3a: remount");
    print_result(file_equals("saved", pattern, 500), "Test 3b: a file survives a remount");
    lfs_remove(&fs.lfs, "saved");
}

static void run_powerfail_tests(void)
{
    br_uart_puts("Test 4: Power loss during a rewrite\n");

    uint8_t old_data[300];
    uint8_t new_data[300];
    for (size_t i = 0; i < sizeof(old_data); i++) {
        old_data[i] = (uint8_t)(i * 3 + 1);
        new_data[i] = (uint8_t)(i * 5 + 2);
    }

    bool all_intact = true;
    int  saw_old = 0;
    int  saw_new = 0;

    for (int cut = 0; cut < 40; cut += 2) {
        write_file("cfg", old_data, sizeof(old_data));

        ramdisk.writes_until_fail = cut;
        write_file("cfg", new_data, sizeof(new_data));
        ramdisk.writes_until_fail = -1;

        lfs_unmount(&fs.lfs);
        if (lfs_mount(&fs.lfs, &fs.cfg) != 0) {
            all_intact = false;
            break;
        }

        bool is_old = file_equals("cfg", old_data, sizeof(old_data));
        bool is_new = file_equals("cfg", new_data, sizeof(new_data));
        saw_old += is_old;
        saw_new += is_new;
        if (!is_old && !is_new) {
            all_intact = false;
        }
    }

    print_result(all_intact, "Test 4a: the file is always either the old or the new version");
    print_result(saw_old > 0 && saw_new > 0, "Test 4b: both outcomes were exercised");
}

static uint8_t stack_supervisor[3072];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== littlefs Test ===\n\n");

    run_setup_tests();
    run_file_tests();
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

    br_uart_puts("\nbedrock[RTOS] - littlefs Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "ff.h"
#include "br_ff.h"
#include "br_ramdisk.h"

#define SECTOR_SIZE   512
#define SECTOR_COUNT  128

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

static uint8_t      disk[SECTOR_SIZE * SECTOR_COUNT];
static br_ramdisk_t ramdisk;
static FATFS        fatfs;
static FIL          fil;
static uint8_t      work[FF_MAX_SS];
static uint8_t      pattern[3000];
static uint8_t      readback[3000];

static void fill_pattern(uint8_t seed, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        pattern[i] = (uint8_t)(seed + i * 7);
    }
}

static bool write_file(const char *path, const uint8_t *data, size_t len)
{
    if (f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        return false;
    }
    UINT written = 0;
    FRESULT r = f_write(&fil, data, (UINT)len, &written);
    return f_close(&fil) == FR_OK && r == FR_OK && written == len;
}

static bool file_equals(const char *path, const uint8_t *data, size_t len)
{
    if (f_open(&fil, path, FA_READ) != FR_OK) {
        return false;
    }
    UINT n = 0;
    FRESULT r = f_read(&fil, readback, sizeof(readback), &n);
    f_close(&fil);

    if (r != FR_OK || n != len) {
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
    DIR dir;
    FILINFO info;
    int n = 0;

    if (f_opendir(&dir, path) != FR_OK) {
        return -1;
    }
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        n++;
    }
    f_closedir(&dir);
    return n;
}

static void run_setup_tests(void)
{
    br_uart_puts("Test 1: Port setup\n");

    br_ramdisk_init(&ramdisk, disk, SECTOR_SIZE, SECTOR_COUNT);

    print_result(br_ff_attach(&ramdisk.dev) == BR_OK, "Test 1a: attach");

    static br_ramdisk_t other;
    static uint8_t other_disk[256 * 4];
    br_ramdisk_init(&other, other_disk, 256, 4);
    print_result(br_ff_attach(&other.dev) == BR_ERR_INVALID,
                 "Test 1b: a block size other than 512 is rejected");
    br_ff_attach(&ramdisk.dev);

    print_result(f_mount(&fatfs, "", 1) == FR_NO_FILESYSTEM,
                 "Test 1c: mounting an unformatted device fails");

    MKFS_PARM opt = { .fmt = FM_FAT | FM_SFD };
    print_result(f_mkfs("", &opt, work, sizeof(work)) == FR_OK, "Test 1d: format");
    print_result(f_mount(&fatfs, "", 1) == FR_OK, "Test 1e: mount");
}

static void run_file_tests(void)
{
    br_uart_puts("Test 2: Files and directories\n");

    print_result(write_file("HELLO.TXT", (const uint8_t *)"hello", 5) &&
                 file_equals("HELLO.TXT", (const uint8_t *)"hello", 5),
                 "Test 2a: write and read a file");

    FILINFO info;
    print_result(f_stat("HELLO.TXT", &info) == FR_OK && info.fsize == 5,
                 "Test 2b: stat");

    fill_pattern(3, sizeof(pattern));
    print_result(write_file("BIG.BIN", pattern, sizeof(pattern)) &&
                 file_equals("BIG.BIN", pattern, sizeof(pattern)),
                 "Test 2c: a file larger than a cluster");

    print_result(f_mkdir("DIR") == FR_OK &&
                 write_file("DIR/A.TXT", (const uint8_t *)"in dir", 6) &&
                 file_equals("DIR/A.TXT", (const uint8_t *)"in dir", 6),
                 "Test 2d: a file in a directory");

    print_result(count_entries("") == 3 && count_entries("DIR") == 1,
                 "Test 2e: directory listing");

    print_result(f_rename("DIR/A.TXT", "MOVED.TXT") == FR_OK &&
                 file_equals("MOVED.TXT", (const uint8_t *)"in dir", 6) &&
                 f_stat("DIR/A.TXT", &info) == FR_NO_FILE,
                 "Test 2f: rename across directories");

    DWORD free_before;
    FATFS *fsp;
    f_getfree("", &free_before, &fsp);

    print_result(f_unlink("MOVED.TXT") == FR_OK &&
                 f_unlink("DIR") == FR_OK &&
                 f_unlink("HELLO.TXT") == FR_OK &&
                 f_stat("HELLO.TXT", &info) == FR_NO_FILE &&
                 count_entries("") == 1,
                 "Test 2g: remove");

    DWORD free_after;
    f_getfree("", &free_after, &fsp);
    print_result(free_after > free_before, "Test 2h: removing files frees clusters");

    print_result(f_open(&fil, "NOTHING.TXT", FA_READ) == FR_NO_FILE, "Test 2i: a missing file");

    f_unlink("BIG.BIN");
}

static void run_persistence_tests(void)
{
    br_uart_puts("Test 3: Persistence\n");

    fill_pattern(11, 1500);
    write_file("SAVED.BIN", pattern, 1500);

    print_result(f_mount(NULL, "", 0) == FR_OK && f_mount(&fatfs, "", 1) == FR_OK,
                 "Test 3a: remount");
    print_result(file_equals("SAVED.BIN", pattern, 1500), "Test 3b: a file survives a remount");
    f_unlink("SAVED.BIN");
}

static uint8_t stack_supervisor[3072];

static void supervisor_task(void *arg)
{
    (void)arg;

    br_uart_puts("\n=== FatFs Test ===\n\n");

    run_setup_tests();
    run_file_tests();
    run_persistence_tests();

    br_uart_puts("\n=== All Tests Complete ===\n");

    while (1) {
        br_sleep_ms(1000);
    }
}

int main(void)
{
    br_kernel_init();

    br_uart_puts("\nbedrock[RTOS] - FatFs Test\n");

    br_tid_t supervisor_tid;
    br_task_create(&supervisor_tid, "supervisor", supervisor_task, NULL,
                   0, stack_supervisor, sizeof(stack_supervisor));

    br_kernel_start();
}

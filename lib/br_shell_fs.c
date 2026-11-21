/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_shell.h"
#include "br_console.h"
#include "br_fs.h"

#define CAT_CHUNK  32

static int ls_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    br_fs_info_t info;
    for (uint32_t i = 0; br_fs_list(i, &info) == BR_OK; i++) {
        size_t n = 0;
        while (info.name[n] != '\0') {
            n++;
        }
        br_printf("%s", info.name);
        for (; n < CONFIG_FS_NAME_MAX - 1; n++) {
            br_uart_putc(' ');
        }
        br_printf(" %6zu\n", info.size);
    }

    br_fs_usage_t usage;
    if (br_fs_usage(&usage) != BR_OK) {
        br_printf("filesystem is not mounted\n");
        return -1;
    }
    br_printf("%u of %u blocks free, block size %u\n",
              (unsigned)usage.free_blocks, (unsigned)usage.total_blocks,
              (unsigned)usage.block_size);
    return 0;
}

static int cat_handler(int argc, char **argv)
{
    if (argc != 2) {
        br_printf("usage: cat <file>\n");
        return -1;
    }

    int fd = br_fs_open(argv[1], BR_FS_READ);
    if (fd < 0) {
        br_printf("cannot open %s: %d\n", argv[1], fd);
        return -1;
    }

    char buf[CAT_CHUNK];
    int  n;
    while ((n = br_fs_read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            br_uart_putc(buf[i]);
        }
    }
    br_fs_close(fd);

    if (n < 0) {
        br_printf("read error: %d\n", n);
        return -1;
    }
    br_uart_puts("\n");
    return 0;
}

static int write_handler(int argc, char **argv)
{
    if (argc < 2) {
        br_printf("usage: write <file> [text...]\n");
        return -1;
    }

    int fd = br_fs_open(argv[1], BR_FS_WRITE);
    if (fd < 0) {
        br_printf("cannot open %s: %d\n", argv[1], fd);
        return -1;
    }

    br_err_t err = BR_OK;
    for (int i = 2; i < argc && err == BR_OK; i++) {
        size_t len = 0;
        while (argv[i][len] != '\0') {
            len++;
        }
        if (i > 2 && br_fs_write(fd, " ", 1) != 1) {
            err = BR_ERR_IO;
        }
        if (err == BR_OK && br_fs_write(fd, argv[i], len) != (int)len) {
            err = BR_ERR_IO;
        }
    }

    br_err_t close_err = br_fs_close(fd);
    if (err == BR_OK) {
        err = close_err;
    }
    if (err != BR_OK) {
        br_printf("write failed: %d\n", (int)err);
        return -1;
    }
    return 0;
}

static br_shell_cmd_t ls_cmd = {
    .name    = "ls",
    .help    = "list files",
    .handler = ls_handler,
};

static br_shell_cmd_t cat_cmd = {
    .name    = "cat",
    .help    = "print a file",
    .handler = cat_handler,
};

static br_shell_cmd_t write_cmd = {
    .name    = "write",
    .help    = "write the arguments to a file",
    .handler = write_handler,
};

void br_shell_register_fs(void)
{
    br_shell_register(&ls_cmd);
    br_shell_register(&cat_cmd);
    br_shell_register(&write_cmd);
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_shell.h"

static int help_handler(int argc, char **argv);

static br_shell_cmd_t help_cmd = {
    .name    = "help",
    .help    = "list commands",
    .handler = help_handler,
    .next    = NULL,
};

static br_shell_cmd_t *cmd_list = &help_cmd;
static bool last_was_cr;

static bool str_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int help_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    for (br_shell_cmd_t *cmd = cmd_list; cmd != NULL; cmd = cmd->next) {
        br_uart_puts(cmd->name);
        br_uart_puts(" - ");
        br_uart_puts(cmd->help);
        br_uart_puts("\n");
    }
    return 0;
}

void br_shell_register(br_shell_cmd_t *cmd)
{
    br_shell_cmd_t *tail = cmd_list;
    while (tail->next != NULL) {
        tail = tail->next;
    }
    cmd->next = NULL;
    tail->next = cmd;
}

br_err_t br_shell_readline(char *buf, size_t size, br_time_t timeout)
{
    size_t len = 0;

    while (1) {
        char c;
        br_err_t err = br_uart_getc(&c, timeout);
        if (err != BR_OK) {
            buf[len] = '\0';
            return err;
        }

        bool skip_lf = (c == '\n' && last_was_cr);
        last_was_cr = (c == '\r');
        if (skip_lf) {
            continue;
        }

        if (c == '\r' || c == '\n') {
            br_uart_puts("\n");
            buf[len] = '\0';
            return BR_OK;
        }

        if (c == '\b' || c == 0x7F) {
            if (len > 0) {
                len--;
                br_uart_puts("\b \b");
            }
            continue;
        }

        if (c < 0x20 || c > 0x7E) {
            continue;
        }

        if (len + 1 < size) {
            buf[len++] = c;
            br_uart_putc(c);
        }
    }
}

int br_shell_execute(char *line)
{
    char *argv[CONFIG_SHELL_MAX_ARGS];
    int argc = 0;

    while (*line) {
        while (is_space(*line)) {
            *line++ = '\0';
        }
        if (*line == '\0') {
            break;
        }
        if (argc == CONFIG_SHELL_MAX_ARGS) {
            br_uart_puts("too many arguments\n");
            return -1;
        }
        argv[argc++] = line;
        while (*line && !is_space(*line)) {
            line++;
        }
    }

    if (argc == 0) {
        return 0;
    }

    for (br_shell_cmd_t *cmd = cmd_list; cmd != NULL; cmd = cmd->next) {
        if (str_equal(cmd->name, argv[0])) {
            return cmd->handler(argc, argv);
        }
    }

    br_uart_puts("unknown command: ");
    br_uart_puts(argv[0]);
    br_uart_puts("\n");
    return -1;
}

void br_shell_run(void)
{
    static char line[CONFIG_SHELL_LINE_MAX];

    while (1) {
        br_uart_puts("> ");
        br_shell_readline(line, sizeof(line), BR_TIME_INFINITE);
        br_shell_execute(line);
    }
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_SHELL_H
#define BR_SHELL_H

#include "bedrock/bedrock.h"

typedef int (*br_shell_handler_t)(int argc, char **argv);

typedef struct br_shell_cmd {
    const char          *name;
    const char          *help;
    br_shell_handler_t   handler;
    struct br_shell_cmd *next;
} br_shell_cmd_t;

void     br_shell_register(br_shell_cmd_t *cmd);
br_err_t br_shell_readline(char *buf, size_t size, br_time_t timeout);
int      br_shell_execute(char *line);
void     br_shell_run(void) __attribute__((noreturn));

#endif

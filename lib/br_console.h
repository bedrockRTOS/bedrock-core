/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_CONSOLE_H
#define BR_CONSOLE_H

#include <stddef.h>

int br_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int br_snprintf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif

/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE-GPL-3.0.md for the full terms and runtime exception.
 *
 * Kernel panic / assert infrastructure.
 *
 * Provides a single configurable panic path:
 *
 *   br_assert(expr)  ->  BR_PANIC(msg)  ->  br_panic_invoke()
 *                                               |
 *                               g_panic_handler (user-supplied, optional)
 *                                               |
 *                               br_hal_panic()  (architecture halt, noreturn)
 *
 * The kernel itself never touches hardware here — all platform-specific
 * work is delegated to br_hal_panic() defined in the arch layer.
 */

#include "bedrock/br_assert.h"
#include "bedrock/br_hal.h"

static br_panic_handler_t g_panic_handler = NULL;

void br_set_panic_handler(br_panic_handler_t handler)
{
    g_panic_handler = handler;
}

__attribute__((noreturn))
void br_panic_invoke(const char *msg, const char *file, int line)
{
    if (g_panic_handler != NULL) {
        g_panic_handler(msg, file, line);
        /* Handler must not return, but guard against a broken one. */
    }

    br_hal_panic(msg, file, line);
}

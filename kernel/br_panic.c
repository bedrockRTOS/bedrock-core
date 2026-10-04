/*
 * SPDX-License-Identifier: GPL-3.0-only
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

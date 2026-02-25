/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 */

#ifndef BR_ASSERT_H
#define BR_ASSERT_H

#include "br_config.h"

/*
 * Panic handler type.
 *
 * A user-supplied function that receives the panic message, source file,
 * and line number.  The handler MUST NOT return — the system is in an
 * unrecoverable state when it is called.
 *
 * If no handler is registered (or the handler returns), br_panic_invoke()
 * falls back to br_hal_panic() which disables interrupts and halts.
 */
typedef void (*br_panic_handler_t)(const char *msg, const char *file, int line);

/*
 * Register a custom panic handler.
 * Pass NULL to restore the default HAL halt behaviour.
 */
void br_set_panic_handler(br_panic_handler_t handler);

/*
 * Internal panic entry point.  Do not call directly — use BR_PANIC() so
 * that __FILE__ and __LINE__ are captured automatically.
 */
void br_panic_invoke(const char *msg, const char *file, int line)
    __attribute__((noreturn));

/*
 * BR_PANIC(msg) — trigger an unrecoverable kernel panic.
 *
 * Captures the call-site file/line and invokes the registered panic
 * handler (or the default HAL halt if none is set).
 *
 * Usage:
 *   BR_PANIC("unexpected state");
 */
#define BR_PANIC(msg)  br_panic_invoke((msg), __FILE__, __LINE__)

/*
 * br_assert(expr) — debug assertion.
 *
 * When CONFIG_ASSERT is non-zero (the default), evaluates expr and calls
 * BR_PANIC() if it is false.  When CONFIG_ASSERT is 0, the expression is
 * still evaluated for side-effects but no check is performed.
 *
 * Usage:
 *   br_assert(ptr != NULL);
 *   br_assert(count < CONFIG_MAX_TASKS);
 */
#if CONFIG_ASSERT
#  define br_assert(expr) \
    do { \
        if (!(expr)) { \
            BR_PANIC("assert failed: " #expr); \
        } \
    } while (0)
#else
#  define br_assert(expr) ((void)(expr))
#endif

#endif /* BR_ASSERT_H */

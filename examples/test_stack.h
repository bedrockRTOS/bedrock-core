/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef TEST_STACK_H
#define TEST_STACK_H

/* Stack size of a test task. An AVR has 2 KiB of RAM in all, so the tests
 * run with 3/16 of the stack, which leaves room for the 35-byte context frame
 * and the deepest kernel call. */
#ifdef __AVR__
#  define TEST_STACK(n)  ((n) * 3 / 16)
#else
#  define TEST_STACK(n)  (n)
#endif

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#include <stdbool.h>
#include "br_pool.h"

extern void br_uart_puts(const char *s);

static void print_result(bool pass, const char *label)
{
    br_uart_puts(label);
    br_uart_puts(pass ? ": PASS\n" : ": FAIL\n");
}

#define POOL_BLOCKS 4

static uint8_t pool_buf[POOL_BLOCKS * sizeof(void *)];

static void run_alloc_free_test(void)
{
    br_uart_puts("Test 1: Alloc/free\n");

    br_pool_handle_t h = br_pool_create(pool_buf, sizeof(pool_buf), sizeof(void *));

    bool created_ok = (h != NULL) && br_pool_total(h) == POOL_BLOCKS &&
                       br_pool_available(h) == POOL_BLOCKS;

    void *a = br_pool_alloc(h);
    bool alloc_ok = (a != NULL) && br_pool_available(h) == POOL_BLOCKS - 1;

    br_pool_free(h, a);
    bool free_ok = br_pool_available(h) == POOL_BLOCKS;

    void *b = br_pool_alloc(h);
    bool reuse_ok = (b == a);

    br_pool_free(h, b);

    print_result(created_ok && alloc_ok && free_ok && reuse_ok,
                 "Test 1: create/alloc/free/reuse behave correctly");
}

static void run_exhaustion_test(void)
{
    br_uart_puts("Test 2: Exhaustion\n");

    br_pool_handle_t h = br_pool_create(pool_buf, sizeof(pool_buf), sizeof(void *));

    void *blocks[POOL_BLOCKS];
    bool all_distinct = true;
    for (int i = 0; i < POOL_BLOCKS; i++) {
        blocks[i] = br_pool_alloc(h);
        if (blocks[i] == NULL) {
            all_distinct = false;
        }
        for (int j = 0; j < i; j++) {
            if (blocks[j] == blocks[i]) {
                all_distinct = false;
            }
        }
    }

    bool exhausted = (br_pool_available(h) == 0);
    void *overflow = br_pool_alloc(h);
    bool overflow_rejected = (overflow == NULL);

    for (int i = 0; i < POOL_BLOCKS; i++) {
        br_pool_free(h, blocks[i]);
    }

    print_result(all_distinct && exhausted && overflow_rejected,
                 "Test 2: pool rejects allocation once exhausted");
}

static void run_double_free_test(void)
{
    br_uart_puts("Test 3: Double-free guard\n");

    br_pool_handle_t h = br_pool_create(pool_buf, sizeof(pool_buf), sizeof(void *));

    void *a = br_pool_alloc(h);
    br_pool_free(h, a);

    br_pool_free(h, a);

    bool available_stable = (br_pool_available(h) == POOL_BLOCKS);

    void *b = br_pool_alloc(h);
    void *c = br_pool_alloc(h);
    bool no_aliasing = (b != NULL) && (c != NULL) && (b != c);

    br_pool_free(h, b);
    br_pool_free(h, c);

    print_result(available_stable && no_aliasing,
                 "Test 3: double-free is ignored instead of aliasing blocks");
}

int main(void)
{
    br_uart_puts("\nbedrock[RTOS] - Memory Pool Test\n");
    br_uart_puts("\n=== Memory Pool Test ===\n\n");

    run_alloc_free_test();
    run_exhaustion_test();
    run_double_free_test();

    br_uart_puts("\n=== All Tests Complete ===\n");

    return 0;
}

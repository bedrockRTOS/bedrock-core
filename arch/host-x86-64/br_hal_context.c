/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Host x86-64 context switch HAL.
 *
 * Uses POSIX ucontext_t for task context switching.  Each task gets an
 * execution stack in a statically allocated host_slot_t; the stack buffer
 * passed by the caller is only used for the stack-canary check.
 *
 * Slot association is keyed on stack_top so that deleted-and-recreated
 * tasks that reuse the same stack buffer reuse the same slot (necessary
 * for br_task_delete + recreate semantics in tests).
 */

#include "bedrock/br_hal.h"

#include <ucontext.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HOST_EXEC_STACK_SIZE (128u * 1024u)

typedef struct {
    ucontext_t      uc;
    br_task_entry_t entry;
    void           *arg;
    void           *stack_top_key;
    bool            in_use;
    char            exec_stack[HOST_EXEC_STACK_SIZE];
} host_slot_t;

static host_slot_t g_slots[CONFIG_MAX_TASKS];

static host_slot_t *alloc_slot(void *stack_top)
{
    for (int i = 0; i < CONFIG_MAX_TASKS; i++) {
        if (g_slots[i].in_use && g_slots[i].stack_top_key == stack_top) {
            return &g_slots[i];
        }
    }
    for (int i = 0; i < CONFIG_MAX_TASKS; i++) {
        if (!g_slots[i].in_use) {
            g_slots[i].in_use        = true;
            g_slots[i].stack_top_key = stack_top;
            return &g_slots[i];
        }
    }
    return NULL;
}

static void task_trampoline(uint32_t ptr_hi, uint32_t ptr_lo)
{
    host_slot_t *slot = (host_slot_t *)((uint64_t)ptr_hi << 32 | (uint64_t)ptr_lo);
    slot->entry(slot->arg);
    while (1) { }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
__attribute__((noinline))
static void setup_context(host_slot_t *slot)
{
    getcontext(&slot->uc);
    sigemptyset(&slot->uc.uc_sigmask);
    slot->uc.uc_link          = NULL;
    slot->uc.uc_stack.ss_sp   = slot->exec_stack;
    slot->uc.uc_stack.ss_size = sizeof(slot->exec_stack);

    uint64_t ptr = (uint64_t)(uintptr_t)slot;
    makecontext(&slot->uc, (void (*)(void))task_trampoline, 2,
                (uint32_t)(ptr >> 32), (uint32_t)(ptr & 0xFFFFFFFFu));
}
#pragma GCC diagnostic pop

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg)
{
    host_slot_t *slot = alloc_slot(stack_top);
    if (slot == NULL) {
        return NULL;
    }

    slot->entry = entry;
    slot->arg   = arg;
    setup_context(slot);

    return slot;
}

void br_hal_context_switch(void **old_sp, void **new_sp)
{
    host_slot_t *old_slot = *(host_slot_t **)old_sp;
    host_slot_t *new_slot = *(host_slot_t **)new_sp;
    swapcontext(&old_slot->uc, &new_slot->uc);
}

void br_hal_start_first_task(void *sp)
{
    host_slot_t *slot = (host_slot_t *)sp;
    setcontext(&slot->uc);
    __builtin_unreachable();
}

void br_hal_check_stack_overflow(br_tcb_t *tcb)
{
    if (tcb == NULL || tcb->stack_canary == NULL) {
        return;
    }
    if (*tcb->stack_canary != BR_STACK_CANARY) {
        br_hal_panic("Stack overflow detected", __FILE__, __LINE__);
    }
}

void br_hal_board_init(void)
{
}

void br_hal_panic(const char *msg, const char *file, int line)
{
    fprintf(stderr, "\n*** KERNEL PANIC ***\n");
    if (msg) {
        fprintf(stderr, "Reason: %s\n", msg);
    }
    if (file) {
        fprintf(stderr, "File:   %s:%d\n", file, line);
    }
    fprintf(stderr, "********************\n");
    fflush(stderr);
    abort();
}

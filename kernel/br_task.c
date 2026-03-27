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
 */

#include "bedrock/bedrock.h"

extern void     br_sched_init(void);
extern void     br_sched_ready(br_tcb_t *tcb);
extern void     br_sched_unready(br_tcb_t *tcb);
extern void     br_sched_reschedule(void);
extern void     br_sched_start(void);
extern br_tcb_t *br_sched_current(void);
extern void     br_time_sleep_list_remove(br_tcb_t *tcb);

/* Static TCB pool (zero dynamic memory) */
static br_tcb_t tcb_pool[CONFIG_MAX_TASKS];

static uint8_t idle_stack[CONFIG_DEFAULT_STACK_SIZE];

static void idle_entry(void *arg)
{
    (void)arg;
    while (1) { }
}

void br_kernel_init(void)
{
    for (int i = 0; i < CONFIG_MAX_TASKS; i++) {
        tcb_pool[i].state = BR_TASK_INACTIVE;
        tcb_pool[i].id    = (br_tid_t)i;
    }

    br_hal_board_init();
    br_hal_timer_init();
    br_sched_init();

    br_tid_t idle_tid;
    br_err_t err = br_task_create(&idle_tid, "idle", idle_entry, NULL,
                                   CONFIG_NUM_PRIORITIES - 1,
                                   idle_stack, sizeof(idle_stack));
    if (err != BR_OK) {
        while (1) { } /* Fatal: cannot create idle task */
    }
}

void br_kernel_start(void)
{
    br_sched_start();
    __builtin_unreachable();
}

br_err_t br_task_create(br_tid_t *tid,
                        const char *name,
                        br_task_entry_t entry,
                        void *arg,
                        uint8_t priority,
                        void *stack,
                        size_t stack_size)
{
    if (entry == NULL || stack == NULL || stack_size == 0) {
        return BR_ERR_INVALID;
    }
    if (priority >= CONFIG_NUM_PRIORITIES) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    /* Find first free TCB slot */
    br_tcb_t *tcb = NULL;
    for (int i = 0; i < CONFIG_MAX_TASKS; i++) {
        if (tcb_pool[i].state == BR_TASK_INACTIVE) {
            tcb = &tcb_pool[i];
            break;
        }
    }

    if (tcb == NULL) {
        br_hal_irq_restore(key);
        return BR_ERR_NOMEM;
    }
    tcb->name        = name;
    tcb->entry       = entry;
    tcb->arg         = arg;
    tcb->priority    = priority;
    tcb->stack_base  = stack;
    tcb->stack_size  = stack_size;
    tcb->wake_time   = 0;
    tcb->rr_remaining = 0;
    tcb->next        = NULL;

    /* Place canary at the bottom of the stack (lowest address) */
    tcb->stack_canary = (uint32_t *)stack;
    *(tcb->stack_canary) = BR_STACK_CANARY;

    void *stack_top = (uint8_t *)stack + stack_size;
    tcb->sp = br_hal_stack_init(stack_top, entry, arg);

    if (tid != NULL) {
        *tid = tcb->id;
    }

    br_hal_irq_restore(key);
    br_sched_ready(tcb);

    return BR_OK;
}

br_err_t br_task_suspend(br_tid_t tid)
{
    if (tid >= CONFIG_MAX_TASKS) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *tcb = &tcb_pool[tid];

    if (tcb->state == BR_TASK_INACTIVE) {
        br_hal_irq_restore(key);
        return BR_ERR_INVALID;
    }

    if (tcb->state == BR_TASK_READY) {
        br_sched_unready(tcb);
    }

    tcb->state = BR_TASK_SUSPENDED;

    br_hal_irq_restore(key);

    if (tcb == br_sched_current()) {
        br_sched_reschedule();
    }

    return BR_OK;
}

br_err_t br_task_resume(br_tid_t tid)
{
    if (tid >= CONFIG_MAX_TASKS) {
        return BR_ERR_INVALID;
    }

    br_tcb_t *tcb = &tcb_pool[tid];

    if (tcb->state == BR_TASK_INACTIVE || tcb->state != BR_TASK_SUSPENDED) {
        return BR_ERR_INVALID;
    }

    br_sched_ready(tcb);
    br_sched_reschedule();

    return BR_OK;
}

br_err_t br_task_delete(br_tid_t tid)
{
    if (tid >= CONFIG_MAX_TASKS) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *tcb = &tcb_pool[tid];

    /* Cannot delete inactive task */
    if (tcb->state == BR_TASK_INACTIVE) {
        br_hal_irq_restore(key);
        return BR_ERR_INVALID;
    }

    /* Cannot delete currently running task (use br_task_yield + delete from another task) */
    if (tcb == br_sched_current()) {
        br_hal_irq_restore(key);
        return BR_ERR_INVALID;
    }

    if (tcb->state == BR_TASK_READY) {
        br_sched_unready(tcb);
    } else if (tcb->state == BR_TASK_BLOCKED) {
        br_time_sleep_list_remove(tcb);
    }

    /* Clear TCB and mark as inactive (returns slot to pool) */
    tcb->state = BR_TASK_INACTIVE;
    tcb->name = NULL;
    tcb->entry = NULL;
    tcb->arg = NULL;
    tcb->stack_base = NULL;
    tcb->stack_size = 0;
    tcb->stack_canary = NULL;
    tcb->sp = NULL;
    tcb->next = NULL;

    br_hal_irq_restore(key);

    return BR_OK;
}

br_tid_t br_task_self(void)
{
    br_tcb_t *cur = br_sched_current();
    return cur ? cur->id : 0;
}

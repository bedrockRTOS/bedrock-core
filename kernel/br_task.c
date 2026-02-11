/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.
 */

#include "bedrock/bedrock.h"

extern void     br_sched_init(void);
extern void     br_sched_ready(br_tcb_t *tcb);
extern void     br_sched_unready(br_tcb_t *tcb);
extern void     br_sched_reschedule(void);
extern void     br_sched_start(void);
extern br_tcb_t *br_sched_current(void);

/* Static TCB pool (zero dynamic memory) */
static br_tcb_t tcb_pool[CONFIG_MAX_TASKS];
static uint8_t  tcb_used;

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
    tcb_used = 0;

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

    if (tcb_used >= CONFIG_MAX_TASKS) {
        br_hal_irq_restore(key);
        return BR_ERR_NOMEM;
    }

    br_tcb_t *tcb    = &tcb_pool[tcb_used];
    tcb->id          = tcb_used;
    tcb->name        = name;
    tcb->entry       = entry;
    tcb->arg         = arg;
    tcb->priority    = priority;
    tcb->stack_base  = stack;
    tcb->stack_size  = stack_size;
    tcb->wake_time   = 0;
    tcb->rr_remaining = 0;
    tcb->next        = NULL;

    void *stack_top = (uint8_t *)stack + stack_size;
    tcb->sp = br_hal_stack_init(stack_top, entry, arg);

    tcb_used++;

    if (tid != NULL) {
        *tid = tcb->id;
    }

    br_hal_irq_restore(key);
    br_sched_ready(tcb);

    return BR_OK;
}

br_err_t br_task_suspend(br_tid_t tid)
{
    if (tid >= tcb_used) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *tcb = &tcb_pool[tid];

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
    if (tid >= tcb_used) {
        return BR_ERR_INVALID;
    }

    br_tcb_t *tcb = &tcb_pool[tid];

    if (tcb->state != BR_TASK_SUSPENDED) {
        return BR_ERR_INVALID;
    }

    br_sched_ready(tcb);
    br_sched_reschedule();

    return BR_OK;
}

br_tid_t br_task_self(void)
{
    br_tcb_t *cur = br_sched_current();
    return cur ? cur->id : 0;
}

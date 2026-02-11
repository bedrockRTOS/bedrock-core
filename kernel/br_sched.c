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

/* Per-priority ready queue heads (singly linked list) */
static br_tcb_t *ready_queue[CONFIG_NUM_PRIORITIES];

/* Pointer to the currently running task */
static br_tcb_t *current_task;

/* Scheduler lock depth (for nested critical sections) */
static volatile uint32_t sched_lock;

void br_sched_init(void)
{
    for (int i = 0; i < CONFIG_NUM_PRIORITIES; i++) {
        ready_queue[i] = NULL;
    }
    current_task = NULL;
    sched_lock = 0;
}

/* Insert a task into its priority-level ready queue (tail) */
void br_sched_ready(br_tcb_t *tcb)
{
    uint32_t key = br_hal_irq_disable();

    tcb->state = BR_TASK_READY;
    tcb->next  = NULL;

    uint8_t prio = tcb->priority;

    if (ready_queue[prio] == NULL) {
        ready_queue[prio] = tcb;
    } else {
        br_tcb_t *tail = ready_queue[prio];
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = tcb;
    }

    br_hal_irq_restore(key);
}

/* Remove a specific task from its ready queue */
void br_sched_unready(br_tcb_t *tcb)
{
    uint32_t key = br_hal_irq_disable();

    uint8_t prio = tcb->priority;
    br_tcb_t **pp = &ready_queue[prio];

    while (*pp != NULL) {
        if (*pp == tcb) {
            *pp = tcb->next;
            tcb->next = NULL;
            break;
        }
        pp = &((*pp)->next);
    }

    br_hal_irq_restore(key);
}

/* Find the highest-priority ready task */
static br_tcb_t *pick_next(void)
{
    for (int prio = 0; prio < CONFIG_NUM_PRIORITIES; prio++) {
        if (ready_queue[prio] != NULL) {
            br_tcb_t *tcb = ready_queue[prio];
            ready_queue[prio] = tcb->next;
            tcb->next = NULL;
            return tcb;
        }
    }
    return NULL;
}

/* Forward declarations */
void br_sched_reschedule(void);

void br_sched_lock(void)
{
    uint32_t key = br_hal_irq_disable();
    sched_lock++;
    br_hal_irq_restore(key);
}

void br_sched_unlock(void)
{
    uint32_t key = br_hal_irq_disable();
    if (sched_lock > 0) {
        sched_lock--;
    }
    br_hal_irq_restore(key);
    if (sched_lock == 0) {
        br_sched_reschedule();
    }
}

br_tcb_t *br_sched_current(void)
{
    return current_task;
}

void br_sched_set_current(br_tcb_t *tcb)
{
    current_task = tcb;
}

void br_sched_reschedule(void)
{
    if (sched_lock > 0) {
        return;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *next = pick_next();
    if (next == NULL || next == current_task) {
        if (next != NULL) {
            br_sched_ready(next);
        }
        br_hal_irq_restore(key);
        return;
    }

    br_tcb_t *prev = current_task;

    if (prev != NULL && prev->state == BR_TASK_RUNNING) {
        prev->state = BR_TASK_READY;
        br_sched_ready(prev);
    }

    next->state  = BR_TASK_RUNNING;
    current_task = next;

    if (prev != NULL) {
        /* Pend PendSV while IRQs are still disabled.
         * PendSV runs at lowest priority, so the actual switch
         * happens after br_hal_irq_restore() re-enables IRQs. */
        br_hal_context_switch(&prev->sp, &next->sp);
    }

    br_hal_irq_restore(key);
}

void br_task_yield(void)
{
    br_sched_reschedule();
}

void br_sched_start(void)
{
    br_tcb_t *first = pick_next();
    if (first == NULL) {
        while (1) { }
    }

    first->state = BR_TASK_RUNNING;
    current_task = first;

    br_hal_start_first_task(first->sp);
}

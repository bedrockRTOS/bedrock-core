/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
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

    /* Check stack overflow on outgoing task before context switch */
    if (prev != NULL) {
        br_hal_check_stack_overflow(prev);
    }

    if (prev != NULL && prev->state == BR_TASK_RUNNING) {
        prev->state = BR_TASK_READY;
        prev->rr_remaining = 0;  /* Reset time slice when preempted */
        br_sched_ready(prev);
    }

    next->state  = BR_TASK_RUNNING;
    next->rr_remaining = CONFIG_RR_TIME_SLICE_US;  /* Initialize time slice */
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
    first->rr_remaining = CONFIG_RR_TIME_SLICE_US;
    current_task = first;

    br_hal_start_first_task(first->sp);
}

/* Called from timer ISR to handle round-robin time slicing */
void br_sched_tick(br_time_t elapsed_us)
{
    if (current_task == NULL) {
        return;
    }

    /* Decrement remaining time slice */
    if (current_task->rr_remaining > elapsed_us) {
        current_task->rr_remaining -= elapsed_us;
    } else {
        current_task->rr_remaining = 0;
        
        /* Time slice expired - check if there are other tasks at same priority */
        uint8_t prio = current_task->priority;
        if (ready_queue[prio] != NULL) {
            /* There are other ready tasks at this priority - preempt */
            br_sched_reschedule();
        } else {
            /* No other tasks at this priority - renew time slice */
            current_task->rr_remaining = CONFIG_RR_TIME_SLICE_US;
        }
    }
}

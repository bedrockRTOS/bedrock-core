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
#include <string.h>

extern void     br_sched_ready(br_tcb_t *tcb);
extern void     br_sched_reschedule(void);
extern br_tcb_t *br_sched_current(void);
extern void     br_time_sleep_list_insert(br_tcb_t *tcb);
extern void     br_time_sleep_list_remove(br_tcb_t *tcb);

/* Wait queue helpers */

static void wq_insert(br_tcb_t **head, br_tcb_t *tcb)
{
    tcb->next = NULL;
    if (*head == NULL) {
        *head = tcb;
        return;
    }
    br_tcb_t **pp = head;
    while (*pp != NULL && (*pp)->priority <= tcb->priority) {
        pp = &((*pp)->next);
    }
    tcb->next = *pp;
    *pp = tcb;
}

static br_tcb_t *wq_pop(br_tcb_t **head)
{
    if (*head == NULL) {
        return NULL;
    }
    br_tcb_t *tcb = *head;
    *head = tcb->next;
    tcb->next = NULL;
    return tcb;
}

static void wq_remove(br_tcb_t **head, br_tcb_t *tcb)
{
    br_tcb_t **pp = head;
    while (*pp != NULL) {
        if (*pp == tcb) {
            *pp = tcb->next;
            tcb->next = NULL;
            return;
        }
        pp = &((*pp)->next);
    }
}

/*
 * Block the current task on a wait queue with optional timeout.
 * If timeout == BR_TIME_INFINITE, wait forever.
 * Otherwise, also insert into the sleep list so the alarm handler
 * can wake us with BR_ERR_TIMEOUT.
 *
 * Must be called with IRQs disabled. Caller must call
 * br_hal_irq_restore + br_sched_reschedule after this returns.
 */
static void block_on_wq(br_tcb_t **wq, br_tcb_t *tcb, br_time_t timeout)
{
    tcb->state       = BR_TASK_BLOCKED;
    tcb->wait_result = BR_OK;
    wq_insert(wq, tcb);

    if (timeout != BR_TIME_INFINITE) {
        tcb->wake_time = br_hal_timer_get_us() + timeout;
        br_time_sleep_list_insert(tcb);
    }
}

/*
 * Wake a waiter from a wait queue (called by give/unlock/send/recv).
 * Removes from sleep list if it was there, sets wait_result = BR_OK.
 */
static void wake_waiter(br_tcb_t *tcb)
{
    tcb->wait_result = BR_OK;
    br_time_sleep_list_remove(tcb);
    tcb->wake_time = 0;
    br_sched_ready(tcb);
}

/* Semaphore */

br_err_t br_sem_init(br_sem_t *sem, int32_t initial, int32_t max)
{
    if (sem == NULL || initial < 0 || max < 1 || initial > max) {
        return BR_ERR_INVALID;
    }
    sem->count      = initial;
    sem->max_count  = max;
    sem->wait_queue = NULL;
    return BR_OK;
}

br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout)
{
    if (sem == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    if (sem->count > 0) {
        sem->count--;
        br_hal_irq_restore(key);
        return BR_OK;
    }

    if (timeout == 0) {
        br_hal_irq_restore(key);
        return BR_ERR_TIMEOUT;
    }

    br_tcb_t *tcb = br_sched_current();
    block_on_wq(&sem->wait_queue, tcb, timeout);

    br_hal_irq_restore(key);
    br_sched_reschedule();

    /* Back from block: check why we woke up */
    if (tcb->wait_result == BR_ERR_TIMEOUT) {
        /* Woken by timer — remove ourselves from the wait queue */
        uint32_t k2 = br_hal_irq_disable();
        wq_remove(&sem->wait_queue, tcb);
        br_hal_irq_restore(k2);
        return BR_ERR_TIMEOUT;
    }

    return BR_OK;
}

br_err_t br_sem_give(br_sem_t *sem)
{
    if (sem == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *waiter = wq_pop(&sem->wait_queue);
    if (waiter != NULL) {
        wake_waiter(waiter);
        br_hal_irq_restore(key);
        br_sched_reschedule();
        return BR_OK;
    }

    if (sem->count < sem->max_count) {
        sem->count++;
        br_hal_irq_restore(key);
        return BR_OK;
    }

    br_hal_irq_restore(key);
    return BR_ERR_OVERFLOW;
}

/* Mutex (with priority inheritance) */

br_err_t br_mutex_init(br_mutex_t *mtx)
{
    if (mtx == NULL) {
        return BR_ERR_INVALID;
    }
    mtx->locked          = false;
    mtx->owner           = NULL;
    mtx->owner_orig_prio = 0;
    mtx->wait_queue      = NULL;
    return BR_OK;
}

br_err_t br_mutex_lock(br_mutex_t *mtx, br_time_t timeout)
{
    if (mtx == NULL) {
        return BR_ERR_INVALID;
    }
    if (br_hal_in_isr()) {
        return BR_ERR_ISR;
    }

    uint32_t key = br_hal_irq_disable();

    if (!mtx->locked) {
        mtx->locked          = true;
        mtx->owner           = br_sched_current();
        mtx->owner_orig_prio = mtx->owner->priority;
        br_hal_irq_restore(key);
        return BR_OK;
    }

    if (timeout == 0) {
        br_hal_irq_restore(key);
        return BR_ERR_TIMEOUT;
    }

    br_tcb_t *tcb = br_sched_current();

    if (tcb->priority < mtx->owner->priority) {
        mtx->owner->priority = tcb->priority;
    }

    block_on_wq(&mtx->wait_queue, tcb, timeout);

    br_hal_irq_restore(key);
    br_sched_reschedule();

    if (tcb->wait_result == BR_ERR_TIMEOUT) {
        uint32_t k2 = br_hal_irq_disable();
        wq_remove(&mtx->wait_queue, tcb);
        br_hal_irq_restore(k2);
        return BR_ERR_TIMEOUT;
    }

    return BR_OK;
}

br_err_t br_mutex_unlock(br_mutex_t *mtx)
{
    if (mtx == NULL) {
        return BR_ERR_INVALID;
    }
    if (br_hal_in_isr()) {
        return BR_ERR_ISR;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *cur = br_sched_current();
    if (mtx->owner != cur) {
        br_hal_irq_restore(key);
        return BR_ERR_INVALID;
    }

    cur->priority = mtx->owner_orig_prio;

    br_tcb_t *waiter = wq_pop(&mtx->wait_queue);
    if (waiter != NULL) {
        mtx->owner           = waiter;
        mtx->owner_orig_prio = waiter->priority;
        wake_waiter(waiter);
        br_hal_irq_restore(key);
        br_sched_reschedule();
        return BR_OK;
    }

    mtx->locked = false;
    mtx->owner  = NULL;

    br_hal_irq_restore(key);
    return BR_OK;
}

/* Message Queue */

br_err_t br_mqueue_init(br_mqueue_t *mq, void *buffer,
                        size_t msg_size, size_t max_msgs)
{
    if (mq == NULL || buffer == NULL || msg_size == 0 || max_msgs == 0) {
        return BR_ERR_INVALID;
    }
    mq->buffer    = (uint8_t *)buffer;
    mq->msg_size  = msg_size;
    mq->max_msgs  = max_msgs;
    mq->count     = 0;
    mq->head      = 0;
    mq->tail      = 0;
    mq->send_wait = NULL;
    mq->recv_wait = NULL;
    return BR_OK;
}

br_err_t br_mqueue_send(br_mqueue_t *mq, const void *msg, br_time_t timeout)
{
    if (mq == NULL || msg == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    if (mq->count < mq->max_msgs) {
        uint8_t *dst = mq->buffer + (mq->tail * mq->msg_size);
        memcpy(dst, msg, mq->msg_size);
        mq->tail = (mq->tail + 1) % mq->max_msgs;
        mq->count++;

        /* Wake one receiver if any */
        br_tcb_t *waiter = wq_pop(&mq->recv_wait);
        if (waiter != NULL) {
            wake_waiter(waiter);
            br_hal_irq_restore(key);
            br_sched_reschedule();
            return BR_OK;
        }

        br_hal_irq_restore(key);
        return BR_OK;
    }

    if (timeout == 0) {
        br_hal_irq_restore(key);
        return BR_ERR_TIMEOUT;
    }

    br_tcb_t *tcb = br_sched_current();
    block_on_wq(&mq->send_wait, tcb, timeout);

    br_hal_irq_restore(key);
    br_sched_reschedule();

    if (tcb->wait_result == BR_ERR_TIMEOUT) {
        uint32_t k2 = br_hal_irq_disable();
        wq_remove(&mq->send_wait, tcb);
        br_hal_irq_restore(k2);
        return BR_ERR_TIMEOUT;
    }

    /* Woken by recv — now enqueue the message */
    key = br_hal_irq_disable();
    if (mq->count < mq->max_msgs) {
        uint8_t *dst = mq->buffer + (mq->tail * mq->msg_size);
        memcpy(dst, msg, mq->msg_size);
        mq->tail = (mq->tail + 1) % mq->max_msgs;
        mq->count++;
    }
    br_hal_irq_restore(key);

    return BR_OK;
}

br_err_t br_mqueue_recv(br_mqueue_t *mq, void *msg, br_time_t timeout)
{
    if (mq == NULL || msg == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    if (mq->count > 0) {
        uint8_t *src = mq->buffer + (mq->head * mq->msg_size);
        memcpy(msg, src, mq->msg_size);
        mq->head = (mq->head + 1) % mq->max_msgs;
        mq->count--;

        /* Wake one sender if any */
        br_tcb_t *sender = wq_pop(&mq->send_wait);
        if (sender != NULL) {
            wake_waiter(sender);
            br_hal_irq_restore(key);
            br_sched_reschedule();
            return BR_OK;
        }

        br_hal_irq_restore(key);
        return BR_OK;
    }

    if (timeout == 0) {
        br_hal_irq_restore(key);
        return BR_ERR_TIMEOUT;
    }

    br_tcb_t *tcb = br_sched_current();
    block_on_wq(&mq->recv_wait, tcb, timeout);

    br_hal_irq_restore(key);
    br_sched_reschedule();

    if (tcb->wait_result == BR_ERR_TIMEOUT) {
        uint32_t k2 = br_hal_irq_disable();
        wq_remove(&mq->recv_wait, tcb);
        br_hal_irq_restore(k2);
        return BR_ERR_TIMEOUT;
    }

    /* Woken by send — now dequeue the message */
    key = br_hal_irq_disable();
    if (mq->count > 0) {
        uint8_t *src = mq->buffer + (mq->head * mq->msg_size);
        memcpy(msg, src, mq->msg_size);
        mq->head = (mq->head + 1) % mq->max_msgs;
        mq->count--;
    }
    br_hal_irq_restore(key);

    return BR_OK;
}

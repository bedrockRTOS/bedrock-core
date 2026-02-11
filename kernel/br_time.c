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

extern void     br_sched_ready(br_tcb_t *tcb);
extern void     br_sched_unready(br_tcb_t *tcb);
extern void     br_sched_reschedule(void);
extern br_tcb_t *br_sched_current(void);

/* Sleep list -- sorted by wake_time (ascending) */
static br_tcb_t *sleep_list;

void br_time_sleep_list_insert(br_tcb_t *tcb)
{
    br_tcb_t **pp = &sleep_list;
    while (*pp != NULL && (*pp)->wake_time <= tcb->wake_time) {
        pp = &((*pp)->next);
    }
    tcb->next = *pp;
    *pp = tcb;
}

void br_time_sleep_list_remove(br_tcb_t *tcb)
{
    br_tcb_t **pp = &sleep_list;
    while (*pp != NULL) {
        if (*pp == tcb) {
            *pp = tcb->next;
            tcb->next = NULL;
            break;
        }
        pp = &((*pp)->next);
    }
}

static void reprogram_alarm(void)
{
    if (sleep_list != NULL) {
        br_hal_timer_set_alarm(sleep_list->wake_time);
    } else {
        br_hal_timer_cancel_alarm();
    }
}

br_time_t br_uptime_us(void)
{
    return br_hal_timer_get_us();
}

void br_sleep_us(br_time_t us)
{
    if (us == 0) {
        br_task_yield();
        return;
    }

    uint32_t key = br_hal_irq_disable();

    br_tcb_t *tcb = br_sched_current();
    tcb->state     = BR_TASK_BLOCKED;
    tcb->wake_time = br_hal_timer_get_us() + us;

    br_time_sleep_list_insert(tcb);
    reprogram_alarm();

    br_hal_irq_restore(key);
    br_sched_reschedule();
}

void br_time_alarm_handler(void)
{
    br_time_t now = br_hal_timer_get_us();

    while (sleep_list != NULL && sleep_list->wake_time <= now) {
        br_tcb_t *tcb = sleep_list;
        sleep_list = tcb->next;
        tcb->next = NULL;
        tcb->wake_time = 0;
        tcb->wait_result = BR_ERR_TIMEOUT;
        br_sched_ready(tcb);
    }

    reprogram_alarm();
    br_sched_reschedule();
}

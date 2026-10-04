/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define TIMER_ALARM0    (*(volatile uint32_t *)0x40054010UL)
#define TIMER_TIMERAWH  (*(volatile uint32_t *)0x40054024UL)
#define TIMER_TIMERAWL  (*(volatile uint32_t *)0x40054028UL)
#define TIMER_INTR      (*(volatile uint32_t *)0x40054034UL)
#define TIMER_INTE      (*(volatile uint32_t *)0x40054038UL)

#define NVIC_ISER       (*(volatile uint32_t *)0xE000E100UL)
#define NVIC_ISPR       (*(volatile uint32_t *)0xE000E200UL)
#define SCB_ICSR        (*(volatile uint32_t *)0xE000ED04UL)

#define TIMER_IRQ_0     0
#define ALARM_MAX_US    0x7FFFFFFFUL

static volatile br_time_t last_tick_us;
static volatile br_time_t next_tick_us;
static volatile br_time_t alarm_target;
static volatile bool      alarm_pending;

extern void br_time_alarm_handler(void);
extern void br_sched_tick(br_time_t elapsed_us);

static void program_next_irq(void)
{
    br_time_t target = next_tick_us;

    if (alarm_pending && alarm_target < target) {
        target = alarm_target;
    }

    br_time_t now = br_hal_timer_get_us();
    if (target > now + ALARM_MAX_US) {
        target = now + ALARM_MAX_US;
    }

    TIMER_ALARM0 = (uint32_t)target;

    if ((int32_t)((uint32_t)target - TIMER_TIMERAWL) <= 0) {
        NVIC_ISPR = 1UL << TIMER_IRQ_0;
    }
}

void br_hal_timer_init(void)
{
    uint32_t key = br_hal_irq_disable();

    alarm_pending = false;
    alarm_target  = 0;
    last_tick_us  = br_hal_timer_get_us();
    next_tick_us  = last_tick_us + CONFIG_RR_TIME_SLICE_US;

    TIMER_INTE = 1UL << 0;
    NVIC_ISER  = 1UL << TIMER_IRQ_0;
    program_next_irq();

    br_hal_irq_restore(key);
}

br_time_t br_hal_timer_get_us(void)
{
    uint32_t hi;
    uint32_t lo;

    do {
        hi = TIMER_TIMERAWH;
        lo = TIMER_TIMERAWL;
    } while (hi != TIMER_TIMERAWH);

    return ((uint64_t)hi << 32) | lo;
}

void br_hal_timer_set_alarm(br_time_t abs_us)
{
    uint32_t key = br_hal_irq_disable();

    alarm_target  = abs_us;
    alarm_pending = true;
    program_next_irq();

    br_hal_irq_restore(key);
}

void br_hal_timer_cancel_alarm(void)
{
    uint32_t key = br_hal_irq_disable();

    alarm_pending = false;
    program_next_irq();

    br_hal_irq_restore(key);
}

void TIMER_IRQ_0_Handler(void)
{
    TIMER_INTR = 1UL << 0;

    br_time_t now = br_hal_timer_get_us();

    if (now >= next_tick_us) {
        br_time_t elapsed = now - last_tick_us;
        last_tick_us = now;
        next_tick_us = now + CONFIG_RR_TIME_SLICE_US;
        br_sched_tick(elapsed);
    }

    if (alarm_pending && now >= alarm_target) {
        alarm_pending = false;
        br_time_alarm_handler();
    }

    program_next_irq();
}

uint32_t br_hal_irq_disable(void)
{
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask));
    __asm volatile ("cpsid i" ::: "memory");
    return primask;
}

void br_hal_irq_restore(uint32_t state)
{
    __asm volatile ("msr primask, %0" :: "r" (state) : "memory");
}

bool br_hal_in_isr(void)
{
    return (SCB_ICSR & 0x1FF) != 0;
}

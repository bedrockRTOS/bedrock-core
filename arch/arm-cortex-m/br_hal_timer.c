/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018)

#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)

#ifndef BR_HAL_SYS_CLOCK_HZ
#define BR_HAL_SYS_CLOCK_HZ  16000000UL
#endif

#define TICKS_PER_US  (BR_HAL_SYS_CLOCK_HZ / 1000000UL)

#if TICKS_PER_US == 0
#error "BR_HAL_SYS_CLOCK_HZ must be >= 1MHz for microsecond timer resolution"
#endif

#define ICSR_PENDSTSET  (1UL << 26)
#define ICSR_PENDSTCLR  (1UL << 25)

#define SYST_MAX_TICKS  0x01000000UL
#define SYST_MIN_TICKS  (TICKS_PER_US * 10UL)

static volatile uint64_t  base_ticks;
static volatile br_time_t last_tick_us;
static volatile br_time_t next_tick_us;
static volatile br_time_t alarm_target;
static volatile bool      alarm_pending;

extern void br_time_alarm_handler(void);
extern void br_sched_tick(br_time_t elapsed_us);

static uint64_t current_ticks(void)
{
    uint32_t pending = SCB_ICSR & ICSR_PENDSTSET;
    uint32_t cvr = SYST_CVR;

    if (!pending && (SCB_ICSR & ICSR_PENDSTSET)) {
        pending = 1;
        cvr = SYST_CVR;
    }

    uint32_t rvr = SYST_RVR;
    uint64_t ticks = base_ticks;

    if (pending) {
        ticks += (uint64_t)rvr + 1;
    }
    if (cvr != 0) {
        ticks += rvr - cvr;
    }
    return ticks;
}

static void program_next_irq(void)
{
    br_time_t target = next_tick_us;

    if (alarm_pending && alarm_target < target) {
        target = alarm_target;
    }

    uint64_t now = current_ticks();
    uint64_t target_ticks = target * TICKS_PER_US;
    uint64_t delta = SYST_MIN_TICKS;

    if (target_ticks > now + SYST_MIN_TICKS) {
        delta = target_ticks - now;
    }
    if (delta > SYST_MAX_TICKS) {
        delta = SYST_MAX_TICKS;
    }

    base_ticks = now;
    SYST_RVR = (uint32_t)delta - 1;
    SYST_CVR = 0;
    SCB_ICSR = ICSR_PENDSTCLR;
}

void br_hal_timer_init(void)
{
    uint32_t key = br_hal_irq_disable();

    base_ticks    = 0;
    alarm_target  = 0;
    alarm_pending = false;
    last_tick_us  = 0;
    next_tick_us  = CONFIG_RR_TIME_SLICE_US;

    program_next_irq();
    SYST_CSR = 0x07;

    br_hal_irq_restore(key);
}

br_time_t br_hal_timer_get_us(void)
{
    uint32_t key = br_hal_irq_disable();
    uint64_t ticks = current_ticks();
    br_hal_irq_restore(key);

    return ticks / TICKS_PER_US;
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

void SysTick_Handler(void)
{
    base_ticks += (uint64_t)SYST_RVR + 1;

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
    uint32_t icsr = SCB_ICSR;
    return (icsr & 0x1FF) != 0;
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define CLINT_MTIMECMP_LO  (*(volatile uint32_t *)0x02004000UL)
#define CLINT_MTIMECMP_HI  (*(volatile uint32_t *)0x02004004UL)
#define CLINT_MTIME_LO     (*(volatile uint32_t *)0x0200BFF8UL)
#define CLINT_MTIME_HI     (*(volatile uint32_t *)0x0200BFFCUL)

#define MIE_MTIE           (1UL << 7)

#define TICKS_PER_US       (BR_HAL_SYS_CLOCK_HZ / 1000000UL)

#if TICKS_PER_US == 0
#error "BR_HAL_SYS_CLOCK_HZ must be >= 1MHz for microsecond timer resolution"
#endif

static volatile br_time_t last_tick_us;
static volatile br_time_t next_tick_us;
static volatile br_time_t alarm_target;
static volatile bool      alarm_pending;

extern void br_time_alarm_handler(void);
extern void br_sched_tick(br_time_t elapsed_us);

static uint64_t read_mtime(void)
{
    uint32_t hi;
    uint32_t lo;

    do {
        hi = CLINT_MTIME_HI;
        lo = CLINT_MTIME_LO;
    } while (hi != CLINT_MTIME_HI);

    return ((uint64_t)hi << 32) | lo;
}

static void write_mtimecmp(uint64_t ticks)
{
    CLINT_MTIMECMP_LO = 0xFFFFFFFFUL;
    CLINT_MTIMECMP_HI = (uint32_t)(ticks >> 32);
    CLINT_MTIMECMP_LO = (uint32_t)ticks;
}

static void program_next_irq(void)
{
    br_time_t next = next_tick_us;

    if (alarm_pending && alarm_target < next) {
        next = alarm_target;
    }

    write_mtimecmp(next * TICKS_PER_US);
}

void br_hal_timer_init(void)
{
    alarm_pending = false;
    alarm_target  = 0;
    last_tick_us  = br_hal_timer_get_us();
    next_tick_us  = last_tick_us + CONFIG_RR_TIME_SLICE_US;

    program_next_irq();

    __asm volatile ("csrs mie, %0" :: "r" (MIE_MTIE));
}

br_time_t br_hal_timer_get_us(void)
{
    return read_mtime() / TICKS_PER_US;
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

void br_hal_timer_isr(void)
{
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

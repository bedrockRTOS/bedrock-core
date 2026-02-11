/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ARM Cortex-M timer HAL implementation.
 * Uses SysTick as the time source and a free-running counter
 * to provide 64-bit microsecond timestamps.
 *
 * This is a minimal reference implementation suitable for
 * QEMU cortex-m3 emulation.
 */

#include "bedrock/br_hal.h"

/* Cortex-M SysTick registers */
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018)

/* SCB ICSR -- to check active ISR */
#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)

#ifndef BR_HAL_SYS_CLOCK_HZ
#define BR_HAL_SYS_CLOCK_HZ  16000000UL
#endif

#define TICKS_PER_US  (BR_HAL_SYS_CLOCK_HZ / 1000000UL)

#if TICKS_PER_US == 0
#error "BR_HAL_SYS_CLOCK_HZ must be >= 1MHz for microsecond timer resolution"
#endif

static volatile uint64_t timer_overflow_us;
static volatile uint32_t systick_reload;

static volatile br_time_t alarm_target;
static volatile bool      alarm_pending;

extern void br_time_alarm_handler(void);

void br_hal_timer_init(void)
{
    systick_reload = 0x00FFFFFF;
    SYST_RVR = systick_reload;
    SYST_CVR = 0;
    SYST_CSR = 0x07;

    timer_overflow_us = 0;
    alarm_target  = 0;
    alarm_pending = false;
}

br_time_t br_hal_timer_get_us(void)
{
    uint32_t key = br_hal_irq_disable();

    uint64_t base = timer_overflow_us;
    uint32_t current = systick_reload - SYST_CVR;
    uint32_t us_frac = current / TICKS_PER_US;

    br_hal_irq_restore(key);

    return base + us_frac;
}

void br_hal_timer_set_alarm(br_time_t abs_us)
{
    alarm_target  = abs_us;
    alarm_pending = true;
}

void br_hal_timer_cancel_alarm(void)
{
    alarm_pending = false;
}

void SysTick_Handler(void)
{
    timer_overflow_us += (uint64_t)(systick_reload + 1) / TICKS_PER_US;

    if (alarm_pending) {
        br_time_t now = timer_overflow_us;
        if (now >= alarm_target) {
            alarm_pending = false;
            br_time_alarm_handler();
        }
    }
}

/* Interrupt control */

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

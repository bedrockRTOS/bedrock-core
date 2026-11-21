/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "x86_io.h"

#define PIT_CH0          0x40
#define PIT_CH2          0x42
#define PIT_CMD          0x43
#define PIT_PORT_B       0x61
#define PIT_HZ           1193182UL

#define PIT_CMD_CH0_ONESHOT   0x30
#define PIT_CMD_CH2_ONESHOT   0xB0

#define CALIBRATE_COUNT       59659U
#define CALIBRATE_US          50000U

#define PIT_MAX_US            50000U
#define PIT_MIN_US            10U

static volatile br_time_t last_tick_us;
static volatile br_time_t next_tick_us;
static volatile br_time_t alarm_target;
static volatile bool      alarm_pending;
static uint32_t           tsc_per_us;

extern void br_time_alarm_handler(void);
extern void br_sched_tick(br_time_t elapsed_us);

static void pit_arm(uint16_t count)
{
    outb(PIT_CMD, PIT_CMD_CH0_ONESHOT);
    outb(PIT_CH0, (uint8_t)(count & 0xFF));
    outb(PIT_CH0, (uint8_t)(count >> 8));
}

static void calibrate_tsc(void)
{
    outb(PIT_PORT_B, (uint8_t)((inb(PIT_PORT_B) & ~0x02) | 0x01));
    outb(PIT_CMD, PIT_CMD_CH2_ONESHOT);
    outb(PIT_CH2, (uint8_t)(CALIBRATE_COUNT & 0xFF));
    outb(PIT_CH2, (uint8_t)(CALIBRATE_COUNT >> 8));

    outb(PIT_PORT_B, (uint8_t)(inb(PIT_PORT_B) & ~0x01));
    outb(PIT_PORT_B, (uint8_t)(inb(PIT_PORT_B) | 0x01));

    uint64_t start = rdtsc();
    while (!(inb(PIT_PORT_B) & 0x20)) { }
    uint64_t end = rdtsc();

    tsc_per_us = (uint32_t)((end - start) / CALIBRATE_US);
    if (tsc_per_us == 0) {
        tsc_per_us = 1;
    }
}

static void program_next_irq(void)
{
    br_time_t now  = br_hal_timer_get_us();
    br_time_t next = next_tick_us;

    if (alarm_pending && alarm_target < next) {
        next = alarm_target;
    }

    br_time_t delta = next > now ? next - now : PIT_MIN_US;
    if (delta > PIT_MAX_US) {
        delta = PIT_MAX_US;
    }
    if (delta < PIT_MIN_US) {
        delta = PIT_MIN_US;
    }

    pit_arm((uint16_t)((delta * PIT_HZ) / 1000000UL));
}

void br_hal_timer_init(void)
{
    calibrate_tsc();

    alarm_pending = false;
    alarm_target  = 0;
    last_tick_us  = br_hal_timer_get_us();
    next_tick_us  = last_tick_us + CONFIG_RR_TIME_SLICE_US;

    uint8_t mask = inb(PIC1_DATA);
    outb(PIC1_DATA, (uint8_t)(mask & ~0x01));

    program_next_irq();
}

br_time_t br_hal_timer_get_us(void)
{
    return rdtsc() / tsc_per_us;
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

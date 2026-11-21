/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "bedrock/br_hal.h"
#include "br_avr_isr.h"

/*
 * Timer1 runs at clk/64 and counts the time. The 16-bit counter wraps every
 * 65536 ticks and the overflow interrupt counts the wraps, which makes a
 * 48-bit tick count. A tick is 4 us at 16 MHz. The compare match A
 * interrupt is the one-shot alarm and also the round-robin time slice.
 *
 * The interrupt flags are never written: the hardware clears a flag when it
 * runs the vector. A flag that is left over from a match that has been
 * replaced only gives an interrupt that finds nothing to do.
 */

#if BR_HAL_SYS_CLOCK_HZ == 16000000UL
#  define TICK_SHIFT  2
#elif BR_HAL_SYS_CLOCK_HZ == 8000000UL
#  define TICK_SHIFT  3
#elif BR_HAL_SYS_CLOCK_HZ == 4000000UL
#  define TICK_SHIFT  4
#elif BR_HAL_SYS_CLOCK_HZ == 2000000UL
#  define TICK_SHIFT  5
#elif BR_HAL_SYS_CLOCK_HZ == 1000000UL
#  define TICK_SHIFT  6
#else
#  error "BR_HAL_SYS_CLOCK_HZ must be 1, 2, 4, 8 or 16 MHz on AVR"
#endif

/* The compare match is at least this many ticks ahead, and at most MAX_AHEAD
 * so that it comes before the counter wraps. */
#define MIN_AHEAD  8UL
#define MAX_AHEAD  0xC000UL

extern void br_time_alarm_handler(void);
extern void br_sched_tick(br_time_t elapsed_us);

static volatile uint32_t  wraps;
static volatile br_time_t last_tick_us;
static volatile br_time_t next_tick_us;
static volatile br_time_t alarm_target;
static volatile bool      alarm_pending;

ISR(TIMER1_OVF_vect)
{
    wraps++;
}

static uint64_t read_ticks(void)
{
    uint8_t  sreg = SREG;
    __asm__ volatile ("cli" ::: "memory");

    uint16_t lo = TCNT1;
    uint32_t hi = wraps;
    if ((TIFR1 & _BV(TOV1)) && lo < 0x8000U) {
        hi++;
    }

    SREG = sreg;
    return ((uint64_t)hi << 16) | lo;
}

static void program_next_irq(void)
{
    br_time_t next = next_tick_us;

    if (alarm_pending && alarm_target < next) {
        next = alarm_target;
    }

    uint64_t now    = read_ticks();
    uint64_t target = next >> TICK_SHIFT;

    if (target < now + MIN_AHEAD) {
        target = now + MIN_AHEAD;
    } else if (target > now + MAX_AHEAD) {
        target = now + MAX_AHEAD;
    }

    uint8_t sreg = SREG;
    __asm__ volatile ("cli" ::: "memory");
    OCR1A = (uint16_t)target;
    SREG = sreg;
}

void br_hal_timer_init(void)
{
    alarm_pending = false;
    alarm_target  = 0;
    wraps         = 0;

    TCCR1A = 0;
    TCCR1B = _BV(CS11) | _BV(CS10);
    TCNT1  = 0;
    TIMSK1 = _BV(OCIE1A) | _BV(TOIE1);

    last_tick_us = br_hal_timer_get_us();
    next_tick_us = last_tick_us + CONFIG_RR_TIME_SLICE_US;

    program_next_irq();
}

br_time_t br_hal_timer_get_us(void)
{
    return (br_time_t)read_ticks() << TICK_SHIFT;
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

BR_AVR_KERNEL_ISR(TIMER1_COMPA_vect_num, br_hal_timer_isr);

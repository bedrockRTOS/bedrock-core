/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>

#include "bedrock/br_hal.h"

/*
 * Timer0 in 8-bit fast PWM: channel 0 is OC0A (PD6), channel 1 is OC0B
 * (PD5). Both channels share the counter, so they share the frequency. The
 * frequency is F_CPU / (prescaler * 256) with a prescaler of 1, 8, 64, 256
 * or 1024, the closest one to the request is used.
 */

static const uint16_t prescaler[]  = { 1, 8, 64, 256, 1024 };
static const uint8_t  cs_bits[]    = { 1, 2, 3, 4, 5 };

static uint8_t running;
static uint8_t active_cs;

static int8_t pick_prescaler(uint32_t freq_hz)
{
    int8_t best = -1;
    uint32_t best_err = 0xFFFFFFFFUL;

    for (uint8_t i = 0; i < sizeof(prescaler) / sizeof(prescaler[0]); i++) {
        uint32_t f = BR_HAL_SYS_CLOCK_HZ / ((uint32_t)prescaler[i] * 256UL);
        uint32_t err = f > freq_hz ? f - freq_hz : freq_hz - f;
        if (err < best_err) {
            best_err = err;
            best = (int8_t)i;
        }
    }
    return best;
}

br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille)
{
    if (ch > 1 || freq_hz == 0 || duty_permille > 1000) {
        return BR_ERR_INVALID;
    }

    uint32_t top = BR_HAL_SYS_CLOCK_HZ / 256UL;
    if (freq_hz > top || freq_hz < top / 1024UL) {
        return BR_ERR_INVALID;
    }

    uint8_t cs = cs_bits[pick_prescaler(freq_hz)];
    if (running != 0 && cs != active_cs) {
        return BR_ERR_BUSY;
    }

    uint8_t duty = (uint8_t)(((uint32_t)duty_permille * 255UL + 500UL) / 1000UL);

    if (ch == 0) {
        DDRD |= _BV(PD6);
        OCR0A = duty;
        TCCR0A |= _BV(COM0A1);
    } else {
        DDRD |= _BV(PD5);
        OCR0B = duty;
        TCCR0A |= _BV(COM0B1);
    }
    TCCR0A |= _BV(WGM01) | _BV(WGM00);
    TCCR0B = cs;

    running |= (uint8_t)(1U << ch);
    active_cs = cs;
    return BR_OK;
}

br_err_t br_hal_pwm_stop(uint8_t ch)
{
    if (ch > 1) {
        return BR_ERR_INVALID;
    }

    TCCR0A &= (uint8_t)~(ch == 0 ? _BV(COM0A1) : _BV(COM0B1));
    running &= (uint8_t)~(1U << ch);
    if (running == 0) {
        TCCR0B = 0;
    }
    return BR_OK;
}

/* Timer1 counts the system time, so there is no input capture */

br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge)
{
    (void)ch;
    (void)edge;
    return BR_ERR_INVALID;
}

br_err_t br_hal_capture_stop(uint8_t ch)
{
    (void)ch;
    return BR_ERR_INVALID;
}

br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us)
{
    (void)ch;
    (void)period_us;
    return BR_ERR_INVALID;
}

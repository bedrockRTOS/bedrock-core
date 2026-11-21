/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define HOST_PWM_CHANNELS  4

static struct {
    bool      running;
    uint32_t  freq_hz;
    uint16_t  duty_permille;
} pwm[HOST_PWM_CHANNELS];

static struct {
    bool      running;
    br_time_t last_us;
} capture[HOST_PWM_CHANNELS];

br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille)
{
    if (ch >= HOST_PWM_CHANNELS || freq_hz == 0 || freq_hz > 1000000 ||
        duty_permille > 1000) {
        return BR_ERR_INVALID;
    }
    pwm[ch].freq_hz       = freq_hz;
    pwm[ch].duty_permille = duty_permille;
    pwm[ch].running       = true;
    return BR_OK;
}

br_err_t br_hal_pwm_stop(uint8_t ch)
{
    if (ch >= HOST_PWM_CHANNELS) {
        return BR_ERR_INVALID;
    }
    pwm[ch].running = false;
    return BR_OK;
}

br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge)
{
    if (ch >= HOST_PWM_CHANNELS || edge > BR_CAPTURE_FALLING) {
        return BR_ERR_INVALID;
    }
    capture[ch].running = true;
    capture[ch].last_us = br_hal_timer_get_us();
    return BR_OK;
}

br_err_t br_hal_capture_stop(uint8_t ch)
{
    if (ch >= HOST_PWM_CHANNELS) {
        return BR_ERR_INVALID;
    }
    capture[ch].running = false;
    return BR_OK;
}

br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us)
{
    if (ch >= HOST_PWM_CHANNELS || period_us == NULL || !capture[ch].running) {
        return BR_ERR_INVALID;
    }
    if (!pwm[ch].running) {
        return BR_ERR_BUSY;
    }

    uint32_t period = 1000000U / pwm[ch].freq_hz;
    br_time_t now = br_hal_timer_get_us();

    if (now - capture[ch].last_us < period) {
        return BR_ERR_BUSY;
    }
    capture[ch].last_us = now;
    *period_us = period;
    return BR_OK;
}

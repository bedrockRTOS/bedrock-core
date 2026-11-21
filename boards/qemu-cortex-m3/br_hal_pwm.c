/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "lm3s_regs.h"

#define PWM_BASE        0x40028000UL
#define PWM_ENABLE      LM3S_REG(PWM_BASE + 0x008UL)
#define PWM_GEN(g, off) LM3S_REG(PWM_BASE + 0x040UL + 0x40UL * (g) + (off))
#define PWM_GEN_CTL     0x00UL
#define PWM_GEN_LOAD    0x10UL
#define PWM_GEN_CMPA    0x18UL
#define PWM_GEN_CMPB    0x1CUL
#define PWM_GEN_GENA    0x20UL
#define PWM_GEN_GENB    0x24UL

#define SYSCTL_RCGC0_PWM    (1UL << 20)
#define SYSCTL_RCC_USEPWMDIV (1UL << 20)
#define SYSCTL_RCC_PWMDIV_M  (7UL << 17)
#define SYSCTL_RCC_PWMDIV_16 (3UL << 17)
#define PWM_CLOCK_HZ    (BR_HAL_SYS_CLOCK_HZ / 16UL)

#define GEN_CTL_ENABLE  1UL
#define GEN_ACTLOAD_LOW  (2UL << 2)
#define GEN_ACTLOAD_HIGH (3UL << 2)
#define GENA_ACTCMPAD_LOW (2UL << 6)
#define GENB_ACTCMPBD_LOW (2UL << 10)

#define TIMER_BASE(n)   (0x40030000UL + 0x1000UL * (n))
#define GPT_CFG(n)      LM3S_REG(TIMER_BASE(n) + 0x000UL)
#define GPT_TAMR(n)     LM3S_REG(TIMER_BASE(n) + 0x004UL)
#define GPT_CTL(n)      LM3S_REG(TIMER_BASE(n) + 0x00CUL)
#define GPT_RIS(n)      LM3S_REG(TIMER_BASE(n) + 0x01CUL)
#define GPT_ICR(n)      LM3S_REG(TIMER_BASE(n) + 0x024UL)
#define GPT_TAILR(n)    LM3S_REG(TIMER_BASE(n) + 0x028UL)
#define GPT_TAPR(n)     LM3S_REG(TIMER_BASE(n) + 0x038UL)
#define GPT_TAR(n)      LM3S_REG(TIMER_BASE(n) + 0x048UL)

#define SYSCTL_RCGC1_TIMER(n) (1UL << (16 + (n)))
#define GPT_CFG_16BIT   4UL
#define GPT_TAMR_CAPTURE_EDGE_TIME 0x7UL
#define GPT_CTL_TAEN    (1UL << 0)
#define GPT_CTL_TAEVENT_FALLING (1UL << 2)
#define GPT_RIS_CAE     (1UL << 2)
#define GPT_COUNTER_MASK 0xFFFFFFUL

#define PWM_CHANNELS    6
#define CAPTURE_CHANNELS 2

static const struct {
    uint8_t port;
    uint8_t pin;
} pwm_pin[PWM_CHANNELS] = {
    { 3, 0 }, { 3, 1 }, { 1, 0 }, { 1, 1 }, { 4, 0 }, { 4, 1 },
};

static const struct {
    uint8_t timer;
    uint8_t port;
    uint8_t pin;
} capture_map[CAPTURE_CHANNELS] = {
    { 0, 3, 4 }, { 2, 3, 5 },
};

static struct {
    bool     running;
    bool     have_prev;
    uint32_t prev;
} capture[CAPTURE_CHANNELS];

br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille)
{
    if (ch >= PWM_CHANNELS || freq_hz == 0 || duty_permille > 1000) {
        return BR_ERR_INVALID;
    }

    uint32_t load = PWM_CLOCK_HZ / freq_hz;
    if (load < 2 || load > 65536UL) {
        return BR_ERR_INVALID;
    }

    uint32_t gen = ch / 2;
    bool     b   = (ch & 1) != 0;
    uint32_t ticks = (load * duty_permille) / 1000UL;

    SYSCTL_RCGC0 |= SYSCTL_RCGC0_PWM;
    (void)SYSCTL_RCGC0;
    SYSCTL_RCC = (SYSCTL_RCC & ~SYSCTL_RCC_PWMDIV_M) |
                 SYSCTL_RCC_PWMDIV_16 | SYSCTL_RCC_USEPWMDIV;

    lm3s_pin_function(pwm_pin[ch].port, pwm_pin[ch].pin, 0);

    PWM_GEN(gen, PWM_GEN_CTL)  = 0;
    PWM_GEN(gen, PWM_GEN_LOAD) = load - 1;

    uint32_t action;
    if (ticks == 0) {
        action = GEN_ACTLOAD_LOW;
    } else if (ticks >= load) {
        action = GEN_ACTLOAD_HIGH;
    } else if (b) {
        action = GEN_ACTLOAD_HIGH | GENB_ACTCMPBD_LOW;
        PWM_GEN(gen, PWM_GEN_CMPB) = (load - 1) - ticks;
    } else {
        action = GEN_ACTLOAD_HIGH | GENA_ACTCMPAD_LOW;
        PWM_GEN(gen, PWM_GEN_CMPA) = (load - 1) - ticks;
    }
    PWM_GEN(gen, b ? PWM_GEN_GENB : PWM_GEN_GENA) = action;

    PWM_GEN(gen, PWM_GEN_CTL) = GEN_CTL_ENABLE;
    PWM_ENABLE |= 1UL << ch;
    return BR_OK;
}

br_err_t br_hal_pwm_stop(uint8_t ch)
{
    if (ch >= PWM_CHANNELS) {
        return BR_ERR_INVALID;
    }
    PWM_ENABLE &= ~(1UL << ch);
    return BR_OK;
}

br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge)
{
    if (ch >= CAPTURE_CHANNELS || edge > BR_CAPTURE_FALLING) {
        return BR_ERR_INVALID;
    }

    uint32_t t = capture_map[ch].timer;

    SYSCTL_RCGC1 |= SYSCTL_RCGC1_TIMER(t);
    (void)SYSCTL_RCGC1;

    lm3s_pin_function(capture_map[ch].port, capture_map[ch].pin, 0);

    GPT_CTL(t)   = 0;
    GPT_CFG(t)   = GPT_CFG_16BIT;
    GPT_TAMR(t)  = GPT_TAMR_CAPTURE_EDGE_TIME;
    GPT_TAILR(t) = 0xFFFF;
    GPT_TAPR(t)  = 0xFF;
    GPT_ICR(t)   = GPT_RIS_CAE;
    GPT_CTL(t)   = GPT_CTL_TAEN |
                   (edge == BR_CAPTURE_FALLING ? GPT_CTL_TAEVENT_FALLING : 0);

    capture[ch].running   = true;
    capture[ch].have_prev = false;
    return BR_OK;
}

br_err_t br_hal_capture_stop(uint8_t ch)
{
    if (ch >= CAPTURE_CHANNELS) {
        return BR_ERR_INVALID;
    }
    GPT_CTL(capture_map[ch].timer) = 0;
    capture[ch].running = false;
    return BR_OK;
}

br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us)
{
    if (ch >= CAPTURE_CHANNELS || period_us == NULL || !capture[ch].running) {
        return BR_ERR_INVALID;
    }

    uint32_t t = capture_map[ch].timer;

    if (!(GPT_RIS(t) & GPT_RIS_CAE)) {
        return BR_ERR_BUSY;
    }

    uint32_t now = GPT_TAR(t) & GPT_COUNTER_MASK;
    GPT_ICR(t) = GPT_RIS_CAE;

    if (!capture[ch].have_prev) {
        capture[ch].prev      = now;
        capture[ch].have_prev = true;
        return BR_ERR_BUSY;
    }

    uint32_t ticks = (capture[ch].prev - now) & GPT_COUNTER_MASK;
    capture[ch].prev = now;
    *period_us = ticks / (BR_HAL_SYS_CLOCK_HZ / 1000000UL);
    return BR_OK;
}

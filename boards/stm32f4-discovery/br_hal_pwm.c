/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "stm32f4_regs.h"

#define TIM2_BASE       0x40000000UL
#define TIM3_BASE       0x40000400UL

#define TIM_CR1(b)      STM32_REG((b) + 0x00UL)
#define TIM_EGR(b)      STM32_REG((b) + 0x14UL)
#define TIM_SR(b)       STM32_REG((b) + 0x10UL)
#define TIM_CCMR1(b)    STM32_REG((b) + 0x18UL)
#define TIM_CCMR2(b)    STM32_REG((b) + 0x1CUL)
#define TIM_CCER(b)     STM32_REG((b) + 0x20UL)
#define TIM_PSC(b)      STM32_REG((b) + 0x28UL)
#define TIM_ARR(b)      STM32_REG((b) + 0x2CUL)
#define TIM_CCR(b, n)   STM32_REG((b) + 0x34UL + 4UL * (n))

#define TIM_CR1_CEN     (1UL << 0)
#define TIM_CR1_ARPE    (1UL << 7)
#define TIM_EGR_UG      (1UL << 0)
#define TIM_CCMR_OCM_PWM1 (6UL << 4)
#define TIM_CCMR_OCPE   (1UL << 3)
#define TIM_CCMR_CCS_TI 1UL
#define TIM_CCER_CCE(n) (1UL << (4 * (n)))
#define TIM_CCER_CCP(n) (1UL << (4 * (n) + 1))
#define TIM_SR_CCIF(n)  (1UL << ((n) + 1))

#define PWM_CHANNELS    4
#define CAPTURE_CHANNELS 2
#define PORT_A          0
#define PORT_B          1
#define AF_TIM2         1UL
#define AF_TIM3         2UL

static const uint8_t capture_ccr[CAPTURE_CHANNELS] = { 2, 3 };
static const uint8_t capture_pin[CAPTURE_CHANNELS] = { 0, 1 };

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

    uint32_t total = STM32_TIM_HZ / freq_hz;
    uint32_t psc   = total > 0 ? (total - 1) / 65536UL : 0;
    uint32_t arr   = total / (psc + 1);
    if (total < 2 || arr < 2) {
        return BR_ERR_INVALID;
    }
    arr -= 1;

    RCC_APB1ENR_REG |= RCC_APB1ENR_TIM2EN;
    (void)RCC_APB1ENR_REG;

    stm32_gpio_pin(PORT_A, ch, GPIO_MODE_AF, GPIO_PULL_NONE, AF_TIM2, 0);

    TIM_PSC(TIM2_BASE) = psc;
    TIM_ARR(TIM2_BASE) = arr;
    TIM_CCR(TIM2_BASE, ch) = ((arr + 1) * (uint32_t)duty_permille) / 1000UL;

    uint32_t mode = TIM_CCMR_OCM_PWM1 | TIM_CCMR_OCPE;
    if (ch < 2) {
        uint32_t shift = 8U * ch;
        TIM_CCMR1(TIM2_BASE) = (TIM_CCMR1(TIM2_BASE) & ~(0xFFUL << shift)) | (mode << shift);
    } else {
        uint32_t shift = 8U * (ch - 2U);
        TIM_CCMR2(TIM2_BASE) = (TIM_CCMR2(TIM2_BASE) & ~(0xFFUL << shift)) | (mode << shift);
    }

    TIM_CCER(TIM2_BASE) |= TIM_CCER_CCE(ch);
    TIM_CR1(TIM2_BASE)   = TIM_CR1_ARPE | TIM_CR1_CEN;
    TIM_EGR(TIM2_BASE)   = TIM_EGR_UG;
    return BR_OK;
}

br_err_t br_hal_pwm_stop(uint8_t ch)
{
    if (ch >= PWM_CHANNELS) {
        return BR_ERR_INVALID;
    }

    TIM_CCER(TIM2_BASE) &= ~TIM_CCER_CCE(ch);
    if ((TIM_CCER(TIM2_BASE) & (TIM_CCER_CCE(0) | TIM_CCER_CCE(1) |
                                TIM_CCER_CCE(2) | TIM_CCER_CCE(3))) == 0) {
        TIM_CR1(TIM2_BASE) &= ~TIM_CR1_CEN;
    }
    return BR_OK;
}

br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge)
{
    if (ch >= CAPTURE_CHANNELS || edge > BR_CAPTURE_FALLING) {
        return BR_ERR_INVALID;
    }

    uint32_t n = capture_ccr[ch];

    RCC_APB1ENR_REG |= RCC_APB1ENR_TIM3EN;
    (void)RCC_APB1ENR_REG;

    stm32_gpio_pin(PORT_B, capture_pin[ch], GPIO_MODE_AF, GPIO_PULL_NONE, AF_TIM3, 0);

    TIM_PSC(TIM3_BASE) = STM32_TIM_HZ / 1000000UL - 1;
    TIM_ARR(TIM3_BASE) = 0xFFFF;

    uint32_t shift = 8U * (n - 2U);
    TIM_CCMR2(TIM3_BASE) = (TIM_CCMR2(TIM3_BASE) & ~(0xFFUL << shift)) |
                           (TIM_CCMR_CCS_TI << shift);

    TIM_CCER(TIM3_BASE) &= ~(TIM_CCER_CCE(n) | TIM_CCER_CCP(n));
    if (edge == BR_CAPTURE_FALLING) {
        TIM_CCER(TIM3_BASE) |= TIM_CCER_CCP(n);
    }
    TIM_CCER(TIM3_BASE) |= TIM_CCER_CCE(n);

    TIM_CR1(TIM3_BASE)  |= TIM_CR1_CEN;
    TIM_EGR(TIM3_BASE)   = TIM_EGR_UG;
    TIM_SR(TIM3_BASE)   &= ~TIM_SR_CCIF(n);

    capture[ch].running   = true;
    capture[ch].have_prev = false;
    return BR_OK;
}

br_err_t br_hal_capture_stop(uint8_t ch)
{
    if (ch >= CAPTURE_CHANNELS) {
        return BR_ERR_INVALID;
    }

    TIM_CCER(TIM3_BASE) &= ~TIM_CCER_CCE(capture_ccr[ch]);
    capture[ch].running = false;

    if (!capture[0].running && !capture[1].running) {
        TIM_CR1(TIM3_BASE) &= ~TIM_CR1_CEN;
    }
    return BR_OK;
}

br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us)
{
    if (ch >= CAPTURE_CHANNELS || period_us == NULL || !capture[ch].running) {
        return BR_ERR_INVALID;
    }

    uint32_t n = capture_ccr[ch];

    if (!(TIM_SR(TIM3_BASE) & TIM_SR_CCIF(n))) {
        return BR_ERR_BUSY;
    }

    uint32_t now = TIM_CCR(TIM3_BASE, n) & 0xFFFFUL;

    if (!capture[ch].have_prev) {
        capture[ch].prev      = now;
        capture[ch].have_prev = true;
        return BR_ERR_BUSY;
    }

    *period_us = (now - capture[ch].prev) & 0xFFFFUL;
    capture[ch].prev = now;
    return BR_OK;
}

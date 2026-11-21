/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "rp2040_regs.h"

#define PWM_BASE            0x40050000UL
#define PWM_CSR(s)          RP_REG(PWM_BASE + 0x14UL * (s) + 0x00UL)
#define PWM_DIV(s)          RP_REG(PWM_BASE + 0x14UL * (s) + 0x04UL)
#define PWM_CC(s)           RP_REG(PWM_BASE + 0x14UL * (s) + 0x0CUL)
#define PWM_TOP(s)          RP_REG(PWM_BASE + 0x14UL * (s) + 0x10UL)
#define PWM_CSR_EN          (1UL << 0)

#define PWM_CHANNELS        4
#define PWM_FIRST_GPIO      10
#define PWM_SLICES          8

#define IO_INTR1            RP_REG(IO_BANK0_BASE + 0x0F4UL)
#define IO_PROC0_INTE1      RP_REG(IO_BANK0_BASE + 0x104UL)
#define IO_PROC0_INTS1      RP_REG(IO_BANK0_BASE + 0x124UL)
#define TIMER_TIMERAWL      RP_REG(0x40054028UL)
#define NVIC_ISER_REG       RP_REG(0xE000E100UL)
#define NVIC_ICER_REG       RP_REG(0xE000E180UL)
#define IO_IRQ_BANK0        13

#define GPIO_IRQ_EDGE_LOW   (1UL << 2)
#define GPIO_IRQ_EDGE_HIGH  (1UL << 3)
#define GPIO_IRQ_MASK       0xFUL

#define CAPTURE_CHANNELS    2
#define CAPTURE_FIRST_GPIO  14

static uint8_t pwm_active[PWM_SLICES];

static volatile struct {
    bool     running;
    bool     have_prev;
    bool     ready;
    uint32_t prev;
    uint32_t period;
} capture[CAPTURE_CHANNELS];

br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille)
{
    if (ch >= PWM_CHANNELS || freq_hz == 0 || duty_permille > 1000) {
        return BR_ERR_INVALID;
    }

    uint32_t total = RP_CLK_PERI_HZ / freq_hz;
    if (total < 2) {
        return BR_ERR_INVALID;
    }
    uint32_t div = (total - 1) / 65536UL + 1;
    uint32_t top = total / div;
    if (div > 255 || top < 2) {
        return BR_ERR_INVALID;
    }
    top -= 1;

    uint8_t gpio  = PWM_FIRST_GPIO + ch;
    uint8_t slice = (gpio >> 1) & 7U;
    uint32_t shift = (gpio & 1U) ? 16U : 0U;
    uint32_t level = ((top + 1) * (uint32_t)duty_permille) / 1000UL;
    if (level > 0xFFFF) {
        level = 0xFFFF;
    }

    rp_unreset(RESET_PWM);
    rp_pin_function(gpio, GPIO_FUNC_PWM, 0);

    PWM_CSR(slice) = 0;
    PWM_DIV(slice) = div << 4;
    PWM_TOP(slice) = top;
    PWM_CC(slice)  = (PWM_CC(slice) & ~(0xFFFFUL << shift)) | (level << shift);
    PWM_CSR(slice) = PWM_CSR_EN;

    pwm_active[slice] |= (uint8_t)(1U << (gpio & 1U));
    return BR_OK;
}

br_err_t br_hal_pwm_stop(uint8_t ch)
{
    if (ch >= PWM_CHANNELS) {
        return BR_ERR_INVALID;
    }

    uint8_t gpio  = PWM_FIRST_GPIO + ch;
    uint8_t slice = (gpio >> 1) & 7U;
    uint32_t shift = (gpio & 1U) ? 16U : 0U;

    PWM_CC(slice) &= ~(0xFFFFUL << shift);
    pwm_active[slice] &= (uint8_t)~(1U << (gpio & 1U));
    if (pwm_active[slice] == 0) {
        PWM_CSR(slice) = 0;
    }
    return BR_OK;
}

void IO_IRQ_BANK0_Handler(void)
{
    uint32_t status = IO_PROC0_INTS1;
    uint32_t stamp  = TIMER_TIMERAWL;

    for (uint32_t ch = 0; ch < CAPTURE_CHANNELS; ch++) {
        uint32_t nibble = (status >> (4 * (CAPTURE_FIRST_GPIO - 8 + ch))) & GPIO_IRQ_MASK;
        if (!(nibble & (GPIO_IRQ_EDGE_LOW | GPIO_IRQ_EDGE_HIGH))) {
            continue;
        }

        IO_INTR1 = nibble << (4 * (CAPTURE_FIRST_GPIO - 8 + ch));

        if (capture[ch].have_prev) {
            capture[ch].period = stamp - capture[ch].prev;
            capture[ch].ready  = true;
        }
        capture[ch].prev      = stamp;
        capture[ch].have_prev = true;
    }
}

br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge)
{
    if (ch >= CAPTURE_CHANNELS || edge > BR_CAPTURE_FALLING) {
        return BR_ERR_INVALID;
    }

    uint8_t  gpio  = CAPTURE_FIRST_GPIO + ch;
    uint32_t shift = 4U * (gpio - 8U);
    uint32_t event = edge == BR_CAPTURE_FALLING ? GPIO_IRQ_EDGE_LOW : GPIO_IRQ_EDGE_HIGH;

    rp_pin_function(gpio, GPIO_FUNC_SIO, 0);
    SIO_GPIO_OE_CLR = 1UL << gpio;

    NVIC_ICER_REG = 1UL << IO_IRQ_BANK0;
    capture[ch].have_prev = false;
    capture[ch].ready     = false;
    capture[ch].running   = true;

    IO_INTR1 = GPIO_IRQ_MASK << shift;
    IO_PROC0_INTE1 = (IO_PROC0_INTE1 & ~(GPIO_IRQ_MASK << shift)) | (event << shift);
    NVIC_ISER_REG = 1UL << IO_IRQ_BANK0;
    return BR_OK;
}

br_err_t br_hal_capture_stop(uint8_t ch)
{
    if (ch >= CAPTURE_CHANNELS) {
        return BR_ERR_INVALID;
    }

    uint32_t shift = 4U * (CAPTURE_FIRST_GPIO - 8U + ch);

    IO_PROC0_INTE1 &= ~(GPIO_IRQ_MASK << shift);
    IO_INTR1 = GPIO_IRQ_MASK << shift;
    capture[ch].running = false;
    return BR_OK;
}

br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us)
{
    if (ch >= CAPTURE_CHANNELS || period_us == NULL || !capture[ch].running) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    bool ready = capture[ch].ready;
    if (ready) {
        *period_us = capture[ch].period;
        capture[ch].ready = false;
    }
    br_hal_irq_restore(key);

    return ready ? BR_OK : BR_ERR_BUSY;
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "lm3s_regs.h"

#define SSI0_CR0        LM3S_REG(0x40008000UL)
#define SSI0_CR1        LM3S_REG(0x40008004UL)
#define SSI0_DR         LM3S_REG(0x40008008UL)
#define SSI0_SR         LM3S_REG(0x4000800CUL)
#define SSI0_CPSR       LM3S_REG(0x40008010UL)

#define SYSCTL_RCGC1_SSI0  (1UL << 4)

#define SSI_CR0_SPO     (1UL << 6)
#define SSI_CR0_SPH     (1UL << 7)
#define SSI_CR0_DSS_8   7UL
#define SSI_CR0_SCR(x)  ((uint32_t)(x) << 8)
#define SSI_CR1_LBM     (1UL << 0)
#define SSI_CR1_SSE     (1UL << 1)
#define SSI_SR_TNF      (1UL << 1)
#define SSI_SR_RNE      (1UL << 2)

#define SSI_CPSR_MAX    254UL
#define SSI_FIFO_DEPTH  8U
#define SSI_IDLE_BYTE   0xFF

#define PORT_A          0
#define PIN_SSI_CLK     2
#define PIN_SSI_RX      4
#define PIN_SSI_TX      5

static bool initialized;

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode)
{
    if (bus != 0 || hz == 0 || (mode & ~(BR_SPI_CPHA | BR_SPI_CPOL | BR_SPI_LOOP))) {
        return BR_ERR_INVALID;
    }

    uint32_t divisor = (BR_HAL_SYS_CLOCK_HZ + hz - 1) / hz;
    if (divisor < 2) {
        divisor = 2;
    }

    uint32_t cpsr = 2;
    while (divisor > cpsr * 256UL && cpsr < SSI_CPSR_MAX) {
        cpsr += 2;
    }
    uint32_t scr = (divisor + cpsr - 1) / cpsr - 1;
    if (scr > 255) {
        return BR_ERR_INVALID;
    }

    SYSCTL_RCGC1 |= SYSCTL_RCGC1_SSI0;
    (void)SYSCTL_RCGC1;

    lm3s_pin_function(PORT_A, PIN_SSI_CLK, 0);
    lm3s_pin_function(PORT_A, PIN_SSI_RX, 0);
    lm3s_pin_function(PORT_A, PIN_SSI_TX, 0);

    SSI0_CR1  = 0;
    SSI0_CPSR = cpsr;
    SSI0_CR0  = SSI_CR0_SCR(scr) | SSI_CR0_DSS_8 |
                ((mode & BR_SPI_CPOL) ? SSI_CR0_SPO : 0) |
                ((mode & BR_SPI_CPHA) ? SSI_CR0_SPH : 0);
    SSI0_CR1  = SSI_CR1_SSE | ((mode & BR_SPI_LOOP) ? SSI_CR1_LBM : 0);

    for (uint32_t i = 0; i < SSI_FIFO_DEPTH && (SSI0_SR & SSI_SR_RNE); i++) {
        (void)SSI0_DR;
    }

    initialized = true;
    return BR_OK;
}

static br_err_t wait_sr(uint32_t flag)
{
    uint32_t spins = LM3S_WAIT_LIMIT;
    while (!(SSI0_SR & flag)) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    return BR_OK;
}

br_err_t br_hal_spi_transfer(uint8_t bus, const uint8_t *tx, uint8_t *rx,
                             size_t len)
{
    if (bus != 0 || !initialized) {
        return BR_ERR_INVALID;
    }

    for (size_t i = 0; i < len; i++) {
        br_err_t err = wait_sr(SSI_SR_TNF);
        if (err != BR_OK) {
            return err;
        }
        SSI0_DR = tx != NULL ? tx[i] : SSI_IDLE_BYTE;

        err = wait_sr(SSI_SR_RNE);
        if (err != BR_OK) {
            return err;
        }
        uint8_t in = (uint8_t)SSI0_DR;
        if (rx != NULL) {
            rx[i] = in;
        }
    }
    return BR_OK;
}

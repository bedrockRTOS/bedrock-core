/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "rp2040_regs.h"

#define SPI0_CR0        RP_REG(0x4003C000UL)
#define SPI0_CR1        RP_REG(0x4003C004UL)
#define SPI0_DR         RP_REG(0x4003C008UL)
#define SPI0_SR         RP_REG(0x4003C00CUL)
#define SPI0_CPSR       RP_REG(0x4003C010UL)

#define SSP_CR0_SPO     (1UL << 6)
#define SSP_CR0_SPH     (1UL << 7)
#define SSP_CR0_DSS_8   7UL
#define SSP_CR0_SCR(x)  ((uint32_t)(x) << 8)
#define SSP_CR1_LBM     (1UL << 0)
#define SSP_CR1_SSE     (1UL << 1)
#define SSP_SR_TNF      (1UL << 1)
#define SSP_SR_RNE      (1UL << 2)
#define SSP_SR_BSY      (1UL << 4)

#define SSP_CPSR_MAX    254UL
#define SSP_IDLE_BYTE   0xFF

#define PIN_SPI_RX      16
#define PIN_SPI_SCK     18
#define PIN_SPI_TX      19

static bool initialized;

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode)
{
    if (bus != 0 || hz == 0 || (mode & ~(BR_SPI_CPHA | BR_SPI_CPOL | BR_SPI_LOOP))) {
        return BR_ERR_INVALID;
    }

    uint32_t divisor = (RP_CLK_PERI_HZ + hz - 1) / hz;
    if (divisor < 2) {
        divisor = 2;
    }

    uint32_t cpsr = 2;
    while (divisor > cpsr * 256UL && cpsr < SSP_CPSR_MAX) {
        cpsr += 2;
    }
    uint32_t scr = (divisor + cpsr - 1) / cpsr - 1;
    if (scr > 255) {
        return BR_ERR_INVALID;
    }

    rp_unreset(RESET_SPI0);

    rp_pin_function(PIN_SPI_RX, GPIO_FUNC_SPI, 0);
    rp_pin_function(PIN_SPI_SCK, GPIO_FUNC_SPI, 0);
    rp_pin_function(PIN_SPI_TX, GPIO_FUNC_SPI, 0);

    SPI0_CR1  = 0;
    SPI0_CPSR = cpsr;
    SPI0_CR0  = SSP_CR0_SCR(scr) | SSP_CR0_DSS_8 |
                ((mode & BR_SPI_CPOL) ? SSP_CR0_SPO : 0) |
                ((mode & BR_SPI_CPHA) ? SSP_CR0_SPH : 0);
    SPI0_CR1  = SSP_CR1_SSE | ((mode & BR_SPI_LOOP) ? SSP_CR1_LBM : 0);

    initialized = true;
    return BR_OK;
}

static br_err_t wait_sr(uint32_t flag)
{
    uint32_t spins = RP_WAIT_LIMIT;
    while (!(SPI0_SR & flag)) {
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
        br_err_t err = wait_sr(SSP_SR_TNF);
        if (err != BR_OK) {
            return err;
        }
        SPI0_DR = tx != NULL ? tx[i] : SSP_IDLE_BYTE;

        err = wait_sr(SSP_SR_RNE);
        if (err != BR_OK) {
            return err;
        }
        uint8_t in = (uint8_t)SPI0_DR;
        if (rx != NULL) {
            rx[i] = in;
        }
    }

    uint32_t spins = RP_WAIT_LIMIT;
    while (SPI0_SR & SSP_SR_BSY) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    return BR_OK;
}

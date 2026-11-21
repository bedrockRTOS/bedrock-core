/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "stm32_regs.h"

#define SPI1_CR1        STM32_REG(0x40013000UL)
#define SPI1_SR         STM32_REG(0x40013008UL)
#define SPI1_DR         STM32_REG(0x4001300CUL)

#define SPI_CR1_CPHA    (1UL << 0)
#define SPI_CR1_CPOL    (1UL << 1)
#define SPI_CR1_MSTR    (1UL << 2)
#define SPI_CR1_BR(x)   ((uint32_t)(x) << 3)
#define SPI_CR1_SPE     (1UL << 6)
#define SPI_CR1_SSI     (1UL << 8)
#define SPI_CR1_SSM     (1UL << 9)
#define SPI_SR_RXNE     (1UL << 0)
#define SPI_SR_TXE      (1UL << 1)
#define SPI_SR_BSY      (1UL << 7)

#define SPI_IDLE_BYTE   0xFF

#define PORT_A          0
#define PIN_SCK         5
#define PIN_MISO        6
#define PIN_MOSI        7

static bool initialized;

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode)
{
    if (bus != 0 || hz == 0 || (mode & ~(BR_SPI_CPHA | BR_SPI_CPOL))) {
        return BR_ERR_INVALID;
    }

    uint32_t br = 0;
    while (br < 7 && STM32_APB2_HZ / (2UL << br) > hz) {
        br++;
    }
    if (STM32_APB2_HZ / (2UL << br) > hz) {
        return BR_ERR_INVALID;
    }

    RCC_APB2ENR_REG |= RCC_APB2ENR_SPI1EN;
    (void)RCC_APB2ENR_REG;

    stm32_gpio_cfg(PORT_A, PIN_SCK, GPIO_CFG_AF_PP_50M);
    stm32_gpio_cfg(PORT_A, PIN_MOSI, GPIO_CFG_AF_PP_50M);
    stm32_gpio_cfg(PORT_A, PIN_MISO, GPIO_CFG_INPUT_FLOAT);

    SPI1_CR1 = 0;
    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR(br) |
               ((mode & BR_SPI_CPHA) ? SPI_CR1_CPHA : 0) |
               ((mode & BR_SPI_CPOL) ? SPI_CR1_CPOL : 0);
    SPI1_CR1 |= SPI_CR1_SPE;

    initialized = true;
    return BR_OK;
}

static br_err_t wait_sr(uint32_t flag)
{
    uint32_t spins = STM32_WAIT_LIMIT;
    while (!(SPI1_SR & flag)) {
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
        br_err_t err = wait_sr(SPI_SR_TXE);
        if (err != BR_OK) {
            return err;
        }
        SPI1_DR = tx != NULL ? tx[i] : SPI_IDLE_BYTE;

        err = wait_sr(SPI_SR_RXNE);
        if (err != BR_OK) {
            return err;
        }
        uint8_t in = (uint8_t)SPI1_DR;
        if (rx != NULL) {
            rx[i] = in;
        }
    }

    uint32_t spins = STM32_WAIT_LIMIT;
    while (SPI1_SR & SPI_SR_BSY) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    return BR_OK;
}

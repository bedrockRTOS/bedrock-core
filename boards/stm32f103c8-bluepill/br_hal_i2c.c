/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "stm32_regs.h"

#define I2C1_CR1        STM32_REG(0x40005400UL)
#define I2C1_CR2        STM32_REG(0x40005404UL)
#define I2C1_DR         STM32_REG(0x40005410UL)
#define I2C1_SR1        STM32_REG(0x40005414UL)
#define I2C1_SR2        STM32_REG(0x40005418UL)
#define I2C1_CCR        STM32_REG(0x4000541CUL)
#define I2C1_TRISE      STM32_REG(0x40005420UL)

#define I2C_CR1_PE      (1UL << 0)
#define I2C_CR1_START   (1UL << 8)
#define I2C_CR1_STOP    (1UL << 9)
#define I2C_CR1_ACK     (1UL << 10)
#define I2C_CR1_POS     (1UL << 11)
#define I2C_CR1_SWRST   (1UL << 15)

#define I2C_SR1_SB      (1UL << 0)
#define I2C_SR1_ADDR    (1UL << 1)
#define I2C_SR1_BTF     (1UL << 2)
#define I2C_SR1_RXNE    (1UL << 6)
#define I2C_SR1_TXE     (1UL << 7)
#define I2C_SR1_AF      (1UL << 10)
#define I2C_SR1_BERR    (1UL << 8)
#define I2C_SR1_ARLO    (1UL << 9)
#define I2C_SR2_BUSY    (1UL << 1)

#define I2C_CCR_FS      (1UL << 15)
#define I2C_MAX_HZ      400000UL

#define PORT_B          1
#define PIN_SCL         6
#define PIN_SDA         7

static bool initialized;
static bool bus_held;

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz)
{
    if (bus != 0 || hz == 0 || hz > I2C_MAX_HZ) {
        return BR_ERR_INVALID;
    }

    uint32_t pclk_mhz = STM32_APB1_HZ / 1000000UL;
    uint32_t ccr;
    uint32_t trise;

    if (hz <= 100000UL) {
        ccr = STM32_APB1_HZ / (2UL * hz);
        trise = pclk_mhz + 1;
    } else {
        ccr = I2C_CCR_FS | (STM32_APB1_HZ / (3UL * hz));
        trise = (pclk_mhz * 300UL) / 1000UL + 1;
    }
    if ((ccr & 0xFFFUL) < 4) {
        return BR_ERR_INVALID;
    }

    RCC_APB1ENR_REG |= RCC_APB1ENR_I2C1EN;
    (void)RCC_APB1ENR_REG;

    stm32_gpio_cfg(PORT_B, PIN_SCL, GPIO_CFG_AF_OD_50M);
    stm32_gpio_cfg(PORT_B, PIN_SDA, GPIO_CFG_AF_OD_50M);

    I2C1_CR1   = I2C_CR1_SWRST;
    I2C1_CR1   = 0;
    I2C1_CR2   = pclk_mhz;
    I2C1_CCR   = ccr;
    I2C1_TRISE = trise;
    I2C1_CR1   = I2C_CR1_PE;

    initialized = true;
    bus_held    = false;
    return BR_OK;
}

static br_err_t wait_sr1(uint32_t flags)
{
    uint32_t spins = STM32_WAIT_LIMIT;
    while (!(I2C1_SR1 & flags)) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    return BR_OK;
}

static br_err_t abort_transfer(br_err_t err)
{
    I2C1_CR1 = (I2C1_CR1 | I2C_CR1_STOP) & ~(I2C_CR1_POS);
    I2C1_SR1 = 0;
    bus_held = false;
    return err;
}

static br_err_t start_address(uint8_t addr, bool read)
{
    if (!bus_held) {
        uint32_t spins = STM32_WAIT_LIMIT;
        while (I2C1_SR2 & I2C_SR2_BUSY) {
            if (--spins == 0) {
                return BR_ERR_TIMEOUT;
            }
        }
    }

    I2C1_CR1 |= I2C_CR1_START;
    br_err_t err = wait_sr1(I2C_SR1_SB);
    if (err != BR_OK) {
        return abort_transfer(err);
    }

    I2C1_DR = ((uint32_t)addr << 1) | (read ? 1UL : 0UL);

    err = wait_sr1(I2C_SR1_ADDR | I2C_SR1_AF);
    if (err != BR_OK) {
        return abort_transfer(err);
    }
    if (I2C1_SR1 & I2C_SR1_AF) {
        return abort_transfer(BR_ERR_BUSY);
    }
    return BR_OK;
}

static void clear_addr(void)
{
    (void)I2C1_SR1;
    (void)I2C1_SR2;
}

br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data,
                          size_t len, bool stop)
{
    if (bus != 0 || !initialized || data == NULL || len == 0 || addr > 0x7F) {
        return BR_ERR_INVALID;
    }

    br_err_t err = start_address(addr, false);
    if (err != BR_OK) {
        return err;
    }
    clear_addr();

    for (size_t i = 0; i < len; i++) {
        err = wait_sr1(I2C_SR1_TXE | I2C_SR1_AF);
        if (err != BR_OK) {
            return abort_transfer(err);
        }
        if (I2C1_SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) {
            return abort_transfer(BR_ERR_BUSY);
        }
        I2C1_DR = data[i];
    }

    err = wait_sr1(I2C_SR1_BTF | I2C_SR1_AF);
    if (err != BR_OK) {
        return abort_transfer(err);
    }
    if (I2C1_SR1 & I2C_SR1_AF) {
        return abort_transfer(BR_ERR_BUSY);
    }

    if (stop) {
        I2C1_CR1 |= I2C_CR1_STOP;
        bus_held = false;
    } else {
        bus_held = true;
    }
    return BR_OK;
}

br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    if (bus != 0 || !initialized || data == NULL || len == 0 || addr > 0x7F) {
        return BR_ERR_INVALID;
    }

    I2C1_CR1 |= I2C_CR1_ACK;
    if (len == 2) {
        I2C1_CR1 |= I2C_CR1_POS;
    }

    br_err_t err = start_address(addr, true);
    if (err != BR_OK) {
        return err;
    }

    size_t i = 0;

    if (len == 1) {
        I2C1_CR1 &= ~I2C_CR1_ACK;
        clear_addr();
        I2C1_CR1 |= I2C_CR1_STOP;
        err = wait_sr1(I2C_SR1_RXNE);
        if (err != BR_OK) {
            return abort_transfer(err);
        }
        data[0] = (uint8_t)I2C1_DR;
    } else if (len == 2) {
        clear_addr();
        I2C1_CR1 &= ~I2C_CR1_ACK;
        err = wait_sr1(I2C_SR1_BTF);
        if (err != BR_OK) {
            return abort_transfer(err);
        }
        I2C1_CR1 |= I2C_CR1_STOP;
        data[0] = (uint8_t)I2C1_DR;
        data[1] = (uint8_t)I2C1_DR;
        I2C1_CR1 &= ~I2C_CR1_POS;
    } else {
        clear_addr();
        for (; i + 3 < len; i++) {
            err = wait_sr1(I2C_SR1_BTF);
            if (err != BR_OK) {
                return abort_transfer(err);
            }
            data[i] = (uint8_t)I2C1_DR;
        }

        err = wait_sr1(I2C_SR1_BTF);
        if (err != BR_OK) {
            return abort_transfer(err);
        }
        I2C1_CR1 &= ~I2C_CR1_ACK;
        data[i++] = (uint8_t)I2C1_DR;

        err = wait_sr1(I2C_SR1_BTF);
        if (err != BR_OK) {
            return abort_transfer(err);
        }
        I2C1_CR1 |= I2C_CR1_STOP;
        data[i++] = (uint8_t)I2C1_DR;
        data[i++] = (uint8_t)I2C1_DR;
    }

    bus_held = false;
    return BR_OK;
}

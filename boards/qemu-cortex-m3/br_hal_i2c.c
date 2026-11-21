/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "lm3s_regs.h"

#define I2C0_MSA        LM3S_REG(0x40020000UL)
#define I2C0_MCS        LM3S_REG(0x40020004UL)
#define I2C0_MDR        LM3S_REG(0x40020008UL)
#define I2C0_MTPR       LM3S_REG(0x4002000CUL)
#define I2C0_MCR        LM3S_REG(0x40020020UL)

#define SYSCTL_RCGC1_I2C0  (1UL << 12)

#define MCR_MFE         (1UL << 4)

#define MCS_RUN         (1UL << 0)
#define MCS_START       (1UL << 1)
#define MCS_STOP        (1UL << 2)
#define MCS_ACK         (1UL << 3)

#define MCS_BUSY        (1UL << 0)
#define MCS_ERROR       (1UL << 1)
#define MCS_ADRACK      (1UL << 2)
#define MCS_DATACK      (1UL << 3)
#define MCS_ARBLST      (1UL << 4)

#define PORT_B          1
#define PIN_I2C_SCL     2
#define PIN_I2C_SDA     3

#define I2C_SCL_CYCLES  20UL
#define I2C_MAX_HZ      400000UL

static bool initialized;

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz)
{
    if (bus != 0 || hz == 0 || hz > I2C_MAX_HZ) {
        return BR_ERR_INVALID;
    }

    uint32_t tpr = BR_HAL_SYS_CLOCK_HZ / (I2C_SCL_CYCLES * hz);
    if (tpr == 0 || tpr > 128) {
        return BR_ERR_INVALID;
    }

    SYSCTL_RCGC1 |= SYSCTL_RCGC1_I2C0;
    (void)SYSCTL_RCGC1;

    lm3s_pin_function(PORT_B, PIN_I2C_SCL, 0);
    lm3s_pin_function(PORT_B, PIN_I2C_SDA, 1);

    I2C0_MCR  = MCR_MFE;
    I2C0_MTPR = tpr - 1;

    initialized = true;
    return BR_OK;
}

static br_err_t run_byte(uint32_t cmd)
{
    I2C0_MCS = cmd;

    uint32_t spins = LM3S_WAIT_LIMIT;
    while (I2C0_MCS & MCS_BUSY) {
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }

    uint32_t status = I2C0_MCS;
    if (status & MCS_ERROR) {
        if (!(status & MCS_ARBLST)) {
            I2C0_MCS = MCS_STOP;
        }
        return BR_ERR_BUSY;
    }
    return BR_OK;
}

br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data,
                          size_t len, bool stop)
{
    if (bus != 0 || !initialized || data == NULL || len == 0 || addr > 0x7F) {
        return BR_ERR_INVALID;
    }

    I2C0_MSA = (uint32_t)addr << 1;

    for (size_t i = 0; i < len; i++) {
        uint32_t cmd = MCS_RUN;
        if (i == 0) {
            cmd |= MCS_START;
        }
        if (i == len - 1 && stop) {
            cmd |= MCS_STOP;
        }

        I2C0_MDR = data[i];
        br_err_t err = run_byte(cmd);
        if (err != BR_OK) {
            return err;
        }
    }
    return BR_OK;
}

br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    if (bus != 0 || !initialized || data == NULL || len == 0 || addr > 0x7F) {
        return BR_ERR_INVALID;
    }

    I2C0_MSA = ((uint32_t)addr << 1) | 1UL;

    for (size_t i = 0; i < len; i++) {
        uint32_t cmd = MCS_RUN;
        if (i == 0) {
            cmd |= MCS_START;
        }
        if (i == len - 1) {
            cmd |= MCS_STOP;
        } else {
            cmd |= MCS_ACK;
        }

        br_err_t err = run_byte(cmd);
        if (err != BR_OK) {
            return err;
        }
        data[i] = (uint8_t)I2C0_MDR;
    }
    return BR_OK;
}

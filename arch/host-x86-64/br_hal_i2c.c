/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define HOST_I2C_EEPROM_ADDR  0x50
#define HOST_I2C_EEPROM_SIZE  256

static bool    initialized;
static uint8_t eeprom[HOST_I2C_EEPROM_SIZE];
static uint8_t eeprom_ptr;

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz)
{
    if (bus != 0 || hz == 0) {
        return BR_ERR_INVALID;
    }
    initialized = true;
    return BR_OK;
}

br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data,
                          size_t len, bool stop)
{
    (void)stop;

    if (bus != 0 || !initialized || data == NULL || len == 0) {
        return BR_ERR_INVALID;
    }
    if (addr != HOST_I2C_EEPROM_ADDR) {
        return BR_ERR_BUSY;
    }

    eeprom_ptr = data[0];
    for (size_t i = 1; i < len; i++) {
        eeprom[eeprom_ptr++] = data[i];
    }
    return BR_OK;
}

br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    if (bus != 0 || !initialized || data == NULL || len == 0) {
        return BR_ERR_INVALID;
    }
    if (addr != HOST_I2C_EEPROM_ADDR) {
        return BR_ERR_BUSY;
    }

    for (size_t i = 0; i < len; i++) {
        data[i] = eeprom[eeprom_ptr++];
    }
    return BR_OK;
}

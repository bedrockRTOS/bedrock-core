/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>

#include "bedrock/br_hal.h"

/* TWI master on PC4 (SDA) and PC5 (SCL). The pins need external pull-ups,
 * the internal ones are switched on as a help. */

#define TWI_WAIT_LIMIT  60000U

#define ST_START        0x08
#define ST_REP_START    0x10
#define ST_SLAW_ACK     0x18
#define ST_DATA_W_ACK   0x28
#define ST_SLAR_ACK     0x40
#define ST_DATA_R_ACK   0x50
#define ST_DATA_R_NACK  0x58

static bool twi_ready;

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz)
{
    if (bus != 0 || hz == 0) {
        return BR_ERR_INVALID;
    }

    /* SCL = F_CPU / (16 + 2 * TWBR * prescaler) */
    uint32_t f = BR_HAL_SYS_CLOCK_HZ;
    if (hz > f / 16 / 1) {
        return BR_ERR_INVALID;
    }

    uint8_t twps = 0;
    uint32_t div = 1;
    uint32_t twbr = 0;
    for (; twps < 4; twps++, div *= 4) {
        uint32_t need = f / hz;
        twbr = need > 16 ? (need - 16) / (2 * div) : 0;
        if (twbr <= 255) {
            break;
        }
    }
    if (twps == 4) {
        return BR_ERR_INVALID;
    }

    PORTC |= _BV(PC4) | _BV(PC5);
    TWSR = twps;
    TWBR = (uint8_t)twbr;
    TWCR = _BV(TWEN);
    twi_ready = true;
    return BR_OK;
}

static bool wait_int(void)
{
    uint16_t spins = TWI_WAIT_LIMIT;

    while (!(TWCR & _BV(TWINT))) {
        if (--spins == 0) {
            return false;
        }
    }
    return true;
}

static void stop(void)
{
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);

    uint16_t spins = TWI_WAIT_LIMIT;
    while ((TWCR & _BV(TWSTO)) && --spins != 0) { }
}

static uint8_t status(void)
{
    return TWSR & 0xF8U;
}

/* The bus is released on every error. A NAK or a lost bus is BR_ERR_BUSY. */
static br_err_t start_address(uint8_t addr, bool read)
{
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    if (!wait_int()) {
        stop();
        return BR_ERR_TIMEOUT;
    }
    if (status() != ST_START && status() != ST_REP_START) {
        stop();
        return BR_ERR_BUSY;
    }

    TWDR = (uint8_t)((addr << 1) | (read ? 1U : 0U));
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!wait_int()) {
        stop();
        return BR_ERR_TIMEOUT;
    }
    if (status() != (read ? ST_SLAR_ACK : ST_SLAW_ACK)) {
        stop();
        return BR_ERR_BUSY;
    }
    return BR_OK;
}

br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data, size_t len, bool stop_after)
{
    if (bus != 0 || addr > 0x7F || (len > 0 && data == NULL) || !twi_ready) {
        return BR_ERR_INVALID;
    }

    br_err_t err = start_address(addr, false);
    if (err != BR_OK) {
        return err;
    }

    for (size_t i = 0; i < len; i++) {
        TWDR = data[i];
        TWCR = _BV(TWINT) | _BV(TWEN);
        if (!wait_int()) {
            stop();
            return BR_ERR_TIMEOUT;
        }
        if (status() != ST_DATA_W_ACK) {
            stop();
            return BR_ERR_BUSY;
        }
    }

    if (stop_after) {
        stop();
    }
    return BR_OK;
}

br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    if (bus != 0 || addr > 0x7F || data == NULL || len == 0 || !twi_ready) {
        return BR_ERR_INVALID;
    }

    br_err_t err = start_address(addr, true);
    if (err != BR_OK) {
        return err;
    }

    for (size_t i = 0; i < len; i++) {
        bool last = i + 1 == len;
        TWCR = _BV(TWINT) | _BV(TWEN) | (last ? 0 : _BV(TWEA));
        if (!wait_int()) {
            stop();
            return BR_ERR_TIMEOUT;
        }
        if (status() != (last ? ST_DATA_R_NACK : ST_DATA_R_ACK)) {
            stop();
            return BR_ERR_BUSY;
        }
        data[i] = TWDR;
    }

    stop();
    return BR_OK;
}

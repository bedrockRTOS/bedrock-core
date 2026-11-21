/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>

#include "bedrock/br_hal.h"

/* SPI master on PB3 (MOSI), PB4 (MISO), PB5 (SCK). PB2 (SS) is an output so
 * that the peripheral stays master, it is also a usable chip select. */

#define SPI_WAIT_LIMIT  30000U

#define MODE_MASK  (BR_SPI_CPHA | BR_SPI_CPOL)

/* SPR1:SPR0:SPI2X for each divider, in the order of the table below */
static const uint8_t divider[] = { 2, 4, 8, 16, 32, 64, 128 };
static const uint8_t spr_bits[] = { 0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x6 };

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode)
{
    if (bus != 0 || hz == 0 || (mode & (uint8_t)~MODE_MASK) != 0) {
        return BR_ERR_INVALID;
    }

    uint8_t pick = (uint8_t)(sizeof(divider) - 1);
    for (uint8_t i = 0; i < sizeof(divider); i++) {
        if (BR_HAL_SYS_CLOCK_HZ / divider[i] <= hz) {
            pick = i;
            break;
        }
    }

    DDRB |= _BV(PB3) | _BV(PB5) | _BV(PB2);
    DDRB &= (uint8_t)~_BV(PB4);

    uint8_t bits = spr_bits[pick];
    SPSR = (bits & 1U) ? _BV(SPI2X) : 0;
    SPCR = _BV(SPE) | _BV(MSTR) | (uint8_t)((bits >> 1) & 0x3U) |
           ((mode & BR_SPI_CPOL) ? _BV(CPOL) : 0) |
           ((mode & BR_SPI_CPHA) ? _BV(CPHA) : 0);
    return BR_OK;
}

br_err_t br_hal_spi_transfer(uint8_t bus, const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (bus != 0 || !(SPCR & _BV(SPE))) {
        return BR_ERR_INVALID;
    }

    for (size_t i = 0; i < len; i++) {
        SPDR = tx != NULL ? tx[i] : 0xFF;

        uint16_t spins = SPI_WAIT_LIMIT;
        while (!(SPSR & _BV(SPIF))) {
            if (--spins == 0) {
                return BR_ERR_TIMEOUT;
            }
        }

        uint8_t b = SPDR;
        if (rx != NULL) {
            rx[i] = b;
        }
    }
    return BR_OK;
}

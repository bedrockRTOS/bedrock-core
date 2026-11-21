/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "rp2040_regs.h"

#define I2C0_BASE            0x40044000UL
#define IC_CON               RP_REG(I2C0_BASE + 0x00UL)
#define IC_TAR               RP_REG(I2C0_BASE + 0x04UL)
#define IC_DATA_CMD          RP_REG(I2C0_BASE + 0x10UL)
#define IC_SS_SCL_HCNT       RP_REG(I2C0_BASE + 0x14UL)
#define IC_SS_SCL_LCNT       RP_REG(I2C0_BASE + 0x18UL)
#define IC_FS_SCL_HCNT       RP_REG(I2C0_BASE + 0x1CUL)
#define IC_FS_SCL_LCNT       RP_REG(I2C0_BASE + 0x20UL)
#define IC_RAW_INTR_STAT     RP_REG(I2C0_BASE + 0x34UL)
#define IC_RX_TL             RP_REG(I2C0_BASE + 0x38UL)
#define IC_TX_TL             RP_REG(I2C0_BASE + 0x3CUL)
#define IC_CLR_TX_ABRT       RP_REG(I2C0_BASE + 0x54UL)
#define IC_CLR_STOP_DET      RP_REG(I2C0_BASE + 0x60UL)
#define IC_ENABLE            RP_REG(I2C0_BASE + 0x6CUL)
#define IC_STATUS            RP_REG(I2C0_BASE + 0x70UL)
#define IC_ENABLE_STATUS     RP_REG(I2C0_BASE + 0x9CUL)
#define IC_FS_SPKLEN         RP_REG(I2C0_BASE + 0xA0UL)

#define IC_CON_MASTER        (1UL << 0)
#define IC_CON_SPEED_SS      (1UL << 1)
#define IC_CON_SPEED_FS      (2UL << 1)
#define IC_CON_RESTART_EN    (1UL << 5)
#define IC_CON_SLAVE_DIS     (1UL << 6)
#define IC_CON_TX_EMPTY_CTRL (1UL << 8)

#define IC_DATA_CMD_READ     (1UL << 8)
#define IC_DATA_CMD_STOP     (1UL << 9)
#define IC_DATA_CMD_RESTART  (1UL << 10)

#define IC_INTR_TX_ABRT      (1UL << 6)
#define IC_INTR_STOP_DET     (1UL << 9)
#define IC_STATUS_TFNF       (1UL << 1)
#define IC_STATUS_TFE        (1UL << 2)
#define IC_STATUS_RFNE       (1UL << 3)

#define I2C_SS_MAX_HZ        100000UL
#define I2C_MAX_HZ           400000UL

#define PIN_SDA              4
#define PIN_SCL              5

static bool    initialized;
static bool    restart_pending;
static uint8_t target = 0xFF;

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz)
{
    if (bus != 0 || hz == 0 || hz > I2C_MAX_HZ) {
        return BR_ERR_INVALID;
    }

    uint32_t period = (RP_CLK_PERI_HZ + hz / 2) / hz;
    uint32_t lcnt   = period * 3 / 5;
    uint32_t hcnt   = period - lcnt;
    if (lcnt < 8 || hcnt < 6) {
        return BR_ERR_INVALID;
    }

    rp_unreset(RESET_I2C0);

    rp_pin_function(PIN_SDA, GPIO_FUNC_I2C, PAD_PUE);
    rp_pin_function(PIN_SCL, GPIO_FUNC_I2C, PAD_PUE);

    IC_ENABLE = 0;
    while (IC_ENABLE_STATUS & 1UL) { }

    IC_CON = IC_CON_MASTER | IC_CON_RESTART_EN | IC_CON_SLAVE_DIS |
             IC_CON_TX_EMPTY_CTRL |
             (hz <= I2C_SS_MAX_HZ ? IC_CON_SPEED_SS : IC_CON_SPEED_FS);

    if (hz <= I2C_SS_MAX_HZ) {
        IC_SS_SCL_HCNT = hcnt;
        IC_SS_SCL_LCNT = lcnt;
    } else {
        IC_FS_SCL_HCNT = hcnt;
        IC_FS_SCL_LCNT = lcnt;
    }
    IC_FS_SPKLEN = lcnt < 16 ? 1 : lcnt / 16;
    IC_TX_TL = 0;
    IC_RX_TL = 0;
    IC_ENABLE = 1;

    target          = 0xFF;
    restart_pending = false;
    initialized     = true;
    return BR_OK;
}

static void set_target(uint8_t addr)
{
    if (addr == target) {
        return;
    }
    IC_ENABLE = 0;
    while (IC_ENABLE_STATUS & 1UL) { }
    IC_TAR = addr;
    IC_ENABLE = 1;
    target = addr;
}

static br_err_t fail_if_aborted(void)
{
    if (IC_RAW_INTR_STAT & IC_INTR_TX_ABRT) {
        (void)IC_CLR_TX_ABRT;
        restart_pending = false;
        return BR_ERR_BUSY;
    }
    return BR_OK;
}

static br_err_t wait_status(uint32_t flag)
{
    uint32_t spins = RP_WAIT_LIMIT;
    while (!(IC_STATUS & flag)) {
        br_err_t err = fail_if_aborted();
        if (err != BR_OK) {
            return err;
        }
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    return BR_OK;
}

static br_err_t wait_stop(void)
{
    uint32_t spins = RP_WAIT_LIMIT;
    while (1) {
        br_err_t err = fail_if_aborted();
        if (err != BR_OK) {
            (void)IC_CLR_STOP_DET;
            return err;
        }
        if (IC_RAW_INTR_STAT & IC_INTR_STOP_DET) {
            break;
        }
        if (--spins == 0) {
            return BR_ERR_TIMEOUT;
        }
    }
    (void)IC_CLR_STOP_DET;
    restart_pending = false;
    return BR_OK;
}

br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data,
                          size_t len, bool stop)
{
    if (bus != 0 || !initialized || data == NULL || len == 0 || addr > 0x7F) {
        return BR_ERR_INVALID;
    }

    set_target(addr);

    for (size_t i = 0; i < len; i++) {
        br_err_t err = wait_status(IC_STATUS_TFNF);
        if (err != BR_OK) {
            return err;
        }

        uint32_t cmd = data[i];
        if (i == 0 && restart_pending) {
            cmd |= IC_DATA_CMD_RESTART;
        }
        if (i == len - 1 && stop) {
            cmd |= IC_DATA_CMD_STOP;
        }
        IC_DATA_CMD = cmd;
    }

    if (stop) {
        return wait_stop();
    }

    br_err_t err = wait_status(IC_STATUS_TFE);
    if (err == BR_OK) {
        restart_pending = true;
    }
    return err;
}

br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    if (bus != 0 || !initialized || data == NULL || len == 0 || addr > 0x7F) {
        return BR_ERR_INVALID;
    }

    set_target(addr);

    for (size_t i = 0; i < len; i++) {
        br_err_t err = wait_status(IC_STATUS_TFNF);
        if (err != BR_OK) {
            return err;
        }

        uint32_t cmd = IC_DATA_CMD_READ;
        if (i == 0 && restart_pending) {
            cmd |= IC_DATA_CMD_RESTART;
        }
        if (i == len - 1) {
            cmd |= IC_DATA_CMD_STOP;
        }
        IC_DATA_CMD = cmd;

        err = wait_status(IC_STATUS_RFNE);
        if (err != BR_OK) {
            return err;
        }
        data[i] = (uint8_t)IC_DATA_CMD;
    }

    return wait_stop();
}

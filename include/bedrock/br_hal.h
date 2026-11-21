/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_HAL_H
#define BR_HAL_H

#include "br_types.h"

/*
 * Hardware Abstraction Layer Interface
 *
 * Each architecture implements these functions independently
 * in arch/<arch_name>/. The kernel never accesses hardware
 * directly -- only through this interface.
 */

/* Timer HAL -- tickless time source */

void br_hal_timer_init(void);
br_time_t br_hal_timer_get_us(void);
void br_hal_timer_set_alarm(br_time_t abs_us);
void br_hal_timer_cancel_alarm(void);

/* Interrupt control HAL */

uint32_t br_hal_irq_disable(void);
void br_hal_irq_restore(uint32_t state);
bool br_hal_in_isr(void);

/* Context switch HAL */

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg);
void br_hal_context_switch(void **old_sp, void **new_sp);
void br_hal_start_first_task(void *sp) __attribute__((noreturn));
void br_hal_check_stack_overflow(br_tcb_t *tcb);

/* Board / early init HAL */

void br_hal_board_init(void);

/* GPIO HAL */

typedef enum {
    BR_GPIO_INPUT          = 0,
    BR_GPIO_INPUT_PULLUP   = 1,
    BR_GPIO_INPUT_PULLDOWN = 2,
    BR_GPIO_OUTPUT         = 3
} br_gpio_mode_t;

br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode);
br_err_t br_hal_gpio_write(uint8_t port, uint8_t pin, bool level);
br_err_t br_hal_gpio_read(uint8_t port, uint8_t pin, bool *level);

/* SPI HAL -- master, blocking, chip select is a GPIO owned by the caller */

#define BR_SPI_CPHA  0x01
#define BR_SPI_CPOL  0x02
#define BR_SPI_LOOP  0x20

br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode);
br_err_t br_hal_spi_transfer(uint8_t bus, const uint8_t *tx, uint8_t *rx,
                             size_t len);

/* I2C HAL -- master, blocking, 7-bit addresses */

br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz);
br_err_t br_hal_i2c_write(uint8_t bus, uint8_t addr, const uint8_t *data,
                          size_t len, bool stop);
br_err_t br_hal_i2c_read(uint8_t bus, uint8_t addr, uint8_t *data, size_t len);

/* PWM and input capture HAL */

typedef enum {
    BR_CAPTURE_RISING  = 0,
    BR_CAPTURE_FALLING = 1
} br_capture_edge_t;

br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille);
br_err_t br_hal_pwm_stop(uint8_t ch);
br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge);
br_err_t br_hal_capture_stop(uint8_t ch);
br_err_t br_hal_capture_read(uint8_t ch, uint32_t *period_us);

/* Network HAL -- the board network interface, NULL if the board has none */

struct br_netif;
struct br_netif *br_hal_netif_default(void);

/* Reset HAL -- restart the system, does not return */

void br_hal_reset(void) __attribute__((noreturn));

/* Panic HAL -- architecture-level unrecoverable halt */

void br_hal_panic(const char *msg, const char *file, int line)
    __attribute__((noreturn));

#endif /* BR_HAL_H */

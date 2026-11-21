/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_GPIO_H
#define BR_GPIO_H

#include "bedrock/bedrock.h"

#define BR_GPIO_IOCTL_CONFIGURE  0x0101
#define BR_GPIO_IOCTL_WRITE      0x0102
#define BR_GPIO_IOCTL_READ       0x0103
#define BR_GPIO_IOCTL_TOGGLE     0x0104

typedef struct {
    uint8_t pin;
    uint8_t value;
} br_gpio_arg_t;

typedef struct {
    br_device_t dev;
    uint8_t     port;
} br_gpio_dev_t;

br_err_t br_gpio_register(br_gpio_dev_t *gpio, const char *name, uint8_t port);
br_err_t br_gpio_configure(br_device_t *dev, uint8_t pin, br_gpio_mode_t mode);
br_err_t br_gpio_write(br_device_t *dev, uint8_t pin, bool level);
br_err_t br_gpio_read(br_device_t *dev, uint8_t pin, bool *level);
br_err_t br_gpio_toggle(br_device_t *dev, uint8_t pin);

#endif

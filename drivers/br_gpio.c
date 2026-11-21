/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_gpio.h"

static br_err_t gpio_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    br_gpio_dev_t *gpio = dev->priv;
    br_gpio_arg_t *a = arg;
    bool level;
    br_err_t err;

    if (a == NULL) {
        return BR_ERR_INVALID;
    }

    switch (cmd) {
    case BR_GPIO_IOCTL_CONFIGURE:
        return br_hal_gpio_configure(gpio->port, a->pin, (br_gpio_mode_t)a->value);
    case BR_GPIO_IOCTL_WRITE:
        return br_hal_gpio_write(gpio->port, a->pin, a->value != 0);
    case BR_GPIO_IOCTL_READ:
        err = br_hal_gpio_read(gpio->port, a->pin, &level);
        a->value = level ? 1 : 0;
        return err;
    case BR_GPIO_IOCTL_TOGGLE:
        err = br_hal_gpio_read(gpio->port, a->pin, &level);
        if (err != BR_OK) {
            return err;
        }
        return br_hal_gpio_write(gpio->port, a->pin, !level);
    default:
        return BR_ERR_INVALID;
    }
}

static const br_device_ops_t gpio_ops = {
    .ioctl = gpio_ioctl,
};

br_err_t br_gpio_register(br_gpio_dev_t *gpio, const char *name, uint8_t port)
{
    if (gpio == NULL) {
        return BR_ERR_INVALID;
    }
    gpio->port = port;
    return br_device_register(&gpio->dev, name, &gpio_ops, gpio);
}

br_err_t br_gpio_configure(br_device_t *dev, uint8_t pin, br_gpio_mode_t mode)
{
    br_gpio_arg_t a = { .pin = pin, .value = (uint8_t)mode };
    return br_device_ioctl(dev, BR_GPIO_IOCTL_CONFIGURE, &a);
}

br_err_t br_gpio_write(br_device_t *dev, uint8_t pin, bool level)
{
    br_gpio_arg_t a = { .pin = pin, .value = level ? 1 : 0 };
    return br_device_ioctl(dev, BR_GPIO_IOCTL_WRITE, &a);
}

br_err_t br_gpio_read(br_device_t *dev, uint8_t pin, bool *level)
{
    br_gpio_arg_t a = { .pin = pin, .value = 0 };
    br_err_t err = br_device_ioctl(dev, BR_GPIO_IOCTL_READ, &a);
    if (err == BR_OK && level != NULL) {
        *level = a.value != 0;
    }
    return err;
}

br_err_t br_gpio_toggle(br_device_t *dev, uint8_t pin)
{
    br_gpio_arg_t a = { .pin = pin, .value = 0 };
    return br_device_ioctl(dev, BR_GPIO_IOCTL_TOGGLE, &a);
}

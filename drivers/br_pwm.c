/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_pwm.h"

#define PWM_DUTY_MAX  1000

static br_err_t pwm_close(br_device_t *dev)
{
    br_pwm_dev_t *pwm = dev->priv;
    return br_hal_pwm_stop(pwm->ch);
}

static br_err_t pwm_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    br_pwm_dev_t *pwm = dev->priv;

    switch (cmd) {
    case BR_PWM_IOCTL_SET: {
        br_pwm_config_t *cfg = arg;
        if (cfg == NULL || cfg->freq_hz == 0 || cfg->duty_permille > PWM_DUTY_MAX) {
            return BR_ERR_INVALID;
        }
        return br_hal_pwm_start(pwm->ch, cfg->freq_hz, cfg->duty_permille);
    }
    case BR_PWM_IOCTL_STOP:
        return br_hal_pwm_stop(pwm->ch);
    default:
        return BR_ERR_INVALID;
    }
}

static const br_device_ops_t pwm_ops = {
    .close = pwm_close,
    .ioctl = pwm_ioctl,
};

br_err_t br_pwm_register(br_pwm_dev_t *pwm, const char *name, uint8_t ch)
{
    if (pwm == NULL) {
        return BR_ERR_INVALID;
    }
    pwm->ch = ch;
    return br_device_register(&pwm->dev, name, &pwm_ops, pwm);
}

br_err_t br_pwm_set(br_device_t *dev, uint32_t freq_hz, uint16_t duty_permille)
{
    br_pwm_config_t cfg = { .freq_hz = freq_hz, .duty_permille = duty_permille };
    return br_device_ioctl(dev, BR_PWM_IOCTL_SET, &cfg);
}

br_err_t br_pwm_stop(br_device_t *dev)
{
    return br_device_ioctl(dev, BR_PWM_IOCTL_STOP, NULL);
}

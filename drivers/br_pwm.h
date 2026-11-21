/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_PWM_H
#define BR_PWM_H

#include "bedrock/bedrock.h"

#define BR_PWM_IOCTL_SET   0x0401
#define BR_PWM_IOCTL_STOP  0x0402

#define BR_CAPTURE_IOCTL_SET_EDGE  0x0501

typedef struct {
    uint32_t freq_hz;
    uint16_t duty_permille;
} br_pwm_config_t;

typedef struct {
    br_device_t dev;
    uint8_t     ch;
} br_pwm_dev_t;

typedef struct {
    br_device_t       dev;
    uint8_t           ch;
    br_capture_edge_t edge;
} br_capture_dev_t;

br_err_t br_pwm_register(br_pwm_dev_t *pwm, const char *name, uint8_t ch);
br_err_t br_pwm_set(br_device_t *dev, uint32_t freq_hz, uint16_t duty_permille);
br_err_t br_pwm_stop(br_device_t *dev);

br_err_t br_capture_register(br_capture_dev_t *cap, const char *name, uint8_t ch);
br_err_t br_capture_set_edge(br_device_t *dev, br_capture_edge_t edge);
br_err_t br_capture_read_period(br_device_t *dev, uint32_t *period_us,
                                br_time_t timeout);

#endif

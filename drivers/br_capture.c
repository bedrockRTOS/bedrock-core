/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_pwm.h"

#define CAPTURE_POLL_US  100

static br_err_t capture_open(br_device_t *dev)
{
    br_capture_dev_t *cap = dev->priv;
    return br_hal_capture_start(cap->ch, cap->edge);
}

static br_err_t capture_close(br_device_t *dev)
{
    br_capture_dev_t *cap = dev->priv;
    return br_hal_capture_stop(cap->ch);
}

static int capture_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    br_capture_dev_t *cap = dev->priv;

    if (len < sizeof(uint32_t)) {
        return BR_ERR_INVALID;
    }

    br_time_t start = br_uptime_us();

    while (1) {
        br_err_t err = br_hal_capture_read(cap->ch, buf);
        if (err == BR_OK) {
            return (int)sizeof(uint32_t);
        }
        if (err != BR_ERR_BUSY) {
            return (int)err;
        }
        if (timeout != BR_TIME_INFINITE && br_uptime_us() - start >= timeout) {
            return BR_ERR_TIMEOUT;
        }
        br_sleep_us(CAPTURE_POLL_US);
    }
}

static br_err_t capture_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    br_capture_dev_t *cap = dev->priv;

    if (cmd != BR_CAPTURE_IOCTL_SET_EDGE || arg == NULL) {
        return BR_ERR_INVALID;
    }

    br_capture_edge_t edge = *(br_capture_edge_t *)arg;
    if (edge != BR_CAPTURE_RISING && edge != BR_CAPTURE_FALLING) {
        return BR_ERR_INVALID;
    }

    br_err_t err = br_hal_capture_start(cap->ch, edge);
    if (err == BR_OK) {
        cap->edge = edge;
    }
    return err;
}

static const br_device_ops_t capture_ops = {
    .open  = capture_open,
    .close = capture_close,
    .read  = capture_read,
    .ioctl = capture_ioctl,
};

br_err_t br_capture_register(br_capture_dev_t *cap, const char *name, uint8_t ch)
{
    if (cap == NULL) {
        return BR_ERR_INVALID;
    }
    cap->ch   = ch;
    cap->edge = BR_CAPTURE_RISING;
    br_device_user_allow_ioctl(BR_CAPTURE_IOCTL_SET_EDGE, sizeof(br_capture_edge_t));
    return br_device_register(&cap->dev, name, &capture_ops, cap);
}

br_err_t br_capture_set_edge(br_device_t *dev, br_capture_edge_t edge)
{
    return br_device_ioctl(dev, BR_CAPTURE_IOCTL_SET_EDGE, &edge);
}

br_err_t br_capture_read_period(br_device_t *dev, uint32_t *period_us,
                                br_time_t timeout)
{
    int n = br_device_read(dev, period_us, sizeof(*period_us), timeout);
    return n < 0 ? (br_err_t)n : BR_OK;
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "bedrock/br_syscall.h"

static br_device_t *device_list;

static bool name_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

br_err_t br_device_register(br_device_t *dev, const char *name,
                            const br_device_ops_t *ops, void *priv)
{
    if (dev == NULL || name == NULL || ops == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();

    for (br_device_t *d = device_list; d != NULL; d = d->next) {
        if (d == dev || name_equal(d->name, name)) {
            br_hal_irq_restore(key);
            return BR_ERR_BUSY;
        }
    }

    dev->name       = name;
    dev->ops        = ops;
    dev->priv       = priv;
    dev->open_count = 0;
    dev->user_ok    = false;
    dev->next       = device_list;
    device_list     = dev;

    br_hal_irq_restore(key);
    return BR_OK;
}

br_device_t *br_device_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    uint32_t key = br_hal_irq_disable();

    br_device_t *d = device_list;
    while (d != NULL && !name_equal(d->name, name)) {
        d = d->next;
    }

    br_hal_irq_restore(key);
    return d;
}

br_err_t br_device_open(br_device_t *dev)
{
    if (dev == NULL || br_hal_in_isr()) {
        return BR_ERR_INVALID;
    }
    if (dev->open_count == UINT8_MAX) {
        return BR_ERR_OVERFLOW;
    }

    if (dev->open_count == 0 && dev->ops->open != NULL) {
        br_err_t err = dev->ops->open(dev);
        if (err != BR_OK) {
            return err;
        }
    }

    dev->open_count++;
    return BR_OK;
}

br_err_t br_device_close(br_device_t *dev)
{
    if (dev == NULL || dev->open_count == 0) {
        return BR_ERR_INVALID;
    }

    if (dev->open_count == 1 && dev->ops->close != NULL) {
        br_err_t err = dev->ops->close(dev);
        if (err != BR_OK) {
            return err;
        }
    }

    dev->open_count--;
    return BR_OK;
}

int br_device_read(br_device_t *dev, void *buf, size_t len, br_time_t timeout)
{
    if (dev == NULL || buf == NULL || dev->open_count == 0 ||
        dev->ops->read == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->read(dev, buf, len, timeout);
}

int br_device_write(br_device_t *dev, const void *buf, size_t len,
                    br_time_t timeout)
{
    if (dev == NULL || buf == NULL || dev->open_count == 0 ||
        dev->ops->write == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->write(dev, buf, len, timeout);
}

br_err_t br_device_ioctl(br_device_t *dev, uint32_t cmd, void *arg)
{
    if (dev == NULL || dev->open_count == 0 || dev->ops->ioctl == NULL) {
        return BR_ERR_INVALID;
    }
    return dev->ops->ioctl(dev, cmd, arg);
}

#if CONFIG_MPU

#define DEVICE_NAME_MAX    16
#define IOCTL_ALLOW_MAX    24

typedef struct {
    br_device_t *dev;
    br_tid_t     owner;
    bool         used;
} dev_slot_t;

typedef struct {
    uint32_t h;
    uint32_t buf;
    uint32_t len;
    uint32_t to_lo;
    uint32_t to_hi;
} io_args_t;

static dev_slot_t slots[CONFIG_USER_OBJECTS];

static struct {
    uint32_t cmd;
    size_t   size;
} allowed[IOCTL_ALLOW_MAX];
static uint8_t allowed_count;

void br_device_set_user(br_device_t *dev, bool allow)
{
    if (dev != NULL) {
        dev->user_ok = allow;
    }
}

void br_device_user_allow_ioctl(uint32_t cmd, size_t size)
{
    uint32_t key = br_hal_irq_disable();

    for (int i = 0; i < allowed_count; i++) {
        if (allowed[i].cmd == cmd) {
            br_hal_irq_restore(key);
            return;
        }
    }
    if (allowed_count < IOCTL_ALLOW_MAX) {
        allowed[allowed_count].cmd  = cmd;
        allowed[allowed_count].size = size;
        allowed_count++;
    }

    br_hal_irq_restore(key);
}

static dev_slot_t *slot_get(const br_tcb_t *t, uint32_t h)
{
    if (h >= CONFIG_USER_OBJECTS || !slots[h].used || slots[h].owner != t->id) {
        return NULL;
    }
    return &slots[h];
}

static int32_t io_args(const br_tcb_t *t, uint32_t ptr, io_args_t *out)
{
    if ((ptr % 4) != 0 || !br_user_access_ok(t, (const void *)ptr, sizeof(*out), BR_MEM_R)) {
        return BR_ERR_PERM;
    }
    *out = *(const io_args_t *)ptr;
    return BR_OK;
}

static int32_t dev_open(const br_tcb_t *t, uint32_t name_ptr)
{
    char name[DEVICE_NAME_MAX];

    int n = br_user_copy_string(t, (const void *)name_ptr, name, sizeof(name));
    if (n < 0) {
        return n;
    }

    br_device_t *dev = br_device_find(name);
    if (dev == NULL) {
        return BR_ERR_NOENT;
    }
    if (!dev->user_ok) {
        return BR_ERR_PERM;
    }

    for (int i = 0; i < CONFIG_USER_OBJECTS; i++) {
        if (!slots[i].used) {
            br_err_t err = br_device_open(dev);
            if (err != BR_OK) {
                return err;
            }
            slots[i].dev   = dev;
            slots[i].owner = t->id;
            slots[i].used  = true;
            return i;
        }
    }
    return BR_ERR_NOMEM;
}

static int32_t dev_ioctl(const br_tcb_t *t, const uint32_t *a)
{
    dev_slot_t *s = slot_get(t, a[1]);
    if (s == NULL) {
        return BR_ERR_INVALID;
    }

    bool found = false;
    for (int i = 0; i < allowed_count; i++) {
        if (allowed[i].cmd == a[2] && allowed[i].size == a[4]) {
            found = true;
        }
    }
    if (!found) {
        return BR_ERR_PERM;
    }

    if (a[4] != 0 && (a[3] % 4 != 0 ||
                      !br_user_access_ok(t, (const void *)a[3], a[4], BR_MEM_R | BR_MEM_W))) {
        return BR_ERR_PERM;
    }
    return br_device_ioctl(s->dev, a[2], a[4] != 0 ? (void *)a[3] : NULL);
}

static uint32_t handler(br_tcb_t *t, const uint32_t *a)
{
    int32_t r = BR_ERR_INVALID;

    switch (a[0]) {
    case BR_SYS_DEV_OPEN:
        r = dev_open(t, a[1]);
        break;

    case BR_SYS_DEV_CLOSE: {
        dev_slot_t *s = slot_get(t, a[1]);
        if (s != NULL) {
            r = br_device_close(s->dev);
            s->used = false;
        }
        break;
    }

    case BR_SYS_DEV_READ:
    case BR_SYS_DEV_WRITE: {
        io_args_t io;
        r = io_args(t, a[1], &io);
        if (r != BR_OK) {
            break;
        }

        dev_slot_t *s = slot_get(t, io.h);
        bool rd = a[0] == BR_SYS_DEV_READ;
        if (s == NULL) {
            r = BR_ERR_INVALID;
        } else if (!br_user_access_ok(t, (const void *)io.buf, io.len, rd ? BR_MEM_W : BR_MEM_R)) {
            r = BR_ERR_PERM;
        } else {
            br_time_t timeout = ((br_time_t)io.to_hi << 32) | io.to_lo;
            r = rd ? br_device_read(s->dev, (void *)io.buf, io.len, timeout)
                   : br_device_write(s->dev, (const void *)io.buf, io.len, timeout);
        }
        break;
    }

    case BR_SYS_DEV_IOCTL:
        r = dev_ioctl(t, a);
        break;

    default:
        break;
    }

    return (uint32_t)r;
}

static void release(br_tid_t tid)
{
    for (int i = 0; i < CONFIG_USER_OBJECTS; i++) {
        if (slots[i].used && slots[i].owner == tid) {
            br_device_close(slots[i].dev);
            slots[i].used = false;
        }
    }
}

br_err_t br_device_enable_user_calls(void)
{
    br_err_t err = br_syscall_register(BR_SYS_DEV_OPEN, 5, handler);
    if (err == BR_OK) {
        err = br_user_release_register(release);
    }
    return err;
}

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "bedrock/br_syscall.h"

#if CONFIG_MPU

extern br_tcb_t *br_sched_current(void);
extern void      br_sched_reschedule(void);
extern br_tcb_t *br_task_lookup(br_tid_t tid);

typedef struct {
    br_sem_t sem;
    bool     used;
} user_sem_t;

typedef struct {
    br_mutex_t mutex;
    bool       used;
} user_mutex_t;

typedef struct {
    br_mqueue_t mq;
    bool        used;
} user_mq_t;

static uint8_t      kstacks[CONFIG_MAX_USER_TASKS][CONFIG_KSTACK_SIZE] __attribute__((aligned(8)));
static bool         kstack_used[CONFIG_MAX_USER_TASKS];
static user_sem_t   sems[CONFIG_USER_OBJECTS];
static user_mutex_t mutexes[CONFIG_USER_OBJECTS];
static user_mq_t    mqs[CONFIG_USER_OBJECTS];
static uint8_t      mq_arena[CONFIG_USER_MQ_ARENA] __attribute__((aligned(4)));
static size_t       mq_arena_used;

#define UART_PUTS_MAX     256
#define SYSCALL_RANGES    6
#define RELEASE_HOOKS     4

static struct {
    uint32_t             first;
    uint32_t             count;
    br_syscall_handler_t handler;
} ranges[SYSCALL_RANGES];
static uint8_t           range_count;

static br_user_release_t release_hooks[RELEASE_HOOKS];
static uint8_t           release_count;

br_err_t br_syscall_register(uint32_t first, uint32_t count, br_syscall_handler_t handler)
{
    if (handler == NULL || count == 0 || first < BR_SYS_COUNT || first + count < first) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    br_err_t err = BR_OK;

    for (int i = 0; i < range_count; i++) {
        if (first < ranges[i].first + ranges[i].count && ranges[i].first < first + count) {
            err = ranges[i].handler == handler ? BR_OK : BR_ERR_BUSY;
            br_hal_irq_restore(key);
            return err;
        }
    }
    if (range_count == SYSCALL_RANGES) {
        err = BR_ERR_NOMEM;
    } else {
        ranges[range_count].first   = first;
        ranges[range_count].count   = count;
        ranges[range_count].handler = handler;
        range_count++;
    }

    br_hal_irq_restore(key);
    return err;
}

br_err_t br_user_release_register(br_user_release_t hook)
{
    if (hook == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    br_err_t err = BR_OK;

    for (int i = 0; i < release_count; i++) {
        if (release_hooks[i] == hook) {
            br_hal_irq_restore(key);
            return BR_OK;
        }
    }
    if (release_count == RELEASE_HOOKS) {
        err = BR_ERR_NOMEM;
    } else {
        release_hooks[release_count++] = hook;
    }

    br_hal_irq_restore(key);
    return err;
}

void br_user_release_all(br_tid_t tid)
{
    for (int i = 0; i < release_count; i++) {
        release_hooks[i](tid);
    }
}

void *br_user_kstack_alloc(void)
{
    uint32_t key = br_hal_irq_disable();
    void *top = NULL;

    for (int i = 0; i < CONFIG_MAX_USER_TASKS; i++) {
        if (!kstack_used[i]) {
            kstack_used[i] = true;
            top = &kstacks[i][CONFIG_KSTACK_SIZE];
            break;
        }
    }

    br_hal_irq_restore(key);
    return top;
}

void br_user_kstack_free(void *top)
{
    if (top == NULL) {
        return;
    }

    uint32_t key = br_hal_irq_disable();
    for (int i = 0; i < CONFIG_MAX_USER_TASKS; i++) {
        if (top == &kstacks[i][CONFIG_KSTACK_SIZE]) {
            kstack_used[i] = false;
        }
    }
    br_hal_irq_restore(key);
}

br_err_t br_task_grant(br_tid_t tid, void *base, size_t size, uint8_t perms)
{
    br_tcb_t *tcb = br_task_lookup(tid);
    br_region_t r = { (uintptr_t)base, size, perms };

    if (tcb == NULL || !tcb->user || (perms & ~(BR_MEM_R | BR_MEM_W)) || perms == 0 ||
        !br_hal_mpu_region_valid(&r)) {
        return BR_ERR_INVALID;
    }
    if (tcb->nregions >= BR_TASK_REGIONS) {
        return BR_ERR_NOMEM;
    }

    uint32_t key = br_hal_irq_disable();
    tcb->regions[tcb->nregions++] = r;
    br_hal_irq_restore(key);
    return BR_OK;
}

static bool region_allows(const br_region_t *r, uintptr_t p, size_t len, uint8_t need)
{
    return (r->perms & need) == need && p >= r->base && p - r->base <= r->size &&
           len <= r->size - (p - r->base);
}

static bool access_ok(const br_tcb_t *t, uintptr_t p, size_t len, uint8_t need)
{
    if (len == 0) {
        return true;
    }
    if (p + len < p) {
        return false;
    }

    for (int i = 0; i < t->nregions; i++) {
        br_region_t r = t->regions[i];
        if (i == 0) {
            r.base += BR_USER_STACK_GUARD(r.size);
            r.size -= BR_USER_STACK_GUARD(r.size);
        }
        if (region_allows(&r, p, len, need)) {
            return true;
        }
    }

    br_region_t shared;
    br_hal_mpu_user_data_region(&shared);
    shared.perms = BR_MEM_R | BR_MEM_W;
    if (region_allows(&shared, p, len, need)) {
        return true;
    }

    if (need == BR_MEM_R) {
        br_region_t code;
        br_hal_mpu_code_region(&code);
        code.perms = BR_MEM_R;
        return region_allows(&code, p, len, need);
    }
    return false;
}

bool br_user_access_ok(const br_tcb_t *t, const void *ptr, size_t len, uint8_t need)
{
    return access_ok(t, (uintptr_t)ptr, len, need);
}

int br_user_copy_string(const br_tcb_t *t, const void *src, char *dst, size_t max)
{
    const char *s = src;

    for (size_t i = 0; i < max; i++) {
        if (!access_ok(t, (uintptr_t)(s + i), 1, BR_MEM_R)) {
            return BR_ERR_PERM;
        }
        dst[i] = s[i];
        if (s[i] == '\0') {
            return (int)i;
        }
    }
    return BR_ERR_INVALID;
}

static br_time_t make_time(uint32_t lo, uint32_t hi)
{
    return ((br_time_t)hi << 32) | lo;
}

static user_sem_t *sem_get(uint32_t h)
{
    return (h < CONFIG_USER_OBJECTS && sems[h].used) ? &sems[h] : NULL;
}

static user_mutex_t *mutex_get(uint32_t h)
{
    return (h < CONFIG_USER_OBJECTS && mutexes[h].used) ? &mutexes[h] : NULL;
}

static user_mq_t *mq_get(uint32_t h)
{
    return (h < CONFIG_USER_OBJECTS && mqs[h].used) ? &mqs[h] : NULL;
}

static int32_t sem_create(uint32_t initial, uint32_t max)
{
    for (int i = 0; i < CONFIG_USER_OBJECTS; i++) {
        if (!sems[i].used) {
            br_err_t err = br_sem_init(&sems[i].sem, (int32_t)initial, (int32_t)max);
            if (err != BR_OK) {
                return err;
            }
            sems[i].used = true;
            return i;
        }
    }
    return BR_ERR_NOMEM;
}

static int32_t mutex_create(void)
{
    for (int i = 0; i < CONFIG_USER_OBJECTS; i++) {
        if (!mutexes[i].used) {
            br_mutex_init(&mutexes[i].mutex);
            mutexes[i].used = true;
            return i;
        }
    }
    return BR_ERR_NOMEM;
}

static int32_t mq_create(uint32_t msg_size, uint32_t max_msgs)
{
    if (msg_size == 0 || max_msgs == 0 || msg_size > CONFIG_USER_MQ_ARENA ||
        max_msgs > CONFIG_USER_MQ_ARENA) {
        return BR_ERR_INVALID;
    }

    size_t bytes = ((size_t)msg_size * max_msgs + 3U) & ~(size_t)3U;
    if (bytes > CONFIG_USER_MQ_ARENA - mq_arena_used) {
        return BR_ERR_NOMEM;
    }

    for (int i = 0; i < CONFIG_USER_OBJECTS; i++) {
        if (!mqs[i].used) {
            br_err_t err = br_mqueue_init(&mqs[i].mq, &mq_arena[mq_arena_used], msg_size, max_msgs);
            if (err != BR_OK) {
                return err;
            }
            mq_arena_used += bytes;
            mqs[i].used = true;
            return i;
        }
    }
    return BR_ERR_NOMEM;
}

static int32_t uart_puts(const br_tcb_t *t, uintptr_t p)
{
    for (size_t i = 0; i < UART_PUTS_MAX; i++) {
        if (!access_ok(t, p + i, 1, BR_MEM_R)) {
            return BR_ERR_PERM;
        }
        char c = *(const char *)(p + i);
        if (c == '\0') {
            return BR_OK;
        }
        if (c == '\n') {
            br_uart_putc('\r');
        }
        br_uart_putc(c);
    }
    return BR_ERR_INVALID;
}

uint32_t br_syscall_dispatch(const uint32_t *a);

uint32_t br_syscall_dispatch(const uint32_t *a)
{
    br_tcb_t *t = br_sched_current();
    int32_t   r = BR_ERR_INVALID;

    switch (a[0]) {
    case BR_SYS_SELF:
        r = t->id;
        break;
    case BR_SYS_YIELD:
        br_task_yield();
        r = BR_OK;
        break;
    case BR_SYS_SLEEP_US:
        br_sleep_us(make_time(a[1], a[2]));
        r = BR_OK;
        break;
    case BR_SYS_UPTIME_US:
        if (a[1] % 4 != 0 || !access_ok(t, a[1], 8, BR_MEM_W)) {
            r = BR_ERR_PERM;
        } else {
            br_time_t now = br_uptime_us();
            ((uint32_t *)a[1])[0] = (uint32_t)now;
            ((uint32_t *)a[1])[1] = (uint32_t)(now >> 32);
            r = BR_OK;
        }
        break;
    case BR_SYS_EXIT:
        br_task_suspend(t->id);
        r = BR_OK;
        break;

    case BR_SYS_SEM_CREATE:
        r = sem_create(a[1], a[2]);
        break;
    case BR_SYS_SEM_TAKE: {
        user_sem_t *s = sem_get(a[1]);
        r = s ? br_sem_take(&s->sem, make_time(a[2], a[3])) : BR_ERR_INVALID;
        break;
    }
    case BR_SYS_SEM_GIVE: {
        user_sem_t *s = sem_get(a[1]);
        r = s ? br_sem_give(&s->sem) : BR_ERR_INVALID;
        break;
    }

    case BR_SYS_MUTEX_CREATE:
        r = mutex_create();
        break;
    case BR_SYS_MUTEX_LOCK: {
        user_mutex_t *m = mutex_get(a[1]);
        r = m ? br_mutex_lock(&m->mutex, make_time(a[2], a[3])) : BR_ERR_INVALID;
        break;
    }
    case BR_SYS_MUTEX_UNLOCK: {
        user_mutex_t *m = mutex_get(a[1]);
        r = m ? br_mutex_unlock(&m->mutex) : BR_ERR_INVALID;
        break;
    }

    case BR_SYS_MQ_CREATE:
        r = mq_create(a[1], a[2]);
        break;
    case BR_SYS_MQ_SEND: {
        user_mq_t *q = mq_get(a[1]);
        if (q == NULL) {
            r = BR_ERR_INVALID;
        } else if (!access_ok(t, a[2], q->mq.msg_size, BR_MEM_R)) {
            r = BR_ERR_PERM;
        } else {
            r = br_mqueue_send(&q->mq, (const void *)a[2], make_time(a[3], a[4]));
        }
        break;
    }
    case BR_SYS_MQ_RECV: {
        user_mq_t *q = mq_get(a[1]);
        if (q == NULL) {
            r = BR_ERR_INVALID;
        } else if (!access_ok(t, a[2], q->mq.msg_size, BR_MEM_W)) {
            r = BR_ERR_PERM;
        } else {
            r = br_mqueue_recv(&q->mq, (void *)a[2], make_time(a[3], a[4]));
        }
        break;
    }

    case BR_SYS_UART_PUTC:
        br_uart_putc((char)a[1]);
        r = BR_OK;
        break;
    case BR_SYS_UART_PUTS:
        r = uart_puts(t, a[1]);
        break;
    case BR_SYS_UART_GETC:
        if (!access_ok(t, a[1], 1, BR_MEM_W)) {
            r = BR_ERR_PERM;
        } else {
            r = br_uart_getc((char *)a[1], make_time(a[2], a[3]));
        }
        break;
    default:
        for (int i = 0; i < range_count; i++) {
            if (a[0] >= ranges[i].first && a[0] - ranges[i].first < ranges[i].count) {
                return ranges[i].handler(t, a);
            }
        }
        r = BR_ERR_INVALID;
        break;
    }

    return (uint32_t)r;
}

void br_task_fault(br_tcb_t *tcb, uint8_t kind, uintptr_t addr)
{
    tcb->faulted    = true;
    tcb->fault_kind = kind;
    tcb->fault_addr = addr;
    tcb->state      = BR_TASK_SUSPENDED;
    br_sched_reschedule();
}

#endif

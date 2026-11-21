/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "arch/sys_arch.h"

#define BR_LWIP_MAX_THREADS    1
#define BR_LWIP_STACK_SIZE     3072

static uint8_t  thread_stacks[BR_LWIP_MAX_THREADS][BR_LWIP_STACK_SIZE] __attribute__((aligned(8)));
static uint8_t  threads_used;
static uint32_t rand_state = 0x2545F491UL;

uint32_t br_lwip_rand(void)
{
    uint32_t key = br_hal_irq_disable();
    rand_state ^= (uint32_t)br_uptime_us();
    rand_state ^= rand_state << 13;
    rand_state ^= rand_state >> 17;
    rand_state ^= rand_state << 5;
    uint32_t v = rand_state;
    br_hal_irq_restore(key);
    return v;
}

void sys_init(void)
{
}

u32_t sys_now(void)
{
    return (u32_t)(br_uptime_us() / 1000U);
}

u32_t sys_jiffies(void)
{
    return sys_now();
}

sys_prot_t sys_arch_protect(void)
{
    return br_hal_irq_disable();
}

void sys_arch_unprotect(sys_prot_t pval)
{
    br_hal_irq_restore(pval);
}

static br_time_t to_timeout(u32_t ms)
{
    return ms == 0 ? BR_TIME_INFINITE : BR_MSEC(ms);
}

static u32_t elapsed_ms(br_time_t start)
{
    u32_t ms = (u32_t)((br_uptime_us() - start) / 1000U);
    return ms == SYS_ARCH_TIMEOUT ? SYS_ARCH_TIMEOUT - 1 : ms;
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    if (br_sem_init(&sem->sem, count, 0x7FFFFFFF) != BR_OK) {
        return ERR_MEM;
    }
    sem->valid = true;
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
    br_sem_give(&sem->sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    br_time_t start = br_uptime_us();

    if (br_sem_take(&sem->sem, to_timeout(timeout)) != BR_OK) {
        return SYS_ARCH_TIMEOUT;
    }
    return elapsed_ms(start);
}

void sys_sem_free(sys_sem_t *sem)
{
    sem->valid = false;
}

int sys_sem_valid(sys_sem_t *sem)
{
    return sem->valid;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    sem->valid = false;
}

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    if (br_mutex_init(&mutex->mutex) != BR_OK) {
        return ERR_MEM;
    }
    mutex->valid = true;
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    br_mutex_lock(&mutex->mutex, BR_TIME_INFINITE);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    br_mutex_unlock(&mutex->mutex);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    mutex->valid = false;
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
    return mutex->valid;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    mutex->valid = false;
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (size <= 0 || size > BR_LWIP_MBOX_MAX) {
        return ERR_MEM;
    }
    if (br_mqueue_init(&mbox->mq, mbox->slots, sizeof(void *), (size_t)size) != BR_OK) {
        return ERR_MEM;
    }
    mbox->valid = true;
    return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    br_mqueue_send(&mbox->mq, &msg, BR_TIME_INFINITE);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    return br_mqueue_send(&mbox->mq, &msg, 0) == BR_OK ? ERR_OK : ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    void *dummy;
    br_time_t start = br_uptime_us();

    if (msg == NULL) {
        msg = &dummy;
    }
    if (br_mqueue_recv(&mbox->mq, msg, to_timeout(timeout)) != BR_OK) {
        return SYS_ARCH_TIMEOUT;
    }
    return elapsed_ms(start);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *dummy;

    if (msg == NULL) {
        msg = &dummy;
    }
    return br_mqueue_recv(&mbox->mq, msg, 0) == BR_OK ? 0 : SYS_MBOX_EMPTY;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    mbox->valid = false;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return mbox->valid;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    mbox->valid = false;
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
                            int stacksize, int prio)
{
    (void)stacksize;

    uint32_t key = br_hal_irq_disable();
    if (threads_used >= BR_LWIP_MAX_THREADS) {
        br_hal_irq_restore(key);
        return 0;
    }
    uint8_t slot = threads_used++;
    br_hal_irq_restore(key);

    br_tid_t tid;
    if (br_task_create(&tid, name, thread, arg, (uint8_t)prio,
                       thread_stacks[slot], BR_LWIP_STACK_SIZE) != BR_OK) {
        return 0;
    }
    return tid;
}

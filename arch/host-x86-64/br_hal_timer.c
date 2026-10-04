/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern void br_time_alarm_handler(void);
extern void br_sched_tick(br_time_t elapsed_us);

static volatile bool      g_in_isr;
static volatile br_time_t g_alarm_target;
static volatile bool      g_alarm_pending;
static volatile br_time_t g_last_tick_us;
static struct timespec    g_start_time;
static sigset_t           g_alarm_sigset;


static void sigalrm_handler(int sig)
{
    (void)sig;
    g_in_isr = true;

    br_time_t now = br_hal_timer_get_us();

    if (g_alarm_pending && now >= g_alarm_target) {
        g_alarm_pending = false;
        g_in_isr = false;
        br_time_alarm_handler();
        now = br_hal_timer_get_us();
        g_in_isr = true;
    }

    br_time_t elapsed = now - g_last_tick_us;
    g_last_tick_us = now;
    g_in_isr = false;
    br_sched_tick(elapsed);
}

void br_hal_timer_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);
    g_alarm_pending = false;
    g_alarm_target  = 0;
    g_last_tick_us  = 0;
    g_in_isr        = false;

    sigemptyset(&g_alarm_sigset);
    sigaddset(&g_alarm_sigset, SIGALRM);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigalrm_handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    struct itimerval itv;
    itv.it_interval.tv_sec  = 0;
    itv.it_interval.tv_usec = CONFIG_RR_TIME_SLICE_US;
    itv.it_value.tv_sec     = 0;
    itv.it_value.tv_usec    = CONFIG_RR_TIME_SLICE_US;
    setitimer(ITIMER_REAL, &itv, NULL);
}

br_time_t br_hal_timer_get_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t sec_diff  = (int64_t)now.tv_sec  - (int64_t)g_start_time.tv_sec;
    int64_t nsec_diff = (int64_t)now.tv_nsec - (int64_t)g_start_time.tv_nsec;
    return (br_time_t)((sec_diff * 1000000000LL + nsec_diff) / 1000LL);
}

void br_hal_timer_set_alarm(br_time_t abs_us)
{
    g_alarm_target  = abs_us;
    g_alarm_pending = true;
}

void br_hal_timer_cancel_alarm(void)
{
    g_alarm_pending = false;
}

uint32_t br_hal_irq_disable(void)
{
    sigset_t old;
    sigprocmask(SIG_BLOCK, &g_alarm_sigset, &old);
    return sigismember(&old, SIGALRM) ? 1u : 0u;
}

void br_hal_irq_restore(uint32_t state)
{
    if (state == 0) {
        sigprocmask(SIG_UNBLOCK, &g_alarm_sigset, NULL);
    }
}

bool br_hal_in_isr(void)
{
    return g_in_isr;
}

#ifdef BR_HOST_TEST
void br_hal_test_force_isr(bool v)
{
    g_in_isr = v;
}
#endif

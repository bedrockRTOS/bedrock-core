/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_TYPES_H
#define BR_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "br_config.h"

/* Core time type -- 64-bit microseconds (~584,000 years range) */
typedef uint64_t br_time_t;

#define BR_TIME_INFINITE  UINT64_MAX
#define BR_USEC(us)       ((br_time_t)(us))
#define BR_MSEC(ms)       ((br_time_t)(ms) * 1000U)
#define BR_SEC(s)         ((br_time_t)(s)  * 1000000U)

/* Stack overflow detection canary value */
#define BR_STACK_CANARY   0xDEADBEEF

/* Byte a new stack is filled with, used to find the high-water mark */
#define BR_STACK_FILL     0xA5

/* Smallest stack br_task_create() accepts: the initial frame, the canary
 * and a little room to run */
#define BR_STACK_MIN      256

/* Error codes */
typedef enum {
    BR_OK            =  0,
    BR_ERR_INVALID   = -1,
    BR_ERR_NOMEM     = -2,
    BR_ERR_TIMEOUT   = -3,
    BR_ERR_BUSY      = -4,
    BR_ERR_ISR       = -5,
    BR_ERR_OVERFLOW  = -6,
    BR_ERR_STACK_OVF = -7,
    BR_ERR_NOENT     = -8,
    BR_ERR_IO        = -9,
    BR_ERR_PERM      = -10
} br_err_t;

/* Task states */
typedef enum {
    BR_TASK_INACTIVE  = 0,
    BR_TASK_READY     = 1,
    BR_TASK_RUNNING   = 2,
    BR_TASK_BLOCKED   = 3,
    BR_TASK_SUSPENDED = 4
} br_task_state_t;

/* Task ID & entry point */
typedef uint8_t  br_tid_t;
typedef void (*br_task_entry_t)(void *arg);

/* Memory access rights of a region */
#define BR_MEM_R   0x01
#define BR_MEM_W   0x02
#define BR_MEM_X   0x04

#define BR_TASK_REGIONS  4

/* The lowest eighth of a user stack is a guard no one may touch, the
 * kernel saves context into it when the stack has overflowed. */
#define BR_USER_STACK_MIN       512
#define BR_USER_STACK_GUARD(n)  ((n) / 8)

typedef struct {
    uintptr_t base;
    size_t    size;
    uint8_t   perms;
} br_region_t;

/* Task Control Block (TCB) */
typedef struct br_tcb {
    /* Saved stack pointer -- must be first for context switch asm */
    void               *sp;

    br_tid_t            id;
    br_task_state_t     state;
    uint8_t             priority;      /* 0 = highest */
    const char         *name;

    /* Stack region */
    void               *stack_base;
    size_t              stack_size;
    uint32_t           *stack_canary;   /* Pointer to canary at stack bottom */

    /* Entry point */
    br_task_entry_t     entry;
    void               *arg;

    /* Scheduler bookkeeping */
    br_time_t           wake_time;     /* Used by sleep / timed waits */
    uint16_t            rr_remaining;  /* Round-robin time-slice ticks */
    br_err_t            wait_result;   /* Result after waking from block */

    /* Simple linked-list pointer for ready queues / IPC wait queues.
     * A task is in at most one of these at a time, so they can share
     * this field. */
    struct br_tcb      *next;

    /* Separate link for the global sleep list. A timed wait on a
     * semaphore/mutex/queue puts a task in *both* its wait queue (via
     * `next` above) and the sleep list at once -- they need independent
     * storage or inserting into one corrupts the other. */
    struct br_tcb      *sleep_next;

#if CONFIG_TASK_STATS
    br_time_t           run_time_us;   /* Accumulated running time */
#endif

#if CONFIG_MPU
    bool                user;          /* Runs unprivileged */
    bool                in_syscall;    /* Runs privileged on its kernel stack */
    bool                faulted;
    uint8_t             nregions;      /* regions[0] is the stack */
    uint8_t             fault_kind;
    br_region_t         regions[BR_TASK_REGIONS];
    void               *kstack_top;
    void               *user_frame;    /* HAL: saved user context in a syscall */
    uint32_t            ctx_save[32];  /* HAL: where the user context waits */
    uint32_t            sysargs[5];
    uintptr_t           fault_addr;
#endif
} br_tcb_t;

/* Snapshot of a task returned by br_task_get_info() */
typedef struct {
    const char         *name;
    br_task_state_t     state;
    uint8_t             priority;
    size_t              stack_size;    /* Total stack size in bytes */
    size_t              stack_used;    /* High-water mark in bytes */
    br_time_t           cpu_time_us;   /* 0 unless CONFIG_TASK_STATS */
    bool                faulted;       /* stopped by a memory fault */
    bool                user;
} br_task_info_t;

/* Semaphore */
typedef struct {
    volatile int32_t  count;
    int32_t           max_count;
    br_tcb_t         *wait_queue;     /* Head of blocked tasks */
} br_sem_t;

/* Mutex (with priority inheritance support) */
typedef struct {
    volatile bool      locked;
    br_tcb_t          *owner;
    uint8_t            owner_orig_prio;  /* For priority inheritance */
    br_tcb_t          *wait_queue;
} br_mutex_t;

/* Message Queue (fixed-size ring buffer, statically allocated) */
typedef struct {
    uint8_t           *buffer;        /* Pointer to caller-provided storage */
    size_t             msg_size;      /* Size of a single message in bytes */
    size_t             max_msgs;      /* Capacity */
    volatile size_t    count;         /* Current number of messages */
    size_t             head;
    size_t             tail;
    br_tcb_t          *send_wait;     /* Tasks blocked on full queue */
    br_tcb_t          *recv_wait;     /* Tasks blocked on empty queue */
} br_mqueue_t;

#endif /* BR_TYPES_H */

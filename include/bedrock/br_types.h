/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
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

/* Error codes */
typedef enum {
    BR_OK            =  0,
    BR_ERR_INVALID   = -1,
    BR_ERR_NOMEM     = -2,
    BR_ERR_TIMEOUT   = -3,
    BR_ERR_BUSY      = -4,
    BR_ERR_ISR       = -5,
    BR_ERR_OVERFLOW  = -6
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

    /* Entry point */
    br_task_entry_t     entry;
    void               *arg;

    /* Scheduler bookkeeping */
    br_time_t           wake_time;     /* Used by sleep / timed waits */
    uint16_t            rr_remaining;  /* Round-robin time-slice ticks */
    br_err_t            wait_result;   /* Result after waking from block */

    /* Simple linked-list pointers for ready / wait queues */
    struct br_tcb      *next;
} br_tcb_t;

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

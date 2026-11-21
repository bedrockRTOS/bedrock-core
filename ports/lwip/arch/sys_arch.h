/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "bedrock/bedrock.h"

#define BR_LWIP_MBOX_MAX    8

typedef struct {
    br_sem_t sem;
    bool     valid;
} sys_sem_t;

typedef struct {
    br_mutex_t mutex;
    bool       valid;
} sys_mutex_t;

typedef struct {
    br_mqueue_t mq;
    void       *slots[BR_LWIP_MBOX_MAX];
    bool        valid;
} sys_mbox_t;

typedef br_tid_t  sys_thread_t;
typedef uint32_t  sys_prot_t;

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "bedrock/br_syscall.h"

#if CONFIG_MPU

#define MPU_TYPE    (*(volatile uint32_t *)0xE000ED90UL)
#define MPU_CTRL    (*(volatile uint32_t *)0xE000ED94UL)
#define MPU_RNR     (*(volatile uint32_t *)0xE000ED98UL)
#define MPU_RBAR    (*(volatile uint32_t *)0xE000ED9CUL)
#define MPU_RASR    (*(volatile uint32_t *)0xE000EDA0UL)
#ifndef __ARM_ARCH_6M__
#define SCB_SHCSR   (*(volatile uint32_t *)0xE000ED24UL)
#endif

#define MPU_CTRL_ENABLE     (1UL << 0)
#define MPU_CTRL_PRIVDEFENA (1UL << 2)

#define RBAR_VALID          (1UL << 4)
#define RASR_ENABLE         (1UL << 0)
#define RASR_SIZE(n)        ((uint32_t)(n) << 1)
#define RASR_AP_FULL        (3UL << 24)
#define RASR_AP_RO          (6UL << 24)
#define RASR_XN             (1UL << 28)
#define RASR_S              (1UL << 18)
#define RASR_C              (1UL << 17)
#define RASR_B              (1UL << 16)

#define RASR_SRD_GUARD      (0x01UL << 8)

#define ATTR_MEMORY         (RASR_S | RASR_C)
#define ATTR_DEVICE         (RASR_S | RASR_B)

#ifndef __ARM_ARCH_6M__
#define SHCSR_FAULT_ENABLE  ((1UL << 16) | (1UL << 17) | (1UL << 18))
#endif

#define REGION_CODE         0
#define REGION_USER_DATA    1
#define REGION_STACK        2
#define REGION_GRANT0       3
#define REGION_COUNT        8

#define SVC_SYSCALL         2
#define SVC_RETURN          3
#define EXC_RETURN_PSP      0xFFFFFFFDUL
#define XPSR_THUMB          0x01000000UL

extern char _mpu_code_base[];
extern char _mpu_code_size[];
extern char _user_data_start[];
extern char _user_data_end[];

extern br_tcb_t *br_sched_current(void);
extern uint32_t  br_syscall_dispatch(const uint32_t *args);
extern void      br_task_fault(br_tcb_t *tcb, uint8_t kind, uintptr_t addr);

static uint32_t log2_size(size_t size)
{
    uint32_t n = 0;
    while (((size_t)1 << n) < size) {
        n++;
    }
    return n;
}

static void region_write(uint32_t number, uintptr_t base, size_t size, uint32_t attrs)
{
    MPU_RBAR = (uint32_t)base | RBAR_VALID | number;
    MPU_RASR = RASR_ENABLE | RASR_SIZE(log2_size(size) - 1) | attrs;
}

static void region_disable(uint32_t number)
{
    MPU_RBAR = RBAR_VALID | number;
    MPU_RASR = 0;
}

bool br_hal_mpu_region_valid(const br_region_t *r)
{
    return r->size >= 32 && (r->size & (r->size - 1)) == 0 &&
           (r->base & (r->size - 1)) == 0 && r->base + r->size > r->base;
}

void br_hal_mpu_code_region(br_region_t *r)
{
    r->base  = (uintptr_t)_mpu_code_base;
    r->size  = (size_t)(uintptr_t)_mpu_code_size;
    r->perms = BR_MEM_R | BR_MEM_X;
}

void br_hal_mpu_user_data_region(br_region_t *r)
{
    r->base  = (uintptr_t)_user_data_start;
    r->size  = (size_t)((uintptr_t)_user_data_end - (uintptr_t)_user_data_start);
    r->perms = BR_MEM_R | BR_MEM_W;
}

void br_hal_mpu_init(void)
{
    br_region_t code;
    br_region_t data;

    br_hal_mpu_code_region(&code);
    br_hal_mpu_user_data_region(&data);

    MPU_CTRL = 0;

    for (uint32_t i = 0; i < REGION_COUNT; i++) {
        region_disable(i);
    }
    region_write(REGION_CODE, code.base, code.size, ATTR_MEMORY | RASR_AP_RO);
    region_write(REGION_USER_DATA, data.base, data.size, ATTR_MEMORY | RASR_AP_FULL | RASR_XN);

#ifndef __ARM_ARCH_6M__
    SCB_SHCSR |= SHCSR_FAULT_ENABLE;
#endif

    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

void *br_hal_stack_init_user(void *kstack_top, void *stack_top, br_task_entry_t entry, void *arg)
{
    (void)kstack_top;
    return br_hal_stack_init(stack_top, entry, arg);
}

uint32_t br_hal_mpu_switch(br_tcb_t *t);

uint32_t br_hal_mpu_switch(br_tcb_t *t)
{
    if (!t->user) {
        return 0;
    }

    region_write(REGION_STACK, t->regions[0].base, t->regions[0].size,
                 ATTR_MEMORY | RASR_AP_FULL | RASR_XN | RASR_SRD_GUARD);

    for (uint32_t i = 1; i < BR_TASK_REGIONS; i++) {
        if (i < t->nregions) {
            region_write(REGION_GRANT0 + i - 1, t->regions[i].base, t->regions[i].size,
                         ATTR_DEVICE | RASR_AP_FULL | RASR_XN);
        } else {
            region_disable(REGION_GRANT0 + i - 1);
        }
    }

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
    return t->in_syscall ? 0 : 1;
}

static void syscall_thread(const uint32_t *args) __attribute__((noreturn, used));

static void syscall_thread(const uint32_t *args)
{
    uint32_t r = br_syscall_dispatch(args);

    __asm volatile (
        "mov r0, %0 \n"
        "svc %1     \n"
        :: "r" (r), "i" (SVC_RETURN) : "r0"
    );
    __builtin_unreachable();
}

__attribute__((naked))
uint32_t br_hal_syscall(uint32_t num, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)num; (void)a0; (void)a1; (void)a2; (void)a3;
#ifdef __ARM_ARCH_6M__
    __asm volatile (
        ".syntax unified \n"
        "push {r4}       \n"
        "ldr  r4, [sp, #4] \n"
        "mov  r12, r4    \n"
        "pop  {r4}       \n"
        "svc  %0         \n"
        "bx   lr         \n"
        :: "i" (SVC_SYSCALL)
    );
#else
    __asm volatile (
        "ldr r12, [sp]  \n"
        "svc %0         \n"
        "bx  lr         \n"
        :: "i" (SVC_SYSCALL)
    );
#endif
}

static void set_control(uint32_t value)
{
    __asm volatile ("msr control, %0" :: "r" (value) : "memory");
    __asm volatile ("isb" ::: "memory");
}

uint32_t br_hal_svc(uint32_t *frame);

uint32_t br_hal_svc(uint32_t *frame)
{
    uint8_t   imm = ((const uint8_t *)frame[6])[-2];
    br_tcb_t *t   = br_sched_current();

    if (imm == SVC_SYSCALL && t->user && !t->in_syscall) {
        uint32_t *args = (uint32_t *)t->kstack_top - 8;
        uint32_t *kf   = args - 8;

        args[0] = frame[0];
        args[1] = frame[1];
        args[2] = frame[2];
        args[3] = frame[3];
        args[4] = frame[4];

        kf[0] = (uint32_t)(uintptr_t)args;
        kf[1] = 0;
        kf[2] = 0;
        kf[3] = 0;
        kf[4] = 0;
        kf[5] = 0;
        kf[6] = (uint32_t)(uintptr_t)syscall_thread;
        kf[7] = XPSR_THUMB;

        t->user_frame = frame;
        t->in_syscall = true;

        __asm volatile ("msr psp, %0" :: "r" (kf));
        set_control(0);
        return EXC_RETURN_PSP;
    }

    if (imm == SVC_RETURN && t->in_syscall) {
        uint32_t *user = t->user_frame;

        user[0]       = frame[0];
        t->in_syscall = false;
        t->user_frame = NULL;

        __asm volatile ("msr psp, %0" :: "r" (user));
        set_control(1);
        return EXC_RETURN_PSP;
    }

    if (imm == SVC_SYSCALL && !t->user) {
        frame[0] = (uint32_t)BR_ERR_INVALID;
        return EXC_RETURN_PSP;
    }

    br_task_fault(t, 3, frame[6]);
    return EXC_RETURN_PSP;
}

#endif

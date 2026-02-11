/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ARM Cortex-M context switch HAL implementation.
 * Uses PendSV for context switching (standard Cortex-M pattern).
 *
 * This is a minimal reference implementation for QEMU cortex-m3.
 */

#include "bedrock/br_hal.h"

#define SCB_ICSR    (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET  (1UL << 28)

/* Global pointers used by PendSV_Handler to perform the actual switch */
volatile void **br_hal_old_sp_ptr;
volatile void **br_hal_new_sp_ptr;

/*
 * Stack frame layout for Cortex-M (initial frame)
 *
 * When a task is first scheduled, the hardware and software
 * expect to see:
 *
 * [stack_top - descending]
 *  xPSR  (thumb bit set)
 *  PC    (entry point)
 *  LR    (return address -- task_exit_handler)
 *  R12
 *  R3, R2, R1, R0  (R0 = arg)
 *  --- hardware frame above ---
 *  R4..R11          (software-saved, zeroed initially)
 */

static void task_exit_handler(void)
{
    while (1) { }
}

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg)
{
    uint32_t *sp = (uint32_t *)((uintptr_t)stack_top & ~0x7UL);

    *(--sp) = 0x01000000UL;
    *(--sp) = (uint32_t)(uintptr_t)entry;
    *(--sp) = (uint32_t)(uintptr_t)task_exit_handler;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = (uint32_t)(uintptr_t)arg;

    for (int i = 0; i < 8; i++) {
        *(--sp) = 0;
    }

    return (void *)sp;
}

void br_hal_context_switch(void **old_sp, void **new_sp)
{
    br_hal_old_sp_ptr = (volatile void **)old_sp;
    br_hal_new_sp_ptr = (volatile void **)new_sp;
    SCB_ICSR = ICSR_PENDSVSET;
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

void br_hal_start_first_task(void *sp)
{
    __asm volatile (
        "msr psp, %0        \n"
        "mov r0, #2          \n"
        "msr control, r0     \n"
        "isb                 \n"
        /* Restore software-saved R4-R11 from initial stack frame */
        "mrs r0, psp         \n"
        "ldmia r0!, {r4-r11} \n"
        "msr psp, r0         \n"
        /* Hardware will pop R0-R3,R12,LR,PC,xPSR on exception return */
        "ldr lr, =0xFFFFFFFD \n"  /* EXC_RETURN: thread mode, PSP */
        "bx lr               \n"
        :
        : "r" (sp)
        : "memory"
    );

    while (1) { }
}

__attribute__((weak))
void br_hal_board_init(void)
{
}

/*
 * PendSV handler (context switch)
 *
 * Saves R4-R11 + PSP into the old task's TCB->sp,
 * restores R4-R11 + PSP from the new task's TCB->sp.
 *
 * br_hal_old_sp_ptr -> &old_tcb->sp  (where to store old SP)
 * br_hal_new_sp_ptr -> &new_tcb->sp  (where to load new SP)
 */

__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        /* Save current context */
        "mrs   r0, psp              \n"
        "stmdb r0!, {r4-r11}        \n"

        /* Store old SP: *br_hal_old_sp_ptr = r0 */
        "ldr   r1, =br_hal_old_sp_ptr \n"
        "ldr   r1, [r1]             \n"
        "str   r0, [r1]             \n"

        /* Load new SP: r0 = *br_hal_new_sp_ptr */
        "ldr   r1, =br_hal_new_sp_ptr \n"
        "ldr   r1, [r1]             \n"
        "ldr   r0, [r1]             \n"

        /* Restore new context */
        "ldmia r0!, {r4-r11}        \n"
        "msr   psp, r0              \n"

        /* Return to thread mode using PSP */
        "ldr   lr, =0xFFFFFFFD      \n"
        "bx    lr                    \n"
    );
}

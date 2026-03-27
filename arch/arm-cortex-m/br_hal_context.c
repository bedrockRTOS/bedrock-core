/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE-GPL-3.0.md for the full terms and runtime exception.
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

/* Pointer to first task's SP for SVC_Handler */
static void *br_hal_first_task_sp __attribute__((used));

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
    /* We need to trigger an exception return to properly enter thread mode with PSP.
     * The cleanest way is to use SVC (supervisor call) exception. */
    __asm volatile (
        /* Save the initial SP pointer */
        "ldr r0, =br_hal_first_task_sp \n"
        "str %0, [r0]                   \n"
        /* Trigger SVC exception which will do the actual switch */
        "svc 0                          \n"
        :
        : "r" (sp)
        : "r0", "memory"
    );

    while (1) { }
}

__attribute__((weak))
void br_hal_board_init(void)
{
}

extern void br_uart_puts(const char *s);
extern void br_uart_putc(char c);

/* Helper to print decimal numbers without printf */
static void br_uart_putu(uint32_t val)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (val == 0) {
        br_uart_putc('0');
        return;
    }
    while (val > 0 && i > 0) {
        buf[--i] = (char)('0' + (val % 10));
        val /= 10;
    }
    br_uart_puts(&buf[i]);
}

/*
 * Stack overflow detection
 *
 * Checks if the canary value at the bottom of the stack has been corrupted.
 * If corrupted, triggers a kernel panic.
 */

__attribute__((noreturn))
void br_hal_panic(const char *msg, const char *file, int line)
{
    /* Disable all interrupts */
    __asm volatile ("cpsid i" ::: "memory");

    br_uart_puts("\n*** KERNEL PANIC ***\n");
    if (msg) {
        br_uart_puts("Reason: ");
        br_uart_puts(msg);
        br_uart_puts("\n");
    }
    if (file) {
        br_uart_puts("File:   ");
        br_uart_puts(file);
        br_uart_puts(":");
        br_uart_putu((uint32_t)line);
        br_uart_puts("\n");
    }
    br_uart_puts("********************\n");

    /* Halt the system */
    while (1) {
        __asm volatile ("wfi");
    }
}

void br_hal_check_stack_overflow(br_tcb_t *tcb)
{
    if (tcb == NULL || tcb->stack_canary == NULL) {
        return;
    }

    if (*(tcb->stack_canary) != BR_STACK_CANARY) {
        br_hal_panic("Stack overflow detected", __FILE__, __LINE__);
    }
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

/*
 * SVC handler (start first task)
 *
 * This is called by br_hal_start_first_task via "svc 0" instruction.
 * It switches from handler mode (MSP) to thread mode (PSP) and starts
 * the first task.
 */

__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        /* Load the first task's SP */
        "ldr r0, =br_hal_first_task_sp \n"
        "ldr r0, [r0]                  \n"
        
        /* Restore R4-R11 from the stack frame */
        "ldmia r0!, {r4-r11}           \n"
        
        /* Set PSP to the adjusted stack pointer */
        "msr psp, r0                   \n"
        
        /* Switch to unprivileged thread mode using PSP */
        "mov r0, #2                    \n"
        "msr control, r0               \n"
        "isb                           \n"
        
        /* Return to thread mode with PSP - hardware will pop remaining registers */
        "ldr lr, =0xFFFFFFFD           \n"
        "bx lr                         \n"
    );
}

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define SCB_ICSR        (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET  (1UL << 28)

volatile void **br_hal_old_sp_ptr;
volatile void **br_hal_new_sp_ptr;

static void *br_hal_first_task_sp __attribute__((used));

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
        "ldr r0, =br_hal_first_task_sp \n"
        "str %0, [r0]                   \n"
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

__attribute__((noreturn))
void br_hal_panic(const char *msg, const char *file, int line)
{
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

__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        ".syntax unified              \n"
        "mrs   r0, psp                \n"
        "subs  r0, #32                \n"
        "stmia r0!, {r4-r7}           \n"
        "mov   r4, r8                 \n"
        "mov   r5, r9                 \n"
        "mov   r6, r10                \n"
        "mov   r7, r11                \n"
        "stmia r0!, {r4-r7}           \n"
        "subs  r0, #32                \n"

        "ldr   r1, =br_hal_old_sp_ptr \n"
        "ldr   r1, [r1]               \n"
        "str   r0, [r1]               \n"

        "ldr   r1, =br_hal_new_sp_ptr \n"
        "ldr   r1, [r1]               \n"
        "ldr   r0, [r1]               \n"

        "adds  r0, #16                \n"
        "ldmia r0!, {r4-r7}           \n"
        "mov   r8, r4                 \n"
        "mov   r9, r5                 \n"
        "mov   r10, r6                \n"
        "mov   r11, r7                \n"
        "subs  r0, #32                \n"
        "ldmia r0!, {r4-r7}           \n"
        "adds  r0, #16                \n"
        "msr   psp, r0                \n"

        "ldr   r0, =0xFFFFFFFD        \n"
        "bx    r0                     \n"
    );
}

__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        ".syntax unified              \n"
        "ldr   r0, =br_hal_first_task_sp \n"
        "ldr   r0, [r0]               \n"

        "adds  r0, #16                \n"
        "ldmia r0!, {r4-r7}           \n"
        "mov   r8, r4                 \n"
        "mov   r9, r5                 \n"
        "mov   r10, r6                \n"
        "mov   r11, r7                \n"
        "subs  r0, #32                \n"
        "ldmia r0!, {r4-r7}           \n"
        "adds  r0, #16                \n"
        "msr   psp, r0                \n"

        "ldr   r0, =0xFFFFFFFD        \n"
        "bx    r0                     \n"
    );
}

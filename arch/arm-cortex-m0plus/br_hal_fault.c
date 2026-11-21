/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

#define EXC_RETURN_THREAD  (1UL << 3)
#define FAULT_HARDFAULT    4

extern br_tcb_t *br_sched_current(void);
extern void      br_uart_puts(const char *s);
extern void      br_uart_putc(char c);

#if CONFIG_MPU
extern void br_task_fault(br_tcb_t *tcb, uint8_t kind, uintptr_t addr);
#endif

static void print_hex(uint32_t v)
{
    br_uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        br_uart_putc("0123456789abcdef"[(v >> shift) & 0xF]);
    }
}

static void print_reg(const char *name, uint32_t v)
{
    br_uart_puts(name);
    print_hex(v);
    br_uart_puts("\n");
}

void br_hal_fault(uint32_t *frame, uint32_t exc_return);

void br_hal_fault(uint32_t *frame, uint32_t exc_return)
{
    br_tcb_t *t = br_sched_current();

#if CONFIG_MPU
    if ((exc_return & EXC_RETURN_THREAD) && t != NULL && t->user && !t->in_syscall) {
        br_uart_puts("\n*** TASK FAULT ***\n");
        br_uart_puts("Task:   ");
        br_uart_puts(t->name != NULL ? t->name : "?");
        br_uart_puts("\nKind:   HardFault\n");
        print_reg("PC:      ", frame[6]);
        br_uart_puts("******************\n");

        br_task_fault(t, FAULT_HARDFAULT, frame[6]);
        return;
    }
#else
    (void)exc_return;
#endif

    br_uart_puts("\n*** CPU FAULT ***\n");
    br_uart_puts("Kind:   HardFault\n");
    if (t != NULL && t->name != NULL) {
        br_uart_puts("Task:   ");
        br_uart_puts(t->name);
        br_uart_puts("\n");
    }
    print_reg("R0:      ", frame[0]);
    print_reg("R1:      ", frame[1]);
    print_reg("R2:      ", frame[2]);
    print_reg("R3:      ", frame[3]);
    print_reg("R12:     ", frame[4]);
    print_reg("LR:      ", frame[5]);
    print_reg("PC:      ", frame[6]);
    print_reg("xPSR:    ", frame[7]);
    br_uart_puts("*****************\n");

    br_hal_panic("CPU fault", __FILE__, __LINE__);
}

__attribute__((naked))
void HardFault_Handler(void)
{
    __asm volatile (
        ".syntax unified     \n"
        "movs r0, #4         \n"
        "mov  r1, lr         \n"
        "tst  r0, r1         \n"
        "beq  1f             \n"
        "mrs  r0, psp        \n"
        "b    2f             \n"
        "1:                  \n"
        "mrs  r0, msp        \n"
        "2:                  \n"
        "mov  r1, lr         \n"
        "ldr  r2, =br_hal_fault \n"
        "bx   r2             \n"
    );
}

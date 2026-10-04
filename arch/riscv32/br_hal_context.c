/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"

#define CLINT_MSIP       (*(volatile uint32_t *)0x02000000UL)

#define MSTATUS_MIE      (1UL << 3)
#define MSTATUS_MPIE     (1UL << 7)
#define MSTATUS_MPP_M    (3UL << 11)

#define MCAUSE_IRQ       (1UL << 31)
#define IRQ_M_SOFT       3
#define IRQ_M_TIMER      7
#define IRQ_M_EXT        11

#define FRAME_WORDS      32
#define FRAME_RA         0
#define FRAME_A0         6
#define FRAME_MEPC       28
#define FRAME_MSTATUS    29

extern void br_hal_timer_isr(void);
extern void br_hal_external_isr(void);
extern void br_uart_puts(const char *s);
extern void br_uart_putc(char c);

static void **volatile switch_old_sp;
static void **volatile switch_new_sp;
static volatile bool switch_pending;
static volatile bool in_isr;

__asm__(
    ".section .text\n"
    ".globl br_hal_trap_entry\n"
    ".globl br_hal_trap_return\n"
    ".align 2\n"
    "br_hal_trap_entry:\n"
    "    addi sp, sp, -128\n"
    "    sw ra,    0(sp)\n"
    "    sw t0,    4(sp)\n"
    "    sw t1,    8(sp)\n"
    "    sw t2,   12(sp)\n"
    "    sw s0,   16(sp)\n"
    "    sw s1,   20(sp)\n"
    "    sw a0,   24(sp)\n"
    "    sw a1,   28(sp)\n"
    "    sw a2,   32(sp)\n"
    "    sw a3,   36(sp)\n"
    "    sw a4,   40(sp)\n"
    "    sw a5,   44(sp)\n"
    "    sw a6,   48(sp)\n"
    "    sw a7,   52(sp)\n"
    "    sw s2,   56(sp)\n"
    "    sw s3,   60(sp)\n"
    "    sw s4,   64(sp)\n"
    "    sw s5,   68(sp)\n"
    "    sw s6,   72(sp)\n"
    "    sw s7,   76(sp)\n"
    "    sw s8,   80(sp)\n"
    "    sw s9,   84(sp)\n"
    "    sw s10,  88(sp)\n"
    "    sw s11,  92(sp)\n"
    "    sw t3,   96(sp)\n"
    "    sw t4,  100(sp)\n"
    "    sw t5,  104(sp)\n"
    "    sw t6,  108(sp)\n"
    "    csrr t0, mepc\n"
    "    sw t0,  112(sp)\n"
    "    csrr t0, mstatus\n"
    "    sw t0,  116(sp)\n"
    "    mv a0, sp\n"
    "    csrr a1, mcause\n"
    "    call br_hal_trap\n"
    "    mv sp, a0\n"
    "br_hal_trap_return:\n"
    "    lw t0,  112(sp)\n"
    "    csrw mepc, t0\n"
    "    lw t0,  116(sp)\n"
    "    csrw mstatus, t0\n"
    "    lw ra,    0(sp)\n"
    "    lw t0,    4(sp)\n"
    "    lw t1,    8(sp)\n"
    "    lw t2,   12(sp)\n"
    "    lw s0,   16(sp)\n"
    "    lw s1,   20(sp)\n"
    "    lw a0,   24(sp)\n"
    "    lw a1,   28(sp)\n"
    "    lw a2,   32(sp)\n"
    "    lw a3,   36(sp)\n"
    "    lw a4,   40(sp)\n"
    "    lw a5,   44(sp)\n"
    "    lw a6,   48(sp)\n"
    "    lw a7,   52(sp)\n"
    "    lw s2,   56(sp)\n"
    "    lw s3,   60(sp)\n"
    "    lw s4,   64(sp)\n"
    "    lw s5,   68(sp)\n"
    "    lw s6,   72(sp)\n"
    "    lw s7,   76(sp)\n"
    "    lw s8,   80(sp)\n"
    "    lw s9,   84(sp)\n"
    "    lw s10,  88(sp)\n"
    "    lw s11,  92(sp)\n"
    "    lw t3,   96(sp)\n"
    "    lw t4,  100(sp)\n"
    "    lw t5,  104(sp)\n"
    "    lw t6,  108(sp)\n"
    "    addi sp, sp, 128\n"
    "    mret\n"
);

void *br_hal_trap(void *sp, uint32_t mcause);

void *br_hal_trap(void *sp, uint32_t mcause)
{
    in_isr = true;

    if (mcause & MCAUSE_IRQ) {
        uint32_t code = mcause & ~MCAUSE_IRQ;
        if (code == IRQ_M_TIMER) {
            br_hal_timer_isr();
        } else if (code == IRQ_M_SOFT) {
            CLINT_MSIP = 0;
        } else if (code == IRQ_M_EXT) {
            br_hal_external_isr();
        }
    } else {
        br_hal_panic("Unhandled exception", __FILE__, __LINE__);
    }

    in_isr = false;

    if (switch_pending) {
        switch_pending = false;
        CLINT_MSIP = 0;
        *switch_old_sp = sp;
        sp = *switch_new_sp;
    }

    return sp;
}

static void task_exit_handler(void)
{
    while (1) { }
}

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg)
{
    uint32_t *sp = (uint32_t *)((uintptr_t)stack_top & ~0xFUL);

    sp -= FRAME_WORDS;
    for (int i = 0; i < FRAME_WORDS; i++) {
        sp[i] = 0;
    }

    sp[FRAME_RA]      = (uint32_t)(uintptr_t)task_exit_handler;
    sp[FRAME_A0]      = (uint32_t)(uintptr_t)arg;
    sp[FRAME_MEPC]    = (uint32_t)(uintptr_t)entry;
    sp[FRAME_MSTATUS] = MSTATUS_MPP_M | MSTATUS_MPIE;

    return (void *)sp;
}

void br_hal_context_switch(void **old_sp, void **new_sp)
{
    if (!switch_pending) {
        switch_old_sp = old_sp;
    }
    switch_new_sp = new_sp;
    switch_pending = true;

    if (!in_isr) {
        CLINT_MSIP = 1;
    }
}

__attribute__((naked))
void br_hal_start_first_task(void *sp)
{
    (void)sp;
    __asm volatile (
        "li t0, 8\n"
        "csrs mie, t0\n"
        "mv sp, a0\n"
        "j br_hal_trap_return\n"
    );
}

uint32_t br_hal_irq_disable(void)
{
    uint32_t mstatus;
    __asm volatile ("csrrci %0, mstatus, 8" : "=r" (mstatus) :: "memory");
    return mstatus & MSTATUS_MIE;
}

void br_hal_irq_restore(uint32_t state)
{
    if (state & MSTATUS_MIE) {
        __asm volatile ("csrsi mstatus, 8" ::: "memory");
    }
}

bool br_hal_in_isr(void)
{
    return in_isr;
}

__attribute__((weak))
void br_hal_board_init(void)
{
}

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
    __asm volatile ("csrci mstatus, 8" ::: "memory");

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

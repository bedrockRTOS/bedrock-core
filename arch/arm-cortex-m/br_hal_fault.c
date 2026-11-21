/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"

#define SCB_CFSR    (*(volatile uint32_t *)0xE000ED28UL)
#define SCB_HFSR    (*(volatile uint32_t *)0xE000ED2CUL)
#define SCB_MMFAR   (*(volatile uint32_t *)0xE000ED34UL)
#define SCB_BFAR    (*(volatile uint32_t *)0xE000ED38UL)

#define CFSR_MMARVALID  (1UL << 7)
#define CFSR_BFARVALID  (1UL << 15)

#define EXC_RETURN_THREAD  (1UL << 3)

#define FAULT_MEMMANAGE  1
#define FAULT_BUSFAULT   2
#define FAULT_USAGEFAULT 3
#define FAULT_HARDFAULT  4

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

static const char *kind_name(uint32_t kind)
{
    switch (kind) {
    case FAULT_MEMMANAGE:  return "MemManage";
    case FAULT_BUSFAULT:   return "BusFault";
    case FAULT_USAGEFAULT: return "UsageFault";
    default:               return "HardFault";
    }
}

void br_hal_fault(uint32_t *frame, uint32_t exc_return, uint32_t kind);

void br_hal_fault(uint32_t *frame, uint32_t exc_return, uint32_t kind)
{
    uint32_t cfsr = SCB_CFSR;
    uint32_t hfsr = SCB_HFSR;
    uint32_t addr = frame[6];

    if (cfsr & CFSR_MMARVALID) {
        addr = SCB_MMFAR;
    } else if (cfsr & CFSR_BFARVALID) {
        addr = SCB_BFAR;
    }

    br_tcb_t *t = br_sched_current();

#if CONFIG_MPU
    if ((exc_return & EXC_RETURN_THREAD) && t != NULL && t->user && !t->in_syscall) {
        br_uart_puts("\n*** TASK FAULT ***\n");
        br_uart_puts("Task:   ");
        br_uart_puts(t->name != NULL ? t->name : "?");
        br_uart_puts("\nKind:   ");
        br_uart_puts(kind_name(kind));
        br_uart_puts("\n");
        print_reg("Address: ", addr);
        print_reg("PC:      ", frame[6]);
        print_reg("CFSR:    ", cfsr);
        br_uart_puts("******************\n");

        SCB_CFSR = cfsr;
        SCB_HFSR = hfsr;
        br_task_fault(t, (uint8_t)kind, addr);
        return;
    }
#else
    (void)exc_return;
#endif

    br_uart_puts("\n*** CPU FAULT ***\n");
    br_uart_puts("Kind:   ");
    br_uart_puts(kind_name(kind));
    br_uart_puts("\n");
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
    print_reg("CFSR:    ", cfsr);
    print_reg("HFSR:    ", hfsr);
    print_reg("Address: ", addr);
    br_uart_puts("*****************\n");

    br_hal_panic("CPU fault", __FILE__, __LINE__);
}

#define FAULT_ENTRY(name, kind) \
    __attribute__((naked)) void name(void) \
    { \
        __asm volatile ( \
            "tst   lr, #4          \n" \
            "ite   eq              \n" \
            "mrseq r0, msp         \n" \
            "mrsne r0, psp         \n" \
            "mov   r1, lr          \n" \
            "mov   r2, %0          \n" \
            "b     br_hal_fault    \n" \
            :: "i" (kind) \
        ); \
    }

FAULT_ENTRY(HardFault_Handler,  FAULT_HARDFAULT)
FAULT_ENTRY(MemManage_Handler,  FAULT_MEMMANAGE)
FAULT_ENTRY(BusFault_Handler,   FAULT_BUSFAULT)
FAULT_ENTRY(UsageFault_Handler, FAULT_USAGEFAULT)

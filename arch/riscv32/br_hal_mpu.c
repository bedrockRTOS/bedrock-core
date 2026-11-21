/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "bedrock/br_syscall.h"

#if CONFIG_MPU

#define FRAME_WORDS      32
#define FRAME_RA         0
#define FRAME_A0         6
#define FRAME_A1         7
#define FRAME_A2         8
#define FRAME_A3         9
#define FRAME_A4         10
#define FRAME_A7         13
#define FRAME_MEPC       28
#define FRAME_MSTATUS    29
#define FRAME_USP        30

#define MSTATUS_MPIE     (1UL << 7)
#define MSTATUS_MPP_M    (3UL << 11)
#define MCAUSE_ECALL_U   8
#define MCAUSE_ECALL_M   11
#define SYSCALL_RETURN   0x7FFFUL

#define PMP_R            0x01
#define PMP_W            0x02
#define PMP_X            0x04
#define PMP_TOR          0x08
#define PMP_NAPOT        0x18

#define USER_DATA_SIZE   4096UL

extern char _text_start[];
extern char _code_end[];
extern char _user_data_start[];

extern br_tcb_t *br_sched_current(void);
extern uint32_t  br_syscall_dispatch(const uint32_t *args);
extern void      br_task_fault(br_tcb_t *tcb, uint8_t kind, uintptr_t addr);
extern void      br_uart_puts(const char *s);
extern void      br_uart_putc(char c);

static void print_hex(uint32_t v)
{
    br_uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        br_uart_putc("0123456789abcdef"[(v >> shift) & 0xF]);
    }
}

static uint32_t napot(uintptr_t base, size_t size)
{
    return (uint32_t)((base | ((size >> 1) - 1)) >> 2);
}

static void write_stack_and_grants(const br_tcb_t *t)
{
    uintptr_t sbase = t->regions[0].base + BR_USER_STACK_GUARD(t->regions[0].size);
    uintptr_t stop  = t->regions[0].base + t->regions[0].size;
    uint32_t  cfg1  = 0;
    uint32_t  cfg2  = 0;

    __asm volatile ("csrw pmpaddr4, %0" :: "r" ((uint32_t)(sbase >> 2)));
    __asm volatile ("csrw pmpaddr5, %0" :: "r" ((uint32_t)(stop >> 2)));
    cfg1 |= (uint32_t)(PMP_TOR | PMP_R | PMP_W) << 8;

    for (uint32_t i = 1; i < BR_TASK_REGIONS; i++) {
        uint32_t cfg = 0;
        uint32_t addr = 0;

        if (i < t->nregions) {
            addr = napot(t->regions[i].base, t->regions[i].size);
            cfg  = PMP_NAPOT | ((t->regions[i].perms & BR_MEM_R) ? PMP_R : 0) |
                   ((t->regions[i].perms & BR_MEM_W) ? PMP_W : 0);
        }

        switch (i) {
        case 1:
            __asm volatile ("csrw pmpaddr6, %0" :: "r" (addr));
            cfg1 |= cfg << 16;
            break;
        case 2:
            __asm volatile ("csrw pmpaddr7, %0" :: "r" (addr));
            cfg1 |= cfg << 24;
            break;
        default:
            __asm volatile ("csrw pmpaddr8, %0" :: "r" (addr));
            cfg2 |= cfg;
            break;
        }
    }

    __asm volatile ("csrw pmpcfg1, %0" :: "r" (cfg1));
    __asm volatile ("csrw pmpcfg2, %0" :: "r" (cfg2));
}

bool br_hal_mpu_region_valid(const br_region_t *r)
{
    return r->size >= 8 && (r->size & (r->size - 1)) == 0 &&
           (r->base & (r->size - 1)) == 0 && r->base + r->size > r->base;
}

void br_hal_mpu_code_region(br_region_t *r)
{
    r->base  = (uintptr_t)_text_start;
    r->size  = (size_t)((uintptr_t)_code_end - (uintptr_t)_text_start);
    r->perms = BR_MEM_R | BR_MEM_X;
}

void br_hal_mpu_user_data_region(br_region_t *r)
{
    r->base  = (uintptr_t)_user_data_start;
    r->size  = USER_DATA_SIZE;
    r->perms = BR_MEM_R | BR_MEM_W;
}

void br_hal_mpu_init(void)
{
    uint32_t cfg0 = 0;

    __asm volatile ("csrw pmpaddr0, %0" :: "r" ((uint32_t)((uintptr_t)_text_start >> 2)));
    __asm volatile ("csrw pmpaddr1, %0" :: "r" ((uint32_t)((uintptr_t)_code_end >> 2)));
    cfg0 |= (uint32_t)(PMP_TOR | PMP_R | PMP_X) << 8;

    __asm volatile ("csrw pmpaddr2, %0" :: "r" (napot((uintptr_t)_user_data_start, USER_DATA_SIZE)));
    cfg0 |= (uint32_t)(PMP_NAPOT | PMP_R | PMP_W) << 16;

    __asm volatile ("csrw pmpcfg0, %0" :: "r" (cfg0));
    __asm volatile ("csrw pmpcfg1, zero");
    __asm volatile ("csrw pmpcfg2, zero");
}

void br_hal_mpu_switch(br_tcb_t *t);

void br_hal_mpu_switch(br_tcb_t *t)
{
    if (t->user) {
        write_stack_and_grants(t);
    }
}

__attribute__((naked))
uint32_t br_hal_syscall(uint32_t num, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)num; (void)a0; (void)a1; (void)a2; (void)a3;
    __asm volatile (
        "ecall\n"
        "ret\n"
    );
}

static void syscall_thread(const uint32_t *args) __attribute__((noreturn, used));

static void syscall_thread(const uint32_t *args)
{
    uint32_t r = br_syscall_dispatch(args);

    __asm volatile (
        "mv a0, %0\n"
        "li a7, %1\n"
        "ecall\n"
        :: "r" (r), "i" (SYSCALL_RETURN) : "a0", "a7"
    );
    __builtin_unreachable();
}

void *br_hal_mpu_trap(uint32_t *frame, uint32_t mcause, bool *handled);

void *br_hal_mpu_trap(uint32_t *frame, uint32_t mcause, bool *handled)
{
    br_tcb_t *t = br_sched_current();
    bool from_user = ((frame[FRAME_MSTATUS] >> 11) & 3U) == 0;

    *handled = true;

    if (mcause == MCAUSE_ECALL_U && from_user && t != NULL && t->user && !t->in_syscall) {
        for (int i = 0; i < FRAME_WORDS; i++) {
            t->ctx_save[i] = frame[i];
        }

        t->sysargs[0] = frame[FRAME_A0];
        t->sysargs[1] = frame[FRAME_A1];
        t->sysargs[2] = frame[FRAME_A2];
        t->sysargs[3] = frame[FRAME_A3];
        t->sysargs[4] = frame[FRAME_A4];
        t->ctx_save[FRAME_MEPC] += 4;

        for (int i = 0; i < FRAME_WORDS; i++) {
            frame[i] = 0;
        }
        frame[FRAME_A0]      = (uint32_t)(uintptr_t)t->sysargs;
        frame[FRAME_MEPC]    = (uint32_t)(uintptr_t)syscall_thread;
        frame[FRAME_MSTATUS] = MSTATUS_MPP_M | MSTATUS_MPIE;

        t->user_frame = t->ctx_save;
        t->in_syscall = true;
        return frame;
    }

    if (mcause == MCAUSE_ECALL_M && !from_user && t != NULL && t->in_syscall &&
        frame[FRAME_A7] == SYSCALL_RETURN) {
        uint32_t *user = t->user_frame;

        user[FRAME_A0] = frame[FRAME_A0];
        t->in_syscall  = false;
        t->user_frame  = NULL;
        return user;
    }

    if (from_user && t != NULL && t->user && !t->in_syscall) {
        uint32_t mtval;
        __asm volatile ("csrr %0, mtval" : "=r" (mtval));

        br_uart_puts("\n*** TASK FAULT ***\n");
        br_uart_puts("Task:   ");
        br_uart_puts(t->name != NULL ? t->name : "?");
        br_uart_puts("\nCause:  ");
        print_hex(mcause);
        br_uart_puts("\nAddress: ");
        print_hex(mtval);
        br_uart_puts("\nPC:      ");
        print_hex(frame[FRAME_MEPC]);
        br_uart_puts("\n******************\n");

        br_task_fault(t, (uint8_t)mcause, mtval);
        return frame;
    }

    *handled = false;
    return frame;
}

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/bedrock.h"
#include "bedrock/br_syscall.h"
#include "x86_io.h"

#if CONFIG_MPU

#define PAGE_SIZE        4096UL
#define PAGE_COUNT       1024U
#define MAPPED_LIMIT     (PAGE_SIZE * PAGE_COUNT)

#define PTE_PRESENT      0x001U
#define PTE_WRITE        0x002U
#define PTE_USER         0x004U

#define CR0_WP           (1UL << 16)
#define CR0_PG           (1UL << 31)

#define FRAME_EDI        0
#define FRAME_ESI        1
#define FRAME_EBX        4
#define FRAME_EDX        5
#define FRAME_ECX        6
#define FRAME_EAX        7
#define FRAME_VECTOR     8
#define FRAME_ERR        9
#define FRAME_EIP        10
#define FRAME_CS         11
#define FRAME_EFLAGS     12
#define FRAME_USER_WORDS 15
#define FRAME_KERNEL_WORDS 13

#define KERNEL_CODE_SELECTOR  0x08
#define USER_DATA_SIZE   4096UL

#define VECTOR_SYSCALL   0x80
#define VECTOR_SYSRET    0x81
#define VECTOR_PAGEFAULT 14

extern char _text_start[];
extern char _code_end[];
extern char _user_data_start[];

extern uint64_t br_hal_gdt[];
extern struct {
    uint32_t prev;
    uint32_t esp0;
} br_hal_tss;

extern br_tcb_t *br_sched_current(void);
extern uint32_t  br_syscall_dispatch(const uint32_t *args);
extern void      br_task_fault(br_tcb_t *tcb, uint8_t kind, uintptr_t addr);
extern void      br_uart_puts(const char *s);
extern void      br_uart_putc(char c);

static uint32_t page_dir[1024]  __attribute__((aligned(4096)));
static uint32_t page_table[1024] __attribute__((aligned(4096)));
static br_tcb_t *mapped;

static void print_hex(uint32_t v)
{
    br_uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        br_uart_putc("0123456789abcdef"[(v >> shift) & 0xF]);
    }
}

static void invlpg(uintptr_t addr)
{
    __asm volatile ("invlpg (%0)" :: "r" (addr) : "memory");
}

static void pages_set_user(uintptr_t base, size_t size, bool user)
{
    for (uintptr_t a = base; a < base + size; a += PAGE_SIZE) {
        uint32_t *pte = &page_table[a / PAGE_SIZE];
        if (user) {
            *pte |= PTE_USER;
        } else {
            *pte &= ~PTE_USER;
        }
        invlpg(a);
    }
}

static void task_map(br_tcb_t *t, bool user)
{
    for (int i = 0; i < t->nregions; i++) {
        pages_set_user(t->regions[i].base, t->regions[i].size, user);
    }
}

bool br_hal_mpu_region_valid(const br_region_t *r)
{
    return r->size >= PAGE_SIZE && (r->size % PAGE_SIZE) == 0 &&
           (r->base % PAGE_SIZE) == 0 && r->base + r->size > r->base &&
           r->base + r->size <= MAPPED_LIMIT;
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
    for (uint32_t i = 0; i < PAGE_COUNT; i++) {
        page_table[i] = (i * (uint32_t)PAGE_SIZE) | PTE_PRESENT | PTE_WRITE;
    }

    for (uintptr_t a = (uintptr_t)_text_start; a < (uintptr_t)_code_end; a += PAGE_SIZE) {
        page_table[a / PAGE_SIZE] = (uint32_t)a | PTE_PRESENT | PTE_USER;
    }
    for (uintptr_t a = (uintptr_t)_user_data_start; a < (uintptr_t)_user_data_start + USER_DATA_SIZE;
         a += PAGE_SIZE) {
        page_table[a / PAGE_SIZE] = (uint32_t)a | PTE_PRESENT | PTE_WRITE | PTE_USER;
    }

    page_dir[0] = (uint32_t)(uintptr_t)page_table | PTE_PRESENT | PTE_WRITE | PTE_USER;

    uint32_t limit = (uint32_t)(((uintptr_t)_code_end >> 12) - 1);
    br_hal_gdt[3] = 0x00C0FA0000000000ULL | (limit & 0xFFFFU) | ((uint64_t)(limit >> 16) << 48);

    uint32_t cr0;
    __asm volatile ("movl %0, %%cr3" :: "r" ((uint32_t)(uintptr_t)page_dir) : "memory");
    __asm volatile ("movl %%cr0, %0" : "=r" (cr0));
    cr0 |= CR0_PG | CR0_WP;
    __asm volatile ("movl %0, %%cr0" :: "r" (cr0) : "memory");
}

void br_hal_mpu_switch(br_tcb_t *t);

void br_hal_mpu_switch(br_tcb_t *t)
{
    if (mapped != NULL) {
        task_map(mapped, false);
        mapped = NULL;
    }
    if (t->user) {
        task_map(t, true);
        mapped = t;
        br_hal_tss.esp0 = (uint32_t)(uintptr_t)t->kstack_top;
    }
}

__attribute__((naked))
uint32_t br_hal_syscall(uint32_t num, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)num; (void)a0; (void)a1; (void)a2; (void)a3;
    __asm volatile (
        "pushl %ebx\n"
        "pushl %esi\n"
        "movl 12(%esp), %eax\n"
        "movl 16(%esp), %ebx\n"
        "movl 20(%esp), %ecx\n"
        "movl 24(%esp), %edx\n"
        "movl 28(%esp), %esi\n"
        "int $0x80\n"
        "popl %esi\n"
        "popl %ebx\n"
        "ret\n"
    );
}

static void syscall_thread(const uint32_t *args) __attribute__((noreturn, used));

static void syscall_thread(const uint32_t *args)
{
    uint32_t r = br_syscall_dispatch(args);

    __asm volatile ("int $0x81" :: "a" (r));
    __builtin_unreachable();
}

void *br_hal_mpu_trap(uint32_t *frame, uint32_t vector, bool *handled);

void *br_hal_mpu_trap(uint32_t *frame, uint32_t vector, bool *handled)
{
    br_tcb_t *t = br_sched_current();
    bool from_user = (frame[FRAME_CS] & 3U) == 3U;

    *handled = true;

    if (vector == VECTOR_SYSCALL && from_user && t != NULL && t->user && !t->in_syscall) {
        for (int i = 0; i < FRAME_USER_WORDS; i++) {
            t->ctx_save[i] = frame[i];
        }
        t->sysargs[0] = frame[FRAME_EAX];
        t->sysargs[1] = frame[FRAME_EBX];
        t->sysargs[2] = frame[FRAME_ECX];
        t->sysargs[3] = frame[FRAME_EDX];
        t->sysargs[4] = frame[FRAME_ESI];

        uint32_t *top = (uint32_t *)t->kstack_top;
        top[-2] = 0;
        top[-1] = (uint32_t)(uintptr_t)t->sysargs;

        uint32_t *kf = top - 2 - FRAME_KERNEL_WORDS;
        for (int i = 0; i < FRAME_KERNEL_WORDS; i++) {
            kf[i] = 0;
        }
        kf[FRAME_EIP]    = (uint32_t)(uintptr_t)syscall_thread;
        kf[FRAME_CS]     = KERNEL_CODE_SELECTOR;
        kf[FRAME_EFLAGS] = 0x202;

        t->user_frame = t->ctx_save;
        t->in_syscall = true;
        return kf;
    }

    if (vector == VECTOR_SYSRET && !from_user && t != NULL && t->in_syscall) {
        uint32_t *user = t->user_frame;

        user[FRAME_EAX] = frame[FRAME_EAX];
        t->in_syscall   = false;
        t->user_frame   = NULL;
        return user;
    }

    if (from_user && t != NULL && t->user && !t->in_syscall && vector < 0x20) {
        uint32_t addr = frame[FRAME_EIP];
        if (vector == VECTOR_PAGEFAULT) {
            __asm volatile ("movl %%cr2, %0" : "=r" (addr));
        }

        br_uart_puts("\n*** TASK FAULT ***\n");
        br_uart_puts("Task:   ");
        br_uart_puts(t->name != NULL ? t->name : "?");
        br_uart_puts("\nVector: ");
        print_hex(vector);
        br_uart_puts("\nAddress: ");
        print_hex(addr);
        br_uart_puts("\nEIP:     ");
        print_hex(frame[FRAME_EIP]);
        br_uart_puts("\n******************\n");

        br_task_fault(t, (uint8_t)vector, addr);
        return frame;
    }

    *handled = false;
    return frame;
}

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "bedrock/br_hal.h"
#include "x86_io.h"

#define FRAME_WORDS      13
#define FRAME_EIP        10
#define FRAME_CS         11
#define FRAME_EFLAGS     12
#define FRAME_RET        13
#define FRAME_ARG        14
#define EFLAGS_IF        0x200U
#define CODE_SELECTOR    0x08
#define USER_CODE_SELECTOR  0x1B
#define USER_DATA_SELECTOR  0x23
#define FRAME_ESP        13
#define FRAME_SS         14
#define FRAME_USER_WORDS 15

#define VECTOR_COUNT     51
#define VECTOR_SYSCALL   0x80
#define VECTOR_SYSRET    0x81

extern void br_hal_timer_isr(void);
extern void br_hal_external_isr(uint32_t irq);
extern void br_uart_puts(const char *s);
extern void br_uart_putc(char c);

#if CONFIG_MPU
extern void *br_hal_mpu_trap(uint32_t *frame, uint32_t vector, bool *handled);
extern void  br_hal_mpu_switch(br_tcb_t *t);
#endif

static void **volatile switch_old_sp;
static void **volatile switch_new_sp;
static volatile bool   switch_pending;
static volatile bool   in_isr;

extern void br_hal_trap_return(void);

__asm__(
    ".section .text\n"
    ".macro ISR_NOERR n\n"
    "    .pushsection .text\n"
    "    isr_stub_\\n:\n"
    "    pushl $0\n"
    "    pushl $\\n\n"
    "    jmp isr_common\n"
    "    .popsection\n"
    "    .pushsection .rodata.isr_table,\"a\"\n"
    "    .long isr_stub_\\n\n"
    "    .popsection\n"
    ".endm\n"
    ".macro ISR_ERR n\n"
    "    .pushsection .text\n"
    "    isr_stub_\\n:\n"
    "    pushl $\\n\n"
    "    jmp isr_common\n"
    "    .popsection\n"
    "    .pushsection .rodata.isr_table,\"a\"\n"
    "    .long isr_stub_\\n\n"
    "    .popsection\n"
    ".endm\n"
    ".pushsection .rodata.isr_table,\"a\"\n"
    ".globl br_hal_isr_table\n"
    ".align 4\n"
    "br_hal_isr_table:\n"
    ".popsection\n"
    "ISR_NOERR 0\n"
    "ISR_NOERR 1\n"
    "ISR_NOERR 2\n"
    "ISR_NOERR 3\n"
    "ISR_NOERR 4\n"
    "ISR_NOERR 5\n"
    "ISR_NOERR 6\n"
    "ISR_NOERR 7\n"
    "ISR_ERR 8\n"
    "ISR_NOERR 9\n"
    "ISR_ERR 10\n"
    "ISR_ERR 11\n"
    "ISR_ERR 12\n"
    "ISR_ERR 13\n"
    "ISR_ERR 14\n"
    "ISR_NOERR 15\n"
    "ISR_NOERR 16\n"
    "ISR_ERR 17\n"
    "ISR_NOERR 18\n"
    "ISR_NOERR 19\n"
    "ISR_NOERR 20\n"
    "ISR_ERR 21\n"
    "ISR_NOERR 22\n"
    "ISR_NOERR 23\n"
    "ISR_NOERR 24\n"
    "ISR_NOERR 25\n"
    "ISR_NOERR 26\n"
    "ISR_NOERR 27\n"
    "ISR_NOERR 28\n"
    "ISR_ERR 29\n"
    "ISR_ERR 30\n"
    "ISR_NOERR 31\n"
    "ISR_NOERR 32\n"
    "ISR_NOERR 33\n"
    "ISR_NOERR 34\n"
    "ISR_NOERR 35\n"
    "ISR_NOERR 36\n"
    "ISR_NOERR 37\n"
    "ISR_NOERR 38\n"
    "ISR_NOERR 39\n"
    "ISR_NOERR 40\n"
    "ISR_NOERR 41\n"
    "ISR_NOERR 42\n"
    "ISR_NOERR 43\n"
    "ISR_NOERR 44\n"
    "ISR_NOERR 45\n"
    "ISR_NOERR 46\n"
    "ISR_NOERR 47\n"
    "ISR_NOERR 48\n"
    "ISR_NOERR 128\n"
    "ISR_NOERR 129\n"
    ".globl br_hal_trap_return\n"
    ".align 16\n"
    "isr_common:\n"
    "    pushal\n"
    "    cld\n"
    "    movl %esp, %eax\n"
    "    pushl %eax\n"
    "    call br_hal_trap\n"
    "    addl $4, %esp\n"
    "    movl %eax, %esp\n"
    "br_hal_trap_return:\n"
    "    popal\n"
    "    addl $8, %esp\n"
    "    iret\n"
);

extern const uint32_t br_hal_isr_table[VECTOR_COUNT];

static void print_hex(uint32_t v)
{
    br_uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        br_uart_putc("0123456789abcdef"[(v >> shift) & 0xF]);
    }
}

void *br_hal_trap(uint32_t *sp);

void *br_hal_trap(uint32_t *sp)
{
    uint32_t vector = sp[8];

    in_isr = true;

#if CONFIG_MPU
    if (vector == VECTOR_SYSCALL || vector == VECTOR_SYSRET || (sp[FRAME_CS] & 3U) == 3U ||
        vector < VECTOR_IRQ_BASE) {
        bool handled = false;
        void *next = br_hal_mpu_trap(sp, vector, &handled);
        if (handled) {
            sp = next;
            vector = 0xFFFFFFFFU;
        }
    }
#endif

    if (vector == 0xFFFFFFFFU) {
        /* handled by the memory protection layer */
    } else if (vector < VECTOR_IRQ_BASE) {
        br_uart_puts("\nCPU exception ");
        print_hex(vector);
        br_uart_puts(" error ");
        print_hex(sp[9]);
        br_uart_puts(" eip ");
        print_hex(sp[FRAME_EIP]);
        br_uart_puts("\n");
        br_hal_panic("Unhandled CPU exception", __FILE__, __LINE__);
    } else if (vector < VECTOR_IRQ_BASE + 16) {
        uint32_t irq = vector - VECTOR_IRQ_BASE;

        if (irq >= 8) {
            outb(PIC2_CMD, PIC_EOI);
        }
        outb(PIC1_CMD, PIC_EOI);

        if (irq == 0) {
            br_hal_timer_isr();
        } else {
            br_hal_external_isr(irq);
        }
    }

    in_isr = false;

    if (switch_pending) {
        switch_pending = false;
        *switch_old_sp = sp;
#if CONFIG_MPU
        br_hal_mpu_switch((br_tcb_t *)switch_new_sp);
#endif
        sp = *switch_new_sp;
    }

    return sp;
}

static void task_exit_handler(void)
{
    while (1) {
        __asm volatile ("hlt");
    }
}

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg)
{
    uint32_t *sp = (uint32_t *)((uintptr_t)stack_top & ~0xFUL);

    sp -= FRAME_ARG + 1;
    for (int i = 0; i < FRAME_ARG + 1; i++) {
        sp[i] = 0;
    }

    sp[FRAME_EIP]    = (uint32_t)(uintptr_t)entry;
    sp[FRAME_CS]     = CODE_SELECTOR;
    sp[FRAME_EFLAGS] = EFLAGS_IF | 0x2U;
    sp[FRAME_RET]    = (uint32_t)(uintptr_t)task_exit_handler;
    sp[FRAME_ARG]    = (uint32_t)(uintptr_t)arg;

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
        __asm volatile ("int $0x30" ::: "memory");
    }
}

#if CONFIG_MPU

extern br_tcb_t *br_sched_current(void);

void *br_hal_stack_init_user(void *kstack_top, void *stack_top, br_task_entry_t entry, void *arg)
{
    uint32_t *usp = (uint32_t *)((uintptr_t)stack_top & ~0xFUL);
    usp -= 2;
    usp[0] = (uint32_t)(uintptr_t)task_exit_handler;
    usp[1] = (uint32_t)(uintptr_t)arg;

    uint32_t *sp = (uint32_t *)((uintptr_t)kstack_top & ~0xFUL);
    sp -= FRAME_USER_WORDS;
    for (int i = 0; i < FRAME_USER_WORDS; i++) {
        sp[i] = 0;
    }

    sp[FRAME_EIP]    = (uint32_t)(uintptr_t)entry;
    sp[FRAME_CS]     = USER_CODE_SELECTOR;
    sp[FRAME_EFLAGS] = EFLAGS_IF | 0x2U;
    sp[FRAME_ESP]    = (uint32_t)(uintptr_t)usp;
    sp[FRAME_SS]     = USER_DATA_SELECTOR;
    return sp;
}

void br_hal_start_first_task(void *sp)
{
    br_hal_mpu_switch(br_sched_current());

    __asm volatile (
        "movl %0, %%esp\n"
        "jmp br_hal_trap_return\n"
        :: "r" (sp)
    );
    __builtin_unreachable();
}

#else

__attribute__((naked))
void br_hal_start_first_task(void *sp)
{
    (void)sp;
    __asm volatile (
        "movl 4(%esp), %esp\n"
        "jmp br_hal_trap_return\n"
    );
}

#endif

uint32_t br_hal_irq_disable(void)
{
    uint32_t flags;
    __asm volatile ("pushfl\n\tcli\n\tpopl %0" : "=r" (flags) :: "memory");
    return flags & EFLAGS_IF;
}

void br_hal_irq_restore(uint32_t state)
{
    if (state & EFLAGS_IF) {
        __asm volatile ("sti" ::: "memory");
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

void br_hal_check_stack_overflow(br_tcb_t *tcb)
{
    if (tcb == NULL || tcb->stack_canary == NULL) {
        return;
    }

    if (*(tcb->stack_canary) != BR_STACK_CANARY) {
        br_hal_panic("Stack overflow detected", __FILE__, __LINE__);
    }
}

__attribute__((noreturn))
void br_hal_panic(const char *msg, const char *file, int line)
{
    __asm volatile ("cli" ::: "memory");

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
        print_hex((uint32_t)line);
        br_uart_puts("\n");
    }
    br_uart_puts("********************\n");

    while (1) {
        __asm volatile ("hlt");
    }
}

__attribute__((noreturn))
void br_hal_reset(void)
{
    __asm volatile ("cli" ::: "memory");
    outb(0x64, 0xFE);

    while (1) {
        __asm volatile ("hlt");
    }
}

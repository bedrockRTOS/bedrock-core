/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <avr/io.h>
#include <avr/wdt.h>

#include "bedrock/br_hal.h"
#include "br_avr_isr.h"

#define IRQ_FLAG  0x80U

extern void br_uart_puts(const char *s);

uint8_t br_avr_isr_stack[BR_AVR_ISR_STACK_SIZE];

/* The address the interrupt stubs set the stack pointer to. It is defined
 * here, next to the array, so the size is only known in this file. */
__asm__(
    ".global br_avr_isr_stack_top\n"
    ".set br_avr_isr_stack_top, br_avr_isr_stack + " BR_AVR_STR(BR_AVR_ISR_STACK_SIZE) " - 1\n"
);
void *volatile br_avr_isr_sp;
volatile uint8_t br_avr_in_isr;

static void **volatile switch_old_sp;
static void **volatile switch_new_sp;
static volatile bool   switch_pending;

/*
 * Called by the interrupt stubs of BR_AVR_KERNEL_ISR when the handler
 * returns. Gives the stack pointer to continue with: the interrupted task,
 * or the task the handler switched to.
 */
void *br_avr_isr_exit(void) __attribute__((used));

void *br_avr_isr_exit(void)
{
    br_avr_in_isr = 0;

    if (br_avr_isr_stack[0] != BR_AVR_ISR_CANARY || br_avr_isr_stack[1] != BR_AVR_ISR_CANARY) {
        br_hal_panic("Interrupt stack overflow", __FILE__, __LINE__);
    }

    if (switch_pending) {
        switch_pending = false;
        *switch_old_sp = br_avr_isr_sp;
        return *switch_new_sp;
    }
    return br_avr_isr_sp;
}

static void task_exit_handler(void)
{
    while (1) { }
}

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg)
{
    uint8_t *sp = (uint8_t *)stack_top - 1;
    uint16_t exit_pc  = (uint16_t)(uintptr_t)task_exit_handler;
    uint16_t entry_pc = (uint16_t)(uintptr_t)entry;
    uint16_t a        = (uint16_t)(uintptr_t)arg;

    /* The entry function returns to task_exit_handler */
    *sp-- = (uint8_t)exit_pc;
    *sp-- = (uint8_t)(exit_pc >> 8);

    /* Where the first restore of the frame returns to */
    *sp-- = (uint8_t)entry_pc;
    *sp-- = (uint8_t)(entry_pc >> 8);

    *sp-- = 0;          /* r0 */
    *sp-- = IRQ_FLAG;    /* SREG: the task starts with interrupts on */
    *sp-- = 0;          /* r1, the zero register */

    for (uint8_t r = 2; r <= 31; r++) {
        uint8_t v = 0;
        if (r == 24) {
            v = (uint8_t)a;
        } else if (r == 25) {
            v = (uint8_t)(a >> 8);
        }
        *sp-- = v;
    }

    return sp;
}

/* Save this task, make the one that old_sp/new_sp name run. The arguments
 * are in r25:r24 and r23:r22. */
__attribute__((naked, noinline))
static void switch_now(void **old_sp, void **new_sp)
{
    (void)old_sp;
    (void)new_sp;

    __asm__ volatile (
        BR_AVR_SAVE_CONTEXT
        "movw r30, r24\n\t"
        "in r18, __SP_L__\n\t"
        "st Z+, r18\n\t"
        "in r19, __SP_H__\n\t"
        "st Z, r19\n\t"
        "movw r30, r22\n\t"
        "ld r18, Z+\n\t"
        "ld r19, Z\n\t"
        "out __SP_H__, r19\n\t"
        "out __SP_L__, r18\n\t"
        BR_AVR_RESTORE_CONTEXT
        "ret\n\t"
    );
}

void br_hal_context_switch(void **old_sp, void **new_sp)
{
    if (br_avr_in_isr) {
        if (!switch_pending) {
            switch_old_sp = old_sp;
        }
        switch_new_sp = new_sp;
        switch_pending = true;
        return;
    }

    switch_now(old_sp, new_sp);
}

__attribute__((naked, noreturn))
static void start_first_task(void *sp)
{
    (void)sp;

    __asm__ volatile (
        "cli\n\t"
        "out __SP_H__, r25\n\t"
        "out __SP_L__, r24\n\t"
        BR_AVR_RESTORE_CONTEXT
        "ret\n\t"
    );
}

void br_hal_start_first_task(void *sp)
{
    for (uint8_t i = 0; i < BR_AVR_ISR_STACK_SIZE; i++) {
        br_avr_isr_stack[i] = BR_AVR_ISR_CANARY;
    }
    start_first_task(sp);
}

uint32_t br_hal_irq_disable(void)
{
    uint8_t state = SREG;
    __asm__ volatile ("cli" ::: "memory");
    return state;
}

void br_hal_irq_restore(uint32_t state)
{
    if (state & IRQ_FLAG) {
        __asm__ volatile ("sei" ::: "memory");
    }
}

bool br_hal_in_isr(void)
{
    return br_avr_in_isr != 0;
}

/* The board brings its peripherals up in br_hal_board_init(). The avr-libc
 * start code calls main() after .init9. .init7 runs after .data and .bss are
 * set up and before main(), which is where the board is initialized. There is
 * no default, so that the board code is always linked from its library. */
extern void br_hal_board_init(void);

__attribute__((naked, used, section(".init7")))
static void early_board_init(void)
{
    __asm__ volatile ("call br_hal_board_init");
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
    __asm__ volatile ("cli" ::: "memory");

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

        char digits[6];
        int i = 5;
        unsigned v = (unsigned)line;
        digits[i] = '\0';
        do {
            digits[--i] = (char)('0' + v % 10);
            v /= 10;
        } while (v != 0 && i > 0);
        br_uart_puts(&digits[i]);
        br_uart_puts("\n");
    }
    br_uart_puts("********************\n");

    while (1) { }
}

__attribute__((noreturn))
void br_hal_reset(void)
{
    __asm__ volatile ("cli" ::: "memory");
    wdt_enable(WDTO_15MS);
    while (1) { }
}

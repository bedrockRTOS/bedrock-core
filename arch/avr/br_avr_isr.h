/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_AVR_ISR_H
#define BR_AVR_ISR_H

/*
 * Context frame of a task on AVR, from the lowest address up:
 *
 *   r31 ... r2, r1, SREG, r0, return address (high byte first)
 *
 * A task that was switched out by br_hal_context_switch() and a task that
 * was interrupted have the same frame, so either can be resumed by the
 * same code. The frame of a task is 35 bytes.
 *
 * An interrupt that calls into the kernel saves the frame on the stack of
 * the task, then runs its handler on a separate interrupt stack, so a task
 * stack only needs room for the task itself and the 35-byte frame.
 * br_hal_context_switch() called by the handler only records the switch,
 * which is made when the handler returns.
 *
 * Only AVR devices with a 2-byte program counter are supported (up to
 * 128 KiB of flash, which includes the ATmega328P).
 */

#ifdef __AVR_3_BYTE_PC__
#  error "AVR devices with more than 128 KiB of flash are not supported"
#endif

#define BR_AVR_ISR_STACK_SIZE  112

/* The lowest bytes of the interrupt stack hold a canary */
#define BR_AVR_ISR_CANARY      0xA5

#define BR_AVR_STR_(x)  #x
#define BR_AVR_STR(x)   BR_AVR_STR_(x)

#define BR_AVR_SAVE_CONTEXT \
    "push r0\n\t" \
    "in r0, __SREG__\n\t" \
    "cli\n\t" \
    "push r0\n\t" \
    "push r1\n\t" \
    "clr r1\n\t" \
    "push r2\n\t"  "push r3\n\t"  "push r4\n\t"  "push r5\n\t" \
    "push r6\n\t"  "push r7\n\t"  "push r8\n\t"  "push r9\n\t" \
    "push r10\n\t" "push r11\n\t" "push r12\n\t" "push r13\n\t" \
    "push r14\n\t" "push r15\n\t" "push r16\n\t" "push r17\n\t" \
    "push r18\n\t" "push r19\n\t" "push r20\n\t" "push r21\n\t" \
    "push r22\n\t" "push r23\n\t" "push r24\n\t" "push r25\n\t" \
    "push r26\n\t" "push r27\n\t" "push r28\n\t" "push r29\n\t" \
    "push r30\n\t" "push r31\n\t"

#define BR_AVR_RESTORE_CONTEXT \
    "pop r31\n\t" "pop r30\n\t" "pop r29\n\t" "pop r28\n\t" \
    "pop r27\n\t" "pop r26\n\t" "pop r25\n\t" "pop r24\n\t" \
    "pop r23\n\t" "pop r22\n\t" "pop r21\n\t" "pop r20\n\t" \
    "pop r19\n\t" "pop r18\n\t" "pop r17\n\t" "pop r16\n\t" \
    "pop r15\n\t" "pop r14\n\t" "pop r13\n\t" "pop r12\n\t" \
    "pop r11\n\t" "pop r10\n\t" "pop r9\n\t"  "pop r8\n\t" \
    "pop r7\n\t"  "pop r6\n\t"  "pop r5\n\t"  "pop r4\n\t" \
    "pop r3\n\t"  "pop r2\n\t"  "pop r1\n\t" \
    "pop r0\n\t" \
    "out __SREG__, r0\n\t" \
    "pop r0\n\t"

/*
 * Define the interrupt vector `vec` (the number of the vector, for
 * example TIMER1_COMPA_vect_num) so that it runs the C function `handler`
 * as a kernel interrupt handler.
 */
#define BR_AVR_KERNEL_ISR(vec, handler) \
    __asm__( \
        ".section .text.br_isr_" BR_AVR_STR(vec) ",\"ax\",@progbits\n" \
        ".global __vector_" BR_AVR_STR(vec) "\n" \
        "__vector_" BR_AVR_STR(vec) ":\n\t" \
        BR_AVR_SAVE_CONTEXT \
        "in r30, __SP_L__\n\t" \
        "in r31, __SP_H__\n\t" \
        "ldd r24, Z+32\n\t" \
        "ori r24, 0x80\n\t" \
        "std Z+32, r24\n\t" \
        "sts br_avr_isr_sp, r30\n\t" \
        "sts br_avr_isr_sp+1, r31\n\t" \
        "ldi r24, lo8(br_avr_isr_stack_top)\n\t" \
        "ldi r25, hi8(br_avr_isr_stack_top)\n\t" \
        "out __SP_H__, r25\n\t" \
        "out __SP_L__, r24\n\t" \
        "ldi r24, 1\n\t" \
        "sts br_avr_in_isr, r24\n\t" \
        "call " #handler "\n\t" \
        "call br_avr_isr_exit\n\t" \
        "out __SP_H__, r25\n\t" \
        "out __SP_L__, r24\n\t" \
        BR_AVR_RESTORE_CONTEXT \
        "ret\n\t" \
        ".section .text\n")

#endif

/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

__attribute__((naked, section(".boot2")))
void boot2_entry(void)
{
    __asm volatile (
        ".syntax unified         \n"
        "push  {lr}              \n"
        "ldr   r3, =0x18000000   \n"
        "movs  r1, #0            \n"
        "str   r1, [r3, #0x08]   \n"
        "movs  r1, #4            \n"
        "str   r1, [r3, #0x14]   \n"
        "ldr   r1, =0x001F0300   \n"
        "str   r1, [r3, #0x00]   \n"
        "movs  r1, #0            \n"
        "str   r1, [r3, #0x04]   \n"
        "ldr   r1, =0x03000218   \n"
        "movs  r2, #0xF4         \n"
        "str   r1, [r3, r2]      \n"
        "movs  r1, #1            \n"
        "str   r1, [r3, #0x08]   \n"
        "pop   {r0}              \n"
        "cmp   r0, #0            \n"
        "beq   1f                \n"
        "bx    r0                \n"
        "1:                      \n"
        "ldr   r0, =0x10000100   \n"
        "ldr   r1, =0xE000ED08   \n"
        "str   r0, [r1]          \n"
        "ldmia r0, {r0, r1}      \n"
        "msr   msp, r0           \n"
        "bx    r1                \n"
        ".ltorg                  \n"
    );
}

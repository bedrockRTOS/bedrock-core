/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

extern uint32_t _sbss, _ebss;

extern int main(void);
extern void br_hal_trap_entry(void);

void reset_handler(void);

void reset_handler(void)
{
    uint32_t *dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    __asm volatile ("csrw mtvec, %0" :: "r" (br_hal_trap_entry));

    main();
    while (1) { }
}

__attribute__((naked, section(".text.start")))
void _start(void)
{
    __asm volatile (
        "la sp, _estack\n"
        "j reset_handler\n"
    );
}

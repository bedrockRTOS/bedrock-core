/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#include "bedrock/br_hal.h"

#define SIFIVE_TEST       (*(volatile uint32_t *)0x100000UL)
#define SIFIVE_TEST_RESET 0x7777UL

extern void br_uart_init(void);

void br_hal_board_init(void)
{
    br_uart_init();
}

__attribute__((noreturn))
void br_hal_reset(void)
{
    __asm volatile ("csrci mstatus, 8" ::: "memory");
    SIFIVE_TEST = SIFIVE_TEST_RESET;

    while (1) {
        __asm volatile ("wfi");
    }
}

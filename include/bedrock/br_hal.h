/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 */

#ifndef BR_HAL_H
#define BR_HAL_H

#include "br_types.h"

/*
 * Hardware Abstraction Layer Interface
 *
 * Each architecture implements these functions independently
 * in arch/<arch_name>/. The kernel never accesses hardware
 * directly -- only through this interface.
 */

/* Timer HAL -- tickless time source */

void br_hal_timer_init(void);
br_time_t br_hal_timer_get_us(void);
void br_hal_timer_set_alarm(br_time_t abs_us);
void br_hal_timer_cancel_alarm(void);

/* Interrupt control HAL */

uint32_t br_hal_irq_disable(void);
void br_hal_irq_restore(uint32_t state);
bool br_hal_in_isr(void);

/* Context switch HAL */

void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg);
void br_hal_context_switch(void **old_sp, void **new_sp);
void br_hal_start_first_task(void *sp) __attribute__((noreturn));

/* Board / early init HAL */

void br_hal_board_init(void);

#endif /* BR_HAL_H */

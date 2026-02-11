<!--
  Project: bedrock[RTOS]
  Version: 0.0.1
  Author:  AnmiTaliDev <anmitalidev@nuros.org>
  License: CC BY-SA 4.0
-->

# Porting Guide

This guide describes how to add support for a new architecture or board to bedrock[RTOS].

## Overview

bedrock[RTOS] strictly separates hardware-dependent code from the kernel. Porting requires:

1. Implementing the HAL functions declared in `include/bedrock/br_hal.h`
2. Providing a startup file (vector table, `.data`/`.bss` init)
3. Providing a linker script
4. Adding a build target in `chorus.build`

Zero kernel source changes are needed.

## Step 1: Create Architecture Directory

```
arch/<arch_name>/
├── br_hal_timer.c      Timer and interrupt control
├── br_hal_context.c    Context switch and stack init
└── startup.c           Vector table and Reset_Handler
```

## Step 2: Implement Timer HAL

```c
void br_hal_timer_init(void);
```

Initialize the hardware timer. Must provide a free-running time source with at least microsecond resolution.

```c
br_time_t br_hal_timer_get_us(void);
```

Return the current time in microseconds since boot. Must be monotonically increasing and handle overflow of the underlying hardware counter.

```c
void br_hal_timer_set_alarm(br_time_t abs_us);
```

Schedule a one-shot alarm at absolute time `abs_us`. When the alarm fires, call `br_time_alarm_handler()` (declared in `bedrock/bedrock.h`).

```c
void br_hal_timer_cancel_alarm(void);
```

Cancel any pending alarm.

## Step 3: Implement Interrupt Control HAL

```c
uint32_t br_hal_irq_disable(void);
```

Disable interrupts globally. Return the previous interrupt state for later restoration.

```c
void br_hal_irq_restore(uint32_t state);
```

Restore the interrupt state returned by `br_hal_irq_disable()`.

```c
bool br_hal_in_isr(void);
```

Return `true` if currently executing in an interrupt/exception context.

## Step 4: Implement Context Switch HAL

```c
void *br_hal_stack_init(void *stack_top, br_task_entry_t entry, void *arg);
```

Build an initial stack frame for a new task. The frame must be arranged so that `br_hal_start_first_task()` or `br_hal_context_switch()` can restore it and begin execution at `entry(arg)`.

**Requirements:**
- `stack_top` points to the top (highest address) of the stack
- The returned pointer is the initial stack pointer (after frame is built)
- The `sp` field of the TCB is the **first** struct member — assembly code relies on this

```c
void br_hal_context_switch(void **old_sp, void **new_sp);
```

Trigger a context switch. Save the current context and store the stack pointer at `*old_sp`. Restore the context from `*new_sp`.

On Cortex-M, this typically pends PendSV rather than switching immediately.

```c
void br_hal_start_first_task(void *sp) __attribute__((noreturn));
```

Start executing the first task. Load the stack pointer `sp`, restore the context, and jump to the task entry point. This function must never return.

## Step 5: Implement Board Init

```c
void br_hal_board_init(void);
```

Perform any early board-level initialization (clock setup, GPIO, peripheral enables). This is called before the timer is initialized. Can be a no-op if nothing is needed.

## Step 6: Startup Code

Provide a `startup.c` (or `.s`) with:

- Vector table placed in `.isr_vector` section
- `Reset_Handler`: copy `.data` from flash to SRAM, zero `.bss`, call `main()`
- Default handlers for exceptions
- Entries for `PendSV_Handler` and `SysTick_Handler` (or equivalent)

## Step 7: Linker Script

Create `boards/<board_name>/linker.ld` with:

- `MEMORY` regions for flash and RAM
- `ENTRY(Reset_Handler)`
- `.text` section with `KEEP(*(.isr_vector))`
- `.data` section with VMA in RAM, LMA in flash
- `.bss` section in RAM
- Export symbols: `_sidata`, `_sdata`, `_edata`, `_sbss`, `_ebss`, `_estack`

## Step 8: Build Configuration

Add compile and link targets for the new architecture in `chorus.build`.

## Reference

See `arch/arm-cortex-m/` and `boards/qemu-cortex-m3/` for a complete working example targeting the QEMU LM3S6965 Cortex-M3.

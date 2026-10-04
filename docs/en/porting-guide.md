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
├── br_hal_uart.c       br_uart_putc() and br_uart_puts()
└── startup.c           Reset entry, .bss init, trap or vector setup
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

The timer interrupt must also call `br_sched_tick(elapsed_us)` at least once per `CONFIG_RR_TIME_SLICE_US` so round-robin works.

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

This function is called with interrupts disabled, from a task or from an ISR. The switch itself can be deferred until interrupts are enabled again or the ISR returns. On Cortex-M it pends PendSV. On RISC-V it records the pointers, sets the CLINT software interrupt when called from a task, and the trap exit path swaps the stack.

If the function is called a second time before the deferred switch happens, keep the first `old_sp` and take the new `new_sp`. The first `old_sp` belongs to the task that is actually running.

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

Provide a `startup.c` (or `.s`) that sets up the stack, initializes `.data` and `.bss`, installs the interrupt entry and calls `main()`.

Cortex-M:

- Vector table placed in `.isr_vector` section
- `Reset_Handler`: copy `.data` from flash to SRAM, zero `.bss`, call `main()`
- Default handlers for exceptions
- Entries for `PendSV_Handler` and `SysTick_Handler`

RISC-V:

- `_start` in `.text.start`: load `sp` from `_estack`, jump to `reset_handler`
- `reset_handler`: zero `.bss`, write the trap entry to `mtvec`, call `main()`

## Step 7: Linker Script

Create `boards/<board_name>/linker.ld` with:

- `MEMORY` regions for flash and RAM
- `ENTRY(Reset_Handler)`
- `.text` section with `KEEP(*(.isr_vector))`
- `.data` section with VMA in RAM, LMA in flash
- `.bss` section in RAM
- Export symbols: `_sidata`, `_sdata`, `_edata`, `_sbss`, `_ebss`, `_estack`

On QEMU RISC-V `virt` the image is loaded straight into RAM at `0x80000000`, so there is one `RAM` region, no `.data` copy, `ENTRY(_start)` and only `_sbss`, `_ebss`, `_estack` are needed.

## Step 8: Build Configuration

Add compile and link targets for the new architecture in `chorus.build`, an `ARCH_<NAME>` option to `Kconfig`, a `boards/<board_name>/defconfig`, and a job in `.github/workflows/ci.yml` that builds the port and runs the tests on QEMU.

## Reference

- `arch/arm-cortex-m/` and `boards/qemu-cortex-m3/`: QEMU LM3S6965 (Cortex-M3)
- `arch/riscv32/` and `boards/qemu-riscv32-virt/`: QEMU `virt` (RV32IMAC, machine mode only)

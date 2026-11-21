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

Chip and board code that does not belong to the core, such as the UART driver and clock setup, can live in `boards/<board_name>/` instead (`br_hal_uart.c`, `board.c` with `br_hal_board_init()`). `arch/arm-cortex-m/` and `boards/stm32f103c8-bluepill/` are laid out this way.

The UART driver provides `br_uart_putc()`, `br_uart_puts()` and `br_uart_init()`. `br_uart_init()` is called from `br_hal_board_init()` and enables the receive interrupt. The receive ISR passes every byte to `br_uart_rx_push(char c)` from `kernel/br_uart.c`, which buffers it for `br_uart_getc()`. On Cortex-M3 the ISR overrides the weak `IRQ<n>_Handler` from `arch/arm-cortex-m/startup.c`.

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

Perform any early board-level initialization (clock setup, GPIO, peripheral enables, UART). The startup code calls it after `.data` and `.bss` are initialized and before `main()`, so `br_uart_puts()` works from the first line of `main()`. The kernel does not call it. Can be a no-op if nothing is needed. The arch layer provides a weak empty version, a board overrides it with its own `board.c` linked as an object file, not from an archive.

```c
void br_hal_reset(void) __attribute__((noreturn));
```

Reset the system. Disable interrupts, trigger the hardware reset and never return. It is used by the shell `reboot` command. Cortex-M writes `SYSRESETREQ` to `SCB->AIRCR` (`br_hal_context.c`). On the QEMU `virt` machine the RISC-V port writes the reset code to the SiFive test device (`boards/qemu-riscv32-virt/board.c`), real RISC-V boards need their own reset source. The host port exits the process.

## Step 5b: Implement Peripheral HAL

```c
br_err_t br_hal_gpio_configure(uint8_t port, uint8_t pin, br_gpio_mode_t mode);
br_err_t br_hal_spi_init(uint8_t bus, uint32_t hz, uint8_t mode);
br_err_t br_hal_i2c_init(uint8_t bus, uint32_t hz);
br_err_t br_hal_pwm_start(uint8_t ch, uint32_t freq_hz, uint16_t duty_permille);
br_err_t br_hal_capture_start(uint8_t ch, br_capture_edge_t edge);
```

and the rest of the GPIO, SPI, I2C, PWM and capture functions listed in `include/bedrock/br_hal.h`. Put them in `boards/<board_name>/br_hal_gpio.c`, `br_hal_spi.c`, `br_hal_i2c.c` and `br_hal_pwm.c`. A target without a peripheral still provides the functions and returns `BR_ERR_INVALID` (`boards/qemu-riscv32-virt/br_hal_periph.c`). The contract of each function and the pin and channel numbering of the existing targets are in the [driver guide](driver-guide.md).

## Step 5c: Network Interface

```c
struct br_netif *br_hal_netif_default(void);
```

Return the board network interface (`br_netif_t`, see [networking](networking.md)), or `NULL` if the board has none. A board with a MAC fills in the name, MAC address, MTU and the `send`, `recv` and `link_up` operations, and sets the hardware up on the first call. The function must exist on every target.

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

## Step 5d: Memory Protection (optional)

An architecture that supports `CONFIG_MPU` implements these functions from `br_hal.h`, with `CONFIG_MPU` set:

```c
void     br_hal_mpu_init(void);
bool     br_hal_mpu_region_valid(const br_region_t *r);
void     br_hal_mpu_code_region(br_region_t *r);
void     br_hal_mpu_user_data_region(br_region_t *r);
void    *br_hal_stack_init_user(void *kstack_top, void *stack_top, br_task_entry_t entry, void *arg);
uint32_t br_hal_syscall(uint32_t num, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
```

On every context switch to a task, program the memory protection for that task and set its privilege level. A system call is a trap that moves the task onto `tcb->kstack_top` as a privileged thread running `br_syscall_dispatch(args)`, and a second trap that restores the user context with the result. A fault from a task with `user` set and `in_syscall` clear calls `br_task_fault()`. The details of the three ports are in [memory protection](memory-protection.md).

## Adding a Board

A board is a directory `boards/<board_name>/` on top of an existing architecture. Nothing in `kernel/`, `lib/` or `drivers/` changes. `boards/stm32f4-discovery/` is a short example: an STM32F407 on the `arch/arm-cortex-m/` core.

1. **Directory.** Copy the board closest to yours and rename it.
2. **Linker script.** Flash and RAM regions with the real sizes, the sections from Step 7 and `_estack` at the top of RAM.
3. **Clock and pins.** `board.c` defines `br_hal_board_init()`. Enable the clocks and the pins of the console UART there, then call `br_uart_init()`. If the clock tree is left at its reset state, say so in the file name or the docs, and set `BR_HAL_SYS_CLOCK_HZ` to the real core clock in the build flags. The SysTick and the UART divider depend on it.
4. **UART.** `br_hal_uart.c` provides `br_uart_putc()`, `br_uart_puts()` and `br_uart_init()`, and passes received bytes to `br_uart_rx_push()` from an interrupt. On Cortex-M the handler is a function named `IRQ<n>_Handler`, which overrides the weak one in the startup file.
5. **Peripheral HAL.** `br_hal_gpio.c`, `br_hal_spi.c`, `br_hal_i2c.c`, `br_hal_pwm.c` and `br_hal_netif.c`, see Steps 5b and 5c. A peripheral the chip does not have returns `BR_ERR_INVALID`. Keep the register definitions in one header (`stm32f4_regs.h`) so the pin muxing is in one place.
6. **defconfig.** `boards/<board_name>/defconfig` with the core clock in `CONFIG_SYS_CLOCK_HZ`.
7. **Build.** A set of flags and a target in `chorus.build`. If the CPU differs from an existing board, such as Cortex-M4 and Cortex-M3, the kernel and the arch objects are built again with the new `-mcpu`, so the board gets its own variant.
8. **Tests.** The same `examples/test_*.c` must run unchanged. Add the board to the CI job of the emulator that can run it.

## Reference

- `arch/arm-cortex-m/` and `boards/qemu-cortex-m3/`: QEMU LM3S6965 (Cortex-M3)
- `arch/arm-cortex-m/` and `boards/stm32f103c8-bluepill/`: STM32F103C8 (Cortex-M3), board code in `board.c`
- `arch/riscv32/` and `boards/qemu-riscv32-virt/`: QEMU `virt` (RV32IMAC, machine mode only)
- `arch/arm-cortex-m0plus/` and `boards/rpi-pico/`: RP2040 (Cortex-M0+, ARMv6-M), with boot2 and UF2 tools
- `arch/arm-cortex-m/` and `boards/stm32f4-discovery/`: STM32F407 (Cortex-M4 without the FPU)
- `arch/x86/` and `boards/qemu-x86/`: 32-bit x86 in QEMU, started by the multiboot loader

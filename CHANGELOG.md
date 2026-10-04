# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/). Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Interrupt-driven UART receive on all targets: `br_uart_getc(char *c, br_time_t timeout)` reads from a ring buffer that the UART RX interrupt fills (`kernel/br_uart.c`). `br_uart_putc`, `br_uart_puts` and `br_uart_getc` are declared in `bedrock.h` as experimental API.
- `CONFIG_UART_RX_BUF_SIZE` (default 64).
- Cortex-M3 vector table has weak `IRQ0_Handler` to `IRQ63_Handler` that boards override.
- RISC-V: machine external interrupts through the QEMU `virt` PLIC.
- `test_uart_rx` on every target, run in CI with input fed to the console.
- `boards/qemu-cortex-m3/board.c` and `boards/qemu-riscv32-virt/board.c`.

### Changed
- The QEMU `virt` UART driver moved from `arch/riscv32/` to `boards/qemu-riscv32-virt/`.
- Libraries are linked inside `--start-group`/`--end-group` because the kernel and HAL now reference each other both ways.
- `boards/*/renode.resc` only set up the machine. The caller runs `emulation RunFor` and `quit`, and can feed UART input in between.

### Added
- RP2040 port in `arch/arm-cortex-m0plus`: ARMv6-M context switch, RP2040 timer with alarm 0, PL011 UART0 on GP0/GP1, XOSC clock setup. Tested in Renode, not on hardware.
- `boards/rpi-pico` with linker script, defconfig, a boot2 that sets up XIP with the 03h read command, and a Renode script.
- `tools/bootcrc.c` adds the boot2 CRC32, `tools/bin2uf2.c` builds the UF2 image.
- `chorus rp2040` and `chorus defconfig-rp2040` targets, `ARCH_ARM_CORTEX_M0PLUS` Kconfig option.
- CI job that builds the RP2040 port, runs the tests in Renode and uploads the example UF2.
- `boards/stm32f103c8-bluepill`: STM32F103C8 at 72 MHz from HSE, USART1 on PA9/PA10, linker script, defconfig and Renode script. Reuses the Cortex-M3 context switch and SysTick timer. Tested in Renode, not on hardware.
- `chorus stm32f103` and `chorus defconfig-stm32f103` targets.
- CI job that builds the STM32F103 port, runs the tests in Renode and uploads the example binary.

### Changed
- `br_hal_board_init()` is called by the startup code before `main()` instead of by `br_kernel_init()`. Board code can now set up clocks and the UART before the first output.
- The LM3S6965 UART driver moved from `arch/arm-cortex-m/` to `boards/qemu-cortex-m3/`. `arch/` holds core code, `boards/` holds chip and board code.

## [0.2.1] - 2026-10-04

### Fixed
- Cortex-M: SysTick ran with a fixed period of about 1 s, and alarms and round-robin were only checked on that interrupt. Sleeps and timeouts fired up to 1 s late and round-robin barely switched tasks. SysTick is now reprogrammed for the nearest alarm or time slice. The scheduler, semaphore timeout and sleep list tests now pass on QEMU.

### Added
- CI runs the Cortex-M tests on QEMU.

## [0.2.0] - 2026-10-04

### Added
- RISC-V port in `arch/riscv32` for RV32IMAC in machine mode: CLINT timer, context switch through a common trap handler and the CLINT software interrupt, `mstatus.MIE` interrupt control, 16550 UART.
- `boards/qemu-riscv32-virt` with linker script and defconfig for QEMU `virt`.
- `chorus riscv32`, `chorus run-riscv32`, `chorus test-*-riscv32` and `chorus defconfig-riscv32` targets.
- `ARCH_RISCV32` Kconfig option.
- CI job that builds the RISC-V port and runs the tests on QEMU.
- `lib/br_string.c` with `memcpy` and `memset`.

### Changed
- The kernel, the Cortex-M and RISC-V HALs and `lib` no longer depend on libc. Targets are built with `-ffreestanding` and linked with `-lgcc` only. newlib is no longer needed.

## [0.1.0] - 2026-10-04

### Added
- `BR_STABLE`, `BR_EXPERIMENTAL` and `BR_INTERNAL` annotations in the public API. `BR_WARN_EXPERIMENTAL` and `BR_WARN_INTERNAL` turn the last two into compiler warnings.
- `include/bedrock/br_version.h` with `BR_VERSION_MAJOR`, `BR_VERSION_MINOR`, `BR_VERSION_PATCH` and `BR_VERSION`.
- Host HAL for x86-64 Linux in `arch/host-x86-64`. The kernel builds and runs as a Linux process with `chorus host`.
- Unit tests for tasks, scheduler, semaphore, mutex, message queue, sleep list and memory pool, for Cortex-M under QEMU and for the host.
- Kconfig support through kconfig-tools: `chorus build-tools`, `chorus defconfig`, `chorus menuconfig`.
- GitHub Actions workflow: Cortex-M build and host test run.
- Double-free check in `br_pool_free()`.

### Changed
- **Breaking:** `BEDROCK_VERSION_MAJOR`, `BEDROCK_VERSION_MINOR`, `BEDROCK_VERSION_PATCH` and `BEDROCK_VERSION` are renamed to `BR_VERSION_MAJOR`, `BR_VERSION_MINOR`, `BR_VERSION_PATCH` and `BR_VERSION`.
- kconfig-tools moved to a separate repository and is built with Zig.
- Source file headers are reduced to the SPDX identifier.

### Fixed
- Priority inheritance did not move a boosted mutex owner to its new ready queue, so the boost had no effect on scheduling.
- Tasks in a timed wait on a semaphore, mutex or queue shared one `next` field between the wait queue and the sleep list, which corrupted one of the lists.
- Timeouts on semaphores, mutexes and queues could fire late because the hardware alarm was not reprogrammed after a timed wait was added.
- `br_pool_free()` called twice on the same block made two later allocations return the same block.
- On the host HAL, a task switched in from the SIGALRM handler could see the ISR flag set and get `BR_ERR_ISR` from IPC calls.

[Unreleased]: https://github.com/bedrockRTOS/bedrock-core/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/bedrockRTOS/bedrock-core/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/bedrockRTOS/bedrock-core/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/bedrockRTOS/bedrock-core/compare/v0.0.3...v0.1.0

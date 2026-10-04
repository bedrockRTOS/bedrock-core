# Roadmap

## 0.1.0: Kernel API and tests

- [x] Split the public API in `include/bedrock/bedrock.h` into stable and experimental parts
- [x] Add a mock HAL for x86-64 Linux so the kernel builds and runs as a regular process
- [x] Unit tests:
  - [x] Tasks: create, suspend, resume, delete
  - [x] Scheduler: priority order, round-robin, preemption
  - [x] Semaphore: take and give, timeout, overflow
  - [x] Mutex: lock and unlock, priority inheritance, timeout, calls from ISR
  - [x] Message queue: send and receive, blocking on full and empty, timeout
  - [x] Sleep list: ordering, wakeup from the alarm handler
  - [x] Memory pool: alloc, free, exhaustion, double free
- [x] GitHub Actions: Cortex-M build and host test run
- [x] `br_version.h` with `BR_VERSION_MAJOR`, `BR_VERSION_MINOR`, `BR_VERSION_PATCH`

## 0.2.0: RISC-V port

- [x] `arch/riscv32/` with the same layout as `arch/arm-cortex-m/`
- [x] Timer HAL on CLINT `mtime`/`mtimecmp`
- [x] RV32I context switch in `br_hal_context.c`: stack frame layout, switching on trap exit via the CLINT software interrupt
- [x] `br_hal_irq_disable`/`br_hal_irq_restore` on `mstatus.MIE`
- [x] `boards/qemu-riscv32-virt/`: linker script and startup code for the QEMU `virt` machine
- [x] UART output for QEMU `virt` (16550)
- [x] `examples/main.c` runs on both architectures without changes
- [x] RISC-V target in `chorus.build`
- [x] `ARCH_RISCV32` in Kconfig
- [x] Build and test both architectures in CI
- [x] Porting guide update

## Extra boards

- [x] RP2040: `arch/arm-cortex-m0plus/` with ARMv6-M PendSV/SVC, RP2040 TIMER alarm, PL011 UART
- [x] RP2040: `boards/rpi-pico/` with linker script, own boot2, defconfig
- [x] RP2040: ELF to UF2 and boot2 CRC tools in `tools/`
- [x] RP2040: tests in Renode in CI
- [ ] RP2040: check on a real Raspberry Pi Pico
- [x] STM32F103C8: `boards/stm32f103c8-bluepill/` with clocks, USART1, linker script, defconfig
- [x] STM32F103C8: tests in Renode in CI
- [ ] STM32F103C8: check on a real Blue Pill

## 0.3.0: Shell and debugging

- [ ] UART RX on interrupts instead of the polled TX-only driver
- [ ] Shell core: line editing, command parsing, command registration
- [ ] Shell commands:
  - [ ] `tasks`: tasks with state, priority and stack usage
  - [ ] `mem`: memory pool usage
  - [ ] `uptime`: time since boot
  - [ ] `version`: kernel version
  - [ ] `reboot`: system reset
- [ ] `br_console` API for formatted output (`br_printf` or a smaller equivalent)
- [ ] Stack high-water mark: fill the stack with a pattern and scan it
- [ ] Per-task CPU usage under `CONFIG_TASK_STATS`
- [ ] `br_hook_idle()` for user code in the idle task, for example power management

## 0.4.0: Drivers

- [ ] Driver model:
  - `br_device_t` with a name, an ops table and private data
  - `br_device_register()` and `br_device_find()`
  - Ops: `open`, `close`, `read`, `write`, `ioctl`
- [ ] GPIO HAL and driver
- [ ] SPI HAL and driver
- [ ] I2C HAL and driver
- [ ] Interrupt-driven UART on the driver model
- [ ] GPIO and UART implementations for Cortex-M and RISC-V
- [ ] Timer/counter driver for user code: PWM, capture
- [ ] Driver guide in `docs/en/driver-guide.md`

## 0.5.0: Filesystem (optional module)

- [ ] Block device HAL `br_blkdev_t`: read, write, erase, sync
- [ ] littlefs in `3rd/lib/littlefs/` or a small FAT reader
- [ ] Thin VFS layer: `br_fs_mount()`, `br_fs_open()`, `br_fs_read()`, `br_fs_write()`, `br_fs_close()`
- [ ] RAM block device for tests under QEMU
- [ ] Shell commands: `ls`, `cat`, `write`
- [ ] Filesystem unit tests

## 0.6.0: Networking (optional module)

- [ ] Network interface HAL `br_netif_t`: send, receive, link status
- [ ] lwIP, picoTCP or a small UDP/IP stack of our own
- [ ] Dedicated task that runs the network stack
- [ ] Socket-style API: `br_net_socket()`, `br_net_bind()`, `br_net_send()`, `br_net_recv()`
- [ ] Loopback interface for tests without hardware
- [ ] QEMU network backend: `-netdev user` with SLIP or virtio-net
- [ ] Example: UDP echo server
- [ ] Shell commands: `ifconfig`, `ping` (if ICMP is supported)

## 0.7.0: More architectures and boards

- [ ] Third architecture, one of:
  - AVR (ATmega328P, Arduino Uno): 8-bit
  - x86 (i386 or x86_64 bare metal)
  - Xtensa (ESP32)
- [ ] At least one real board per architecture:
  - ARM: STM32F4-Discovery or similar
  - RISC-V: Sipeed Longan Nano (GD32VF103) or similar
- [ ] Board packages in `boards/<board>/`:
  - Linker script
  - Clock and pin setup
  - `br_hal_board_init()`
- [ ] Run all examples and tests on the new targets
- [ ] Porting guide section on adding a board

## 0.8.0: Memory protection

- [ ] MPU HAL
- [ ] Per-task MPU regions on Cortex-M: stack, peripherals
- [ ] PMP on RISC-V
- [ ] `CONFIG_MPU`: tasks cannot write to other tasks' stacks
- [ ] Kernel runs privileged, tasks run unprivileged (Cortex-M Handler/Thread mode)
- [ ] System calls (`svc`/`ecall`) for unprivileged tasks
- [ ] Kernel code audit:
  - Buffer overflows
  - Integer overflows in size math
  - Unchecked pointer dereferences
- [ ] Fault handlers with diagnostic output: HardFault on Cortex-M, trap handler on RISC-V

## 0.9.0: API freeze, docs, performance

- [ ] Freeze `include/bedrock/bedrock.h`: no breaking changes after this release
- [ ] API reference for every public function
- [ ] Getting started guide: from an empty project to a blinking LED
- [ ] Architecture decision records for the main design choices
- [ ] Benchmarks:
  - Context switch latency in cycles
  - Interrupt latency
  - Message queue send/receive per second
  - Kernel size: `.text`, `.data`, `.bss`
- [ ] Optimize the hot paths found by the benchmarks
- [ ] Benchmark results in `docs/en/benchmarks.md`
- [ ] Static analysis: cppcheck, Coverity Scan or similar
- [ ] Fix all warnings and analyzer findings
- [ ] Review all EN and RU docs
- [ ] Release notes template

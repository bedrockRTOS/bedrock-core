# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/). Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- AVR port for the ATmega328P (Arduino Uno) in `arch/avr/` and `boards/arduino-uno/`: context switch, interrupts on a separate stack (`BR_AVR_KERNEL_ISR`), Timer1 as the tickless time source, USART0, GPIO, SPI, I2C and PWM. Runs 13 tests in QEMU (`chorus avr`, `chorus test-<name>-avr`), CI job `avr`. See `docs/en/avr.md`.
- `examples/test_stack.h`: `TEST_STACK(n)` scales the stacks of the tests down on an AVR.
- AVR defaults in `br_config.h` (`CONFIG_MAX_TASKS`, idle stack, buffers, `CONFIG_TASK_STATS` off) and `BR_STACK_MIN` of 64 bytes.
- IPv6 in `ports/lwip`: dual stack with a link-local address from the MAC address, SLAAC from router advertisements, static addresses (`br_net_ip6_add()`, `br_net_wait_ip6()`), ICMPv6 ping, TCP and UDP over IPv6 sockets, AAAA lookups and IPv6 DNS servers. `br_ip_t` (`include/bedrock/br_ip.h`) holds an address of either family, with `br_ip_parse()`, `br_ip_format()` (RFC 5952), `br_ip_equal()`, `br_ip_is_any()` and `br_ip_any()`. Link-local addresses carry the interface in `scope`. The shell commands `ifconfig`, `ping` and `nslookup` handle IPv6 (`nslookup <name> [4|6]`). Tests in `test_net` and `test_net_qemu`.
- System calls for the device model, file system and network: `br_user_device_*`, `br_user_fs_*`, `br_user_net_*`. Modules register their own call numbers with `br_syscall_register()` and enable them with `br_device_enable_user_calls()`, `br_fs_enable_user_calls()` and `br_net_enable_user_calls()`. Devices are open to unprivileged tasks only after `br_device_set_user()`, `ioctl` goes through a whitelist (`br_device_user_allow_ioctl()`), descriptors belong to their task and are closed when it is deleted (`br_user_release_register()`). Tests in `test_mpu`.
- Interrupt-driven UART receive on all targets: `br_uart_getc(char *c, br_time_t timeout)` reads from a ring buffer that the UART RX interrupt fills (`kernel/br_uart.c`). `br_uart_putc`, `br_uart_puts` and `br_uart_getc` are declared in `bedrock.h` as experimental API.
- `CONFIG_UART_RX_BUF_SIZE` (default 64).
- Cortex-M3 vector table has weak `IRQ0_Handler` to `IRQ63_Handler` that boards override.
- RISC-V: machine external interrupts through the QEMU `virt` PLIC.
- `test_uart_rx` on every target, run in CI with input fed to the console.
- `boards/qemu-cortex-m3/board.c` and `boards/qemu-riscv32-virt/board.c`.
- Shell core in `lib/br_shell.c`: `br_shell_register`, `br_shell_readline` (echo, backspace, CRLF), `br_shell_execute`, `br_shell_run` and a built-in `help` command. `CONFIG_SHELL_LINE_MAX` (default 64) and `CONFIG_SHELL_MAX_ARGS` (default 8).
- `test_shell` on every target.
- `br_printf` and `br_snprintf` in `lib/br_console.c` with `%d %i %u %x %X %c %s %p %%`, `-` and `0` flags, width and `l`, `ll`, `z` length modifiers.
- `br_task_get_info()` returns name, state, priority, stack size, stack high-water mark and CPU time of a task. New tasks get their stack filled with `BR_STACK_FILL` (`0xA5`) to measure the mark.
- `CONFIG_TASK_STATS` (default on): per-task CPU time accounting in the scheduler.
- `br_hook_idle()`: weak function called in a loop by the idle task.
- `br_hal_reset()` on every target.
- Shell commands `tasks`, `mem`, `uptime`, `version` and `reboot`, registered with `br_shell_register_builtin()` (`lib/br_shell_cmds.c`).
- `br_pool_count()`, `br_pool_get()` and `br_pool_block_size()` to enumerate memory pools.
- `test_console`, `test_task_info` and `test_shell_cmds` on every target.
- Device model: `br_device_t`, `br_device_register()`, `br_device_find()`, `br_device_open()`, `br_device_close()`, `br_device_read()`, `br_device_write()` and `br_device_ioctl()` (`kernel/br_device.c`).
- Drivers in `drivers/`: UART, GPIO, SPI, I2C, PWM and input capture on the device model.
- HAL functions `br_hal_gpio_*`, `br_hal_spi_*`, `br_hal_i2c_*`, `br_hal_pwm_*` and `br_hal_capture_*`. Implemented for LM3S6965, STM32F103C8 and RP2040 from the reference manuals, as a model on the host, and as `BR_ERR_INVALID` stubs for RISC-V `virt`, which has none of these devices.
- `docs/en/driver-guide.md`.
- `test_device`, `test_gpio`, `test_spi`, `test_i2c` and `test_pwm` on every target.
- Block device interface `br_blkdev_t` (`include/bedrock/br_blkdev.h`) and a RAM block device with flash semantics and power loss injection (`lib/br_ramdisk.c`).
- Filesystem in `lib/br_fs.c`: flat namespace, power-loss safe commit, `br_fs_format`, `br_fs_mount`, `br_fs_unmount`, `br_fs_open`, `br_fs_read`, `br_fs_write`, `br_fs_close`, `br_fs_remove`, `br_fs_stat`, `br_fs_list` and `br_fs_usage`. See `docs/en/filesystem.md`.
- Error codes `BR_ERR_NOENT` and `BR_ERR_IO`.
- `CONFIG_FS_NAME_MAX`, `CONFIG_FS_BLOCK_MAX`, `CONFIG_FS_MAX_BLOCKS` and `CONFIG_FS_MAX_OPEN`.
- Shell commands `ls`, `cat` and `write`, registered with `br_shell_register_fs()`.
- `test_fs` and `test_shell_fs` on every target.
- Optional ports in `ports/`: littlefs v2.11.3 (`ports/littlefs/`, submodule) and FatFs R0.16 (`ports/fatfs/`) on `br_blkdev_t`, without dynamic allocation and with bedrock mutexes. See `docs/en/ports.md`.
- `test_littlefs` on every target, `test_fatfs` on the host, RISC-V and RP2040.
- Network interface contract `br_netif_t` (`include/bedrock/br_netif.h`), `br_hal_netif_default()` on every target and a software interface for tests (`drivers/br_netif_mock.c`).
- Network interface drivers: Stellaris Ethernet MAC for LM3S6965 and virtio-net for RISC-V `virt`, both tested in QEMU.
- lwIP 2.2.1 as an optional port in `ports/lwip/` with socket-style API `br_net_*` (UDP, TCP, loopback, DHCP, ping), the system layer on kernel primitives and the shell commands `ifconfig` and `ping`. See `docs/en/networking.md`.
- Example `examples/net_udp_echo.c`, `chorus run-udp-echo` and `chorus run-udp-echo-riscv32`.
- `test_net` on the host, LM3S6965, RISC-V and RP2040, `test_net_qemu` on LM3S6965 and RISC-V.
- x86 port in `arch/x86` and `boards/qemu-x86`: 32-bit protected mode, multiboot, PIT one-shot alarms over a TSC clock, 16550 UART, context switch on interrupt exit. `chorus x86` and `chorus run-x86`. The peripheral HAL returns `BR_ERR_INVALID` and there is no network interface.
- STM32F4-Discovery board in `boards/stm32f4-discovery` (STM32F407, Cortex-M4 without the FPU, 16 MHz internal clock): USART1 console, GPIO, SPI1, I2C1, TIM2 PWM and TIM3 capture. `chorus stm32f4`, `chorus run-stm32f4`, `chorus defconfig-stm32f4`.
- Porting guide section on adding a board.
- DNS in the network port: `br_net_resolve()`, `br_net_set_dns()`, `br_net_dns()` and the shell command `nslookup`. DHCP sets the DNS server. `ports/br_libc.c` gained `strlen` and `strcmp`.
- Filesystem: `br_fs_seek()` for read descriptors, `BR_FS_APPEND`, directories (`br_fs_mkdir`, `br_fs_rmdir`, `br_fs_info`, `br_fs_readdir`) and `br_fs_info_t::dir`. Shell commands `mkdir`, `rm` and `ls [dir]`.
- Interrupt-driven network receive on the Stellaris Ethernet MAC and virtio-net: `br_netif_t::irq` and `rx_notify`, `br_net_rx_wakeups()`. The receive task sleeps on a semaphore instead of polling every 5 ms. Interfaces without an interrupt are still polled.
- Memory protection, `CONFIG_MPU`: unprivileged tasks (`br_task_create_user()`), per-task MPU regions on Cortex-M, PMP on RISC-V and paging on x86, grants (`br_task_grant()`), and system calls for task, time, semaphore, mutex, message queue and UART (`br_user_*`). Faulting tasks are stopped with a report. See `docs/en/memory-protection.md`. Run on QEMU for Cortex-M3, Cortex-M4, RISC-V and x86. Cortex-M0+ builds but has not been run.
- Fault handlers with diagnostic output for HardFault, MemManage, BusFault and UsageFault on Cortex-M and for traps on RISC-V and x86.
- Linker scripts of LM3S6965, STM32F4, RP2040, RISC-V `virt` and x86 reserve a 4 KiB section for shared user data.
- `test_mpu` for Cortex-M3, Cortex-M4, RISC-V, x86 and RP2040 (build only) and `test_audit`.

### Fixed
- `br_mqueue_init` accepted dimensions whose product overflows `size_t`, which made the queue indices wrap.
- `br_task_create` accepted a stack that is too small for the initial frame, which wrote below the stack.
- `br_shell_readline` wrote to the buffer when the size was 0.
- A sleep or timeout close to `BR_TIME_INFINITE` wrapped around and woke the task at once. The wake time now saturates. The RISC-V timer clamps the alarm so that the compare value does not overflow.
- `BR_ERR_PERM` for an access that a task is not allowed to make, and `BR_STACK_MIN` for the smallest task stack.

### Changed
- Tests delete the tasks they are done with and `test_scheduler` shares its stacks between the phases.
- `br_printf` and `br_snprintf` format numbers above 32 bits without the 64-bit division routine of the compiler, and 32-bit numbers with 32-bit division.
- **BREAKING CHANGE:** the network API takes `br_ip_t` instead of an IPv4 `uint32_t`. `br_net_socket(family, type)` takes the family, `br_net_bind`, `br_net_connect`, `br_net_sendto`, `br_net_ping`, `br_net_set_dns` take `const br_ip_t *` (`NULL` means any address for `br_net_bind`), `br_net_accept`, `br_net_recvfrom` and `br_net_dns` return it through a pointer, `br_net_resolve(name, family, &ip)` takes a family, and `br_net_netif_add` takes pointers (`NULL` for DHCP). `BR_IP4(a, b, c, d)` is now a `br_ip_t` and `br_net_ifinfo_t` holds `br_ip_t` fields. The system calls `br_user_net_*` changed the same way. To port IPv4 code: `br_net_socket(BR_AF_INET, type)` and `&BR_IP4(...)`.
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

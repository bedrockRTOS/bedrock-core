# bedrock[RTOS] Roadmap

## Phase 1 — Bring the Kernel to Life (0.0.x)

### 0.0.2 — First Verified Boot

- [x] Create linker script for `boards/qemu-cortex-m3/`
- [x] Fix library link order in `chorus.build` (HAL before kernel to resolve symbols correctly)
- [x] Implement `SVC_Handler` for first task start (proper handler→thread mode transition with PSP)
- [x] Add `chorus run` target to launch ELF on QEMU

### 0.0.3 — Kernel Hardening

- [ ] Implement round-robin time-slice logic (currently `rr_remaining` field exists but is unused)
- [ ] Add stack overflow detection: canary value at stack bottom, checked on context switch
- [ ] Add `br_task_delete()` — return a TCB slot to the pool (currently tasks can only be suspended)
- [ ] Add `br_assert()` macro with configurable panic handler (`BR_PANIC()`)
- [ ] Implement `br_hal_panic()` in Cortex-M HAL (print fault info via UART, halt)
- [ ] Add ISR guards to semaphore and message queue operations (mutex already has them)

## Phase 2 — Stable Foundation (0.1.0)

### 0.1.0 — Stable Kernel API & Test Infrastructure

- [ ] Define the public API contract in `include/bedrock/bedrock.h` — mark stable vs. experimental
- [ ] Create a host-native test harness: mock HAL for x86-64 Linux so kernel code compiles and runs as a Linux process
- [ ] Write unit tests for:
  - [ ] Task creation, suspension, resumption, deletion
  - [ ] Scheduler: priority ordering, round-robin, preemption
  - [ ] Semaphore: basic take/give, timeout, overflow
  - [ ] Mutex: lock/unlock, priority inheritance, timeout, ISR rejection
  - [ ] Message queue: send/recv, full/empty blocking, timeout
  - [ ] Sleep list: correct ordering, alarm handler wakeup
  - [ ] Memory pool: alloc, free, exhaustion, double-free guard
- [ ] Add CI pipeline (GitHub Actions): build for Cortex-M + run host-native tests
- [ ] Write `br_version.h` with `BR_VERSION_MAJOR`, `BR_VERSION_MINOR`, `BR_VERSION_PATCH` macros
- [ ] Formalize coding style and document it in `docs/en/contributing.md`
- [ ] Add `CHANGELOG.md`

## Phase 3 — Portability Proven (0.2.0)

### 0.2.0 — RISC-V Port

- [ ] Create `arch/riscv32/` directory structure mirroring `arch/arm-cortex-m/`
- [ ] Implement `br_hal_timer.h` for RISC-V (using `mtime`/`mtimecmp` on CLINT)
- [ ] Implement `br_hal_context.c` for RV32I — stack frame layout, context switch via `mscratch`
- [ ] Implement `br_hal_irq_disable/restore` using `mstatus.MIE`
- [ ] Create `boards/qemu-riscv32-virt/` with linker script and startup code for QEMU `virt` machine
- [ ] Implement minimal UART output for QEMU `virt` (16550-compatible)
- [ ] Verify the same `examples/main.c` boots and runs on both architectures without changes
- [ ] Add RISC-V build target to `chorus.build`
- [ ] Add `ARCH_RISCV32` option to Kconfig
- [ ] Update CI to build and test both architectures
- [ ] Update porting guide documentation

## Phase 4 — Developer Experience (0.3.0)

### 0.3.0 — Shell, Debug Console, Introspection

- [ ] Implement interrupt-driven UART RX (replace polled TX-only driver)
- [ ] Design a minimal shell framework: line editing, command parsing, command registration
- [ ] Implement built-in shell commands:
  - [ ] `tasks` — list all tasks with state, priority, stack usage
  - [ ] `mem` — show memory pool usage
  - [ ] `uptime` — display system uptime
  - [ ] `version` — print kernel version
  - [ ] `reboot` — trigger system reset
- [ ] Add a `br_console` API for formatted output (`br_printf` or lightweight equivalent)
- [ ] Add stack high-water-mark tracking (paint stack with pattern, scan for usage)
- [ ] Add runtime statistics: per-task CPU usage (optional, behind `CONFIG_TASK_STATS`)
- [ ] Implement `br_hook_idle()` — user-defined idle hook for power management

## Phase 5 — Driver Model (0.4.0)

### 0.4.0 — Peripheral Driver Framework

- [ ] Design a lightweight driver model:
  - Device descriptor struct (`br_device_t`) with name, ops table, private data
  - `br_device_register()` / `br_device_find()` API
  - Operations: `open`, `close`, `read`, `write`, `ioctl`
- [ ] Implement GPIO HAL interface and driver
- [ ] Implement SPI HAL interface and driver
- [ ] Implement I2C HAL interface and driver
- [ ] Implement interrupt-driven UART as a proper driver (using the new driver model)
- [ ] Provide Cortex-M and RISC-V reference implementations for at least GPIO and UART
- [ ] Add timer/counter driver (beyond the kernel timer — for user-facing PWM, capture, etc.)
- [ ] Document driver authoring guide in `docs/en/driver-guide.md`

## Phase 6 — Storage (0.5.0)

### 0.5.0 — Lightweight Filesystem (Optional Module)

- [ ] Define a block device HAL interface (`br_blkdev_t`: read, write, erase, sync)
- [ ] Integrate littlefs as a third-party library (in `3rd/lib/littlefs/`) or implement a minimal FAT reader
- [ ] Create a VFS-like thin layer: `br_fs_mount()`, `br_fs_open()`, `br_fs_read()`, `br_fs_write()`, `br_fs_close()`
- [ ] Implement a RAM-backed block device for testing on QEMU
- [ ] Add shell commands: `ls`, `cat`, `write` (for testing)
- [ ] Write unit tests for filesystem operations

## Phase 7 — Networking (0.6.0)

### 0.6.0 — Lightweight IP Networking (Optional Module)

- [ ] Define a network interface HAL (`br_netif_t`: send, recv, link status)
- [ ] Integrate lwIP or picoTCP as a third-party library, or implement a minimal UDP/IP stack
- [ ] Create a network task that drives the stack from a dedicated thread
- [ ] Implement socket-like API: `br_net_socket()`, `br_net_bind()`, `br_net_send()`, `br_net_recv()`
- [ ] Provide a loopback interface for testing without hardware
- [ ] Implement a QEMU network backend (e.g., `-netdev user` with SLIP or virtio-net)
- [ ] Add an example: simple UDP echo server
- [ ] Shell commands: `ifconfig`, `ping` (if ICMP is supported)

## Phase 8 — Architecture Breadth (0.7.0)

### 0.7.0 — Third Architecture + Expanded Board Support

- [ ] Port to a third architecture — candidates (pick one based on community interest):
  - AVR (ATmega328P — Arduino Uno) — 8-bit, tests the lower end
  - x86 (i386 or x86_64 bare-metal) — tests the upper end
  - Xtensa (ESP32) — tests a real-world commercial target
- [ ] Add at least one real hardware board target per existing architecture:
  - ARM: STM32F4-Discovery or similar (real silicon, not just QEMU)
  - RISC-V: Sipeed Longan Nano (GD32VF103) or similar
- [ ] Create board support packages (`boards/<board>/`) with:
  - Linker script
  - Clock and pin configuration
  - Board-specific `br_hal_board_init()`
- [ ] Validate all existing examples and tests on new targets
- [ ] Document adding a new board in porting guide

## Phase 9 — Security & Isolation (0.8.0)

### 0.8.0 — MPU Support and Memory Protection

- [ ] Implement MPU (Memory Protection Unit) HAL interface
- [ ] Configure per-task MPU regions on Cortex-M (stack, peripheral access)
- [ ] Implement PMP (Physical Memory Protection) support on RISC-V
- [ ] Add `CONFIG_MPU` option — when enabled, tasks cannot corrupt each other's stacks
- [ ] Implement privilege levels: kernel code runs privileged, tasks run unprivileged (Cortex-M: Handler/Thread mode)
- [ ] Add system call interface (`svc`/`ecall`) for unprivileged task access to kernel services
- [ ] Audit all kernel code for:
  - Buffer overflows
  - Integer overflows in size calculations
  - Unchecked pointer dereferences
- [ ] Add fault handlers: HardFault (Cortex-M), trap handler (RISC-V) with diagnostic output

## Phase 10 — Release Preparation (0.9.0)

### 0.9.0 — API Freeze, Docs, Performance

- [ ] **API freeze** — no breaking changes to `include/bedrock/bedrock.h` after this version
- [ ] Complete API reference documentation for every public function
- [ ] Write a getting-started tutorial: from zero to blinking LED
- [ ] Write architecture decision records (ADRs) for key design choices
- [ ] Performance benchmarking:
  - Context switch latency (cycles)
  - Interrupt latency
  - IPC throughput (message queue send/recv per second)
  - Memory footprint (kernel .text + .data + .bss)
- [ ] Optimize critical paths identified by benchmarks
- [ ] Publish benchmark results in `docs/en/benchmarks.md`
- [ ] Run static analysis (cppcheck, Coverity scan, or similar)
- [ ] Resolve all warnings and static analysis findings
- [ ] Final review of all documentation (EN and RU)
- [ ] Prepare release notes template

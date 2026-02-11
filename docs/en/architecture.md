<!--
  Project: bedrock[RTOS]
  Version: 0.0.1
  Author:  AnmiTaliDev <anmitalidev@nuros.org>
  License: CC BY-SA 4.0
-->

# Architecture

## Overview

bedrock[RTOS] is a nanokernel real-time operating system. It is a platform, not a library — the kernel owns the boot process, the scheduler, and the hardware abstraction layer. Application code runs as tasks managed by the kernel.

## Directory Structure

```
bedrock-core/
├── kernel/              Scheduler, task management, time, IPC
├── include/bedrock/     Public API headers
├── arch/                HAL implementations per architecture
│   └── arm-cortex-m/    ARM Cortex-M3 reference port
├── boards/              Board-specific linker scripts and configs
│   └── qemu-cortex-m3/
├── lib/                 Static pool allocator, utilities
├── examples/            Application templates
├── docs/                Documentation (EN / RU)
├── 3rd/tools/chorus     Build system (git submodule)
├── Kconfig              Kernel configuration
└── chorus.build         Build configuration
```

## Kernel Components

### Scheduler (`kernel/br_sched.c`)

Priority-based preemptive scheduler with round-robin at equal priority levels.

- `CONFIG_NUM_PRIORITIES` separate ready queues (default 8), priority 0 is highest
- Tasks at the same priority are scheduled round-robin (FIFO within queue)
- Scheduler lock/unlock mechanism for nested critical sections
- Context switch triggered via `br_hal_context_switch()` — on Cortex-M this pends PendSV

### Task Management (`kernel/br_task.c`)

Static TCB pool — no dynamic allocation.

- Up to `CONFIG_MAX_TASKS` tasks (default 16)
- Each task has: ID, name, priority, stack, entry point, state
- Task states: `INACTIVE`, `READY`, `RUNNING`, `BLOCKED`, `SUSPENDED`
- Idle task is created automatically at lowest priority during `br_kernel_init()`

### Time Services (`kernel/br_time.c`)

Tickless design — no periodic timer interrupts.

- 64-bit microsecond timestamps (`br_time_t` = `uint64_t`, ~584,000 years range)
- Sleep list sorted by wake time (ascending)
- Hardware alarm reprogrammed only when needed
- `br_time_alarm_handler()` wakes expired tasks and reschedules

### IPC (`kernel/br_ipc.c`)

Three synchronization primitives:

- **Semaphore** — counting semaphore with configurable max count and wait queue
- **Mutex** — binary lock with priority inheritance to prevent priority inversion
- **Message Queue** — fixed-size ring buffer with separate send/receive wait queues

All wait queues are priority-ordered.

## Hardware Abstraction Layer

The kernel never accesses hardware directly. All platform interaction goes through the HAL interface defined in `include/bedrock/br_hal.h`.

### HAL Functions

| Category | Functions |
|----------|-----------|
| Timer | `br_hal_timer_init`, `br_hal_timer_get_us`, `br_hal_timer_set_alarm`, `br_hal_timer_cancel_alarm` |
| Interrupts | `br_hal_irq_disable`, `br_hal_irq_restore`, `br_hal_in_isr` |
| Context | `br_hal_stack_init`, `br_hal_context_switch`, `br_hal_start_first_task` |
| Board | `br_hal_board_init` |

### Porting

Adding a new architecture requires implementing these functions in `arch/<arch_name>/`. Zero kernel changes needed.

## Memory Model

- Zero `malloc` in kernel — all structures are statically allocated
- TCB pool: fixed array of `CONFIG_MAX_TASKS` entries
- Stacks: caller-provided buffers passed to `br_task_create()`
- IPC objects: caller declares them as variables (stack or global)
- `/lib` pool allocator: optional, for application-level fixed-size block allocation

## Boot Sequence

1. `Reset_Handler` — copies `.data`, zeros `.bss`, calls `main()`
2. `main()` calls `br_kernel_init()`:
   - Zeros TCB pool
   - Calls `br_hal_board_init()` and `br_hal_timer_init()`
   - Initializes scheduler
   - Creates idle task
3. Application creates tasks via `br_task_create()`
4. `br_kernel_start()` — picks the highest-priority ready task and starts it (never returns)

## Context Switch (ARM Cortex-M)

- `br_hal_stack_init()` builds an initial exception frame: xPSR, PC, LR, R12, R3-R0 (hardware frame) + R4-R11 (software-saved)
- `br_hal_context_switch()` pends PendSV
- `PendSV_Handler` saves/restores R4-R11 and swaps PSP between tasks
- `br_hal_start_first_task()` sets PSP and switches to thread mode

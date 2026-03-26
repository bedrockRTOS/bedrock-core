# bedrock[RTOS]

**Solid Foundation** — A nanokernel real-time operating system written in pure C (C11/C17).

> *When the foundation is solid, you're free to create.*

## Philosophy

bedrock[RTOS] is designed around three core principles:

1. **Zero dynamic allocation** — The kernel never calls `malloc`. All objects (tasks, semaphores, mutexes, queues) come from statically-sized pools determined at compile time.
2. **Tickless by design** — No periodic timer interrupts. The kernel programs one-shot hardware alarms only when needed, minimizing power consumption and interrupt overhead.
3. **Clean separation** — Hardware details are strictly isolated in `/arch` and `/boards`. The kernel source code has zero platform-specific code. Adding a new architecture requires zero kernel changes.

## HAL Interface

The kernel accesses hardware exclusively through three timer functions:

```c
void      br_hal_timer_init(void);
br_time_t br_hal_timer_get_us(void);
void      br_hal_timer_set_alarm(br_time_t abs_us);
```

Plus interrupt control and context switch primitives defined in `include/bedrock/br_hal.h`.

## Quick Start

### Prerequisites

- `arm-none-eabi-gcc` toolchain
- `qemu-system-arm` (for Cortex-M3 emulation)
- [chorus](https://github.com/z3nnix/chorus) build system (included as `3rd/tools/chorus`)

### Build

```bash
chorus
```

### Run on QEMU

```bash
qemu-system-arm -M lm3s6965evb -nographic -kernel bedrock_example.elf
```

### Clean

```bash
chorus clean
```

### Configuration

Edit `Kconfig` values or pass them as `-D` flags in `chorus.build` CFLAGS.

## Multi-License Model

bedrock[RTOS] uses a per-directory licensing approach to maximize flexibility:

- **`/kernel`, `/arch`, `/boards`, `/include`** — GNU GPL 3.0 (Runtime exception)
- **`/lib`** — GNU GPL 3.0 (Runtime exception)
- **`/examples`** — GNU GPL 3.0 (Runtime exception)
- **`/docs`** — CC BY-SA 4.0

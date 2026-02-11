<!--
  Project: bedrock[RTOS]
  Version: 0.0.1
  Author:  AnmiTaliDev <anmitalidev@nuros.org>
  License: CC BY-SA 4.0
-->

# Building

bedrock[RTOS] uses [chorus](https://github.com/z3nnix/chorus) as its build system. Chorus is included as a git submodule at `3rd/tools/chorus`.

## Prerequisites

- `arm-none-eabi-gcc` toolchain
- `qemu-system-arm` (for running on QEMU)
- Go compiler (to build chorus, if not using a prebuilt binary)

### Installing on Arch Linux

```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib qemu-system-arm
```

### Building Chorus

```bash
cd 3rd/tools/chorus
go build -o chorus .
sudo cp chorus /usr/local/bin/
```

## Build

From the project root:

```bash
chorus
```

This produces:

| Artifact | Description |
|----------|-------------|
| `libbedrock_kernel.a` | Kernel: scheduler, tasks, time, IPC |
| `libbedrock_hal.a` | HAL: timer, context switch, UART, startup |
| `libbedrock_lib.a` | Library: static pool allocator |
| `bedrock_example.elf` | Example application linked with all libraries |

## Clean

```bash
chorus clean
```

Removes all `*.o`, `*.a`, `*.elf`, `*.bin` files.

## Running on QEMU

```bash
qemu-system-arm -M lm3s6965evb -nographic -kernel bedrock_example.elf
```

Press `Ctrl+A`, then `X` to exit QEMU.

## Build Configuration

The `chorus.build` file defines:

### Variables

| Variable | Description |
|----------|-------------|
| `CC` | C compiler (`arm-none-eabi-gcc`) |
| `AR` | Archiver (`arm-none-eabi-ar`) |
| `OBJCOPY` | Object copy tool (`arm-none-eabi-objcopy`) |
| `CFLAGS` | Compiler flags (C11, Cortex-M3, thumb, includes, config defines) |
| `LDFLAGS` | Linker flags (nostdlib, gc-sections, linker script) |

### Compile-Time Configuration

Configuration values are passed as `-D` flags in `CFLAGS`:

| Define | Default | Description |
|--------|---------|-------------|
| `CONFIG_MAX_TASKS` | 16 | Maximum number of tasks |
| `CONFIG_NUM_PRIORITIES` | 8 | Number of priority levels |
| `CONFIG_DEFAULT_STACK_SIZE` | 1024 | Default stack size in bytes |
| `CONFIG_TICKLESS` | 1 | Enable tickless operation |
| `BR_HAL_SYS_CLOCK_HZ` | 16000000 | System clock frequency |

To change a value, edit the `CFLAGS` line in `chorus.build`.

## Link Order

The linker processes libraries left-to-right. The link command places project libraries before system libraries:

```
main.o -lbedrock_kernel -lbedrock_hal -lbedrock_lib -lc -lnosys -lgcc
```

This ensures that references from kernel/HAL code to `memcpy`, `memset` etc. are resolved by `-lc`.

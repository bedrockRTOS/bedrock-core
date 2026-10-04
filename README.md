# bedrock[RTOS]

[![CI](https://github.com/bedrockRTOS/bedrock-core/actions/workflows/ci.yml/badge.svg)](https://github.com/bedrockRTOS/bedrock-core/actions/workflows/ci.yml)
[![License: GPL-3.0 with runtime exception](https://img.shields.io/badge/license-GPL--3.0%20%2B%20runtime%20exception-blue)](LICENSE.md)

## About

bedrock[RTOS] is a tickless nanokernel RTOS in C11 with no dynamic allocation. It is for microcontroller firmware. The main target is ARM Cortex-M.

## Dependencies

- `arm-none-eabi-gcc` and newlib
- [chorus](https://github.com/z3nnix/chorus) 1.1.0 or newer
- `qemu-system-arm` to run on QEMU
- `gcc` for host tests
- [kconfig-tools](https://github.com/bedrockRTOS/kconfig-tools) and `zig`, only for Kconfig

Arch Linux:

```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib qemu-system-arm
```

Ubuntu:

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi qemu-system-arm
```

Chorus:

```bash
sudo curl -L -o /usr/local/bin/chorus https://github.com/z3nnix/chorus/releases/download/1.1.0/chorus.bin
sudo chmod +x /usr/local/bin/chorus
```

## Build

```bash
chorus
```

The result is `bedrock_example.elf` and the `libbedrock_kernel.a`, `libbedrock_hal.a`, `libbedrock_lib.a` libraries.

Clean:

```bash
chorus clean
```

## Running on QEMU

```bash
chorus run
```

Exit QEMU with `Ctrl+A`, then `X`.

## Tests

The tests run on Linux x86-64 with a mock HAL:

```bash
chorus host
./test_scheduler_host
```

The binaries are `test_task_delete_host`, `test_scheduler_host`, `test_semaphore_host`, `test_mutex_host`, `test_mqueue_host`, `test_sleep_list_host` and `test_pool_host`. Each one prints `PASS` or `FAIL` per case and `=== All Tests Complete ===` at the end.

The same tests for Cortex-M under QEMU: `chorus test-scheduler`, `chorus test-semaphore` and so on.

## Configuration

Defaults are in `include/bedrock/br_config.h`. To use Kconfig instead, put kconfig-tools in `3rd/tools/kconfig` and run:

```bash
chorus build-tools
chorus defconfig
chorus menuconfig
```

This generates `include/generated/autoconf.h`, which takes precedence over the defaults.

## Acknowledgments

- [chorus](https://github.com/z3nnix/chorus): build system
- [QEMU](https://www.qemu.org/): Cortex-M emulation

## Documentation

- [Docs (EN)](docs/en/)
- [Docs (RU)](docs/ru/)
- [Roadmap](ROADMAP.md)
- [Contributing](CONTRIBUTING.md)

## License

`/kernel`, `/arch`, `/boards`, `/include`, `/lib` and `/examples` are under GPL-3.0 with the bedrock[RTOS] runtime exception, see [LICENSE.md](LICENSE.md). Application code linked with the kernel can use any license. Changes to bedrock[RTOS] itself stay under GPL-3.0.

`/docs` is under CC BY-SA 4.0, see [LICENSE-CC-BY-SA-4.0.md](LICENSE-CC-BY-SA-4.0.md).

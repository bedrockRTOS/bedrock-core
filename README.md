# bedrock[RTOS]

[![CI](https://github.com/bedrockRTOS/bedrock-core/actions/workflows/ci.yml/badge.svg)](https://github.com/bedrockRTOS/bedrock-core/actions/workflows/ci.yml)
[![License: GPL-3.0 with runtime exception](https://img.shields.io/badge/license-GPL--3.0%20%2B%20runtime%20exception-blue)](LICENSE.md)

## About

bedrock[RTOS] is a tickless nanokernel RTOS in C11 with no dynamic allocation. It is for microcontroller firmware. Supported targets: ARM Cortex-M3 (QEMU LM3S6965, STM32F103C8 Blue Pill), ARM Cortex-M0+ (RP2040, Raspberry Pi Pico) and RISC-V RV32 (QEMU `virt`).

## Dependencies

- `arm-none-eabi-gcc` for Cortex-M3, STM32F103 and RP2040
- `riscv64-elf-gcc` for RISC-V
- [chorus](https://github.com/z3nnix/chorus) 1.1.0 or newer
- `qemu-system-arm` and `qemu-system-riscv32` to run on QEMU
- `gcc` for host tests and for the RP2040 image tools
- [Renode](https://renode.io/) 1.16.1, only to run STM32F103 and RP2040 tests without hardware, plus [Renode_RP2040](https://github.com/matgla/Renode_RP2040) for RP2040
- [kconfig-tools](https://github.com/bedrockRTOS/kconfig-tools) and `zig`, only for Kconfig

Arch Linux:

```bash
sudo pacman -S arm-none-eabi-gcc qemu-system-arm riscv64-elf-gcc qemu-system-riscv
```

Ubuntu (Cortex-M only):

```bash
sudo apt install gcc-arm-none-eabi qemu-system-arm
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

The result is `bedrock_example.elf` and the `libbedrock_kernel.a`, `libbedrock_hal.a`, `libbedrock_lib.a` libraries for Cortex-M.

RISC-V:

```bash
chorus riscv32
```

The result is `bedrock_example_riscv32.elf`, the `test_*_riscv32.elf` tests and the `libbedrock_*_riscv32.a` libraries.

RP2040:

```bash
chorus rp2040
```

The result is `bedrock_example_rp2040.uf2`, `bedrock_example_rp2040.elf`, the `test_*_rp2040.elf` tests and the `libbedrock_*_rp2040.a` libraries.

STM32F103C8:

```bash
chorus stm32f103
```

The result is `bedrock_example_stm32f103.bin`, `bedrock_example_stm32f103.elf` and the `test_*_stm32f103.elf` tests.

Clean:

```bash
chorus clean
```

## Running on QEMU

Cortex-M (LM3S6965):

```bash
chorus run
```

RISC-V (`virt`):

```bash
chorus run-riscv32
```

Exit QEMU with `Ctrl+A`, then `X`.

## Running on RP2040

Hold BOOTSEL, connect the Pico over USB and copy `bedrock_example_rp2040.uf2` to the `RPI-RP2` drive. Output goes to UART0: TX on GP0, RX on GP1, 115200 8N1. The port has been tested in Renode only, not on hardware yet.

In Renode, with Renode_RP2040 cloned to `<path>`:

```bash
renode --disable-gui --console -e '$rp2040=@<path>; $fw=@'"$PWD"'/test_scheduler_rp2040.elf; $out=@/tmp/uart.txt; $time="2"; include @'"$PWD"'/boards/rpi-pico/renode.resc'
cat /tmp/uart.txt
```

## Running on STM32F103C8

Flash `bedrock_example_stm32f103.bin` at `0x08000000`, for example with `st-flash write bedrock_example_stm32f103.bin 0x08000000`. The board runs at 72 MHz from an 8 MHz crystal. Output goes to USART1: TX on PA9, RX on PA10, 115200 8N1. The port has been tested in Renode only, not on hardware yet.

In Renode:

```bash
renode --disable-gui --console -e '$fw=@'"$PWD"'/test_scheduler_stm32f103.elf; $out=@/tmp/uart.txt; $time="2"; include @'"$PWD"'/boards/stm32f103c8-bluepill/renode.resc'
cat /tmp/uart.txt
```

## Tests

The tests run on Linux x86-64 with a mock HAL:

```bash
chorus host
./test_scheduler_host
```

The binaries are `test_task_delete_host`, `test_scheduler_host`, `test_semaphore_host`, `test_mutex_host`, `test_mqueue_host`, `test_sleep_list_host` and `test_pool_host`. Each one prints `PASS` or `FAIL` per case and `=== All Tests Complete ===` at the end.

The same tests under QEMU: `chorus test-scheduler`, `chorus test-semaphore` and so on for Cortex-M, `chorus test-scheduler-riscv32` and so on for RISC-V. RP2040 and STM32F103 tests run in Renode, see above.

## Configuration

Defaults are in `include/bedrock/br_config.h`. To use Kconfig instead, put kconfig-tools in `3rd/tools/kconfig` and run:

```bash
chorus build-tools
chorus defconfig
chorus menuconfig
```

For RISC-V use `chorus defconfig-riscv32`, for RP2040 `chorus defconfig-rp2040`, for STM32F103 `chorus defconfig-stm32f103` instead of `chorus defconfig`.

This generates `include/generated/autoconf.h`, which takes precedence over the defaults.

## Acknowledgments

- [chorus](https://github.com/z3nnix/chorus): build system
- [QEMU](https://www.qemu.org/): Cortex-M and RISC-V emulation
- [Renode](https://renode.io/): STM32F103 and RP2040 emulation, [Renode_RP2040](https://github.com/matgla/Renode_RP2040): RP2040 models

## Documentation

- [Docs (EN)](docs/en/)
- [Docs (RU)](docs/ru/)
- [Roadmap](ROADMAP.md)
- [Contributing](CONTRIBUTING.md)

## License

`/kernel`, `/arch`, `/boards`, `/include`, `/lib` and `/examples` are under GPL-3.0 with the bedrock[RTOS] runtime exception, see [LICENSE.md](LICENSE.md). Application code linked with the kernel can use any license. Changes to bedrock[RTOS] itself stay under GPL-3.0.

`/docs` is under CC BY-SA 4.0, see [LICENSE-CC-BY-SA-4.0.md](LICENSE-CC-BY-SA-4.0.md).

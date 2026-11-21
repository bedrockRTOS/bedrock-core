# AVR (ATmega328P, Arduino Uno)

The AVR port runs the kernel on an 8-bit ATmega328P: 32 KiB of flash and 2 KiB of RAM. It is `arch/avr/` with the board in `boards/arduino-uno/`. The tests run in QEMU (`qemu-system-avr -machine uno`).

## Build and run

The toolchain is `avr-gcc` with `avr-libc` (`gcc-avr`, `avr-libc` and `binutils-avr` on Debian and Ubuntu, `avr-gcc` and `avr-libc` on Arch) and `qemu-system-avr` (`qemu-system-misc` on Debian and Ubuntu).

```bash
chorus avr
chorus test-semaphore-avr
```

`chorus avr` builds `test_<name>_avr.elf` for the tests below. `chorus test-<name>-avr` runs one in QEMU. `uart_rx` and `shell` read a line from the console, so type `bedrock` after a second. The QEMU model is slow, a test can take several seconds.

The startup code, the vector table and the C runtime come from avr-libc. The clock is `BR_HAL_SYS_CLOCK_HZ`, 16 MHz in `AVR_CFLAGS`. It must be 1, 2, 4, 8 or 16 MHz. For another ATmega with a 2-byte program counter (up to 128 KiB of flash) change `-mmcu` in `AVR_CFLAGS` and `AVR_LDFLAGS`, and the vector numbers come from avr-libc by name. An ATmega2560 has a 3-byte program counter and is not supported (`#error`).

## Memory

RAM is the limit. The defaults in `include/bedrock/br_config.h` for `__AVR__`:

| Setting | AVR | Others |
|---------|-----|--------|
| `CONFIG_MAX_TASKS` | 5 (a TCB is 34 bytes) | 16 |
| `CONFIG_DEFAULT_STACK_SIZE` (the idle stack) | 64 | 1024 |
| `CONFIG_UART_RX_BUF_SIZE` | 16 | 64 |
| `CONFIG_SHELL_LINE_MAX`, `CONFIG_SHELL_MAX_ARGS` | 32, 4 | 64, 8 |
| `CONFIG_TASK_STATS` | 0 | 1 |
| `BR_STACK_MIN` | 64 | 256 |

The kernel itself takes about 600 bytes of RAM with five tasks. A task needs 4 bytes for the canary, the 35-byte context frame and its own calls: a task that sleeps and prints needs about 90 bytes. There is one more stack for the interrupts (112 bytes, see below).

String literals live in RAM on an AVR, because the CPU reads flash and RAM with different instructions. A test with many messages uses 600 to 1300 bytes for them.

Link-time budget: `AVR_LDFLAGS` sets `__DATA_REGION_LENGTH__` to 1952, which keeps 96 bytes at the top of RAM for the stack of `main()` until the scheduler starts. A program that needs more fails to link.

## Design

**Context.** A task that is switched out and a task that is interrupted have the same frame, from the lowest address up: `r31 ... r2, r1, SREG, r0` and the 2-byte return address, 35 bytes. `br_hal_context_switch()` called from a task saves the frame, stores the stack pointer in `*old_sp`, loads `*new_sp` and restores. `SREG` of a new task has the interrupt flag set.

**Interrupts.** `BR_AVR_KERNEL_ISR(vector, handler)` (`arch/avr/br_avr_isr.h`) defines a vector that saves the frame on the stack of the task, moves to a separate 112-byte interrupt stack and calls the C `handler`. When the handler returns it makes the switch that `br_hal_context_switch()` recorded, as on the other ports. A task stack so only needs room for the frame. The lowest bytes of the interrupt stack hold a canary that is checked at each exit, `br_hal_panic()` on overflow. A plain `ISR()` of avr-libc must not call the kernel.

**Timer.** Timer1 runs at clk/64: 4 us per tick at 16 MHz. Its overflow interrupt counts the wraps, which makes a 48-bit tick count (`br_hal_timer_get_us()`). The compare match A interrupt is the one-shot alarm and the round-robin time slice, at most 196 ms ahead (the timer re-arms itself when nothing is due). The resolution of `br_time_t` is 4 us. The interrupt flags are never written, the hardware clears them.

**Board init.** `br_hal_board_init()` is called from the `.init7` section, after `.data` and `.bss` are set up and before `main()`.

**UART.** USART0, 115200 baud, 8N1, the receive interrupt feeds the ring buffer of `br_uart_getc()`.

## Peripherals

| HAL | Implementation |
|-----|----------------|
| GPIO | ports 0, 1, 2 are PORTB, PORTC, PORTD. Input, pull-up and output. No pull-down (`BR_ERR_INVALID`). A pin that is an output reads back its output latch |
| SPI | master on PB3, PB4, PB5, dividers 2 to 128, modes 0 to 3, no loopback |
| I2C | TWI master on PC4 and PC5, 100 kHz and up, a NAK is `BR_ERR_BUSY` |
| PWM | Timer0 fast PWM, channel 0 on PD6 and channel 1 on PD5. Both channels share the frequency, which is F_CPU / (prescaler * 256), the closest prescaler is taken. A different frequency for the second channel is `BR_ERR_BUSY`. 8-bit duty |
| Capture | none (`BR_ERR_INVALID`), Timer1 is the system timer |
| Network | none |

The GPIO, SPI, I2C and PWM drivers are not exercised by a test: QEMU has no GPIO ports in its ATmega328P model (every register reads 0, so the driver returns `BR_ERR_IO` and `test_gpio` prints `SKIP`), no SPI and no TWI. They follow the data sheet and have not run on a chip.

## Tests

| Test | RAM used |
|------|----------|
| `task_delete`, `scheduler`, `sleep_list`, `task_info`, `console`, `semaphore`, `mutex`, `mqueue`, `pool`, `uart_rx`, `shell`, `device`, `gpio` | 0.9 to 1.9 KiB of 1.9 KiB |

`shell_cmds`, `audit`, `spi`, `i2c`, `pwm`, the file system, the network and the memory protection tests do not run on this target: they need more RAM than 1952 bytes, or the hardware (see above), or a feature the port does not have (file system and network need more RAM than the whole chip, there is no MPU).

The tests were changed to fit: `test_stack.h` gives `TEST_STACK(n)`, which is 3/16 of `n` on an AVR, the tests delete the tasks they are done with, the format tests use smaller buffers, and the supervisor of `console` and `shell` has a larger stack.

## Limits

- 2-byte program counter only (up to 128 KiB of flash).
- No memory protection, no file system, no network. `CONFIG_MPU` is not available.
- Timer1 is taken by the kernel, Timer0 by PWM.
- One UART, no interrupt-driven transmit: `br_uart_putc()` waits for the data register.
- The port was run in QEMU only. QEMU runs the CPU faster than 16 MHz and the timers in real time, so the short waits in the tests can fail on a very loaded machine.

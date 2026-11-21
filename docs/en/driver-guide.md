# Driver Guide

This guide describes the device model, the peripheral drivers and the HAL functions behind them.

## Layers

```
application
    |  br_device_open / read / write / ioctl, typed helpers (br_spi_transfer, ...)
drivers/            br_gpio, br_spi, br_i2c, br_pwm, br_capture, br_uart_dev
    |  br_device_t with an ops table
kernel/br_device.c  registry and open counting
    |
HAL                 br_hal_gpio_*, br_hal_spi_*, br_hal_i2c_*, br_hal_pwm_*, br_hal_capture_*
    |
arch/, boards/      register access
```

The kernel never touches a peripheral. A driver only calls the HAL. The HAL is implemented per architecture or board.

## Device model

Declared in `include/bedrock/br_device.h` and `bedrock.h`.

```c
typedef struct {
    br_err_t (*open)(br_device_t *dev);
    br_err_t (*close)(br_device_t *dev);
    int      (*read)(br_device_t *dev, void *buf, size_t len, br_time_t timeout);
    int      (*write)(br_device_t *dev, const void *buf, size_t len, br_time_t timeout);
    br_err_t (*ioctl)(br_device_t *dev, uint32_t cmd, void *arg);
} br_device_ops_t;

typedef struct br_device {
    const char            *name;
    const br_device_ops_t *ops;
    void                  *priv;
    uint8_t                open_count;
    struct br_device      *next;
} br_device_t;
```

Every operation is optional. A call to a missing operation returns `BR_ERR_INVALID`.

| Function | Behavior |
|----------|----------|
| `br_device_register(dev, name, ops, priv)` | Add `dev` to the registry. `BR_ERR_BUSY` if the name or the object is already registered, `BR_ERR_INVALID` for `NULL` arguments. `name` and `ops` must stay valid. |
| `br_device_find(name)` | Look a device up by name, `NULL` if there is none. |
| `br_device_open(dev)` | The first open calls `ops->open`. Later opens only count. `BR_ERR_INVALID` from an ISR. |
| `br_device_close(dev)` | The last close calls `ops->close`. `BR_ERR_INVALID` if the device is not open. |
| `br_device_read` / `br_device_write` | Return the number of bytes transferred, or a negative `br_err_t`. The device must be open. |
| `br_device_ioctl(dev, cmd, arg)` | Driver specific. The device must be open. |

The registry is a linked list through `br_device_t::next`. Device objects belong to the application, there is no allocation.

## Writing a driver

A driver is a structure that embeds `br_device_t`, an ops table and a register function. `drivers/br_gpio.c` is the smallest example.

```c
typedef struct {
    br_device_t dev;
    uint8_t     port;
} br_gpio_dev_t;

br_err_t br_gpio_register(br_gpio_dev_t *gpio, const char *name, uint8_t port)
{
    gpio->port = port;
    return br_device_register(&gpio->dev, name, &gpio_ops, gpio);
}
```

Rules:

- Keep per-device state in the driver structure and reach it through `dev->priv`.
- Shared buses (SPI, I2C) hold a `br_mutex_t` in the driver structure and take it for every transaction. Drivers must not call `br_mutex_lock` from an ISR.
- Return the byte count from `read` and `write`, and a negative `br_err_t` on failure.
- Put commands for `ioctl` in the driver header. Command numbers are `0x01xx` GPIO, `0x02xx` SPI, `0x03xx` I2C, `0x04xx` PWM, `0x05xx` capture.

## Drivers

All drivers are registered by the application, with the name it picks. Open the device before using it.

### UART: `drivers/br_uart_dev.h`

```c
br_err_t br_uart_dev_register(br_uart_dev_t *uart, const char *name);
```

`write` sends every byte through `br_uart_putc`. `read` blocks for the first byte up to `timeout`, then returns the bytes already in the receive buffer. It returns `BR_ERR_TIMEOUT` if nothing arrived. Reception is interrupt driven and buffered by `kernel/br_uart.c`.

### GPIO: `drivers/br_gpio.h`

One device per port.

```c
br_err_t br_gpio_register(br_gpio_dev_t *gpio, const char *name, uint8_t port);
br_err_t br_gpio_configure(br_device_t *dev, uint8_t pin, br_gpio_mode_t mode);
br_err_t br_gpio_write(br_device_t *dev, uint8_t pin, bool level);
br_err_t br_gpio_read(br_device_t *dev, uint8_t pin, bool *level);
br_err_t br_gpio_toggle(br_device_t *dev, uint8_t pin);
```

Modes: `BR_GPIO_INPUT`, `BR_GPIO_INPUT_PULLUP`, `BR_GPIO_INPUT_PULLDOWN`, `BR_GPIO_OUTPUT`. An output pin reads back its driven level. `br_gpio_toggle` reads and writes, so it is not atomic.

### SPI: `drivers/br_spi.h`

One device per bus, master only. Chip select is a GPIO that the application drives.

```c
br_err_t br_spi_register(br_spi_dev_t *spi, const char *name, uint8_t bus);
br_err_t br_spi_set_speed(br_device_t *dev, uint32_t hz);
br_err_t br_spi_set_mode(br_device_t *dev, uint8_t mode);
br_err_t br_spi_transfer(br_device_t *dev, const void *tx, void *rx, size_t len);
```

The default is 1 MHz, mode 0, 8 bits, MSB first. `mode` is a mask of `BR_SPI_CPHA`, `BR_SPI_CPOL` and `BR_SPI_LOOP` (internal loopback, `BR_ERR_INVALID` where the controller has none). `tx` or `rx` can be `NULL`: a missing `tx` sends `0xFF`, a missing `rx` discards the data. `write` and `read` are one-direction transfers. The requested speed is rounded down to the nearest the divider allows, and `BR_ERR_INVALID` is returned if the controller cannot go that slow.

### I2C: `drivers/br_i2c.h`

One device per bus, master only, 7-bit addresses, 100 kHz by default, up to 400 kHz.

```c
br_err_t br_i2c_register(br_i2c_dev_t *i2c, const char *name, uint8_t bus);
br_err_t br_i2c_set_addr(br_device_t *dev, uint8_t addr);
br_err_t br_i2c_set_speed(br_device_t *dev, uint32_t hz);
br_err_t br_i2c_write_read(br_device_t *dev, uint8_t addr,
                           const void *wbuf, size_t wlen,
                           void *rbuf, size_t rlen);
```

`write` and `read` use the address set with `br_i2c_set_addr`. `br_i2c_write_read` writes `wbuf` without a stop condition and reads `rbuf` after a repeated start, under one lock. A NAK from the device is `BR_ERR_BUSY`. A bus that does not complete is `BR_ERR_TIMEOUT`.

### PWM and capture: `drivers/br_pwm.h`

```c
br_err_t br_pwm_register(br_pwm_dev_t *pwm, const char *name, uint8_t ch);
br_err_t br_pwm_set(br_device_t *dev, uint32_t freq_hz, uint16_t duty_permille);
br_err_t br_pwm_stop(br_device_t *dev);

br_err_t br_capture_register(br_capture_dev_t *cap, const char *name, uint8_t ch);
br_err_t br_capture_set_edge(br_device_t *dev, br_capture_edge_t edge);
br_err_t br_capture_read_period(br_device_t *dev, uint32_t *period_us, br_time_t timeout);
```

`duty_permille` is 0 to 1000. Closing a PWM device stops the output. The capture device measures the time in microseconds between two consecutive selected edges. Opening it starts capturing. `read` returns four bytes, a `uint32_t`, after the next edge pair, or `BR_ERR_TIMEOUT`. The driver polls the HAL every 100 us, so it needs a running scheduler.

## HAL contract

All functions are in `include/bedrock/br_hal.h`. All return `br_err_t`.

| Function | Contract |
|----------|----------|
| `br_hal_gpio_configure(port, pin, mode)` | Enable the port clock if needed and set the mode. |
| `br_hal_gpio_write(port, pin, level)` | Drive the pin. The level of a pin that is not an output is unspecified. |
| `br_hal_gpio_read(port, pin, &level)` | Read the pin. |
| `br_hal_spi_init(bus, hz, mode)` | Enable the controller and its pins, program the clock. Can be called again to change the setup. |
| `br_hal_spi_transfer(bus, tx, rx, len)` | Full duplex, blocking. `BR_ERR_INVALID` before init. |
| `br_hal_i2c_init(bus, hz)` | Enable the controller and its pins, program the clock. |
| `br_hal_i2c_write(bus, addr, data, len, stop)` | Start, address, data. Send a stop only if `stop` is true, otherwise hold the bus for a repeated start. |
| `br_hal_i2c_read(bus, addr, data, len)` | Start or repeated start, address, `len` bytes, NAK on the last, stop. |
| `br_hal_pwm_start(ch, freq_hz, duty_permille)` | Start or retune a channel. |
| `br_hal_pwm_stop(ch)` | Stop a channel. |
| `br_hal_capture_start(ch, edge)` | Start capturing the chosen edge, forget earlier edges. |
| `br_hal_capture_stop(ch)` | Stop capturing. |
| `br_hal_capture_read(ch, &period_us)` | `BR_OK` and the period when a new edge pair was seen, `BR_ERR_BUSY` when there is none. |

Common errors: `BR_ERR_INVALID` for a port, pin, bus, channel or argument that the target does not have, `BR_ERR_BUSY` for an I2C NAK or arbitration loss, `BR_ERR_TIMEOUT` when the peripheral does not respond. Blocking loops are bounded and end with `BR_ERR_TIMEOUT`.

A function that the target cannot provide must still exist and return `BR_ERR_INVALID`.

## Targets

| | QEMU LM3S6965 | STM32F103C8 | RP2040 | RISC-V `virt` | Host |
|---|---|---|---|---|---|
| GPIO | ports 0 to 6 (A to G), pins 0 to 7 | ports 0 to 4 (A to E), pins 0 to 15 | port 0, pins 0 to 29 | none | ports 0 and 1, pins 0 to 31 |
| SPI bus 0 | SSI0: PA2 SCK, PA4 MISO, PA5 MOSI | SPI1: PA5 SCK, PA6 MISO, PA7 MOSI | SPI0: GP18 SCK, GP16 MISO, GP19 MOSI | none | loopback model |
| SPI loopback | yes | no | yes | no | yes |
| I2C bus 0 | I2C0: PB2 SCL, PB3 SDA | I2C1: PB6 SCL, PB7 SDA | I2C0: GP5 SCL, GP4 SDA | none | EEPROM model at 0x50 |
| PWM channels | 0 to 5: PD0, PD1, PB0, PB1, PE0, PE1 | 0 to 3: PA0 to PA3 (TIM2) | 0 to 3: GP10 to GP13 | none | 0 to 3 |
| Capture channels | 0: PD4 (Timer0), 1: PD5 (Timer2) | 0: PB0, 1: PB1 (TIM3) | 0: GP14, 1: GP15 | none | 0 to 3, follow PWM |

Notes:

- PWM channels that share a generator (LM3S6965 pairs 0/1, 2/3, 4/5), a slice (RP2040 pairs 0/1, 2/3) or a timer (STM32F103 channels 0 to 3) share one frequency. Starting one with a new frequency changes the other.
- LM3S6965 PWM runs from the system clock divided by 16, so the lowest frequency is about 245 Hz at 16 MHz. Capture is a 24-bit counter on the system clock, 1.05 s at 16 MHz.
- STM32F103 capture counts at 1 MHz in 16 bits, so the longest period is 65 ms.
- RP2040 capture uses the GPIO edge interrupt and the 1 MHz timer. The interrupt handler `IO_IRQ_BANK0_Handler` lives in `boards/rpi-pico/br_hal_pwm.c`.
- The RISC-V `virt` machine has no GPIO, SPI, I2C or PWM device. Those functions return `BR_ERR_INVALID` until a real RISC-V board is added.
- STM32F103 SPI has no internal loopback. RP2040 uses the internal 12 MHz peripheral clock for SPI, I2C and PWM.

## Verification status

The register code was written from the reference manuals. It was run on:

| Part | Where |
|------|-------|
| All drivers and the device model | host |
| GPIO, SPI (loopback), I2C (NAK), PWM start and stop on LM3S6965 | QEMU |
| STM32F103 GPIO | Renode |

Not verified: STM32F103 SPI and I2C and the timers (Renode has no SPI model and models I2C as an STM32F4 part), LM3S6965 PWM and capture output (QEMU does not model them), all RP2040 peripherals (no Renode RP2040 model in the test setup), and anything on real hardware.

## Tests

`test_device`, `test_gpio`, `test_spi`, `test_i2c` and `test_pwm` run on every target. A test prints `SKIP` instead of `PASS` when the target has no such peripheral or the peripheral does not respond, for example SPI in Renode.

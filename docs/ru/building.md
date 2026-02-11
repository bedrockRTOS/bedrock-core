<!--
  Project: bedrock[RTOS]
  Version: 0.0.1
  Author:  AnmiTaliDev <anmitalidev@nuros.org>
  License: CC BY-SA 4.0
-->

# Сборка

bedrock[RTOS] использует [chorus](https://github.com/z3nnix/chorus) в качестве системы сборки. Chorus включён как git-подмодуль в `3rd/tools/chorus`.

## Зависимости

- Тулчейн `arm-none-eabi-gcc`
- `qemu-system-arm` (для запуска в QEMU)
- Компилятор Go (для сборки chorus, если нет готового бинарника)

### Установка на Arch Linux

```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib qemu-system-arm
```

### Сборка Chorus

```bash
cd 3rd/tools/chorus
go build -o chorus .
sudo cp chorus /usr/local/bin/
```

## Сборка проекта

Из корня проекта:

```bash
chorus
```

Результат:

| Артефакт | Описание |
|----------|----------|
| `libbedrock_kernel.a` | Ядро: планировщик, задачи, время, IPC |
| `libbedrock_hal.a` | HAL: таймер, переключение контекста, UART, стартовый код |
| `libbedrock_lib.a` | Библиотека: статический аллокатор пулов |
| `bedrock_example.elf` | Пример приложения, слинкованный со всеми библиотеками |

## Очистка

```bash
chorus clean
```

Удаляет все файлы `*.o`, `*.a`, `*.elf`, `*.bin`.

## Запуск в QEMU

```bash
qemu-system-arm -M lm3s6965evb -nographic -kernel bedrock_example.elf
```

Для выхода из QEMU: `Ctrl+A`, затем `X`.

## Конфигурация сборки

Файл `chorus.build` определяет:

### Переменные

| Переменная | Описание |
|------------|----------|
| `CC` | Компилятор C (`arm-none-eabi-gcc`) |
| `AR` | Архиватор (`arm-none-eabi-ar`) |
| `OBJCOPY` | Утилита копирования объектов (`arm-none-eabi-objcopy`) |
| `CFLAGS` | Флаги компиляции (C11, Cortex-M3, thumb, пути включения, define-конфигурация) |
| `LDFLAGS` | Флаги линковки (nostdlib, gc-sections, скрипт линковки) |

### Конфигурация времени компиляции

Значения конфигурации передаются как флаги `-D` в `CFLAGS`:

| Определение | По умолчанию | Описание |
|-------------|--------------|----------|
| `CONFIG_MAX_TASKS` | 16 | Максимальное число задач |
| `CONFIG_NUM_PRIORITIES` | 8 | Количество уровней приоритета |
| `CONFIG_DEFAULT_STACK_SIZE` | 1024 | Размер стека по умолчанию в байтах |
| `CONFIG_TICKLESS` | 1 | Тиклесс-режим |
| `BR_HAL_SYS_CLOCK_HZ` | 16000000 | Частота системного тактирования |

Для изменения значения отредактируйте строку `CFLAGS` в `chorus.build`.

## Порядок линковки

Линкер обрабатывает библиотеки слева направо. Команда линковки размещает библиотеки проекта перед системными:

```
main.o -lbedrock_kernel -lbedrock_hal -lbedrock_lib -lc -lnosys -lgcc
```

Это обеспечивает разрешение ссылок из кода ядра/HAL на `memcpy`, `memset` и т.д. через `-lc`.

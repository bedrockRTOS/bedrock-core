/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

OUTPUT_ARCH(riscv)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

ENTRY(_start)

SECTIONS
{
    .text :
    {
        KEEP(*(.text.start))
        *(.text*)
        . = ALIGN(4);
    } > RAM

    .rodata :
    {
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
    } > RAM

    .data :
    {
        *(.data*)
        *(.sdata*)
        . = ALIGN(4);
    } > RAM

    .bss :
    {
        _sbss = .;
        *(.sbss*)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    _estack = ORIGIN(RAM) + LENGTH(RAM);
}

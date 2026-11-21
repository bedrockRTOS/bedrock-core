/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot :
    {
        KEEP(*(.multiboot))
    }

    .text :
    {
        *(.text.start)
        *(.text*)
        . = ALIGN(16);
    }

    .rodata :
    {
        *(.rodata*)
        . = ALIGN(16);
    }

    .data :
    {
        *(.data*)
        . = ALIGN(16);
    }

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    }

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.eh_frame*)
    }
}

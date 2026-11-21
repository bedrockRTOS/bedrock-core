/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

ENTRY(_start)

SECTIONS
{
    . = 1M;
    _text_start = .;

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
        . = ALIGN(4096);
        _code_end = .;
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

    .user_bss (NOLOAD) : ALIGN(4096)
    {
        _user_data_start = .;
        *(.user_bss*)
        . = _user_data_start + 4096;
        _user_data_end = .;
    }

    ASSERT(. <= 4M, "the image must fit in the first 4 MiB")

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.eh_frame*)
    }
}

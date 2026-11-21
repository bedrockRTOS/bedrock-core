/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 112K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT> FLASH

    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    .user_bss (NOLOAD) : ALIGN(4096)
    {
        _user_data_start = .;
        *(.user_bss*)
        . = _user_data_start + 4096;
        _user_data_end = .;
    } > SRAM

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);

    _mpu_code_base = ORIGIN(FLASH);
    _mpu_code_size = LENGTH(FLASH);
}

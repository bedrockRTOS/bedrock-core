/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#include <stddef.h>

#include "x86_io.h"

#define BOOT_STACK_SIZE  16384
#define MULTIBOOT_MAGIC  0x1BADB002UL
#define MULTIBOOT_FLAGS  0x00000000UL

#define GDT_ENTRIES      6
#define IDT_ENTRIES      51
#define IDT_SIZE         0x82
#define GATE_INTERRUPT   0x8E
#define GATE_USER        0xEE

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) descriptor_ptr_t;

typedef struct {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type;
    uint16_t offset_hi;
} __attribute__((packed)) idt_gate_t;

extern uint32_t _sbss, _ebss;
extern uint32_t _user_data_start __attribute__((weak));
extern uint32_t _user_data_end __attribute__((weak));
extern const uint32_t br_hal_isr_table[IDT_ENTRIES];
extern int main(void);
extern void br_hal_board_init(void);

typedef struct {
    uint32_t prev;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t unused[22];
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

uint64_t          br_hal_gdt[GDT_ENTRIES];
tss_t             br_hal_tss;
static idt_gate_t idt[IDT_SIZE];

static uint64_t make_descriptor(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    uint64_t d = limit & 0xFFFFU;
    d |= (uint64_t)(base & 0xFFFFFFU) << 16;
    d |= (uint64_t)access << 40;
    d |= (uint64_t)((limit >> 16) & 0xFU) << 48;
    d |= (uint64_t)(flags & 0xFU) << 52;
    d |= (uint64_t)(base >> 24) << 56;
    return d;
}

__attribute__((section(".multiboot"), used))
static const uint32_t multiboot_header[3] = {
    MULTIBOOT_MAGIC,
    MULTIBOOT_FLAGS,
    (uint32_t)-(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS),
};

uint8_t boot_stack[BOOT_STACK_SIZE] __attribute__((aligned(16), used));

void reset_handler(void);

__attribute__((naked, section(".text.start")))
void _start(void)
{
    __asm volatile (
        "cli\n"
        "movl $boot_stack + %c0, %%esp\n"
        "xorl %%ebp, %%ebp\n"
        "call reset_handler\n"
        :: "i" (BOOT_STACK_SIZE)
    );
}

static void gdt_init(void)
{
    br_hal_gdt[0] = 0;
    br_hal_gdt[1] = 0x00CF9A000000FFFFULL;
    br_hal_gdt[2] = 0x00CF92000000FFFFULL;
    br_hal_gdt[3] = 0x00CFFA000000FFFFULL;
    br_hal_gdt[4] = 0x00CFF2000000FFFFULL;

    br_hal_tss.ss0        = 0x10;
    br_hal_tss.iomap_base = sizeof(br_hal_tss);
    br_hal_gdt[5] = make_descriptor((uint32_t)(uintptr_t)&br_hal_tss, sizeof(br_hal_tss) - 1, 0x89, 0x0);

    descriptor_ptr_t ptr = { sizeof(br_hal_gdt) - 1, (uint32_t)(uintptr_t)br_hal_gdt };

    __asm volatile (
        "lgdt %0\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        "movw $0x28, %%ax\n"
        "ltr %%ax\n"
        :: "m" (ptr) : "eax", "memory"
    );
}

static void idt_init(void)
{
    for (int i = 0; i < IDT_ENTRIES; i++) {
        int vector = i < 49 ? i : 0x80 + (i - 49);
        uint32_t handler = br_hal_isr_table[i];
        idt[vector].offset_lo = (uint16_t)(handler & 0xFFFF);
        idt[vector].selector  = 0x08;
        idt[vector].zero      = 0;
        idt[vector].type      = (vector == 0x80) ? GATE_USER : GATE_INTERRUPT;
        idt[vector].offset_hi = (uint16_t)(handler >> 16);
    }

    descriptor_ptr_t ptr = { sizeof(idt) - 1, (uint32_t)(uintptr_t)idt };
    __asm volatile ("lidt %0" :: "m" (ptr));
}

static void pic_init(void)
{
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, VECTOR_IRQ_BASE);
    outb(PIC2_DATA, VECTOR_IRQ_BASE + 8);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void reset_handler(void)
{
    uint32_t *dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    dst = &_user_data_start;
    while (dst != NULL && dst < &_user_data_end) {
        *dst++ = 0;
    }

    gdt_init();
    idt_init();
    pic_init();

    br_hal_board_init();
    main();
    while (1) {
        __asm volatile ("hlt");
    }
}

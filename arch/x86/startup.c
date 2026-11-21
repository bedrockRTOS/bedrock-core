/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#include "x86_io.h"

#define BOOT_STACK_SIZE  16384
#define MULTIBOOT_MAGIC  0x1BADB002UL
#define MULTIBOOT_FLAGS  0x00000000UL

#define GDT_ENTRIES      3
#define IDT_ENTRIES      49
#define GATE_INTERRUPT   0x8E

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
extern const uint32_t br_hal_isr_table[IDT_ENTRIES];
extern int main(void);
extern void br_hal_board_init(void);

static uint64_t   gdt[GDT_ENTRIES];
static idt_gate_t idt[IDT_ENTRIES];

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
    gdt[0] = 0;
    gdt[1] = 0x00CF9A000000FFFFULL;
    gdt[2] = 0x00CF92000000FFFFULL;

    descriptor_ptr_t ptr = { sizeof(gdt) - 1, (uint32_t)(uintptr_t)gdt };

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
        :: "m" (ptr) : "eax", "memory"
    );
}

static void idt_init(void)
{
    for (int i = 0; i < IDT_ENTRIES; i++) {
        uint32_t handler = br_hal_isr_table[i];
        idt[i].offset_lo = (uint16_t)(handler & 0xFFFF);
        idt[i].selector  = 0x08;
        idt[i].zero      = 0;
        idt[i].type      = GATE_INTERRUPT;
        idt[i].offset_hi = (uint16_t)(handler >> 16);
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

    gdt_init();
    idt_init();
    pic_init();

    br_hal_board_init();
    main();
    while (1) {
        __asm volatile ("hlt");
    }
}

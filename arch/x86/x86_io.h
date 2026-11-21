/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef X86_IO_H
#define X86_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    __asm volatile ("outb %0, %1" :: "a" (value), "Nd" (port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline uint64_t rdtsc(void)
{
    uint32_t lo;
    uint32_t hi;
    __asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

#define VECTOR_IRQ_BASE   0x20
#define VECTOR_SWITCH     0x30

#endif

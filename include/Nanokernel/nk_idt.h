/* nk_idt.h - System 7X Nanokernel IDT Driver Header (C23)
 *
 * Interrupt Descriptor Table management for x86-32.
 */

#ifndef NK_IDT_H
#define NK_IDT_H

#include <stdint.h>

/* IDT gate types */
#define IDT_TYPE_TASK      0x5  /* Task gate */
#define IDT_TYPE_INT16     0x6  /* 16-bit interrupt gate */
#define IDT_TYPE_TRAP16    0x7  /* 16-bit trap gate */
#define IDT_TYPE_INT32     0xE  /* 32-bit interrupt gate */
#define IDT_TYPE_TRAP32    0xF  /* 32-bit trap gate */

/* IDT gate flags */
#define IDT_FLAG_PRESENT   0x80  /* Present bit */
#define IDT_FLAG_DPL0      0x00  /* Privilege level 0 (kernel) */
#define IDT_FLAG_DPL1      0x20  /* Privilege level 1 */
#define IDT_FLAG_DPL2      0x40  /* Privilege level 2 */
#define IDT_FLAG_DPL3      0x60  /* Privilege level 3 (user) */

/**
 * IDT entry (gate descriptor) for x86-32.
 * 8 bytes per entry.
 */
struct idt_entry {
    uint16_t offset_low;   /* Offset bits 0-15 */
    uint16_t selector;     /* Code segment selector */
    uint8_t  zero;         /* Reserved, must be zero */
    uint8_t  type_attr;    /* Type and attributes */
    uint16_t offset_high;  /* Offset bits 16-31 */
} __attribute__((packed));

/**
 * IDT pointer structure (for LIDT instruction).
 */
struct idt_ptr {
    uint16_t limit;   /* Size of IDT - 1 */
    uint32_t base;    /* Base address of IDT */
} __attribute__((packed));

/**
 * Set an IDT gate entry.
 *
 * @param num IDT entry number (0-255)
 * @param handler Pointer to interrupt handler function
 * @param selector Code segment selector (typically 0x08 for kernel)
 * @param flags Type and attribute flags
 */
void nk_idt_set_gate(uint8_t num, void (*handler)(void), uint16_t selector, uint8_t flags);

/**
 * Initialize and install the IDT.
 * Sets up 256 IDT entries and loads the IDT register.
 */
void nk_idt_install(void);

/**
 * Get the current IDT entry count (for debugging).
 */
uint16_t nk_idt_get_count(void);

#endif /* NK_IDT_H */

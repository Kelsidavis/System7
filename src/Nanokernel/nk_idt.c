/* nk_idt.c - System 7X Nanokernel IDT Driver Implementation (C23)
 *
 * Manages the Interrupt Descriptor Table for x86-32.
 */

#include "../../include/Nanokernel/nk_idt.h"
#include <stddef.h>

/* Serial logging */
extern void serial_puts(const char *s);

/* IDT table: 256 entries */
#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

/**
 * Set an IDT gate entry.
 */
void nk_idt_set_gate(uint8_t num, void (*handler)(void), uint16_t selector, uint8_t flags) {
    uintptr_t base = (uintptr_t)handler;

    idt[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt[num].offset_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

/**
 * Load IDT register using LIDT instruction.
 */
static inline void idt_load(struct idt_ptr *ptr) {
    __asm__ volatile("lidt (%0)" : : "r"(ptr));
}

/**
 * Initialize and install the IDT.
 */
void nk_idt_install(void) {
    /* Set up IDT pointer */
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (uint32_t)&idt;

    /* Clear all IDT entries */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low  = 0;
        idt[i].offset_high = 0;
        idt[i].selector    = 0;
        idt[i].zero        = 0;
        idt[i].type_attr   = 0;
    }

    /* Load IDT register */
    idt_load(&idtp);

    serial_puts("[nk_idt] IDT installed (256 entries)\n");
}

/**
 * Get IDT entry count (for debugging).
 */
uint16_t nk_idt_get_count(void) {
    return IDT_ENTRIES;
}

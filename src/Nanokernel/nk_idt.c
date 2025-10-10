/* nk_idt.c - System 7X Nanokernel IDT Driver Implementation (C23)
 *
 * Manages the Interrupt Descriptor Table for x86-32.
 */

#include "../../include/Nanokernel/nk_idt.h"
#include <stddef.h>

/* Serial logging */
extern void serial_puts(const char *s);
extern void nk_printf(const char *fmt, ...);

/* IDT table: 256 entries with guard patterns */
#define IDT_ENTRIES 256
#define IDT_GUARD_MAGIC 0xDEADBEEF

static uint32_t idt_guard_before = IDT_GUARD_MAGIC;
static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(16)));
static uint32_t idt_guard_after = IDT_GUARD_MAGIC;
static struct idt_ptr idtp;

/**
 * Set an IDT gate entry (hardened with explicit 32-bit math).
 */
void nk_idt_set_gate(uint8_t num, void (*handler)(void), uint16_t selector, uint8_t flags) {
    /* Force 32-bit arithmetic to prevent 64-bit compiler issues */
    uint32_t addr = (uint32_t)((uintptr_t)handler);

    idt[num].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[num].offset_high = (uint16_t)((addr >> 16) & 0xFFFF);
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

/**
 * Verify an IDT entry matches expected handler address.
 * Returns 0 if match, 1 if mismatch.
 */
int nk_idt_verify_entry(uint8_t vec, void *expected_handler) {
    uint32_t expected = (uint32_t)((uintptr_t)expected_handler);
    uint32_t actual = ((uint32_t)idt[vec].offset_high << 16) | idt[vec].offset_low;

    if (actual != expected) {
        nk_printf("[IDTCHK] Vector %u MISMATCH: actual=0x%08X expected=0x%08X\n",
                  vec, actual, expected);
        return 1;
    }
    return 0;
}

/**
 * Check IDT guard patterns for memory corruption.
 * Returns 0 if OK, 1 if corrupted.
 */
int nk_idt_check_guards(void) {
    int corrupted = 0;

    if (idt_guard_before != IDT_GUARD_MAGIC) {
        serial_puts("[IDTCHK] GUARD BEFORE corrupted!\n");
        corrupted = 1;
    }

    if (idt_guard_after != IDT_GUARD_MAGIC) {
        serial_puts("[IDTCHK] GUARD AFTER corrupted!\n");
        corrupted = 1;
    }

    return corrupted;
}

/*
 * gdt.c - x86 Global Descriptor Table setup
 *
 * Multiboot2 hands the kernel a machine in 32-bit protected mode but leaves
 * GDTR explicitly undefined - the spec requires the OS image to install its own
 * GDT before it relies on one. Until this ran, the kernel was borrowing GRUB's
 * temporary GDT, which lives in memory the kernel later allocates over. That
 * works right up until the first interrupt fires: the IDT gate's 0x08 selector
 * is resolved against a table that no longer exists, which raises #GP, then
 * #DF, then a triple fault (the machine silently resets).
 *
 * Layout matches the selectors the IDT and boot code already assume:
 *   0x00  null
 *   0x08  ring-0 code, base 0, limit 4 GiB, 32-bit
 *   0x10  ring-0 data, base 0, limit 4 GiB, 32-bit
 */

#include "gdt.h"

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity; /* high nibble = flags, low nibble = limit[19:16] */
    uint8_t  base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} gdt_ptr_t;

static gdt_entry_t g_gdt[3];
static gdt_ptr_t g_gdt_ptr;

static void gdt_set_gate(int index, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t flags) {
    g_gdt[index].base_low    = (uint16_t)(base & 0xFFFF);
    g_gdt[index].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    g_gdt[index].base_high   = (uint8_t)((base >> 24) & 0xFF);
    g_gdt[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    g_gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    g_gdt[index].access      = access;
}

void gdt_init(void) {
    /* 0x9A = present, ring 0, code, executable, readable
     * 0x92 = present, ring 0, data, writable
     * 0xC0 = 4 KiB granularity, 32-bit operand size */
    gdt_set_gate(0, 0, 0x00000, 0x00, 0x00);
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xC0);
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xC0);

    g_gdt_ptr.limit = (uint16_t)(sizeof(g_gdt) - 1);
    g_gdt_ptr.base  = (uint32_t)(uintptr_t)&g_gdt[0];

    /* Load the table, then reload every segment register. CS can only be
     * changed by a far transfer, so use a far return to our own label. */
    __asm__ volatile(
        "lgdt %0\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        :
        : "m"(g_gdt_ptr)
        : "eax", "memory");
}

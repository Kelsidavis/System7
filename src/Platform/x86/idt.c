/*
 * idt.c - x86 Interrupt Descriptor Table setup
 */

#include "idt.h"
#include "pic.h"

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);
extern void isr_default(void);

static idt_entry_t g_idt[256];
static idt_ptr_t g_idt_ptr;

static void idt_set_gate(uint8_t vector, void (*handler)(void)) {
    uintptr_t addr = (uintptr_t)handler;
    g_idt[vector].offset_low = (uint16_t)(addr & 0xFFFF);
    g_idt[vector].selector = 0x08; /* Kernel code segment */
    g_idt[vector].zero = 0;
    g_idt[vector].type_attr = 0x8E; /* present, ring 0, 32-bit interrupt gate */
    g_idt[vector].offset_high = (uint16_t)((addr >> 16) & 0xFFFF);
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, isr_default);
    }

    idt_set_gate(0x20, irq0);
    idt_set_gate(0x21, irq1);
    idt_set_gate(0x22, irq2);
    idt_set_gate(0x23, irq3);
    idt_set_gate(0x24, irq4);
    idt_set_gate(0x25, irq5);
    idt_set_gate(0x26, irq6);
    idt_set_gate(0x27, irq7);
    idt_set_gate(0x28, irq8);
    idt_set_gate(0x29, irq9);
    idt_set_gate(0x2A, irq10);
    idt_set_gate(0x2B, irq11);
    idt_set_gate(0x2C, irq12);
    idt_set_gate(0x2D, irq13);
    idt_set_gate(0x2E, irq14);
    idt_set_gate(0x2F, irq15);

    g_idt_ptr.limit = (uint16_t)(sizeof(g_idt) - 1);
    g_idt_ptr.base = (uint32_t)(uintptr_t)&g_idt[0];

    __asm__ volatile("lidt %0" : : "m"(g_idt_ptr));
}

void idt_enable_interrupts(void) {
    __asm__ volatile("sti");
}

void irq_dispatch(uint32_t irq) {
    pic_send_eoi((uint8_t)irq);
}

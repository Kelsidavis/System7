/*
 * idt.c - x86 Interrupt Descriptor Table setup
 */

#include "idt.h"
#include "pic.h"
#include "Platform/include/serial.h"

#include <stdint.h>

static irq_handler_t g_irq_handlers[16] = {0};

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

extern void exc0(void);  extern void exc1(void);  extern void exc2(void);
extern void exc3(void);  extern void exc4(void);  extern void exc5(void);
extern void exc6(void);  extern void exc7(void);  extern void exc8(void);
extern void exc9(void);  extern void exc10(void); extern void exc11(void);
extern void exc12(void); extern void exc13(void); extern void exc14(void);
extern void exc15(void); extern void exc16(void); extern void exc17(void);
extern void exc18(void); extern void exc19(void);

static void (*const g_exception_stubs[20])(void) = {
    exc0,  exc1,  exc2,  exc3,  exc4,  exc5,  exc6,  exc7,  exc8,  exc9,
    exc10, exc11, exc12, exc13, exc14, exc15, exc16, exc17, exc18, exc19
};

static const char *const g_exception_names[20] = {
    "divide error", "debug", "NMI", "breakpoint",
    "overflow", "BOUND range exceeded", "invalid opcode", "device not available",
    "double fault", "coprocessor segment overrun", "invalid TSS",
    "segment not present", "stack-segment fault", "general protection fault",
    "page fault", "reserved", "x87 FP exception", "alignment check",
    "machine check", "SIMD FP exception"
};

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

    /* Real handlers for the CPU exceptions so a fault is reported rather than
     * silently triple-faulting the machine. */
    for (int i = 0; i < 20; i++) {
        idt_set_gate((uint8_t)i, g_exception_stubs[i]);
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

/* Emit an 8-digit hex value without going through serial_printf.
 * A crash report that can be silenced by a log-level filter is worthless, and
 * serial_printf runs the whole SystemLog classification path - far too much
 * machinery to trust from inside a fault handler. */
static void emit_hex32(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = digits[(value >> ((7 - i) * 4)) & 0xF];
    }
    buf[10] = '\0';
    serial_puts(buf);
}

void exception_dispatch(uint32_t vector, uint32_t error_code, uint32_t eip) {
    serial_puts("\n*** CPU EXCEPTION: ");
    serial_puts((vector < 20) ? g_exception_names[vector] : "unknown");
    serial_puts(" (vector ");
    emit_hex32(vector);
    serial_puts(")\n    eip=");
    emit_hex32(eip);
    serial_puts(" err=");
    emit_hex32(error_code);

    if (vector == 14) { /* page fault - CR2 holds the offending address */
        uint32_t cr2 = 0;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_puts(" cr2=");
        emit_hex32(cr2);
    }

    /* Not recoverable: returning would re-execute the faulting instruction and
     * livelock. Halt so the report above survives instead of being buried under
     * a triple-fault reset. */
    serial_puts("\n    halted.\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void irq_dispatch(uint32_t irq) {
    static uint32_t irq_counts[16] = {0};
    if (irq < 16) {
        irq_counts[irq]++;
        if (irq_counts[irq] <= 5 || (irq_counts[irq] % 1000u) == 0) {
            serial_puts("[IRQ] line active\n");
        }
    }
    if (irq < 16 && g_irq_handlers[irq]) {
        g_irq_handlers[irq]((uint8_t)irq);
    }
    pic_send_eoi((uint8_t)irq);
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        g_irq_handlers[irq] = handler;
    }
}

void irq_unregister_handler(uint8_t irq) {
    if (irq < 16) {
        g_irq_handlers[irq] = 0;
    }
}

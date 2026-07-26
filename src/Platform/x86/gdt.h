/*
 * gdt.h - x86 Global Descriptor Table setup
 */

#ifndef PLATFORM_X86_GDT_H
#define PLATFORM_X86_GDT_H

/* Install the kernel's own GDT and reload all segment registers.
 * Must run before idt_init() - the IDT's gate selectors are resolved against
 * whatever GDT is live when an interrupt fires. */
void gdt_init(void);

#endif /* PLATFORM_X86_GDT_H */

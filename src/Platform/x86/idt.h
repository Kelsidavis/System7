#ifndef X86_IDT_H
#define X86_IDT_H

#include <stdint.h>

void idt_init(void);
void idt_enable_interrupts(void);
void irq_dispatch(uint32_t irq);

#endif /* X86_IDT_H */

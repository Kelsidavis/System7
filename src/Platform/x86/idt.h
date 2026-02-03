#ifndef X86_IDT_H
#define X86_IDT_H

#include <stdint.h>

void idt_init(void);
void idt_enable_interrupts(void);
void irq_dispatch(uint32_t irq);
typedef void (*irq_handler_t)(uint8_t irq);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_unregister_handler(uint8_t irq);

#endif /* X86_IDT_H */

/* nk_pic.c - System 7X Nanokernel PIC Driver (8259A) Implementation (C23)
 *
 * Manages the 8259A Programmable Interrupt Controller.
 */

#include "../../include/Nanokernel/nk_pic.h"

/* I/O port access from platform layer */
extern void hal_outb(uint16_t port, uint8_t value);
extern uint8_t hal_inb(uint16_t port);

/* Convenience wrappers to match expected names */
static inline void outb(uint16_t port, uint8_t val) { hal_outb(port, val); }
static inline uint8_t inb(uint16_t port) { return hal_inb(port); }

/* Serial logging */
extern void serial_puts(const char *s);

/**
 * Initialize and remap the PIC.
 *
 * Standard PC IRQ mapping (before remap):
 *   IRQ 0-7  -> INT 0x08-0x0F (conflicts with CPU exceptions!)
 *   IRQ 8-15 -> INT 0x70-0x77
 *
 * After remap:
 *   IRQ 0-7  -> INT 0x20-0x27
 *   IRQ 8-15 -> INT 0x28-0x2F
 */
void nk_pic_remap(void) {
    uint8_t mask1, mask2;

    /* Save current masks */
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (ICW1) */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, IRQ_OFFSET);       /* Master PIC offset: 0x20 */
    outb(PIC2_DATA, IRQ_OFFSET + 8);   /* Slave PIC offset: 0x28 */

    /* ICW3: Tell master about slave at IRQ2 */
    outb(PIC1_DATA, 0x04);  /* Slave on IRQ2 (bit 2) */
    outb(PIC2_DATA, 0x02);  /* Slave identity (cascade identity = 2) */

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    serial_puts("[nk_pic] PIC remapped to vectors 0x20-0x2F\n");
}

/**
 * Send End Of Interrupt to the PIC.
 */
void nk_pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        /* Send EOI to slave PIC */
        outb(PIC2_CMD, PIC_EOI);
    }
    /* Always send EOI to master PIC */
    outb(PIC1_CMD, PIC_EOI);
}

/**
 * Mask (disable) an IRQ line.
 */
void nk_pic_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}

/**
 * Unmask (enable) an IRQ line.
 */
void nk_pic_unmask(uint8_t irq) {
    serial_puts("[PIC] unmask: entry\n");

    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
        serial_puts("[PIC] unmask: IRQ<8, using master PIC\n");
    } else {
        port = PIC2_DATA;
        irq -= 8;
        serial_puts("[PIC] unmask: IRQ>=8, using slave PIC\n");
    }

    serial_puts("[PIC] unmask: about to read mask\n");
    value = inb(port) & ~(1 << irq);
    serial_puts("[PIC] unmask: about to write mask\n");
    outb(port, value);
    serial_puts("[PIC] unmask: exit\n");
}

/**
 * Disable all IRQs by masking them.
 */
void nk_pic_disable_all(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

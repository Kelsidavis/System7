/* nk_pic.h - System 7X Nanokernel PIC Driver (8259A) Header (C23)
 *
 * Programmable Interrupt Controller (8259A) driver for x86.
 * Handles IRQ remapping, masking, and EOI (End Of Interrupt).
 */

#ifndef NK_PIC_H
#define NK_PIC_H

#include <stdint.h>
#include <stdbool.h>

/* PIC I/O ports */
#define PIC1_CMD   0x20    /* Master PIC command port */
#define PIC1_DATA  0x21    /* Master PIC data port */
#define PIC2_CMD   0xA0    /* Slave PIC command port */
#define PIC2_DATA  0xA1    /* Slave PIC data port */

/* PIC commands */
#define PIC_EOI    0x20    /* End of interrupt command */

/* ICW1 flags */
#define ICW1_ICW4      0x01    /* ICW4 needed */
#define ICW1_SINGLE    0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL     0x08    /* Level triggered mode */
#define ICW1_INIT      0x10    /* Initialization required */

/* ICW4 flags */
#define ICW4_8086      0x01    /* 8086/88 mode */
#define ICW4_AUTO      0x02    /* Auto EOI */
#define ICW4_BUF_SLAVE 0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C   /* Buffered mode/master */
#define ICW4_SFNM      0x10    /* Special fully nested mode */

/* IRQ offset in IDT (remapped vectors) */
#define IRQ_OFFSET 32

/**
 * Initialize and remap the PIC.
 * Remaps IRQs 0-15 to IDT vectors 32-47.
 */
void nk_pic_remap(void);

/**
 * Send End Of Interrupt to the PIC.
 * @param irq IRQ number (0-15)
 */
void nk_pic_send_eoi(uint8_t irq);

/**
 * Mask (disable) an IRQ line.
 * @param irq IRQ number (0-15)
 */
void nk_pic_mask(uint8_t irq);

/**
 * Unmask (enable) an IRQ line.
 * @param irq IRQ number (0-15)
 */
void nk_pic_unmask(uint8_t irq);

/**
 * Disable all IRQs by masking them.
 */
void nk_pic_disable_all(void);

#endif /* NK_PIC_H */

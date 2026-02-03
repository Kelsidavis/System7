/*
 * pic.c - 8259 PIC initialization and control
 */

#include "pic.h"
#include "Platform/include/io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

static inline void io_wait(void) {
    /* Port 0x80 is used for 'checkpoints' during POST. */
    hal_outb(0x80, 0);
}

void pic_init(void) {
    uint8_t mask1 = hal_inb(PIC1_DATA);
    uint8_t mask2 = hal_inb(PIC2_DATA);

    /* Start initialization sequence (ICW1) */
    hal_outb(PIC1_COMMAND, 0x11);
    io_wait();
    hal_outb(PIC2_COMMAND, 0x11);
    io_wait();

    /* ICW2: Remap offsets to 0x20 and 0x28 */
    hal_outb(PIC1_DATA, 0x20);
    io_wait();
    hal_outb(PIC2_DATA, 0x28);
    io_wait();

    /* ICW3: Tell Master PIC that Slave PIC is at IRQ2 (0000 0100) */
    hal_outb(PIC1_DATA, 0x04);
    io_wait();
    /* ICW3: Tell Slave PIC its cascade identity (0000 0010) */
    hal_outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086/88 mode */
    hal_outb(PIC1_DATA, 0x01);
    io_wait();
    hal_outb(PIC2_DATA, 0x01);
    io_wait();

    /* Restore saved masks */
    hal_outb(PIC1_DATA, mask1);
    hal_outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        hal_outb(PIC2_COMMAND, PIC_EOI);
    }
    hal_outb(PIC1_COMMAND, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = hal_inb(port);
    value |= (1u << (irq & 0x7));
    hal_outb(port, value);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = hal_inb(port);
    value &= (uint8_t)~(1u << (irq & 0x7));
    hal_outb(port, value);
}

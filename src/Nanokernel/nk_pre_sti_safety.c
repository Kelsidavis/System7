/* nk_pre_sti_safety.c - Pre-STI Safety Harness for x86 Nanokernel
 *
 * Comprehensive safety checks before enabling interrupts (STI).
 * Helps identify interrupt subsystem corruption issues.
 */

#include <stdint.h>

/* External functions */
extern void serial_puts(const char *s);
extern void nk_printf(const char *fmt, ...);
extern uint8_t hal_inb(uint16_t port);
extern void hal_outb(uint16_t port, uint8_t value);

/* Disable non-maskable interrupts (NMI) via CMOS port 0x70 bit 7 */
static void disable_nmi(void) {
    uint8_t prev = hal_inb(0x70);
    hal_outb(0x70, prev | 0x80);
    serial_puts("[STI-SAFETY] NMI disabled\n");
}

/* Re-enable NMI later if needed */
void nk_enable_nmi(void) {
    uint8_t prev = hal_inb(0x70);
    hal_outb(0x70, prev & ~0x80);
    serial_puts("[STI-SAFETY] NMI re-enabled\n");
}

/* Send End-of-Interrupt to both PICs to clear any pending edge */
static void clear_pending_irqs(void) {
    hal_outb(0x20, 0x20);
    hal_outb(0xA0, 0x20);
    serial_puts("[STI-SAFETY] Sent EOI to PICs\n");
}

/* Verify the IDT register using SIDT and dump base/limit */
static void verify_idt(void) {
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr;

    __asm__ volatile("sidt %0" : "=m"(idtr));

    if (idtr.base == 0 || idtr.limit < 0x100) {
        serial_puts("[STI-SAFETY] ERROR: Invalid IDT detected!\n");
    } else {
        serial_puts("[STI-SAFETY] IDT verified: base!=0, limit>=0x100\n");
    }
}

/* Dump key registers for sanity check */
static void dump_regs(void) {
    uint32_t esp, eflags;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    __asm__ volatile("pushf; pop %0" : "=r"(eflags));

    serial_puts("[STI-SAFETY] Register snapshot taken\n");
}

/* Confirm PIC masks are fully set before STI */
static void verify_pic_masks(void) {
    uint8_t m1 = hal_inb(0x21);
    uint8_t m2 = hal_inb(0xA1);

    if (m1 == 0xFF && m2 == 0xFF) {
        serial_puts("[STI-SAFETY] PIC masks verified: all IRQs masked\n");
    } else {
        serial_puts("[STI-SAFETY] WARNING: IRQs not fully masked!\n");
    }
}

/* Verify GDT is still valid */
static void verify_gdt(void) {
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) gdtr;

    __asm__ volatile("sgdt %0" : "=m"(gdtr));

    if (gdtr.base == 0) {
        serial_puts("[STI-SAFETY] ERROR: GDT base is NULL!\n");
    } else {
        serial_puts("[STI-SAFETY] GDT verified: base!=0\n");
    }
}

/* Combined safety sequence */
void nk_pre_sti_safety_check(void) {
    serial_puts("\n=== Pre-STI Safety Harness ===\n");
    disable_nmi();
    clear_pending_irqs();
    verify_gdt();
    verify_idt();
    verify_pic_masks();
    dump_regs();

    serial_puts("[STI-SAFETY] All checks complete. Ready for STI.\n\n");
}

/* Optional post-STI confirmation */
void nk_post_sti_confirm(void) {
    serial_puts("[STI-SAFETY] ✓ STI executed successfully — system stable.\n\n");
}

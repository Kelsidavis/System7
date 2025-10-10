/* nk_platform_init.c - x86 Platform-Specific Nanokernel Initialization
 *
 * Handles x86-specific setup: IDT, PIC, PIT timer.
 */

#include "../../../../include/Nanokernel/nk_idt.h"
#include "../../../../include/Nanokernel/nk_pic.h"
#include "../../../../include/Nanokernel/nk_timer.h"

/* External declarations */
extern void nk_printf(const char *fmt, ...);

/**
 * Initialize x86 platform-specific nanokernel components.
 *
 * This function sets up:
 * - Interrupt Descriptor Table (IDT)
 * - Programmable Interrupt Controller (PIC)
 * - Programmable Interval Timer (PIT)
 *
 * Must be called before enabling interrupts.
 */
void nk_platform_init(void) {
    nk_printf("[PLATFORM] Initializing x86 nanokernel components...\n");

    // Initialize IDT
    nk_idt_install();
    nk_printf("[PLATFORM] IDT initialized\n");

    // Initialize PIC
    nk_pic_remap();
    nk_printf("[PLATFORM] PIC initialized\n");

    nk_printf("[PLATFORM] x86 nanokernel initialization complete\n");
}

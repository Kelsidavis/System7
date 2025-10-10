/* nk_isr.c - System 7X Nanokernel ISR Handlers (C23)
 *
 * C handlers for hardware interrupts.
 */

#include "../../include/Nanokernel/nk_pic.h"
#include "../../include/Nanokernel/nk_timer.h"

/* Serial logging */
extern void serial_puts(const char *s);

/* Track timer ticks for debugging (32-bit is sufficient - wraps after ~49 days at 1000 Hz) */
static volatile uint32_t debug_tick_count = 0;

/**
 * IRQ0 handler - Timer interrupt (1000 Hz)
 * Called by irq0_stub in nk_isr.S
 */
void irq0_handler(void) {
    extern void nk_request_reschedule(void);
    extern void serial_puts(const char *s);

    debug_tick_count++;

    /* Debug: Print first few ticks to verify handler is called */
    if (debug_tick_count <= 5) {
        serial_puts("[IRQ0] Handler called\n");
    }

    /* Debug: Print tick count every 1000 ticks (once per second) */
    if (debug_tick_count % 1000 == 0) {
        extern void nk_printf(const char *fmt, ...);
        nk_printf("[IRQ0] Timer tick %u\n", (unsigned)debug_tick_count);
    }

    /* Call timer tick handler (updates tick counter, wakes threads) */
    nk_timer_tick();

    /* Request deferred reschedule - scheduler will run in safe context */
    nk_request_reschedule();

    /* Send EOI to PIC */
    nk_pic_send_eoi(0);
}

/**
 * IRQ1 handler - Keyboard interrupt (for future use)
 * Called by irq1_stub in nk_isr.S
 */
void irq1_handler(void) {
    /* TODO: Handle keyboard input */

    /* Send EOI to PIC */
    nk_pic_send_eoi(1);
}

/**
 * IRQ7 handler - Spurious interrupt
 * Called by irq7_stub in nk_isr.S
 */
void irq7_handler(void) {
    /* Spurious IRQ7 - don't send EOI */
    serial_puts("[nk_isr] Spurious IRQ7 detected\n");
}

/**
 * Get current timer tick count (for debugging).
 */
uint32_t nk_isr_get_tick_count(void) {
    return debug_tick_count;
}

/**
 * Software interrupt handler for deferred rescheduling (INT 0x81).
 * Called by irq_resched_stub in nk_isr.S
 *
 * This handler runs the scheduler in a safe context, not directly
 * from the hardware timer interrupt.
 */
void irq_resched_handler(void) {
    extern void nk_printf(const char *fmt, ...);
    extern void nk_clear_reschedule(void);
    extern void nk_schedule(void);

    nk_printf("[INT81] Deferred scheduler invoked\n");
    nk_clear_reschedule();
    nk_schedule();
}

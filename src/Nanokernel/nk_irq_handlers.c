/* nk_isr.c - System 7X Nanokernel ISR Handlers (C23)
 *
 * C handlers for hardware interrupts.
 */

#include "../../include/Nanokernel/nk_pic.h"
#include "../../include/Nanokernel/nk_timer.h"
#include "../../include/Nanokernel/nk_thread.h"
#include <stdatomic.h>

/* Serial logging */
extern void serial_puts(const char *s);

/* Interrupt context tracking (defined in nk_sched.c) */
extern _Atomic bool nk_in_interrupt;
extern nk_interrupt_frame_t *nk_current_frame;

/* Track timer ticks for debugging (32-bit is sufficient - wraps after ~49 days at 1000 Hz) */
static volatile uint32_t debug_tick_count = 0;

/**
 * IRQ0 handler - Timer interrupt (100 Hz)
 * Called by irq0_stub in nk_isr.S with interrupt frame pointer.
 *
 * @param frame  Pointer to saved interrupt frame on stack
 */
void irq0_handler(nk_interrupt_frame_t *frame) {
    (void)frame;  /* Unused - timer tick doesn't need frame */

    /* Diagnostic: Print to serial on first IRQ */
    static int first_irq = 1;
    if (first_irq) {
        first_irq = 0;
        serial_puts("[IRQ0] First timer interrupt!\n");
    }

    /* Send EOI to PIC before processing */
    nk_pic_send_eoi(0);

    /* Increment debug tick count */
    debug_tick_count++;

    /* Call timer tick handler - increments system ticks, wakes sleeping threads, triggers scheduling */
    nk_timer_tick();
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

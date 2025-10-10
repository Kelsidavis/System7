/* nk_timer.h - System 7X Nanokernel Timer Subsystem Header (C23)
 *
 * Timer tick handling and sleep management.
 */

#ifndef NK_TIMER_H
#define NK_TIMER_H

#include <stdint.h>
#include "nk_thread.h"

/* Timer frequency in Hz - reduced to 100 Hz for testing IRQ-safe context switch */
#define NK_TIMER_HZ 100

/* PIT (Programmable Interval Timer) base frequency */
#define PIT_BASE_HZ 1193182

/**
 * Initialize timer subsystem.
 * Programs PIT to generate interrupts at NK_TIMER_HZ.
 */
void nk_timer_init(void);

/**
 * Timer tick handler - called from IRQ0 handler.
 * Increments tick counter, wakes sleeping threads, triggers scheduling.
 */
void nk_timer_tick(void);

/**
 * Get current system ticks (milliseconds since boot).
 */
uint64_t nk_get_ticks(void);

/**
 * Put thread to sleep for specified milliseconds.
 * Thread is added to sleep queue and removed from ready queue.
 *
 * @param thread Thread to sleep
 * @param millis Milliseconds to sleep
 */
void nk_sleep_until(nk_thread_t *thread, uint64_t millis);

#endif /* NK_TIMER_H */

/* nk_resched.c - System 7X Nanokernel Deferred Rescheduling (C23)
 *
 * Implements safe interrupt-driven rescheduling using atomic flags
 * and optional software interrupts.
 */

#include "../../include/Nanokernel/nk_resched.h"
#include "../../include/Nanokernel/nk_sched.h"

/* External printf for debugging */
extern void nk_printf(const char *fmt, ...);

/* ============================================================
 *   Reschedule State
 * ============================================================ */

/* Atomic flag indicating reschedule is pending */
static _Atomic bool resched_flag = false;

/* Statistics */
static _Atomic uint64_t resched_requests = 0;
static _Atomic uint64_t resched_serviced = 0;

/* ============================================================
 *   Deferred Rescheduling API
 * ============================================================ */

/**
 * Request a deferred reschedule.
 * Called from interrupt context (timer, etc).
 *
 * NOTE: Do NOT trigger INT 0x81 from here! It would create nested interrupts
 * which corrupt the stack during context switching. Instead, the idle thread
 * polls this flag and calls nk_schedule() in safe context.
 */
void nk_request_reschedule(void) {
    atomic_store_explicit(&resched_flag, true, memory_order_release);
    atomic_fetch_add_explicit(&resched_requests, 1, memory_order_relaxed);
}

/**
 * Check if reschedule is pending.
 */
bool nk_reschedule_pending(void) {
    return atomic_load_explicit(&resched_flag, memory_order_acquire);
}

/**
 * Clear reschedule flag.
 */
void nk_clear_reschedule(void) {
    atomic_store_explicit(&resched_flag, false, memory_order_release);
    atomic_fetch_add_explicit(&resched_serviced, 1, memory_order_relaxed);
}

/**
 * Trigger software interrupt for deferred rescheduling.
 * Uses INT 0x81 to invoke scheduler in safe context.
 */
void nk_trigger_software_resched(void) {
    __asm__ volatile("int $0x81");
}

/**
 * Get reschedule statistics (for debugging).
 */
void nk_resched_stats(uint64_t *requests, uint64_t *serviced) {
    if (requests) {
        *requests = atomic_load_explicit(&resched_requests, memory_order_relaxed);
    }
    if (serviced) {
        *serviced = atomic_load_explicit(&resched_serviced, memory_order_relaxed);
    }
}

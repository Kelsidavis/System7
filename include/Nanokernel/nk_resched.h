/* nk_resched.h - System 7X Nanokernel Deferred Rescheduling (C23)
 *
 * Provides safe, interrupt-driven rescheduling without performing
 * context switches directly inside interrupt handlers.
 */

#ifndef NK_RESCHED_H
#define NK_RESCHED_H

#include <stdatomic.h>
#include <stdbool.h>

/* ============================================================
 *   Deferred Rescheduling API
 * ============================================================ */

/**
 * Request a deferred reschedule.
 * Called from interrupt context (timer, etc).
 * Sets atomic flag to indicate scheduler should run.
 */
void nk_request_reschedule(void);

/**
 * Check if reschedule is pending.
 * Returns true if nk_request_reschedule() was called.
 */
[[nodiscard]] bool nk_reschedule_pending(void);

/**
 * Clear reschedule flag.
 * Called after servicing the reschedule request.
 */
void nk_clear_reschedule(void);

/**
 * Trigger software interrupt for deferred rescheduling.
 * Uses INT 0x81 to invoke scheduler in safe context.
 * Optional: can poll in idle loop instead.
 */
void nk_trigger_software_resched(void);

#endif /* NK_RESCHED_H */

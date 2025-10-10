/* nk_sched.c - System 7X Nanokernel Scheduler Implementation (C23)
 *
 * Preemptive, priority-based round-robin scheduler.
 */

#include "../../include/Nanokernel/nk_sched.h"
#include "../../include/Nanokernel/nk_task.h"
#include "../../include/Nanokernel/nk_memory.h"
#include "../../include/Nanokernel/nk_stats.h"
#include <stdatomic.h>

/* External context switch from nk_context.S */
extern void nk_switch_context(nk_context_t *old, nk_context_t *new);
extern void nk_switch_context_irq(nk_thread_t *prev, nk_thread_t *next, nk_interrupt_frame_t *prev_frame);

/* External printf for debugging */
extern void nk_printf(const char *fmt, ...);

/* ============================================================
 *   Interrupt Context Detection
 * ============================================================ */

/* Tracks whether we're currently executing in interrupt context */
_Atomic bool nk_in_interrupt = false;

/* Points to the current interrupt frame when in interrupt context */
nk_interrupt_frame_t *nk_current_frame = nullptr;

/* ============================================================
 *   Scheduler State
 * ============================================================ */

/* Ready queue (simple linked list for now) */
static nk_thread_t *ready_queue_head = nullptr;
static nk_thread_t *ready_queue_tail = nullptr;

/* Current running thread (per-CPU, single CPU for now) */
static nk_thread_t *current_thread = nullptr;

/* Idle thread */
static nk_thread_t *idle_thread = nullptr;

/* Scheduler lock */
static nk_spinlock_t sched_lock = { .locked = 0 };

/* Statistics */
static uint32_t ready_count = 0;

/* ============================================================
 *   Idle Thread
 * ============================================================ */

static void idle_thread_entry(void *arg) {
    extern bool nk_reschedule_pending(void);
    extern void nk_clear_reschedule(void);
    extern void serial_puts(const char *s);

    (void)arg;
    serial_puts("[IDLE] Idle thread started\n");

    for (;;) {
        // Check for deferred reschedule requests from timer interrupt
        if (nk_reschedule_pending()) {
            serial_puts("[IDLE] Reschedule pending\n");
            nk_clear_reschedule();
            nk_schedule();
        }

        // Halt CPU until next interrupt
        serial_puts("[IDLE] Halting CPU\n");
        __asm__ volatile("hlt");
        serial_puts("[IDLE] Resumed from HLT\n");
    }
}

/* ============================================================
 *   Scheduler Implementation
 * ============================================================ */

/**
 * Initialize scheduler subsystem.
 */
void nk_sched_init(void) {
    // Initialize performance instrumentation
    nk_stats_init();

    // Create idle task and thread
    auto idle_task = nk_task_create();
    if (!idle_task) {
        nk_printf("[SCHED] Failed to create idle task\n");
        return;
    }

    // Create idle thread
    idle_thread = nk_thread_create(
        idle_task,
        idle_thread_entry,
        nullptr,
        4096,  // Small stack for idle
        NK_IDLE_PRIORITY
    );

    if (!idle_thread) {
        nk_printf("[SCHED] Failed to create idle thread\n");
        return;
    }

    // Remove idle from ready queue - we'll schedule it manually
    nk_sched_remove_thread(idle_thread);

    nk_printf("[SCHED] Scheduler initialized\n");
}

/**
 * Add thread to ready queue (priority-sorted).
 */
void nk_sched_add_thread(nk_thread_t *thread) {
    if (!thread || thread->state != NK_THREAD_READY) {
        return;
    }

    nk_spinlock_acquire(&sched_lock);

    // Simple FIFO for now - insert at tail
    thread->next = nullptr;
    thread->prev = ready_queue_tail;

    if (ready_queue_tail) {
        ready_queue_tail->next = thread;
    } else {
        ready_queue_head = thread;
    }

    ready_queue_tail = thread;
    ready_count++;

    nk_spinlock_release(&sched_lock);
}

/**
 * Remove thread from ready queue.
 */
void nk_sched_remove_thread(nk_thread_t *thread) {
    if (!thread) {
        return;
    }

    nk_spinlock_acquire(&sched_lock);

    // Unlink from ready queue
    if (thread->prev) {
        thread->prev->next = thread->next;
    } else {
        ready_queue_head = thread->next;
    }

    if (thread->next) {
        thread->next->prev = thread->prev;
    } else {
        ready_queue_tail = thread->prev;
    }

    thread->next = nullptr;
    thread->prev = nullptr;

    if (ready_count > 0) {
        ready_count--;
    }

    nk_spinlock_release(&sched_lock);
}

/**
 * Select next thread to run (round-robin).
 */
static nk_thread_t *select_next_thread(void) {
    nk_spinlock_acquire(&sched_lock);

    nk_thread_t *next = ready_queue_head;

    // If no ready threads, use idle
    if (!next) {
        nk_spinlock_release(&sched_lock);
        return idle_thread;
    }

    // Remove from head of ready queue
    ready_queue_head = next->next;
    if (ready_queue_head) {
        ready_queue_head->prev = nullptr;
    } else {
        ready_queue_tail = nullptr;
    }

    next->next = nullptr;
    next->prev = nullptr;

    if (ready_count > 0) {
        ready_count--;
    }

    nk_spinlock_release(&sched_lock);

    return next;
}

/**
 * Main scheduler - select and switch to next thread.
 */
void nk_schedule(void) {
    nk_thread_t *prev = current_thread;
    nk_thread_t *next = select_next_thread();

    if (!next) {
        next = idle_thread;
    }

    // If current thread is still runnable, put it back in ready queue
    if (prev && prev != idle_thread && prev->state == NK_THREAD_RUNNING) {
        prev->state = NK_THREAD_READY;
        nk_sched_add_thread(prev);
    }

    // Mark next thread as running
    next->state = NK_THREAD_RUNNING;
    current_thread = next;

    // Record context switch for performance instrumentation
    nk_stats_record_switch(prev, next);

    // Context switch if different thread
    if (prev != next) {
        extern void serial_puts(const char *s);

        // Check if we're in interrupt context
        bool in_irq = atomic_load_explicit(&nk_in_interrupt, memory_order_acquire);

        if (in_irq && prev) {
            // IRQ-safe context switch (uses IRET)
            // NOTE: Only valid when switching between threads, not from kernel to first thread
            // Cannot use serial_puts/nk_printf from IRQ context (not reentrant)

            // Placeholder for page table switch
            if (prev && next && prev->task != next->task) {
                // Future: load_page_table(next->task->page_table_root);
            }

            // Perform IRQ-safe context switch
            nk_switch_context_irq(prev, next, nk_current_frame);

            // NOTE: We never return here! The context switch returns via IRET
            // to the new thread's saved context.
        } else {
            // Regular cooperative context switch (uses RET)
            // Safe to use serial I/O here (not in IRQ context)
            extern void serial_puts(const char *s);
            if (!prev) {
                serial_puts("[SCHED] First context switch to thread\n");
            }

            // Placeholder for page table switch
            if (prev && next && prev->task != next->task) {
                // Future: load_page_table(next->task->page_table_root);
            }

            // Perform regular context switch
            serial_puts("[SCHED] About to call nk_switch_context\n");
            if (prev) {
                nk_switch_context(&prev->context, &next->context);
            } else {
                // First time - just jump to thread
                nk_switch_context(nullptr, &next->context);
            }
            serial_puts("[SCHED] Returned from nk_switch_context\n");
        }
    }
}

/**
 * Get scheduler statistics.
 */
void nk_sched_stats(uint32_t *ready_cnt, uint32_t *running_cnt) {
    if (ready_cnt) {
        *ready_cnt = ready_count;
    }
    if (running_cnt) {
        *running_cnt = current_thread ? 1 : 0;
    }
}

/**
 * Get current thread (external access).
 */
nk_thread_t *nk_thread_current(void) {
    return current_thread;
}

/**
 * Set current thread (internal use).
 */
void nk_thread_set_current(nk_thread_t *thread) {
    current_thread = thread;
}

/**
 * Scheduler tick handler - called from timer interrupt.
 *
 * Handles timer-driven preemption.
 * This is the entry point from the timer interrupt handler.
 */
void nk_sched_tick(void) {
    // Update global tick counter for performance instrumentation
    nk_stats_tick();

    // Don't try to schedule if no thread is currently running
    // (can't launch first thread from IRQ context)
    nk_thread_t *curr = current_thread;
    if (!curr) {
        // Silent return - can't use serial_puts from IRQ context
        return;
    }

    // Future enhancements:
    // - Decrement time slices
    // - Handle deadline scheduling

    // Trigger preemptive reschedule
    nk_schedule();
}

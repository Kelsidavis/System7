/* nk_sched.h - System 7X Nanokernel Scheduler (C23)
 *
 * Preemptive, priority-based round-robin scheduler.
 * SMP-ready design with per-CPU current thread tracking.
 */

#pragma once

#include <stdint.h>
#include <stdatomic.h>
#include "nk_thread.h"

/* ============================================================
 *   Scheduler Constants
 * ============================================================ */

#define NK_MAX_PRIORITY 256            // Priority levels: 0-255
#define NK_DEFAULT_PRIORITY 128        // Default thread priority
#define NK_IDLE_PRIORITY 0             // Idle thread priority

/* ============================================================
 *   Spinlock (SMP Placeholder)
 * ============================================================ */

/**
 * Spinlock for SMP synchronization (stubbed for single CPU).
 */
typedef struct {
    _Atomic uint32_t locked;
} nk_spinlock_t;

/**
 * Initialize a spinlock.
 *
 * @param lock  Spinlock to initialize
 */
static inline void nk_spinlock_init(nk_spinlock_t *lock) {
    atomic_store_explicit(&lock->locked, 0, memory_order_relaxed);
}

/**
 * Acquire a spinlock.
 *
 * @param lock  Spinlock to acquire
 */
static inline void nk_spinlock_acquire(nk_spinlock_t *lock) {
    // Stub for single CPU - no actual locking needed
    (void)lock;
}

/**
 * Release a spinlock.
 *
 * @param lock  Spinlock to release
 */
static inline void nk_spinlock_release(nk_spinlock_t *lock) {
    // Stub for single CPU
    (void)lock;
}

/* ============================================================
 *   Scheduler API
 * ============================================================ */

/**
 * Initialize the scheduler subsystem.
 *
 * Sets up ready queues and creates idle thread.
 */
void nk_sched_init(void);

/**
 * Schedule next thread to run.
 *
 * Selects highest priority READY thread and context switches to it.
 * Called from timer interrupt or explicit yield.
 */
void nk_schedule(void);

/**
 * Scheduler tick handler - called from timer interrupt.
 *
 * Handles timer-driven scheduling (preemption).
 * This function is called by nk_timer_tick().
 */
void nk_sched_tick(void);

/**
 * Add a thread to the ready queue.
 *
 * @param thread  Thread to add (must be in READY state)
 */
void nk_sched_add_thread(nk_thread_t *thread);

/**
 * Remove a thread from the ready queue.
 *
 * @param thread  Thread to remove
 */
void nk_sched_remove_thread(nk_thread_t *thread);

/**
 * Get scheduler statistics.
 *
 * @param ready_count    Output: number of ready threads
 * @param running_count  Output: number of running threads
 */
void nk_sched_stats(uint32_t *ready_count, uint32_t *running_count);

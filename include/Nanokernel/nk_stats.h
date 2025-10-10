/* nk_stats.h - Nanokernel Performance Instrumentation (C23)
 *
 * Lightweight profiling for scheduler telemetry and fairness analysis.
 * Measures per-thread CPU usage, context switches, and quantum statistics.
 *
 * Thread-safe under preemption (atomic operations not required for stats).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration */
typedef struct nk_thread nk_thread_t;

/**
 * Per-thread statistics (embedded in nk_thread_t).
 */
typedef struct {
    uint64_t context_switches;    /* Number of times this thread was scheduled */
    uint64_t cpu_ticks;            /* Total CPU ticks consumed by this thread */
    uint64_t last_scheduled_tick;  /* Tick count when thread was last scheduled in */
} nk_thread_stats_t;

/**
 * Global scheduler statistics.
 */
typedef struct {
    uint64_t total_context_switches;  /* Total scheduler invocations */
    uint64_t total_cpu_ticks;          /* Total ticks since scheduler started */
    uint64_t start_tick;               /* Tick count when scheduler started */
} nk_global_stats_t;

/**
 * Initialize the statistics subsystem.
 * Must be called before scheduler starts.
 */
void nk_stats_init(void);

/**
 * Record a context switch from prev to next.
 * Called by scheduler during every context switch.
 *
 * @param prev  Previous thread (may be NULL)
 * @param next  Next thread (must not be NULL)
 */
void nk_stats_record_switch(nk_thread_t *prev, nk_thread_t *next);

/**
 * Update tick count (called from timer IRQ).
 * Must be called on every timer tick.
 */
void nk_stats_tick(void);

/**
 * Get current tick count.
 * @return Current global tick count
 */
uint64_t nk_stats_get_ticks(void);

/**
 * Dump comprehensive scheduler statistics to serial console.
 * Shows per-thread CPU usage, context switches, and fairness metrics.
 */
void nk_debug_dump_stats(void);

/**
 * Reset all statistics counters to zero.
 * Useful for benchmarking specific workloads.
 */
void nk_stats_reset(void);

/* nk_debug.h - System 7X Nanokernel Debug Tools (C23)
 *
 * Stack visualization, overflow detection, and performance instrumentation.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nk_thread.h"

/* ============================================================
 *   Stack Guard Canary
 * ============================================================ */

#define NK_STACK_CANARY 0xCAFECAFE

/* ============================================================
 *   Stack Visualization API
 * ============================================================ */

/**
 * Dump stack usage for a single thread.
 *
 * Shows:
 * - Stack base address
 * - Current ESP
 * - Used bytes and percentage
 * - Canary status (if present)
 *
 * @param thread  Thread to analyze (NULL for current)
 */
void nk_debug_dump_stack(nk_thread_t *thread);

/**
 * Check all thread stacks for overflow and report usage.
 *
 * Walks all tasks and threads, checking:
 * - Guard canary integrity
 * - Stack usage percentage
 * - Warnings for >75% usage
 * - Errors for overflow
 */
void nk_debug_check_stacks(void);

/**
 * Verify stack canary for a thread.
 *
 * @param thread  Thread to check
 * @return true if canary is intact, false if corrupted
 */
bool nk_debug_verify_canary(nk_thread_t *thread);

/**
 * Get stack usage statistics for a thread.
 *
 * @param thread      Thread to analyze
 * @param used_bytes  Output: bytes used (may be NULL)
 * @param total_bytes Output: total stack size (may be NULL)
 * @return Percentage used (0-100), or -1 on error
 */
int nk_debug_stack_usage(nk_thread_t *thread, size_t *used_bytes, size_t *total_bytes);

/* ============================================================
 *   Performance Instrumentation (Future)
 * ============================================================ */

#ifdef NK_PERF_COUNTERS
/**
 * Dump performance counters for all threads.
 * Shows CPU ticks, context switches, preemption count.
 */
void nk_debug_dump_stats(void);
#endif

/* ============================================================
 *   Compile-Time Debug Options
 * ============================================================ */

#ifdef NK_STACK_DEBUG
/**
 * Enable verbose stack checking on every context switch.
 * WARNING: High overhead, use only for debugging.
 */
void nk_debug_enable_stack_checking(void);
void nk_debug_disable_stack_checking(void);
#endif

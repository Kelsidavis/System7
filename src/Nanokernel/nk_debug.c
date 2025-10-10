/* nk_debug.c - System 7X Nanokernel Debug Tools (C23)
 *
 * Stack visualization and overflow detection.
 */

#include "../../include/Nanokernel/nk_debug.h"
#include "../../include/Nanokernel/nk_thread.h"
#include "../../include/Nanokernel/nk_task.h"
#include "../../include/Nanokernel/nk_sched.h"

/* External declarations */
extern void nk_printf(const char *fmt, ...);
extern void serial_puts(const char *s);

/* ============================================================
 *   Stack Analysis
 * ============================================================ */

/**
 * Get current ESP from a thread's context.
 *
 * For the current running thread, we need to get ESP from CPU.
 * For other threads, ESP is saved in their context structure.
 */
static uint32_t get_thread_esp(nk_thread_t *thread) {
    if (!thread) {
        return 0;
    }

    // If this is the current running thread, get live ESP
    nk_thread_t *current = nk_thread_current();
    if (thread == current) {
        uint32_t esp;
        __asm__ volatile("movl %%esp, %0" : "=r"(esp));
        return esp;
    }

    // Otherwise use saved ESP from context
    return thread->context.esp;
}

/**
 * Verify stack canary for a thread.
 */
bool nk_debug_verify_canary(nk_thread_t *thread) {
    if (!thread || !thread->stack_base) {
        return false;
    }

    // Canary is at the very bottom of the stack (lowest address)
    uint32_t *canary = (uint32_t *)thread->stack_base;
    return (*canary == NK_STACK_CANARY);
}

/**
 * Get stack usage statistics for a thread.
 */
int nk_debug_stack_usage(nk_thread_t *thread, size_t *used_bytes, size_t *total_bytes) {
    if (!thread || !thread->stack_base) {
        return -1;
    }

    uint32_t esp = get_thread_esp(thread);
    if (esp == 0) {
        return -1;
    }

    uintptr_t stack_base = (uintptr_t)thread->stack_base;
    uintptr_t stack_top = stack_base + thread->stack_size;

    // Check if ESP is within valid stack bounds
    if (esp < stack_base || esp > stack_top) {
        // ESP outside stack - severe corruption!
        if (used_bytes) *used_bytes = 0;
        if (total_bytes) *total_bytes = thread->stack_size;
        return -1;
    }

    // Calculate usage (stack grows down, so base + size - esp = used)
    size_t used = stack_top - esp;
    size_t total = thread->stack_size;

    if (used_bytes) *used_bytes = used;
    if (total_bytes) *total_bytes = total;

    // Return percentage (0-100)
    return (int)((used * 100) / total);
}

/**
 * Dump stack usage for a single thread.
 */
void nk_debug_dump_stack(nk_thread_t *thread) {
    if (!thread) {
        thread = nk_thread_current();
        if (!thread) {
            nk_printf("[STACK] No thread to analyze\n");
            return;
        }
    }

    size_t used_bytes = 0;
    size_t total_bytes = 0;
    int percentage = nk_debug_stack_usage(thread, &used_bytes, &total_bytes);

    uint32_t esp = get_thread_esp(thread);
    uintptr_t stack_base = (uintptr_t)thread->stack_base;
    uintptr_t stack_top = stack_base + thread->stack_size;

    // Determine thread name
    const char *name = "unknown";
    nk_thread_t *idle = nullptr;

    // Simple heuristic: if priority is 255, it's likely the idle thread
    if (thread->priority == 255) {
        name = "idle";
    } else {
        // Use TID for worker threads
        static char tid_name[16];
        if (thread->tid == 2) {
            name = "A";
        } else if (thread->tid == 3) {
            name = "B";
        } else if (thread->tid == 4) {
            name = "C";
        } else {
            // Generic TID-based name
            // Can't use snprintf in freestanding, do it manually
            tid_name[0] = 'T';
            tid_name[1] = 'I';
            tid_name[2] = 'D';
            tid_name[3] = '_';

            // Simple decimal conversion for TID
            uint32_t tid = thread->tid;
            int pos = 4;
            if (tid == 0) {
                tid_name[pos++] = '0';
            } else {
                char digits[10];
                int digit_count = 0;
                while (tid > 0) {
                    digits[digit_count++] = '0' + (tid % 10);
                    tid /= 10;
                }
                for (int i = digit_count - 1; i >= 0; i--) {
                    tid_name[pos++] = digits[i];
                }
            }
            tid_name[pos] = '\0';
            name = tid_name;
        }
    }

    // Print stack info
    nk_printf("[STACK %s] base=0x%08X top=0x%08X esp=0x%08X\n",
              name, (uint32_t)stack_base, (uint32_t)stack_top, esp);

    if (percentage < 0) {
        nk_printf("           ERROR: ESP outside stack bounds!\n");
        return;
    }

    // Convert bytes to KB with decimal
    uint32_t used_kb = used_bytes / 1024;
    uint32_t used_bytes_rem = used_bytes % 1024;
    uint32_t used_decimal = (used_bytes_rem * 10) / 1024;  // One decimal place

    nk_printf("           used=%u.%u KB (%u%%) of %u KB\n",
              used_kb, used_decimal, percentage, (uint32_t)(total_bytes / 1024));

    // Check canary
    bool canary_ok = nk_debug_verify_canary(thread);
    if (!canary_ok) {
        nk_printf("           WARNING: Stack canary corrupted!\n");
    }

    // Warning for high usage
    if (percentage >= 75) {
        nk_printf("           WARNING: High stack usage (>75%%)\n");
    }
}

/**
 * Check all thread stacks for overflow and report usage.
 */
void nk_debug_check_stacks(void) {
    nk_printf("\n========================================\n");
    nk_printf("  Stack Usage Report\n");
    nk_printf("========================================\n\n");

    // Get all tasks and walk their threads
    // For now, we'll just check the current thread and any we can access
    // A full implementation would walk the task list

    nk_thread_t *current = nk_thread_current();
    if (current) {
        nk_printf("Current thread:\n");
        nk_debug_dump_stack(current);
        nk_printf("\n");
    }

    // TODO: Walk all tasks and threads when task list API is available
    // For now, this reports on the current thread which is sufficient
    // to verify the stack checking mechanism works

    nk_printf("========================================\n\n");
}

#ifdef NK_STACK_DEBUG
/* ============================================================
 *   Runtime Stack Checking (Optional)
 * ============================================================ */

static bool stack_checking_enabled = false;

void nk_debug_enable_stack_checking(void) {
    stack_checking_enabled = true;
    nk_printf("[DEBUG] Stack checking enabled\n");
}

void nk_debug_disable_stack_checking(void) {
    stack_checking_enabled = false;
    nk_printf("[DEBUG] Stack checking disabled\n");
}

/**
 * Called from context switch path to verify stack integrity.
 * Only active when NK_STACK_DEBUG is defined.
 */
void nk_debug_check_context_switch(nk_thread_t *prev, nk_thread_t *next) {
    if (!stack_checking_enabled) {
        return;
    }

    if (prev && !nk_debug_verify_canary(prev)) {
        nk_printf("[DEBUG] Stack overflow detected in thread %u!\n", prev->tid);
    }

    if (next && !nk_debug_verify_canary(next)) {
        nk_printf("[DEBUG] Stack overflow detected in thread %u!\n", next->tid);
    }
}
#endif

#ifdef NK_PERF_COUNTERS
/* ============================================================
 *   Performance Instrumentation (Future)
 * ============================================================ */

void nk_debug_dump_stats(void) {
    // TODO: Implement performance counter reporting
    nk_printf("[PERF] Performance counter support not yet implemented\n");
}
#endif

/* nk_thread.h - System 7X Nanokernel Thread Subsystem (C23)
 *
 * Modern multi-threaded kernel with preemptive scheduling.
 * Freestanding implementation with no libc dependencies.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct nk_task nk_task_t;
typedef struct nk_thread nk_thread_t;

/* ============================================================
 *   CPU Context Structure
 * ============================================================ */

/**
 * CPU context for context switching (matches nk_context.S)
 *
 * This structure must match the layout expected by nk_switch_context()
 * in assembly. All general-purpose registers are saved/restored.
 */
typedef struct nk_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t esp;
    uint32_t eip;
    uint32_t eflags;
} nk_context_t;

static_assert(sizeof(nk_context_t) == 28, "Context size must be 28 bytes");

/* ============================================================
 *   Thread States
 * ============================================================ */

enum nk_thread_state {
    NK_THREAD_READY,        // Ready to run
    NK_THREAD_RUNNING,      // Currently executing
    NK_THREAD_SLEEPING,     // Waiting for timer
    NK_THREAD_BLOCKED,      // Waiting for resource
    NK_THREAD_TERMINATED    // Exited
};

/* ============================================================
 *   Thread Structure
 * ============================================================ */

struct nk_thread {
    uint32_t tid;                      // Thread ID
    nk_task_t *task;                   // Parent task

    void *stack_base;                  // Stack allocation
    size_t stack_size;                 // Stack size in bytes

    nk_context_t context;              // Saved CPU context
    enum nk_thread_state state;        // Current state
    int priority;                      // Priority (0-255, higher = higher priority)

    uint64_t wake_time;                // Wake tick for sleeping threads

    nk_thread_t *next;                 // Next in queue
    nk_thread_t *prev;                 // Previous in queue
};

/* ============================================================
 *   Thread API
 * ============================================================ */

/**
 * Create a new thread within a task.
 *
 * @param task         Parent task
 * @param entry        Thread entry point function
 * @param arg          Argument passed to entry function
 * @param stack_size   Stack size in bytes (page-aligned)
 * @param priority     Priority level (0-255)
 * @return Thread handle, or nullptr on failure
 */
[[nodiscard]] nk_thread_t *nk_thread_create(
    nk_task_t *task,
    void (*entry)(void *),
    void *arg,
    size_t stack_size,
    int priority
);

/**
 * Voluntarily yield CPU to another thread.
 */
void nk_thread_yield(void);

/**
 * Exit the current thread (does not return).
 */
[[noreturn]] void nk_thread_exit(void);

/**
 * Sleep for specified milliseconds.
 *
 * @param millis  Milliseconds to sleep
 */
void nk_thread_sleep(uint64_t millis);

/**
 * Get current running thread.
 *
 * @return Current thread, or nullptr if none
 */
nk_thread_t *nk_thread_current(void);

/**
 * Set current thread (internal scheduler use only).
 *
 * @param thread  Thread to set as current
 */
void nk_thread_set_current(nk_thread_t *thread);

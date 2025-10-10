/* nk_thread.h - System 7X Nanokernel Thread Subsystem (C23)
 *
 * Modern multi-threaded kernel with preemptive scheduling.
 * Freestanding implementation with no libc dependencies.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>  /* for offsetof */
#include "nk_stats.h"

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
 *   Interrupt Frame Structure
 * ============================================================ */

/**
 * Interrupt frame structure for IRQ-safe context switching.
 *
 * This structure matches the exact stack layout created by the CPU
 * and our ISR stubs when an interrupt occurs. The ISR stub does:
 *
 * 1. CPU pushes (on interrupt entry): EFLAGS, CS, EIP (stack grows DOWN)
 * 2. ISR stub executes: pusha (pushes EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
 * 3. ISR stub pushes: DS, ES, FS, GS (last pushes = lowest addresses)
 *
 * So ESP points to GS (lowest address), and the frame extends upward.
 * Total size: 68 bytes (17 dwords including user_esp/ss for ring transitions)
 *
 * When this structure is used for context switching, we save a pointer
 * to it in the thread's context, then restore the new thread's frame
 * and return via IRET.
 */
typedef struct nk_interrupt_frame {
    /* Segment registers pushed LAST by ISR (lowest addresses, ESP points here) */
    uint32_t gs;         /* Offset 0 - ESP points here */
    uint32_t fs;         /* Offset 4 */
    uint32_t es;         /* Offset 8 */
    uint32_t ds;         /* Offset 12 */

    /* General-purpose registers saved by PUSHA */
    uint32_t edi;        /* Offset 16 - last pushed by PUSHA */
    uint32_t esi;        /* Offset 20 */
    uint32_t ebp;        /* Offset 24 */
    uint32_t esp_dummy;  /* Offset 28 - ESP before PUSHA (not restored) */
    uint32_t ebx;        /* Offset 32 */
    uint32_t edx;        /* Offset 36 */
    uint32_t ecx;        /* Offset 40 */
    uint32_t eax;        /* Offset 44 - first pushed by PUSHA */

    /* CPU-pushed values (highest addresses) */
    uint32_t eip;        /* Offset 48 */
    uint32_t cs;         /* Offset 52 */
    uint32_t eflags;     /* Offset 56 */
    uint32_t user_esp;   /* Offset 60 - Only if privilege level changed */
    uint32_t ss;         /* Offset 64 - Only if privilege level changed */
} nk_interrupt_frame_t;

static_assert(sizeof(nk_interrupt_frame_t) == 68, "Interrupt frame must be 68 bytes");

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
    nk_interrupt_frame_t *irq_frame;   // Saved interrupt frame (for IRQ context switches)
    enum nk_thread_state state;        // Current state
    int priority;                      // Priority (0-255, higher = higher priority)

    uint64_t wake_time;                // Wake tick for sleeping threads

    nk_thread_stats_t stats;           // Performance instrumentation data

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

/* ============================================================
 *   Field Offset Verification (for assembly code)
 * ============================================================ */

/* These macros verify that assembly code using hardcoded offsets
 * matches the actual C structure layout. If compilation fails here,
 * update the offsets in nk_context.S */

#define NK_THREAD_OFFSETOF_CONTEXT    (offsetof(nk_thread_t, context))
#define NK_THREAD_OFFSETOF_IRQ_FRAME  (offsetof(nk_thread_t, irq_frame))

#define NK_CONTEXT_OFFSETOF_EDI       (0)
#define NK_CONTEXT_OFFSETOF_ESI       (4)
#define NK_CONTEXT_OFFSETOF_EBX       (8)
#define NK_CONTEXT_OFFSETOF_EBP       (12)
#define NK_CONTEXT_OFFSETOF_ESP       (16)
#define NK_CONTEXT_OFFSETOF_EIP       (20)
#define NK_CONTEXT_OFFSETOF_EFLAGS    (24)

/* nk_thread.c - System 7X Nanokernel Thread Implementation (C23)
 *
 * Thread creation, lifecycle management, and synchronization primitives.
 */

#include "../../include/Nanokernel/nk_thread.h"
#include "../../include/Nanokernel/nk_task.h"
#include "../../include/Nanokernel/nk_sched.h"
#include "../../include/Nanokernel/nk_memory.h"
#include <stdatomic.h>

/* External declarations */
extern void nk_printf(const char *fmt, ...);
extern void nk_sleep_until(nk_thread_t *thread, uint64_t wake_time);

/* Thread ID counter */
static _Atomic uint32_t next_tid = 1;

/* ============================================================
 *   Thread Entry Wrapper
 * ============================================================ */

/**
 * Thread entry wrapper - never returns.
 * This is the actual entry point for all threads.
 */
[[noreturn]] static void thread_entry_wrapper(void) {
    // Get current thread
    nk_thread_t *self = nk_thread_current();
    if (!self) {
        for (;;);  // Should never happen
    }

    // Extract entry function and argument from stack
    // Stack layout: sp[0]=ret_addr, sp[1]=entry, sp[2]=arg
    uint32_t *sp = (uint32_t *)self->context.esp;
    void (*entry)(void *) = (void (*)(void *))(sp[1]);
    void *arg = (void *)(sp[2]);

    // Call user entry function
    entry(arg);

    // Thread returned - exit
    nk_thread_exit();
}

/* ============================================================
 *   Thread Creation
 * ============================================================ */

/**
 * Create a new thread within a task.
 */
nk_thread_t *nk_thread_create(
    nk_task_t *task,
    void (*entry)(void *),
    void *arg,
    size_t stack_size,
    int priority
) {
    if (!task || !entry || stack_size == 0) {
        return nullptr;
    }

    // Clamp priority
    if (priority < 0) priority = 0;
    if (priority >= NK_MAX_PRIORITY) priority = NK_MAX_PRIORITY - 1;

    // Allocate thread structure
    auto thread = (nk_thread_t *)kmalloc(sizeof(nk_thread_t));
    if (!thread) {
        return nullptr;
    }

    // Allocate stack (page-aligned)
    size_t aligned_stack_size = NK_PAGE_ALIGN(stack_size);
    void *stack = kmalloc(aligned_stack_size);
    if (!stack) {
        kfree(thread);
        return nullptr;
    }

    // Initialize thread structure
    *thread = (nk_thread_t){
        .tid = atomic_fetch_add_explicit(&next_tid, 1, memory_order_seq_cst),
        .task = task,
        .stack_base = stack,
        .stack_size = aligned_stack_size,
        .state = NK_THREAD_READY,
        .priority = priority,
        .wake_time = 0,
        .next = nullptr,
        .prev = nullptr
    };

    // Setup initial stack frame
    uintptr_t sp = (uintptr_t)stack + aligned_stack_size;

    // Align to 16 bytes (x86 ABI requirement)
    sp &= ~0xFUL;

    // Push argument and entry function for wrapper
    sp -= sizeof(void *);
    *(void **)sp = arg;

    sp -= sizeof(void *);
    *(void **)sp = (void *)entry;

    sp -= sizeof(void *);
    *(uint32_t *)sp = 0;  // Return address (should never be used)

    // Initialize context to start at wrapper
    thread->context.esp = (uint32_t)sp;
    thread->context.eip = (uint32_t)thread_entry_wrapper;
    thread->context.ebp = 0;
    thread->context.ebx = 0;
    thread->context.esi = 0;
    thread->context.edi = 0;
    thread->context.eflags = 0x202;  // IF (interrupts enabled) + reserved bit

    // Add to parent task
    nk_task_add_thread(task, thread);

    // Add to scheduler ready queue
    nk_sched_add_thread(thread);

    return thread;
}

/* ============================================================
 *   Thread Lifecycle
 * ============================================================ */

/**
 * Voluntarily yield CPU to another thread.
 */
void nk_thread_yield(void) {
    nk_schedule();
}

/**
 * Exit current thread (does not return).
 */
[[noreturn]] void nk_thread_exit(void) {
    nk_thread_t *self = nk_thread_current();
    if (!self) {
        for (;;);  // Should never happen
    }

    // Mark as terminated
    self->state = NK_THREAD_TERMINATED;

    // Remove from scheduler
    nk_sched_remove_thread(self);

    // Remove from parent task
    nk_task_remove_thread(self->task, self);

    // Schedule next thread (this never returns)
    nk_schedule();

    // Should never reach here
    for (;;);
}

/**
 * Sleep for specified milliseconds.
 */
void nk_thread_sleep(uint64_t millis) {
    if (millis == 0) {
        nk_thread_yield();
        return;
    }

    nk_thread_t *self = nk_thread_current();
    if (!self) {
        return;
    }

    // Let timer subsystem handle sleep
    nk_sleep_until(self, millis);

    // This will context switch to another thread
    nk_schedule();
}

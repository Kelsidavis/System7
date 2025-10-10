/* nk_task.c - System 7X Nanokernel Task Implementation (C23)
 *
 * Tasks are process containers that hold threads and resources.
 */

#include "../../include/Nanokernel/nk_task.h"
#include "../../include/Nanokernel/nk_sched.h"
#include "../../include/Nanokernel/nk_memory.h"
#include <stdatomic.h>

/* ============================================================
 *   Task Management
 * ============================================================ */

/* Global task list */
static nk_task_t *task_list_head = nullptr;
static _Atomic uint32_t next_pid = 1;

/* Task list lock */
static nk_spinlock_t task_list_lock = { .locked = 0 };

/**
 * Create a new task (process container).
 */
nk_task_t *nk_task_create(void) {
    // Allocate task structure
    auto task = (nk_task_t *)kmalloc(sizeof(nk_task_t));
    if (!task) {
        return nullptr;
    }

    // Initialize task
    *task = (nk_task_t){
        .pid = atomic_fetch_add_explicit(&next_pid, 1, memory_order_seq_cst),
        .page_table_root = 0,  // Future: allocate page table
        .threads = nullptr,
        .thread_count = 0,
        .next = nullptr
    };

    // Add to global task list
    nk_spinlock_acquire(&task_list_lock);
    task->next = task_list_head;
    task_list_head = task;
    nk_spinlock_release(&task_list_lock);

    return task;
}

/**
 * Add a thread to a task's thread list.
 */
void nk_task_add_thread(nk_task_t *task, nk_thread_t *thread) {
    if (!task || !thread) {
        return;
    }

    // Link thread into task's thread list
    thread->next = task->threads;
    if (task->threads) {
        task->threads->prev = thread;
    }
    thread->prev = nullptr;
    task->threads = thread;

    task->thread_count++;
}

/**
 * Remove a thread from a task's thread list.
 */
void nk_task_remove_thread(nk_task_t *task, nk_thread_t *thread) {
    if (!task || !thread) {
        return;
    }

    // Unlink thread from task's thread list
    if (thread->prev) {
        thread->prev->next = thread->next;
    } else {
        task->threads = thread->next;
    }

    if (thread->next) {
        thread->next->prev = thread->prev;
    }

    thread->next = nullptr;
    thread->prev = nullptr;

    task->thread_count--;
}

/**
 * Destroy a task and all its threads.
 */
void nk_task_destroy(nk_task_t *task) {
    if (!task) {
        return;
    }

    // Free all threads (stub - proper cleanup later)
    nk_thread_t *thread = task->threads;
    while (thread) {
        auto next = thread->next;
        kfree(thread->stack_base);
        kfree(thread);
        thread = next;
    }

    // Remove from task list
    nk_spinlock_acquire(&task_list_lock);
    if (task_list_head == task) {
        task_list_head = task->next;
    } else {
        nk_task_t *prev = task_list_head;
        while (prev && prev->next != task) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = task->next;
        }
    }
    nk_spinlock_release(&task_list_lock);

    // Free task structure
    kfree(task);
}

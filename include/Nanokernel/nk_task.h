/* nk_task.h - System 7X Nanokernel Task (Process) Subsystem (C23)
 *
 * Tasks are containers for threads, representing processes.
 * Each task has its own address space (future VMM integration).
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "nk_thread.h"

/* ============================================================
 *   Task Structure (Process Container)
 * ============================================================ */

struct nk_task {
    uint32_t pid;                      // Process ID

    uintptr_t page_table_root;         // Page table root (future VMM)

    nk_thread_t *threads;              // Linked list of threads
    uint32_t thread_count;             // Number of threads in task

    nk_task_t *next;                   // Next task in system list
};

/* ============================================================
 *   Task API
 * ============================================================ */

/**
 * Create a new task (process).
 *
 * @return Task handle, or nullptr on failure
 */
[[nodiscard]] nk_task_t *nk_task_create(void);

/**
 * Add a thread to a task's thread list.
 *
 * @param task    Task to add thread to
 * @param thread  Thread to add
 */
void nk_task_add_thread(nk_task_t *task, nk_thread_t *thread);

/**
 * Remove a thread from a task's thread list.
 *
 * @param task    Task to remove thread from
 * @param thread  Thread to remove
 */
void nk_task_remove_thread(nk_task_t *task, nk_thread_t *thread);

/**
 * Destroy a task and all its threads.
 *
 * @param task  Task to destroy
 */
void nk_task_destroy(nk_task_t *task);

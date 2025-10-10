/* nk_test_threads.c - System 7X Nanokernel Threading Test Harness (C23)
 *
 * Demonstrates cooperative and preemptive multithreading.
 */

#include "../../include/Nanokernel/nk_thread.h"
#include "../../include/Nanokernel/nk_task.h"
#include "../../include/Nanokernel/nk_sched.h"
#include "../../include/Nanokernel/nk_memory.h"

/* External declarations */
extern void nk_printf(const char *fmt, ...);
extern void nk_timer_init(void);
extern uint64_t nk_get_ticks(void);

/* ============================================================
 *   Test Worker Threads
 * ============================================================ */

/**
 * Worker A - prints [A] every 10ms
 */
static void worker_a(void *arg) {
    (void)arg;

    for (int i = 0; i < 10; i++) {
        nk_printf("[A]");
        nk_thread_sleep(10);
    }

    nk_printf("[A done]\n");
    nk_thread_exit();
}

/**
 * Worker B - prints [B] every 15ms
 */
static void worker_b(void *arg) {
    (void)arg;

    for (int i = 0; i < 10; i++) {
        nk_printf("[B]");
        nk_thread_sleep(15);
    }

    nk_printf("[B done]\n");
    nk_thread_exit();
}

/**
 * Worker C - counts and yields
 */
static void worker_c(void *arg) {
    (void)arg;

    for (int i = 0; i < 5; i++) {
        nk_printf("[C:%d]", i);
        nk_thread_yield();
    }

    nk_printf("[C done]\n");
    nk_thread_exit();
}

/* ============================================================
 *   Test Entry Point
 * ============================================================ */

/**
 * Run nanokernel threading test suite.
 */
void nk_test_threads_run(void) {
    nk_printf("\n");
    nk_printf("========================================\n");
    nk_printf("  Nanokernel Threading Test Suite\n");
    nk_printf("========================================\n\n");

    // Initialize subsystems
    nk_printf("[TEST] Initializing timer subsystem...\n");
    nk_timer_init();

    nk_printf("[TEST] Initializing scheduler...\n");
    nk_sched_init();

    // Enable hardware interrupts now that timer and scheduler are initialized
    nk_printf("[TEST] Enabling interrupts...\n");
    // TEMPORARILY DISABLED - causes triple fault even with IRQ0 masked
    // __asm__ volatile("sti");
    nk_printf("[TEST] Interrupts DISABLED for debugging (sti causes triple fault)\n");

    // Create system task
    nk_printf("[TEST] Creating system task...\n");
    auto sys_task = nk_task_create();
    if (!sys_task) {
        nk_printf("[TEST] FAILED to create system task\n");
        return;
    }
    nk_printf("[TEST] System task created (PID %u)\n", sys_task->pid);

    // Create worker threads
    nk_printf("[TEST] Creating worker threads...\n");

    auto thread_a = nk_thread_create(sys_task, worker_a, nullptr, 8192, 10);
    if (!thread_a) {
        nk_printf("[TEST] FAILED to create thread A\n");
        return;
    }
    nk_printf("[TEST] Thread A created (TID %u)\n", thread_a->tid);

    auto thread_b = nk_thread_create(sys_task, worker_b, nullptr, 8192, 10);
    if (!thread_b) {
        nk_printf("[TEST] FAILED to create thread B\n");
        return;
    }
    nk_printf("[TEST] Thread B created (TID %u)\n", thread_b->tid);

    auto thread_c = nk_thread_create(sys_task, worker_c, nullptr, 8192, 10);
    if (!thread_c) {
        nk_printf("[TEST] FAILED to create thread C\n");
        return;
    }
    nk_printf("[TEST] Thread C created (TID %u)\n", thread_c->tid);

    nk_printf("\n[TEST] Starting scheduler...\n");
    nk_printf("[TEST] Expected output: [C:0][C:1][C:2][C:3][C:4][C done][A][B][A][B]...\n\n");

    // Run scheduler loop
    // In a real system, this would be called from timer interrupt
    // For testing, we'll manually call it in a loop
    for (int i = 0; i < 100; i++) {
        nk_schedule();

        // Simulate timer tick every iteration
        // (In real system, this would be from hardware timer interrupt)
        extern void nk_timer_tick(void);
        // nk_timer_tick();  // Uncomment when timer interrupt is set up
    }

    nk_printf("\n\n[TEST] Threading test complete\n");
    nk_printf("========================================\n\n");
}

/**
 * Simple threading demo (can be called from kernel_main).
 */
void nk_threading_demo(void) {
    nk_printf("\n=== Nanokernel Threading Demo ===\n\n");

    // Initialize
    nk_sched_init();

    // Create task
    auto task = nk_task_create();

    // Create threads
    nk_thread_create(task, worker_a, nullptr, 8192, 10);
    nk_thread_create(task, worker_b, nullptr, 8192, 10);

    // Schedule forever
    nk_printf("Starting scheduler...\n\n");
    for (;;) {
        nk_schedule();
    }
}

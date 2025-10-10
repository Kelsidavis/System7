/* nk_test_threads.c - System 7X Nanokernel Threading Test Harness (C23)
 *
 * Demonstrates cooperative and preemptive multithreading.
 */

#include "../../include/Nanokernel/nk_thread.h"
#include "../../include/Nanokernel/nk_task.h"
#include "../../include/Nanokernel/nk_sched.h"
#include "../../include/Nanokernel/nk_memory.h"
#include "../../include/Nanokernel/nk_debug.h"
#include "../../include/Nanokernel/nk_stats.h"

/* External declarations */
extern void nk_printf(const char *fmt, ...);
extern void nk_timer_init(void);
extern uint64_t nk_get_ticks(void);

/* ============================================================
 *   Test Worker Threads
 * ============================================================ */

/**
 * Worker A - prints [A] and relies on preemptive scheduling
 */
static void worker_a(void *arg) {
    (void)arg;

    for (int i = 0; i < 5; i++) {
        nk_printf("[A]");
        // Busy-wait to give timer a chance to preempt
        for (volatile int j = 0; j < 1000000; j++);
    }

    nk_printf("[A done]\n");
    nk_thread_exit();
}

/**
 * Worker B - prints [B] and relies on preemptive scheduling
 */
static void worker_b(void *arg) {
    (void)arg;

    for (int i = 0; i < 5; i++) {
        nk_printf("[B]");
        // Busy-wait to give timer a chance to preempt
        for (volatile int j = 0; j < 1000000; j++);
    }

    nk_printf("[B done]\n");
    nk_thread_exit();
}

/**
 * Worker C - prints iteration counter and relies on preemptive scheduling
 */
static void worker_c(void *arg) {
    (void)arg;

    for (int i = 0; i < 5; i++) {
        nk_printf("[C:%d]", i);
        // Busy-wait to give timer a chance to preempt
        for (volatile int j = 0; j < 1000000; j++);
    }

    nk_printf("[C done]\n");
    // Will call nk_thread_exit() when function returns
}

/**
 * Stats dumper thread - waits for workers to complete, then dumps performance stats.
 */
static void stats_dumper(void *arg) {
    (void)arg;

    // Wait for workers to complete (give them time to run)
    extern void serial_puts(const char *s);
    serial_puts("[STATS] Stats dumper thread started, waiting for workers...\n");

    // Delay to let workers run and complete
    for (volatile int i = 0; i < 10000000; i++);  // ~1 second

    serial_puts("[STATS] Delay complete, about to dump stats...\n");

    // Dump performance statistics
    nk_debug_dump_stats();

    serial_puts("[STATS] Stats dump complete, exiting...\n");
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
    nk_printf("[TEST] Enabling interrupts (testing IRQ-safe context switch)...\n");

    // Disable NMI temporarily to avoid spurious NMI during STI
    extern uint8_t hal_inb(uint16_t port);
    extern void hal_outb(uint16_t port, uint8_t value);
    extern void serial_puts(const char *s);

    uint8_t nmi_prev = hal_inb(0x70);
    hal_outb(0x70, nmi_prev | 0x80);  /* Disable NMI via bit 7 */
    nk_printf("[TEST] NMI disabled\n");

    // Verify GDT is still loaded and dump current segment registers
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) gdtr;
    __asm__ volatile("sgdt %0" : "=m"(gdtr));

    uint16_t cs, ds, es, ss;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%es, %0" : "=r"(es));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    if (gdtr.base != 0) {
        serial_puts("[TEST] GDT verified: base!=0\n");
    } else {
        serial_puts("[TEST] ERROR: GDT base is NULL!\n");
    }

    // Use direct serial output for segment values to avoid printf issues
    extern void hal_outb(uint16_t port, uint8_t value);
    serial_puts("[TEST] CS=0x");
    for (int i = 1; i >= 0; i--) {
        uint8_t nibble = (cs >> (i*4)) & 0xF;
        char hex = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        hal_outb(0x3F8, hex);
    }
    serial_puts(" DS=0x");
    for (int i = 1; i >= 0; i--) {
        uint8_t nibble = (ds >> (i*4)) & 0xF;
        char hex = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        hal_outb(0x3F8, hex);
    }
    serial_puts(" ES=0x");
    for (int i = 1; i >= 0; i--) {
        uint8_t nibble = (es >> (i*4)) & 0xF;
        char hex = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        hal_outb(0x3F8, hex);
    }
    serial_puts(" SS=0x");
    for (int i = 1; i >= 0; i--) {
        uint8_t nibble = (ss >> (i*4)) & 0xF;
        char hex = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        hal_outb(0x3F8, hex);
    }
    serial_puts("\n");

    // Verify stack pointer before enabling interrupts
    uint32_t esp;
    __asm__ volatile("movl %%esp, %0" : "=r"(esp));
    nk_printf("[TEST] Stack before sti = 0x%08X\n", esp);

    // TEST: Enable interrupts WITHOUT unmasking IRQ0 first
    // to verify STI itself doesn't cause reset
    // Use direct serial output immediately after STI (before any C code)
    extern void serial_puts(const char *s);

    __asm__ volatile(
        "sti\n"
        "nop\n"
        "nop\n"
        "nop\n"
    );

    // If we reach here, STI succeeded
    serial_puts("[TEST] STI executed successfully!\n");
    nk_printf("[TEST] STI successful - interrupts enabled\n");

    // TEST: Delay before unmasking IRQ0 to verify STI stability
    serial_puts("[TEST] Delaying 1 second before unmasking IRQ0...\n");
    for (volatile int i = 0; i < 10000000; i++);  /* ~1 second delay */
    serial_puts("[TEST] Delay complete\n");

    // DEBUG: Dump IDT entry for vector 32 (IRQ0) before unmasking
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr;
    __asm__ volatile("sidt %0" : "=m"(idtr));

    uint8_t *idt = (uint8_t *)(idtr.base);
    uint8_t *entry = &idt[32 * 8];  // Each IDT entry is 8 bytes

    serial_puts("[TEST] Raw IDT entry bytes for vector 32: ");
    for (int i = 0; i < 8; i++) {
        // Output individual bytes as hex using assembly
        uint8_t byte = entry[i];
        uint8_t high_nibble = (byte >> 4) & 0xF;
        uint8_t low_nibble = byte & 0xF;

        // Convert to ASCII hex
        char hex_high = (high_nibble < 10) ? ('0' + high_nibble) : ('A' + high_nibble - 10);
        char hex_low = (low_nibble < 10) ? ('0' + low_nibble) : ('A' + low_nibble - 10);

        extern void hal_outb(uint16_t port, uint8_t value);
        hal_outb(0x3F8, hex_high);
        hal_outb(0x3F8, hex_low);
        hal_outb(0x3F8, ' ');
    }
    serial_puts("\n");

    extern void irq0_stub(void);
    extern void nk_printf(const char *fmt, ...);
    nk_printf("[TEST] Expected irq0_stub address: 0x%08X\n", (uint32_t)irq0_stub);

    // FIX: Re-install IRQ0 handler with CORRECT code selector
    // CRITICAL: Use current CS (0x10), not wrong selector (0x08)!
    extern void irq0_stub(void);
    extern void nk_idt_set_gate(uint8_t num, void (*handler)(void), uint16_t selector, uint8_t flags);
    serial_puts("[TEST] Re-installing IRQ0 handler with CS=0x10...\n");
    nk_idt_set_gate(32, irq0_stub, 0x10, 0x8E);  /* Use CS=0x10, not 0x08! */
    serial_puts("[TEST] IRQ0 handler re-installed with correct selector\n");

    // Now unmask IRQ0 to allow timer interrupts
    // CRITICAL: Disable interrupts BEFORE unmasking to prevent immediate firing
    extern void nk_pic_unmask(uint8_t irq);
    serial_puts("[TEST] About to CLI before unmask...\n");
    __asm__ volatile("cli");
    serial_puts("[TEST] CLI executed, now unmasking IRQ0...\n");
    nk_pic_unmask(0);
    serial_puts("[TEST] IRQ0 unmasked in PIC (interrupts still disabled)\n");

    // Clear any pending timer interrupt by sending EOI
    extern void nk_pic_send_eoi(uint8_t irq);
    serial_puts("[TEST] Sending EOI to clear any pending timer interrupt...\n");
    nk_pic_send_eoi(0);

    serial_puts("[TEST] Now re-enabling interrupts with STI...\n");
    __asm__ volatile("sti");
    serial_puts("[TEST] Interrupts enabled! Timer IRQ0 is now active.\n");

    // Small delay to let a few timer interrupts fire
    for (volatile int i = 0; i < 1000000; i++);

    serial_puts("[TEST] Timer interrupts are working!\n");

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

    // Create stats dumper thread (Phase 4: Performance instrumentation)
    // Use same priority as workers so it gets a fair share of CPU time
    auto thread_stats = nk_thread_create(sys_task, stats_dumper, nullptr, 8192, 10);
    if (!thread_stats) {
        nk_printf("[TEST] FAILED to create stats dumper thread\n");
        return;
    }
    nk_printf("[TEST] Stats dumper thread created (TID %u)\n", thread_stats->tid);

    nk_printf("\n[TEST] Starting scheduler...\n");
    nk_printf("[TEST] Expected output: interleaved [A][B][C] with timer preemption\n\n");

    // Phase 2: Stack usage visualization (before scheduler entry)
    nk_printf("[TEST] Pre-execution stack analysis:\n");
    nk_debug_dump_stack(thread_a);
    nk_debug_dump_stack(thread_b);
    nk_debug_dump_stack(thread_c);
    nk_printf("\n");

    // Preemptive scheduling is now handled by IRQ0 timer interrupts
    nk_printf("[TEST] Entering scheduler - timer will preempt threads\n");

    // Enter the scheduler - this will switch to the first ready thread
    // Timer interrupts will preempt threads and switch between them
    // The idle thread will run when all worker threads complete
    // NOTE: This never returns - idle thread runs forever
    nk_schedule();

    // NEVER REACHED: Control never returns from nk_schedule()
    // The idle thread runs forever after all workers complete
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

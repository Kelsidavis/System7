/* nk_timer.c - System 7X Nanokernel Timer Subsystem (C23)
 *
 * Timer tick handling, sleep queue management, and thread waking.
 */

#include "../../include/Nanokernel/nk_timer.h"
#include "../../include/Nanokernel/nk_thread.h"
#include "../../include/Nanokernel/nk_sched.h"
#include <stdatomic.h>

/* I/O port access from platform layer */
extern void hal_outb(uint16_t port, uint8_t value);
extern uint8_t hal_inb(uint16_t port);

/* Convenience wrappers to match expected names */
static inline void outb(uint16_t port, uint8_t val) { hal_outb(port, val); }
static inline uint8_t inb(uint16_t port) { return hal_inb(port); }

/* External declarations */
extern void nk_printf(const char *fmt, ...);
extern void nk_schedule(void);
extern void serial_puts(const char *s);

/* ============================================================
 *   Timer State
 * ============================================================ */

/* Global tick counter (milliseconds) */
static _Atomic uint64_t system_ticks = 0;

/* Sleep queue (sorted by wake_time) */
static nk_thread_t *sleep_queue_head = nullptr;

/* Sleep queue lock */
static nk_spinlock_t sleep_lock = { .locked = 0 };

/* ============================================================
 *   Sleep Queue Management
 * ============================================================ */

/**
 * Insert thread into sleep queue (sorted by wake time).
 */
void nk_sleep_until(nk_thread_t *thread, uint64_t millis) {
    if (!thread) {
        return;
    }

    // Calculate absolute wake time
    uint64_t current = atomic_load_explicit(&system_ticks, memory_order_relaxed);
    thread->wake_time = current + millis;
    thread->state = NK_THREAD_SLEEPING;

    // Remove from scheduler ready queue
    nk_sched_remove_thread(thread);

    // Insert into sleep queue (sorted by wake_time)
    nk_spinlock_acquire(&sleep_lock);

    if (!sleep_queue_head || thread->wake_time < sleep_queue_head->wake_time) {
        // Insert at head
        thread->next = sleep_queue_head;
        thread->prev = nullptr;
        if (sleep_queue_head) {
            sleep_queue_head->prev = thread;
        }
        sleep_queue_head = thread;
    } else {
        // Find insertion point
        nk_thread_t *curr = sleep_queue_head;
        while (curr->next && curr->next->wake_time <= thread->wake_time) {
            curr = curr->next;
        }

        // Insert after curr
        thread->next = curr->next;
        thread->prev = curr;
        if (curr->next) {
            curr->next->prev = thread;
        }
        curr->next = thread;
    }

    nk_spinlock_release(&sleep_lock);
}

/**
 * Wake threads whose time has expired.
 */
static void wake_sleeping_threads(void) {
    uint64_t current = atomic_load_explicit(&system_ticks, memory_order_relaxed);

    nk_spinlock_acquire(&sleep_lock);

    // Check sleep queue head
    while (sleep_queue_head && sleep_queue_head->wake_time <= current) {
        nk_thread_t *thread = sleep_queue_head;

        // Remove from sleep queue
        sleep_queue_head = thread->next;
        if (sleep_queue_head) {
            sleep_queue_head->prev = nullptr;
        }

        thread->next = nullptr;
        thread->prev = nullptr;

        // Mark as ready and add to scheduler
        thread->state = NK_THREAD_READY;

        nk_spinlock_release(&sleep_lock);
        nk_sched_add_thread(thread);
        nk_spinlock_acquire(&sleep_lock);
    }

    nk_spinlock_release(&sleep_lock);
}

/* ============================================================
 *   Timer Tick Handler
 * ============================================================ */

/**
 * Timer tick handler - called from timer interrupt.
 *
 * Increments tick counter, wakes sleeping threads, and triggers scheduling.
 */
void nk_timer_tick(void) {
    // Increment tick counter
    atomic_fetch_add_explicit(&system_ticks, 1, memory_order_relaxed);

    // Wake any threads whose sleep time has expired
    wake_sleeping_threads();

    // TODO: Trigger preemptive scheduling via scheduler tick
    // DISABLED: Cannot context switch from interrupt handler without proper setup
    // Need to implement deferred scheduling or use a separate scheduling interrupt
    // extern void nk_sched_tick(void);
    // nk_sched_tick();
}

/**
 * Get current system ticks (milliseconds).
 */
uint64_t nk_get_ticks(void) {
    return atomic_load_explicit(&system_ticks, memory_order_relaxed);
}

/* ============================================================
 *   PIT (Programmable Interval Timer) Hardware
 * ============================================================ */

/**
 * Program the PIT to generate timer interrupts at specified frequency.
 *
 * @param frequency Desired frequency in Hz (typically 1000 Hz = 1ms tick)
 */
static void pit_init(uint32_t frequency) {
    // Calculate divisor: base frequency / desired frequency
    uint32_t divisor = PIT_BASE_HZ / frequency;
    if (divisor > 65535) {
        divisor = 65535;  // Maximum divisor for 16-bit counter
    }

    // Command byte: channel 0, lo/hi byte, rate generator (mode 2)
    // 0x36 = 00 11 011 0
    //        |  |  |   |
    //        |  |  |   +-- BCD mode (0 = binary)
    //        |  |  +------ Mode 2 (rate generator)
    //        |  +--------- Access mode (3 = lo/hi byte)
    //        +------------ Channel 0
    outb(0x43, 0x36);

    // Send divisor (lo byte, then hi byte)
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    serial_puts("[nk_timer] PIT programmed for 1000 Hz\n");
}

/**
 * Initialize timer subsystem.
 * Programs PIT hardware and initializes sleep queue.
 */
void nk_timer_init(void) {
    atomic_store_explicit(&system_ticks, 0, memory_order_relaxed);
    sleep_queue_head = nullptr;
    nk_spinlock_init(&sleep_lock);

    // Program PIT hardware
    pit_init(NK_TIMER_HZ);

    nk_printf("[TIMER] Timer subsystem initialized (%u Hz)\n", NK_TIMER_HZ);
}

/*
 * ARM64 Generic Timer Implementation
 * ARMv8-A Architecture Timer
 */

#include <stdint.h>
#include <stdbool.h>
#include "timer.h"

/* ARM Generic Timer system registers */

/* Counter-timer Frequency - set by firmware */
static uint64_t timer_frequency = 0;

/*
 * Read the counter-timer frequency
 * This is set by firmware and doesn't change
 */
static inline uint64_t timer_get_frequency(void) {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

/*
 * Read the physical counter value
 * This is a 64-bit counter that increments at the frequency
 */
static inline uint64_t timer_get_counter(void) {
    uint64_t count;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(count));
    return count;
}

/*
 * Read the virtual counter value
 */
static inline uint64_t timer_get_virtual_counter(void) {
    uint64_t count;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(count));
    return count;
}

/*
 * Set the physical timer compare value
 */
static inline void timer_set_compare(uint64_t value) {
    __asm__ volatile("msr cntp_cval_el0, %0" :: "r"(value));
    __asm__ volatile("isb");
}

/*
 * Set the physical timer control
 * Bit 0: Enable
 * Bit 1: Interrupt mask
 * Bit 2: Status (ISTATUS)
 */
static inline void timer_set_control(uint32_t value) {
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((uint64_t)value));
    __asm__ volatile("isb");
}

/*
 * Read the physical timer control
 */
static inline uint32_t timer_get_control(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(value));
    return (uint32_t)value;
}

/*
 * Initialize ARM Generic Timer
 */
void timer_init(void) {
    /* Read timer frequency from system register */
    timer_frequency = timer_get_frequency();

    /* Disable timer during initialization */
    timer_set_control(0);
}

/*
 * Get timer frequency in Hz
 */
uint64_t timer_get_freq(void) {
    return timer_frequency;
}

/*
 * Get current counter value
 */
uint64_t timer_get_ticks(void) {
    return timer_get_counter();
}

/*
 * Get current time in microseconds
 */
uint64_t timer_get_usec(void) {
    if (timer_frequency == 0) return 0;
    uint64_t ticks = timer_get_counter();
    return (ticks * 1000000ULL) / timer_frequency;
}

/*
 * Get current time in milliseconds
 */
uint64_t timer_get_msec(void) {
    if (timer_frequency == 0) return 0;
    uint64_t ticks = timer_get_counter();
    return (ticks * 1000ULL) / timer_frequency;
}

/*
 * Busy-wait delay in microseconds
 */
void timer_usleep(uint64_t usec) {
    if (timer_frequency == 0) return;

    uint64_t start = timer_get_counter();
    uint64_t delay_ticks = (usec * timer_frequency) / 1000000ULL;
    uint64_t end = start + delay_ticks;

    /* Handle counter wrap */
    if (end < start) {
        while (timer_get_counter() >= start) {
            __asm__ volatile("yield");
        }
    }

    while (timer_get_counter() < end) {
        __asm__ volatile("yield");
    }
}

/*
 * Busy-wait delay in milliseconds
 */
void timer_msleep(uint64_t msec) {
    timer_usleep(msec * 1000ULL);
}

/*
 * Set a timer interrupt to fire after usec microseconds
 * Returns true if timer was set, false if timer not initialized
 */
bool timer_set_timeout(uint64_t usec) {
    if (timer_frequency == 0) return false;

    uint64_t current = timer_get_counter();
    uint64_t delay_ticks = (usec * timer_frequency) / 1000000ULL;
    uint64_t target = current + delay_ticks;

    timer_set_compare(target);

    /* Enable timer, unmask interrupt */
    timer_set_control(1);

    return true;
}

/*
 * Disable timer interrupt
 */
void timer_disable(void) {
    timer_set_control(0);
}

/*
 * Check if timer has fired
 */
bool timer_is_pending(void) {
    uint32_t ctl = timer_get_control();
    return (ctl & (1 << 2)) != 0;  /* Check ISTATUS bit */
}

/*
 * Acknowledge timer interrupt
 */
void timer_ack(void) {
    /* Disable timer to clear interrupt */
    timer_set_control(0);
}

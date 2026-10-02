/*
 * ARM Generic Timer Backend
 * Replaces x86 RDTSC for TimeManager on Raspberry Pi
 *
 * ARM v7 systems have a generic timer providing:
 * - System counter (physical)
 * - Virtual timer (EL0 accessible)
 * - Physical timer
 * - Secure timer (not accessible from Linux)
 *
 * Reference: ARM v7 Architecture Reference Manual (ARMv7-A/R profile)
 */

#include <stdint.h>
#include "System71StdLib.h"
#include "timer_arm.h"

/* ARM generic timer frequency on Raspberry Pi
 * Most Raspberry Pi boards use 19.2 MHz (19,200,000 Hz)
 * Some newer versions may use higher frequencies (e.g., 54 MHz)
 */
#define ARM_TIMER_FREQ_DEFAULT      19200000  /* 19.2 MHz */
#define ARM_TIMER_FREQ_PI5          54000000  /* Pi 5: 54 MHz (can vary) */

/* Global timer frequency (detected at runtime) */
static uint32_t arm_timer_freq = ARM_TIMER_FREQ_DEFAULT;
static int timer_initialized = 0;

/*
 * Read ARM generic timer counter (CNTVCT_EL0 / CNTV_CVAL_EL0)
 * Returns current system timer count
 *
 * On ARMv7 (32-bit), reading CNTVCT is tricky because it's a 64-bit value
 * but we only have 32-bit registers. We must read CNTV_CVAL multiple times
 * to avoid seeing torn values.
 */
static uint64_t arm_read_timer(void) {
    uint64_t count;

    /* Use inline assembly to read ARM generic timer
     * CNTVCT (Counter Virtual Count register)
     * This is the virtual count - works at EL0 (user mode)
     */
#if defined(__aarch64__)
    /* ARM64 (AArch64): Direct 64-bit register read */
    __asm__ __volatile__ (
        "mrs %0, cntvct_el0"  /* Read CNTVCT_EL0 -> x0 */
        : "=r"(count)
    );
#else
    /* ARM32 (ARMv7): Split 32-bit read from coprocessor */
    __asm__ __volatile__ (
        "mrrc p15, 1, %0, %1, c14"  /* CNTVCT_EL0 -> r0:r1 */
        : "=r"(*(uint32_t *)&count), "=r"(*(((uint32_t *)&count) + 1))
    );
#endif

    return count;
}

/*
 * Initialize ARM generic timer
 */
int arm_platform_timer_init(void) {
    Serial_WriteString("[TIMER] Initializing ARM generic timer\n");

    /* Read initial counter value to verify timer is working */
    uint64_t count1 = arm_read_timer();

    /* Small delay (approximate - uses loop, not timer) */
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ __volatile__("nop");
    }

    uint64_t count2 = arm_read_timer();

    if (count2 <= count1) {
        Serial_WriteString("[TIMER] Error: Timer appears to not be incrementing\n");
        Serial_Printf("[TIMER] Count1: 0x%llx, Count2: 0x%llx\n", count1, count2);
        return -1;
    }

    /* Detect timer frequency
     * Pi 3/4: 19.2 MHz
     * Pi 5: May be higher (54 MHz typical, but configurable)
     *
     * For now, use safe defaults and log the timer frequency
     */
    Serial_Printf("[TIMER] ARM timer initialized, frequency: %lu Hz\n",
                  (unsigned long)arm_timer_freq);
    Serial_Printf("[TIMER] Initial counter: 0x%llx\n", count1);

    timer_initialized = 1;
    return 0;
}

/*
 * Get current timer ticks (for TimeManager)
 * Returns 64-bit counter value
 */
uint64_t arm_get_timer_ticks(void) {
    if (!timer_initialized) {
        return 0;
    }

    return arm_read_timer();
}

/*
 * Get timer frequency (Hz)
 * Used by TimeManager to convert ticks to microseconds
 */
uint32_t arm_get_timer_frequency(void) {
    return arm_timer_freq;
}

/*
 * Set timer frequency
 * Typically called if we detect a different Pi model with different timer freq
 */
void arm_set_timer_frequency(uint32_t freq_hz) {
    if (freq_hz > 0) {
        arm_timer_freq = freq_hz;
        Serial_Printf("[TIMER] Timer frequency set to %lu Hz\n", (unsigned long)freq_hz);
    }
}

/*
 * Calibrate timer if needed
 * Some systems may have variable clock speeds
 */
uint32_t arm_calibrate_timer(void) {
    if (!timer_initialized) {
        return 0;
    }

    Serial_WriteString("[TIMER] Calibrating ARM timer\n");

    /* Read timer before and after known delay
     * This is a stub - real implementation would do sophisticated calibration
     */
    uint64_t ticks_before = arm_read_timer();

    /* Approximate 1 millisecond delay (empirical) */
    for (volatile int i = 0; i < 19200; i++) {  /* 19.2M / 1000 = ~19,200 for 1ms */
        __asm__ __volatile__("nop");
    }

    uint64_t ticks_after = arm_read_timer();
    uint64_t ticks_delta = ticks_after - ticks_before;

    Serial_Printf("[TIMER] Ticks per millisecond (approx): %lu\n",
                  (unsigned long)(uint32_t)ticks_delta);

    return (uint32_t)ticks_delta;
}

/*
 * Get current time in microseconds
 * Convenience function combining timer ticks and frequency
 */
uint64_t arm_get_microseconds(void) {
    if (!timer_initialized || arm_timer_freq == 0) {
        return 0;
    }

    uint64_t ticks = arm_read_timer();

    /* Convert ticks to microseconds: (ticks / freq_hz) * 1,000,000 */
    uint64_t microseconds = (ticks * 1000000) / arm_timer_freq;

    return microseconds;
}

/*
 * Get milliseconds (for compatibility)
 */
uint32_t arm_get_milliseconds(void) {
    uint64_t us = arm_get_microseconds();
    return (uint32_t)(us / 1000);
}

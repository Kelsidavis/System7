/*
 * MicrosecondTimer.h
 *
 * High-Resolution Timing APIs for System 7.1 Time Manager
 *
 * This file provides high-resolution timing services, implementing the
 * _MicroSeconds trap and related functionality from the original Time Manager.
 */

#ifndef MICROSECONDTIMER_H
#define MICROSECONDTIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "TimerTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Microsecond Timer Functions ===== */

/**
 * Get 64-bit Microsecond Counter
 *
 * Returns the current value of the 64-bit microsecond counter, which is
 * useful for timestamping and performance measurements. This implements
 * the _MicroSeconds trap from the original Time Manager.
 *
 * The counter represents the number of microseconds since system startup
 * and provides high-resolution timing capabilities.
 *
 * @param microTickCount - Pointer to UnsignedWide to receive the counter value
 *                        (hi = high 32 bits, lo = low 32 bits)
 */
void Microseconds(UnsignedWide *microTickCount);

/**
 * Get Time Base Information
 *
 * Returns information about the system's time base, including conversion
 * factors and timing resolution. This information can be used to convert
 * between different time units and determine timing accuracy.
 *
 * @param timeBaseInfo - Pointer to TimeBaseInfo structure to fill
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetTimeBaseInfo(TimeBaseInfo *timeBaseInfo);

/**
 * Convert Absolute Time to Duration
 *
 * Converts an absolute time value to a duration in nanoseconds.
 * This is useful for calculating elapsed time from timestamp differences.
 *
 * @param absoluteTime - Absolute time value to convert
 * @param duration - Pointer to UnsignedWide to receive duration in nanoseconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr AbsoluteToNanoseconds(UnsignedWide absoluteTime, UnsignedWide *duration);

/**
 * Convert Duration to Absolute Time
 *
 * Converts a duration in nanoseconds to an absolute time value.
 * This is useful for setting up precise timing delays.
 *
 * @param duration - Duration in nanoseconds
 * @param absoluteTime - Pointer to UnsignedWide to receive absolute time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr NanosecondsToAbsolute(UnsignedWide duration, UnsignedWide *absoluteTime);

/**
 * Add Time Values
 *
 * Adds two UnsignedWide time values together, handling 64-bit overflow correctly.
 *
 * @param time1 - First time value
 * @param time2 - Second time value
 * @param result - Pointer to UnsignedWide to receive sum
 * @return OSErr - noErr on success, overflow error if result overflows
 */
OSErr AddWideTime(UnsignedWide time1, UnsignedWide time2, UnsignedWide *result);

/**
 * Subtract Time Values
 *
 * Subtracts time2 from time1, handling underflow correctly.
 *
 * @param time1 - Time value to subtract from
 * @param time2 - Time value to subtract
 * @param result - Pointer to UnsignedWide to receive difference
 * @return OSErr - noErr on success, underflow error if time2 > time1
 */
OSErr SubtractWideTime(UnsignedWide time1, UnsignedWide time2, UnsignedWide *result);

/* ===== High-Resolution Delay Functions ===== */

/**
 * Microsecond Delay
 *
 * Performs a high-precision delay for the specified number of microseconds.
 * This function provides accurate timing delays using the system's highest
 * resolution timer.
 *
 * @param microseconds - Number of microseconds to delay
 * @return OSErr - noErr on success, error code on failure
 */
OSErr MicrosecondDelay(uint32_t microseconds);

/**
 * Nanosecond Delay
 *
 * Performs an extremely high-precision delay for the specified number of
 * nanoseconds. Accuracy depends on system capabilities.
 *
 * @param nanoseconds - Number of nanoseconds to delay
 * @return OSErr - noErr on success, error code on failure
 */
OSErr NanosecondDelay(uint64_t nanoseconds);

/* ===== Timer Calibration Functions ===== */

/**
 * Calibrate Timer
 *
 * Performs runtime calibration of the timer system to account for
 * processor speed and system characteristics. This is called during
 * Time Manager initialization.
 *
 * @return OSErr - noErr on success, error code on failure
 */
OSErr CalibrateTimer(void);

/**
 * Get Timer Overhead
 *
 * Returns the measured overhead of timer operations in microseconds.
 * This value is used to adjust timer calculations for accuracy.
 *
 * @return uint32_t - Timer overhead in microseconds
 */
uint32_t GetTimerOverhead(void);

/**
 * Get Timer Resolution
 *
 * Returns the resolution of the system timer in nanoseconds.
 * This represents the smallest measurable time difference.
 *
 * @return uint64_t - Timer resolution in nanoseconds
 */
uint64_t GetTimerResolution(void);

/* ===== Platform-Specific Timer Functions ===== */

/**
 * Get Platform Time
 *
 * Gets the current time from the platform's highest resolution timer.
 * This is used internally for all high-precision timing operations.
 *
 * @param timeValue - Pointer to UnsignedWide to receive current time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetPlatformTime(UnsignedWide *timeValue);

/**
 * Get Platform Frequency
 *
 * Returns the frequency of the platform's high-resolution timer in Hz.
 * This is used for time conversion calculations.
 *
 * @return uint64_t - Timer frequency in Hz
 */
uint64_t GetPlatformFrequency(void);

/**
 * Initialize Platform Timer
 *
 * Initializes the platform-specific timer subsystem.
 * Called during Time Manager initialization.
 *
 * @return OSErr - noErr on success, error code on failure
 */
OSErr InitPlatformTimer(void);

/**
 * Shutdown Platform Timer
 *
 * Shuts down the platform-specific timer subsystem.
 * Called during Time Manager shutdown.
 */
void ShutdownPlatformTimer(void);

/* ===== Conversion Utilities ===== */

/**
 * Convert UnsignedWide to Microseconds
 *
 * Converts a platform-specific time value to microseconds.
 *
 * @param timeValue - Platform time value
 * @param microseconds - Pointer to UnsignedWide to receive microseconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr TimeToMicroseconds(UnsignedWide timeValue, UnsignedWide *microseconds);

/**
 * Convert Microseconds to UnsignedWide
 *
 * Converts microseconds to a platform-specific time value.
 *
 * @param microseconds - Microseconds value
 * @param timeValue - Pointer to UnsignedWide to receive platform time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr MicrosecondsToTime(UnsignedWide microseconds, UnsignedWide *timeValue);

/**
 * Convert to Nanoseconds with High Precision
 *
 * Converts a time value to nanoseconds using high-precision arithmetic
 * to minimize rounding errors.
 *
 * @param timeValue - Time value to convert
 * @param nanoseconds - Pointer to UnsignedWide to receive nanoseconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr TimeToNanoseconds(UnsignedWide timeValue, UnsignedWide *nanoseconds);

/* ===== Performance Measurement Utilities ===== */

/**
 * Start Performance Timer
 *
 * Starts a high-resolution performance measurement timer.
 * Returns a timestamp that can be used with EndPerformanceTimer.
 *
 * @param startTime - Pointer to UnsignedWide to receive start timestamp
 * @return OSErr - noErr on success, error code on failure
 */
OSErr StartPerformanceTimer(UnsignedWide *startTime);

/**
 * End Performance Timer
 *
 * Ends a performance measurement and returns the elapsed time.
 *
 * @param startTime - Start timestamp from StartPerformanceTimer
 * @param elapsedTime - Pointer to UnsignedWide to receive elapsed microseconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr EndPerformanceTimer(UnsignedWide startTime, UnsignedWide *elapsedTime);

/* ===== Timing Constants ===== */

/* Conversion factors */
#define NANOSECONDS_PER_SECOND      1000000000ULL
#define NANOSECONDS_PER_MICROSECOND 1000ULL
#define MICROSECONDS_PER_SECOND     1000000UL

/* Time limits */
#define MAX_MICROSECOND_DELAY       4294967295UL    /* Maximum 32-bit microseconds */
#define MAX_NANOSECOND_DELAY        18446744073709551615ULL /* Maximum 64-bit nanoseconds */

/* Timer accuracy thresholds */
#define MIN_ACCURATE_DELAY_US       10              /* Minimum accurately measurable delay in μs */
#define MIN_ACCURATE_DELAY_NS       10000           /* Minimum accurately measurable delay in ns */

#ifdef __cplusplus
}
#endif

#endif /* MICROSECONDTIMER_H */
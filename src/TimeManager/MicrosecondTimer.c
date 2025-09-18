/*
 * MicrosecondTimer.c
 *
 * High-Resolution Timing Services for System 7.1 Time Manager
 *
 * This file implements high-resolution timing services, including the
 * _MicroSeconds trap and related functionality from the original Time Manager.
 *
 * Provides microsecond-precision timing capabilities essential for performance
 * measurement and high-precision timing operations.
 */

#include "MicrosecondTimer.h"
#include "TimeManager.h"
#include "TimerTypes.h"
#include "TimeBase.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

/* ===== Platform-Specific Includes ===== */
#ifdef __MACH__
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/clock.h>
#include <mach/mach_host.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

/* ===== External References ===== */
extern TimeMgrPrivate *gTimeMgrPrivate;
extern pthread_mutex_t gTimeMgrMutex;
extern bool gTimeMgrInitialized;

/* ===== Global State ===== */
static TimeBaseInfo gTimeBaseInfo;
static bool gPlatformTimerInitialized = false;
static uint64_t gPlatformFrequency = 0;
static uint64_t gTimerOverhead = 0;
static pthread_mutex_t gMicrosecondMutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef __MACH__
static mach_timebase_info_data_t gMachTimebase;
#endif

#ifdef _WIN32
static LARGE_INTEGER gPerformanceFrequency;
#endif

/* ===== Private Function Prototypes ===== */
static OSErr CalibrateTimerOverhead(void);
static uint64_t GetRawPlatformTime(void);
static OSErr ConvertRawTimeToMicroseconds(uint64_t rawTime, UnsignedWide *microseconds);

/* ===== Platform Timer Initialization ===== */

/**
 * Initialize Platform Timer
 *
 * Sets up the platform-specific high-resolution timer system.
 */
OSErr InitPlatformTimer(void)
{
    OSErr err = NO_ERR;

    if (gPlatformTimerInitialized) {
        return NO_ERR;
    }

    pthread_mutex_lock(&gMicrosecondMutex);

    if (gPlatformTimerInitialized) {
        pthread_mutex_unlock(&gMicrosecondMutex);
        return NO_ERR;
    }

#ifdef __MACH__
    // macOS: Use mach_absolute_time
    if (mach_timebase_info(&gMachTimebase) != KERN_SUCCESS) {
        pthread_mutex_unlock(&gMicrosecondMutex);
        return -1;
    }
    gPlatformFrequency = 1000000000ULL; // Nanoseconds
#elif defined(_WIN32)
    // Windows: Use QueryPerformanceCounter
    if (!QueryPerformanceFrequency(&gPerformanceFrequency)) {
        pthread_mutex_unlock(&gMicrosecondMutex);
        return -1;
    }
    gPlatformFrequency = gPerformanceFrequency.QuadPart;
#else
    // Unix/Linux: Use clock_gettime
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) != 0) {
        // Fall back to CLOCK_REALTIME
        if (clock_getres(CLOCK_REALTIME, &res) != 0) {
            pthread_mutex_unlock(&gMicrosecondMutex);
            return -1;
        }
    }
    gPlatformFrequency = 1000000000ULL; // Nanoseconds
#endif

    // Initialize time base info
    gTimeBaseInfo.numerator = 1000000; // Convert to microseconds
    gTimeBaseInfo.denominator = (uint32_t)(gPlatformFrequency / 1000000); // From platform frequency
    gTimeBaseInfo.minDelta = 1; // 1 microsecond minimum
    gTimeBaseInfo.maxDelta = 0xFFFFFFFF; // Maximum 32-bit value

    // Calibrate timer overhead
    err = CalibrateTimerOverhead();
    if (err != NO_ERR) {
        // Non-fatal - continue with default overhead
        gTimerOverhead = 100; // Default 100ns overhead
    }

    gPlatformTimerInitialized = true;
    pthread_mutex_unlock(&gMicrosecondMutex);

    return NO_ERR;
}

/**
 * Shutdown Platform Timer
 *
 * Cleans up platform-specific timer resources.
 */
void ShutdownPlatformTimer(void)
{
    pthread_mutex_lock(&gMicrosecondMutex);
    gPlatformTimerInitialized = false;
    gPlatformFrequency = 0;
    gTimerOverhead = 0;
    pthread_mutex_unlock(&gMicrosecondMutex);
}

/* ===== High-Resolution Timer Functions ===== */

/**
 * Get 64-bit Microsecond Counter
 *
 * Returns the current value of the 64-bit microsecond counter, implementing
 * the _MicroSeconds trap from the original Time Manager.
 */
void Microseconds(UnsignedWide *microTickCount)
{
    UnsignedWide currentTime;
    OSErr err;

    if (!microTickCount) {
        return;
    }

    if (!gPlatformTimerInitialized) {
        // Return zero if not initialized
        microTickCount->parts.hi = 0;
        microTickCount->parts.lo = 0;
        return;
    }

    // Get current platform time
    err = GetPlatformTime(&currentTime);
    if (err != NO_ERR) {
        microTickCount->parts.hi = 0;
        microTickCount->parts.lo = 0;
        return;
    }

    // Convert to microseconds
    err = ConvertRawTimeToMicroseconds(currentTime.value, microTickCount);
    if (err != NO_ERR) {
        microTickCount->parts.hi = 0;
        microTickCount->parts.lo = 0;
        return;
    }

    // If Time Manager is initialized, use its microsecond counter for consistency
    if (gTimeMgrInitialized && gTimeMgrPrivate) {
        pthread_mutex_lock(&gTimeMgrMutex);

        // Use the Time Manager's microsecond counter (implements original behavior)
        microTickCount->parts.hi = gTimeMgrPrivate->highUSecs;
        microTickCount->parts.lo = gTimeMgrPrivate->lowUSecs;

        pthread_mutex_unlock(&gTimeMgrMutex);
    }
}

/**
 * Get Platform Time
 *
 * Gets the current time from the platform's highest resolution timer.
 */
OSErr GetPlatformTime(UnsignedWide *timeValue)
{
    if (!timeValue) {
        return -1;
    }

    if (!gPlatformTimerInitialized) {
        return -2;
    }

    uint64_t rawTime = GetRawPlatformTime();
    if (rawTime == 0) {
        return -3;
    }

    timeValue->value = rawTime;
    return NO_ERR;
}

/**
 * Get Platform Frequency
 *
 * Returns the frequency of the platform's high-resolution timer in Hz.
 */
uint64_t GetPlatformFrequency(void)
{
    return gPlatformFrequency;
}

/**
 * Get Time Base Information
 *
 * Returns information about the system's time base.
 */
OSErr GetTimeBaseInfo(TimeBaseInfo *timeBaseInfo)
{
    if (!timeBaseInfo) {
        return -1;
    }

    if (!gPlatformTimerInitialized) {
        return -2;
    }

    *timeBaseInfo = gTimeBaseInfo;
    return NO_ERR;
}

/* ===== Time Conversion Functions ===== */

/**
 * Convert Absolute Time to Duration in Nanoseconds
 */
OSErr AbsoluteToNanoseconds(UnsignedWide absoluteTime, UnsignedWide *duration)
{
    if (!duration) {
        return -1;
    }

    if (!gPlatformTimerInitialized) {
        return -2;
    }

#ifdef __MACH__
    // macOS: Convert using timebase
    duration->value = absoluteTime.value * gMachTimebase.numer / gMachTimebase.denom;
#elif defined(_WIN32)
    // Windows: Convert from performance counter to nanoseconds
    duration->value = (absoluteTime.value * 1000000000ULL) / gPlatformFrequency;
#else
    // Unix/Linux: Already in nanoseconds
    duration->value = absoluteTime.value;
#endif

    return NO_ERR;
}

/**
 * Convert Duration to Absolute Time
 */
OSErr NanosecondsToAbsolute(UnsignedWide duration, UnsignedWide *absoluteTime)
{
    if (!absoluteTime) {
        return -1;
    }

    if (!gPlatformTimerInitialized) {
        return -2;
    }

#ifdef __MACH__
    // macOS: Convert using timebase
    absoluteTime->value = duration.value * gMachTimebase.denom / gMachTimebase.numer;
#elif defined(_WIN32)
    // Windows: Convert from nanoseconds to performance counter
    absoluteTime->value = (duration.value * gPlatformFrequency) / 1000000000ULL;
#else
    // Unix/Linux: Already in nanoseconds
    absoluteTime->value = duration.value;
#endif

    return NO_ERR;
}

/**
 * Add Time Values
 */
OSErr AddWideTime(UnsignedWide time1, UnsignedWide time2, UnsignedWide *result)
{
    if (!result) {
        return -1;
    }

    // Check for overflow
    if (time1.value > (0xFFFFFFFFFFFFFFFFULL - time2.value)) {
        return -2; // Overflow
    }

    result->value = time1.value + time2.value;
    return NO_ERR;
}

/**
 * Subtract Time Values
 */
OSErr SubtractWideTime(UnsignedWide time1, UnsignedWide time2, UnsignedWide *result)
{
    if (!result) {
        return -1;
    }

    // Check for underflow
    if (time1.value < time2.value) {
        return -2; // Underflow
    }

    result->value = time1.value - time2.value;
    return NO_ERR;
}

/* ===== High-Precision Delay Functions ===== */

/**
 * Microsecond Delay
 *
 * Performs a high-precision delay for the specified number of microseconds.
 */
OSErr MicrosecondDelay(uint32_t microseconds)
{
    if (microseconds == 0) {
        return NO_ERR;
    }

    if (microseconds > MAX_MICROSECOND_DELAY) {
        return -1; // Delay too large
    }

#ifdef __MACH__
    // macOS: Use mach_wait_until for high precision
    uint64_t deadline = mach_absolute_time() +
                       (microseconds * 1000ULL * gMachTimebase.denom) / gMachTimebase.numer;
    mach_wait_until(deadline);
#elif defined(_WIN32)
    // Windows: Use Sleep or high-resolution timer
    if (microseconds >= 1000) {
        Sleep(microseconds / 1000);
        microseconds %= 1000;
    }
    if (microseconds > 0) {
        // For sub-millisecond delays, use busy wait with QueryPerformanceCounter
        LARGE_INTEGER start, current, frequency;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);
        uint64_t targetTicks = (microseconds * frequency.QuadPart) / 1000000;
        do {
            QueryPerformanceCounter(&current);
        } while ((current.QuadPart - start.QuadPart) < targetTicks);
    }
#else
    // Unix/Linux: Use nanosleep
    struct timespec delay;
    delay.tv_sec = microseconds / 1000000;
    delay.tv_nsec = (microseconds % 1000000) * 1000;

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
        // Continue if interrupted by signal
    }
#endif

    return NO_ERR;
}

/**
 * Nanosecond Delay
 *
 * Performs an extremely high-precision delay for the specified number of nanoseconds.
 */
OSErr NanosecondDelay(uint64_t nanoseconds)
{
    if (nanoseconds == 0) {
        return NO_ERR;
    }

    if (nanoseconds > MAX_NANOSECOND_DELAY) {
        return -1; // Delay too large
    }

    // Convert to microseconds and use MicrosecondDelay for most of the delay
    uint32_t microseconds = (uint32_t)(nanoseconds / 1000);
    uint32_t remainingNanos = (uint32_t)(nanoseconds % 1000);

    if (microseconds > 0) {
        OSErr err = MicrosecondDelay(microseconds);
        if (err != NO_ERR) {
            return err;
        }
    }

    if (remainingNanos > 0) {
#ifdef __MACH__
        // macOS: Use mach_wait_until
        uint64_t deadline = mach_absolute_time() +
                           (remainingNanos * gMachTimebase.denom) / gMachTimebase.numer;
        mach_wait_until(deadline);
#elif defined(_WIN32)
        // Windows: Busy wait for nanosecond precision
        LARGE_INTEGER start, current, frequency;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);
        uint64_t targetTicks = (remainingNanos * frequency.QuadPart) / 1000000000ULL;
        if (targetTicks > 0) {
            do {
                QueryPerformanceCounter(&current);
            } while ((current.QuadPart - start.QuadPart) < targetTicks);
        }
#else
        // Unix/Linux: Use nanosleep
        struct timespec delay;
        delay.tv_sec = 0;
        delay.tv_nsec = remainingNanos;

        while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
            // Continue if interrupted by signal
        }
#endif
    }

    return NO_ERR;
}

/* ===== Timer Calibration ===== */

/**
 * Calibrate Timer
 *
 * Performs runtime calibration of the timer system.
 */
OSErr CalibrateTimer(void)
{
    if (!gPlatformTimerInitialized) {
        return -1;
    }

    return CalibrateTimerOverhead();
}

/**
 * Get Timer Overhead
 *
 * Returns the measured overhead of timer operations in nanoseconds.
 */
uint32_t GetTimerOverhead(void)
{
    return (uint32_t)gTimerOverhead;
}

/**
 * Get Timer Resolution
 *
 * Returns the resolution of the system timer in nanoseconds.
 */
uint64_t GetTimerResolution(void)
{
    if (!gPlatformTimerInitialized) {
        return 1000; // Default 1 microsecond
    }

#ifdef __MACH__
    return gMachTimebase.numer / gMachTimebase.denom;
#elif defined(_WIN32)
    return 1000000000ULL / gPlatformFrequency;
#else
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) == 0) {
        return res.tv_sec * 1000000000ULL + res.tv_nsec;
    }
    return 1000; // Default 1 microsecond
#endif
}

/* ===== Performance Measurement ===== */

/**
 * Start Performance Timer
 *
 * Starts a high-resolution performance measurement timer.
 */
OSErr StartPerformanceTimer(UnsignedWide *startTime)
{
    return GetPlatformTime(startTime);
}

/**
 * End Performance Timer
 *
 * Ends a performance measurement and returns the elapsed time in microseconds.
 */
OSErr EndPerformanceTimer(UnsignedWide startTime, UnsignedWide *elapsedTime)
{
    UnsignedWide endTime, duration;
    OSErr err;

    err = GetPlatformTime(&endTime);
    if (err != NO_ERR) {
        return err;
    }

    err = SubtractWideTime(endTime, startTime, &duration);
    if (err != NO_ERR) {
        return err;
    }

    return ConvertRawTimeToMicroseconds(duration.value, elapsedTime);
}

/* ===== Private Implementation Functions ===== */

/**
 * Get Raw Platform Time
 *
 * Gets the raw time value from the platform's highest resolution timer.
 */
static uint64_t GetRawPlatformTime(void)
{
#ifdef __MACH__
    return mach_absolute_time();
#elif defined(_WIN32)
    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter)) {
        return counter.QuadPart;
    }
    return 0;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    return 0;
#endif
}

/**
 * Convert Raw Time to Microseconds
 *
 * Converts a raw platform time value to microseconds.
 */
static OSErr ConvertRawTimeToMicroseconds(uint64_t rawTime, UnsignedWide *microseconds)
{
    if (!microseconds) {
        return -1;
    }

#ifdef __MACH__
    // Convert from mach absolute time to microseconds
    uint64_t nanos = rawTime * gMachTimebase.numer / gMachTimebase.denom;
    microseconds->value = nanos / 1000;
#elif defined(_WIN32)
    // Convert from performance counter to microseconds
    microseconds->value = (rawTime * 1000000ULL) / gPlatformFrequency;
#else
    // Convert from nanoseconds to microseconds
    microseconds->value = rawTime / 1000;
#endif

    return NO_ERR;
}

/**
 * Calibrate Timer Overhead
 *
 * Measures the overhead of timer function calls to improve accuracy.
 */
static OSErr CalibrateTimerOverhead(void)
{
    const int numSamples = 1000;
    uint64_t totalOverhead = 0;
    UnsignedWide start, end, diff;
    int validSamples = 0;

    // Measure timer call overhead
    for (int i = 0; i < numSamples; i++) {
        GetPlatformTime(&start);
        GetPlatformTime(&end);

        if (end.value > start.value) {
            diff.value = end.value - start.value;
            if (ConvertRawTimeToMicroseconds(diff.value, &diff) == NO_ERR) {
                // Convert to nanoseconds
                totalOverhead += diff.value * 1000;
                validSamples++;
            }
        }
    }

    if (validSamples > 0) {
        gTimerOverhead = totalOverhead / validSamples;
    } else {
        gTimerOverhead = 100; // Default 100ns overhead
    }

    return NO_ERR;
}
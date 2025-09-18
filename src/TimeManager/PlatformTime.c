/*
 * PlatformTime.c
 *
 * Modern Platform Timing Abstraction for System 7.1 Time Manager
 *
 * This file provides a platform-independent abstraction layer for modern
 * high-resolution timing APIs, enabling the Time Manager to work efficiently
 * across different operating systems and hardware platforms.
 */

#include "TimeManager.h"
#include "TimerTypes.h"
#include "MicrosecondTimer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

/* ===== Platform-Specific Includes ===== */
#ifdef __MACH__
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <winnt.h>
#include <intrin.h>
#endif

#ifdef __linux__
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#endif

/* ===== Platform Detection ===== */
typedef enum {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_MACOS,
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_FREEBSD,
    PLATFORM_GENERIC_POSIX
} PlatformType;

typedef enum {
    TIMER_SOURCE_UNKNOWN = 0,
    TIMER_SOURCE_MACH_ABSOLUTE,     // macOS mach_absolute_time
    TIMER_SOURCE_QPC,               // Windows QueryPerformanceCounter
    TIMER_SOURCE_CLOCK_MONOTONIC,   // POSIX clock_gettime(CLOCK_MONOTONIC)
    TIMER_SOURCE_CLOCK_REALTIME,    // POSIX clock_gettime(CLOCK_REALTIME)
    TIMER_SOURCE_TSC,               // x86 Time Stamp Counter
    TIMER_SOURCE_GETTIMEOFDAY       // Legacy gettimeofday
} TimerSourceType;

/* ===== Platform-Specific State ===== */
static struct {
    PlatformType platform;
    TimerSourceType timerSource;
    uint64_t frequency;             // Timer frequency in Hz
    uint64_t overhead;              // Measured timer overhead in nanoseconds
    bool isHighResolution;          // True if timer has sub-microsecond resolution
    bool isMonotonic;               // True if timer is monotonic
    void *platformData;             // Platform-specific data
    pthread_mutex_t mutex;          // Thread safety
} gPlatformTimer = {0};

#ifdef __MACH__
static mach_timebase_info_data_t gMachTimebase;
#endif

#ifdef _WIN32
static LARGE_INTEGER gPerformanceFrequency;
static bool gHasInvariantTSC = false;
#endif

/* ===== Private Function Prototypes ===== */
static OSErr DetectPlatform(void);
static OSErr InitializePlatformTimer(void);
static uint64_t GetRawTimerValue(void);
static OSErr ConvertRawToNanoseconds(uint64_t rawValue, uint64_t *nanoseconds);
static OSErr MeasureTimerOverhead(void);
static OSErr DetectCPUFeatures(void);
static uint64_t ReadTSC(void);
static bool IsInvariantTSCAvailable(void);

/* ===== Platform Timer Initialization ===== */

/**
 * Initialize Platform Timer System
 *
 * Detects the platform and initializes the best available high-resolution timer.
 */
OSErr InitPlatformTimer(void)
{
    OSErr err;

    if (gPlatformTimer.platform != PLATFORM_UNKNOWN) {
        return NO_ERR; // Already initialized
    }

    pthread_mutex_init(&gPlatformTimer.mutex, NULL);
    pthread_mutex_lock(&gPlatformTimer.mutex);

    // Detect platform and available timer sources
    err = DetectPlatform();
    if (err != NO_ERR) {
        pthread_mutex_unlock(&gPlatformTimer.mutex);
        return err;
    }

    // Initialize the selected timer source
    err = InitializePlatformTimer();
    if (err != NO_ERR) {
        pthread_mutex_unlock(&gPlatformTimer.mutex);
        return err;
    }

    // Measure timer overhead for accuracy correction
    err = MeasureTimerOverhead();
    if (err != NO_ERR) {
        // Non-fatal - use default overhead estimate
        gPlatformTimer.overhead = 100; // 100ns default
    }

    pthread_mutex_unlock(&gPlatformTimer.mutex);

    return NO_ERR;
}

/**
 * Shutdown Platform Timer System
 *
 * Cleans up platform-specific timer resources.
 */
void ShutdownPlatformTimer(void)
{
    pthread_mutex_lock(&gPlatformTimer.mutex);

    if (gPlatformTimer.platformData) {
        free(gPlatformTimer.platformData);
        gPlatformTimer.platformData = NULL;
    }

    memset(&gPlatformTimer, 0, sizeof(gPlatformTimer));

    pthread_mutex_unlock(&gPlatformTimer.mutex);
    pthread_mutex_destroy(&gPlatformTimer.mutex);
}

/* ===== High-Level Platform Timer Functions ===== */

/**
 * Get Platform Time
 *
 * Returns the current time from the platform's highest resolution timer.
 */
OSErr GetPlatformTime(UnsignedWide *timeValue)
{
    uint64_t rawTime;
    OSErr err;

    if (!timeValue) {
        return -1;
    }

    if (gPlatformTimer.platform == PLATFORM_UNKNOWN) {
        return -2; // Not initialized
    }

    rawTime = GetRawTimerValue();
    if (rawTime == 0) {
        return -3; // Timer read failed
    }

    timeValue->value = rawTime;
    return NO_ERR;
}

/**
 * Get Platform Frequency
 *
 * Returns the frequency of the platform timer in Hz.
 */
uint64_t GetPlatformFrequency(void)
{
    return gPlatformTimer.frequency;
}

/**
 * Get Timer Resolution
 *
 * Returns the resolution of the platform timer in nanoseconds.
 */
uint64_t GetTimerResolution(void)
{
    if (gPlatformTimer.frequency == 0) {
        return 1000000; // 1ms default
    }

    return 1000000000ULL / gPlatformTimer.frequency;
}

/**
 * Is High Resolution Timer Available
 *
 * Returns true if a high-resolution timer is available.
 */
bool IsHighResolutionTimerAvailable(void)
{
    return gPlatformTimer.isHighResolution;
}

/**
 * Is Monotonic Timer Available
 *
 * Returns true if the timer is monotonic (never goes backwards).
 */
bool IsMonotonicTimerAvailable(void)
{
    return gPlatformTimer.isMonotonic;
}

/* ===== Time Conversion Functions ===== */

/**
 * Convert Platform Time to Nanoseconds
 *
 * Converts a platform-specific time value to nanoseconds.
 */
OSErr PlatformTimeToNanoseconds(UnsignedWide platformTime, uint64_t *nanoseconds)
{
    if (!nanoseconds) {
        return -1;
    }

    return ConvertRawToNanoseconds(platformTime.value, nanoseconds);
}

/**
 * Convert Nanoseconds to Platform Time
 *
 * Converts nanoseconds to a platform-specific time value.
 */
OSErr NanosecondsToPlatformTime(uint64_t nanoseconds, UnsignedWide *platformTime)
{
    if (!platformTime) {
        return -1;
    }

    switch (gPlatformTimer.timerSource) {
#ifdef __MACH__
        case TIMER_SOURCE_MACH_ABSOLUTE:
            platformTime->value = nanoseconds * gMachTimebase.denom / gMachTimebase.numer;
            break;
#endif
#ifdef _WIN32
        case TIMER_SOURCE_QPC:
            platformTime->value = (nanoseconds * gPlatformTimer.frequency) / 1000000000ULL;
            break;
#endif
        case TIMER_SOURCE_CLOCK_MONOTONIC:
        case TIMER_SOURCE_CLOCK_REALTIME:
            platformTime->value = nanoseconds;
            break;

        default:
            platformTime->value = nanoseconds;
            break;
    }

    return NO_ERR;
}

/**
 * Get Timer Overhead
 *
 * Returns the measured timer overhead in nanoseconds.
 */
uint64_t GetTimerOverhead(void)
{
    return gPlatformTimer.overhead;
}

/* ===== Private Implementation Functions ===== */

/**
 * Detect Platform
 *
 * Detects the current platform and available timer sources.
 */
static OSErr DetectPlatform(void)
{
#ifdef __MACH__
    gPlatformTimer.platform = PLATFORM_MACOS;
    gPlatformTimer.timerSource = TIMER_SOURCE_MACH_ABSOLUTE;
    gPlatformTimer.isHighResolution = true;
    gPlatformTimer.isMonotonic = true;
#elif defined(_WIN32)
    gPlatformTimer.platform = PLATFORM_WINDOWS;

    // Check if QueryPerformanceCounter is available
    if (QueryPerformanceFrequency(&gPerformanceFrequency) && gPerformanceFrequency.QuadPart > 0) {
        gPlatformTimer.timerSource = TIMER_SOURCE_QPC;
        gPlatformTimer.frequency = gPerformanceFrequency.QuadPart;
        gPlatformTimer.isHighResolution = (gPerformanceFrequency.QuadPart >= 1000000); // 1MHz+
        gPlatformTimer.isMonotonic = true;
    } else {
        return -1; // No suitable timer found
    }
#elif defined(__linux__)
    gPlatformTimer.platform = PLATFORM_LINUX;

    // Try CLOCK_MONOTONIC first
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) == 0) {
        gPlatformTimer.timerSource = TIMER_SOURCE_CLOCK_MONOTONIC;
        gPlatformTimer.frequency = 1000000000ULL; // Nanoseconds
        gPlatformTimer.isHighResolution = (res.tv_nsec <= 1000); // 1μs or better
        gPlatformTimer.isMonotonic = true;
    } else if (clock_getres(CLOCK_REALTIME, &res) == 0) {
        gPlatformTimer.timerSource = TIMER_SOURCE_CLOCK_REALTIME;
        gPlatformTimer.frequency = 1000000000ULL; // Nanoseconds
        gPlatformTimer.isHighResolution = (res.tv_nsec <= 1000); // 1μs or better
        gPlatformTimer.isMonotonic = false;
    } else {
        return -1; // No suitable timer found
    }
#else
    // Generic POSIX fallback
    gPlatformTimer.platform = PLATFORM_GENERIC_POSIX;

    struct timespec res;
    if (clock_getres(CLOCK_REALTIME, &res) == 0) {
        gPlatformTimer.timerSource = TIMER_SOURCE_CLOCK_REALTIME;
        gPlatformTimer.frequency = 1000000000ULL;
        gPlatformTimer.isHighResolution = (res.tv_nsec <= 10000); // 10μs or better
        gPlatformTimer.isMonotonic = false;
    } else {
        gPlatformTimer.timerSource = TIMER_SOURCE_GETTIMEOFDAY;
        gPlatformTimer.frequency = 1000000ULL; // Microseconds
        gPlatformTimer.isHighResolution = false;
        gPlatformTimer.isMonotonic = false;
    }
#endif

    return NO_ERR;
}

/**
 * Initialize Platform Timer
 *
 * Performs platform-specific timer initialization.
 */
static OSErr InitializePlatformTimer(void)
{
    switch (gPlatformTimer.timerSource) {
#ifdef __MACH__
        case TIMER_SOURCE_MACH_ABSOLUTE:
            if (mach_timebase_info(&gMachTimebase) != KERN_SUCCESS) {
                return -1;
            }
            gPlatformTimer.frequency = 1000000000ULL; // Nanoseconds after conversion
            break;
#endif

#ifdef _WIN32
        case TIMER_SOURCE_QPC:
            // Already initialized in DetectPlatform
            DetectCPUFeatures(); // Check for invariant TSC
            break;
#endif

        case TIMER_SOURCE_CLOCK_MONOTONIC:
        case TIMER_SOURCE_CLOCK_REALTIME:
            // No additional initialization needed
            break;

        case TIMER_SOURCE_GETTIMEOFDAY:
            // Legacy fallback - no initialization needed
            break;

        default:
            return -1;
    }

    return NO_ERR;
}

/**
 * Get Raw Timer Value
 *
 * Reads the raw value from the platform timer.
 */
static uint64_t GetRawTimerValue(void)
{
    switch (gPlatformTimer.timerSource) {
#ifdef __MACH__
        case TIMER_SOURCE_MACH_ABSOLUTE:
            return mach_absolute_time();
#endif

#ifdef _WIN32
        case TIMER_SOURCE_QPC:
            {
                LARGE_INTEGER counter;
                if (QueryPerformanceCounter(&counter)) {
                    return counter.QuadPart;
                }
                return 0;
            }
#endif

        case TIMER_SOURCE_CLOCK_MONOTONIC:
            {
                struct timespec ts;
                if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
                    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
                }
                return 0;
            }

        case TIMER_SOURCE_CLOCK_REALTIME:
            {
                struct timespec ts;
                if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
                    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
                }
                return 0;
            }

        case TIMER_SOURCE_GETTIMEOFDAY:
            {
                struct timeval tv;
                if (gettimeofday(&tv, NULL) == 0) {
                    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
                }
                return 0;
            }

        default:
            return 0;
    }
}

/**
 * Convert Raw Timer Value to Nanoseconds
 *
 * Converts a raw platform timer value to nanoseconds.
 */
static OSErr ConvertRawToNanoseconds(uint64_t rawValue, uint64_t *nanoseconds)
{
    if (!nanoseconds) {
        return -1;
    }

    switch (gPlatformTimer.timerSource) {
#ifdef __MACH__
        case TIMER_SOURCE_MACH_ABSOLUTE:
            *nanoseconds = rawValue * gMachTimebase.numer / gMachTimebase.denom;
            break;
#endif

#ifdef _WIN32
        case TIMER_SOURCE_QPC:
            *nanoseconds = (rawValue * 1000000000ULL) / gPlatformTimer.frequency;
            break;
#endif

        case TIMER_SOURCE_CLOCK_MONOTONIC:
        case TIMER_SOURCE_CLOCK_REALTIME:
            *nanoseconds = rawValue; // Already in nanoseconds
            break;

        case TIMER_SOURCE_GETTIMEOFDAY:
            *nanoseconds = rawValue * 1000; // Convert microseconds to nanoseconds
            break;

        default:
            *nanoseconds = rawValue;
            break;
    }

    return NO_ERR;
}

/**
 * Measure Timer Overhead
 *
 * Measures the overhead of timer function calls.
 */
static OSErr MeasureTimerOverhead(void)
{
    const int numSamples = 1000;
    uint64_t totalOverhead = 0;
    uint64_t start, end;
    int validSamples = 0;

    for (int i = 0; i < numSamples; i++) {
        start = GetRawTimerValue();
        end = GetRawTimerValue();

        if (end > start) {
            uint64_t startNS, endNS;
            if (ConvertRawToNanoseconds(start, &startNS) == NO_ERR &&
                ConvertRawToNanoseconds(end, &endNS) == NO_ERR) {
                totalOverhead += (endNS - startNS);
                validSamples++;
            }
        }
    }

    if (validSamples > 0) {
        gPlatformTimer.overhead = totalOverhead / validSamples;
    } else {
        gPlatformTimer.overhead = 100; // Default 100ns
    }

    return NO_ERR;
}

/**
 * Detect CPU Features
 *
 * Detects CPU-specific timing features (like invariant TSC).
 */
static OSErr DetectCPUFeatures(void)
{
#ifdef _WIN32
    gPlatformTimer.platformData = NULL;
    gHasInvariantTSC = IsInvariantTSCAvailable();
#endif

    return NO_ERR;
}

/**
 * Read Time Stamp Counter (x86/x64)
 *
 * Reads the processor's time stamp counter for high-resolution timing.
 */
static uint64_t ReadTSC(void)
{
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
    return __rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return 0; // Not supported on this architecture
#endif
}

/**
 * Check for Invariant TSC
 *
 * Determines if the processor has an invariant time stamp counter.
 */
static bool IsInvariantTSCAvailable(void)
{
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x80000007);
    return (cpuInfo[3] & 0x100) != 0; // Bit 8 indicates invariant TSC
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x80000007));
    return (edx & 0x100) != 0; // Bit 8 indicates invariant TSC
#else
    return false; // Not supported on this architecture
#endif
}

/* ===== Platform Information Functions ===== */

/**
 * Get Platform Information
 *
 * Returns information about the current platform and timer capabilities.
 */
OSErr GetPlatformTimerInfo(PlatformTimerInfo *info)
{
    if (!info) {
        return -1;
    }

    pthread_mutex_lock(&gPlatformTimer.mutex);

    info->platform = gPlatformTimer.platform;
    info->timerSource = gPlatformTimer.timerSource;
    info->frequency = gPlatformTimer.frequency;
    info->resolution = GetTimerResolution();
    info->overhead = gPlatformTimer.overhead;
    info->isHighResolution = gPlatformTimer.isHighResolution;
    info->isMonotonic = gPlatformTimer.isMonotonic;

    pthread_mutex_unlock(&gPlatformTimer.mutex);

    return NO_ERR;
}

/* ===== Platform Timer Information Structure ===== */
typedef struct PlatformTimerInfo {
    int32_t         platform;          /* Platform type */
    int32_t         timerSource;       /* Timer source type */
    uint64_t        frequency;         /* Timer frequency in Hz */
    uint64_t        resolution;        /* Timer resolution in nanoseconds */
    uint64_t        overhead;          /* Timer overhead in nanoseconds */
    bool            isHighResolution;  /* True if sub-microsecond resolution */
    bool            isMonotonic;       /* True if monotonic */
} PlatformTimerInfo;
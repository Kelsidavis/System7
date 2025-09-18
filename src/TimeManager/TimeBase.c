/*
 * TimeBase.c
 *
 * Time Base Management and System Time Tracking for System 7.1 Time Manager
 *
 * This file implements time base management and system time tracking functionality,
 * providing the core timing infrastructure used by the Time Manager.
 *
 * Handles system time, date/time conversions, time zones, and high-precision
 * time base operations compatible with the original Mac OS implementation.
 */

#include "TimeBase.h"
#include "TimeManager.h"
#include "TimerTypes.h"
#include "MicrosecondTimer.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

/* ===== Global Time Base State ===== */
static SystemTimeBase *gSystemTimeBase = NULL;
static pthread_mutex_t gTimeBaseMutex = PTHREAD_MUTEX_INITIALIZER;
static bool gTimeBaseInitialized = false;

/* Time zone and location data */
static TimeLocationRec gTimeLocation = {0};
static bool gTimeLocationValid = false;

/* Days in month lookup table */
static const uint16_t gDaysInMonth[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* ===== Private Function Prototypes ===== */
static OSErr AllocateTimeBase(void);
static void DeallocateTimeBase(void);
static uint32_t GetSystemSeconds(void);
static OSErr SetSystemSeconds(uint32_t seconds);
static uint16_t GetDaysInMonth(uint16_t year, uint16_t month);
static bool IsValidDate(const DateTimeRec *dateTime);
static uint32_t MacEpochToUnixEpoch(uint32_t macSeconds);
static uint32_t UnixEpochToMacEpoch(uint32_t unixSeconds);

/* ===== Time Base Initialization ===== */

/**
 * Initialize Time Base
 *
 * Sets up the system time base with current time and calibration.
 */
OSErr InitTimeBase(void)
{
    OSErr err;
    time_t currentUnixTime;
    uint64_t startupTime;

    if (gTimeBaseInitialized) {
        return NO_ERR;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    if (gTimeBaseInitialized) {
        pthread_mutex_unlock(&gTimeBaseMutex);
        return NO_ERR;
    }

    // Allocate time base structure
    err = AllocateTimeBase();
    if (err != NO_ERR) {
        pthread_mutex_unlock(&gTimeBaseMutex);
        return err;
    }

    // Get current system time
    time(&currentUnixTime);

    // Convert to Mac OS epoch and store
    gSystemTimeBase->bootTime = UnixEpochToMacEpoch((uint32_t)currentUnixTime);

    // Get high-resolution startup time
    UnsignedWide startTime;
    if (GetPlatformTime(&startTime) == NO_ERR) {
        gSystemTimeBase->systemStartTime = startTime.value;
    } else {
        gSystemTimeBase->systemStartTime = 0;
    }

    // Initialize time base counters
    gSystemTimeBase->currentTime = 0;
    gSystemTimeBase->timeOffset = 0;
    gSystemTimeBase->highMicros = 0;
    gSystemTimeBase->lowMicros = 0;
    gSystemTimeBase->fracMicros = 0;
    gSystemTimeBase->microThreshold = TIMEBASE_UPDATE_THRESHOLD;

    // Set default time zone (GMT)
    gTimeLocation.gmtDelta = 0;
    gTimeLocation.dstDelta = 0;
    gTimeLocation.dstStart = 0;
    gTimeLocation.dstEnd = 0;
    gTimeLocation.dstType = 0;
    gTimeLocationValid = true;

    // Initialize calibration
    gSystemTimeBase->calibrationFactor = 1000000; // 1.0 in fixed point
    gSystemTimeBase->timerOverhead = GetTimerOverhead();
    gSystemTimeBase->isCalibrated = false;

    gTimeBaseInitialized = true;

    pthread_mutex_unlock(&gTimeBaseMutex);

    // Perform initial calibration
    CalibrateTimeBase();

    return NO_ERR;
}

/**
 * Shutdown Time Base
 *
 * Shuts down the time base system and saves any persistent timing data.
 */
void ShutdownTimeBase(void)
{
    if (!gTimeBaseInitialized) {
        return;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    if (!gTimeBaseInitialized) {
        pthread_mutex_unlock(&gTimeBaseMutex);
        return;
    }

    DeallocateTimeBase();
    gTimeBaseInitialized = false;

    pthread_mutex_unlock(&gTimeBaseMutex);
}

/**
 * Update Time Base
 *
 * Updates the time base with current timer values.
 */
OSErr UpdateTimeBase(uint32_t timerValue)
{
    if (!gTimeBaseInitialized || !gSystemTimeBase) {
        return -1;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    // Convert timer value to internal time format
    uint32_t internalTime = MicrosecondsToInternal(timerValue);
    uint32_t timeDelta = internalTime - gSystemTimeBase->currentTime;

    // Update current time
    gSystemTimeBase->currentTime = internalTime;

    // Update microsecond counter with threshold checking (from original algorithm)
    uint16_t timeThresh = gSystemTimeBase->microThreshold;
    uint16_t lowTime = gSystemTimeBase->currentTime & 0xFFFF;

    while (lowTime >= timeThresh) {
        timeThresh += TIMEBASE_UPDATE_THRESHOLD;

        // Add microsecond increment (16.16 fixed point)
        uint32_t fracSum = gSystemTimeBase->fracMicros + (TIMEBASE_USEC_INCREMENT & 0xFFFF);
        gSystemTimeBase->fracMicros = fracSum & 0xFFFF;

        uint32_t microSum = gSystemTimeBase->lowMicros + (TIMEBASE_USEC_INCREMENT >> 16);
        if (fracSum > 0xFFFF) {
            microSum++; // Carry from fraction
        }

        if (microSum < gSystemTimeBase->lowMicros) {
            // Carry to high word
            gSystemTimeBase->highMicros++;
        }
        gSystemTimeBase->lowMicros = microSum;
    }

    gSystemTimeBase->microThreshold = timeThresh;

    pthread_mutex_unlock(&gTimeBaseMutex);

    return NO_ERR;
}

/**
 * Calibrate Time Base
 *
 * Performs runtime calibration of the time base to ensure accuracy.
 */
OSErr CalibrateTimeBase(void)
{
    if (!gTimeBaseInitialized || !gSystemTimeBase) {
        return -1;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    // Simple calibration - measure timer accuracy over short period
    UnsignedWide start, end;
    struct timespec sleepTime = {0, 100000000}; // 100ms

    if (GetPlatformTime(&start) == NO_ERR) {
        nanosleep(&sleepTime, NULL);
        if (GetPlatformTime(&end) == NO_ERR) {
            uint64_t measured = end.value - start.value;
            uint64_t expected = 100000000; // 100ms in nanoseconds

            if (measured > 0 && expected > 0) {
                // Calculate calibration factor as fixed-point ratio
                gSystemTimeBase->calibrationFactor = (uint32_t)((expected * 1000000) / measured);
                gSystemTimeBase->isCalibrated = true;
            }
        }
    }

    pthread_mutex_unlock(&gTimeBaseMutex);

    return NO_ERR;
}

/* ===== System Time Functions ===== */

/**
 * Get Current Time
 *
 * Returns the current system time in seconds since the Mac OS epoch.
 */
OSErr GetCurrentTime(uint32_t *seconds)
{
    if (!seconds) {
        return -1;
    }

    *seconds = GetSystemSeconds();
    return NO_ERR;
}

/**
 * Set Current Time
 *
 * Sets the system time to the specified value.
 */
OSErr SetCurrentTime(uint32_t seconds)
{
    return SetSystemSeconds(seconds);
}

/**
 * Get Date and Time
 *
 * Returns the current date and time in human-readable format.
 */
OSErr GetDateTime(DateTimeRec *dateTime)
{
    uint32_t seconds;
    OSErr err;

    if (!dateTime) {
        return -1;
    }

    err = GetCurrentTime(&seconds);
    if (err != NO_ERR) {
        return err;
    }

    return SecondsToDate(seconds, dateTime);
}

/**
 * Set Date and Time
 *
 * Sets the system date and time from human-readable format.
 */
OSErr SetDateTime(const DateTimeRec *dateTime)
{
    uint32_t seconds;
    OSErr err;

    if (!dateTime) {
        return -1;
    }

    if (!ValidateDateTime(dateTime)) {
        return -2; // Invalid date/time
    }

    err = DateToSeconds(dateTime, &seconds);
    if (err != NO_ERR) {
        return err;
    }

    return SetCurrentTime(seconds);
}

/* ===== High-Resolution Time Functions ===== */

/**
 * Get System Uptime
 *
 * Returns the time since system startup in microseconds.
 */
OSErr GetSystemUptime(UnsignedWide *uptime)
{
    UnsignedWide currentTime;
    OSErr err;

    if (!uptime) {
        return -1;
    }

    if (!gTimeBaseInitialized || !gSystemTimeBase) {
        uptime->value = 0;
        return -2;
    }

    err = GetPlatformTime(&currentTime);
    if (err != NO_ERR) {
        return err;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    if (gSystemTimeBase->systemStartTime != 0) {
        uptime->value = currentTime.value - gSystemTimeBase->systemStartTime;
        // Convert to microseconds if needed (platform dependent)
        uptime->value /= 1000; // Assuming nanoseconds, convert to microseconds
    } else {
        uptime->value = 0;
    }

    pthread_mutex_unlock(&gTimeBaseMutex);

    return NO_ERR;
}

/**
 * Get Absolute Time
 *
 * Returns the current absolute time using the highest resolution timer.
 */
OSErr GetAbsoluteTime(UnsignedWide *absoluteTime)
{
    return GetPlatformTime(absoluteTime);
}

/**
 * Convert Time to Seconds
 *
 * Converts an absolute time value to seconds since Mac OS epoch.
 */
OSErr AbsoluteTimeToSeconds(UnsignedWide absoluteTime, uint32_t *seconds)
{
    if (!seconds) {
        return -1;
    }

    if (!gTimeBaseInitialized || !gSystemTimeBase) {
        return -2;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    // Calculate elapsed time since startup
    uint64_t elapsed = absoluteTime.value - gSystemTimeBase->systemStartTime;
    elapsed /= 1000000; // Convert to seconds (assuming microseconds)

    // Add to boot time
    *seconds = gSystemTimeBase->bootTime + (uint32_t)elapsed;

    pthread_mutex_unlock(&gTimeBaseMutex);

    return NO_ERR;
}

/**
 * Convert Seconds to Time
 *
 * Converts seconds since Mac OS epoch to absolute time value.
 */
OSErr SecondsToAbsoluteTime(uint32_t seconds, UnsignedWide *absoluteTime)
{
    if (!absoluteTime) {
        return -1;
    }

    if (!gTimeBaseInitialized || !gSystemTimeBase) {
        return -2;
    }

    pthread_mutex_lock(&gTimeBaseMutex);

    // Calculate time relative to boot
    uint32_t elapsedSeconds = seconds - gSystemTimeBase->bootTime;

    // Convert to absolute time
    absoluteTime->value = gSystemTimeBase->systemStartTime +
                         ((uint64_t)elapsedSeconds * 1000000); // Convert to microseconds

    pthread_mutex_unlock(&gTimeBaseMutex);

    return NO_ERR;
}

/* ===== Date/Time Conversion Functions ===== */

/**
 * Seconds to DateTimeRec
 *
 * Converts seconds since Mac OS epoch to date/time structure.
 */
OSErr SecondsToDate(uint32_t seconds, DateTimeRec *dateTime)
{
    if (!dateTime) {
        return -1;
    }

    // Convert Mac epoch to Unix epoch for standard library functions
    time_t unixTime = MacEpochToUnixEpoch(seconds);
    struct tm *tmPtr = gmtime(&unixTime);

    if (!tmPtr) {
        return -2; // Invalid time
    }

    // Fill DateTimeRec structure
    dateTime->year = tmPtr->tm_year + 1900;
    dateTime->month = tmPtr->tm_mon + 1;
    dateTime->day = tmPtr->tm_mday;
    dateTime->hour = tmPtr->tm_hour;
    dateTime->minute = tmPtr->tm_min;
    dateTime->second = tmPtr->tm_sec;
    dateTime->dayOfWeek = tmPtr->tm_wday + 1; // Mac OS uses 1-based day of week

    return NO_ERR;
}

/**
 * DateTimeRec to Seconds
 *
 * Converts date/time structure to seconds since Mac OS epoch.
 */
OSErr DateToSeconds(const DateTimeRec *dateTime, uint32_t *seconds)
{
    struct tm tm;
    time_t unixTime;

    if (!dateTime || !seconds) {
        return -1;
    }

    if (!ValidateDateTime(dateTime)) {
        return -2; // Invalid date/time
    }

    // Fill struct tm
    tm.tm_year = dateTime->year - 1900;
    tm.tm_mon = dateTime->month - 1;
    tm.tm_mday = dateTime->day;
    tm.tm_hour = dateTime->hour;
    tm.tm_min = dateTime->minute;
    tm.tm_sec = dateTime->second;
    tm.tm_isdst = 0; // No DST for GMT calculation

    // Convert to Unix time
    unixTime = mktime(&tm);
    if (unixTime == -1) {
        return -3; // Conversion failed
    }

    // Convert to Mac epoch
    *seconds = UnixEpochToMacEpoch((uint32_t)unixTime);

    return NO_ERR;
}

/**
 * Validate Date and Time
 *
 * Validates that a DateTimeRec contains valid date/time values.
 */
bool ValidateDateTime(const DateTimeRec *dateTime)
{
    if (!dateTime) {
        return false;
    }

    // Check year range
    if (dateTime->year < MIN_YEAR || dateTime->year > MAX_YEAR) {
        return false;
    }

    // Check month range
    if (dateTime->month < 1 || dateTime->month > 12) {
        return false;
    }

    // Check day range
    uint16_t daysInMonth = GetDaysInMonth(dateTime->year, dateTime->month);
    if (dateTime->day < 1 || dateTime->day > daysInMonth) {
        return false;
    }

    // Check time ranges
    if (dateTime->hour > 23 || dateTime->minute > 59 || dateTime->second > 59) {
        return false;
    }

    // Check day of week (if specified)
    if (dateTime->dayOfWeek > 7) {
        return false;
    }

    return true;
}

/**
 * Calculate Day of Week
 *
 * Calculates the day of week for a given date using Zeller's congruence.
 */
uint16_t CalculateDayOfWeek(uint16_t year, uint16_t month, uint16_t day)
{
    if (month < 3) {
        month += 12;
        year--;
    }

    int k = year % 100;
    int j = year / 100;

    int h = (day + ((13 * (month + 1)) / 5) + k + (k / 4) + (j / 4) - 2 * j) % 7;

    // Convert to Mac OS day numbering (1=Sunday, 7=Saturday)
    return (h + 6) % 7 + 1;
}

/**
 * Is Leap Year
 *
 * Determines if a given year is a leap year.
 */
bool IsLeapYear(uint16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* ===== Private Implementation Functions ===== */

/**
 * Allocate Time Base
 */
static OSErr AllocateTimeBase(void)
{
    gSystemTimeBase = (SystemTimeBase*)calloc(1, sizeof(SystemTimeBase));
    if (!gSystemTimeBase) {
        return -1; // Memory allocation failed
    }

    return NO_ERR;
}

/**
 * Deallocate Time Base
 */
static void DeallocateTimeBase(void)
{
    if (gSystemTimeBase) {
        free(gSystemTimeBase);
        gSystemTimeBase = NULL;
    }
}

/**
 * Get System Seconds
 */
static uint32_t GetSystemSeconds(void)
{
    time_t currentTime;
    time(&currentTime);
    return UnixEpochToMacEpoch((uint32_t)currentTime);
}

/**
 * Set System Seconds
 */
static OSErr SetSystemSeconds(uint32_t seconds)
{
    // In a real implementation, this would set the system clock
    // For this portable version, we just update our time base
    if (gSystemTimeBase) {
        pthread_mutex_lock(&gTimeBaseMutex);
        gSystemTimeBase->bootTime = seconds;
        pthread_mutex_unlock(&gTimeBaseMutex);
    }
    return NO_ERR;
}

/**
 * Get Days in Month
 */
static uint16_t GetDaysInMonth(uint16_t year, uint16_t month)
{
    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && IsLeapYear(year)) {
        return 29;
    }

    return gDaysInMonth[month - 1];
}

/**
 * Convert Mac Epoch to Unix Epoch
 */
static uint32_t MacEpochToUnixEpoch(uint32_t macSeconds)
{
    // Mac epoch is January 1, 1904, Unix epoch is January 1, 1970
    return macSeconds - TIMEBASE_EPOCH_OFFSET;
}

/**
 * Convert Unix Epoch to Mac Epoch
 */
static uint32_t UnixEpochToMacEpoch(uint32_t unixSeconds)
{
    // Mac epoch is January 1, 1904, Unix epoch is January 1, 1970
    return unixSeconds + TIMEBASE_EPOCH_OFFSET;
}
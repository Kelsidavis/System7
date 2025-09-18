/*
 * TimeBase.h
 *
 * Time Base Management and System Time APIs for System 7.1 Time Manager
 *
 * This file provides time base management and system time tracking functionality,
 * implementing the core timing infrastructure used by the Time Manager.
 */

#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>
#include <stdbool.h>
#include "TimerTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Time Base Constants ===== */

/* Time base scaling factors (from original VIA timer implementation) */
#define TIMEBASE_TICKS_PER_SEC      783360      /* Original VIA Timer clock rate */
#define TIMEBASE_TICK_SCALE         4           /* Internal scaling factor */
#define TIMEBASE_VIRTUAL_RANGE      (0x0000FFFF >> TIMEBASE_TICK_SCALE)

/* Time base update thresholds */
#define TIMEBASE_UPDATE_THRESHOLD   3208        /* Threshold for microsecond updates */
#define TIMEBASE_USEC_INCREMENT     0xFFF2E035  /* Microsecond increment (16.16 fixed) */

/* System time epoch */
#define TIMEBASE_EPOCH_YEAR         1904        /* Mac OS epoch (January 1, 1904) */
#define TIMEBASE_EPOCH_OFFSET       2082844800  /* Seconds from Unix epoch to Mac epoch */

/* ===== Time Base Structure ===== */

/**
 * System Time Base
 *
 * Contains the fundamental timing information used by the Time Manager
 * and other system components for time calculations.
 */
typedef struct SystemTimeBase {
    /* Current time tracking */
    uint32_t        currentTime;        /* Virtual ticks since system start */
    uint32_t        timeOffset;         /* Offset for time base calculations */

    /* Microsecond counter (high precision) */
    uint32_t        highMicros;         /* High 32 bits of microsecond counter */
    uint32_t        lowMicros;          /* Low 32 bits of microsecond counter */
    uint16_t        fracMicros;         /* 16-bit fractional microseconds */
    uint16_t        microThreshold;     /* Threshold for microsecond updates */

    /* System startup time */
    uint64_t        systemStartTime;    /* System startup time (platform time) */
    uint32_t        bootTime;           /* Boot time in Mac OS format */

    /* Time base calibration */
    uint32_t        calibrationFactor;  /* Calibration factor for timing accuracy */
    uint32_t        timerOverhead;      /* Measured timer overhead */
    bool            isCalibrated;       /* True if time base has been calibrated */

    /* Time zone and DST information */
    int32_t         timeZoneOffset;     /* Time zone offset in seconds */
    bool            isDSTActive;        /* True if daylight saving time is active */
    uint32_t        dstStartTime;       /* DST start time (seconds since epoch) */
    uint32_t        dstEndTime;         /* DST end time (seconds since epoch) */
} SystemTimeBase;

/* ===== Time Conversion Structures ===== */

/**
 * Date and Time Record
 *
 * Represents a date and time in human-readable format,
 * compatible with Mac OS DateTimeRec.
 */
typedef struct DateTimeRec {
    uint16_t        year;               /* Year (e.g., 1993) */
    uint16_t        month;              /* Month (1-12) */
    uint16_t        day;                /* Day of month (1-31) */
    uint16_t        hour;               /* Hour (0-23) */
    uint16_t        minute;             /* Minute (0-59) */
    uint16_t        second;             /* Second (0-59) */
    uint16_t        dayOfWeek;          /* Day of week (1=Sunday, 7=Saturday) */
} DateTimeRec;

/**
 * Time Location Record
 *
 * Contains time zone and location information for time calculations.
 */
typedef struct TimeLocationRec {
    int32_t         gmtDelta;           /* GMT offset in seconds */
    int32_t         dstDelta;           /* DST offset in seconds */
    uint32_t        dstStart;           /* DST start time */
    uint32_t        dstEnd;             /* DST end time */
    uint16_t        dstType;            /* DST calculation type */
} TimeLocationRec;

/* ===== Time Base Functions ===== */

/**
 * Initialize Time Base
 *
 * Sets up the system time base with current time and calibration.
 * Must be called during system initialization.
 *
 * @return OSErr - noErr on success, error code on failure
 */
OSErr InitTimeBase(void);

/**
 * Shutdown Time Base
 *
 * Shuts down the time base system and saves any persistent timing data.
 */
void ShutdownTimeBase(void);

/**
 * Update Time Base
 *
 * Updates the time base with current timer values. Called regularly
 * by the timer interrupt handler to maintain accurate time.
 *
 * @param timerValue - Current timer value
 * @return OSErr - noErr on success, error code on failure
 */
OSErr UpdateTimeBase(uint32_t timerValue);

/**
 * Calibrate Time Base
 *
 * Performs runtime calibration of the time base to ensure accuracy
 * across different processor speeds and system configurations.
 *
 * @return OSErr - noErr on success, error code on failure
 */
OSErr CalibrateTimeBase(void);

/* ===== System Time Functions ===== */

/**
 * Get Current Time
 *
 * Returns the current system time in seconds since the Mac OS epoch
 * (January 1, 1904, 12:00 AM GMT).
 *
 * @param seconds - Pointer to receive current time in seconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetCurrentTime(uint32_t *seconds);

/**
 * Set Current Time
 *
 * Sets the system time to the specified value in seconds since the
 * Mac OS epoch. Requires appropriate privileges.
 *
 * @param seconds - New time in seconds since Mac OS epoch
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SetCurrentTime(uint32_t seconds);

/**
 * Get Date and Time
 *
 * Returns the current date and time in human-readable format.
 *
 * @param dateTime - Pointer to DateTimeRec to fill
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetDateTime(DateTimeRec *dateTime);

/**
 * Set Date and Time
 *
 * Sets the system date and time from human-readable format.
 *
 * @param dateTime - Pointer to DateTimeRec with new date/time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SetDateTime(const DateTimeRec *dateTime);

/* ===== High-Resolution Time Functions ===== */

/**
 * Get System Uptime
 *
 * Returns the time since system startup in microseconds.
 *
 * @param uptime - Pointer to UnsignedWide to receive uptime
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetSystemUptime(UnsignedWide *uptime);

/**
 * Get Absolute Time
 *
 * Returns the current absolute time using the highest resolution
 * timer available on the system.
 *
 * @param absoluteTime - Pointer to UnsignedWide to receive absolute time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetAbsoluteTime(UnsignedWide *absoluteTime);

/**
 * Convert Time to Seconds
 *
 * Converts an absolute time value to seconds since Mac OS epoch.
 *
 * @param absoluteTime - Absolute time value
 * @param seconds - Pointer to receive seconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr AbsoluteTimeToSeconds(UnsignedWide absoluteTime, uint32_t *seconds);

/**
 * Convert Seconds to Time
 *
 * Converts seconds since Mac OS epoch to absolute time value.
 *
 * @param seconds - Seconds since Mac OS epoch
 * @param absoluteTime - Pointer to UnsignedWide to receive absolute time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SecondsToAbsoluteTime(uint32_t seconds, UnsignedWide *absoluteTime);

/* ===== Time Zone Functions ===== */

/**
 * Get Time Zone Information
 *
 * Returns information about the current time zone setting.
 *
 * @param timeLocation - Pointer to TimeLocationRec to fill
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetTimeZoneInfo(TimeLocationRec *timeLocation);

/**
 * Set Time Zone Information
 *
 * Sets the time zone information for the system.
 *
 * @param timeLocation - Pointer to TimeLocationRec with new settings
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SetTimeZoneInfo(const TimeLocationRec *timeLocation);

/**
 * Convert to Local Time
 *
 * Converts UTC time to local time using current time zone settings.
 *
 * @param utcTime - UTC time in seconds since Mac OS epoch
 * @param localTime - Pointer to receive local time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr ConvertToLocalTime(uint32_t utcTime, uint32_t *localTime);

/**
 * Convert to UTC Time
 *
 * Converts local time to UTC time using current time zone settings.
 *
 * @param localTime - Local time in seconds since Mac OS epoch
 * @param utcTime - Pointer to receive UTC time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr ConvertToUTCTime(uint32_t localTime, uint32_t *utcTime);

/* ===== Conversion Utilities ===== */

/**
 * Seconds to DateTimeRec
 *
 * Converts seconds since Mac OS epoch to date/time structure.
 *
 * @param seconds - Seconds since Mac OS epoch
 * @param dateTime - Pointer to DateTimeRec to fill
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SecondsToDate(uint32_t seconds, DateTimeRec *dateTime);

/**
 * DateTimeRec to Seconds
 *
 * Converts date/time structure to seconds since Mac OS epoch.
 *
 * @param dateTime - Pointer to DateTimeRec to convert
 * @param seconds - Pointer to receive seconds
 * @return OSErr - noErr on success, error code on failure
 */
OSErr DateToSeconds(const DateTimeRec *dateTime, uint32_t *seconds);

/**
 * Validate Date and Time
 *
 * Validates that a DateTimeRec contains valid date/time values.
 *
 * @param dateTime - Pointer to DateTimeRec to validate
 * @return bool - true if valid, false if invalid
 */
bool ValidateDateTime(const DateTimeRec *dateTime);

/**
 * Calculate Day of Week
 *
 * Calculates the day of week for a given date.
 *
 * @param year - Year (e.g., 1993)
 * @param month - Month (1-12)
 * @param day - Day of month (1-31)
 * @return uint16_t - Day of week (1=Sunday, 7=Saturday)
 */
uint16_t CalculateDayOfWeek(uint16_t year, uint16_t month, uint16_t day);

/**
 * Is Leap Year
 *
 * Determines if a given year is a leap year.
 *
 * @param year - Year to check
 * @return bool - true if leap year, false otherwise
 */
bool IsLeapYear(uint16_t year);

/* ===== Time Base Status Functions ===== */

/**
 * Get Time Base Status
 *
 * Returns information about the current state of the time base.
 *
 * @param timeBase - Pointer to SystemTimeBase to fill
 * @return OSErr - noErr on success, error code on failure
 */
OSErr GetTimeBaseStatus(SystemTimeBase *timeBase);

/**
 * Is Time Base Valid
 *
 * Checks if the time base has been properly initialized and calibrated.
 *
 * @return bool - true if time base is valid and ready
 */
bool IsTimeBaseValid(void);

/**
 * Get Time Base Accuracy
 *
 * Returns the estimated accuracy of the time base in parts per million.
 *
 * @return uint32_t - Accuracy in PPM (lower is better)
 */
uint32_t GetTimeBaseAccuracy(void);

/* ===== Advanced Time Functions ===== */

/**
 * Set Time Base Correction
 *
 * Applies a correction factor to the time base to improve accuracy.
 * Used for synchronization with external time sources.
 *
 * @param correction - Correction factor (signed PPM)
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SetTimeBaseCorrection(int32_t correction);

/**
 * Synchronize Time Base
 *
 * Synchronizes the time base with an external time source.
 *
 * @param referenceTime - Reference time from external source
 * @param localTime - Corresponding local time
 * @return OSErr - noErr on success, error code on failure
 */
OSErr SynchronizeTimeBase(uint64_t referenceTime, uint64_t localTime);

/* ===== Constants ===== */

/* Date/time limits */
#define MIN_YEAR                    1904        /* Minimum valid year */
#define MAX_YEAR                    2040        /* Maximum valid year */
#define SECONDS_PER_DAY             86400       /* Seconds in a day */
#define SECONDS_PER_HOUR            3600        /* Seconds in an hour */
#define SECONDS_PER_MINUTE          60          /* Seconds in a minute */

/* Time base accuracy */
#define TIMEBASE_ACCURACY_GOOD      100         /* Good accuracy (100 PPM) */
#define TIMEBASE_ACCURACY_EXCELLENT 10          /* Excellent accuracy (10 PPM) */

#ifdef __cplusplus
}
#endif

#endif /* TIMEBASE_H */
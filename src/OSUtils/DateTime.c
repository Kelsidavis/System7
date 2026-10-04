/*
 * DateTime.c - Date and Time Utilities
 *
 * Implements Mac OS date and time functions for getting and setting the
 * system time. These are standard Toolbox utilities used throughout
 * the system and applications.
 *
 * Based on Inside Macintosh: Operating System Utilities
 */

#include "OSUtils/OSUtils.h"
#include "DateTime.h"
#include "SystemTypes.h"
#include "DeskManager/DeskManager.h"
#include "System71StdLib.h"
#include "TimeManager/TimeBase.h"
#include <time.h>
#if defined(__i386__) || defined(__x86_64__)
#include "Platform/x86/rtc.h"
#endif

/* Debug logging */
#define DATETIME_DEBUG 0

#if DATETIME_DEBUG
#define DT_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[DateTime] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define DT_LOG(...)
#endif

/* Global storage for current date/time (if we need to track set time) */
static UInt32 gSystemDateTime = 0;
static Boolean gSystemDateTimeOverride = false;

UInt32 DateTime_Current(void)
{
#if defined(__i386__) || defined(__x86_64__)
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        int y = (int)dt.year;
        int m = (int)dt.month;
        int d = (int)dt.day;
        int hour = (int)dt.hour;
        int minute = (int)dt.minute;
        int second = (int)dt.second;

        int adj_y = y - (m <= 2);
        int era = (adj_y >= 0 ? adj_y : adj_y - 399) / 400;
        unsigned yoe = (unsigned)(adj_y - era * 400);
        unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        int64_t days = (int64_t)(era * 146097 + (int)doe) - 719468;

        int64_t unix_time = days * 86400 + hour * 3600 + minute * 60 + second;
        if (unix_time < 0) {
            unix_time = 0;
        }
        return DateTime_FromUnix((time_t)unix_time);
    }
#endif
    /* No wall clock is available on this platform. Avoid time(), which uses
     * GetDateTime() and would recurse back into this function. */
    return 0;
}

UInt32 DateTime_FromUnix(time_t unixTime)
{
    return (UInt32)(unixTime + MAC_UNIX_EPOCH_OFFSET);
}

time_t DateTime_ToUnix(UInt32 macTime)
{
    if (macTime < MAC_UNIX_EPOCH_OFFSET) {
        return 0;
    }
    return (time_t)(macTime - MAC_UNIX_EPOCH_OFFSET);
}

/*
 * GetDateTime - Get current date and time
 *
 * Returns the current date and time in seconds since midnight,
 * January 1, 1904 (Mac epoch). This is the standard Mac OS time format.
 *
 * Parameters:
 *   secs - Pointer to receive the current date/time
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 4
 */
void GetDateTime(UInt32* secs) {
    if (!secs) {
        DT_LOG("GetDateTime: NULL pointer\n");
        return;
    }

    if (gSystemDateTimeOverride) {
        /* Return overridden time if SetDateTime was called */
        *secs = gSystemDateTime;
        DT_LOG("GetDateTime: Returning override time %lu\n",
               (unsigned long)*secs);
    } else {
        /* Get current time from system */
        *secs = DateTime_Current();
        DT_LOG("GetDateTime: Returning current time %lu\n",
               (unsigned long)*secs);
    }
}

/*
 * SetDateTime - Set the system date and time
 *
 * Sets the system's date and time. This affects all subsequent calls to
 * GetDateTime and ReadDateTime until the system is restarted or
 * SetDateTime is called again.
 *
 * Parameters:
 *   secs - New date/time in seconds since midnight, January 1, 1904
 *
 * Note: In a full implementation, this would set the hardware clock.
 * Here we just override the returned value.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 4
 */
void SetDateTime(UInt32 secs) {
    gSystemDateTime = secs;
    gSystemDateTimeOverride = true;

    DT_LOG("SetDateTime: Set time to %lu\n", (unsigned long)secs);

    /* In a full implementation, we would set the hardware clock here.
     * For now, we just track the override time. */
}

/*
 * ReadDateTime - Read the system date and time
 *
 * Identical to GetDateTime - reads the current date and time.
 * This function exists for compatibility with older code.
 *
 * Parameters:
 *   secs - Pointer to receive the current date/time
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 4
 */
void ReadDateTime(UInt32* secs) {
    /* ReadDateTime is just an alias for GetDateTime */
    GetDateTime(secs);
}

/*
 * InitDateTime - Initialize date/time system
 *
 * Called during system initialization to set up the date/time system.
 */
void InitDateTime(void) {
    gSystemDateTime = 0;
    gSystemDateTimeOverride = false;

    /* Get initial time from system */
    UInt32 currentTime = DateTime_Current();

    (void)currentTime;  /* Used only in debug logging */
    DT_LOG("InitDateTime: System time initialized to %lu\n",
           (unsigned long)currentTime);

#if defined(__i386__) || defined(__x86_64__)
    static Boolean logged_rtc = false;
    if (!logged_rtc) {
        rtc_datetime_t dt;
        if (rtc_read_datetime(&dt)) {
            serial_printf("[RTC] %04u-%02u-%02u %02u:%02u:%02u\n",
                          dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        } else {
            serial_puts("[RTC] read failed\n");
        }
        logged_rtc = true;
    }
#endif
}

/*
 * DateTimeRec structure (matches Inside Macintosh)
 */
typedef struct {
    SInt16 year;
    SInt16 month;
    SInt16 day;
    SInt16 hour;
    SInt16 minute;
    SInt16 second;
    SInt16 dayOfWeek;
} DateTimeRec;

/* Forward declarations */
void Secs2Date(UInt32 secs, DateTimeRec *d);
void SecondsToDate(UInt32 secs, DateTimeRec *d);
void Date2Secs(const DateTimeRec *d, UInt32 *secs);
void DateToSeconds(const DateTimeRec *d, UInt32 *secs);

void Secs2Date(UInt32 secs, DateTimeRec *d) {
    if (!d) return;

    UInt32 totalDays = secs / 86400;
    UInt32 secsInDay = secs % 86400;

    d->hour = (SInt16)(secsInDay / 3600);
    d->minute = (SInt16)((secsInDay % 3600) / 60);
    d->second = (SInt16)(secsInDay % 60);

    /* Day of week: Jan 1, 1904 was a Friday (dayOfWeek=6) */
    d->dayOfWeek = (SInt16)((totalDays + 6) % 7) + 1;

    /* Calculate year from days since 1904-01-01 */
    SInt16 year = 1904;
    for (;;) {
        SInt16 diy = 365;
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) diy = 366;
        if (totalDays < (UInt32)diy) break;
        totalDays -= (UInt32)diy;
        year++;
    }
    d->year = year;

    /* Calculate month and day */
    static const SInt16 daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    SInt16 month;
    for (month = 0; month < 12; month++) {
        SInt16 dim = daysInMonth[month];
        if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
            dim = 29;
        if (totalDays < (UInt32)dim) break;
        totalDays -= (UInt32)dim;
    }
    d->month = month + 1;
    d->day = (SInt16)totalDays + 1;
}

/* Alias for compatibility */
void SecondsToDate(UInt32 secs, DateTimeRec *d) {
    Secs2Date(secs, d);
}

/*
 * Date2Secs - Convert DateTimeRec to Mac epoch seconds
 * (Also known as DateToSeconds in some headers)
 */
void Date2Secs(const DateTimeRec *d, UInt32 *secs) {
    if (!d || !secs) return;

    static const SInt16 daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    UInt32 totalDays = 0;

    /* Count days from 1904 to target year */
    for (SInt16 y = 1904; y < d->year; y++) {
        totalDays += 365;
        if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) totalDays++;
    }

    /* Add days for months in target year */
    for (SInt16 m = 0; m < d->month - 1 && m < 12; m++) {
        totalDays += daysInMonth[m];
        if (m == 1 && ((d->year % 4 == 0 && d->year % 100 != 0) || d->year % 400 == 0))
            totalDays++;
    }

    /* Add days within month */
    totalDays += (d->day - 1);

    *secs = totalDays * 86400 + d->hour * 3600 + d->minute * 60 + d->second;
}

/* Alias for compatibility */
void DateToSeconds(const DateTimeRec *d, UInt32 *secs) {
    Date2Secs(d, secs);
}

/*
 * Delay - wait numTicks sixtieths of a second, giving desk accessories their
 * time meanwhile; *finalTicks gets TickCount at the end.
 */
void Delay(UInt32 numTicks, UInt32* finalTicks) {
    /* Wait for specified number of ticks with cooperative multitasking
     *
     * Timing:
     * - One tick = 1/60th second (16.67 ms) on most Macs
     * - Some systems use 1/50th second (PAL regions)
     * - Query actual tick rate with TickCount() frequency
     *
     * Cooperative Multitasking:
     * - Calls SystemTask() during wait to service Desk Accessories
     * - Allows DA windows to update, respond to events
     * - Critical for responsive UI during delays
     *
     * Common uses:
     * - Animation frame delays (e.g., 3 ticks = ~50ms)
     * - Double-click detection timeouts
     * - Debouncing user input
     * - Pacing Finder operations (icon dragging, etc.)
     *
     * Parameters:
     * - numTicks: Number of ticks to wait (60 ticks = 1 second)
     * - finalTicks: Optional output of actual final tick count
     *
     * Note: Not suitable for precise timing due to cooperative scheduling
     * overhead. For animations, use actual elapsed time calculations.
     */

    /* Until the ticks have passed, however long each SystemTask takes. The
     * difference is unsigned, so the counter wrapping does not end it early.
     * This used to give up after numTicks*1000 passes, and after 100 passes
     * without a tick - a pass is one SystemTask, far shorter than a 60th of a
     * second, so a short Delay could end almost at once. The only way out now
     * is the tick count really not moving: the timer is dead. */
    UInt32 startTicks = TickCount();
    UInt32 lastTicks = startTicks;
    UInt32 passesSinceTick = 0;
    while ((UInt32)(TickCount() - startTicks) < numTicks) {
        SystemTask();
        UInt32 now = TickCount();
        if (now != lastTicks) {
            lastTicks = now;
            passesSinceTick = 0;
        } else if (++passesSinceTick > 50000000u) {
            serial_printf("[Delay] TickCount has stopped; giving up the wait\n");
            break;
        }
    }

    if (finalTicks) {
        *finalTicks = TickCount();
    }
}

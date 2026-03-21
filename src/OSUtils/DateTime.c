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
#include "SystemTypes.h"
#include "System71StdLib.h"
#include <time.h>
#if defined(__i386__) || defined(__x86_64__)
#include "Platform/x86/rtc.h"
#endif

/* Debug logging */
#define DATETIME_DEBUG 0

#if DATETIME_DEBUG
extern void serial_puts(const char* str);
#define DT_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[DateTime] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define DT_LOG(...)
#endif

/* Mac epoch offset: difference between Mac epoch (1904) and Unix epoch (1970) */
#define MAC_EPOCH_OFFSET 2082844800UL

/* Forward declarations of helper functions from FileManager */
extern UInt32 DateTime_Current(void);
extern UInt32 DateTime_FromUnix(time_t unixTime);
extern time_t DateTime_ToUnix(UInt32 macTime);

/* Global storage for current date/time (if we need to track set time) */
static UInt32 gSystemDateTime = 0;
static Boolean gSystemDateTimeOverride = false;

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
 * DateTimeRec structure (matches Inside Macintosh):
 *   short year, month, day, hour, minute, second, dayOfWeek
 * dayOfWeek: 1=Sunday ... 7=Saturday
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

/*
 * Secs2Date - Convert Mac epoch seconds to DateTimeRec
 * (Also known as SecondsToDate in some headers)
 */
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

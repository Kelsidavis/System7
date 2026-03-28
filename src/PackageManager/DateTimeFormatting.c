/*
 * DateTimeFormatting.c - International Utilities Date/Time Formatting
 *
 * Implements IUDateString and IUTimeString functions for formatting
 * Mac OS date/time values into human-readable strings. These are part of
 * the International Utilities Package (Pack 6).
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include <string.h>

/* Date format enumeration */
typedef enum {
    shortDate = 0,
    longDate = 1,
    abbrevDate = 2
} DateForm;

/* Forward declarations */
void IUDateString(UInt32 dateTime, DateForm longFlag, char *result);
void IUTimeString(UInt32 dateTime, Boolean wantSeconds, char *result);

/* Debug logging */
#define DATETIME_FMT_DEBUG 0

#if DATETIME_FMT_DEBUG
extern void serial_puts(const char* str);
#define DTFMT_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[DateTimeFmt] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define DTFMT_LOG(...)
#endif

/* Mac epoch offset: difference between Mac epoch (1904) and Unix epoch (1970) */
#define MAC_EPOCH_OFFSET 2082844800UL

/* Month names for date formatting */
static const char* kMonthNamesShort[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* kMonthNamesLong[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char* kDayNames[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

/*
 * IUDateString - Format a date/time value as a date string
 *
 * Converts a Mac OS date/time value (seconds since January 1, 1904)
 * into a formatted date string. The format depends on the longFlag parameter.
 *
 * Parameters:
 *   dateTime - Mac epoch time (seconds since midnight, January 1, 1904)
 *   longFlag - Format to use:
 *              shortDate (0): "1/15/25" (MM/DD/YY)
 *              longDate (1): "Friday, January 15, 2025"
 *              abbrevDate (2): "Fri, Jan 15, 2025"
 *   result - Buffer to receive the formatted date string (at least 256 bytes)
 *            The string is returned as a Pascal string (length byte first)
 *
 * Note: This is a simplified implementation using US date formats.
 * A full implementation would use international resources for locale-specific
 * formatting.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
/*
 * Simple date/time breakdown - converts seconds since epoch to date components
 * This is a simplified implementation that doesn't account for all edge cases
 * but is sufficient for basic date/time formatting without using the C library.
 */
static void BreakdownDateTime(UInt32 macTime, int *year, int *month, int *day,
                              int *hour, int *minute, int *second, int *dayOfWeek) {
    UInt32 days;
    UInt32 secs;
    int y, m;
    int daysInMonth;
    int isLeap;

    /* Calculate total days since Mac epoch (Jan 1, 1904) */
    days = macTime / 86400;
    secs = macTime % 86400;

    /* Calculate time components */
    *hour = secs / 3600;
    *minute = (secs % 3600) / 60;
    *second = secs % 60;

    /* Calculate day of week (Jan 1, 1904 was a Friday = 5) */
    *dayOfWeek = (days + 5) % 7;

    /* Calculate year */
    y = 1904;
    while (1) {
        isLeap = ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
        int daysInYear = isLeap ? 366 : 365;
        if (days < daysInYear) break;
        days -= daysInYear;
        y++;
    }
    *year = y;

    /* Calculate month and day */
    m = 0;
    while (1) {
        /* Days in each month */
        static const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        daysInMonth = monthDays[m];
        if (m == 1 && isLeap) daysInMonth = 29;  /* February in leap year */

        if (days < daysInMonth) break;
        days -= daysInMonth;
        m++;
        if (m >= 12) {
            m = 0;
            y++;
        }
    }
    *month = m + 1;  /* 1-based month */
    *day = days + 1; /* 1-based day */
}

void IUDateString(UInt32 dateTime, DateForm longFlag, char *result) {
    char tempBuf[256];
    int len;
    int year, month, day, hour, minute, second, dayOfWeek;

    if (!result) {
        DTFMT_LOG("IUDateString: NULL result buffer\n");
        return;
    }

    /* Break down the date/time */
    BreakdownDateTime(dateTime, &year, &month, &day, &hour, &minute, &second, &dayOfWeek);

    /* Validate month and dayOfWeek to prevent array out-of-bounds */
    if (month < 1 || month > 12) month = 1;
    if (dayOfWeek < 0 || dayOfWeek > 6) dayOfWeek = 0;

    /* Format based on longFlag */
    switch (longFlag) {
        case shortDate:
            /* Short format: "1/15/25" */
            len = snprintf(tempBuf, sizeof(tempBuf), "%d/%d/%02d",
                          month, day, year % 100);
            break;

        case longDate:
            /* Long format: "Friday, January 15, 2025" */
            len = snprintf(tempBuf, sizeof(tempBuf), "%s, %s %d, %d",
                          kDayNames[dayOfWeek],
                          kMonthNamesLong[month - 1],
                          day, year);
            break;

        case abbrevDate:
        default:
            /* Abbreviated format: "Fri, Jan 15, 2025" */
            len = snprintf(tempBuf, sizeof(tempBuf), "%.3s, %.3s %d, %d",
                          kDayNames[dayOfWeek],
                          kMonthNamesShort[month - 1],
                          day, year);
            break;
    }

    /* Clamp length to Pascal string maximum (check for snprintf error too) */
    if (len < 0) {
        len = 0;
    } else if (len > 255) {
        len = 255;
    }

    /* Convert to Pascal string (length byte + data) */
    result[0] = (char)len;
    if (len > 0) {
        memcpy(&result[1], tempBuf, len);
    }

    DTFMT_LOG("IUDateString: dateTime=%lu, format=%d -> '%.*s'\n",
              (unsigned long)dateTime, (int)longFlag, len, tempBuf);
}

/*
 * IUTimeString - Format a date/time value as a time string
 *
 * Converts a Mac OS date/time value (seconds since January 1, 1904)
 * into a formatted time string in 12-hour format with AM/PM.
 *
 * Parameters:
 *   dateTime - Mac epoch time (seconds since midnight, January 1, 1904)
 *   wantSeconds - If true, include seconds in the output
 *                 If false, show only hours and minutes
 *   result - Buffer to receive the formatted time string (at least 256 bytes)
 *            The string is returned as a Pascal string (length byte first)
 *
 * Format:
 *   With seconds: "11:45:30 PM"
 *   Without seconds: "11:45 PM"
 *
 * Note: This is a simplified implementation using 12-hour US format.
 * A full implementation would use international resources for locale-specific
 * time formats (12-hour vs 24-hour, separators, etc.).
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
void IUTimeString(UInt32 dateTime, Boolean wantSeconds, char *result) {
    char tempBuf[256];
    int len;
    int year, month, day, hour, minute, second, dayOfWeek;
    int hour12;
    const char *ampm;

    if (!result) {
        DTFMT_LOG("IUTimeString: NULL result buffer\n");
        return;
    }

    /* Break down the date/time */
    BreakdownDateTime(dateTime, &year, &month, &day, &hour, &minute, &second, &dayOfWeek);

    /* Convert to 12-hour format */
    if (hour == 0) {
        hour12 = 12;  /* Midnight */
        ampm = "AM";
    } else if (hour < 12) {
        hour12 = hour;
        ampm = "AM";
    } else if (hour == 12) {
        hour12 = 12;  /* Noon */
        ampm = "PM";
    } else {
        hour12 = hour - 12;
        ampm = "PM";
    }

    /* Format time string */
    if (wantSeconds) {
        len = snprintf(tempBuf, sizeof(tempBuf), "%d:%02d:%02d %s",
                      hour12, minute, second, ampm);
    } else {
        len = snprintf(tempBuf, sizeof(tempBuf), "%d:%02d %s",
                      hour12, minute, ampm);
    }

    /* Clamp length to Pascal string maximum (check for snprintf error too) */
    if (len < 0) {
        len = 0;
    } else if (len > 255) {
        len = 255;
    }

    /* Convert to Pascal string (length byte + data) */
    result[0] = (char)len;
    if (len > 0) {
        memcpy(&result[1], tempBuf, len);
    }

    DTFMT_LOG("IUTimeString: dateTime=%lu, wantSeconds=%d -> '%.*s'\n",
              (unsigned long)dateTime, (int)wantSeconds, len, tempBuf);
}

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
}

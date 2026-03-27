/*
 * InternationalUtilities.c - International Utilities Package Functions
 *
 * Implements international resource management functions for the Mac OS
 * International Utilities Package (Pack 6). These functions provide access
 * to locale settings, measurement systems, and international resources.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>

/* Forward declarations */
Handle IUGetIntl(SInt16 theID);
void IUSetIntl(SInt16 refNum, SInt16 theID, const void* intlParam);
Boolean IUMetric(void);
void IUClearCache(void);

/* Debug logging */
#define INTL_UTIL_DEBUG 0

#if INTL_UTIL_DEBUG
extern void serial_puts(const char* str);
#define INTL_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[IntlUtil] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define INTL_LOG(...)
#endif

/* International resource IDs */
#define kIntl0ResID   0  /* Format settings (date, time, currency) */
#define kIntl1ResID   1  /* Sorting and collation tables */
#define kIntl2ResID   2  /* Calendar information */
#define kIntl3ResID   3  /* Additional locale data */

/* Global cache for international resources */
static Handle g_intl0Handle = NULL;
static Handle g_intl1Handle = NULL;
static Handle g_intl2Handle = NULL;
static Handle g_intl3Handle = NULL;

/* Default Intl0 structure (US English settings) */
typedef struct {
    char      decimalPt;          /* Decimal point character */
    SInt16    thousSep;           /* Thousands separator */
    SInt16    listSep;            /* List separator */
    SInt16    currSym1;           /* Currency symbol 1 */
    SInt16    currSym2;           /* Currency symbol 2 */
    SInt16    currSym3;           /* Currency symbol 3 */
    UInt8     currFmt;            /* Currency format */
    UInt8     dateOrder;          /* Date order */
    UInt8     shrtDateFmt;        /* Short date format */
    char      dateSep;            /* Date separator */
    UInt8     timeCycle;          /* Time cycle (12/24 hour) */
    UInt8     timeFmt;            /* Time format */
    char      mornStr[4];         /* Morning string */
    char      eveStr[4];          /* Evening string */
    char      timeSep;            /* Time separator */
    char      time1Suff;          /* Time suffix 1 */
    char      time2Suff;          /* Time suffix 2 */
    char      time3Suff;          /* Time suffix 3 */
    char      time4Suff;          /* Time suffix 4 */
    char      time5Suff;          /* Time suffix 5 */
    char      time6Suff;          /* Time suffix 6 */
    char      time7Suff;          /* Time suffix 7 */
    char      time8Suff;          /* Time suffix 8 */
    UInt8     metricSys;          /* Metric system flag */
    SInt16    intl0Vers;          /* International resource version */
} Intl0Rec;

/* Intl1 structure (Sorting and collation tables) */
typedef struct {
    UInt8     collTable[256];     /* Collation table for sorting */
    SInt16    intl1Vers;          /* International resource version */
} Intl1Rec;

/* Intl2 structure (Calendar information) */
typedef struct {
    SInt16    dateOrder;          /* Date order (mdy, dmy, ymd) */
    SInt16    longDateFormat;     /* Long date format */
    char      dayNames[7][16];    /* Day names (Sun, Mon, ...) */
    char      monthNames[12][16]; /* Month names (Jan, Feb, ...) */
    char      amString[8];        /* AM string */
    char      pmString[8];        /* PM string */
    SInt16    intl2Vers;          /* International resource version */
} Intl2Rec;

/* Intl3 structure (Additional locale data) */
typedef struct {
    UInt8     unTokenTable[256];  /* Untoken table for text processing */
    char      currencySymbol[8];  /* Currency symbol string */
    char      thousandsSep[4];    /* Thousands separator string */
    char      decimalSep[4];      /* Decimal separator string */
    char      listSep[4];         /* List separator string */
    SInt16    intl3Vers;          /* International resource version */
} Intl3Rec;

/*
 * CreateDefaultIntl0 - Create default US English Intl0 resource
 *
 * Creates a default international format resource with US English settings.
 * This is used when no custom international resources are available.
 */
static Handle CreateDefaultIntl0(void) {
    Handle h;
    Intl0Rec* intlPtr;

    /* Allocate handle for Intl0 structure */
    h = NewHandle(sizeof(Intl0Rec));
    if (h == NULL) {
        INTL_LOG("CreateDefaultIntl0: Failed to allocate handle\n");
        return NULL;
    }

    /* Lock handle and fill in default US English settings */
    HLock(h);
    intlPtr = (Intl0Rec*)*h;

    /* Number formatting */
    intlPtr->decimalPt = '.';        /* Period for decimal point */
    intlPtr->thousSep = ',';         /* Comma for thousands */
    intlPtr->listSep = ',';          /* Comma for list separator */

    /* Currency formatting (US Dollar) */
    intlPtr->currSym1 = '$';         /* Dollar sign */
    intlPtr->currSym2 = 0;
    intlPtr->currSym3 = 0;
    intlPtr->currFmt = 0;            /* $1,234.56 format */

    /* Date formatting */
    intlPtr->dateOrder = 0;          /* 0 = month/day/year */
    intlPtr->shrtDateFmt = 0;        /* Short date format */
    intlPtr->dateSep = '/';          /* Slash separator */

    /* Time formatting */
    intlPtr->timeCycle = 0;          /* 0 = 12-hour clock */
    intlPtr->timeFmt = 0;            /* Time format */
    intlPtr->timeSep = ':';          /* Colon separator */

    /* AM/PM strings */
    intlPtr->mornStr[0] = 'A';
    intlPtr->mornStr[1] = 'M';
    intlPtr->mornStr[2] = 0;
    intlPtr->mornStr[3] = 0;

    intlPtr->eveStr[0] = 'P';
    intlPtr->eveStr[1] = 'M';
    intlPtr->eveStr[2] = 0;
    intlPtr->eveStr[3] = 0;

    /* Time suffixes */
    intlPtr->time1Suff = 0;
    intlPtr->time2Suff = 0;
    intlPtr->time3Suff = 0;
    intlPtr->time4Suff = 0;
    intlPtr->time5Suff = 0;
    intlPtr->time6Suff = 0;
    intlPtr->time7Suff = 0;
    intlPtr->time8Suff = 0;

    /* Measurement system */
    intlPtr->metricSys = 0;          /* 0 = Imperial (US), 255 = Metric */

    /* Version */
    intlPtr->intl0Vers = 0;

    HUnlock(h);

    INTL_LOG("CreateDefaultIntl0: Created default US English Intl0\n");
    return h;
}

/*
 * CreateDefaultIntl1 - Create default US English Intl1 resource
 *
 * Creates a default international sorting/collation resource with US English
 * character ordering for string comparison and sorting operations.
 */
static Handle CreateDefaultIntl1(void) {
    Handle h;
    Intl1Rec* intlPtr;
    int i;

    /* Allocate handle for Intl1 structure */
    h = NewHandle(sizeof(Intl1Rec));
    if (h == NULL) {
        INTL_LOG("CreateDefaultIntl1: Failed to allocate handle\n");
        return NULL;
    }

    /* Lock handle and fill in default collation table */
    HLock(h);
    intlPtr = (Intl1Rec*)*h;

    /* Build case-insensitive ASCII collation table
     * This table maps characters to their sort order values.
     * A-Z and a-z map to same values for case-insensitive sorting.
     */

    /* Initialize all characters to their ASCII values */
    for (i = 0; i < 256; i++) {
        intlPtr->collTable[i] = (UInt8)i;
    }

    /* Map uppercase letters to lowercase equivalents for sorting
     * This makes 'A' sort the same as 'a', 'B' same as 'b', etc. */
    for (i = 'A'; i <= 'Z'; i++) {
        intlPtr->collTable[i] = (UInt8)(i + 32); /* Convert to lowercase */
    }

    /* Control characters sort before printable characters */
    for (i = 0; i < 32; i++) {
        intlPtr->collTable[i] = (UInt8)i;
    }

    /* Space and punctuation sort before letters */
    /* Numbers sort before letters */
    /* Letters sort in alphabetical order (lowercase) */
    /* Extended ASCII characters sort after standard ASCII */

    /* Version */
    intlPtr->intl1Vers = 0;

    HUnlock(h);

    INTL_LOG("CreateDefaultIntl1: Created default US English Intl1\n");
    return h;
}

/*
 * CreateDefaultIntl2 - Create default US English Intl2 resource
 *
 * Creates a default calendar information resource with US English day/month
 * names and date formatting conventions.
 */
static Handle CreateDefaultIntl2(void) {
    Handle h;
    Intl2Rec* intlPtr;

    /* Allocate handle for Intl2 structure */
    h = NewHandle(sizeof(Intl2Rec));
    if (h == NULL) {
        INTL_LOG("CreateDefaultIntl2: Failed to allocate handle\n");
        return NULL;
    }

    /* Lock handle and fill in default calendar data */
    HLock(h);
    intlPtr = (Intl2Rec*)*h;

    /* Date ordering: month/day/year (US format) */
    intlPtr->dateOrder = 0;  /* 0=MDY, 1=DMY, 2=YMD */

    /* Long date format flags */
    intlPtr->longDateFormat = 0;

    /* Day names (Sunday = 0) */
    strncpy(intlPtr->dayNames[0], "Sunday", 15);    intlPtr->dayNames[0][15] = '\0';
    strncpy(intlPtr->dayNames[1], "Monday", 15);    intlPtr->dayNames[1][15] = '\0';
    strncpy(intlPtr->dayNames[2], "Tuesday", 15);   intlPtr->dayNames[2][15] = '\0';
    strncpy(intlPtr->dayNames[3], "Wednesday", 15); intlPtr->dayNames[3][15] = '\0';
    strncpy(intlPtr->dayNames[4], "Thursday", 15);  intlPtr->dayNames[4][15] = '\0';
    strncpy(intlPtr->dayNames[5], "Friday", 15);    intlPtr->dayNames[5][15] = '\0';
    strncpy(intlPtr->dayNames[6], "Saturday", 15);  intlPtr->dayNames[6][15] = '\0';

    /* Month names (January = 0) */
    strncpy(intlPtr->monthNames[0], "January", 15);   intlPtr->monthNames[0][15] = '\0';
    strncpy(intlPtr->monthNames[1], "February", 15);  intlPtr->monthNames[1][15] = '\0';
    strncpy(intlPtr->monthNames[2], "March", 15);     intlPtr->monthNames[2][15] = '\0';
    strncpy(intlPtr->monthNames[3], "April", 15);     intlPtr->monthNames[3][15] = '\0';
    strncpy(intlPtr->monthNames[4], "May", 15);       intlPtr->monthNames[4][15] = '\0';
    strncpy(intlPtr->monthNames[5], "June", 15);      intlPtr->monthNames[5][15] = '\0';
    strncpy(intlPtr->monthNames[6], "July", 15);      intlPtr->monthNames[6][15] = '\0';
    strncpy(intlPtr->monthNames[7], "August", 15);    intlPtr->monthNames[7][15] = '\0';
    strncpy(intlPtr->monthNames[8], "September", 15); intlPtr->monthNames[8][15] = '\0';
    strncpy(intlPtr->monthNames[9], "October", 15);   intlPtr->monthNames[9][15] = '\0';
    strncpy(intlPtr->monthNames[10], "November", 15); intlPtr->monthNames[10][15] = '\0';
    strncpy(intlPtr->monthNames[11], "December", 15); intlPtr->monthNames[11][15] = '\0';

    /* AM/PM strings */
    strncpy(intlPtr->amString, "AM", 7);  intlPtr->amString[7] = '\0';
    strncpy(intlPtr->pmString, "PM", 7);  intlPtr->pmString[7] = '\0';

    /* Version */
    intlPtr->intl2Vers = 0;

    HUnlock(h);

    INTL_LOG("CreateDefaultIntl2: Created default US English Intl2\n");
    return h;
}

/*
 * CreateDefaultIntl3 - Create default US English Intl3 resource
 *
 * Creates a default additional locale data resource with US English settings
 * for currency symbols, separators, and text tokenization.
 */
static Handle CreateDefaultIntl3(void) {
    Handle h;
    Intl3Rec* intlPtr;
    int i;

    /* Allocate handle for Intl3 structure */
    h = NewHandle(sizeof(Intl3Rec));
    if (h == NULL) {
        INTL_LOG("CreateDefaultIntl3: Failed to allocate handle\n");
        return NULL;
    }

    /* Lock handle and fill in default locale data */
    HLock(h);
    intlPtr = (Intl3Rec*)*h;

    /* Initialize untoken table - maps lowercase to uppercase for text processing */
    for (i = 0; i < 256; i++) {
        intlPtr->unTokenTable[i] = (UInt8)i;
    }

    /* Map lowercase letters to uppercase equivalents */
    for (i = 'a'; i <= 'z'; i++) {
        intlPtr->unTokenTable[i] = (UInt8)(i - 32); /* Convert to uppercase */
    }

    /* Currency symbol (US Dollar) */
    strncpy(intlPtr->currencySymbol, "$", 8);

    /* Thousands separator (comma) */
    strncpy(intlPtr->thousandsSep, ",", 4);

    /* Decimal separator (period) */
    strncpy(intlPtr->decimalSep, ".", 4);

    /* List separator (comma + space) */
    strncpy(intlPtr->listSep, ", ", 4);

    /* Version */
    intlPtr->intl3Vers = 0;

    HUnlock(h);

    INTL_LOG("CreateDefaultIntl3: Created default US English Intl3\n");
    return h;
}

/*
 * IUGetIntl - Get international resource handle
 *
 * Returns a handle to the specified international resource. These resources
 * contain locale-specific formatting information for dates, times, numbers,
 * and currency. If the resource hasn't been loaded yet, it creates a default
 * resource.
 *
 * Parameters:
 *   theID - International resource ID (0-3)
 *           0 = Intl0 (format settings)
 *           1 = Intl1 (sorting tables)
 *           2 = Intl2 (calendar data)
 *           3 = Intl3 (additional locale data)
 *
 * Returns:
 *   Handle to international resource, or NULL if unavailable
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
Handle IUGetIntl(SInt16 theID) {
    Handle result = NULL;

    INTL_LOG("IUGetIntl: Getting international resource %d\n", (int)theID);

    switch (theID) {
        case kIntl0ResID:
            /* Format settings (date, time, currency, numbers) */
            if (g_intl0Handle == NULL) {
                g_intl0Handle = CreateDefaultIntl0();
            }
            result = g_intl0Handle;
            break;

        case kIntl1ResID:
            /* Sorting and collation tables */
            if (g_intl1Handle == NULL) {
                g_intl1Handle = CreateDefaultIntl1();
            }
            result = g_intl1Handle;
            break;

        case kIntl2ResID:
            /* Calendar information */
            if (g_intl2Handle == NULL) {
                g_intl2Handle = CreateDefaultIntl2();
            }
            result = g_intl2Handle;
            break;

        case kIntl3ResID:
            /* Additional locale data */
            if (g_intl3Handle == NULL) {
                g_intl3Handle = CreateDefaultIntl3();
            }
            result = g_intl3Handle;
            break;

        default:
            INTL_LOG("IUGetIntl: Invalid resource ID %d\n", (int)theID);
            break;
    }

    return result;
}

/*
 * IUSetIntl - Set international resource handle
 *
 * Sets or updates an international resource. This allows applications to
 * customize locale settings. Note: This implementation currently doesn't
 * support custom resource files (refNum parameter is ignored).
 *
 * Parameters:
 *   refNum - Resource file reference (currently ignored)
 *   theID - International resource ID (0-3)
 *   intlParam - Pointer to new international resource data
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
void IUSetIntl(SInt16 refNum, SInt16 theID, const void* intlParam) {
    Handle h;
    Size dataSize;

    (void)refNum;  /* Currently unused - would specify resource file */

    if (intlParam == NULL) {
        INTL_LOG("IUSetIntl: NULL intlParam\n");
        return;
    }

    INTL_LOG("IUSetIntl: Setting international resource %d\n", (int)theID);

    /* Determine size based on resource type */
    switch (theID) {
        case kIntl0ResID:
            dataSize = sizeof(Intl0Rec);

            /* Dispose old handle if it exists */
            if (g_intl0Handle != NULL) {
                DisposeHandle(g_intl0Handle);
            }

            /* Create new handle and copy data */
            h = NewHandle(dataSize);
            if (h != NULL) {
                HLock(h);
                memcpy(*h, intlParam, dataSize);
                HUnlock(h);
                g_intl0Handle = h;
                INTL_LOG("IUSetIntl: Successfully set Intl0\n");
            } else {
                INTL_LOG("IUSetIntl: Failed to allocate handle\n");
            }
            break;

        case kIntl1ResID:
            dataSize = sizeof(Intl1Rec);

            /* Dispose old handle if it exists */
            if (g_intl1Handle != NULL) {
                DisposeHandle(g_intl1Handle);
            }

            /* Create new handle and copy data */
            h = NewHandle(dataSize);
            if (h != NULL) {
                HLock(h);
                memcpy(*h, intlParam, dataSize);
                HUnlock(h);
                g_intl1Handle = h;
                INTL_LOG("IUSetIntl: Successfully set Intl1\n");
            } else {
                INTL_LOG("IUSetIntl: Failed to allocate handle\n");
            }
            break;

        case kIntl2ResID:
            dataSize = sizeof(Intl2Rec);

            /* Dispose old handle if it exists */
            if (g_intl2Handle != NULL) {
                DisposeHandle(g_intl2Handle);
            }

            /* Create new handle and copy data */
            h = NewHandle(dataSize);
            if (h != NULL) {
                HLock(h);
                memcpy(*h, intlParam, dataSize);
                HUnlock(h);
                g_intl2Handle = h;
                INTL_LOG("IUSetIntl: Successfully set Intl2\n");
            } else {
                INTL_LOG("IUSetIntl: Failed to allocate handle\n");
            }
            break;

        case kIntl3ResID:
            dataSize = sizeof(Intl3Rec);

            /* Dispose old handle if it exists */
            if (g_intl3Handle != NULL) {
                DisposeHandle(g_intl3Handle);
            }

            /* Create new handle and copy data */
            h = NewHandle(dataSize);
            if (h != NULL) {
                HLock(h);
                memcpy(*h, intlParam, dataSize);
                HUnlock(h);
                g_intl3Handle = h;
                INTL_LOG("IUSetIntl: Successfully set Intl3\n");
            } else {
                INTL_LOG("IUSetIntl: Failed to allocate handle\n");
            }
            break;

        default:
            INTL_LOG("IUSetIntl: Invalid resource ID %d\n", (int)theID);
            break;
    }
}

/*
 * IUMetric - Check if system is using metric measurements
 *
 * Returns whether the current international resource specifies metric
 * or imperial measurements. This affects how distances, weights, and
 * other measurements are displayed throughout the system.
 *
 * Returns:
 *   true if system is using metric measurements
 *   false if system is using imperial measurements
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
Boolean IUMetric(void) {
    Handle intl0;
    Intl0Rec* intlPtr;
    Boolean isMetric = false;

    /* Get Intl0 resource */
    intl0 = IUGetIntl(kIntl0ResID);
    if (intl0 != NULL) {
        HLock(intl0);
        intlPtr = (Intl0Rec*)*intl0;

        /* Check metricSys flag: 0 = Imperial, 255 = Metric */
        isMetric = (intlPtr->metricSys != 0);

        HUnlock(intl0);

        INTL_LOG("IUMetric: System is using %s measurements\n",
                 isMetric ? "metric" : "imperial");
    } else {
        INTL_LOG("IUMetric: Could not get Intl0 resource, defaulting to imperial\n");
    }

    return isMetric;
}

/*
 * IUClearCache - Clear cached international resources
 *
 * Clears the cached international resources, forcing them to be reloaded
 * the next time they are requested. This is typically called after changing
 * system locale settings.
 *
 * Based on Inside Macintosh: Text, Chapter 6
 */
void IUClearCache(void) {
    INTL_LOG("IUClearCache: Clearing international resource cache\n");

    /* Dispose cached resources */
    if (g_intl0Handle != NULL) {
        DisposeHandle(g_intl0Handle);
        g_intl0Handle = NULL;
    }

    if (g_intl1Handle != NULL) {
        DisposeHandle(g_intl1Handle);
        g_intl1Handle = NULL;
    }

    if (g_intl2Handle != NULL) {
        DisposeHandle(g_intl2Handle);
        g_intl2Handle = NULL;
    }

    if (g_intl3Handle != NULL) {
        DisposeHandle(g_intl3Handle);
        g_intl3Handle = NULL;
    }

    INTL_LOG("IUClearCache: Cache cleared\n");
}

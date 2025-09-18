/*
 * Date & Time Control Panel Header
 *
 * RE-AGENT-BANNER
 * Source: Date & Time.rsrc (SHA256: 9c3392e59c8b6bed0774aa9a654ceaf569ca7f10f5718499fdd5e4cd4b411fea)
 * Evidence: /home/k/Desktop/system7/evidence.curated.datetime.json
 * Mappings: /home/k/Desktop/system7/mappings.datetime.json
 * Layouts: /home/k/Desktop/system7/layouts.curated.datetime.json
 * Architecture: m68k Classic Mac OS
 * Type: Control Panel Device (cdev)
 * Creator: time
 */

#ifndef DATETIME_CDEV_H
#define DATETIME_CDEV_H

/* DateTime Control Panel - Standalone Implementation */
#include "../MacTypes.h"

/* Forward declarations and local type definitions */
typedef struct WindowRecord* WindowPtr;
typedef struct DialogRecord* DialogPtr;
typedef struct MenuInfo** MenuHandle;

/* Complete EventRecord definition */
typedef struct EventRecord {
    short what;
    long message;
    long when;
    Point where;
    short modifiers;
} EventRecord;

/* Event types */
enum {
    keyDown = 3,
    autoKey = 5
};

/* Item types */
enum {
    userItem = 0
};

typedef struct DateTimeRec {
    short year;
    short month;
    short day;
    short hour;
    short minute;
    short second;
    short dayOfWeek;
} DateTimeRec;

/* Key constants */
enum {
    charCodeMask = 0x000000FF,
    kUpArrow = 0x1E,
    kDownArrow = 0x1F,
    kLeftArrow = 0x1C,
    kRightArrow = 0x1D,
    kTabKey = 0x09
};

/* Control Panel Message Types */
enum {
    initDev = 0,
    hitDev = 1,
    closeDev = 2,
    nulDev = 3,
    updateDev = 4,
    activDev = 5,
    deactivDev = 6,
    keyEvtDev = 7,
    macDev = 8,
    undoDev = 9,
    cutDev = 10,
    copyDev = 11,
    pasteDev = 12,
    clearDev = 13
};

/* Date Format Types */
enum {
    shortDate = 0,
    longDate = 1,
    abbrevDate = 2
};

/* Time Format Types */
enum {
    time12Hour = 0,
    time24Hour = 1
};

/* Dialog Item IDs */
enum {
    kDateDisplayItem = 1,
    kTimeDisplayItem = 2,
    kDateFormatButton = 3,
    kTimeFormatButton = 4,
    kStatusDisplayItem = 5
};

/* Control Panel Device Record */
typedef struct CDevRecord {
    short message;              /* Control panel message type */
    Boolean setup;              /* Setup flag */
    char padding1;              /* Alignment padding */
    Rect *rect;                 /* Control panel rectangle */
    short procID;               /* Procedure ID */
    short refCon;               /* Reference constant */
    Handle storage;             /* Private storage handle */
} CDevRecord, *CDevPtr;

/* Date & Time Preferences */
typedef struct DateTimePrefs {
    OSType signature;           /* 'time' signature */
    short version;              /* Preferences version */
    short dateFormat;           /* Date display format */
    short timeFormat;           /* Time display format */
    Boolean suppressLeadingZero; /* Suppress leading zero flag */
    Boolean showSeconds;        /* Show seconds flag */
    unsigned long currentDate;  /* Current date (Mac format) */
    unsigned long currentTime;  /* Current time cache */
    short menuChoice;           /* Selected menu choice */
    short editField;            /* Currently active edit field */
    Boolean modified;           /* Settings modified flag */
    char padding[7];            /* Reserved space */
} DateTimePrefs, *DateTimePrefsPtr;

/* Date & Time Edit Record */
typedef struct DateTimeEditRec {
    short year;                 /* Year being edited */
    short month;                /* Month being edited */
    short day;                  /* Day being edited */
    short hour;                 /* Hour being edited */
    short minute;               /* Minute being edited */
    short second;               /* Second being edited */
    Boolean isPM;               /* PM indicator */
    Boolean valid;              /* Values valid flag */
    short activeField;          /* Active edit field */
    unsigned long originalDateTime; /* Original date/time */
    unsigned long tempDateTime;     /* Working date/time */
} DateTimeEditRec, *DateTimeEditPtr;

/* Function Prototypes - Remove pascal calling convention for C compatibility */
OSErr DateTimeMain(short message, Boolean setup, Rect *rect,
                  short procID, short refCon, Handle storage);
void InitDateTimeDialog(DialogPtr dialog);
Boolean HandleDateTimeItem(DialogPtr dialog, short item);
void DrawDateTimeControls(DialogPtr dialog);
Boolean FilterDateTimeEvent(DialogPtr dialog, EventRecord *event, short *item);

/* Utility Functions */
static void SetupDateTimePrefs(DateTimePrefsPtr prefs);
static void UpdateDateTimeDisplay(DialogPtr dialog, DateTimePrefsPtr prefs);
static void HandleDateFormatButton(DialogPtr dialog, DateTimePrefsPtr prefs);
static void HandleTimeFormatButton(DialogPtr dialog, DateTimePrefsPtr prefs);
static void ConvertToDateTimeRec(unsigned long dateTime, DateTimeEditPtr editRec);
static unsigned long ConvertFromDateTimeRec(DateTimeEditPtr editRec);
static void ValidateDateTimeValues(DateTimeEditPtr editRec);

#endif /* DATETIME_CDEV_H */
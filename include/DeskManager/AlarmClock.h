#ifndef ALARMCLOCK_H
#define ALARMCLOCK_H

/*
 * AlarmClock.h - Alarm Clock Desk Accessory
 *
 * Provides time display and alarm functionality. Shows current time and date,
 * allows setting alarms, and provides notification when alarms trigger.
 * Integrates with system time and notification services.
 *
 * Based on Apple's Alarm Clock DA from System 7.1
 */

#include "DeskAccessory.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Alarm Clock Constants */
#define ALARM_VERSION           0x0100      /* Alarm Clock version 1.0 */
#define MAX_ALARMS              10          /* Maximum number of alarms */
#define ALARM_NAME_LENGTH       32          /* Maximum alarm name length */
#define TIME_FORMAT_LENGTH      32          /* Time format string length */

/* Time Display Modes */
typedef enum {
    TIME_MODE_12_HOUR       = 0,    /* 12-hour format with AM/PM */
    TIME_MODE_24_HOUR       = 1,    /* 24-hour format */
    TIME_MODE_SECONDS       = 2,    /* Include seconds */
    TIME_MODE_MILLISECONDS  = 3     /* Include milliseconds */
} TimeDisplayMode;

/* Date Display Modes */
typedef enum {
    DATE_MODE_MDY           = 0,    /* MM/DD/YYYY */
    DATE_MODE_DMY           = 1,    /* DD/MM/YYYY */
    DATE_MODE_YMD           = 2,    /* YYYY/MM/DD */
    DATE_MODE_LONG          = 3,    /* Monday, January 1, 2023 */
    DATE_MODE_SHORT         = 4,    /* Mon, Jan 1, 2023 */
    DATE_MODE_ISO           = 5     /* ISO 8601 format */
} DateDisplayMode;

/* Alarm Types */
typedef enum {
    ALARM_TYPE_ONCE         = 0,    /* One-time alarm */
    ALARM_TYPE_DAILY        = 1,    /* Daily repeat */
    ALARM_TYPE_WEEKLY       = 2,    /* Weekly repeat */
    ALARM_TYPE_MONTHLY      = 3,    /* Monthly repeat */
    ALARM_TYPE_YEARLY       = 4,    /* Yearly repeat */
    ALARM_TYPE_CUSTOM       = 5     /* Custom repeat pattern */
} AlarmType;

/* Alarm States */
typedef enum {
    ALARM_STATE_DISABLED    = 0,    /* Alarm disabled */
    ALARM_STATE_ENABLED     = 1,    /* Alarm enabled */
    ALARM_STATE_TRIGGERED   = 2,    /* Alarm triggered */
    ALARM_STATE_SNOOZED     = 3,    /* Alarm snoozed */
    ALARM_STATE_EXPIRED     = 4     /* Alarm expired */
} AlarmState;

/* Notification Types */
typedef enum {
    NOTIFY_SOUND            = 0x01, /* Sound notification */
    NOTIFY_DIALOG           = 0x02, /* Dialog box */
    NOTIFY_MENU_FLASH       = 0x04, /* Flash menu bar */
    NOTIFY_DOCK_BOUNCE      = 0x08, /* Bounce dock icon */
    NOTIFY_SYSTEM_ALERT     = 0x10  /* System alert */
} NotificationType;

/* Time Structure */
typedef struct AlarmTime {
    int16_t     hour;               /* Hour (0-23) */
    int16_t     minute;             /* Minute (0-59) */
    int16_t     second;             /* Second (0-59) */
    int16_t     millisecond;        /* Millisecond (0-999) */
} AlarmTime;

/* Date Structure */
typedef struct AlarmDate {
    int16_t     year;               /* Year */
    int16_t     month;              /* Month (1-12) */
    int16_t     day;                /* Day (1-31) */
    int16_t     weekday;            /* Day of week (0=Sunday) */
} AlarmDate;

/* Date/Time Structure */
typedef struct DateTime {
    AlarmDate   date;               /* Date component */
    AlarmTime   time;               /* Time component */
    int32_t     timestamp;          /* Unix timestamp */
    int16_t     timezone;           /* Timezone offset (minutes) */
    bool        dstActive;          /* Daylight saving time active */
} DateTime;

/* Alarm Structure */
typedef struct Alarm {
    int16_t         id;             /* Alarm ID */
    char            name[ALARM_NAME_LENGTH + 1];  /* Alarm name */
    AlarmType       type;           /* Alarm type */
    AlarmState      state;          /* Current state */

    /* Trigger time */
    DateTime        triggerTime;    /* When alarm should trigger */
    DateTime        nextTrigger;    /* Next trigger time (for repeating) */

    /* Repeat settings */
    int16_t         repeatInterval; /* Repeat interval (days, weeks, etc.) */
    uint8_t         weekdayMask;    /* Days of week (bit mask) */
    bool            endDate;        /* Has end date */
    DateTime        endDateTime;    /* End date/time */

    /* Notification settings */
    NotificationType notifyType;    /* Notification types */
    char            soundName[64];  /* Sound file name */
    int16_t         volume;         /* Sound volume (0-100) */
    bool            canSnooze;      /* Allow snooze */
    int16_t         snoozeMinutes;  /* Snooze duration */
    int16_t         snoozeCount;    /* Current snooze count */
    int16_t         maxSnoozes;     /* Maximum snoozes */

    /* Message */
    char            message[256];   /* Alarm message */
    bool            showMessage;    /* Show message in notification */

    /* Status */
    DateTime        lastTriggered;  /* Last trigger time */
    int32_t         triggerCount;   /* Number of times triggered */
    bool            userCreated;    /* User-created alarm */

    /* Linked list */
    struct Alarm    *next;          /* Next alarm in list */
} Alarm;

/* Alarm Clock State */
typedef struct AlarmClock {
    /* Time display */
    DateTime        currentTime;    /* Current date/time */
    TimeDisplayMode timeMode;       /* Time display mode */
    DateDisplayMode dateMode;       /* Date display mode */
    bool            showDate;       /* Show date */
    bool            showSeconds;    /* Show seconds */
    bool            blinkColon;     /* Blink colon separator */

    /* Display formatting */
    char            timeFormat[TIME_FORMAT_LENGTH];   /* Time format string */
    char            dateFormat[TIME_FORMAT_LENGTH];   /* Date format string */
    char            timeString[64]; /* Formatted time string */
    char            dateString[64]; /* Formatted date string */

    /* Alarms */
    Alarm           *alarms;        /* List of alarms */
    int16_t         numAlarms;      /* Number of alarms */
    int16_t         nextAlarmID;    /* Next alarm ID */
    Alarm           *nextAlarm;     /* Next alarm to trigger */

    /* Settings */
    bool            autoUpdate;     /* Auto-update time display */
    int16_t         updateInterval; /* Update interval (milliseconds) */
    bool            use24Hour;      /* Use 24-hour format */
    bool            showAMPM;       /* Show AM/PM indicator */
    int16_t         timezone;       /* Timezone offset */
    bool            useDST;         /* Use daylight saving time */

    /* Window and display */
    Rect            windowBounds;   /* Window bounds */
    Rect            timeRect;       /* Time display rectangle */
    Rect            dateRect;       /* Date display rectangle */
    Rect            alarmRect;      /* Alarm indicator rectangle */
    bool            windowVisible;  /* Window visibility */

    /* Fonts and colors */
    void            *timeFont;      /* Time display font */
    void            *dateFont;      /* Date display font */
    int16_t         timeFontSize;   /* Time font size */
    int16_t         dateFontSize;   /* Date font size */
    uint32_t        textColor;      /* Text color */
    uint32_t        backgroundColor; /* Background color */

    /* Sound */
    bool            soundEnabled;   /* Sound enabled */
    char            defaultSound[64]; /* Default alarm sound */
    int16_t         defaultVolume;  /* Default volume */

    /* Notification state */
    bool            alarmPending;   /* Alarm notification pending */
    Alarm           *pendingAlarm;  /* Pending alarm */
    void            *notifyDialog;  /* Notification dialog */
} AlarmClock;

/* Alarm Clock Functions */

/**
 * Initialize Alarm Clock
 * @param clock Pointer to alarm clock structure
 * @return 0 on success, negative on error
 */
int AlarmClock_Initialize(AlarmClock *clock);

/**
 * Shutdown Alarm Clock
 * @param clock Pointer to alarm clock structure
 */
void AlarmClock_Shutdown(AlarmClock *clock);

/**
 * Update current time
 * @param clock Pointer to alarm clock structure
 */
void AlarmClock_UpdateTime(AlarmClock *clock);

/**
 * Check for triggered alarms
 * @param clock Pointer to alarm clock structure
 * @return Number of triggered alarms
 */
int AlarmClock_CheckAlarms(AlarmClock *clock);

/* Time Display Functions */

/**
 * Set time display mode
 * @param clock Pointer to alarm clock structure
 * @param mode Time display mode
 */
void AlarmClock_SetTimeMode(AlarmClock *clock, TimeDisplayMode mode);

/**
 * Set date display mode
 * @param clock Pointer to alarm clock structure
 * @param mode Date display mode
 */
void AlarmClock_SetDateMode(AlarmClock *clock, DateDisplayMode mode);

/**
 * Format time for display
 * @param clock Pointer to alarm clock structure
 * @param time Time to format
 * @param buffer Buffer for formatted string
 * @param bufferSize Size of buffer
 */
void AlarmClock_FormatTime(AlarmClock *clock, const AlarmTime *time,
                           char *buffer, int bufferSize);

/**
 * Format date for display
 * @param clock Pointer to alarm clock structure
 * @param date Date to format
 * @param buffer Buffer for formatted string
 * @param bufferSize Size of buffer
 */
void AlarmClock_FormatDate(AlarmClock *clock, const AlarmDate *date,
                           char *buffer, int bufferSize);

/**
 * Get current time string
 * @param clock Pointer to alarm clock structure
 * @return Formatted time string
 */
const char *AlarmClock_GetTimeString(AlarmClock *clock);

/**
 * Get current date string
 * @param clock Pointer to alarm clock structure
 * @return Formatted date string
 */
const char *AlarmClock_GetDateString(AlarmClock *clock);

/* Alarm Management Functions */

/**
 * Create new alarm
 * @param clock Pointer to alarm clock structure
 * @param name Alarm name
 * @param triggerTime When alarm should trigger
 * @param type Alarm type
 * @return Pointer to new alarm or NULL on error
 */
Alarm *AlarmClock_CreateAlarm(AlarmClock *clock, const char *name,
                              const DateTime *triggerTime, AlarmType type);

/**
 * Delete alarm
 * @param clock Pointer to alarm clock structure
 * @param alarmID Alarm ID
 * @return 0 on success, negative on error
 */
int AlarmClock_DeleteAlarm(AlarmClock *clock, int16_t alarmID);

/**
 * Enable alarm
 * @param clock Pointer to alarm clock structure
 * @param alarmID Alarm ID
 * @return 0 on success, negative on error
 */
int AlarmClock_EnableAlarm(AlarmClock *clock, int16_t alarmID);

/**
 * Disable alarm
 * @param clock Pointer to alarm clock structure
 * @param alarmID Alarm ID
 * @return 0 on success, negative on error
 */
int AlarmClock_DisableAlarm(AlarmClock *clock, int16_t alarmID);

/**
 * Get alarm by ID
 * @param clock Pointer to alarm clock structure
 * @param alarmID Alarm ID
 * @return Pointer to alarm or NULL if not found
 */
Alarm *AlarmClock_GetAlarm(AlarmClock *clock, int16_t alarmID);

/**
 * Get next alarm to trigger
 * @param clock Pointer to alarm clock structure
 * @return Pointer to next alarm or NULL if none
 */
Alarm *AlarmClock_GetNextAlarm(AlarmClock *clock);

/**
 * Snooze alarm
 * @param clock Pointer to alarm clock structure
 * @param alarmID Alarm ID
 * @return 0 on success, negative on error
 */
int AlarmClock_SnoozeAlarm(AlarmClock *clock, int16_t alarmID);

/**
 * Dismiss alarm
 * @param clock Pointer to alarm clock structure
 * @param alarmID Alarm ID
 * @return 0 on success, negative on error
 */
int AlarmClock_DismissAlarm(AlarmClock *clock, int16_t alarmID);

/* Date/Time Utility Functions */

/**
 * Get current system time
 * @param dateTime Pointer to date/time structure
 */
void AlarmClock_GetCurrentTime(DateTime *dateTime);

/**
 * Convert timestamp to date/time
 * @param timestamp Unix timestamp
 * @param dateTime Pointer to date/time structure
 */
void AlarmClock_TimestampToDateTime(int32_t timestamp, DateTime *dateTime);

/**
 * Convert date/time to timestamp
 * @param dateTime Pointer to date/time structure
 * @return Unix timestamp
 */
int32_t AlarmClock_DateTimeToTimestamp(const DateTime *dateTime);

/**
 * Add time interval to date/time
 * @param dateTime Pointer to date/time structure
 * @param days Days to add
 * @param hours Hours to add
 * @param minutes Minutes to add
 * @param seconds Seconds to add
 */
void AlarmClock_AddInterval(DateTime *dateTime, int16_t days, int16_t hours,
                            int16_t minutes, int16_t seconds);

/**
 * Calculate next trigger time for repeating alarm
 * @param alarm Pointer to alarm
 * @param currentTime Current time
 * @param nextTime Pointer to next trigger time (output)
 * @return 0 on success, negative on error
 */
int AlarmClock_CalculateNextTrigger(Alarm *alarm, const DateTime *currentTime,
                                    DateTime *nextTime);

/**
 * Check if date/time matches alarm pattern
 * @param alarm Pointer to alarm
 * @param dateTime Date/time to check
 * @return true if matches
 */
bool AlarmClock_MatchesPattern(Alarm *alarm, const DateTime *dateTime);

/* Notification Functions */

/**
 * Trigger alarm notification
 * @param clock Pointer to alarm clock structure
 * @param alarm Pointer to alarm
 * @return 0 on success, negative on error
 */
int AlarmClock_TriggerNotification(AlarmClock *clock, Alarm *alarm);

/**
 * Play alarm sound
 * @param soundName Sound file name
 * @param volume Volume (0-100)
 * @return 0 on success, negative on error
 */
int AlarmClock_PlaySound(const char *soundName, int16_t volume);

/**
 * Show alarm dialog
 * @param clock Pointer to alarm clock structure
 * @param alarm Pointer to alarm
 * @return User response (snooze, dismiss, etc.)
 */
int AlarmClock_ShowAlarmDialog(AlarmClock *clock, Alarm *alarm);

/**
 * Flash menu bar
 * @param duration Flash duration in milliseconds
 */
void AlarmClock_FlashMenuBar(int duration);

/* Display Functions */

/**
 * Draw alarm clock window
 * @param clock Pointer to alarm clock structure
 * @param updateRect Rectangle to update or NULL for all
 */
void AlarmClock_Draw(AlarmClock *clock, const Rect *updateRect);

/**
 * Draw time display
 * @param clock Pointer to alarm clock structure
 */
void AlarmClock_DrawTime(AlarmClock *clock);

/**
 * Draw date display
 * @param clock Pointer to alarm clock structure
 */
void AlarmClock_DrawDate(AlarmClock *clock);

/**
 * Draw alarm indicator
 * @param clock Pointer to alarm clock structure
 */
void AlarmClock_DrawAlarmIndicator(AlarmClock *clock);

/* Settings Functions */

/**
 * Load settings from preferences
 * @param clock Pointer to alarm clock structure
 * @return 0 on success, negative on error
 */
int AlarmClock_LoadSettings(AlarmClock *clock);

/**
 * Save settings to preferences
 * @param clock Pointer to alarm clock structure
 * @return 0 on success, negative on error
 */
int AlarmClock_SaveSettings(AlarmClock *clock);

/**
 * Reset to default settings
 * @param clock Pointer to alarm clock structure
 */
void AlarmClock_ResetSettings(AlarmClock *clock);

/* Desk Accessory Integration */

/**
 * Register Alarm Clock as a desk accessory
 * @return 0 on success, negative on error
 */
int AlarmClock_RegisterDA(void);

/**
 * Create Alarm Clock DA instance
 * @return Pointer to DA instance or NULL on error
 */
DeskAccessory *AlarmClock_CreateDA(void);

/* Alarm Clock Error Codes */
#define ALARM_ERR_NONE              0       /* No error */
#define ALARM_ERR_INVALID_TIME      -1      /* Invalid time */
#define ALARM_ERR_INVALID_DATE      -2      /* Invalid date */
#define ALARM_ERR_ALARM_NOT_FOUND   -3      /* Alarm not found */
#define ALARM_ERR_TOO_MANY_ALARMS   -4      /* Too many alarms */
#define ALARM_ERR_SOUND_ERROR       -5      /* Sound playback error */
#define ALARM_ERR_NOTIFICATION_ERROR -6     /* Notification error */
#define ALARM_ERR_TIME_FORMAT       -7      /* Time format error */

#endif /* ALARMCLOCK_H */
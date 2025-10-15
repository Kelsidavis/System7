// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
/*
 * AlarmClock.c - Alarm Clock Desk Accessory Implementation
 *
 * Provides time display and alarm functionality. Shows current time and date,
 * allows setting alarms, and provides notification when alarms trigger.
 *
 * Derived from ROM analysis (System 7)
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/AlarmClock.h"
#include "DeskManager/DeskManager.h"
#include "SoundManager/SoundEffects.h"

static int AlarmClock_StrEqualsIgnoreCase(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static SoundEffectId AlarmClock_EffectForName(const char* soundName)
{
    if (!soundName || *soundName == '\0') {
        return kSoundEffectBeep;
    }

    if (AlarmClock_StrEqualsIgnoreCase(soundName, "wild eep")) {
        return kSoundEffectWildEep;
    }
    if (AlarmClock_StrEqualsIgnoreCase(soundName, "droplet")) {
        return kSoundEffectDroplet;
    }
    if (AlarmClock_StrEqualsIgnoreCase(soundName, "quack")) {
        return kSoundEffectQuack;
    }
    if (AlarmClock_StrEqualsIgnoreCase(soundName, "indigo")) {
        return kSoundEffectIndigo;
    }
    if (AlarmClock_StrEqualsIgnoreCase(soundName, "sosumi")) {
        return kSoundEffectBeep;
    }

    return kSoundEffectBeep;
}


/*
 * Initialize Alarm Clock
 */
int AlarmClock_Initialize(AlarmClock *clock)
{
    if (!clock) {
        return ALARM_ERR_INVALID_TIME;
    }

    memset(clock, 0, sizeof(AlarmClock));

    /* Set default display modes */
    clock->timeMode = TIME_MODE_12_HOUR;
    clock->dateMode = DATE_MODE_MDY;
    clock->showDate = true;
    clock->showSeconds = true;
    clock->blinkColon = true;
    clock->use24Hour = false;
    clock->showAMPM = true;

    /* Set default update interval */
    clock->autoUpdate = true;
    clock->updateInterval = 1000;  /* 1 second */

    /* Set window bounds */
    (clock)->left = 100;
    (clock)->top = 100;
    (clock)->right = 300;
    (clock)->bottom = 200;

    /* Set display areas */
    (clock)->left = 20;
    (clock)->top = 40;
    (clock)->right = 180;
    (clock)->bottom = 70;

    (clock)->left = 20;
    (clock)->top = 80;
    (clock)->right = 180;
    (clock)->bottom = 100;

    (clock)->left = 20;
    (clock)->top = 110;
    (clock)->right = 180;
    (clock)->bottom = 130;

    /* Initialize time */
    AlarmClock_UpdateTime(clock);

    /* Set default sound settings */
    clock->soundEnabled = true;
    strcpy(clock->defaultSound, "Beep");
    clock->defaultVolume = 50;

    /* Set default format strings */
    strcpy(clock->timeFormat, "%I:%M:%S %p");
    strcpy(clock->dateFormat, "%m/%d/%Y");

    clock->nextAlarmID = 1;

    return ALARM_ERR_NONE;
}

/*
 * Shutdown Alarm Clock
 */
void AlarmClock_Shutdown(AlarmClock *clock)
{
    if (!clock) {
        return;
    }

    /* Free all alarms */
    Alarm *alarm = clock->alarms;
    while (alarm) {
        Alarm *next = alarm->next;
        free(alarm);
        alarm = next;
    }
    clock->alarms = NULL;
}

/*
 * Update current time
 */
void AlarmClock_UpdateTime(AlarmClock *clock)
{
    if (!clock) {
        return;
    }

    /* Get current system time */
    AlarmClock_GetCurrentTime(&clock->currentTime);

    /* Format time string */
    AlarmClock_FormatTime(clock, &(clock)->time,
                          clock->timeString, sizeof(clock->timeString));

    /* Format date string */
    AlarmClock_FormatDate(clock, &(clock)->date,
                          clock->dateString, sizeof(clock->dateString));
}

/*
 * Check for triggered alarms
 */
int AlarmClock_CheckAlarms(AlarmClock *clock)
{
    if (!clock) {
        return 0;
    }

    int triggeredCount = 0;
    Alarm *alarm = clock->alarms;

    while (alarm) {
        if (alarm->state == ALARM_STATE_ENABLED) {
            /* Check if alarm should trigger */
            if ((alarm)->timestamp <= (clock)->timestamp) {
                /* Trigger alarm */
                AlarmClock_TriggerNotification(clock, alarm);
                alarm->state = ALARM_STATE_TRIGGERED;
                alarm->lastTriggered = clock->currentTime;
                alarm->triggerCount++;
                triggeredCount++;

                /* Calculate next trigger time for repeating alarms */
                if (alarm->type != ALARM_TYPE_ONCE) {
                    AlarmClock_CalculateNextTrigger(alarm, &clock->currentTime,
                                                    &alarm->nextTrigger);
                    alarm->triggerTime = alarm->nextTrigger;
                    alarm->state = ALARM_STATE_ENABLED;
                }
            }
        }
        alarm = alarm->next;
    }

    return triggeredCount;
}

/*
 * Format time for display
 */
void AlarmClock_FormatTime(AlarmClock *clock, const AlarmTime *time,
                           char *buffer, int bufferSize)
{
    if (!clock || !time || !buffer || bufferSize <= 0) {
        return;
    }

    if (clock->use24Hour) {
        if (clock->showSeconds) {
            snprintf(buffer, bufferSize, "%02d:%02d:%02d",
                     time->hour, time->minute, time->second);
        } else {
            snprintf(buffer, bufferSize, "%02d:%02d",
                     time->hour, time->minute);
        }
    } else {
        int displayHour = time->hour;
        const char *ampm = "AM";

        if (displayHour == 0) {
            displayHour = 12;
        } else if (displayHour > 12) {
            displayHour -= 12;
            ampm = "PM";
        } else if (displayHour == 12) {
            ampm = "PM";
        }

        if (clock->showSeconds) {
            snprintf(buffer, bufferSize, "%d:%02d:%02d %s",
                     displayHour, time->minute, time->second, ampm);
        } else {
            snprintf(buffer, bufferSize, "%d:%02d %s",
                     displayHour, time->minute, ampm);
        }
    }
}

/*
 * Format date for display
 */
void AlarmClock_FormatDate(AlarmClock *clock, const AlarmDate *date,
                           char *buffer, int bufferSize)
{
    if (!clock || !date || !buffer || bufferSize <= 0) {
        return;
    }

    switch (clock->dateMode) {
        case DATE_MODE_MDY:
            snprintf(buffer, bufferSize, "%d/%d/%d",
                     date->month, date->day, date->year);
            break;
        case DATE_MODE_DMY:
            snprintf(buffer, bufferSize, "%d/%d/%d",
                     date->day, date->month, date->year);
            break;
        case DATE_MODE_YMD:
            snprintf(buffer, bufferSize, "%d/%d/%d",
                     date->year, date->month, date->day);
            break;
        case DATE_MODE_LONG:
            {
                const char *months[] = {
                    "", "January", "February", "March", "April", "May", "June",
                    "July", "August", "September", "October", "November", "December"
                };
                const char *weekdays[] = {
                    "Sunday", "Monday", "Tuesday", "Wednesday",
                    "Thursday", "Friday", "Saturday"
                };
                snprintf(buffer, bufferSize, "%s, %s %d, %d",
                         weekdays[date->weekday], months[date->month],
                         date->day, date->year);
            }
            break;
        default:
            snprintf(buffer, bufferSize, "%d/%d/%d",
                     date->month, date->day, date->year);
            break;
    }
}

/*
 * Get current time string
 */
const char *AlarmClock_GetTimeString(AlarmClock *clock)
{
    return clock ? clock->timeString : "00:00:00";
}

/*
 * Get current date string
 */
const char *AlarmClock_GetDateString(AlarmClock *clock)
{
    return clock ? clock->dateString : "1/1/1970";
}

/*
 * Create new alarm
 */
Alarm *AlarmClock_CreateAlarm(AlarmClock *clock, const char *name,
                              const DateTime *triggerTime, AlarmType type)
{
    if (!clock || !name || !triggerTime) {
        return NULL;
    }

    Alarm *alarm = malloc(sizeof(Alarm));
    if (!alarm) {
        return NULL;
    }

    memset(alarm, 0, sizeof(Alarm));

    /* Set alarm properties */
    alarm->id = clock->nextAlarmID++;
    strncpy(alarm->name, name, ALARM_NAME_LENGTH);
    alarm->name[ALARM_NAME_LENGTH] = '\0';
    alarm->type = type;
    alarm->state = ALARM_STATE_ENABLED;
    alarm->triggerTime = *triggerTime;
    alarm->notifyType = NOTIFY_SOUND | NOTIFY_DIALOG;
    strcpy(alarm->soundName, clock->defaultSound);
    alarm->volume = clock->defaultVolume;
    alarm->canSnooze = true;
    alarm->snoozeMinutes = 5;
    alarm->maxSnoozes = 3;
    alarm->userCreated = true;

    /* Add to alarm list */
    alarm->next = clock->alarms;
    clock->alarms = alarm;
    clock->numAlarms++;

    return alarm;
}

/*
 * Get current system time
 */
void AlarmClock_GetCurrentTime(DateTime *dateTime)
{
    if (!dateTime) {
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    (dateTime)->year = tm_info->tm_year + 1900;
    (dateTime)->month = tm_info->tm_mon + 1;
    (dateTime)->day = tm_info->tm_mday;
    (dateTime)->weekday = tm_info->tm_wday;

    (dateTime)->hour = tm_info->tm_hour;
    (dateTime)->minute = tm_info->tm_min;
    (dateTime)->second = tm_info->tm_sec;
    (dateTime)->millisecond = 0;

    dateTime->timestamp = (SInt32)now;
    dateTime->timezone = 0;  /* UTC offset in minutes */
    dateTime->dstActive = tm_info->tm_isdst > 0;
}

/*
 * Trigger alarm notification
 */
int AlarmClock_TriggerNotification(AlarmClock *clock, Alarm *alarm)
{
    if (!clock || !alarm) {
        return ALARM_ERR_NOTIFICATION_ERROR;
    }

    /* Play sound if enabled */
    if ((alarm->notifyType & NOTIFY_SOUND) && clock->soundEnabled) {
        AlarmClock_PlaySound(alarm->soundName, alarm->volume);
    }

    /* Show dialog if enabled */
    if (alarm->notifyType & NOTIFY_DIALOG) {
        AlarmClock_ShowAlarmDialog(clock, alarm);
    }

    /* Flash menu bar if enabled */
    if (alarm->notifyType & NOTIFY_MENU_FLASH) {
        AlarmClock_FlashMenuBar(1000);
    }

    return ALARM_ERR_NONE;
}

/*
 * Play alarm sound
 */
int AlarmClock_PlaySound(const char *soundName, SInt16 volume)
{
    (void)volume;
    (void)SoundEffects_Play(AlarmClock_EffectForName(soundName));
    return ALARM_ERR_NONE;
}

/*
 * Show alarm dialog
 */
int AlarmClock_ShowAlarmDialog(AlarmClock *clock, Alarm *alarm)
{
    /* In a real implementation, this would show an alarm dialog */
    /* For now, just a placeholder */
    (void)clock;
    (void)alarm;
    return 0;  /* User dismissed */
}

/*
 * Flash menu bar
 */
void AlarmClock_FlashMenuBar(int duration)
{
    /* In a real implementation, this would flash the menu bar */
    (void)duration;
}

/*
 * Draw alarm clock window
 */
void AlarmClock_Draw(AlarmClock *clock, const Rect *updateRect)
{
    if (!clock) {
        return;
    }

    /* In a real implementation, this would draw the clock display */
    /* For now, just update the time strings */
    AlarmClock_UpdateTime(clock);
}

/*
 * Calculate next trigger time for repeating alarm
 */
int AlarmClock_CalculateNextTrigger(Alarm *alarm, const DateTime *currentTime,
                                    DateTime *nextTime)
{
    if (!alarm || !currentTime || !nextTime) {
        return ALARM_ERR_INVALID_TIME;
    }

    *nextTime = alarm->triggerTime;

    switch (alarm->type) {
        case ALARM_TYPE_DAILY:
            nextTime->timestamp += 24 * 60 * 60;  /* Add 24 hours */
            break;
        case ALARM_TYPE_WEEKLY:
            nextTime->timestamp += 7 * 24 * 60 * 60;  /* Add 7 days */
            break;
        case ALARM_TYPE_MONTHLY:
            /* Add one month (simplified) */
            (nextTime)->month++;
            if ((nextTime)->month > 12) {
                (nextTime)->month = 1;
                (nextTime)->year++;
            }
            nextTime->timestamp = AlarmClock_DateTimeToTimestamp(nextTime);
            break;
        case ALARM_TYPE_YEARLY:
            /* Add one year */
            (nextTime)->year++;
            nextTime->timestamp = AlarmClock_DateTimeToTimestamp(nextTime);
            break;
        default:
            return ALARM_ERR_INVALID_TIME;
    }

    return ALARM_ERR_NONE;
}

/*
 * Convert date/time to timestamp
 */
SInt32 AlarmClock_DateTimeToTimestamp(const DateTime *dateTime)
{
    if (!dateTime) {
        return 0;
    }

    struct tm tm_info = {0};
    tm_info.tm_year = (dateTime)->year - 1900;
    tm_info.tm_mon = (dateTime)->month - 1;
    tm_info.tm_mday = (dateTime)->day;
    tm_info.tm_hour = (dateTime)->hour;
    tm_info.tm_min = (dateTime)->minute;
    tm_info.tm_sec = (dateTime)->second;

    return (SInt32)mktime(&tm_info);
}

/*
 * Register Alarm Clock as a desk accessory
 */
int AlarmClock_RegisterDA(void)
{
    /* This is handled in BuiltinDAs.c */
    return ALARM_ERR_NONE;
}

/*
 * Create Alarm Clock DA instance
 */
DeskAccessory *AlarmClock_CreateDA(void)
{
    /* This is handled in BuiltinDAs.c */
    return NULL;
}

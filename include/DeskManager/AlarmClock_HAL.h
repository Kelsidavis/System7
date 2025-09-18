/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * AlarmClock_HAL.h - Hardware Abstraction Layer for Alarm Clock
 */

#ifndef ALARMCLOCK_HAL_H
#define ALARMCLOCK_HAL_H

#include <Types.h>
#include <Windows.h>
#include <Events.h>
#include "AlarmClock.h"

/* HAL Initialization */
void AlarmClock_HAL_Init(void);
void AlarmClock_HAL_Cleanup(void);

/* Window Management */
WindowPtr AlarmClock_HAL_CreateWindow(AlarmClock *clock);
void AlarmClock_HAL_DisposeWindow(WindowPtr window);
void AlarmClock_HAL_HandleEvent(WindowPtr window, EventRecord *event);

/* Time Functions */
void AlarmClock_HAL_GetSystemTime(DateTime *dt);

/* Alarm Functions */
void AlarmClock_HAL_TriggerAlarm(AlarmClock *clock, Alarm *alarm);
void AlarmClock_HAL_ShowNotification(const char *message);
Boolean AlarmClock_HAL_SetAlarmDialog(Alarm *alarm);

/* Check if alarm should trigger */
static inline Boolean AlarmClock_ShouldTrigger(Alarm *alarm, DateTime *current) {
    if (!alarm || !current) return false;

    /* Check if time matches (ignore seconds) */
    if (alarm->triggerTime.hour == current->hour &&
        alarm->triggerTime.minute == current->minute) {

        /* Check date based on alarm type */
        switch (alarm->type) {
            case ALARM_TYPE_ONCE:
                return (alarm->triggerTime.year == current->year &&
                       alarm->triggerTime.month == current->month &&
                       alarm->triggerTime.day == current->day);

            case ALARM_TYPE_DAILY:
                return true;

            case ALARM_TYPE_WEEKLY:
                return (alarm->triggerTime.weekday == current->weekday);

            case ALARM_TYPE_WEEKDAYS:
                return (current->weekday >= 1 && current->weekday <= 5);

            case ALARM_TYPE_WEEKENDS:
                return (current->weekday == 0 || current->weekday == 6);

            default:
                return false;
        }
    }

    return false;
}

#endif /* ALARMCLOCK_HAL_H */
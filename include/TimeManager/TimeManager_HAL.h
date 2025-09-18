/*
 * TimeManager_HAL.h - Hardware Abstraction Layer for Time Manager
 *
 * Provides platform-specific timing implementations for high-resolution
 * timers, microsecond timing, and interrupt handling.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef TIMEMANAGER_HAL_H
#define TIMEMANAGER_HAL_H

#include "TimeManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Timer callback function type */
typedef void (*TimerCallbackFunc)(void);

/* HAL Initialization */
OSErr TimeManager_HAL_Init(void);
void TimeManager_HAL_Cleanup(void);

/* High-resolution timing */
void TimeManager_HAL_GetMicroseconds(UnsignedWide* microseconds);
void TimeManager_HAL_GetNanoseconds(UnsignedWide* nanoseconds);
UInt32 TimeManager_HAL_GetMilliseconds(void);
UInt32 TimeManager_HAL_GetTicks(void);

/* Platform timer management */
void* TimeManager_HAL_CreateTimer(TimerCallbackFunc callback,
                                  UInt32 intervalMicros);
void TimeManager_HAL_DestroyTimer(void* timer);
OSErr TimeManager_HAL_StartTimer(void* timer);
OSErr TimeManager_HAL_StopTimer(void* timer);
OSErr TimeManager_HAL_SetTimerInterval(void* timer, UInt32 intervalMicros);

/* Interrupt control */
OSErr TimeManager_HAL_DisableInterrupts(void);
OSErr TimeManager_HAL_EnableInterrupts(void);
OSErr TimeManager_HAL_SetInterruptLevel(short level);
short TimeManager_HAL_GetInterruptLevel(void);

/* Thread/process control */
void TimeManager_HAL_Yield(void);
void TimeManager_HAL_Sleep(UInt32 microseconds);
OSErr TimeManager_HAL_LockScheduler(void);
OSErr TimeManager_HAL_UnlockScheduler(void);

/* Time base calibration */
OSErr TimeManager_HAL_CalibrateTimeBase(void);
UInt32 TimeManager_HAL_GetTimeBaseFrequency(void);
OSErr TimeManager_HAL_SetTimeBaseFrequency(UInt32 frequency);

/* Platform-specific features */
Boolean TimeManager_HAL_HasHighResTimer(void);
Boolean TimeManager_HAL_HasPeriodicTimer(void);
Boolean TimeManager_HAL_HasDriftFreeTimer(void);
UInt32 TimeManager_HAL_GetTimerResolution(void);

/* Debugging and diagnostics */
void TimeManager_HAL_GetTimerStats(UInt32* fired, UInt32* missed,
                                   UInt32* drift);
void TimeManager_HAL_ResetTimerStats(void);

#ifdef __cplusplus
}
#endif

#endif /* TIMEMANAGER_HAL_H */
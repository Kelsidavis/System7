/*
 * TimeManager.c - Public Time Manager API
 * Based on Inside Macintosh: Operating System Utilities (Time Manager)
 */

#include "SystemTypes.h"
#include "TimeManager/TimeManager.h"
#include "TimeManager/TimeManagerPriv.h"
#include "TimeManager/TimeBase.h"

OSErr InitTimeManager(void) {
    OSErr err = InitTimeBase();
    if (err != noErr) return err;

    InitDeferredQueue();

    err = Core_Initialize();
    if (err != noErr) {
        ShutdownTimeBase();
        return err;
    }

    return noErr;
}

void ShutdownTimeManager(void) {
    Core_Shutdown();
    ShutdownDeferredQueue();
    ShutdownTimeBase();
}

OSErr InsTime(TMTask* task) {
    if (!task) return tmParamErr;
    return Core_InsertTask(task);
}

OSErr RmvTime(TMTask* task) {
    if (!task) return tmParamErr;
    return Core_RemoveTask(task);
}

OSErr PrimeTime(TMTask* task, UInt32 microseconds) {
    if (!task) return tmParamErr;
    task->tmCount = microseconds;
    return Core_PrimeTask(task, microseconds);
}

OSErr CancelTime(TMTask* task) {
    if (!task) return tmParamErr;
    return Core_CancelTask(task);
}

UInt32 TimeManager_GetResolutionNS(void) {
    return (UInt32)GetTimerResolution();
}

UInt32 TimeManager_GetOverheadUS(void) {
    return GetTimerOverhead();
}

UInt32 TimeManager_GetActiveCount(void) {
    return Core_GetActiveCount();
}

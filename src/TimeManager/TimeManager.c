/*
 * TimeManager.c - System 7.1 Time Manager Implementation
 *
 * Implements the Time Manager which provides high-resolution timing services,
 * timer task scheduling, and VBL synchronization for Mac OS applications.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "TimeManager/TimeManager.h"
#include "TimeManager/TimeManager_HAL.h"
#include "Memory.h"
#include <string.h>
#include <stdlib.h>

/* Time Manager State */
static struct {
    Boolean         initialized;
    QHdr            tmQueue;        /* Time Manager task queue */
    QHdr            vblQueue;       /* VBL task queue */
    UInt32          tickCount;      /* System tick counter */
    UnsignedWide    currentTime;    /* Current microsecond time */
    UInt32          timeBase;       /* Time base for calculations */
    short           interruptMask;  /* Interrupt mask */
    long            activeTaskCount;
    void*           timerHandle;    /* Platform timer handle */
} gTMState = {0};

/* Internal function prototypes */
static void InitializeQueue(QHdrPtr queuePtr);
static void InsertTaskInQueue(TMTaskPtr tmTaskPtr);
static void RemoveTaskFromQueue(TMTaskPtr tmTaskPtr);
static TMTaskPtr GetNextExpiredTask(void);
static void ExecuteTask(TMTaskPtr tmTaskPtr);
static void UpdateCurrentTime(void);
static Boolean IsExtendedTask(TMTaskPtr tmTaskPtr);
static void TimerCallback(void);

/* Initialize Time Manager */
OSErr InitTimeManager(void) {
    if (gTMState.initialized) {
        return noErr;
    }

    /* Initialize HAL layer */
    OSErr err = TimeManager_HAL_Init();
    if (err != noErr) {
        return err;
    }

    /* Initialize queues */
    InitializeQueue(&gTMState.tmQueue);
    InitializeQueue(&gTMState.vblQueue);

    /* Initialize time tracking */
    gTMState.tickCount = 0;
    gTMState.currentTime.hi = 0;
    gTMState.currentTime.lo = 0;
    gTMState.timeBase = 0;
    gTMState.interruptMask = 0;
    gTMState.activeTaskCount = 0;

    /* Get initial time from HAL */
    TimeManager_HAL_GetMicroseconds(&gTMState.currentTime);

    /* Start platform timer */
    gTMState.timerHandle = TimeManager_HAL_CreateTimer(TimerCallback,
                                                       1000); /* 1ms resolution */
    if (!gTMState.timerHandle) {
        TimeManager_HAL_Cleanup();
        return memFullErr;
    }

    gTMState.initialized = true;
    return noErr;
}

/* Shutdown Time Manager */
void ShutdownTimeManager(void) {
    if (!gTMState.initialized) {
        return;
    }

    /* Stop platform timer */
    if (gTMState.timerHandle) {
        TimeManager_HAL_DestroyTimer(gTMState.timerHandle);
        gTMState.timerHandle = NULL;
    }

    /* Remove all tasks */
    while (gTMState.tmQueue.qHead) {
        RmvTime(gTMState.tmQueue.qHead);
    }

    while (gTMState.vblQueue.qHead) {
        VRemove(gTMState.vblQueue.qHead);
    }

    /* Cleanup HAL layer */
    TimeManager_HAL_Cleanup();

    gTMState.initialized = false;
}

/* Install time task */
OSErr InsTime(QElemPtr tmTaskPtr) {
    TMTaskPtr task = (TMTaskPtr)tmTaskPtr;

    if (!task || !gTMState.initialized) {
        return paramErr;
    }

    /* Initialize task */
    task->qLink = NULL;
    task->qType = tmType;
    task->tmWakeUp = 0;

    return noErr;
}

/* Install extended time task */
OSErr InstallXTimeTask(QElemPtr tmTaskPtr) {
    ExtendedTimerPtr task = (ExtendedTimerPtr)tmTaskPtr;

    if (!task || !gTMState.initialized) {
        return paramErr;
    }

    /* Initialize extended task */
    task->qLink = NULL;
    task->qType = tmType | tmTaskExtended;
    task->tmWakeUp = 0;
    task->tmWakeUpTime.hi = 0;
    task->tmWakeUpTime.lo = 0;

    return noErr;
}

/* Remove time task */
OSErr RmvTime(QElemPtr tmTaskPtr) {
    TMTaskPtr task = (TMTaskPtr)tmTaskPtr;

    if (!task || !gTMState.initialized) {
        return paramErr;
    }

    /* Calculate remaining time if task is active */
    if (task->qType & tmTaskActive) {
        UpdateCurrentTime();

        if (IsExtendedTask(task)) {
            ExtendedTimerPtr extTask = (ExtendedTimerPtr)task;
            UnsignedWide remaining;

            /* Calculate microseconds remaining */
            remaining.hi = extTask->tmWakeUpTime.hi - gTMState.currentTime.hi;
            remaining.lo = extTask->tmWakeUpTime.lo - gTMState.currentTime.lo;

            /* Handle underflow */
            if (remaining.lo > extTask->tmWakeUpTime.lo) {
                remaining.hi--;
            }

            /* Convert to negative microseconds in tmCount */
            if (remaining.hi == 0 && remaining.lo < 0x7FFFFFFF) {
                task->tmCount = -(long)remaining.lo;
            } else {
                task->tmCount = -0x7FFFFFFF; /* Max negative */
            }
        } else {
            /* Calculate milliseconds remaining */
            long ticksRemaining = task->tmWakeUp - gTMState.tickCount;
            if (ticksRemaining > 0) {
                task->tmCount = TicksToMilliseconds(ticksRemaining);
            } else {
                task->tmCount = 0;
            }
        }
    }

    /* Remove from queue */
    RemoveTaskFromQueue(task);

    /* Clear active flag */
    task->qType &= ~tmTaskActive;

    return noErr;
}

/* Prime time task */
OSErr PrimeTime(QElemPtr tmTaskPtr, long count) {
    TMTaskPtr task = (TMTaskPtr)tmTaskPtr;

    if (!task || !gTMState.initialized) {
        return paramErr;
    }

    UpdateCurrentTime();

    /* Remove from queue if already there */
    if (task->qType & tmTaskActive) {
        RemoveTaskFromQueue(task);
    }

    /* Store count */
    task->tmCount = count;

    /* Calculate wake-up time */
    if (IsExtendedTask(task)) {
        ExtendedTimerPtr extTask = (ExtendedTimerPtr)task;
        UnsignedWide delay;

        if (count < 0) {
            /* Negative count means microseconds */
            delay.hi = 0;
            delay.lo = (UInt32)(-count);
        } else {
            /* Positive count means milliseconds */
            delay.hi = 0;
            delay.lo = (UInt32)count * kMicrosecondsPerMillisecond;
        }

        /* Add delay to current time */
        extTask->tmWakeUpTime.lo = gTMState.currentTime.lo + delay.lo;
        extTask->tmWakeUpTime.hi = gTMState.currentTime.hi + delay.hi;

        /* Handle overflow */
        if (extTask->tmWakeUpTime.lo < gTMState.currentTime.lo) {
            extTask->tmWakeUpTime.hi++;
        }
    } else {
        /* Classic task - use tick-based timing */
        if (count < 0) {
            /* Immediate execution */
            task->tmWakeUp = gTMState.tickCount;
        } else {
            /* Calculate wake-up tick */
            task->tmWakeUp = gTMState.tickCount + MillisecondsToTicks(count);
        }
    }

    /* Set active flag and insert into queue */
    task->qType |= tmTaskActive | tmTaskPrimed;
    InsertTaskInQueue(task);

    return noErr;
}

/* Get current microsecond time */
void Microseconds(UnsignedWide* microTickCount) {
    if (!microTickCount) return;

    UpdateCurrentTime();
    *microTickCount = gTMState.currentTime;
}

/* Get current tick count */
UInt32 TickCount(void) {
    UpdateCurrentTime();
    return gTMState.tickCount;
}

/* Get current tick count (alias) */
UInt32 GetTicks(void) {
    return TickCount();
}

/* Delay for specified ticks */
void Delay(long numTicks, long* finalTicks) {
    UInt32 startTicks = TickCount();
    UInt32 endTicks = startTicks + numTicks;

    /* Busy wait or yield to system */
    while (TickCount() < endTicks) {
        TimeManager_HAL_Yield();
    }

    if (finalTicks) {
        *finalTicks = TickCount();
    }
}

/* Time conversion utilities */
long MillisecondsToTicks(long milliseconds) {
    return (milliseconds * kTicksPerSecond) / kMillisecondsPerSecond;
}

long TicksToMilliseconds(long ticks) {
    return (ticks * kMillisecondsPerSecond) / kTicksPerSecond;
}

long MicrosecondsToTicks(long microseconds) {
    return (microseconds * kTicksPerSecond) / kMicrosecondsPerSecond;
}

long TicksToMicroseconds(long ticks) {
    return (ticks * kMicrosecondsPerSecond) / kTicksPerSecond;
}

/* VBL task management */
OSErr VInstall(QElemPtr vblTaskPtr) {
    VBLTaskPtr task = (VBLTaskPtr)vblTaskPtr;

    if (!task || !gTMState.initialized) {
        return paramErr;
    }

    /* Initialize VBL task */
    task->qLink = NULL;
    task->qType = vblType;

    /* Add to VBL queue */
    task->qLink = gTMState.vblQueue.qHead;
    gTMState.vblQueue.qHead = (QElemPtr)task;

    if (!gTMState.vblQueue.qTail) {
        gTMState.vblQueue.qTail = (QElemPtr)task;
    }

    return noErr;
}

/* Remove VBL task */
OSErr VRemove(QElemPtr vblTaskPtr) {
    VBLTaskPtr task = (VBLTaskPtr)vblTaskPtr;
    QElemPtr prev = NULL;
    QElemPtr current = gTMState.vblQueue.qHead;

    if (!task || !gTMState.initialized) {
        return paramErr;
    }

    /* Find and remove task */
    while (current) {
        if (current == (QElemPtr)task) {
            if (prev) {
                prev->qLink = current->qLink;
            } else {
                gTMState.vblQueue.qHead = current->qLink;
            }

            if (gTMState.vblQueue.qTail == current) {
                gTMState.vblQueue.qTail = prev;
            }

            task->qLink = NULL;
            return noErr;
        }

        prev = current;
        current = current->qLink;
    }

    return qErr;
}

/* Timer UPP management */
TimerUPP NewTimerUPP(TimerProcPtr userRoutine) {
    /* In our implementation, UPPs are just function pointers */
    return (TimerUPP)userRoutine;
}

void DisposeTimerUPP(TimerUPP userUPP) {
    /* Nothing to dispose in our implementation */
}

void InvokeTimerUPP(TMTaskPtr tmTaskPtr, TimerUPP userUPP) {
    if (userUPP) {
        userUPP(tmTaskPtr);
    }
}

/* Get Time Manager information */
OSErr GetTimeManagerVersion(short* version) {
    if (!version) return paramErr;
    *version = 2; /* Version 2 supports extended timing */
    return noErr;
}

OSErr GetTimeManagerFeatures(long* features) {
    if (!features) return paramErr;
    *features = tmTaskExtended | tmTaskRecurring;
    return noErr;
}

Boolean TimeManagerAvailable(void) {
    return gTMState.initialized;
}

/* Get active timer count */
long GetActiveTimerCount(void) {
    return gTMState.activeTaskCount;
}

/* Internal functions */

static void InitializeQueue(QHdrPtr queuePtr) {
    queuePtr->qFlags = 0;
    queuePtr->qHead = NULL;
    queuePtr->qTail = NULL;
}

static void InsertTaskInQueue(TMTaskPtr tmTaskPtr) {
    QElemPtr prev = NULL;
    QElemPtr current = gTMState.tmQueue.qHead;
    long wakeTime;

    if (IsExtendedTask(tmTaskPtr)) {
        ExtendedTimerPtr extTask = (ExtendedTimerPtr)tmTaskPtr;
        wakeTime = extTask->tmWakeUpTime.lo; /* Simplified comparison */
    } else {
        wakeTime = tmTaskPtr->tmWakeUp;
    }

    /* Find insertion point (sorted by wake-up time) */
    while (current) {
        TMTaskPtr currentTask = (TMTaskPtr)current;
        long currentWakeTime;

        if (IsExtendedTask(currentTask)) {
            currentWakeTime = ((ExtendedTimerPtr)currentTask)->tmWakeUpTime.lo;
        } else {
            currentWakeTime = currentTask->tmWakeUp;
        }

        if (wakeTime < currentWakeTime) {
            break;
        }

        prev = current;
        current = current->qLink;
    }

    /* Insert task */
    tmTaskPtr->qLink = current;

    if (prev) {
        prev->qLink = (QElemPtr)tmTaskPtr;
    } else {
        gTMState.tmQueue.qHead = (QElemPtr)tmTaskPtr;
    }

    if (!current) {
        gTMState.tmQueue.qTail = (QElemPtr)tmTaskPtr;
    }

    gTMState.activeTaskCount++;
}

static void RemoveTaskFromQueue(TMTaskPtr tmTaskPtr) {
    QElemPtr prev = NULL;
    QElemPtr current = gTMState.tmQueue.qHead;

    while (current) {
        if (current == (QElemPtr)tmTaskPtr) {
            if (prev) {
                prev->qLink = current->qLink;
            } else {
                gTMState.tmQueue.qHead = current->qLink;
            }

            if (gTMState.tmQueue.qTail == current) {
                gTMState.tmQueue.qTail = prev;
            }

            tmTaskPtr->qLink = NULL;
            gTMState.activeTaskCount--;
            break;
        }

        prev = current;
        current = current->qLink;
    }
}

static TMTaskPtr GetNextExpiredTask(void) {
    TMTaskPtr task = (TMTaskPtr)gTMState.tmQueue.qHead;

    if (!task) return NULL;

    if (IsExtendedTask(task)) {
        ExtendedTimerPtr extTask = (ExtendedTimerPtr)task;

        /* Check if expired (simplified comparison) */
        if (extTask->tmWakeUpTime.hi < gTMState.currentTime.hi ||
            (extTask->tmWakeUpTime.hi == gTMState.currentTime.hi &&
             extTask->tmWakeUpTime.lo <= gTMState.currentTime.lo)) {
            return task;
        }
    } else {
        /* Check tick-based expiration */
        if (task->tmWakeUp <= gTMState.tickCount) {
            return task;
        }
    }

    return NULL;
}

static void ExecuteTask(TMTaskPtr tmTaskPtr) {
    if (tmTaskPtr->tmAddr) {
        /* Execute task procedure */
        tmTaskPtr->tmAddr(tmTaskPtr);

        /* Handle recurring tasks */
        if (tmTaskPtr->qType & tmTaskRecurring) {
            /* Re-prime with original count */
            PrimeTime((QElemPtr)tmTaskPtr, tmTaskPtr->tmCount);
        }
    }
}

static void UpdateCurrentTime(void) {
    UnsignedWide newTime;

    /* Get current microseconds from HAL */
    TimeManager_HAL_GetMicroseconds(&newTime);

    /* Update tick count based on elapsed time */
    if (gTMState.currentTime.lo != 0 || gTMState.currentTime.hi != 0) {
        UInt32 elapsed = newTime.lo - gTMState.currentTime.lo;
        if (newTime.lo < gTMState.currentTime.lo) {
            /* Handle wrap-around */
            elapsed = (0xFFFFFFFF - gTMState.currentTime.lo) + newTime.lo + 1;
        }

        /* Convert microseconds to ticks */
        gTMState.tickCount += MicrosecondsToTicks(elapsed);
    }

    gTMState.currentTime = newTime;
}

static Boolean IsExtendedTask(TMTaskPtr tmTaskPtr) {
    return (tmTaskPtr->qType & tmTaskExtended) != 0;
}

/* Timer callback - called by HAL timer */
static void TimerCallback(void) {
    TMTaskPtr expiredTask;

    UpdateCurrentTime();

    /* Process expired timer tasks */
    while ((expiredTask = GetNextExpiredTask()) != NULL) {
        RemoveTaskFromQueue(expiredTask);
        expiredTask->qType &= ~(tmTaskActive | tmTaskPrimed);
        ExecuteTask(expiredTask);
    }

    /* Process VBL tasks */
    if (gTMState.tickCount % (kTicksPerSecond / 60) == 0) {
        /* VBL occurs at 60Hz */
        VBLTaskPtr vblTask = (VBLTaskPtr)gTMState.vblQueue.qHead;

        while (vblTask) {
            VBLTaskPtr nextTask = (VBLTaskPtr)vblTask->qLink;

            if (vblTask->vblCount > 0) {
                vblTask->vblCount--;

                if (vblTask->vblCount == 0) {
                    /* Execute VBL task */
                    if (vblTask->vblAddr) {
                        vblTask->vblAddr();
                    }

                    /* Reset count for recurring VBL */
                    vblTask->vblCount = vblTask->vblPhase;
                }
            }

            vblTask = nextTask;
        }
    }
}
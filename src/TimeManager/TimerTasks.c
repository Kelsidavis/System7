/*
 * TimerTasks.c
 *
 * Timer Task Management for System 7.1 Time Manager
 *
 * This file implements the timer task management functions (InsTime, RmvTime, PrimeTime)
 * converted from the original 68k assembly implementation.
 *
 * These functions manage the Time Manager task queue and provide the core timing
 * services used throughout System 7.1.
 */

#include "TimeManager.h"
#include "TimerTypes.h"
#include "MicrosecondTimer.h"
#include "TimeBase.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

/* ===== External References ===== */
extern TimeMgrPrivate *gTimeMgrPrivate;
extern pthread_mutex_t gTimeMgrMutex;
extern bool gTimeMgrInitialized;

/* ===== Private Function Prototypes ===== */
static void FreezeTimeInternal(uint16_t *savedSR, uint8_t *timerLow);
static void ThawTimeInternal(uint16_t savedSR, uint8_t timerLow);
static OSErr InsertTaskInQueue(TMTaskPtr tmTaskPtr, uint32_t delayTime);
static OSErr RemoveTaskFromQueue(TMTaskPtr tmTaskPtr, uint32_t *remainingTime);
static uint32_t ConvertTimeToInternal(int32_t timeValue);
static int32_t ConvertInternalToTime(uint32_t internalTime);

/* ===== Timer Task Management Functions ===== */

/**
 * Remove Time Manager Task
 *
 * Removes a Time Manager Task from active management. If the task was active
 * and had time remaining, that time is returned in the tmCount field.
 */
OSErr RmvTime(TMTaskPtr tmTaskPtr)
{
    uint16_t savedSR;
    uint8_t timerLow;
    uint32_t remainingTime = 0;
    OSErr err;

    if (!tmTaskPtr) {
        return -1; // Invalid parameter
    }

    if (!gTimeMgrInitialized) {
        return -2; // Time Manager not initialized
    }

    // Freeze time operations for atomic queue manipulation
    FreezeTimeInternal(&savedSR, &timerLow);

    // Remove task from queue and get remaining time
    err = RemoveTaskFromQueue(tmTaskPtr, &remainingTime);

    // Thaw time operations
    ThawTimeInternal(savedSR, timerLow);

    if (err == NO_ERR) {
        // Convert remaining time to external format and store in tmCount
        tmTaskPtr->tmCount = ConvertInternalToTime(remainingTime);

        // Clear active flag
        CLEAR_TMTASK_ACTIVE(tmTaskPtr);
        tmTaskPtr->isActive = false;
    }

    return err;
}

/**
 * Prime Time Manager Task
 *
 * Schedules a Time Manager Task to execute after a specified delay.
 * The delay can be specified in milliseconds (positive) or microseconds (negative).
 */
OSErr PrimeTime(TMTaskPtr tmTaskPtr, int32_t count)
{
    uint16_t savedSR;
    uint8_t timerLow;
    uint32_t delayTime;
    OSErr err;

    if (!tmTaskPtr) {
        return -1; // Invalid parameter
    }

    if (!gTimeMgrInitialized) {
        return -2; // Time Manager not initialized
    }

    // Handle extended task backlog prevention (from Quicktime patch)
    if (IS_TMTASK_EXTENDED(tmTaskPtr)) {
        if (tmTaskPtr->tmWakeUp != 0) {
            if (count == 0) {
                // Increment retry counter and check for overflow
                uint8_t *retryCounter = (uint8_t*)&tmTaskPtr->tmReserved + 3;
                (*retryCounter)++;
                if (*retryCounter & 0x80) {
                    // Too many retries, convert to standard task
                    CLEAR_TMTASK_EXTENDED(tmTaskPtr);
                    tmTaskPtr->isExtended = false;
                }
            } else {
                // Reset retry counter
                *((uint8_t*)&tmTaskPtr->tmReserved + 3) = 0;
            }
        } else {
            // Clear retry counter for new extended task
            *((uint8_t*)&tmTaskPtr->tmReserved + 3) = 0;
        }
    }

    // Convert external time format to internal format
    delayTime = ConvertTimeToInternal(count);

    // Freeze time operations for atomic queue manipulation
    FreezeTimeInternal(&savedSR, &timerLow);

    // Set task as active
    SET_TMTASK_ACTIVE(tmTaskPtr);
    tmTaskPtr->isActive = true;
    tmTaskPtr->originalCount = count;

    // Handle extended task timing calculations
    if (IS_TMTASK_EXTENDED(tmTaskPtr)) {
        uint32_t currentTime = gTimeMgrPrivate->currentTime;
        uint32_t wakeTime = tmTaskPtr->tmWakeUp;

        if (wakeTime != 0) {
            // Calculate delay relative to previous wake time
            int32_t timeSinceWake = currentTime - wakeTime;
            if (timeSinceWake >= 0) {
                // We're past the wake time, subtract the lateness
                if (delayTime > (uint32_t)timeSinceWake) {
                    delayTime -= timeSinceWake;
                } else {
                    // We're very late - add to backlog and run immediately
                    uint32_t lateness = timeSinceWake - delayTime;
                    gTimeMgrPrivate->backLog += lateness;
                    delayTime = 0;

                    // Update wake time to reflect the backlog
                    tmTaskPtr->tmWakeUp = currentTime + delayTime;
                    if (tmTaskPtr->tmWakeUp == 0) {
                        tmTaskPtr->tmWakeUp = 1; // Avoid special zero value
                    }
                }
            } else {
                // We're before the wake time, add the remaining time
                delayTime += (-timeSinceWake);
            }
        }

        // Set new wake time if not already set
        if (wakeTime == 0 || count != 0) {
            tmTaskPtr->tmWakeUp = currentTime + delayTime;
            if (tmTaskPtr->tmWakeUp == 0) {
                tmTaskPtr->tmWakeUp = 1; // Avoid special zero value
            }
        }
    }

    // Insert task into the active queue
    err = InsertTaskInQueue(tmTaskPtr, delayTime);

    // Thaw time operations
    ThawTimeInternal(savedSR, timerLow);

    return err;
}

/* ===== Private Implementation Functions ===== */

/**
 * Freeze Time Operations
 *
 * Disables interrupts and captures current timer state for atomic operations.
 * This implements the FreezeTime functionality from the original code.
 */
static void FreezeTimeInternal(uint16_t *savedSR, uint8_t *timerLow)
{
    UnsignedWide currentTime;
    uint32_t timerValue;

    // Save current interrupt state (simulated)
    *savedSR = 0; // In the portable version, we use mutex locking

    // Lock the Time Manager mutex for atomic operations
    pthread_mutex_lock(&gTimeMgrMutex);

    // Get current high-resolution time
    GetPlatformTime(&currentTime);

    // Calculate elapsed time since last update
    uint64_t elapsed = currentTime.value - gTimeMgrPrivate->startTime;
    timerValue = (uint32_t)(elapsed / 1000); // Convert to microseconds

    // Update current time in internal format
    uint32_t newCurrentTime = MicrosecondsToInternal(timerValue);
    uint32_t timeDelta = newCurrentTime - gTimeMgrPrivate->currentTime;
    gTimeMgrPrivate->currentTime = newCurrentTime;

    // Save timer low byte (simulated VIA timer behavior)
    *timerLow = (uint8_t)(timerValue & ((1 << TICK_SCALE) - 1));

    // Update microsecond counter with threshold checking
    uint16_t timeThresh = gTimeMgrPrivate->curTimeThresh;
    uint16_t lowTime = gTimeMgrPrivate->currentTime & 0xFFFF;

    while (lowTime >= timeThresh) {
        timeThresh += THRESH_INC;
        gTimeMgrPrivate->fractUSecs += (USECS_INC & 0xFFFF);
        if (gTimeMgrPrivate->fractUSecs < (USECS_INC & 0xFFFF)) {
            // Handle carry
            gTimeMgrPrivate->lowUSecs += (USECS_INC >> 16) + 1;
            if (gTimeMgrPrivate->lowUSecs < (USECS_INC >> 16) + 1) {
                // Handle carry to high word
                gTimeMgrPrivate->highUSecs++;
            }
        } else {
            gTimeMgrPrivate->lowUSecs += (USECS_INC >> 16);
            if (gTimeMgrPrivate->lowUSecs < (USECS_INC >> 16)) {
                // Handle carry to high word
                gTimeMgrPrivate->highUSecs++;
            }
        }
    }
    gTimeMgrPrivate->curTimeThresh = timeThresh;

    // Adjust active task times based on elapsed time
    TMTaskPtr activeTask = gTimeMgrPrivate->activePtr;
    if (activeTask && timeDelta > 0) {
        if (timeDelta >= gTimeMgrPrivate->backLog) {
            timeDelta -= gTimeMgrPrivate->backLog;
            gTimeMgrPrivate->backLog = 0;

            if (timeDelta >= activeTask->tmCount) {
                // Task has expired or will expire
                gTimeMgrPrivate->backLog = timeDelta - activeTask->tmCount;
                activeTask->tmCount = 0;
            } else {
                activeTask->tmCount -= timeDelta;
            }
        } else {
            gTimeMgrPrivate->backLog -= timeDelta;
        }
    }
}

/**
 * Thaw Time Operations
 *
 * Restores interrupt state and starts the next timer.
 * This implements the ThawTime functionality from the original code.
 */
static void ThawTimeInternal(uint16_t savedSR, uint8_t timerLow)
{
    TMTaskPtr activeTask;
    uint32_t nextDelay;
    uint32_t timerValue;

    (void)savedSR; // Unused in portable version
    (void)timerLow; // Stored for compatibility

    // Calculate next timer delay
    activeTask = gTimeMgrPrivate->activePtr;
    if (activeTask) {
        nextDelay = activeTask->tmCount;
        if (nextDelay > gTimeMgrPrivate->backLog) {
            nextDelay -= gTimeMgrPrivate->backLog;
            gTimeMgrPrivate->backLog = 0;
        } else {
            gTimeMgrPrivate->backLog -= nextDelay;
            nextDelay = 0;
        }

        // Limit to maximum timer range
        if (nextDelay > MAX_TIMER_RANGE) {
            timerValue = MAX_TIMER_RANGE;
            activeTask->tmCount = nextDelay - MAX_TIMER_RANGE;
        } else {
            timerValue = nextDelay;
            activeTask->tmCount = 0;
        }
    } else {
        // No active tasks, use maximum timer value
        timerValue = MAX_TIMER_RANGE;
        gTimeMgrPrivate->backLog = 0;
    }

    // Update current time with timer value
    gTimeMgrPrivate->currentTime += timerValue;

    // In the original, this would start the VIA timer
    // In the portable version, the timer thread handles timing

    // Restore interrupt state (unlock mutex)
    pthread_mutex_unlock(&gTimeMgrMutex);
}

/**
 * Insert Task in Active Queue
 *
 * Inserts a timer task into the active queue, maintaining proper ordering
 * by expiration time.
 */
static OSErr InsertTaskInQueue(TMTaskPtr tmTaskPtr, uint32_t delayTime)
{
    TMTaskPtr *prevLink = &gTimeMgrPrivate->activePtr;
    TMTaskPtr current = gTimeMgrPrivate->activePtr;
    uint32_t totalTime = delayTime + gTimeMgrPrivate->backLog;

    // Remove task from queue if it's already active
    if (tmTaskPtr->isActive) {
        uint32_t dummy;
        RemoveTaskFromQueue(tmTaskPtr, &dummy);
    }

    // Find insertion point in the queue
    while (current != NULL) {
        if (totalTime <= current->tmCount) {
            // Insert before this task
            current->tmCount -= totalTime;
            break;
        }
        totalTime -= current->tmCount;
        prevLink = &current->qLink;
        current = current->qLink;
    }

    // Insert the task
    tmTaskPtr->qLink = current;
    tmTaskPtr->tmCount = totalTime;
    *prevLink = tmTaskPtr;

    return NO_ERR;
}

/**
 * Remove Task from Active Queue
 *
 * Removes a timer task from the active queue and calculates remaining time.
 */
static OSErr RemoveTaskFromQueue(TMTaskPtr tmTaskPtr, uint32_t *remainingTime)
{
    TMTaskPtr *prevLink = &gTimeMgrPrivate->activePtr;
    TMTaskPtr current = gTimeMgrPrivate->activePtr;
    uint32_t totalTime = 0;

    // Search for the task in the queue
    while (current != NULL) {
        totalTime += current->tmCount;

        if (current == tmTaskPtr) {
            // Found the task - remove it
            *prevLink = current->qLink;

            // Pass remaining time to next task if present
            if (current->qLink != NULL) {
                current->qLink->tmCount += current->tmCount;
            }

            // Calculate total remaining time
            *remainingTime = totalTime;
            if (*remainingTime > gTimeMgrPrivate->backLog) {
                *remainingTime -= gTimeMgrPrivate->backLog;
            } else {
                *remainingTime = 0;
            }

            // Clear task linkage
            tmTaskPtr->qLink = NULL;
            tmTaskPtr->tmCount = 0;

            return NO_ERR;
        }

        prevLink = &current->qLink;
        current = current->qLink;
    }

    // Task not found in queue
    *remainingTime = 0;
    return -1;
}

/**
 * Convert External Time to Internal Format
 *
 * Converts time from external format (positive milliseconds or negative
 * microseconds) to internal timer format.
 */
static uint32_t ConvertTimeToInternal(int32_t timeValue)
{
    if (timeValue >= 0) {
        // Positive value = milliseconds
        return MillisecondsToInternal((uint32_t)timeValue);
    } else {
        // Negative value = negated microseconds
        return MicrosecondsToInternal((uint32_t)(-timeValue));
    }
}

/**
 * Convert Internal Time to External Format
 *
 * Converts time from internal format to external format, choosing between
 * milliseconds and microseconds based on magnitude.
 */
static int32_t ConvertInternalToTime(uint32_t internalTime)
{
    uint32_t microseconds = InternalToMicroseconds(internalTime);

    // Try to represent as negated microseconds first
    if (microseconds <= 0x7FFFFFFF) {
        return -(int32_t)microseconds;
    }

    // Value too large for microseconds, use milliseconds
    uint32_t milliseconds = InternalToMilliseconds(internalTime);
    if (milliseconds <= 0x7FFFFFFF) {
        return (int32_t)milliseconds;
    }

    // Value too large even for milliseconds, return maximum
    return 0x7FFFFFFF;
}

/* ===== Freeze/Thaw Time Public Interface ===== */

/**
 * Freeze Time Manager operations
 *
 * Public interface to freeze time operations for external callers.
 */
void FreezeTime(uint16_t *savedSR, uint8_t *timerLow)
{
    if (!gTimeMgrInitialized || !savedSR || !timerLow) {
        return;
    }

    FreezeTimeInternal(savedSR, timerLow);
}

/**
 * Thaw Time Manager operations
 *
 * Public interface to thaw time operations for external callers.
 */
void ThawTime(uint16_t savedSR, uint8_t timerLow)
{
    if (!gTimeMgrInitialized) {
        return;
    }

    ThawTimeInternal(savedSR, timerLow);
}
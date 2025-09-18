/*
 * NotificationScheduler.c
 *
 * Priority-based notification scheduling and timing
 * Manages when and how notifications are displayed
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/NotificationManager.h"
#include "NotificationManager/NotificationQueue.h"
#include "NotificationManager/SystemAlerts.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"

/* Scheduler state */
typedef struct {
    Boolean     initialized;
    Boolean     enabled;
    UInt32      lastScheduleTime;
    UInt32      scheduleInterval;
    short       maxConcurrentNotifications;
    short       currentActiveCount;
    Boolean     priorityScheduling;
    Boolean     timeSlicing;
    UInt32      timeSliceDuration;
    short       roundRobinIndex;
} SchedulerState;

static SchedulerState gScheduler = {0};

/* Internal prototypes */
static void NMScheduleByPriority(void);
static void NMScheduleRoundRobin(void);
static Boolean NMCanScheduleNotification(NMExtendedRecPtr nmExtPtr);
static void NMUpdateSchedulerStats(void);

/*
 * Notification Scheduler Implementation
 */

OSErr NMSchedulerInit(void)
{
    if (gScheduler.initialized) {
        return noErr;
    }

    gScheduler.enabled = true;
    gScheduler.lastScheduleTime = NMGetTimestamp();
    gScheduler.scheduleInterval = 10; /* 10 ticks */
    gScheduler.maxConcurrentNotifications = 5;
    gScheduler.currentActiveCount = 0;
    gScheduler.priorityScheduling = true;
    gScheduler.timeSlicing = false;
    gScheduler.timeSliceDuration = 60; /* 1 second */
    gScheduler.roundRobinIndex = 0;

    gScheduler.initialized = true;
    return noErr;
}

void NMSchedulerCleanup(void)
{
    gScheduler.initialized = false;
}

void NMScheduleNotifications(void)
{
    UInt32 currentTime;

    if (!gScheduler.initialized || !gScheduler.enabled) {
        return;
    }

    currentTime = NMGetTimestamp();

    /* Check if it's time to schedule */
    if ((currentTime - gScheduler.lastScheduleTime) < gScheduler.scheduleInterval) {
        return;
    }

    gScheduler.lastScheduleTime = currentTime;

    /* Schedule based on configured method */
    if (gScheduler.priorityScheduling) {
        NMScheduleByPriority();
    } else {
        NMScheduleRoundRobin();
    }

    NMUpdateSchedulerStats();
}

static void NMScheduleByPriority(void)
{
    NMExtendedRecPtr nmExtPtr;
    NMPriority currentPriority;

    /* Schedule by priority: Urgent -> High -> Normal -> Low */
    for (currentPriority = nmPriorityUrgent; currentPriority >= nmPriorityLow; currentPriority--) {
        if (gScheduler.currentActiveCount >= gScheduler.maxConcurrentNotifications) {
            break;
        }

        nmExtPtr = NMGetNextByPriority(currentPriority);
        while (nmExtPtr && gScheduler.currentActiveCount < gScheduler.maxConcurrentNotifications) {
            if (nmExtPtr->status == nmStatusPosted && NMCanScheduleNotification(nmExtPtr)) {
                /* Process this notification */
                if (nmExtPtr->base.nmStr) {
                    NMShowSystemAlert(nmExtPtr);
                    gScheduler.currentActiveCount++;
                }
            }
            nmExtPtr = NMGetNextByPriority(currentPriority);
        }
    }
}

static void NMScheduleRoundRobin(void)
{
    NMExtendedRecPtr nmExtPtr;
    short count = 0;
    const short maxCheck = 20; /* Limit iterations */

    nmExtPtr = NMGetFirstInQueue();

    /* Advance to round-robin starting point */
    for (int i = 0; i < gScheduler.roundRobinIndex && nmExtPtr; i++) {
        nmExtPtr = NMGetNextInQueue(nmExtPtr);
    }

    /* Schedule notifications in round-robin fashion */
    while (nmExtPtr && count < maxCheck &&
           gScheduler.currentActiveCount < gScheduler.maxConcurrentNotifications) {

        if (nmExtPtr->status == nmStatusPosted && NMCanScheduleNotification(nmExtPtr)) {
            if (nmExtPtr->base.nmStr) {
                NMShowSystemAlert(nmExtPtr);
                gScheduler.currentActiveCount++;
            }
        }

        nmExtPtr = NMGetNextInQueue(nmExtPtr);
        count++;

        /* Wrap around if we reach the end */
        if (!nmExtPtr) {
            nmExtPtr = NMGetFirstInQueue();
            gScheduler.roundRobinIndex = 0;
        } else {
            gScheduler.roundRobinIndex++;
        }
    }
}

static Boolean NMCanScheduleNotification(NMExtendedRecPtr nmExtPtr)
{
    if (!nmExtPtr) {
        return false;
    }

    /* Check if we're already at capacity */
    if (gScheduler.currentActiveCount >= gScheduler.maxConcurrentNotifications) {
        return false;
    }

    /* Check if notification is in correct state */
    if (nmExtPtr->status != nmStatusPosted) {
        return false;
    }

    /* Check priority constraints */
    if (gScheduler.priorityScheduling) {
        /* In priority mode, only schedule if higher priority notifications aren't waiting */
        if (nmExtPtr->priority < nmPriorityHigh) {
            /* Check if higher priority notifications are pending */
            if (NMCountByPriority(nmPriorityUrgent) > 0 ||
                NMCountByPriority(nmPriorityHigh) > 0) {
                return false;
            }
        }
    }

    return true;
}

static void NMUpdateSchedulerStats(void)
{
    short totalDisplayed = 0;
    NMExtendedRecPtr nmExtPtr;

    /* Count currently displayed notifications */
    nmExtPtr = NMGetFirstInQueue();
    while (nmExtPtr) {
        if (nmExtPtr->status == nmStatusDisplayed) {
            totalDisplayed++;
        }
        nmExtPtr = NMGetNextInQueue(nmExtPtr);
    }

    gScheduler.currentActiveCount = totalDisplayed;
}

OSErr NMSetSchedulerEnabled(Boolean enabled)
{
    if (!gScheduler.initialized) {
        return nmErrNotInstalled;
    }

    gScheduler.enabled = enabled;
    return noErr;
}

Boolean NMIsSchedulerEnabled(void)
{
    return gScheduler.initialized && gScheduler.enabled;
}

OSErr NMSetMaxConcurrentNotifications(short maxCount)
{
    if (!gScheduler.initialized) {
        return nmErrNotInstalled;
    }

    if (maxCount < 1 || maxCount > 20) {
        return nmErrInvalidParameter;
    }

    gScheduler.maxConcurrentNotifications = maxCount;
    return noErr;
}

short NMGetMaxConcurrentNotifications(void)
{
    return gScheduler.maxConcurrentNotifications;
}

OSErr NMSetSchedulingMode(Boolean priorityMode)
{
    if (!gScheduler.initialized) {
        return nmErrNotInstalled;
    }

    gScheduler.priorityScheduling = priorityMode;
    return noErr;
}

Boolean NMIsPriorityScheduling(void)
{
    return gScheduler.priorityScheduling;
}
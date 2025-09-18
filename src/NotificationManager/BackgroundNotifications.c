/*
 * BackgroundNotifications.c
 *
 * Background task notifications and system resource monitoring
 * Handles notifications for background processes and system events
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/BackgroundNotifications.h"
#include "NotificationManager/NotificationManager.h"
#include "NotificationManager/NotificationQueue.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"
#include "Gestalt.h"

/* Background notification state */
static Boolean gBGInitialized = false;
static BackgroundTaskPtr gTaskRegistry[BG_MAX_TASKS];
static short gTaskCount = 0;
static UInt32 gNextTaskID = 1;
static SystemResourceStatus gResourceStatus = {0};
static ResourceMonitorProc gResourceMonitor = NULL;
static void *gResourceContext = NULL;
static Boolean gResourceMonitorActive = false;
static UInt32 gLastResourceCheck = 0;
static BackgroundEventProc gEventHandlers[bgNotifyUserDefined];
static void *gEventContexts[bgNotifyUserDefined];
static Boolean gNotificationEnabled[bgNotifyUserDefined];
static Boolean gGlobalEnabled = true;

/* Resource thresholds */
static UInt32 gMinMemory = BG_DEFAULT_MIN_MEMORY;
static UInt32 gMinDiskSpace = BG_DEFAULT_MIN_DISK;
static short gMinBattery = BG_DEFAULT_MIN_BATTERY;

/* Internal function prototypes */
static OSErr BGInitializeRegistry(void);
static void BGCleanupRegistry(void);
static BackgroundTaskPtr BGFindTask(UInt32 taskID);
static OSErr BGValidateTaskState(BackgroundTaskState state);
static void BGTriggerTaskStateChange(BackgroundTaskPtr taskPtr, BackgroundTaskState newState);
static OSErr BGCheckSystemResources(void);
static void BGProcessResourceAlerts(SystemResourceStatus *oldStatus, SystemResourceStatus *newStatus);
static StringPtr BGCreateResourceMessage(BackgroundNotificationType type, UInt32 value);

/*
 * Initialization and Cleanup
 */

OSErr BGNotifyInit(void)
{
    OSErr err;
    int i;

    if (gBGInitialized) {
        return noErr;
    }

    /* Initialize task registry */
    err = BGInitializeRegistry();
    if (err != noErr) {
        return err;
    }

    /* Initialize event handlers array */
    for (i = 0; i < bgNotifyUserDefined; i++) {
        gEventHandlers[i] = NULL;
        gEventContexts[i] = NULL;
        gNotificationEnabled[i] = true; /* Enable all by default */
    }

    /* Initialize resource monitoring */
    gResourceMonitorActive = false;
    gLastResourceCheck = NMGetTimestamp();

    /* Get initial system resource status */
    BGGetSystemStatus(&gResourceStatus);

    /* Initialize platform-specific components */
    err = BGPlatformInit();
    if (err != noErr) {
        BGCleanupRegistry();
        return err;
    }

    gBGInitialized = true;
    return noErr;
}

void BGNotifyCleanup(void)
{
    if (!gBGInitialized) {
        return;
    }

    /* Stop resource monitoring */
    BGStopResourceMonitoring();

    /* Cleanup platform components */
    BGPlatformCleanup();

    /* Cleanup task registry */
    BGCleanupRegistry();

    gBGInitialized = false;
}

/*
 * Task Registration
 */

OSErr BGRegisterTask(BackgroundTaskPtr taskPtr)
{
    OSErr err;
    int i;

    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    if (!taskPtr) {
        return nmErrInvalidParameter;
    }

    err = BGValidateTaskPtr(taskPtr);
    if (err != noErr) {
        return err;
    }

    /* Check if we have space for more tasks */
    if (gTaskCount >= BG_MAX_TASKS) {
        return bgErrTooManyTasks;
    }

    /* Check if task already exists */
    if (BGFindTask(taskPtr->taskID) != NULL) {
        return bgErrTaskExists;
    }

    /* Find empty slot in registry */
    for (i = 0; i < BG_MAX_TASKS; i++) {
        if (gTaskRegistry[i] == NULL) {
            /* Allocate and copy task registration */
            gTaskRegistry[i] = (BackgroundTaskPtr)NewPtrClear(sizeof(BackgroundTaskRegistration));
            if (!gTaskRegistry[i]) {
                return nmErrOutOfMemory;
            }

            BlockMoveData(taskPtr, gTaskRegistry[i], sizeof(BackgroundTaskRegistration));

            /* Set registration time */
            gTaskRegistry[i]->registrationTime = NMGetTimestamp();
            gTaskRegistry[i]->lastActivity = gTaskRegistry[i]->registrationTime;

            /* Register with platform */
            err = BGPlatformRegisterTask(gTaskRegistry[i]);
            if (err != noErr) {
                DisposePtr((Ptr)gTaskRegistry[i]);
                gTaskRegistry[i] = NULL;
                return err;
            }

            gTaskCount++;
            return noErr;
        }
    }

    return bgErrTooManyTasks;
}

OSErr BGUnregisterTask(UInt32 taskID)
{
    BackgroundTaskPtr taskPtr;
    int i;

    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    /* Find task in registry */
    for (i = 0; i < BG_MAX_TASKS; i++) {
        if (gTaskRegistry[i] && gTaskRegistry[i]->taskID == taskID) {
            taskPtr = gTaskRegistry[i];

            /* Unregister from platform */
            BGPlatformUnregisterTask(taskID);

            /* Clean up task registration */
            DisposePtr((Ptr)taskPtr);
            gTaskRegistry[i] = NULL;
            gTaskCount--;

            return noErr;
        }
    }

    return bgErrTaskNotFound;
}

OSErr BGUpdateTaskState(UInt32 taskID, BackgroundTaskState newState)
{
    BackgroundTaskPtr taskPtr;
    BackgroundTaskState oldState;
    OSErr err;

    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    err = BGValidateTaskState(newState);
    if (err != noErr) {
        return err;
    }

    taskPtr = BGFindTask(taskID);
    if (!taskPtr) {
        return bgErrTaskNotFound;
    }

    oldState = taskPtr->state;
    taskPtr->state = newState;
    taskPtr->lastActivity = NMGetTimestamp();

    /* Update platform */
    BGPlatformUpdateTaskState(taskID, newState);

    /* Trigger state change notification if enabled */
    if (taskPtr->notifyOnStateChange && oldState != newState) {
        BGTriggerTaskStateChange(taskPtr, newState);
    }

    return noErr;
}

/*
 * Background Notifications
 */

OSErr BGPostNotification(BackgroundNotificationPtr bgNotifyPtr)
{
    NMExtendedRec nmExtRec;
    OSErr err;

    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    if (!bgNotifyPtr) {
        return nmErrInvalidParameter;
    }

    err = BGValidateNotificationPtr(bgNotifyPtr);
    if (err != noErr) {
        return err;
    }

    /* Check if this notification type is enabled */
    if (!BGIsNotificationEnabled(bgNotifyPtr->type)) {
        return noErr; /* Silently ignore disabled notifications */
    }

    /* Create extended notification record */
    nmExtRec.base.qLink = NULL;
    nmExtRec.base.qType = nmType;
    nmExtRec.base.nmFlags = 0;
    nmExtRec.base.nmPrivate = 0;
    nmExtRec.base.nmReserved = 0;
    nmExtRec.base.nmMark = 0;
    nmExtRec.base.nmIcon = bgNotifyPtr->icon;
    nmExtRec.base.nmSound = bgNotifyPtr->sound;
    nmExtRec.base.nmStr = bgNotifyPtr->message;
    nmExtRec.base.nmResp = bgNotifyPtr->callback;
    nmExtRec.base.nmRefCon = bgNotifyPtr->refCon;

    /* Set extended properties */
    nmExtRec.priority = bgNotifyPtr->urgent ? nmPriorityHigh : nmPriorityNormal;
    nmExtRec.status = nmStatusPending;
    nmExtRec.timestamp = bgNotifyPtr->timestamp;
    nmExtRec.timeout = 0; /* No timeout for background notifications by default */
    nmExtRec.richContent = bgNotifyPtr->taskData;
    nmExtRec.modernCallback = NULL;
    nmExtRec.callbackContext = NULL;
    nmExtRec.persistent = bgNotifyPtr->persistent;
    nmExtRec.modal = false;
    nmExtRec.groupID = (short)bgNotifyPtr->type;

    /* Create category string */
    if (bgNotifyPtr->appName) {
        nmExtRec.category = (StringPtr)NewPtr(bgNotifyPtr->appName[0] + 1);
        if (nmExtRec.category) {
            BlockMoveData(bgNotifyPtr->appName, nmExtRec.category, bgNotifyPtr->appName[0] + 1);
        }
    } else {
        nmExtRec.category = NULL;
    }

    /* Post the notification */
    err = NMInstallExtended(&nmExtRec);

    return err;
}

OSErr BGPostSystemNotification(BackgroundNotificationType type,
                              StringPtr message,
                              Handle icon)
{
    BackgroundNotificationRequest bgNotify;

    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    /* Create system notification */
    bgNotify.type = type;
    bgNotify.appSignature = 'SYST';
    bgNotify.appName = "\psystem";
    bgNotify.taskState = bgTaskActive;
    bgNotify.taskID = 0;
    bgNotify.taskData = NULL;
    bgNotify.message = message;
    bgNotify.icon = icon;
    bgNotify.sound = NULL;
    bgNotify.timestamp = NMGetTimestamp();
    bgNotify.refCon = type;
    bgNotify.callback = NULL;
    bgNotify.persistent = false;
    bgNotify.urgent = (type == bgNotifyLowMemory || type == bgNotifyBatteryLow);

    return BGPostNotification(&bgNotify);
}

OSErr BGPostTaskNotification(UInt32 taskID,
                           BackgroundNotificationType type,
                           StringPtr message)
{
    BackgroundTaskPtr taskPtr;
    BackgroundNotificationRequest bgNotify;

    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    taskPtr = BGFindTask(taskID);
    if (!taskPtr) {
        return bgErrTaskNotFound;
    }

    /* Create task notification */
    bgNotify.type = type;
    bgNotify.appSignature = taskPtr->appSignature;
    bgNotify.appName = taskPtr->appName;
    bgNotify.taskState = taskPtr->state;
    bgNotify.taskID = taskID;
    bgNotify.taskData = NULL;
    bgNotify.message = message;
    bgNotify.icon = NULL;
    bgNotify.sound = NULL;
    bgNotify.timestamp = NMGetTimestamp();
    bgNotify.refCon = taskID;
    bgNotify.callback = taskPtr->statusCallback;
    bgNotify.persistent = taskPtr->notifyOnCompletion;
    bgNotify.urgent = (taskPtr->state == bgTaskError);

    return BGPostNotification(&bgNotify);
}

/*
 * System Resource Monitoring
 */

OSErr BGStartResourceMonitoring(ResourceMonitorProc monitorProc, void *context)
{
    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    gResourceMonitor = monitorProc;
    gResourceContext = context;
    gResourceMonitorActive = true;
    gLastResourceCheck = NMGetTimestamp();

    /* Get initial status */
    BGGetSystemStatus(&gResourceStatus);

    return noErr;
}

OSErr BGStopResourceMonitoring(void)
{
    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    gResourceMonitorActive = false;
    gResourceMonitor = NULL;
    gResourceContext = NULL;

    return noErr;
}

OSErr BGGetSystemStatus(SystemResourceStatus *status)
{
    long freeBytes, totalBytes;
    OSErr err;

    if (!status) {
        return nmErrInvalidParameter;
    }

    /* Get memory information */
    freeBytes = FreeMem();
    totalBytes = freeBytes + (long)GetApplLimit() - (long)GetCurrentA5();
    status->freeMemory = (UInt32)freeBytes;
    status->totalMemory = (UInt32)totalBytes;

    /* Get disk space information */
    /* This would require platform-specific implementation */
    status->freeDiskSpace = 0;
    status->totalDiskSpace = 0;

    /* Get battery level (if supported) */
    status->batteryLevel = 100; /* Default to full battery */

    /* Check network connectivity */
    status->networkConnected = true; /* Default to connected */

    /* CPU usage and network activity would be platform-specific */
    status->cpuUsage = 0;
    status->networkActivity = 0;

    return noErr;
}

OSErr BGSetResourceThresholds(UInt32 minMemory, UInt32 minDiskSpace, short minBattery)
{
    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    gMinMemory = minMemory;
    gMinDiskSpace = minDiskSpace;
    gMinBattery = minBattery;

    return noErr;
}

/*
 * Background Processing
 */

void BGProcessBackgroundTasks(void)
{
    int i;
    BackgroundTaskPtr taskPtr;
    UInt32 currentTime;

    if (!gBGInitialized || !gGlobalEnabled) {
        return;
    }

    currentTime = NMGetTimestamp();

    /* Check for timed out tasks */
    for (i = 0; i < BG_MAX_TASKS; i++) {
        taskPtr = gTaskRegistry[i];
        if (taskPtr && taskPtr->state == bgTaskActive) {
            if ((currentTime - taskPtr->lastActivity) > BG_TASK_TIMEOUT) {
                /* Task has timed out */
                BGUpdateTaskState(taskPtr->taskID, bgTaskError);

                if (taskPtr->notifyOnError) {
                    BGPostTaskNotification(taskPtr->taskID, bgNotifySystemEvent,
                                         "\pTask timed out");
                }
            }
        }
    }

    /* Check system resources */
    if (gResourceMonitorActive &&
        (currentTime - gLastResourceCheck) > BG_RESOURCE_CHECK_INTERVAL) {
        BGCheckResourceStatus();
        gLastResourceCheck = currentTime;
    }
}

void BGCheckResourceStatus(void)
{
    SystemResourceStatus oldStatus, newStatus;
    OSErr err;

    if (!gBGInitialized) {
        return;
    }

    oldStatus = gResourceStatus;
    err = BGGetSystemStatus(&newStatus);
    if (err != noErr) {
        return;
    }

    gResourceStatus = newStatus;

    /* Process resource alerts */
    BGProcessResourceAlerts(&oldStatus, &newStatus);

    /* Call user resource monitor if set */
    if (gResourceMonitor) {
        gResourceMonitor(&newStatus, gResourceContext);
    }
}

/*
 * Configuration
 */

OSErr BGSetNotificationEnabled(BackgroundNotificationType type, Boolean enabled)
{
    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    if (type < 1 || type >= bgNotifyUserDefined) {
        return nmErrInvalidParameter;
    }

    gNotificationEnabled[type - 1] = enabled;
    return noErr;
}

Boolean BGIsNotificationEnabled(BackgroundNotificationType type)
{
    if (!gBGInitialized || !gGlobalEnabled) {
        return false;
    }

    if (type < 1 || type >= bgNotifyUserDefined) {
        return false;
    }

    return gNotificationEnabled[type - 1];
}

OSErr BGSetGlobalEnabled(Boolean enabled)
{
    if (!gBGInitialized) {
        return bgErrNotInitialized;
    }

    gGlobalEnabled = enabled;
    return noErr;
}

Boolean BGIsGlobalEnabled(void)
{
    return gBGInitialized && gGlobalEnabled;
}

/*
 * Internal Implementation
 */

static OSErr BGInitializeRegistry(void)
{
    int i;

    for (i = 0; i < BG_MAX_TASKS; i++) {
        gTaskRegistry[i] = NULL;
    }

    gTaskCount = 0;
    gNextTaskID = 1;

    return noErr;
}

static void BGCleanupRegistry(void)
{
    int i;

    for (i = 0; i < BG_MAX_TASKS; i++) {
        if (gTaskRegistry[i]) {
            BGPlatformUnregisterTask(gTaskRegistry[i]->taskID);
            DisposePtr((Ptr)gTaskRegistry[i]);
            gTaskRegistry[i] = NULL;
        }
    }

    gTaskCount = 0;
}

static BackgroundTaskPtr BGFindTask(UInt32 taskID)
{
    int i;

    for (i = 0; i < BG_MAX_TASKS; i++) {
        if (gTaskRegistry[i] && gTaskRegistry[i]->taskID == taskID) {
            return gTaskRegistry[i];
        }
    }

    return NULL;
}

static OSErr BGValidateTaskState(BackgroundTaskState state)
{
    switch (state) {
        case bgTaskInactive:
        case bgTaskActive:
        case bgTaskSuspended:
        case bgTaskError:
        case bgTaskCompleted:
            return noErr;
        default:
            return bgErrInvalidState;
    }
}

static void BGTriggerTaskStateChange(BackgroundTaskPtr taskPtr, BackgroundTaskState newState)
{
    StringPtr message;

    /* Create state change message */
    switch (newState) {
        case bgTaskActive:
            message = "\pTask activated";
            break;
        case bgTaskSuspended:
            message = "\pTask suspended";
            break;
        case bgTaskError:
            message = "\pTask encountered error";
            break;
        case bgTaskCompleted:
            message = "\pTask completed";
            break;
        default:
            message = "\pTask state changed";
            break;
    }

    BGPostTaskNotification(taskPtr->taskID, bgNotifySystemEvent, message);
}

static void BGProcessResourceAlerts(SystemResourceStatus *oldStatus, SystemResourceStatus *newStatus)
{
    /* Check memory threshold */
    if (newStatus->freeMemory < gMinMemory &&
        oldStatus->freeMemory >= gMinMemory) {
        BGPostSystemNotification(bgNotifyLowMemory,
                               BGCreateResourceMessage(bgNotifyLowMemory, newStatus->freeMemory),
                               NULL);
    }

    /* Check disk space threshold */
    if (newStatus->freeDiskSpace < gMinDiskSpace &&
        oldStatus->freeDiskSpace >= gMinDiskSpace) {
        BGPostSystemNotification(bgNotifyDiskFull,
                               BGCreateResourceMessage(bgNotifyDiskFull, newStatus->freeDiskSpace),
                               NULL);
    }

    /* Check battery threshold */
    if (newStatus->batteryLevel < gMinBattery &&
        oldStatus->batteryLevel >= gMinBattery) {
        BGPostSystemNotification(bgNotifyBatteryLow,
                               BGCreateResourceMessage(bgNotifyBatteryLow, newStatus->batteryLevel),
                               NULL);
    }
}

static StringPtr BGCreateResourceMessage(BackgroundNotificationType type, UInt32 value)
{
    static Str255 message;

    switch (type) {
        case bgNotifyLowMemory:
            sprintf((char *)&message[1], "Low memory: %lu bytes available", value);
            break;
        case bgNotifyDiskFull:
            sprintf((char *)&message[1], "Disk full: %lu bytes available", value);
            break;
        case bgNotifyBatteryLow:
            sprintf((char *)&message[1], "Low battery: %lu%% remaining", value);
            break;
        default:
            sprintf((char *)&message[1], "System resource alert: %lu", value);
            break;
    }

    message[0] = strlen((char *)&message[1]);
    return message;
}

UInt32 BGGenerateTaskID(void)
{
    if (!gBGInitialized) {
        return 0;
    }

    return gNextTaskID++;
}

OSErr BGValidateTaskPtr(BackgroundTaskPtr taskPtr)
{
    if (!taskPtr) {
        return nmErrInvalidParameter;
    }

    if (taskPtr->taskID == 0) {
        return bgErrInvalidTaskID;
    }

    if (!taskPtr->appName || taskPtr->appName[0] == 0) {
        return nmErrInvalidParameter;
    }

    return BGValidateTaskState(taskPtr->state);
}

OSErr BGValidateNotificationPtr(BackgroundNotificationPtr bgNotifyPtr)
{
    if (!bgNotifyPtr) {
        return nmErrInvalidParameter;
    }

    if (bgNotifyPtr->type < 1 || bgNotifyPtr->type >= bgNotifyUserDefined) {
        return nmErrInvalidParameter;
    }

    if (!bgNotifyPtr->message || bgNotifyPtr->message[0] == 0) {
        return nmErrInvalidParameter;
    }

    return noErr;
}

/*
 * Weak platform stubs - should be implemented platform-specifically
 */

#pragma weak BGPlatformInit
OSErr BGPlatformInit(void)
{
    return noErr;
}

#pragma weak BGPlatformCleanup
void BGPlatformCleanup(void)
{
}

#pragma weak BGPlatformRegisterTask
OSErr BGPlatformRegisterTask(BackgroundTaskPtr taskPtr)
{
    return noErr;
}

#pragma weak BGPlatformUnregisterTask
OSErr BGPlatformUnregisterTask(UInt32 taskID)
{
    return noErr;
}

#pragma weak BGPlatformUpdateTaskState
OSErr BGPlatformUpdateTaskState(UInt32 taskID, BackgroundTaskState state)
{
    return noErr;
}
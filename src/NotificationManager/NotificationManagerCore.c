/*
 * NotificationManagerCore.c
 *
 * Core Notification Manager implementation for System 7.1
 * Provides background notifications and system alerts
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/NotificationManager.h"
#include "NotificationManager/NotificationQueue.h"
#include "NotificationManager/SystemAlerts.h"
#include "NotificationManager/ResponseHandling.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"
#include "Resources.h"
#include "Sound.h"

/* Global notification manager state */
static NMGlobals gNMGlobals = {0};
static Boolean gNMInitialized = false;

/* Internal function prototypes */
static OSErr NMInitialize(void);
static void NMCleanup(void);
static OSErr NMValidateAndPrepare(NMRecPtr nmReqPtr);
static void NMExecuteResponse(NMExtendedRecPtr nmExtPtr);
static Boolean NMShouldShowAlert(NMExtendedRecPtr nmExtPtr);

/*
 * NMInstall - Install a notification request
 *
 * This is the main entry point for posting notifications.
 * Compatible with original Mac OS NMInstall trap.
 */
pascal OSErr NMInstall(NMRecPtr nmReqPtr)
{
    OSErr err;
    NMExtendedRecPtr nmExtPtr;

    /* Validate parameters */
    if (!nmReqPtr) {
        return nmErrInvalidParameter;
    }

    /* Initialize if needed */
    if (!gNMInitialized) {
        err = NMInitialize();
        if (err != noErr) {
            return err;
        }
    }

    /* Validate the notification record */
    err = NMValidateAndPrepare(nmReqPtr);
    if (err != noErr) {
        return err;
    }

    /* Create extended record */
    nmExtPtr = (NMExtendedRecPtr)NewPtrClear(sizeof(NMExtendedRec));
    if (!nmExtPtr) {
        return nmErrOutOfMemory;
    }

    /* Copy base record */
    BlockMoveData(nmReqPtr, &nmExtPtr->base, sizeof(NMRec));

    /* Set default extended properties */
    nmExtPtr->priority = nmPriorityNormal;
    nmExtPtr->status = nmStatusPending;
    nmExtPtr->timestamp = NMGetTimestamp();
    nmExtPtr->timeout = gNMGlobals.nmDefaultTimeout;
    nmExtPtr->persistent = false;
    nmExtPtr->modal = false;
    nmExtPtr->groupID = 0;
    nmExtPtr->category = NULL;

    /* Update original record with extended pointer in private field */
    nmReqPtr->nmPrivate = (long)nmExtPtr;

    /* Add to queue */
    err = NMInsertInQueue(nmExtPtr);
    if (err != noErr) {
        DisposePtr((Ptr)nmExtPtr);
        return err;
    }

    /* Update status */
    nmExtPtr->status = nmStatusPosted;

    /* Trigger immediate processing if high priority */
    if (nmExtPtr->priority >= nmPriorityHigh) {
        NMProcessQueue();
    }

    return noErr;
}

/*
 * NMRemove - Remove a notification request
 *
 * Compatible with original Mac OS NMRemove trap.
 */
pascal OSErr NMRemove(NMRecPtr nmReqPtr)
{
    OSErr err;
    NMExtendedRecPtr nmExtPtr;

    /* Validate parameters */
    if (!nmReqPtr) {
        return nmErrInvalidParameter;
    }

    if (!gNMInitialized) {
        return nmErrNotInstalled;
    }

    /* Get extended record */
    nmExtPtr = (NMExtendedRecPtr)nmReqPtr->nmPrivate;
    if (!nmExtPtr) {
        return nmErrNotFound;
    }

    /* Remove from queue */
    err = NMRemoveFromQueue(nmExtPtr);
    if (err != noErr) {
        return err;
    }

    /* Remove from platform system if posted */
    if (nmExtPtr->status >= nmStatusDisplayed) {
        NMPlatformRemoveNotification(nmExtPtr);
    }

    /* Update status */
    nmExtPtr->status = nmStatusRemoved;

    /* Clean up extended record */
    if (nmExtPtr->category) {
        DisposePtr((Ptr)nmExtPtr->category);
    }

    if (nmExtPtr->richContent) {
        DisposeHandle(nmExtPtr->richContent);
    }

    DisposePtr((Ptr)nmExtPtr);

    /* Clear private field */
    nmReqPtr->nmPrivate = 0;

    return noErr;
}

/*
 * Extended API Functions
 */

OSErr NMInstallExtended(NMExtendedRecPtr nmExtPtr)
{
    OSErr err;

    if (!nmExtPtr) {
        return nmErrInvalidParameter;
    }

    /* Initialize if needed */
    if (!gNMInitialized) {
        err = NMInitialize();
        if (err != noErr) {
            return err;
        }
    }

    /* Validate the base record */
    err = NMValidateAndPrepare(&nmExtPtr->base);
    if (err != noErr) {
        return err;
    }

    /* Set status and timestamp */
    nmExtPtr->status = nmStatusPending;
    nmExtPtr->timestamp = NMGetTimestamp();

    /* Link extended record */
    nmExtPtr->base.nmPrivate = (long)nmExtPtr;

    /* Add to queue */
    err = NMInsertInQueue(nmExtPtr);
    if (err != noErr) {
        return err;
    }

    nmExtPtr->status = nmStatusPosted;

    /* Process immediately if high priority */
    if (nmExtPtr->priority >= nmPriorityHigh) {
        NMProcessQueue();
    }

    return noErr;
}

OSErr NMRemoveExtended(NMExtendedRecPtr nmExtPtr)
{
    if (!nmExtPtr) {
        return nmErrInvalidParameter;
    }

    return NMRemove(&nmExtPtr->base);
}

/*
 * Configuration Functions
 */

OSErr NMSetEnabled(Boolean enabled)
{
    if (!gNMInitialized) {
        return nmErrNotInstalled;
    }

    gNMGlobals.nmActive = enabled;

    if (!enabled) {
        /* Flush all pending notifications */
        NMFlushQueue();
    }

    return noErr;
}

Boolean NMIsEnabled(void)
{
    return gNMInitialized && gNMGlobals.nmActive;
}

OSErr NMSetSoundsEnabled(Boolean enabled)
{
    if (!gNMInitialized) {
        return nmErrNotInstalled;
    }

    gNMGlobals.nmSoundsEnabled = enabled;
    return noErr;
}

Boolean NMSoundsEnabled(void)
{
    return gNMInitialized && gNMGlobals.nmSoundsEnabled;
}

OSErr NMSetAlertsEnabled(Boolean enabled)
{
    if (!gNMInitialized) {
        return nmErrNotInstalled;
    }

    gNMGlobals.nmAlertsEnabled = enabled;
    return noErr;
}

Boolean NMAlertsEnabled(void)
{
    return gNMInitialized && gNMGlobals.nmAlertsEnabled;
}

/*
 * Status Functions
 */

OSErr NMGetStatus(NMRecPtr nmReqPtr, NMStatus *status)
{
    NMExtendedRecPtr nmExtPtr;

    if (!nmReqPtr || !status) {
        return nmErrInvalidParameter;
    }

    nmExtPtr = (NMExtendedRecPtr)nmReqPtr->nmPrivate;
    if (!nmExtPtr) {
        *status = nmStatusRemoved;
        return nmErrNotFound;
    }

    *status = nmExtPtr->status;
    return noErr;
}

Boolean NMIsPending(NMRecPtr nmReqPtr)
{
    NMStatus status;
    OSErr err = NMGetStatus(nmReqPtr, &status);
    return (err == noErr) && (status == nmStatusPending || status == nmStatusPosted);
}

Boolean NMIsDisplayed(NMRecPtr nmReqPtr)
{
    NMStatus status;
    OSErr err = NMGetStatus(nmReqPtr, &status);
    return (err == noErr) && (status == nmStatusDisplayed);
}

/*
 * Internal Implementation Functions
 */

static OSErr NMInitialize(void)
{
    OSErr err;

    if (gNMInitialized) {
        return noErr;
    }

    /* Initialize globals */
    gNMGlobals.nmQueue.qFlags = 0;
    gNMGlobals.nmQueue.qHead = NULL;
    gNMGlobals.nmQueue.qTail = NULL;

    gNMGlobals.nmActive = true;
    gNMGlobals.nmInAlert = false;
    gNMGlobals.nmNextID = 1;
    gNMGlobals.nmLastCheck = NMGetTimestamp();
    gNMGlobals.nmCheckInterval = 30; /* 30 ticks = 0.5 seconds */
    gNMGlobals.nmCurrentAlert = NULL;
    gNMGlobals.nmMaxQueueSize = 50;
    gNMGlobals.nmCurrentSize = 0;
    gNMGlobals.nmSoundsEnabled = true;
    gNMGlobals.nmAlertsEnabled = true;
    gNMGlobals.nmDefaultTimeout = 300; /* 5 seconds */
    gNMGlobals.platformData = NULL;

    /* Initialize platform-specific components */
    err = NMPlatformInit();
    if (err != noErr) {
        return err;
    }

    /* Initialize queue management */
    err = NMQueueInit();
    if (err != noErr) {
        NMPlatformCleanup();
        return err;
    }

    /* Initialize system alerts */
    err = NMSystemAlertsInit();
    if (err != noErr) {
        NMQueueCleanup();
        NMPlatformCleanup();
        return err;
    }

    /* Initialize response handling */
    err = NMResponseHandlingInit();
    if (err != noErr) {
        NMSystemAlertsCleanup();
        NMQueueCleanup();
        NMPlatformCleanup();
        return err;
    }

    /* Load default resources */
    NMLoadNotificationResources();

    gNMInitialized = true;
    return noErr;
}

static void NMCleanup(void)
{
    if (!gNMInitialized) {
        return;
    }

    /* Flush all notifications */
    NMFlushQueue();

    /* Cleanup components */
    NMResponseHandlingCleanup();
    NMSystemAlertsCleanup();
    NMQueueCleanup();
    NMPlatformCleanup();

    /* Unload resources */
    NMUnloadNotificationResources();

    gNMInitialized = false;
}

static OSErr NMValidateAndPrepare(NMRecPtr nmReqPtr)
{
    /* Validate required fields */
    if (nmReqPtr->qType != nmType) {
        nmReqPtr->qType = nmType;
    }

    /* Ensure valid queue linkage */
    nmReqPtr->qLink = NULL;

    /* Validate string pointer */
    if (nmReqPtr->nmStr) {
        if (nmReqPtr->nmStr[0] > nmMaxStrLen) {
            return nmErrInvalidParameter;
        }
    }

    /* Validate icon handle */
    if (nmReqPtr->nmIcon) {
        if (GetHandleSize(nmReqPtr->nmIcon) <= 0) {
            return nmErrInvalidParameter;
        }
    }

    /* Validate sound handle */
    if (nmReqPtr->nmSound) {
        if (GetHandleSize(nmReqPtr->nmSound) <= 0) {
            return nmErrInvalidParameter;
        }
    }

    return noErr;
}

/*
 * Queue Processing
 */

void NMProcessQueue(void)
{
    NMExtendedRecPtr nmExtPtr;
    QElemPtr qElem;

    if (!gNMInitialized || !gNMGlobals.nmActive) {
        return;
    }

    /* Don't process if currently showing an alert */
    if (gNMGlobals.nmInAlert) {
        return;
    }

    /* Check timeouts first */
    NMCheckTimeouts();

    /* Process pending notifications by priority */
    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nmExtPtr = (NMExtendedRecPtr)qElem;

        if (nmExtPtr->status == nmStatusPosted) {
            /* Should we show an alert for this notification? */
            if (NMShouldShowAlert(nmExtPtr)) {
                gNMGlobals.nmInAlert = true;
                gNMGlobals.nmCurrentAlert = nmExtPtr;
                nmExtPtr->status = nmStatusDisplayed;

                /* Show the alert */
                NMShowSystemAlert(nmExtPtr);
                break; /* Only one alert at a time */
            }

            /* Play sound if requested */
            if (nmExtPtr->base.nmSound && gNMGlobals.nmSoundsEnabled) {
                SndPlay(NULL, nmExtPtr->base.nmSound, false);
            }

            /* Add to Apple menu if requested */
            if (nmExtPtr->base.nmMark && nmExtPtr->base.nmIcon) {
                /* Apple menu integration would go here */
            }

            /* Post to native system */
            NMPlatformPostNotification(nmExtPtr);

            /* Mark as displayed */
            nmExtPtr->status = nmStatusDisplayed;
        }

        qElem = qElem->qLink;
    }

    gNMGlobals.nmLastCheck = NMGetTimestamp();
}

void NMCheckTimeouts(void)
{
    NMExtendedRecPtr nmExtPtr;
    QElemPtr qElem, nextElem;
    UInt32 currentTime;

    if (!gNMInitialized) {
        return;
    }

    currentTime = NMGetTimestamp();

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        /* Check if notification has timed out */
        if (nmExtPtr->timeout > 0 &&
            (currentTime - nmExtPtr->timestamp) > nmExtPtr->timeout) {

            /* Remove timed out notification */
            NMRemoveFromQueue(nmExtPtr);

            /* Execute timeout callback if present */
            if (nmExtPtr->modernCallback) {
                nmExtPtr->modernCallback(&nmExtPtr->base, nmExtPtr->callbackContext);
            }

            /* Clean up */
            nmExtPtr->status = nmStatusRemoved;
            if (!nmExtPtr->persistent) {
                DisposePtr((Ptr)nmExtPtr);
            }
        }

        qElem = nextElem;
    }
}

static Boolean NMShouldShowAlert(NMExtendedRecPtr nmExtPtr)
{
    /* Show alert if we have a string and alerts are enabled */
    if (!gNMGlobals.nmAlertsEnabled) {
        return false;
    }

    if (!nmExtPtr->base.nmStr) {
        return false;
    }

    /* Don't show if already showing an alert */
    if (gNMGlobals.nmInAlert) {
        return false;
    }

    /* Show for high priority or modal notifications */
    if (nmExtPtr->priority >= nmPriorityHigh || nmExtPtr->modal) {
        return true;
    }

    /* Show for normal priority if no other alerts pending */
    return true;
}

/*
 * Utility Functions
 */

UInt32 NMGetTimestamp(void)
{
    UInt32 ticks;
    Microseconds((UnsignedWide *)&ticks);
    return ticks;
}

short NMGenerateID(void)
{
    if (!gNMInitialized) {
        return 0;
    }

    return gNMGlobals.nmNextID++;
}

OSErr NMValidateRecord(NMRecPtr nmReqPtr)
{
    if (!nmReqPtr) {
        return nmErrInvalidParameter;
    }

    return NMValidateAndPrepare(nmReqPtr);
}

/*
 * Cleanup on termination
 */
void NMTerminate(void)
{
    NMCleanup();
}
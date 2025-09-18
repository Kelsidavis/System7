/*
 * SystemAlerts.c
 *
 * System alert display and user interaction handling
 * Manages modal and non-modal alerts for notifications
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/SystemAlerts.h"
#include "NotificationManager/NotificationManager.h"
#include "NotificationManager/ResponseHandling.h"
#include "Dialogs.h"
#include "Controls.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"
#include "Resources.h"
#include "Sound.h"
#include "TextUtils.h"

/* Global alert manager state */
static AlertManagerState gAlertState = {0};

/* Internal function prototypes */
static OSErr NMInitializeAlertManager(void);
static void NMCleanupAlertManager(void);
static AlertInstancePtr NMCreateAlertFromNotification(NMExtendedRecPtr nmExtPtr);
static OSErr NMConfigureAlertForNotification(AlertConfigPtr config, NMExtendedRecPtr nmExtPtr);
static OSErr NMCreateAlertDialog(AlertInstancePtr alertPtr);
static void NMDestroyAlertDialog(AlertInstancePtr alertPtr);
static void NMDrawAlertContent(AlertInstancePtr alertPtr);
static Boolean NMCheckAlertTimeout(AlertInstancePtr alertPtr);
static OSErr NMPositionNewAlert(AlertInstancePtr alertPtr);
static void NMPlayAlertSound(Handle soundHandle);
static OSErr NMCreateDefaultAlertConfig(AlertConfigPtr *config, AlertType type, StringPtr message);

/*
 * System Alert Initialization
 */

OSErr NMSystemAlertsInit(void)
{
    OSErr err;

    if (gAlertState.initialized) {
        return noErr;
    }

    /* Initialize alert manager state */
    gAlertState.enabled = true;
    gAlertState.soundEnabled = true;
    gAlertState.maxConcurrentAlerts = ALERT_MAX_CONCURRENT;
    gAlertState.currentAlertCount = 0;
    gAlertState.alertChain = NULL;
    gAlertState.modalAlert = NULL;
    gAlertState.nextAlertID = 1;
    gAlertState.autoCenter = true;
    gAlertState.alertSpacing = ALERT_CASCADE_OFFSET;

    /* Set default position to center */
    gAlertState.defaultPosition.h = 0;
    gAlertState.defaultPosition.v = 0;

    /* Load alert resources */
    err = NMLoadAlertResources();
    if (err != noErr) {
        return err;
    }

    /* Initialize platform-specific components */
    /* Platform-specific initialization would go here */

    gAlertState.initialized = true;
    return noErr;
}

void NMSystemAlertsCleanup(void)
{
    if (!gAlertState.initialized) {
        return;
    }

    /* Dismiss all active alerts */
    NMDismissAllAlerts();

    /* Unload resources */
    NMUnloadAlertResources();

    /* Cleanup platform components */
    /* Platform-specific cleanup would go here */

    gAlertState.initialized = false;
}

/*
 * Alert Display Functions
 */

OSErr NMShowSystemAlert(NMExtendedRecPtr nmExtPtr)
{
    AlertInstancePtr alertPtr;
    OSErr err;

    if (!gAlertState.initialized) {
        return alertErrNotInitialized;
    }

    if (!nmExtPtr) {
        return nmErrInvalidParameter;
    }

    if (!gAlertState.enabled) {
        return noErr; /* Silently ignore if alerts disabled */
    }

    /* Check if we can show another alert */
    if (gAlertState.currentAlertCount >= gAlertState.maxConcurrentAlerts) {
        return alertErrTooManyAlerts;
    }

    /* Check for modal restriction */
    if (nmExtPtr->modal && gAlertState.modalAlert) {
        return alertErrModalActive;
    }

    /* Create alert instance from notification */
    alertPtr = NMCreateAlertFromNotification(nmExtPtr);
    if (!alertPtr) {
        return nmErrOutOfMemory;
    }

    /* Create the dialog */
    err = NMCreateAlertDialog(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    /* Position the alert */
    err = NMPositionNewAlert(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    /* Add to alert chain */
    err = NMAddToAlertChain(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    /* Play sound if requested */
    if (alertPtr->config.sound && gAlertState.soundEnabled) {
        NMPlayAlertSound(alertPtr->config.sound);
    }

    /* Show the alert */
    ShowWindow(alertPtr->dialog);
    alertPtr->isVisible = true;
    alertPtr->showTime = NMGetTimestamp();

    /* Set timeout if specified */
    if (alertPtr->config.hasTimeout) {
        alertPtr->timeoutTime = alertPtr->showTime + alertPtr->config.timeout;
    }

    /* Set as modal if requested */
    if (alertPtr->config.modal) {
        gAlertState.modalAlert = alertPtr;
        alertPtr->isModal = true;
    }

    /* Update notification status */
    nmExtPtr->status = nmStatusDisplayed;

    gAlertState.currentAlertCount++;
    return noErr;
}

OSErr NMShowAlert(AlertConfigPtr config, AlertResponse *response)
{
    AlertInstancePtr alertPtr;
    OSErr err;
    Boolean done = false;
    EventRecord event;

    if (!gAlertState.initialized) {
        return alertErrNotInitialized;
    }

    if (!config || !response) {
        return nmErrInvalidParameter;
    }

    /* Create alert instance */
    alertPtr = NMCreateAlertInstance(config);
    if (!alertPtr) {
        return nmErrOutOfMemory;
    }

    /* Create and show dialog */
    err = NMCreateAlertDialog(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    err = NMPositionNewAlert(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    ShowWindow(alertPtr->dialog);
    alertPtr->isVisible = true;
    alertPtr->showTime = NMGetTimestamp();

    /* Play sound */
    if (config->sound && gAlertState.soundEnabled) {
        NMPlayAlertSound(config->sound);
    }

    /* Modal event loop */
    while (!done) {
        if (WaitNextEvent(everyEvent, &event, 0, NULL)) {
            if (NMHandleAlertEvent(&event)) {
                /* Event was handled by alert system */
                if (alertPtr->responded) {
                    done = true;
                }
            } else {
                /* Handle other events */
                switch (event.what) {
                    case mouseDown:
                    case keyDown:
                    case autoKey:
                        /* Check if event is for our dialog */
                        if (IsDialogEvent(&event)) {
                            DialogPtr dialog;
                            short itemHit;

                            if (DialogSelect(&event, &dialog, &itemHit)) {
                                if (dialog == alertPtr->dialog) {
                                    /* Handle button click */
                                    alertPtr->response = (AlertResponse)itemHit;
                                    alertPtr->responded = true;
                                    done = true;
                                }
                            }
                        }
                        break;

                    case updateEvt:
                        if ((WindowPtr)event.message == alertPtr->dialog) {
                            BeginUpdate(alertPtr->dialog);
                            NMDrawAlertContent(alertPtr);
                            EndUpdate(alertPtr->dialog);
                        }
                        break;
                }
            }
        }

        /* Check timeout */
        if (alertPtr->config.hasTimeout && NMCheckAlertTimeout(alertPtr)) {
            alertPtr->response = alertResponseTimeout;
            alertPtr->responded = true;
            done = true;
        }
    }

    *response = alertPtr->response;

    /* Clean up */
    HideWindow(alertPtr->dialog);
    NMDestroyAlertInstance(alertPtr);

    return noErr;
}

OSErr NMShowAlertAsync(AlertConfigPtr config, AlertResponseProc responseProc, void *context)
{
    AlertInstancePtr alertPtr;
    OSErr err;

    if (!gAlertState.initialized) {
        return alertErrNotInitialized;
    }

    if (!config) {
        return nmErrInvalidParameter;
    }

    /* Create alert instance */
    alertPtr = NMCreateAlertInstance(config);
    if (!alertPtr) {
        return nmErrOutOfMemory;
    }

    /* Store callback information */
    /* This would require extending the AlertInstance structure */

    /* Create and show dialog */
    err = NMCreateAlertDialog(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    err = NMPositionNewAlert(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    err = NMAddToAlertChain(alertPtr);
    if (err != noErr) {
        NMDestroyAlertInstance(alertPtr);
        return err;
    }

    ShowWindow(alertPtr->dialog);
    alertPtr->isVisible = true;
    alertPtr->showTime = NMGetTimestamp();

    if (config->sound && gAlertState.soundEnabled) {
        NMPlayAlertSound(config->sound);
    }

    gAlertState.currentAlertCount++;
    return noErr;
}

/*
 * Alert Control Functions
 */

OSErr NMDismissAlert(AlertInstancePtr alertPtr)
{
    if (!gAlertState.initialized) {
        return alertErrNotInitialized;
    }

    if (!alertPtr) {
        return nmErrInvalidParameter;
    }

    /* Hide the alert */
    if (alertPtr->isVisible) {
        HideWindow(alertPtr->dialog);
        alertPtr->isVisible = false;
    }

    /* Remove from modal state */
    if (alertPtr->isModal && gAlertState.modalAlert == alertPtr) {
        gAlertState.modalAlert = NULL;
        alertPtr->isModal = false;
    }

    /* Remove from chain */
    NMRemoveFromAlertChain(alertPtr);

    /* Update notification if associated */
    if (alertPtr->notification) {
        alertPtr->notification->status = nmStatusRemoved;
    }

    /* Clean up */
    NMDestroyAlertInstance(alertPtr);

    gAlertState.currentAlertCount--;
    return noErr;
}

OSErr NMDismissAllAlerts(void)
{
    AlertInstancePtr alertPtr, nextPtr;

    if (!gAlertState.initialized) {
        return alertErrNotInitialized;
    }

    alertPtr = gAlertState.alertChain;
    while (alertPtr) {
        nextPtr = alertPtr->next;
        NMDismissAlert(alertPtr);
        alertPtr = nextPtr;
    }

    gAlertState.alertChain = NULL;
    gAlertState.modalAlert = NULL;
    gAlertState.currentAlertCount = 0;

    return noErr;
}

/*
 * Alert Processing
 */

void NMProcessAlerts(void)
{
    AlertInstancePtr alertPtr, nextPtr;

    if (!gAlertState.initialized || !gAlertState.enabled) {
        return;
    }

    /* Check timeouts */
    NMCheckAlertTimeouts();

    /* Update alert display */
    alertPtr = gAlertState.alertChain;
    while (alertPtr) {
        nextPtr = alertPtr->next;

        if (alertPtr->isVisible) {
            NMUpdateAlertDisplay(alertPtr);
        }

        alertPtr = nextPtr;
    }
}

void NMCheckAlertTimeouts(void)
{
    AlertInstancePtr alertPtr, nextPtr;
    UInt32 currentTime;

    if (!gAlertState.initialized) {
        return;
    }

    currentTime = NMGetTimestamp();

    alertPtr = gAlertState.alertChain;
    while (alertPtr) {
        nextPtr = alertPtr->next;

        if (alertPtr->config.hasTimeout && currentTime >= alertPtr->timeoutTime) {
            alertPtr->response = alertResponseTimeout;
            alertPtr->responded = true;

            /* Trigger response handling */
            if (alertPtr->notification) {
                NMTriggerResponse(alertPtr->notification, alertResponseTimeout);
            }

            NMDismissAlert(alertPtr);
        }

        alertPtr = nextPtr;
    }
}

Boolean NMHandleAlertEvent(EventRecord *event)
{
    AlertInstancePtr alertPtr;
    DialogPtr dialog;
    short itemHit;

    if (!gAlertState.initialized || !event) {
        return false;
    }

    /* Check if this is a dialog event */
    if (!IsDialogEvent(event)) {
        return false;
    }

    /* Handle dialog events */
    if (DialogSelect(event, &dialog, &itemHit)) {
        /* Find the alert instance for this dialog */
        alertPtr = gAlertState.alertChain;
        while (alertPtr) {
            if (alertPtr->dialog == dialog) {
                /* Handle the button click */
                alertPtr->response = (AlertResponse)itemHit;
                alertPtr->responded = true;

                /* Trigger response handling */
                if (alertPtr->notification) {
                    NMTriggerResponse(alertPtr->notification, alertPtr->response);
                }

                /* Dismiss the alert */
                NMDismissAlert(alertPtr);
                return true;
            }
            alertPtr = alertPtr->next;
        }
    }

    return false;
}

/*
 * Alert Utilities
 */

OSErr NMCreateSimpleAlert(AlertType type, StringPtr message, AlertResponse *response)
{
    AlertConfigPtr config;
    OSErr err;

    err = NMCreateDefaultAlertConfig(&config, type, message);
    if (err != noErr) {
        return err;
    }

    err = NMShowAlert(config, response);

    NMDisposeAlertConfig(config);
    return err;
}

OSErr NMCreateConfirmAlert(StringPtr message, Boolean *confirmed)
{
    AlertConfigPtr config;
    AlertResponse response;
    OSErr err;

    err = NMCreateDefaultAlertConfig(&config, alertTypeCaution, message);
    if (err != noErr) {
        return err;
    }

    config->buttonType = alertButtonYesNo;
    config->defaultButton = 1; /* Yes */
    config->cancelButton = 2;  /* No */

    err = NMShowAlert(config, &response);
    if (err == noErr) {
        *confirmed = (response == alertResponseYes);
    }

    NMDisposeAlertConfig(config);
    return err;
}

OSErr NMCreateErrorAlert(OSErr errorCode, StringPtr message)
{
    AlertConfigPtr config;
    AlertResponse response;
    OSErr err;
    Str255 errorMsg;

    /* Create error message */
    if (message) {
        BlockMoveData(message, errorMsg, message[0] + 1);
    } else {
        sprintf((char *)&errorMsg[1], "An error occurred (error %d)", errorCode);
        errorMsg[0] = strlen((char *)&errorMsg[1]);
    }

    err = NMCreateDefaultAlertConfig(&config, alertTypeStop, errorMsg);
    if (err != noErr) {
        return err;
    }

    err = NMShowAlert(config, &response);

    NMDisposeAlertConfig(config);
    return err;
}

/*
 * Internal Implementation
 */

static AlertInstancePtr NMCreateAlertFromNotification(NMExtendedRecPtr nmExtPtr)
{
    AlertInstancePtr alertPtr;
    AlertConfigPtr config;
    OSErr err;

    /* Allocate alert instance */
    alertPtr = (AlertInstancePtr)NewPtrClear(sizeof(AlertInstance));
    if (!alertPtr) {
        return NULL;
    }

    /* Allocate configuration */
    config = (AlertConfigPtr)NewPtrClear(sizeof(AlertConfig));
    if (!config) {
        DisposePtr((Ptr)alertPtr);
        return NULL;
    }

    /* Configure alert from notification */
    err = NMConfigureAlertForNotification(config, nmExtPtr);
    if (err != noErr) {
        DisposePtr((Ptr)config);
        DisposePtr((Ptr)alertPtr);
        return NULL;
    }

    /* Initialize alert instance */
    alertPtr->config = *config;
    alertPtr->dialog = NULL;
    alertPtr->isVisible = false;
    alertPtr->isModal = nmExtPtr->modal;
    alertPtr->showTime = 0;
    alertPtr->timeoutTime = 0;
    alertPtr->response = alertResponseOK;
    alertPtr->responded = false;
    alertPtr->notification = nmExtPtr;
    alertPtr->platformData = NULL;
    alertPtr->next = NULL;

    DisposePtr((Ptr)config);
    return alertPtr;
}

static OSErr NMConfigureAlertForNotification(AlertConfigPtr config, NMExtendedRecPtr nmExtPtr)
{
    /* Set basic configuration */
    config->type = alertTypeNote;
    config->buttonType = alertButtonOK;
    config->title = "\pNotification";
    config->message = nmExtPtr->base.nmStr;
    config->detailText = NULL;
    config->icon = nmExtPtr->base.nmIcon;
    config->sound = nmExtPtr->base.nmSound;
    config->modal = nmExtPtr->modal;
    config->movable = true;
    config->hasTimeout = (nmExtPtr->timeout > 0);
    config->timeout = nmExtPtr->timeout;
    config->position = gAlertState.defaultPosition;
    config->defaultButton = 1;
    config->cancelButton = 1;
    config->customButtonCount = 0;
    config->refCon = nmExtPtr->base.nmRefCon;

    /* Adjust alert type based on priority */
    switch (nmExtPtr->priority) {
        case nmPriorityLow:
            config->type = alertTypeNote;
            break;
        case nmPriorityNormal:
            config->type = alertTypeNote;
            break;
        case nmPriorityHigh:
            config->type = alertTypeCaution;
            break;
        case nmPriorityUrgent:
            config->type = alertTypeStop;
            config->modal = true;
            break;
    }

    return noErr;
}

static OSErr NMCreateAlertDialog(AlertInstancePtr alertPtr)
{
    Rect bounds;
    DialogPtr dialog;
    short alertID;

    /* Calculate dialog bounds */
    NMCalculateAlertSize(&alertPtr->config, &bounds);

    /* Determine alert dialog ID based on type */
    switch (alertPtr->config.type) {
        case alertTypeNote:
            alertID = 128; /* Note alert template */
            break;
        case alertTypeCaution:
            alertID = 129; /* Caution alert template */
            break;
        case alertTypeStop:
            alertID = 130; /* Stop alert template */
            break;
        default:
            alertID = 128;
            break;
    }

    /* Create dialog */
    dialog = GetNewDialog(alertID, NULL, (WindowPtr)-1);
    if (!dialog) {
        return resNotFound;
    }

    alertPtr->dialog = dialog;

    /* Set dialog title */
    if (alertPtr->config.title) {
        SetWTitle(dialog, alertPtr->config.title);
    }

    /* Update dialog content */
    NMDrawAlertContent(alertPtr);

    return noErr;
}

static void NMDestroyAlertDialog(AlertInstancePtr alertPtr)
{
    if (alertPtr && alertPtr->dialog) {
        DisposeDialog(alertPtr->dialog);
        alertPtr->dialog = NULL;
    }
}

static void NMDrawAlertContent(AlertInstancePtr alertPtr)
{
    GrafPtr savePort;
    Rect textRect;
    short fontNum, fontSize;

    if (!alertPtr || !alertPtr->dialog) {
        return;
    }

    GetPort(&savePort);
    SetPort(alertPtr->dialog);

    /* Draw alert text */
    if (alertPtr->config.message) {
        SetRect(&textRect, 60, 20, 340, 80);
        GetFNum("\pGeneva", &fontNum);
        TextFont(fontNum);
        TextSize(12);
        TextBox(alertPtr->config.message + 1, alertPtr->config.message[0], &textRect, teJustLeft);
    }

    /* Draw icon if present */
    if (alertPtr->config.icon) {
        Rect iconRect;
        SetRect(&iconRect, 20, 20, 52, 52);
        PlotIcon(&iconRect, alertPtr->config.icon);
    }

    SetPort(savePort);
}

static Boolean NMCheckAlertTimeout(AlertInstancePtr alertPtr)
{
    UInt32 currentTime;

    if (!alertPtr->config.hasTimeout) {
        return false;
    }

    currentTime = NMGetTimestamp();
    return (currentTime >= alertPtr->timeoutTime);
}

static OSErr NMPositionNewAlert(AlertInstancePtr alertPtr)
{
    Rect bounds, screenBounds;
    Point position;

    if (!alertPtr || !alertPtr->dialog) {
        return nmErrInvalidParameter;
    }

    /* Get current dialog bounds */
    bounds = alertPtr->dialog->portRect;

    /* Determine position */
    if (alertPtr->config.position.h == 0 && alertPtr->config.position.v == 0) {
        /* Center on screen */
        screenBounds = qd.screenBits.bounds;
        position.h = (screenBounds.right - screenBounds.left - (bounds.right - bounds.left)) / 2;
        position.v = (screenBounds.bottom - screenBounds.top - (bounds.bottom - bounds.top)) / 3;
    } else {
        position = alertPtr->config.position;
    }

    /* Move dialog to position */
    MoveWindow(alertPtr->dialog, position.h, position.v, false);

    return noErr;
}

static void NMPlayAlertSound(Handle soundHandle)
{
    if (soundHandle && gAlertState.soundEnabled) {
        SndPlay(NULL, soundHandle, false);
    } else {
        /* Play system beep */
        SysBeep(1);
    }
}

static OSErr NMCreateDefaultAlertConfig(AlertConfigPtr *config, AlertType type, StringPtr message)
{
    AlertConfigPtr newConfig;

    newConfig = (AlertConfigPtr)NewPtrClear(sizeof(AlertConfig));
    if (!newConfig) {
        return nmErrOutOfMemory;
    }

    newConfig->type = type;
    newConfig->buttonType = alertButtonOK;
    newConfig->title = "\pAlert";
    newConfig->message = message;
    newConfig->detailText = NULL;
    newConfig->icon = NMGetDefaultAlertIcon(type);
    newConfig->sound = NMGetDefaultAlertSound();
    newConfig->modal = true;
    newConfig->movable = true;
    newConfig->hasTimeout = false;
    newConfig->timeout = 0;
    newConfig->position.h = 0;
    newConfig->position.v = 0;
    newConfig->defaultButton = 1;
    newConfig->cancelButton = 1;
    newConfig->customButtonCount = 0;
    newConfig->refCon = 0;

    *config = newConfig;
    return noErr;
}

/*
 * Alert Instance Management
 */

AlertInstancePtr NMCreateAlertInstance(AlertConfigPtr config)
{
    AlertInstancePtr alertPtr;

    alertPtr = (AlertInstancePtr)NewPtrClear(sizeof(AlertInstance));
    if (!alertPtr) {
        return NULL;
    }

    alertPtr->config = *config;
    alertPtr->dialog = NULL;
    alertPtr->isVisible = false;
    alertPtr->isModal = config->modal;
    alertPtr->showTime = 0;
    alertPtr->timeoutTime = 0;
    alertPtr->response = alertResponseOK;
    alertPtr->responded = false;
    alertPtr->notification = NULL;
    alertPtr->platformData = NULL;
    alertPtr->next = NULL;

    return alertPtr;
}

void NMDestroyAlertInstance(AlertInstancePtr alertPtr)
{
    if (!alertPtr) {
        return;
    }

    /* Destroy dialog if present */
    NMDestroyAlertDialog(alertPtr);

    /* Clean up platform data */
    if (alertPtr->platformData) {
        /* Platform-specific cleanup */
    }

    DisposePtr((Ptr)alertPtr);
}

OSErr NMAddToAlertChain(AlertInstancePtr alertPtr)
{
    if (!alertPtr) {
        return nmErrInvalidParameter;
    }

    /* Add to front of chain */
    alertPtr->next = gAlertState.alertChain;
    gAlertState.alertChain = alertPtr;

    return noErr;
}

OSErr NMRemoveFromAlertChain(AlertInstancePtr alertPtr)
{
    AlertInstancePtr current, previous = NULL;

    if (!alertPtr) {
        return nmErrInvalidParameter;
    }

    current = gAlertState.alertChain;
    while (current) {
        if (current == alertPtr) {
            if (previous) {
                previous->next = current->next;
            } else {
                gAlertState.alertChain = current->next;
            }
            current->next = NULL;
            return noErr;
        }
        previous = current;
        current = current->next;
    }

    return alertErrAlertNotFound;
}

/*
 * Weak stubs for platform-specific functions
 */

#pragma weak NMLoadAlertResources
OSErr NMLoadAlertResources(void)
{
    return noErr;
}

#pragma weak NMUnloadAlertResources
OSErr NMUnloadAlertResources(void)
{
    return noErr;
}

#pragma weak NMGetDefaultAlertIcon
Handle NMGetDefaultAlertIcon(AlertType type)
{
    return NULL;
}

#pragma weak NMGetDefaultAlertSound
Handle NMGetDefaultAlertSound(void)
{
    return NULL;
}

#pragma weak NMCalculateAlertSize
OSErr NMCalculateAlertSize(AlertConfigPtr config, Rect *bounds)
{
    SetRect(bounds, 0, 0, 360, 120);
    return noErr;
}

#pragma weak NMUpdateAlertDisplay
OSErr NMUpdateAlertDisplay(AlertInstancePtr alertPtr)
{
    return noErr;
}
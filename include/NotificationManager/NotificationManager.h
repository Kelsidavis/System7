#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include "Types.h"
#include "OSUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Notification Manager Constants */
#define nmType                  8           /* Queue type for notification records */
#define nmMaxStrLen             255         /* Maximum string length for notifications */
#define nmMaxIconSize           32          /* Maximum icon size in pixels */
#define nmDefaultMark           0           /* Default Apple menu mark */

/* Notification Flags */
#define nmFlagReserved          0x0000      /* Reserved flags */
#define nmFlagSound             0x0001      /* Play sound when posting */
#define nmFlagIcon              0x0002      /* Show icon in Apple menu */
#define nmFlagString            0x0004      /* Show string in alert */
#define nmFlagResponse          0x0008      /* Call response routine */

/* Notification Priorities */
typedef enum {
    nmPriorityLow = 0,
    nmPriorityNormal = 1,
    nmPriorityHigh = 2,
    nmPriorityUrgent = 3
} NMPriority;

/* Notification Status */
typedef enum {
    nmStatusPending = 0,
    nmStatusPosted = 1,
    nmStatusDisplayed = 2,
    nmStatusResponded = 3,
    nmStatusRemoved = 4
} NMStatus;

/* Forward declarations */
struct NMRec;
typedef struct NMRec NMRec;
typedef NMRec *NMRecPtr;

/* Notification response procedure */
typedef pascal void (*NMProcPtr)(NMRecPtr nmReqPtr);

/* Modern notification callback */
typedef void (*NotificationCallback)(NMRecPtr nmReqPtr, void *context);

/* Notification Record - exact match of original Mac OS structure */
struct NMRec {
    QElemPtr    qLink;          /* Next queue entry */
    short       qType;          /* Queue type - ORD(nmType) = 8 */
    short       nmFlags;        /* Notification flags */
    long        nmPrivate;      /* Reserved for system use */
    short       nmReserved;     /* Reserved */
    short       nmMark;         /* Item to mark in Apple menu */
    Handle      nmIcon;         /* Handle to small icon */
    Handle      nmSound;        /* Handle to sound record */
    StringPtr   nmStr;          /* String to appear in alert */
    NMProcPtr   nmResp;         /* Pointer to response routine */
    long        nmRefCon;       /* For application use */
};

/* Extended notification record with modern features */
typedef struct {
    NMRec           base;           /* Original notification record */
    NMPriority      priority;       /* Notification priority */
    NMStatus        status;         /* Current status */
    UInt32          timestamp;      /* When notification was posted */
    UInt32          timeout;        /* Timeout in ticks (0 = no timeout) */
    Handle          richContent;    /* Rich content data */
    NotificationCallback modernCallback;  /* Modern callback */
    void           *callbackContext;      /* Callback context */
    Boolean         persistent;     /* Keep until explicitly removed */
    Boolean         modal;          /* Show as modal alert */
    short           groupID;        /* Notification group identifier */
    StringPtr       category;       /* Notification category */
} NMExtendedRec, *NMExtendedRecPtr;

/* Notification Manager Globals */
typedef struct {
    QHdr            nmQueue;            /* Notification queue header */
    Boolean         nmActive;           /* Notification Manager active */
    Boolean         nmInAlert;          /* Currently showing alert */
    short           nmNextID;           /* Next notification ID */
    UInt32          nmLastCheck;        /* Last queue check time */
    UInt32          nmCheckInterval;    /* Queue check interval in ticks */
    NMExtendedRecPtr nmCurrentAlert;    /* Currently displayed alert */
    Handle          nmQueueHandle;      /* Handle to queue storage */
    short           nmMaxQueueSize;     /* Maximum queue size */
    short           nmCurrentSize;      /* Current queue size */
    Boolean         nmSoundsEnabled;    /* Sound notifications enabled */
    Boolean         nmAlertsEnabled;    /* Alert notifications enabled */
    short           nmDefaultTimeout;   /* Default timeout in ticks */
    void           *platformData;       /* Platform-specific data */
} NMGlobals, *NMGlobalsPtr;

/* Platform-specific notification data */
typedef struct {
    void    *nativeHandle;      /* Native notification handle */
    int     platformID;         /* Platform-specific ID */
    Boolean delivered;          /* Notification delivered to system */
    Boolean userInteracted;     /* User interacted with notification */
    void    *systemContext;     /* System notification context */
} PlatformNotificationData;

/* Core Notification Manager API - original Mac OS functions */
OSErr NMInstall(NMRecPtr nmReqPtr);
OSErr NMRemove(NMRecPtr nmReqPtr);

/* Extended Notification Manager API */
OSErr NMInstallExtended(NMExtendedRecPtr nmExtPtr);
OSErr NMRemoveExtended(NMExtendedRecPtr nmExtPtr);
OSErr NMSetPriority(NMRecPtr nmReqPtr, NMPriority priority);
OSErr NMSetTimeout(NMRecPtr nmReqPtr, UInt32 timeout);
OSErr NMSetCategory(NMRecPtr nmReqPtr, StringPtr category);

/* Queue Management */
OSErr NMGetQueueStatus(short *count, short *maxSize);
OSErr NMFlushQueue(void);
OSErr NMFlushCategory(StringPtr category);
OSErr NMFlushApplication(OSType appSignature);

/* Notification Status */
OSErr NMGetStatus(NMRecPtr nmReqPtr, NMStatus *status);
Boolean NMIsPending(NMRecPtr nmReqPtr);
Boolean NMIsDisplayed(NMRecPtr nmReqPtr);

/* Configuration */
OSErr NMSetEnabled(Boolean enabled);
Boolean NMIsEnabled(void);
OSErr NMSetSoundsEnabled(Boolean enabled);
Boolean NMSoundsEnabled(void);
OSErr NMSetAlertsEnabled(Boolean enabled);
Boolean NMAlertsEnabled(void);

/* Modern Platform Integration */
OSErr NMRegisterPlatformCallback(NotificationCallback callback, void *context);
OSErr NMUnregisterPlatformCallback(void);
OSErr NMPostToNativeSystem(NMExtendedRecPtr nmExtPtr);
OSErr NMRemoveFromNativeSystem(NMExtendedRecPtr nmExtPtr);

/* Resource Management */
OSErr NMLoadNotificationResources(void);
OSErr NMUnloadNotificationResources(void);
Handle NMGetDefaultIcon(void);
Handle NMGetDefaultSound(void);

/* Utility Functions */
OSErr NMCopyString(StringPtr source, StringPtr dest, short maxLen);
OSErr NMValidateRecord(NMRecPtr nmReqPtr);
UInt32 NMGetTimestamp(void);
short NMGenerateID(void);

/* Platform Abstraction */
OSErr NMPlatformInit(void);
void NMPlatformCleanup(void);
OSErr NMPlatformPostNotification(NMExtendedRecPtr nmExtPtr);
OSErr NMPlatformRemoveNotification(NMExtendedRecPtr nmExtPtr);
OSErr NMPlatformUpdateNotification(NMExtendedRecPtr nmExtPtr);

/* Internal Functions */
void NMProcessQueue(void);
void NMCheckTimeouts(void);
void NMTriggerCallback(NMExtendedRecPtr nmExtPtr);
OSErr NMInsertInQueue(NMExtendedRecPtr nmExtPtr);
OSErr NMRemoveFromQueue(NMExtendedRecPtr nmExtPtr);

/* Error Codes */
#define nmErrNotInstalled       -40900      /* Notification Manager not installed */
#define nmErrInvalidRecord      -40901      /* Invalid notification record */
#define nmErrQueueFull          -40902      /* Notification queue is full */
#define nmErrNotFound           -40903      /* Notification not found */
#define nmErrInUse              -40904      /* Notification is in use */
#define nmErrPlatformFailure    -40905      /* Platform notification failed */
#define nmErrInvalidParameter   -40906      /* Invalid parameter */
#define nmErrOutOfMemory        -40907      /* Out of memory */
#define nmErrTimeout            -40908      /* Notification timed out */

#ifdef __cplusplus
}
#endif

#endif /* NOTIFICATION_MANAGER_H */
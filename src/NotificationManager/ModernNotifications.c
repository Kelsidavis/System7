/*
 * ModernNotifications.c
 *
 * Modern notification system abstraction layer
 * Provides integration with platform-specific notification systems
 *
 * This bridges the classic Mac OS Notification Manager with modern notification APIs
 */

#include "NotificationManager/ModernNotifications.h"
#include "NotificationManager/NotificationManager.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"

/* Modern notification system state */
typedef struct {
    Boolean                         initialized;
    Boolean                         permissionGranted;
    Boolean                         legacyMode;
    Boolean                         legacyFallback;
    PlatformNotificationType        activePlatform;
    PlatformNotificationInterfacePtr platformInterface;
    ModernNotificationFeatures      supportedFeatures;

    /* Callbacks */
    ModernNotificationDeliveredProc deliveredCallback;
    ModernNotificationResponseProc  responseCallback;
    ModernNotificationWillPresentProc willPresentCallback;

    /* Statistics */
    UInt32                          totalPosted;
    UInt32                          totalDelivered;
    UInt32                          totalFailed;

    /* Categories */
    StringPtr                       categories[MODERN_MAX_CATEGORIES];
    NotificationActionPtr           categoryActions[MODERN_MAX_CATEGORIES];
    short                           categoryCount;
} ModernNotificationState;

static ModernNotificationState gModernState = {0};

/* Internal function prototypes */
static OSErr NMDetectPlatform(PlatformNotificationType *platformType);
static OSErr NMInitializePlatform(PlatformNotificationType platformType);
static OSErr NMValidateModernNotification(ModernNotificationPtr notification);
static void NMUpdateModernStatistics(Boolean delivered, OSErr error);
static OSErr NMCreateDefaultPlatformInterface(PlatformNotificationType platformType, PlatformNotificationInterfacePtr *interface);

/*
 * Modern Notification Initialization
 */

OSErr NMModernInit(PlatformNotificationType platformType)
{
    OSErr err;

    if (gModernState.initialized) {
        return noErr;
    }

    /* Initialize state */
    gModernState.permissionGranted = false;
    gModernState.legacyMode = false;
    gModernState.legacyFallback = true;
    gModernState.activePlatform = platformNotificationNone;
    gModernState.platformInterface = NULL;
    gModernState.supportedFeatures = modernFeatureBasic;

    gModernState.deliveredCallback = NULL;
    gModernState.responseCallback = NULL;
    gModernState.willPresentCallback = NULL;

    gModernState.totalPosted = 0;
    gModernState.totalDelivered = 0;
    gModernState.totalFailed = 0;
    gModernState.categoryCount = 0;

    /* Auto-detect platform if not specified */
    if (platformType == platformNotificationNone) {
        err = NMDetectPlatform(&platformType);
        if (err != noErr) {
            platformType = platformNotificationNone;
        }
    }

    /* Initialize platform-specific interface */
    if (platformType != platformNotificationNone) {
        err = NMInitializePlatform(platformType);
        if (err != noErr && gModernState.legacyFallback) {
            /* Fall back to legacy mode */
            gModernState.legacyMode = true;
            err = noErr;
        }
    } else {
        /* Use legacy mode */
        gModernState.legacyMode = true;
    }

    if (err == noErr) {
        gModernState.initialized = true;
    }

    return err;
}

void NMModernCleanup(void)
{
    int i;

    if (!gModernState.initialized) {
        return;
    }

    /* Cleanup platform interface */
    if (gModernState.platformInterface && gModernState.platformInterface->cleanup) {
        gModernState.platformInterface->cleanup();
    }

    /* Cleanup categories */
    for (i = 0; i < gModernState.categoryCount; i++) {
        if (gModernState.categories[i]) {
            DisposePtr((Ptr)gModernState.categories[i]);
        }
        if (gModernState.categoryActions[i]) {
            NMDisposeNotificationAction(gModernState.categoryActions[i]);
        }
    }

    gModernState.initialized = false;
}

/*
 * Permission Management
 */

OSErr NMRequestNotificationPermission(Boolean *granted)
{
    OSErr err = noErr;

    if (!gModernState.initialized) {
        return modernErrNotInitialized;
    }

    if (!granted) {
        return nmErrInvalidParameter;
    }

    /* Check if platform supports permission requests */
    if (gModernState.platformInterface && gModernState.platformInterface->requestPermission) {
        err = gModernState.platformInterface->requestPermission(granted);
        gModernState.permissionGranted = *granted;
    } else {
        /* Legacy mode or platform doesn't require permissions */
        *granted = true;
        gModernState.permissionGranted = true;
    }

    return err;
}

Boolean NMHasNotificationPermission(void)
{
    return gModernState.initialized && gModernState.permissionGranted;
}

/*
 * Modern Notification Posting
 */

OSErr NMPostModernNotification(ModernNotificationPtr notification)
{
    OSErr err;
    NMExtendedRecPtr legacyNotification = NULL;

    if (!gModernState.initialized) {
        return modernErrNotInitialized;
    }

    if (!notification) {
        return nmErrInvalidParameter;
    }

    /* Validate notification */
    err = NMValidateModernNotification(notification);
    if (err != noErr) {
        return err;
    }

    gModernState.totalPosted++;

    /* Check permission */
    if (!gModernState.permissionGranted) {
        Boolean granted;
        err = NMRequestNotificationPermission(&granted);
        if (err != noErr || !granted) {
            NMUpdateModernStatistics(false, modernErrPermissionDenied);
            return modernErrPermissionDenied;
        }
    }

    /* Post to platform or fall back to legacy */
    if (!gModernState.legacyMode && gModernState.platformInterface &&
        gModernState.platformInterface->postNotification) {

        err = gModernState.platformInterface->postNotification(notification);
        if (err != noErr && gModernState.legacyFallback) {
            /* Fall back to legacy system */
            err = NMConvertFromModern(notification, &legacyNotification);
            if (err == noErr) {
                err = NMInstallExtended(legacyNotification);
            }
        }
    } else {
        /* Use legacy system */
        err = NMConvertFromModern(notification, &legacyNotification);
        if (err == noErr) {
            err = NMInstallExtended(legacyNotification);
        }
    }

    /* Update statistics */
    NMUpdateModernStatistics(err == noErr, err);

    /* Trigger delivered callback */
    if (gModernState.deliveredCallback) {
        gModernState.deliveredCallback(notification->identifier, err == noErr, err);
    }

    return err;
}

OSErr NMRemoveModernNotification(StringPtr identifier)
{
    OSErr err = noErr;

    if (!gModernState.initialized) {
        return modernErrNotInitialized;
    }

    if (!identifier) {
        return nmErrInvalidParameter;
    }

    /* Remove from platform */
    if (!gModernState.legacyMode && gModernState.platformInterface &&
        gModernState.platformInterface->removeNotification) {

        err = gModernState.platformInterface->removeNotification(identifier);
    }

    /* Also try to remove from legacy system */
    /* This would require maintaining a mapping between modern and legacy notifications */

    return err;
}

/*
 * Rich Content Management
 */

OSErr NMCreateRichContent(RichNotificationPtr *content)
{
    RichNotificationPtr newContent;

    if (!content) {
        return nmErrInvalidParameter;
    }

    newContent = (RichNotificationPtr)NewPtrClear(sizeof(RichNotificationContent));
    if (!newContent) {
        return nmErrOutOfMemory;
    }

    *content = newContent;
    return noErr;
}

void NMDisposeRichContent(RichNotificationPtr content)
{
    if (!content) {
        return;
    }

    /* Dispose string resources */
    if (content->title) DisposePtr((Ptr)content->title);
    if (content->subtitle) DisposePtr((Ptr)content->subtitle);
    if (content->body) DisposePtr((Ptr)content->body);
    if (content->footer) DisposePtr((Ptr)content->footer);
    if (content->progressText) DisposePtr((Ptr)content->progressText);

    /* Dispose handle resources */
    if (content->image) DisposeHandle(content->image);
    if (content->icon) DisposeHandle(content->icon);
    if (content->sound) DisposeHandle(content->sound);
    if (content->customData) DisposeHandle(content->customData);

    DisposePtr((Ptr)content);
}

OSErr NMSetContentTitle(RichNotificationPtr content, StringPtr title)
{
    if (!content) {
        return nmErrInvalidParameter;
    }

    /* Dispose existing title */
    if (content->title) {
        DisposePtr((Ptr)content->title);
    }

    /* Copy new title */
    if (title && title[0] > 0) {
        content->title = (StringPtr)NewPtr(title[0] + 1);
        if (!content->title) {
            return nmErrOutOfMemory;
        }
        BlockMoveData(title, content->title, title[0] + 1);
    } else {
        content->title = NULL;
    }

    return noErr;
}

OSErr NMSetContentBody(RichNotificationPtr content, StringPtr body)
{
    if (!content) {
        return nmErrInvalidParameter;
    }

    /* Dispose existing body */
    if (content->body) {
        DisposePtr((Ptr)content->body);
    }

    /* Copy new body */
    if (body && body[0] > 0) {
        content->body = (StringPtr)NewPtr(body[0] + 1);
        if (!content->body) {
            return nmErrOutOfMemory;
        }
        BlockMoveData(body, content->body, body[0] + 1);
    } else {
        content->body = NULL;
    }

    return noErr;
}

/*
 * Action Management
 */

OSErr NMCreateNotificationAction(NotificationActionPtr *action, StringPtr title, StringPtr identifier)
{
    NotificationActionPtr newAction;

    if (!action || !title || !identifier) {
        return nmErrInvalidParameter;
    }

    newAction = (NotificationActionPtr)NewPtrClear(sizeof(NotificationAction));
    if (!newAction) {
        return nmErrOutOfMemory;
    }

    /* Copy title */
    newAction->title = (StringPtr)NewPtr(title[0] + 1);
    if (!newAction->title) {
        DisposePtr((Ptr)newAction);
        return nmErrOutOfMemory;
    }
    BlockMoveData(title, newAction->title, title[0] + 1);

    /* Copy identifier */
    newAction->identifier = (StringPtr)NewPtr(identifier[0] + 1);
    if (!newAction->identifier) {
        DisposePtr((Ptr)newAction->title);
        DisposePtr((Ptr)newAction);
        return nmErrOutOfMemory;
    }
    BlockMoveData(identifier, newAction->identifier, identifier[0] + 1);

    newAction->isDefault = false;
    newAction->isDestructive = false;
    newAction->icon = NULL;
    newAction->next = NULL;

    *action = newAction;
    return noErr;
}

void NMDisposeNotificationAction(NotificationActionPtr action)
{
    NotificationActionPtr current, next;

    current = action;
    while (current) {
        next = current->next;

        if (current->title) DisposePtr((Ptr)current->title);
        if (current->identifier) DisposePtr((Ptr)current->identifier);
        if (current->icon) DisposeHandle(current->icon);

        DisposePtr((Ptr)current);
        current = next;
    }
}

/*
 * Platform Integration Utilities
 */

OSErr NMConvertToModern(NMExtendedRecPtr nmExtPtr, ModernNotificationPtr *modernPtr)
{
    ModernNotificationPtr modern;
    OSErr err;

    if (!nmExtPtr || !modernPtr) {
        return nmErrInvalidParameter;
    }

    modern = (ModernNotificationPtr)NewPtrClear(sizeof(ModernNotificationRequest));
    if (!modern) {
        return nmErrOutOfMemory;
    }

    /* Create identifier from timestamp and refcon */
    modern->identifier = (StringPtr)NewPtr(32);
    if (modern->identifier) {
        sprintf((char *)&modern->identifier[1], "%08lX%08lX",
                nmExtPtr->timestamp, nmExtPtr->base.nmRefCon);
        modern->identifier[0] = strlen((char *)&modern->identifier[1]);
    }

    /* Convert content */
    err = NMCreateRichContent(&modern->content);
    if (err != noErr) {
        DisposePtr((Ptr)modern);
        return err;
    }

    if (nmExtPtr->base.nmStr) {
        NMSetContentBody(&modern->content, nmExtPtr->base.nmStr);
    }

    modern->content.icon = nmExtPtr->base.nmIcon;
    modern->content.sound = nmExtPtr->base.nmSound;

    /* Set properties based on priority */
    modern->critical = (nmExtPtr->priority >= nmPriorityHigh);
    modern->silent = (nmExtPtr->base.nmSound == NULL);

    /* Set category */
    if (nmExtPtr->category) {
        modern->category = (StringPtr)NewPtr(nmExtPtr->category[0] + 1);
        if (modern->category) {
            BlockMoveData(nmExtPtr->category, modern->category, nmExtPtr->category[0] + 1);
        }
    }

    modern->platform = gModernState.activePlatform;
    modern->features = modernFeatureBasic;

    *modernPtr = modern;
    return noErr;
}

OSErr NMConvertFromModern(ModernNotificationPtr modernPtr, NMExtendedRecPtr *nmExtPtr)
{
    NMExtendedRecPtr nmExt;

    if (!modernPtr || !nmExtPtr) {
        return nmErrInvalidParameter;
    }

    nmExt = (NMExtendedRecPtr)NewPtrClear(sizeof(NMExtendedRec));
    if (!nmExt) {
        return nmErrOutOfMemory;
    }

    /* Convert basic properties */
    nmExt->base.qType = nmType;
    nmExt->base.nmStr = modernPtr->content.body;
    nmExt->base.nmIcon = modernPtr->content.icon;
    nmExt->base.nmSound = modernPtr->content.sound;

    /* Set priority based on modern properties */
    if (modernPtr->critical) {
        nmExt->priority = nmPriorityUrgent;
    } else {
        nmExt->priority = nmPriorityNormal;
    }

    nmExt->status = nmStatusPending;
    nmExt->timestamp = NMGetTimestamp();
    nmExt->modal = modernPtr->critical;
    nmExt->persistent = true;

    /* Set category */
    if (modernPtr->category) {
        nmExt->category = (StringPtr)NewPtr(modernPtr->category[0] + 1);
        if (nmExt->category) {
            BlockMoveData(modernPtr->category, nmExt->category, modernPtr->category[0] + 1);
        }
    }

    *nmExtPtr = nmExt;
    return noErr;
}

/*
 * Badge Management
 */

OSErr NMSetAppBadge(short count)
{
    if (!gModernState.initialized) {
        return modernErrNotInitialized;
    }

    if (gModernState.platformInterface && gModernState.platformInterface->setBadgeCount) {
        return gModernState.platformInterface->setBadgeCount(count);
    }

    return modernErrNotSupported;
}

OSErr NMGetAppBadge(short *count)
{
    if (!gModernState.initialized) {
        return modernErrNotInitialized;
    }

    if (!count) {
        return nmErrInvalidParameter;
    }

    if (gModernState.platformInterface && gModernState.platformInterface->getBadgeCount) {
        return gModernState.platformInterface->getBadgeCount(count);
    }

    *count = 0;
    return modernErrNotSupported;
}

/*
 * Internal Implementation
 */

static OSErr NMDetectPlatform(PlatformNotificationType *platformType)
{
    *platformType = platformNotificationNone;

#ifdef __APPLE__
    *platformType = platformNotificationMacOS;
#elif defined(_WIN32)
    *platformType = platformNotificationWindows;
#elif defined(__linux__)
    *platformType = platformNotificationLinux;
#endif

    return noErr;
}

static OSErr NMInitializePlatform(PlatformNotificationType platformType)
{
    OSErr err;

    gModernState.activePlatform = platformType;

    /* Create platform interface */
    err = NMCreateDefaultPlatformInterface(platformType, &gModernState.platformInterface);
    if (err != noErr) {
        return err;
    }

    /* Initialize platform */
    if (gModernState.platformInterface->initialize) {
        err = gModernState.platformInterface->initialize();
        if (err != noErr) {
            DisposePtr((Ptr)gModernState.platformInterface);
            gModernState.platformInterface = NULL;
            return err;
        }
    }

    gModernState.supportedFeatures = gModernState.platformInterface->supportedFeatures;
    return noErr;
}

static OSErr NMValidateModernNotification(ModernNotificationPtr notification)
{
    if (!notification) {
        return nmErrInvalidParameter;
    }

    if (!notification->identifier || notification->identifier[0] == 0) {
        return modernErrInvalidContent;
    }

    if (!notification->content.title && !notification->content.body) {
        return modernErrInvalidContent;
    }

    return noErr;
}

static void NMUpdateModernStatistics(Boolean delivered, OSErr error)
{
    if (delivered) {
        gModernState.totalDelivered++;
    } else {
        gModernState.totalFailed++;
    }
}

static OSErr NMCreateDefaultPlatformInterface(PlatformNotificationType platformType,
                                            PlatformNotificationInterfacePtr *interface)
{
    PlatformNotificationInterfacePtr newInterface;

    newInterface = (PlatformNotificationInterfacePtr)NewPtrClear(sizeof(PlatformNotificationInterface));
    if (!newInterface) {
        return nmErrOutOfMemory;
    }

    newInterface->type = platformType;
    newInterface->supportedFeatures = modernFeatureBasic;

    /* Set platform-specific properties and function pointers */
    switch (platformType) {
#ifdef __APPLE__
        case platformNotificationMacOS:
            newInterface->name = "\pmacOS Notification Center";
            newInterface->version = "\p1.0";
            newInterface->supportedFeatures = modernFeatureBasic | modernFeatureRichText |
                                            modernFeatureImages | modernFeatureActions |
                                            modernFeatureBadges;
            newInterface->initialize = NMInitializeMacOSNotifications;
            newInterface->cleanup = NMCleanupMacOSNotifications;
            newInterface->postNotification = NMPostToNotificationCenter;
            newInterface->removeNotification = NMRemoveFromNotificationCenter;
            break;
#endif

#ifdef _WIN32
        case platformNotificationWindows:
            newInterface->name = "\pWindows Notifications";
            newInterface->version = "\p1.0";
            newInterface->supportedFeatures = modernFeatureBasic | modernFeatureRichText |
                                            modernFeatureImages | modernFeatureActions;
            newInterface->initialize = NMInitializeWindowsNotifications;
            newInterface->cleanup = NMCleanupWindowsNotifications;
            newInterface->postNotification = NMPostToWindowsNotifications;
            newInterface->removeNotification = NMRemoveFromWindowsNotifications;
            break;
#endif

#ifdef __linux__
        case platformNotificationLinux:
            newInterface->name = "\pLinux Desktop Notifications";
            newInterface->version = "\p1.0";
            newInterface->supportedFeatures = modernFeatureBasic | modernFeatureRichText |
                                            modernFeatureImages;
            newInterface->initialize = NMInitializeLinuxNotifications;
            newInterface->cleanup = NMCleanupLinuxNotifications;
            newInterface->postNotification = NMPostToLibnotify;
            newInterface->removeNotification = NMRemoveFromLibnotify;
            break;
#endif

        default:
            DisposePtr((Ptr)newInterface);
            return modernErrNotSupported;
    }

    *interface = newInterface;
    return noErr;
}

/*
 * Legacy Compatibility
 */

OSErr NMEnableLegacyMode(Boolean enabled)
{
    if (!gModernState.initialized) {
        return modernErrNotInitialized;
    }

    gModernState.legacyMode = enabled;
    return noErr;
}

Boolean NMIsLegacyModeEnabled(void)
{
    return gModernState.initialized && gModernState.legacyMode;
}

/*
 * Weak stubs for platform-specific implementations
 */

#pragma weak NMInitializeMacOSNotifications
OSErr NMInitializeMacOSNotifications(void)
{
    return modernErrNotSupported;
}

#pragma weak NMCleanupMacOSNotifications
void NMCleanupMacOSNotifications(void)
{
}

#pragma weak NMPostToNotificationCenter
OSErr NMPostToNotificationCenter(ModernNotificationPtr notification)
{
    return modernErrNotSupported;
}

#pragma weak NMRemoveFromNotificationCenter
OSErr NMRemoveFromNotificationCenter(StringPtr identifier)
{
    return modernErrNotSupported;
}

#pragma weak NMInitializeWindowsNotifications
OSErr NMInitializeWindowsNotifications(void)
{
    return modernErrNotSupported;
}

#pragma weak NMCleanupWindowsNotifications
void NMCleanupWindowsNotifications(void)
{
}

#pragma weak NMPostToWindowsNotifications
OSErr NMPostToWindowsNotifications(ModernNotificationPtr notification)
{
    return modernErrNotSupported;
}

#pragma weak NMRemoveFromWindowsNotifications
OSErr NMRemoveFromWindowsNotifications(StringPtr identifier)
{
    return modernErrNotSupported;
}

#pragma weak NMInitializeLinuxNotifications
OSErr NMInitializeLinuxNotifications(void)
{
    return modernErrNotSupported;
}

#pragma weak NMCleanupLinuxNotifications
void NMCleanupLinuxNotifications(void)
{
}

#pragma weak NMPostToLibnotify
OSErr NMPostToLibnotify(ModernNotificationPtr notification)
{
    return modernErrNotSupported;
}

#pragma weak NMRemoveFromLibnotify
OSErr NMRemoveFromLibnotify(StringPtr identifier)
{
    return modernErrNotSupported;
}
#ifndef MODERN_NOTIFICATIONS_H
#define MODERN_NOTIFICATIONS_H

#include "NotificationManager/NotificationManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modern Notification System Integration */

/* Platform notification systems */
typedef enum {
    platformNotificationNone = 0,      /* No platform integration */
    platformNotificationMacOS = 1,     /* macOS Notification Center */
    platformNotificationWindows = 2,   /* Windows notifications */
    platformNotificationLinux = 3,     /* Linux desktop notifications */
    platformNotificationCustom = 4     /* Custom platform implementation */
} PlatformNotificationType;

/* Modern notification features */
typedef enum {
    modernFeatureBasic = 0x0001,       /* Basic notifications */
    modernFeatureRichText = 0x0002,    /* Rich text content */
    modernFeatureImages = 0x0004,      /* Image attachments */
    modernFeatureActions = 0x0008,     /* Action buttons */
    modernFeatureProgress = 0x0010,    /* Progress indicators */
    modernFeatureGrouping = 0x0020,    /* Notification grouping */
    modernFeaturePersistence = 0x0040, /* Persistent notifications */
    modernFeatureScheduling = 0x0080,  /* Scheduled delivery */
    modernFeatureGeofencing = 0x0100,  /* Location-based triggers */
    modernFeatureBadges = 0x0200,      /* App badges */
    modernFeatureChannels = 0x0400     /* Notification channels */
} ModernNotificationFeatures;

/* Rich notification content */
typedef struct {
    StringPtr       title;              /* Notification title */
    StringPtr       subtitle;           /* Notification subtitle */
    StringPtr       body;               /* Notification body text */
    StringPtr       footer;             /* Footer text */
    Handle          image;              /* Attached image */
    Handle          icon;               /* Notification icon */
    Handle          sound;              /* Notification sound */
    Boolean         hasProgress;        /* Has progress indicator */
    short           progress;           /* Progress value (0-100) */
    short           maxProgress;        /* Maximum progress value */
    StringPtr       progressText;       /* Progress description */
    Handle          customData;         /* Custom platform data */
} RichNotificationContent, *RichNotificationPtr;

/* Notification actions */
typedef struct NotificationAction {
    StringPtr                   title;      /* Action button title */
    StringPtr                   identifier; /* Action identifier */
    Boolean                     isDefault;  /* Default action */
    Boolean                     isDestructive; /* Destructive action */
    Handle                      icon;       /* Action icon */
    struct NotificationAction  *next;       /* Next action */
} NotificationAction, *NotificationActionPtr;

/* Modern notification request */
typedef struct {
    /* Basic properties */
    StringPtr               identifier;     /* Unique identifier */
    RichNotificationContent content;        /* Rich content */
    NotificationActionPtr   actions;        /* Available actions */

    /* Scheduling */
    Boolean                 scheduleDelivery; /* Schedule for later */
    UInt32                  deliveryTime;   /* When to deliver */
    Boolean                 repeating;      /* Repeating notification */
    UInt32                  repeatInterval; /* Repeat interval */

    /* Behavior */
    Boolean                 silent;         /* Silent notification */
    Boolean                 critical;       /* Critical notification */
    Boolean                 provisional;    /* Provisional authorization */
    short                   badge;          /* App badge number */
    StringPtr               category;       /* Notification category */
    StringPtr               threadID;       /* Thread identifier for grouping */

    /* Platform-specific */
    PlatformNotificationType platform;     /* Target platform */
    Handle                  platformData;   /* Platform-specific data */
    ModernNotificationFeatures features;   /* Required features */
} ModernNotificationRequest, *ModernNotificationPtr;

/* Notification response */
typedef struct {
    StringPtr               identifier;     /* Notification identifier */
    StringPtr               actionID;       /* Selected action ID */
    StringPtr               userText;       /* User input text */
    UInt32                  responseTime;   /* When user responded */
    Boolean                 isDefaultAction; /* Default action selected */
} ModernNotificationResponse, *ModernNotificationResponsePtr;

/* Modern notification callbacks */
typedef void (*ModernNotificationDeliveredProc)(StringPtr identifier, Boolean delivered, OSErr error);
typedef void (*ModernNotificationResponseProc)(ModernNotificationResponsePtr response);
typedef void (*ModernNotificationWillPresentProc)(ModernNotificationPtr notification, Boolean *shouldPresent);

/* Platform abstraction interface */
typedef struct {
    PlatformNotificationType    type;
    StringPtr                   name;
    StringPtr                   version;
    ModernNotificationFeatures  supportedFeatures;

    /* Function pointers */
    OSErr (*initialize)(void);
    void (*cleanup)(void);
    OSErr (*requestPermission)(Boolean *granted);
    OSErr (*postNotification)(ModernNotificationPtr notification);
    OSErr (*removeNotification)(StringPtr identifier);
    OSErr (*removeAllNotifications)(void);
    OSErr (*getBadgeCount)(short *count);
    OSErr (*setBadgeCount)(short count);
    OSErr (*getDeliveredNotifications)(StringPtr **identifiers, short *count);
    OSErr (*getPendingNotifications)(StringPtr **identifiers, short *count);
} PlatformNotificationInterface, *PlatformNotificationInterfacePtr;

/* Modern Notification Manager Functions */

/* Initialization */
OSErr NMModernInit(PlatformNotificationType platformType);
void NMModernCleanup(void);
OSErr NMRegisterPlatform(PlatformNotificationInterfacePtr interface);
OSErr NMUnregisterPlatform(PlatformNotificationType platformType);

/* Permission management */
OSErr NMRequestNotificationPermission(Boolean *granted);
Boolean NMHasNotificationPermission(void);
OSErr NMGetPermissionStatus(Boolean *authorized, Boolean *provisional);

/* Modern notification posting */
OSErr NMPostModernNotification(ModernNotificationPtr notification);
OSErr NMScheduleModernNotification(ModernNotificationPtr notification, UInt32 deliveryTime);
OSErr NMRemoveModernNotification(StringPtr identifier);
OSErr NMRemoveAllModernNotifications(void);

/* Rich content creation */
OSErr NMCreateRichContent(RichNotificationPtr *content);
void NMDisposeRichContent(RichNotificationPtr content);
OSErr NMSetContentTitle(RichNotificationPtr content, StringPtr title);
OSErr NMSetContentSubtitle(RichNotificationPtr content, StringPtr subtitle);
OSErr NMSetContentBody(RichNotificationPtr content, StringPtr body);
OSErr NMSetContentImage(RichNotificationPtr content, Handle image);
OSErr NMSetContentProgress(RichNotificationPtr content, short progress, short maxProgress);

/* Action management */
OSErr NMCreateNotificationAction(NotificationActionPtr *action, StringPtr title, StringPtr identifier);
void NMDisposeNotificationAction(NotificationActionPtr action);
OSErr NMAddActionToNotification(ModernNotificationPtr notification, NotificationActionPtr action);
OSErr NMRemoveActionFromNotification(ModernNotificationPtr notification, StringPtr identifier);

/* Callback registration */
OSErr NMSetNotificationDeliveredCallback(ModernNotificationDeliveredProc callback);
OSErr NMSetNotificationResponseCallback(ModernNotificationResponseProc callback);
OSErr NMSetNotificationWillPresentCallback(ModernNotificationWillPresentProc callback);

/* Badge management */
OSErr NMSetAppBadge(short count);
OSErr NMGetAppBadge(short *count);
OSErr NMClearAppBadge(void);

/* Notification management */
OSErr NMGetDeliveredNotifications(StringPtr **identifiers, short *count);
OSErr NMGetPendingNotifications(StringPtr **identifiers, short *count);
OSErr NMGetNotificationSettings(ModernNotificationFeatures *features, Boolean *enabled);

/* Category management */
OSErr NMRegisterNotificationCategory(StringPtr category, NotificationActionPtr actions);
OSErr NMUnregisterNotificationCategory(StringPtr category);
OSErr NMGetRegisteredCategories(StringPtr **categories, short *count);

/* Platform integration utilities */
OSErr NMConvertToModern(NMExtendedRecPtr nmExtPtr, ModernNotificationPtr *modernPtr);
OSErr NMConvertFromModern(ModernNotificationPtr modernPtr, NMExtendedRecPtr *nmExtPtr);
OSErr NMGetPlatformCapabilities(ModernNotificationFeatures *features);
Boolean NMIsPlatformFeatureSupported(ModernNotificationFeatures feature);

/* Legacy compatibility */
OSErr NMEnableLegacyMode(Boolean enabled);
Boolean NMIsLegacyModeEnabled(void);
OSErr NMSetLegacyFallback(Boolean fallback);

/* Platform-specific implementations */

/* macOS Notification Center */
#ifdef __APPLE__
OSErr NMInitializeMacOSNotifications(void);
void NMCleanupMacOSNotifications(void);
OSErr NMPostToNotificationCenter(ModernNotificationPtr notification);
OSErr NMRemoveFromNotificationCenter(StringPtr identifier);
#endif

/* Windows notifications */
#ifdef _WIN32
OSErr NMInitializeWindowsNotifications(void);
void NMCleanupWindowsNotifications(void);
OSErr NMPostToWindowsNotifications(ModernNotificationPtr notification);
OSErr NMRemoveFromWindowsNotifications(StringPtr identifier);
#endif

/* Linux desktop notifications */
#ifdef __linux__
OSErr NMInitializeLinuxNotifications(void);
void NMCleanupLinuxNotifications(void);
OSErr NMPostToLibnotify(ModernNotificationPtr notification);
OSErr NMRemoveFromLibnotify(StringPtr identifier);
#endif

/* Error codes */
#define modernErrNotSupported       -44000  /* Feature not supported */
#define modernErrPermissionDenied   -44001  /* Permission denied */
#define modernErrInvalidContent     -44002  /* Invalid content */
#define modernErrPlatformFailure    -44003  /* Platform-specific failure */
#define modernErrNotInitialized     -44004  /* Modern notifications not initialized */
#define modernErrInvalidAction      -44005  /* Invalid action */
#define modernErrInvalidCategory    -44006  /* Invalid category */

/* Constants */
#define MODERN_MAX_TITLE_LENGTH     100     /* Maximum title length */
#define MODERN_MAX_BODY_LENGTH      500     /* Maximum body text length */
#define MODERN_MAX_ACTIONS          10      /* Maximum actions per notification */
#define MODERN_MAX_CATEGORIES       50      /* Maximum registered categories */

#ifdef __cplusplus
}
#endif

#endif /* MODERN_NOTIFICATIONS_H */
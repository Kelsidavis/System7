/*
 * NotificationSystem.h
 *
 * Notification System API for Edition Manager
 * Handles update notifications, callbacks, and event distribution
 */

#ifndef __NOTIFICATION_SYSTEM_H__
#define __NOTIFICATION_SYSTEM_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Notification Event Types
 */
enum {
    kNotificationEventGeneric = 0x0001,         /* Generic event */
    kNotificationEventDataChanged = 0x0002,     /* Data has changed */
    kNotificationEventDataRead = 0x0004,        /* Data was read */
    kNotificationEventDataWritten = 0x0008,     /* Data was written */
    kNotificationEventFormatChanged = 0x0010,   /* Format was changed */
    kNotificationEventSectionChanged = 0x0020,  /* Section was modified */
    kNotificationEventContainerMoved = 0x0040,  /* Container was moved */
    kNotificationEventContainerDeleted = 0x0080, /* Container was deleted */
    kNotificationEventPublisherDisconnected = 0x0100, /* Publisher disconnected */
    kNotificationEventSubscriberAdded = 0x0200, /* New subscriber added */
    kNotificationEventError = 0x8000            /* Error occurred */
};

/*
 * Notification Priority Levels
 */
enum {
    kNotificationPriorityLow = 1,
    kNotificationPriorityNormal = 5,
    kNotificationPriorityHigh = 10,
    kNotificationPriorityCritical = 20
};

/*
 * Notification Event Structure
 */
typedef struct {
    ResType eventClass;             /* Event class identifier */
    ResType eventID;                /* Event ID */
    SectionHandle sectionH;         /* Associated section */
    TimeStamp timestamp;            /* When event occurred */
    void* eventData;                /* Event-specific data */
    Size dataSize;                  /* Size of event data */
} NotificationEvent;

/*
 * Notification Callback Type
 */
typedef void (*NotificationCallback)(const NotificationEvent* event, void* userData);

/*
 * Notification Statistics
 */
typedef struct {
    uint32_t activeSubscriptions;   /* Number of active subscriptions */
    uint32_t pendingNotifications;  /* Number of pending notifications */
    uint32_t totalNotificationsSent; /* Total notifications sent */
    uint32_t failedNotifications;   /* Failed notification deliveries */
    TimeStamp lastNotificationTime; /* Last notification timestamp */
    bool notificationThreadRunning; /* Notification thread status */
    int32_t processingInterval;     /* Processing interval in ms */
} NotificationStatistics;

/*
 * Notification Filter
 */
typedef struct {
    uint32_t eventMask;             /* Event types to include */
    SectionHandle sectionFilter;    /* Filter by section (NULL = all) */
    AppRefNum appFilter;            /* Filter by application (NULL = all) */
    TimeStamp startTime;            /* Filter by time range start */
    TimeStamp endTime;              /* Filter by time range end */
    int32_t minPriority;            /* Minimum priority level */
} NotificationFilter;

/*
 * Core Notification Functions
 */

/* Initialize notification system */
OSErr InitializeNotificationSystem(void);

/* Clean up notification system */
void CleanupNotificationSystem(void);

/* Register for edition notifications */
OSErr RegisterForEditionNotifications(SectionHandle sectionH,
                                     const EditionContainerSpec* container);

/* Unregister from edition notifications */
OSErr UnregisterFromEditionNotifications(SectionHandle sectionH);

/* Set notification callback */
OSErr SetNotificationCallback(SectionHandle sectionH,
                             NotificationCallback callback,
                             void* userData,
                             uint32_t eventMask);

/* Remove notification callback */
OSErr RemoveNotificationCallback(SectionHandle sectionH);

/*
 * Event Posting and Processing
 */

/* Post notification event */
OSErr PostNotificationEvent(SectionHandle sectionH,
                           ResType eventClass,
                           ResType eventID,
                           const void* eventData,
                           Size dataSize);

/* Post high-priority notification */
OSErr PostPriorityNotification(SectionHandle sectionH,
                              ResType eventClass,
                              ResType eventID,
                              const void* eventData,
                              Size dataSize,
                              int32_t priority);

/* Process notification queue */
OSErr ProcessNotificationQueue(void);

/* Flush all pending notifications */
OSErr FlushNotificationQueue(void);

/*
 * Event Filtering and Routing
 */

/* Set notification filter */
OSErr SetNotificationFilter(SectionHandle sectionH, const NotificationFilter* filter);

/* Get notification filter */
OSErr GetNotificationFilter(SectionHandle sectionH, NotificationFilter* filter);

/* Clear notification filter */
OSErr ClearNotificationFilter(SectionHandle sectionH);

/* Route notification to specific application */
OSErr RouteNotification(const NotificationEvent* event, AppRefNum targetApp);

/*
 * Notification Timing and Scheduling
 */

/* Set notification processing interval */
OSErr SetNotificationInterval(int32_t intervalMs);

/* Get notification processing interval */
OSErr GetNotificationInterval(int32_t* intervalMs);

/* Schedule delayed notification */
OSErr ScheduleDelayedNotification(SectionHandle sectionH,
                                 ResType eventClass,
                                 ResType eventID,
                                 const void* eventData,
                                 Size dataSize,
                                 uint32_t delayMs);

/* Cancel scheduled notification */
OSErr CancelScheduledNotification(SectionHandle sectionH,
                                 ResType eventClass,
                                 ResType eventID);

/*
 * Batch Notifications
 */

/* Begin notification batch */
OSErr BeginNotificationBatch(void);

/* End notification batch and deliver */
OSErr EndNotificationBatch(void);

/* Cancel notification batch */
OSErr CancelNotificationBatch(void);

/* Set batch delivery mode */
typedef enum {
    kBatchModeImmediate,            /* Deliver immediately when batch ends */
    kBatchModeDelayed,              /* Deliver after specified delay */
    kBatchModeCoalesced             /* Coalesce similar events */
} BatchDeliveryMode;

OSErr SetBatchDeliveryMode(BatchDeliveryMode mode, uint32_t delayMs);

/*
 * Notification History and Logging
 */

/* Enable notification logging */
OSErr EnableNotificationLogging(const char* logFilePath, bool appendMode);

/* Disable notification logging */
OSErr DisableNotificationLogging(void);

/* Get notification history */
OSErr GetNotificationHistory(SectionHandle sectionH,
                            NotificationEvent** events,
                            uint32_t* eventCount,
                            uint32_t maxEvents);

/* Clear notification history */
OSErr ClearNotificationHistory(SectionHandle sectionH);

/*
 * Statistics and Monitoring
 */

/* Get notification statistics */
OSErr GetNotificationStatistics(NotificationStatistics* stats);

/* Reset notification statistics */
OSErr ResetNotificationStatistics(void);

/* Get notification performance metrics */
typedef struct {
    uint32_t averageDeliveryTime;   /* Average delivery time in ms */
    uint32_t maxDeliveryTime;       /* Maximum delivery time in ms */
    uint32_t queuedNotifications;   /* Currently queued notifications */
    uint32_t droppedNotifications;  /* Notifications dropped due to overflow */
    float deliverySuccessRate;      /* Success rate (0.0 to 1.0) */
} NotificationPerformanceMetrics;

OSErr GetNotificationPerformance(NotificationPerformanceMetrics* metrics);

/*
 * Advanced Notification Features
 */

/* Set notification coalescing */
OSErr SetNotificationCoalescing(bool enable, uint32_t windowMs);

/* Set notification throttling */
OSErr SetNotificationThrottling(uint32_t maxNotificationsPerSecond);

/* Set notification compression */
OSErr SetNotificationCompression(bool enable);

/* Set duplicate notification filtering */
OSErr SetDuplicateFiltering(bool enable, uint32_t windowMs);

/*
 * Error Handling and Recovery
 */

/* Set notification error handler */
typedef void (*NotificationErrorHandler)(OSErr error,
                                        const NotificationEvent* failedEvent,
                                        void* userData);

OSErr SetNotificationErrorHandler(NotificationErrorHandler handler, void* userData);

/* Retry failed notifications */
OSErr RetryFailedNotifications(void);

/* Get failed notification count */
OSErr GetFailedNotificationCount(uint32_t* count);

/*
 * Platform Integration
 */

/* Post section event to platform */
OSErr PostSectionEventToPlatform(SectionHandle sectionH, AppRefNum toApp, ResType classID);

/* Create AppleEvent-style notification */
OSErr CreateSectionAppleEvent(const void* notification);

/* Integrate with platform notification system */
OSErr IntegrateWithPlatformNotifications(bool enable);

/* Set platform notification options */
typedef struct {
    bool useSystemNotifications;   /* Use system notification center */
    bool showBadges;               /* Show notification badges */
    bool playSound;                /* Play notification sounds */
    char soundFile[256];           /* Custom sound file path */
} PlatformNotificationOptions;

OSErr SetPlatformNotificationOptions(const PlatformNotificationOptions* options);

/*
 * Network Notifications
 */

/* Enable network notifications */
OSErr EnableNetworkNotifications(const char* serverAddress, uint16_t port);

/* Disable network notifications */
OSErr DisableNetworkNotifications(void);

/* Send network notification */
OSErr SendNetworkNotification(const char* targetAddress,
                             const NotificationEvent* event);

/* Register network notification handler */
typedef void (*NetworkNotificationHandler)(const NotificationEvent* event,
                                          const char* sourceAddress,
                                          void* userData);

OSErr RegisterNetworkNotificationHandler(NetworkNotificationHandler handler,
                                        void* userData);

/*
 * Security and Access Control
 */

/* Set notification security policy */
typedef enum {
    kNotificationSecurityNone,      /* No security */
    kNotificationSecurityBasic,     /* Basic validation */
    kNotificationSecurityStrict     /* Strict access control */
} NotificationSecurityLevel;

OSErr SetNotificationSecurity(NotificationSecurityLevel level);

/* Set notification access permissions */
OSErr SetNotificationPermissions(SectionHandle sectionH,
                                AppRefNum allowedApp,
                                uint32_t permissions);

/* Validate notification sender */
OSErr ValidateNotificationSender(const NotificationEvent* event,
                                AppRefNum senderApp,
                                bool* isValid);

/*
 * Debugging and Diagnostics
 */

/* Enable notification debugging */
OSErr EnableNotificationDebugging(bool enable);

/* Dump notification queue state */
OSErr DumpNotificationQueue(const char* outputPath);

/* Validate notification system integrity */
OSErr ValidateNotificationSystem(bool* isValid, char* errorMessage, Size messageSize);

/* Get notification system memory usage */
OSErr GetNotificationMemoryUsage(Size* totalMemory, Size* activeMemory);

/*
 * Constants and Limits
 */

#define kMaxNotificationQueueSize 1000      /* Maximum queued notifications */
#define kMaxNotificationDataSize 8192       /* Maximum notification data size */
#define kDefaultNotificationInterval 100    /* Default processing interval (ms) */
#define kMaxNotificationHistory 500         /* Maximum history entries */
#define kMinNotificationInterval 10         /* Minimum processing interval (ms) */
#define kMaxNotificationInterval 10000      /* Maximum processing interval (ms) */

/* Notification delivery modes */
enum {
    kDeliveryModeAsync,             /* Asynchronous delivery */
    kDeliveryModeSync,              /* Synchronous delivery */
    kDeliveryModeQueued             /* Queued delivery */
};

/* Error codes specific to notifications */
enum {
    notificationQueueFullErr = -510, /* Notification queue is full */
    notificationDeliveryErr = -511,  /* Failed to deliver notification */
    notificationTimeoutErr = -512,   /* Notification delivery timeout */
    notificationSecurityErr = -513   /* Notification security violation */
};

#ifdef __cplusplus
}
#endif

#endif /* __NOTIFICATION_SYSTEM_H__ */
/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * NotificationSystem.c
 *
 * Notification system for Edition Manager
 * Handles update notifications, callbacks, and event distribution
 *
 * This module provides the notification infrastructure for keeping
 * publishers and subscribers informed of data changes and events
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/NotificationSystem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

/* Notification subscription entry */
typedef struct NotificationSubscription {
    SectionHandle sectionH;             /* Associated section */
    AppRefNum targetApp;                /* Target application */
    NotificationCallback callback;      /* Callback function */
    void* userData;                     /* User data for callback */
    uint32_t eventMask;                 /* Event types to receive */
    bool isActive;                      /* Subscription is active */
    struct NotificationSubscription* next; /* Linked list */
} NotificationSubscription;

/* Pending notification entry */
typedef struct PendingNotification {
    SectionHandle sectionH;             /* Source section */
    AppRefNum targetApp;                /* Target application */
    ResType eventClass;                 /* Event class */
    ResType eventID;                    /* Event ID */
    void* eventData;                    /* Event data */
    Size dataSize;                      /* Size of event data */
    TimeStamp timestamp;                /* When event was queued */
    int32_t priority;                   /* Event priority */
    struct PendingNotification* next;   /* Linked list */
} PendingNotification;

/* Global notification state */
static NotificationSubscription* gSubscriptions = NULL;
static PendingNotification* gPendingNotifications = NULL;
static pthread_mutex_t gNotificationMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t gNotificationThread = 0;
static bool gNotificationThreadRunning = false;
static int32_t gNotificationInterval = 100;  /* milliseconds */

/* Internal helper functions */
static OSErr AddNotificationSubscription(SectionHandle sectionH, AppRefNum targetApp,
                                         NotificationCallback callback, void* userData,
                                         uint32_t eventMask);
static void RemoveNotificationSubscription(SectionHandle sectionH, AppRefNum targetApp);
static OSErr QueueNotification(SectionHandle sectionH, AppRefNum targetApp,
                              ResType eventClass, ResType eventID,
                              const void* eventData, Size dataSize, int32_t priority);
static void ProcessPendingNotifications(void);
static void* NotificationThreadProc(void* param);
static OSErr DeliverNotification(const PendingNotification* notification);
static NotificationSubscription* FindSubscription(SectionHandle sectionH, AppRefNum targetApp);

/*
 * InitializeNotificationSystem
 *
 * Initialize the notification system.
 */
OSErr InitializeNotificationSystem(void)
{
    /* Initialize mutex */
    pthread_mutex_init(&gNotificationMutex, NULL);

    /* Start notification thread */
    if (!gNotificationThreadRunning) {
        gNotificationThreadRunning = true;
        int result = pthread_create(&gNotificationThread, NULL, NotificationThreadProc, NULL);
        if (result != 0) {
            gNotificationThreadRunning = false;
            return editionMgrInitErr;
        }
    }

    return noErr;
}

/*
 * CleanupNotificationSystem
 *
 * Clean up the notification system.
 */
void CleanupNotificationSystem(void)
{
    /* Stop notification thread */
    if (gNotificationThreadRunning) {
        gNotificationThreadRunning = false;
        pthread_join(gNotificationThread, NULL);
        gNotificationThread = 0;
    }

    pthread_mutex_lock(&gNotificationMutex);

    /* Clean up subscriptions */
    NotificationSubscription* subscription = gSubscriptions;
    while (subscription) {
        NotificationSubscription* next = subscription->next;
        free(subscription);
        subscription = next;
    }
    gSubscriptions = NULL;

    /* Clean up pending notifications */
    PendingNotification* notification = gPendingNotifications;
    while (notification) {
        PendingNotification* next = notification->next;
        if (notification->eventData) {
            free(notification->eventData);
        }
        free(notification);
        notification = next;
    }
    gPendingNotifications = NULL;

    pthread_mutex_unlock(&gNotificationMutex);
    pthread_mutex_destroy(&gNotificationMutex);
}

/*
 * PostSectionEventToPlatform
 *
 * Post a section event to the platform notification system.
 */
OSErr PostSectionEventToPlatform(SectionHandle sectionH, AppRefNum toApp, ResType classID)
{
    if (!sectionH) {
        return badSectionErr;
    }

    /* Queue the notification for delivery */
    OSErr err = QueueNotification(sectionH, toApp, classID, sectionEventMsgClass,
                                 NULL, 0, kNotificationPriorityNormal);
    return err;
}

/*
 * RegisterForEditionNotifications
 *
 * Register to receive notifications for an edition.
 */
OSErr RegisterForEditionNotifications(SectionHandle sectionH,
                                     const EditionContainerSpec* container)
{
    if (!sectionH || !container) {
        return badSectionErr;
    }

    /* Get current application */
    AppRefNum currentApp;
    OSErr err = GetCurrentAppRefNum(&currentApp);
    if (err != noErr) {
        return err;
    }

    /* Add notification subscription */
    err = AddNotificationSubscription(sectionH, currentApp, NULL, NULL,
                                     kNotificationEventDataChanged |
                                     kNotificationEventFormatChanged |
                                     kNotificationEventSectionChanged);
    return err;
}

/*
 * UnregisterFromEditionNotifications
 *
 * Unregister from edition notifications.
 */
OSErr UnregisterFromEditionNotifications(SectionHandle sectionH)
{
    if (!sectionH) {
        return badSectionErr;
    }

    /* Get current application */
    AppRefNum currentApp;
    OSErr err = GetCurrentAppRefNum(&currentApp);
    if (err != noErr) {
        return err;
    }

    /* Remove notification subscription */
    RemoveNotificationSubscription(sectionH, currentApp);
    return noErr;
}

/*
 * SetNotificationCallback
 *
 * Set callback function for notifications.
 */
OSErr SetNotificationCallback(SectionHandle sectionH,
                             NotificationCallback callback,
                             void* userData,
                             uint32_t eventMask)
{
    if (!sectionH || !callback) {
        return badSectionErr;
    }

    /* Get current application */
    AppRefNum currentApp;
    OSErr err = GetCurrentAppRefNum(&currentApp);
    if (err != noErr) {
        return err;
    }

    /* Find existing subscription */
    NotificationSubscription* subscription = FindSubscription(sectionH, currentApp);
    if (subscription) {
        /* Update existing subscription */
        subscription->callback = callback;
        subscription->userData = userData;
        subscription->eventMask = eventMask;
        subscription->isActive = true;
    } else {
        /* Add new subscription */
        err = AddNotificationSubscription(sectionH, currentApp, callback, userData, eventMask);
    }

    return err;
}

/*
 * RemoveNotificationCallback
 *
 * Remove notification callback.
 */
OSErr RemoveNotificationCallback(SectionHandle sectionH)
{
    if (!sectionH) {
        return badSectionErr;
    }

    /* Get current application */
    AppRefNum currentApp;
    OSErr err = GetCurrentAppRefNum(&currentApp);
    if (err != noErr) {
        return err;
    }

    /* Remove subscription */
    RemoveNotificationSubscription(sectionH, currentApp);
    return noErr;
}

/*
 * PostNotificationEvent
 *
 * Post a notification event.
 */
OSErr PostNotificationEvent(SectionHandle sectionH,
                           ResType eventClass,
                           ResType eventID,
                           const void* eventData,
                           Size dataSize)
{
    if (!sectionH) {
        return badSectionErr;
    }

    /* Post to all subscribers of this section */
    pthread_mutex_lock(&gNotificationMutex);

    NotificationSubscription* subscription = gSubscriptions;
    while (subscription) {
        if (subscription->sectionH == sectionH && subscription->isActive) {
            /* Check if subscriber wants this event type */
            uint32_t eventType = 0;
            switch (eventClass) {
                case sectionEventMsgClass:
                    eventType = kNotificationEventDataChanged;
                    break;
                case sectionReadMsgID:
                    eventType = kNotificationEventDataRead;
                    break;
                case sectionWriteMsgID:
                    eventType = kNotificationEventDataWritten;
                    break;
                default:
                    eventType = kNotificationEventGeneric;
                    break;
            }

            if (subscription->eventMask & eventType) {
                QueueNotification(sectionH, subscription->targetApp,
                                eventClass, eventID, eventData, dataSize,
                                kNotificationPriorityNormal);
            }
        }
        subscription = subscription->next;
    }

    pthread_mutex_unlock(&gNotificationMutex);
    return noErr;
}

/*
 * ProcessNotificationQueue
 *
 * Process pending notifications.
 */
OSErr ProcessNotificationQueue(void)
{
    ProcessPendingNotifications();
    return noErr;
}

/*
 * SetNotificationInterval
 *
 * Set notification processing interval.
 */
OSErr SetNotificationInterval(int32_t intervalMs)
{
    if (intervalMs < 10 || intervalMs > 10000) {
        return badSectionErr;
    }

    gNotificationInterval = intervalMs;
    return noErr;
}

/*
 * GetNotificationInterval
 *
 * Get current notification processing interval.
 */
OSErr GetNotificationInterval(int32_t* intervalMs)
{
    if (!intervalMs) {
        return badSectionErr;
    }

    *intervalMs = gNotificationInterval;
    return noErr;
}

/*
 * GetNotificationStatistics
 *
 * Get notification system statistics.
 */
OSErr GetNotificationStatistics(NotificationStatistics* stats)
{
    if (!stats) {
        return badSectionErr;
    }

    memset(stats, 0, sizeof(NotificationStatistics));

    pthread_mutex_lock(&gNotificationMutex);

    /* Count subscriptions */
    NotificationSubscription* subscription = gSubscriptions;
    while (subscription) {
        stats->activeSubscriptions++;
        subscription = subscription->next;
    }

    /* Count pending notifications */
    PendingNotification* notification = gPendingNotifications;
    while (notification) {
        stats->pendingNotifications++;
        notification = notification->next;
    }

    pthread_mutex_unlock(&gNotificationMutex);

    stats->notificationThreadRunning = gNotificationThreadRunning;
    stats->processingInterval = gNotificationInterval;

    return noErr;
}

/* Internal helper functions */

static OSErr AddNotificationSubscription(SectionHandle sectionH, AppRefNum targetApp,
                                         NotificationCallback callback, void* userData,
                                         uint32_t eventMask)
{
    NotificationSubscription* subscription = (NotificationSubscription*)malloc(sizeof(NotificationSubscription));
    if (!subscription) {
        return editionMgrInitErr;
    }

    subscription->sectionH = sectionH;
    subscription->targetApp = targetApp;
    subscription->callback = callback;
    subscription->userData = userData;
    subscription->eventMask = eventMask;
    subscription->isActive = true;

    pthread_mutex_lock(&gNotificationMutex);
    subscription->next = gSubscriptions;
    gSubscriptions = subscription;
    pthread_mutex_unlock(&gNotificationMutex);

    return noErr;
}

static void RemoveNotificationSubscription(SectionHandle sectionH, AppRefNum targetApp)
{
    pthread_mutex_lock(&gNotificationMutex);

    NotificationSubscription* prev = NULL;
    NotificationSubscription* current = gSubscriptions;

    while (current) {
        if (current->sectionH == sectionH && current->targetApp == targetApp) {
            if (prev) {
                prev->next = current->next;
            } else {
                gSubscriptions = current->next;
            }
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&gNotificationMutex);
}

static OSErr QueueNotification(SectionHandle sectionH, AppRefNum targetApp,
                              ResType eventClass, ResType eventID,
                              const void* eventData, Size dataSize, int32_t priority)
{
    PendingNotification* notification = (PendingNotification*)malloc(sizeof(PendingNotification));
    if (!notification) {
        return editionMgrInitErr;
    }

    notification->sectionH = sectionH;
    notification->targetApp = targetApp;
    notification->eventClass = eventClass;
    notification->eventID = eventID;
    notification->dataSize = dataSize;
    notification->timestamp = GetCurrentTimeStamp();
    notification->priority = priority;

    /* Copy event data if provided */
    if (eventData && dataSize > 0) {
        notification->eventData = malloc(dataSize);
        if (notification->eventData) {
            memcpy(notification->eventData, eventData, dataSize);
        } else {
            free(notification);
            return editionMgrInitErr;
        }
    } else {
        notification->eventData = NULL;
    }

    /* Insert into queue based on priority */
    pthread_mutex_lock(&gNotificationMutex);

    if (!gPendingNotifications || priority > gPendingNotifications->priority) {
        /* Insert at head */
        notification->next = gPendingNotifications;
        gPendingNotifications = notification;
    } else {
        /* Find insertion point */
        PendingNotification* prev = gPendingNotifications;
        while (prev->next && prev->next->priority >= priority) {
            prev = prev->next;
        }
        notification->next = prev->next;
        prev->next = notification;
    }

    pthread_mutex_unlock(&gNotificationMutex);
    return noErr;
}

static void ProcessPendingNotifications(void)
{
    pthread_mutex_lock(&gNotificationMutex);

    PendingNotification* notification = gPendingNotifications;
    PendingNotification* prev = NULL;

    while (notification) {
        PendingNotification* next = notification->next;

        /* Try to deliver the notification */
        OSErr err = DeliverNotification(notification);
        if (err == noErr) {
            /* Remove from queue */
            if (prev) {
                prev->next = next;
            } else {
                gPendingNotifications = next;
            }

            /* Free notification */
            if (notification->eventData) {
                free(notification->eventData);
            }
            free(notification);
        } else {
            /* Keep notification in queue for retry */
            prev = notification;
        }

        notification = next;
    }

    pthread_mutex_unlock(&gNotificationMutex);
}

static void* NotificationThreadProc(void* param)
{
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = gNotificationInterval * 1000000;  /* Convert ms to ns */

    while (gNotificationThreadRunning) {
        /* Process pending notifications */
        ProcessPendingNotifications();

        /* Sleep for the specified interval */
        nanosleep(&sleepTime, NULL);

        /* Update sleep time in case interval changed */
        sleepTime.tv_nsec = gNotificationInterval * 1000000;
    }

    return NULL;
}

static OSErr DeliverNotification(const PendingNotification* notification)
{
    if (!notification) {
        return badSectionErr;
    }

    /* Find the subscription for this notification */
    NotificationSubscription* subscription = FindSubscription(notification->sectionH,
                                                              notification->targetApp);
    if (!subscription || !subscription->isActive) {
        return badSectionErr;  /* No active subscription */
    }

    /* Call the callback if available */
    if (subscription->callback) {
        NotificationEvent event;
        event.eventClass = notification->eventClass;
        event.eventID = notification->eventID;
        event.sectionH = notification->sectionH;
        event.timestamp = notification->timestamp;
        event.eventData = notification->eventData;
        event.dataSize = notification->dataSize;

        subscription->callback(&event, subscription->userData);
    } else {
        /* Create AppleEvent-style notification for applications without callbacks */
        CreateSectionAppleEvent(notification);
    }

    return noErr;
}

static NotificationSubscription* FindSubscription(SectionHandle sectionH, AppRefNum targetApp)
{
    NotificationSubscription* current = gSubscriptions;
    while (current) {
        if (current->sectionH == sectionH && current->targetApp == targetApp) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/*
 * CreateSectionAppleEvent
 *
 * Create AppleEvent-style notification for compatibility.
 */
OSErr CreateSectionAppleEvent(const PendingNotification* notification)
{
    /* This would create an AppleEvent in the original Mac OS format
     * For this portable implementation, we'll use a simpler approach */

    printf("Section Event: Class=%c%c%c%c, ID=%c%c%c%c, Section=%p\n",
           (char)(notification->eventClass >> 24),
           (char)(notification->eventClass >> 16),
           (char)(notification->eventClass >> 8),
           (char)(notification->eventClass),
           (char)(notification->eventID >> 24),
           (char)(notification->eventID >> 16),
           (char)(notification->eventID >> 8),
           (char)(notification->eventID),
           (void*)notification->sectionH);

    return noErr;
}
/*
 * NotificationManagerCore.c - Notification Manager Implementation
 *
 * Implements the Mac OS Notification Manager, which allows applications
 * to notify users of events while the app is in the background.
 *
 * Based on Inside Macintosh: Processes, Chapter 3.
 */

#include "SystemTypes.h"
#include "NotificationManager/NotificationManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "SoundManager/SoundManager.h"
#include "System71StdLib.h"
#include "TimeManager/TimeBase.h"
#include <string.h>

/* Debug logging */
#define NM_DEBUG 1

#if NM_DEBUG
extern void serial_puts(const char* str);
#define NM_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[NM] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define NM_LOG(...)
#endif

/* Notification queue - linked list of active notifications */
typedef struct NotificationQueueEntry {
    struct NotificationQueueEntry* next;
    NMRec notification;
    Boolean active;
    UInt32 installTime;  /* Tick count when installed */
} NotificationQueueEntry;

static struct {
    NotificationQueueEntry* queueHead;
    NotificationQueueEntry* queueTail;  /* Tail pointer for O(1) insertion */
    UInt16 notificationCount;
    Boolean initialized;
} gNMState = {NULL, NULL, 0, false};

/* Maximum active notifications */
#define MAX_NOTIFICATIONS 16

/* Notification timeout in ticks (60 ticks = 1 second, 300 seconds = 5 minutes) */
#define NOTIFICATION_TIMEOUT_TICKS (60 * 300)

/*
 * InitNotificationManager - Initialize the Notification Manager
 */
void InitNotificationManager(void) {
    if (gNMState.initialized) {
        return;
    }

    gNMState.queueHead = NULL;
    gNMState.queueTail = NULL;
    gNMState.notificationCount = 0;
    gNMState.initialized = true;

    NM_LOG("Notification Manager initialized\n");
}

/*
 * CleanupExpiredNotifications - Remove notifications that have timed out
 *
 * Automatically removes notifications that have been in the queue longer
 * than NOTIFICATION_TIMEOUT_TICKS to prevent resource leaks.
 */
static void CleanupExpiredNotifications(void) {
    UInt32 currentTime = TickCount();
    NotificationQueueEntry* prev = NULL;
    NotificationQueueEntry* current = gNMState.queueHead;
    UInt16 removedCount = 0;

    while (current != NULL) {
        NotificationQueueEntry* next = current->next;

        /* Check if notification has expired */
        UInt32 age = currentTime - current->installTime;
        if (age > NOTIFICATION_TIMEOUT_TICKS) {
            /* Remove expired notification */
            if (prev == NULL) {
                gNMState.queueHead = next;
            } else {
                prev->next = next;
            }

            /* Update tail if removing last item */
            if (current == gNMState.queueTail) {
                gNMState.queueTail = prev;
            }

            /* Free entry */
            DisposePtr((Ptr)current);
            gNMState.notificationCount--;
            removedCount++;

            current = next;
        } else {
            prev = current;
            current = next;
        }
    }

    if (removedCount > 0) {
        NM_LOG("CleanupExpiredNotifications: Removed %d expired notifications\n", removedCount);
    }
}

/*
 * NMInstall - Install a notification request
 *
 * This function adds a notification to the notification queue.
 * The notification will be displayed to the user according to
 * the settings in the NMRec structure.
 */
OSErr NMInstall(NMRecPtr nmReqPtr) {
    NotificationQueueEntry* entry;

    if (!gNMState.initialized) {
        InitNotificationManager();
    }

    if (!nmReqPtr) {
        return paramErr;
    }

    /* Clean up expired notifications to free space */
    CleanupExpiredNotifications();

    /* Check if we've reached the maximum */
    if (gNMState.notificationCount >= MAX_NOTIFICATIONS) {
        NM_LOG("NMInstall: Maximum notifications reached\n");
        return qErr;  /* Queue full */
    }

    /* Allocate a new queue entry */
    entry = (NotificationQueueEntry*)NewPtr(sizeof(NotificationQueueEntry));
    if (!entry) {
        return memFullErr;
    }

    /* Copy the notification record */
    memcpy(&entry->notification, nmReqPtr, sizeof(NMRec));
    entry->next = NULL;
    entry->active = true;
    entry->installTime = TickCount();  /* Record installation time for timeout */

    /* Add to queue using tail pointer for O(1) insertion */
    if (gNMState.queueHead == NULL) {
        gNMState.queueHead = entry;
        gNMState.queueTail = entry;
    } else {
        gNMState.queueTail->next = entry;
        gNMState.queueTail = entry;
    }

    gNMState.notificationCount++;

    NM_LOG("NMInstall: Installed notification (count=%d)\n", gNMState.notificationCount);

    /* Play notification sound if specified */
    if (nmReqPtr->nmSound != NULL) {
        /* Simple beep for now - could be enhanced to play actual sound handle */
        extern void SysBeep(SInt16 duration);
        SysBeep(10);
        NM_LOG("NMInstall: Played notification sound\n");
    }

    /* Display notification string if specified */
    if (nmReqPtr->nmStr != NULL) {
        NM_LOG("NMInstall: Notification message: %s\n", nmReqPtr->nmStr);
        /* In a full implementation, this would display an alert or notification dialog */
    }

    /* Mark the notification in the menu bar if requested */
    if (nmReqPtr->nmMark != 0) {
        NM_LOG("NMInstall: Notification mark requested (%d)\n", nmReqPtr->nmMark);
        /* In a full implementation, this would flash the app icon in the menu bar */
    }

    /* Update the qLink in the original record to point to our entry */
    nmReqPtr->qLink = (struct NMRec*)entry;

    return noErr;
}

/*
 * NMRemove - Remove a notification request
 *
 * This function removes a notification from the notification queue.
 */
OSErr NMRemove(NMRecPtr nmReqPtr) {
    NotificationQueueEntry* entry;
    NotificationQueueEntry* prev;

    if (!gNMState.initialized) {
        return nmTypeErr;  /* Notification Manager not initialized */
    }

    if (!nmReqPtr) {
        return paramErr;
    }

    /* Find the notification in the queue */
    prev = NULL;
    entry = gNMState.queueHead;

    while (entry != NULL) {
        /* Check if this is the notification we're looking for */
        if ((struct NMRec*)entry == nmReqPtr->qLink) {
            /* Remove from queue */
            if (prev == NULL) {
                gNMState.queueHead = entry->next;
            } else {
                prev->next = entry->next;
            }

            /* Update tail pointer if we removed the last element */
            if (gNMState.queueTail == entry) {
                gNMState.queueTail = prev;
            }

            /* Free the entry */
            DisposePtr((Ptr)entry);
            gNMState.notificationCount--;

            NM_LOG("NMRemove: Removed notification (count=%d)\n", gNMState.notificationCount);

            /* Clear the qLink in the original record */
            nmReqPtr->qLink = NULL;

            return noErr;
        }

        prev = entry;
        entry = entry->next;
    }

    /* Notification not found */
    NM_LOG("NMRemove: Notification not found\n");
    return qErr;
}

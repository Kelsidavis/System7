/*
 * InterAppScrap.c - Inter-Application Scrap Exchange
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements inter-application data exchange, ownership tracking,
 * change notifications, and communication protocols for the Scrap Manager.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "ScrapManager/ScrapManager.h"
#include "ScrapManager/ScrapTypes.h"
#include "MacTypes.h"
#include "ProcessManager.h"
#include "ErrorCodes.h"

/* Process tracking for scrap ownership */
typedef struct ProcessEntry {
    ProcessSerialNumber psn;
    Str255              processName;
    pid_t               processID;
    Boolean             isActive;
    time_t              lastAccess;
    uint32_t            accessCount;
    struct ProcessEntry *next;
} ProcessEntry;

/* Change notification tracking */
typedef struct NotificationEntry {
    ScrapChangeCallback callback;
    void                *userData;
    Boolean             isActive;
    struct NotificationEntry *next;
} NotificationEntry;

/* Inter-app communication state */
typedef struct {
    ProcessSerialNumber     currentOwner;
    Boolean                 hasOwner;
    ProcessEntry            *processList;
    NotificationEntry       *notifications;
    int16_t                 lastScrapCount;
    Boolean                 broadcastChanges;
    time_t                  lastNotification;
    uint32_t                notificationCount;
} InterAppState;

static InterAppState gInterAppState = {0};
static Boolean gInterAppInitialized = false;

/* Internal function prototypes */
static OSErr InitializeInterApp(void);
static ProcessEntry *FindProcessEntry(const ProcessSerialNumber *psn);
static ProcessEntry *AddProcessEntry(const ProcessSerialNumber *psn, ConstStr255Param name);
static void RemoveProcessEntry(const ProcessSerialNumber *psn);
static OSErr BroadcastScrapChange(int16_t newCount);
static OSErr ValidateProcessSerialNumber(const ProcessSerialNumber *psn);
static void CleanupInactiveProcesses(void);
static NotificationEntry *FindNotificationEntry(ScrapChangeCallback callback);
static pid_t GetProcessID(const ProcessSerialNumber *psn);
static OSErr GetProcessName(const ProcessSerialNumber *psn, Str255 name);

/*
 * Inter-Application Functions
 */

OSErr RegisterScrapChangeCallback(ScrapChangeCallback callback, void *userData)
{
    NotificationEntry *entry;

    if (callback == NULL) {
        return paramErr;
    }

    if (!gInterAppInitialized) {
        InitializeInterApp();
    }

    /* Check if already registered */
    entry = FindNotificationEntry(callback);
    if (entry) {
        entry->userData = userData;
        entry->isActive = true;
        return noErr;
    }

    /* Add new notification entry */
    entry = (NotificationEntry *)malloc(sizeof(NotificationEntry));
    if (!entry) {
        return memFullErr;
    }

    entry->callback = callback;
    entry->userData = userData;
    entry->isActive = true;
    entry->next = gInterAppState.notifications;
    gInterAppState.notifications = entry;

    return noErr;
}

OSErr UnregisterScrapChangeCallback(ScrapChangeCallback callback)
{
    NotificationEntry *entry, *prev = NULL;

    if (callback == NULL) {
        return paramErr;
    }

    if (!gInterAppInitialized) {
        return scrapNoError;
    }

    /* Find and remove notification entry */
    entry = gInterAppState.notifications;
    while (entry) {
        if (entry->callback == callback) {
            if (prev) {
                prev->next = entry->next;
            } else {
                gInterAppState.notifications = entry->next;
            }
            free(entry);
            return noErr;
        }
        prev = entry;
        entry = entry->next;
    }

    return scrapNoTypeError;
}

OSErr GetScrapOwner(ProcessSerialNumber *psn, Str255 processName)
{
    if (!gInterAppInitialized) {
        InitializeInterApp();
    }

    if (!gInterAppState.hasOwner) {
        return scrapNoScrap;
    }

    if (psn) {
        *psn = gInterAppState.currentOwner;
    }

    if (processName) {
        GetProcessName(&gInterAppState.currentOwner, processName);
    }

    return noErr;
}

OSErr SetScrapOwner(const ProcessSerialNumber *psn)
{
    ProcessEntry *entry;

    if (!gInterAppInitialized) {
        InitializeInterApp();
    }

    if (psn) {
        /* Validate process serial number */
        OSErr err = ValidateProcessSerialNumber(psn);
        if (err != noErr) {
            return err;
        }

        gInterAppState.currentOwner = *psn;
        gInterAppState.hasOwner = true;

        /* Update process tracking */
        entry = FindProcessEntry(psn);
        if (!entry) {
            Str255 name;
            GetProcessName(psn, name);
            AddProcessEntry(psn, name);
        } else {
            entry->lastAccess = time(NULL);
            entry->accessCount++;
        }
    } else {
        /* Clear ownership */
        gInterAppState.hasOwner = false;
    }

    return noErr;
}

void NotifyScrapChange(void)
{
    PScrapStuff scrapInfo;
    int16_t currentCount;

    if (!gInterAppInitialized) {
        InitializeInterApp();
    }

    scrapInfo = InfoScrap();
    currentCount = scrapInfo ? scrapInfo->scrapCount : 0;

    /* Check if count actually changed */
    if (currentCount != gInterAppState.lastScrapCount) {
        gInterAppState.lastScrapCount = currentCount;

        if (gInterAppState.broadcastChanges) {
            BroadcastScrapChange(currentCount);
        }

        gInterAppState.lastNotification = time(NULL);
        gInterAppState.notificationCount++;
    }
}

/*
 * Process Management Functions
 */

OSErr RegisterScrapProcess(const ProcessSerialNumber *psn, ConstStr255Param processName)
{
    ProcessEntry *entry;

    if (!psn) {
        return paramErr;
    }

    if (!gInterAppInitialized) {
        InitializeInterApp();
    }

    /* Check if already registered */
    entry = FindProcessEntry(psn);
    if (entry) {
        entry->isActive = true;
        entry->lastAccess = time(NULL);
        if (processName && processName[0] > 0) {
            memcpy(entry->processName, processName, processName[0] + 1);
        }
        return noErr;
    }

    /* Add new process entry */
    entry = AddProcessEntry(psn, processName);
    return entry ? noErr : memFullErr;
}

OSErr UnregisterScrapProcess(const ProcessSerialNumber *psn)
{
    if (!psn) {
        return paramErr;
    }

    if (!gInterAppInitialized) {
        return scrapNoError;
    }

    /* If this process was the owner, clear ownership */
    if (gInterAppState.hasOwner &&
        gInterAppState.currentOwner.highLongOfPSN == psn->highLongOfPSN &&
        gInterAppState.currentOwner.lowLongOfPSN == psn->lowLongOfPSN) {
        gInterAppState.hasOwner = false;
    }

    RemoveProcessEntry(psn);
    return noErr;
}

OSErr EnumerateScrapProcesses(ProcessSerialNumber *processes, Str255 *names,
                             int16_t *count, int16_t maxProcesses)
{
    ProcessEntry *entry;
    int16_t actualCount = 0;

    if (!count) {
        return paramErr;
    }

    if (!gInterAppInitialized) {
        InitializeInterApp();
        *count = 0;
        return noErr;
    }

    /* Clean up inactive processes first */
    CleanupInactiveProcesses();

    /* Enumerate active processes */
    entry = gInterAppState.processList;
    while (entry && actualCount < maxProcesses) {
        if (entry->isActive) {
            if (processes) {
                processes[actualCount] = entry->psn;
            }
            if (names) {
                memcpy(names[actualCount], entry->processName, entry->processName[0] + 1);
            }
            actualCount++;
        }
        entry = entry->next;
    }

    *count = actualCount;
    return noErr;
}

/*
 * Access Control Functions
 */

OSErr ShareScrapWith(const ProcessSerialNumber *targetApps, int16_t appCount)
{
    /* For now, scrap is always shared globally */
    /* In a more secure implementation, this would set access permissions */

    return noErr;
}

OSErr RestrictScrapAccess(const ProcessSerialNumber *allowedApps, int16_t appCount)
{
    /* For now, no access restrictions are implemented */
    /* In a more secure implementation, this would set access controls */

    return noErr;
}

OSErr ClearScrapRestrictions(void)
{
    /* Clear any access restrictions */
    return noErr;
}

/*
 * Communication Protocol Functions
 */

OSErr SendScrapMessage(const ProcessSerialNumber *targetPSN, uint32_t messageType,
                      const void *messageData, int32_t dataSize)
{
    /* This would implement inter-process communication */
    /* For now, return success without actual implementation */

    return noErr;
}

OSErr ReceiveScrapMessage(ProcessSerialNumber *senderPSN, uint32_t *messageType,
                         void *messageData, int32_t *dataSize, int32_t timeout)
{
    /* This would implement message receiving */
    /* For now, return no message available */

    return scrapNoScrap;
}

/*
 * Statistics and Monitoring Functions
 */

OSErr GetInterAppStatistics(uint32_t *processCount, uint32_t *notificationCount,
                           uint32_t *ownerChanges, uint32_t *messagesSent)
{
    ProcessEntry *entry;
    uint32_t activeProcesses = 0;

    if (!gInterAppInitialized) {
        InitializeInterApp();
    }

    /* Count active processes */
    entry = gInterAppState.processList;
    while (entry) {
        if (entry->isActive) {
            activeProcesses++;
        }
        entry = entry->next;
    }

    if (processCount) *processCount = activeProcesses;
    if (notificationCount) *notificationCount = gInterAppState.notificationCount;
    if (ownerChanges) *ownerChanges = 0; /* Would track this in full implementation */
    if (messagesSent) *messagesSent = 0; /* Would track this in full implementation */

    return noErr;
}

void ResetInterAppStatistics(void)
{
    if (!gInterAppInitialized) {
        return;
    }

    gInterAppState.notificationCount = 0;
}

/*
 * Internal Helper Functions
 */

static OSErr InitializeInterApp(void)
{
    if (gInterAppInitialized) {
        return noErr;
    }

    memset(&gInterAppState, 0, sizeof(gInterAppState));

    gInterAppState.broadcastChanges = true;
    gInterAppState.lastScrapCount = 0;

    /* Set up current process as initial owner */
    ProcessSerialNumber currentPSN;
    GetCurrentProcess(&currentPSN);
    SetScrapOwner(&currentPSN);

    gInterAppInitialized = true;
    return noErr;
}

static ProcessEntry *FindProcessEntry(const ProcessSerialNumber *psn)
{
    ProcessEntry *entry;

    if (!psn) {
        return NULL;
    }

    entry = gInterAppState.processList;
    while (entry) {
        if (entry->psn.highLongOfPSN == psn->highLongOfPSN &&
            entry->psn.lowLongOfPSN == psn->lowLongOfPSN) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static ProcessEntry *AddProcessEntry(const ProcessSerialNumber *psn, ConstStr255Param name)
{
    ProcessEntry *entry;

    if (!psn) {
        return NULL;
    }

    entry = (ProcessEntry *)malloc(sizeof(ProcessEntry));
    if (!entry) {
        return NULL;
    }

    memset(entry, 0, sizeof(ProcessEntry));
    entry->psn = *psn;
    entry->processID = GetProcessID(psn);
    entry->isActive = true;
    entry->lastAccess = time(NULL);
    entry->accessCount = 1;

    if (name && name[0] > 0) {
        memcpy(entry->processName, name, name[0] + 1);
    } else {
        GetProcessName(psn, entry->processName);
    }

    /* Add to list */
    entry->next = gInterAppState.processList;
    gInterAppState.processList = entry;

    return entry;
}

static void RemoveProcessEntry(const ProcessSerialNumber *psn)
{
    ProcessEntry *entry, *prev = NULL;

    if (!psn) {
        return;
    }

    entry = gInterAppState.processList;
    while (entry) {
        if (entry->psn.highLongOfPSN == psn->highLongOfPSN &&
            entry->psn.lowLongOfPSN == psn->lowLongOfPSN) {

            if (prev) {
                prev->next = entry->next;
            } else {
                gInterAppState.processList = entry->next;
            }

            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

static OSErr BroadcastScrapChange(int16_t newCount)
{
    NotificationEntry *entry;

    entry = gInterAppState.notifications;
    while (entry) {
        if (entry->isActive && entry->callback) {
            entry->callback(newCount, entry->userData);
        }
        entry = entry->next;
    }

    return noErr;
}

static OSErr ValidateProcessSerialNumber(const ProcessSerialNumber *psn)
{
    if (!psn) {
        return paramErr;
    }

    /* In a full implementation, this would validate that the PSN
     * refers to an actual running process */

    return noErr;
}

static void CleanupInactiveProcesses(void)
{
    ProcessEntry *entry, *next;
    time_t cutoffTime = time(NULL) - 300; /* 5 minutes */

    entry = gInterAppState.processList;
    while (entry) {
        next = entry->next;

        /* Check if process is still active */
        if (entry->lastAccess < cutoffTime) {
            /* Process seems inactive - verify it's still running */
            if (entry->processID > 0) {
                if (kill(entry->processID, 0) != 0) {
                    /* Process is no longer running */
                    entry->isActive = false;
                }
            }
        }

        entry = next;
    }
}

static NotificationEntry *FindNotificationEntry(ScrapChangeCallback callback)
{
    NotificationEntry *entry;

    entry = gInterAppState.notifications;
    while (entry) {
        if (entry->callback == callback) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static pid_t GetProcessID(const ProcessSerialNumber *psn)
{
    /* In a full implementation, this would convert PSN to PID */
    /* For now, return current process ID */
    return getpid();
}

static OSErr GetProcessName(const ProcessSerialNumber *psn, Str255 name)
{
    /* In a full implementation, this would get the actual process name */
    /* For now, provide a generic name */

    const char *defaultName = "Unknown Process";
    size_t nameLen = strlen(defaultName);

    if (nameLen > 255) nameLen = 255;

    name[0] = (unsigned char)nameLen;
    memcpy(&name[1], defaultName, nameLen);

    return noErr;
}

/*
 * Cleanup Functions
 */

void CleanupInterAppScrap(void)
{
    ProcessEntry *processEntry, *nextProcess;
    NotificationEntry *notifyEntry, *nextNotify;

    if (!gInterAppInitialized) {
        return;
    }

    /* Free process list */
    processEntry = gInterAppState.processList;
    while (processEntry) {
        nextProcess = processEntry->next;
        free(processEntry);
        processEntry = nextProcess;
    }

    /* Free notification list */
    notifyEntry = gInterAppState.notifications;
    while (notifyEntry) {
        nextNotify = notifyEntry->next;
        free(notifyEntry);
        notifyEntry = nextNotify;
    }

    memset(&gInterAppState, 0, sizeof(gInterAppState));
    gInterAppInitialized = false;
}
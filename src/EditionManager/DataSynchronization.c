/*
 * DataSynchronization.c
 *
 * Data synchronization and live updating for Edition Manager
 * Handles real-time data sharing, change detection, and automatic updates
 *
 * This module provides the core functionality for keeping publisher and
 * subscriber data synchronized across the system
 */

#include "EditionManager/EditionManager.h"
#include "EditionManager/EditionManagerPrivate.h"
#include "EditionManager/DataSynchronization.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>

/* Synchronization state management */
typedef struct SyncState {
    SectionHandle section;          /* Associated section */
    TimeStamp lastSyncTime;         /* Last synchronization time */
    TimeStamp lastModTime;          /* Last modification time */
    uint32_t syncFlags;             /* Synchronization flags */
    bool isDirty;                   /* Data needs synchronization */
    bool isActive;                  /* Synchronization is active */
    pthread_mutex_t syncMutex;      /* Thread safety */

    /* Format-specific sync info */
    FormatSyncInfo* formatInfo;     /* Per-format sync information */
    int32_t formatCount;            /* Number of tracked formats */

    /* Change detection */
    uint32_t changeSequence;        /* Change sequence number */
    Handle changeBuffer;            /* Buffer for detecting changes */
    Size changeBufferSize;          /* Size of change buffer */

    struct SyncState* next;         /* Linked list */
} SyncState;

/* Global synchronization state */
static SyncState* gSyncStateList = NULL;
static pthread_mutex_t gSyncListMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t gSyncThread = 0;
static bool gSyncThreadRunning = false;
static int32_t gSyncInterval = 1000;  /* milliseconds */

/* Internal helper functions */
static SyncState* FindSyncState(SectionHandle sectionH);
static SyncState* CreateSyncState(SectionHandle sectionH);
static void DestroySyncState(SyncState* syncState);
static OSErr DetectDataChanges(SyncState* syncState, bool* hasChanges);
static OSErr SynchronizeSection(SyncState* syncState);
static void* SyncThreadProc(void* param);
static OSErr NotifyDataChange(SectionHandle sectionH, FormatType format);
static OSErr UpdateChangeSequence(SyncState* syncState);

/*
 * InitDataSharingPlatform
 *
 * Initialize the data synchronization system.
 */
OSErr InitDataSharingPlatform(void)
{
    /* Initialize global mutex */
    pthread_mutex_init(&gSyncListMutex, NULL);

    /* Start synchronization thread */
    if (!gSyncThreadRunning) {
        gSyncThreadRunning = true;
        int result = pthread_create(&gSyncThread, NULL, SyncThreadProc, NULL);
        if (result != 0) {
            gSyncThreadRunning = false;
            return editionMgrInitErr;
        }
    }

    return noErr;
}

/*
 * CleanupDataSharingPlatform
 *
 * Clean up the data synchronization system.
 */
void CleanupDataSharingPlatform(void)
{
    /* Stop synchronization thread */
    if (gSyncThreadRunning) {
        gSyncThreadRunning = false;
        pthread_join(gSyncThread, NULL);
        gSyncThread = 0;
    }

    /* Clean up all sync states */
    pthread_mutex_lock(&gSyncListMutex);

    SyncState* current = gSyncStateList;
    while (current) {
        SyncState* next = current->next;
        DestroySyncState(current);
        current = next;
    }
    gSyncStateList = NULL;

    pthread_mutex_unlock(&gSyncListMutex);
    pthread_mutex_destroy(&gSyncListMutex);
}

/*
 * RegisterSectionWithPlatform
 *
 * Register a section for data synchronization.
 */
OSErr RegisterSectionWithPlatform(SectionHandle sectionH, const FSSpec* document)
{
    if (!sectionH || !document) {
        return badSectionErr;
    }

    /* Create synchronization state for this section */
    SyncState* syncState = CreateSyncState(sectionH);
    if (!syncState) {
        return editionMgrInitErr;
    }

    /* Initialize synchronization for the section's data formats */
    OSErr err = InitializeSectionFormats(syncState);
    if (err != noErr) {
        DestroySyncState(syncState);
        return err;
    }

    /* Start monitoring for changes */
    syncState->isActive = true;

    return noErr;
}

/*
 * UnregisterSectionFromPlatform
 *
 * Unregister a section from data synchronization.
 */
void UnregisterSectionFromPlatform(SectionHandle sectionH)
{
    if (!sectionH) {
        return;
    }

    pthread_mutex_lock(&gSyncListMutex);

    SyncState* syncState = FindSyncState(sectionH);
    if (syncState) {
        /* Remove from list */
        if (syncState == gSyncStateList) {
            gSyncStateList = syncState->next;
        } else {
            SyncState* prev = gSyncStateList;
            while (prev && prev->next != syncState) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = syncState->next;
            }
        }

        DestroySyncState(syncState);
    }

    pthread_mutex_unlock(&gSyncListMutex);
}

/*
 * SynchronizePublisherData
 *
 * Synchronize publisher data with its edition container.
 */
OSErr SynchronizePublisherData(SectionHandle publisherH)
{
    if (!publisherH) {
        return badSectionErr;
    }

    SyncState* syncState = FindSyncState(publisherH);
    if (!syncState) {
        return badSectionErr;
    }

    /* Check if data has changed */
    bool hasChanges = false;
    OSErr err = DetectDataChanges(syncState, &hasChanges);
    if (err != noErr) {
        return err;
    }

    /* If no changes, check if forced sync is needed */
    if (!hasChanges && !syncState->isDirty) {
        return noErr;
    }

    /* Perform synchronization */
    err = SynchronizeSection(syncState);
    if (err == noErr) {
        syncState->lastSyncTime = GetCurrentTimeStamp();
        syncState->isDirty = false;

        /* Notify subscribers of the change */
        NotifySubscribers(publisherH);
    }

    return err;
}

/*
 * SynchronizeSubscriberData
 *
 * Synchronize subscriber data from its edition container.
 */
OSErr SynchronizeSubscriberData(SectionHandle subscriberH)
{
    if (!subscriberH) {
        return badSectionErr;
    }

    SyncState* syncState = FindSyncState(subscriberH);
    if (!syncState) {
        return badSectionErr;
    }

    /* Check if subscriber is in automatic update mode */
    if ((*subscriberH)->mode != sumAutomatic) {
        return noErr;  /* Manual mode - don't auto-sync */
    }

    /* Check if edition has been updated */
    EditionContainerSpec* container = (EditionContainerSpec*)(*subscriberH)->controlBlock;
    if (!container) {
        return badSectionErr;
    }

    TimeStamp editionModTime;
    OSErr err = GetEditionModificationTime(container, &editionModTime);
    if (err != noErr) {
        return err;
    }

    /* Only sync if edition is newer than our last sync */
    if (editionModTime <= syncState->lastSyncTime) {
        return noErr;
    }

    /* Perform synchronization */
    err = SynchronizeSection(syncState);
    if (err == noErr) {
        syncState->lastSyncTime = GetCurrentTimeStamp();

        /* Notify application of the update */
        NotifyDataChange(subscriberH, 0);  /* General change notification */
    }

    return err;
}

/*
 * NotifySubscribers
 *
 * Notify all subscribers that publisher data has changed.
 */
OSErr NotifySubscribers(SectionHandle publisherH)
{
    if (!publisherH || (*publisherH)->kind != stPublisher) {
        return badSectionErr;
    }

    /* Find all subscribers for this publisher's edition */
    EditionContainerSpec* container = (EditionContainerSpec*)(*publisherH)->controlBlock;
    if (!container) {
        return badSectionErr;
    }

    /* Search for subscribers using the same edition container */
    pthread_mutex_lock(&gSyncListMutex);

    SyncState* current = gSyncStateList;
    while (current) {
        if (current->section != publisherH &&
            (*current->section)->kind == stSubscriber) {

            /* Check if this subscriber uses the same edition */
            EditionContainerSpec* subContainer = (EditionContainerSpec*)(*current->section)->controlBlock;
            if (subContainer && CompareFSSpec(&container->theFile, &subContainer->theFile)) {
                /* Mark subscriber for update */
                current->isDirty = true;

                /* Trigger immediate sync if in automatic mode */
                if ((*current->section)->mode == sumAutomatic) {
                    SynchronizeSubscriberData(current->section);
                }
            }
        }
        current = current->next;
    }

    pthread_mutex_unlock(&gSyncListMutex);
    return noErr;
}

/*
 * ProcessDataSharingBackground
 *
 * Process background data sharing tasks.
 */
OSErr ProcessDataSharingBackground(void)
{
    static TimeStamp lastBackgroundTime = 0;
    TimeStamp currentTime = GetCurrentTimeStamp();

    /* Only process background tasks periodically */
    if (currentTime - lastBackgroundTime < 5) {  /* 5 seconds */
        return noErr;
    }
    lastBackgroundTime = currentTime;

    pthread_mutex_lock(&gSyncListMutex);

    SyncState* current = gSyncStateList;
    while (current) {
        if (current->isActive) {
            /* Check for changes and sync if needed */
            bool hasChanges = false;
            DetectDataChanges(current, &hasChanges);

            if (hasChanges || current->isDirty) {
                SynchronizeSection(current);
                current->lastSyncTime = currentTime;
                current->isDirty = false;
            }
        }
        current = current->next;
    }

    pthread_mutex_unlock(&gSyncListMutex);
    return noErr;
}

/*
 * SetSynchronizationMode
 *
 * Set synchronization mode for a section.
 */
OSErr SetSynchronizationMode(SectionHandle sectionH, SyncMode mode)
{
    if (!sectionH) {
        return badSectionErr;
    }

    SyncState* syncState = FindSyncState(sectionH);
    if (!syncState) {
        return badSectionErr;
    }

    pthread_mutex_lock(&syncState->syncMutex);

    switch (mode) {
        case kSyncAutomatic:
            syncState->syncFlags |= kSyncFlagAutomatic;
            syncState->isActive = true;
            break;

        case kSyncManual:
            syncState->syncFlags &= ~kSyncFlagAutomatic;
            break;

        case kSyncDisabled:
            syncState->isActive = false;
            break;

        case kSyncRealtime:
            syncState->syncFlags |= (kSyncFlagAutomatic | kSyncFlagRealtime);
            syncState->isActive = true;
            break;
    }

    pthread_mutex_unlock(&syncState->syncMutex);
    return noErr;
}

/*
 * ForceSynchronization
 *
 * Force immediate synchronization of a section.
 */
OSErr ForceSynchronization(SectionHandle sectionH)
{
    if (!sectionH) {
        return badSectionErr;
    }

    SyncState* syncState = FindSyncState(sectionH);
    if (!syncState) {
        return badSectionErr;
    }

    pthread_mutex_lock(&syncState->syncMutex);

    OSErr err = SynchronizeSection(syncState);
    if (err == noErr) {
        syncState->lastSyncTime = GetCurrentTimeStamp();
        syncState->isDirty = false;
        UpdateChangeSequence(syncState);
    }

    pthread_mutex_unlock(&syncState->syncMutex);
    return err;
}

/* Internal helper functions */

static SyncState* FindSyncState(SectionHandle sectionH)
{
    SyncState* current = gSyncStateList;
    while (current) {
        if (current->section == sectionH) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static SyncState* CreateSyncState(SectionHandle sectionH)
{
    SyncState* syncState = (SyncState*)malloc(sizeof(SyncState));
    if (!syncState) {
        return NULL;
    }

    memset(syncState, 0, sizeof(SyncState));
    syncState->section = sectionH;
    syncState->lastSyncTime = 0;
    syncState->lastModTime = 0;
    syncState->syncFlags = kSyncFlagAutomatic;
    syncState->isDirty = false;
    syncState->isActive = false;
    syncState->formatInfo = NULL;
    syncState->formatCount = 0;
    syncState->changeSequence = 0;
    syncState->changeBuffer = NULL;
    syncState->changeBufferSize = 0;

    pthread_mutex_init(&syncState->syncMutex, NULL);

    /* Add to global list */
    pthread_mutex_lock(&gSyncListMutex);
    syncState->next = gSyncStateList;
    gSyncStateList = syncState;
    pthread_mutex_unlock(&gSyncListMutex);

    return syncState;
}

static void DestroySyncState(SyncState* syncState)
{
    if (!syncState) {
        return;
    }

    /* Free format info */
    if (syncState->formatInfo) {
        free(syncState->formatInfo);
    }

    /* Free change buffer */
    if (syncState->changeBuffer) {
        DisposeHandleData(syncState->changeBuffer);
    }

    pthread_mutex_destroy(&syncState->syncMutex);
    free(syncState);
}

static OSErr DetectDataChanges(SyncState* syncState, bool* hasChanges)
{
    *hasChanges = false;

    if (!syncState->section) {
        return badSectionErr;
    }

    /* For publishers, check if source data has changed */
    if ((*syncState->section)->kind == stPublisher) {
        /* Get current modification time from section */
        TimeStamp currentModTime = (*syncState->section)->mdDate;

        if (currentModTime > syncState->lastModTime) {
            *hasChanges = true;
            syncState->lastModTime = currentModTime;
        }
    }
    /* For subscribers, check if edition has been updated */
    else if ((*syncState->section)->kind == stSubscriber) {
        EditionContainerSpec* container = (EditionContainerSpec*)(*syncState->section)->controlBlock;
        if (container) {
            TimeStamp editionModTime;
            OSErr err = GetEditionModificationTime(container, &editionModTime);
            if (err == noErr && editionModTime > syncState->lastSyncTime) {
                *hasChanges = true;
            }
        }
    }

    return noErr;
}

static OSErr SynchronizeSection(SyncState* syncState)
{
    if (!syncState->section) {
        return badSectionErr;
    }

    OSErr err = noErr;

    /* Handle publisher synchronization */
    if ((*syncState->section)->kind == stPublisher) {
        /* Synchronize publisher data to edition container */
        err = SyncPublisherToEdition(syncState);
    }
    /* Handle subscriber synchronization */
    else if ((*syncState->section)->kind == stSubscriber) {
        /* Synchronize edition data to subscriber */
        err = SyncEditionToSubscriber(syncState);
    }

    if (err == noErr) {
        UpdateChangeSequence(syncState);
    }

    return err;
}

static void* SyncThreadProc(void* param)
{
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = gSyncInterval * 1000000;  /* Convert ms to ns */

    while (gSyncThreadRunning) {
        /* Process background synchronization */
        ProcessDataSharingBackground();

        /* Sleep for the specified interval */
        nanosleep(&sleepTime, NULL);
    }

    return NULL;
}

static OSErr NotifyDataChange(SectionHandle sectionH, FormatType format)
{
    /* Post section event for data change */
    AppRefNum targetApp = NULL;
    GetCurrentAppRefNum(&targetApp);

    ResType eventClass = (format == 0) ? sectionEventMsgClass : format;

    return PostSectionEvent(sectionH, targetApp, eventClass);
}

static OSErr UpdateChangeSequence(SyncState* syncState)
{
    syncState->changeSequence++;
    return noErr;
}
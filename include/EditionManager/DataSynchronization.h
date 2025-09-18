/*
 * DataSynchronization.h
 *
 * Data Synchronization API for Edition Manager
 * Handles real-time data sharing, change detection, and automatic updates
 */

#ifndef __DATA_SYNCHRONIZATION_H__
#define __DATA_SYNCHRONIZATION_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Synchronization Modes
 */
typedef enum {
    kSyncAutomatic,     /* Automatically sync when data changes */
    kSyncManual,        /* Only sync when explicitly requested */
    kSyncDisabled,      /* No synchronization */
    kSyncRealtime       /* Real-time synchronization (immediate) */
} SyncMode;

/*
 * Synchronization Flags
 */
enum {
    kSyncFlagAutomatic = 0x0001,    /* Automatic synchronization enabled */
    kSyncFlagRealtime = 0x0002,     /* Real-time synchronization */
    kSyncFlagBidirectional = 0x0004, /* Two-way synchronization */
    kSyncFlagCompressed = 0x0008,   /* Compress data during sync */
    kSyncFlagEncrypted = 0x0010,    /* Encrypt data during sync */
    kSyncFlagVersioned = 0x0020,    /* Maintain version history */
    kSyncFlagConflictDetection = 0x0040 /* Enable conflict detection */
};

/*
 * Format-specific synchronization information
 */
typedef struct {
    FormatType formatType;          /* Format identifier */
    TimeStamp lastSyncTime;         /* Last synchronization time */
    uint32_t syncSequence;          /* Synchronization sequence number */
    uint32_t syncFlags;             /* Format-specific sync flags */
    Size dataSize;                  /* Current data size */
    uint32_t checksum;              /* Data checksum for change detection */
    bool isDirty;                   /* Data needs synchronization */
} FormatSyncInfo;

/*
 * Synchronization statistics
 */
typedef struct {
    uint32_t syncCount;             /* Number of synchronizations */
    uint32_t successCount;          /* Successful synchronizations */
    uint32_t failureCount;          /* Failed synchronizations */
    TimeStamp lastSyncTime;         /* Last synchronization time */
    TimeStamp lastFailureTime;      /* Last failure time */
    OSErr lastError;                /* Last error code */
    Size totalDataSynced;           /* Total data synchronized */
    uint32_t averageSyncTime;       /* Average sync time in ms */
} SyncStatistics;

/*
 * Conflict resolution strategies
 */
typedef enum {
    kConflictResolvePublisherWins,  /* Publisher data takes precedence */
    kConflictResolveSubscriberWins, /* Subscriber data takes precedence */
    kConflictResolveNewerWins,      /* Newer timestamp wins */
    kConflictResolveMerge,          /* Attempt to merge changes */
    kConflictResolvePromptUser      /* Ask user to resolve */
} ConflictResolution;

/*
 * Core Synchronization Functions
 */

/* Set synchronization mode for a section */
OSErr SetSynchronizationMode(SectionHandle sectionH, SyncMode mode);

/* Get current synchronization mode */
OSErr GetSynchronizationMode(SectionHandle sectionH, SyncMode* mode);

/* Force immediate synchronization */
OSErr ForceSynchronization(SectionHandle sectionH);

/* Check if section needs synchronization */
OSErr NeedsSynchronization(SectionHandle sectionH, bool* needsSync);

/* Get time since last synchronization */
OSErr GetTimeSinceLastSync(SectionHandle sectionH, uint32_t* seconds);

/*
 * Publisher Synchronization
 */

/* Synchronize publisher data to edition */
OSErr SyncPublisherToEdition(void* syncState);

/* Check if publisher data has changed */
OSErr CheckPublisherDataChanges(SectionHandle publisherH, bool* hasChanges);

/* Set publisher data change callback */
typedef void (*PublisherChangeCallback)(SectionHandle publisherH,
                                       FormatType changedFormat,
                                       void* userData);

OSErr SetPublisherChangeCallback(SectionHandle publisherH,
                                PublisherChangeCallback callback,
                                void* userData);

/* Enable/disable automatic publishing */
OSErr SetAutoPublishing(SectionHandle publisherH, bool enable);

/* Get auto-publishing status */
OSErr GetAutoPublishing(SectionHandle publisherH, bool* isEnabled);

/*
 * Subscriber Synchronization
 */

/* Synchronize edition data to subscriber */
OSErr SyncEditionToSubscriber(void* syncState);

/* Check for edition updates */
OSErr CheckEditionUpdates(SectionHandle subscriberH, bool* hasUpdates);

/* Set subscriber update callback */
typedef void (*SubscriberUpdateCallback)(SectionHandle subscriberH,
                                        FormatType updatedFormat,
                                        const void* newData,
                                        Size dataSize,
                                        void* userData);

OSErr SetSubscriberUpdateCallback(SectionHandle subscriberH,
                                 SubscriberUpdateCallback callback,
                                 void* userData);

/* Enable/disable automatic subscription updates */
OSErr SetAutoSubscription(SectionHandle subscriberH, bool enable);

/* Get auto-subscription status */
OSErr GetAutoSubscription(SectionHandle subscriberH, bool* isEnabled);

/*
 * Change Detection
 */

/* Initialize change detection for a section */
OSErr InitializeChangeDetection(SectionHandle sectionH);

/* Register data for change monitoring */
OSErr RegisterDataForMonitoring(SectionHandle sectionH,
                               FormatType format,
                               const void* data,
                               Size dataSize);

/* Check for data changes */
OSErr DetectDataChanges(SectionHandle sectionH,
                       FormatType format,
                       bool* hasChanged);

/* Get change detection information */
OSErr GetChangeInfo(SectionHandle sectionH,
                   FormatType format,
                   FormatSyncInfo* syncInfo);

/* Reset change detection state */
OSErr ResetChangeDetection(SectionHandle sectionH, FormatType format);

/*
 * Conflict Resolution
 */

/* Set conflict resolution strategy */
OSErr SetConflictResolution(SectionHandle sectionH, ConflictResolution strategy);

/* Get conflict resolution strategy */
OSErr GetConflictResolution(SectionHandle sectionH, ConflictResolution* strategy);

/* Resolve synchronization conflict */
OSErr ResolveConflict(SectionHandle sectionH,
                     FormatType format,
                     const void* publisherData,
                     Size publisherSize,
                     const void* subscriberData,
                     Size subscriberSize,
                     void** resolvedData,
                     Size* resolvedSize);

/* Check for synchronization conflicts */
OSErr CheckForConflicts(SectionHandle sectionH, bool* hasConflicts);

/*
 * Performance and Optimization
 */

/* Set synchronization interval */
OSErr SetSyncInterval(uint32_t intervalMs);

/* Get synchronization interval */
OSErr GetSyncInterval(uint32_t* intervalMs);

/* Enable/disable batched synchronization */
OSErr SetBatchedSync(bool enable);

/* Set sync batch size */
OSErr SetSyncBatchSize(uint32_t batchSize);

/* Enable/disable delta synchronization */
OSErr SetDeltaSync(SectionHandle sectionH, bool enable);

/* Get delta synchronization status */
OSErr GetDeltaSync(SectionHandle sectionH, bool* isEnabled);

/*
 * Statistics and Monitoring
 */

/* Get synchronization statistics */
OSErr GetSyncStatistics(SectionHandle sectionH, SyncStatistics* stats);

/* Reset synchronization statistics */
OSErr ResetSyncStatistics(SectionHandle sectionH);

/* Get global synchronization status */
OSErr GetGlobalSyncStatus(uint32_t* activeSections,
                         uint32_t* pendingSyncs,
                         uint32_t* failedSyncs);

/* Enable/disable sync logging */
OSErr SetSyncLogging(bool enable, const char* logFilePath);

/*
 * Network and Remote Synchronization
 */

/* Enable network synchronization */
OSErr EnableNetworkSync(SectionHandle sectionH, const char* remoteAddress);

/* Disable network synchronization */
OSErr DisableNetworkSync(SectionHandle sectionH);

/* Set network sync credentials */
OSErr SetNetworkCredentials(const char* username, const char* password);

/* Get network sync status */
OSErr GetNetworkSyncStatus(SectionHandle sectionH,
                          bool* isConnected,
                          char* remoteAddress,
                          Size addressSize);

/*
 * Version Management
 */

/* Enable version tracking */
OSErr EnableVersionTracking(SectionHandle sectionH);

/* Get version history */
OSErr GetVersionHistory(SectionHandle sectionH,
                       FormatType format,
                       TimeStamp** versions,
                       uint32_t* versionCount);

/* Revert to previous version */
OSErr RevertToVersion(SectionHandle sectionH,
                     FormatType format,
                     TimeStamp version);

/* Clean up old versions */
OSErr CleanupOldVersions(SectionHandle sectionH,
                        uint32_t keepCount);

/*
 * Advanced Synchronization Features
 */

/* Set custom synchronization filter */
typedef bool (*SyncFilterCallback)(SectionHandle sectionH,
                                  FormatType format,
                                  const void* data,
                                  Size dataSize,
                                  void* userData);

OSErr SetSyncFilter(SectionHandle sectionH,
                   SyncFilterCallback filter,
                   void* userData);

/* Set data transformation callback */
typedef OSErr (*DataTransformCallback)(const void* inputData,
                                      Size inputSize,
                                      void** outputData,
                                      Size* outputSize,
                                      void* userData);

OSErr SetDataTransform(SectionHandle sectionH,
                      FormatType format,
                      DataTransformCallback transform,
                      void* userData);

/* Enable/disable compression during sync */
OSErr SetSyncCompression(SectionHandle sectionH, bool enable);

/* Enable/disable encryption during sync */
OSErr SetSyncEncryption(SectionHandle sectionH,
                       bool enable,
                       const char* encryptionKey);

/*
 * Platform Integration Functions
 */

/* Associate with platform data sharing mechanism */
OSErr AssociateSectionWithPlatform(SectionHandle sectionH, const FSSpec* document);

/* Get edition modification time */
OSErr GetEditionModificationTime(const EditionContainerSpec* container,
                                TimeStamp* modTime);

/* Compare file specifications */
bool CompareFSSpec(const FSSpec* spec1, const FSSpec* spec2);

/* Initialize section formats for sync */
OSErr InitializeSectionFormats(void* syncState);

/*
 * Constants
 */

#define kDefaultSyncInterval 1000       /* 1 second */
#define kMinSyncInterval 100           /* 100ms minimum */
#define kMaxSyncInterval 60000         /* 60 seconds maximum */
#define kDefaultBatchSize 10           /* Default sync batch size */
#define kMaxVersionHistory 100         /* Maximum versions to keep */

/* Sync priority levels */
enum {
    kSyncPriorityLow = 1,
    kSyncPriorityNormal = 2,
    kSyncPriorityHigh = 3,
    kSyncPriorityCritical = 4
};

/* Error codes for synchronization */
enum {
    syncConflictErr = -480,
    syncTimeoutErr = -481,
    syncNetworkErr = -482,
    syncVersionErr = -483,
    syncCorruptionErr = -484
};

#ifdef __cplusplus
}
#endif

#endif /* __DATA_SYNCHRONIZATION_H__ */
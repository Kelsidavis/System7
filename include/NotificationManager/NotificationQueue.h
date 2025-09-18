#ifndef NOTIFICATION_QUEUE_H
#define NOTIFICATION_QUEUE_H

#include "NotificationManager/NotificationManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Queue Management Constants */
#define NM_QUEUE_MAX_SIZE       100         /* Maximum queue size */
#define NM_QUEUE_DEFAULT_SIZE   50          /* Default queue size */
#define NM_QUEUE_COMPACT_THRESHOLD  75      /* Compact when 75% full */

/* Queue Statistics */
typedef struct {
    short   totalCount;         /* Total notifications in queue */
    short   pendingCount;       /* Pending notifications */
    short   displayedCount;     /* Currently displayed notifications */
    short   priorityCount[4];   /* Count by priority level */
    UInt32  oldestTimestamp;    /* Timestamp of oldest notification */
    UInt32  newestTimestamp;    /* Timestamp of newest notification */
    short   maxSizeReached;     /* Maximum queue size reached */
    UInt32  totalProcessed;     /* Total notifications processed */
} NMQueueStats;

/* Queue Configuration */
typedef struct {
    short   maxSize;            /* Maximum queue size */
    Boolean autoCompact;        /* Automatically compact queue */
    Boolean priorityOrdering;   /* Maintain priority order */
    Boolean fifoWithinPriority; /* FIFO within same priority */
    UInt32  maxAge;             /* Maximum age before auto-removal */
    Boolean allowDuplicates;    /* Allow duplicate notifications */
} NMQueueConfig;

/* Queue Management Functions */
OSErr NMQueueInit(void);
void NMQueueCleanup(void);

/* Queue Operations */
OSErr NMInsertInQueue(NMExtendedRecPtr nmExtPtr);
OSErr NMRemoveFromQueue(NMExtendedRecPtr nmExtPtr);
OSErr NMFindInQueue(NMRecPtr nmReqPtr, NMExtendedRecPtr *nmExtPtr);

/* Queue Manipulation */
OSErr NMFlushQueue(void);
OSErr NMFlushCategory(StringPtr category);
OSErr NMFlushApplication(OSType appSignature);
OSErr NMFlushPriority(NMPriority priority);
OSErr NMFlushOlderThan(UInt32 timestamp);

/* Queue Configuration */
OSErr NMGetQueueConfig(NMQueueConfig *config);
OSErr NMSetQueueConfig(const NMQueueConfig *config);
OSErr NMGetQueueStatus(short *count, short *maxSize);
OSErr NMGetQueueStats(NMQueueStats *stats);

/* Queue Navigation */
NMExtendedRecPtr NMGetFirstInQueue(void);
NMExtendedRecPtr NMGetNextInQueue(NMExtendedRecPtr current);
NMExtendedRecPtr NMGetLastInQueue(void);
NMExtendedRecPtr NMGetPreviousInQueue(NMExtendedRecPtr current);

/* Priority-based Access */
NMExtendedRecPtr NMGetHighestPriority(void);
NMExtendedRecPtr NMGetNextByPriority(NMPriority priority);
short NMCountByPriority(NMPriority priority);

/* Queue Maintenance */
OSErr NMCompactQueue(void);
OSErr NMValidateQueue(void);
OSErr NMRemoveExpired(void);
OSErr NMReorderByPriority(void);

/* Search Functions */
NMExtendedRecPtr NMFindByID(short notificationID);
NMExtendedRecPtr NMFindByRefCon(long refCon);
NMExtendedRecPtr NMFindByCategory(StringPtr category);
NMExtendedRecPtr NMFindByApplication(OSType appSignature);

/* Queue Enumeration */
typedef OSErr (*NMQueueEnumProc)(NMExtendedRecPtr nmExtPtr, void *context);
OSErr NMEnumerateQueue(NMQueueEnumProc enumProc, void *context);
OSErr NMEnumerateByPriority(NMPriority priority, NMQueueEnumProc enumProc, void *context);
OSErr NMEnumerateByStatus(NMStatus status, NMQueueEnumProc enumProc, void *context);

/* Internal Queue Management */
void NMQueueInsertSorted(NMExtendedRecPtr nmExtPtr);
void NMQueueRemoveElement(NMExtendedRecPtr nmExtPtr);
Boolean NMQueueIsFull(void);
Boolean NMQueueIsEmpty(void);
short NMQueueGetCount(void);

/* Queue Integrity */
OSErr NMQueueVerifyIntegrity(void);
OSErr NMQueueRepair(void);
void NMQueueUpdateStats(void);

#ifdef __cplusplus
}
#endif

#endif /* NOTIFICATION_QUEUE_H */
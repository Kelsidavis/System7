/*
 * NotificationQueue.c
 *
 * Notification queue management and priority handling
 * Manages the queue of pending and active notifications
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/NotificationQueue.h"
#include "NotificationManager/NotificationManager.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"

/* External globals from core */
extern NMGlobals gNMGlobals;

/* Local queue management state */
static NMQueueConfig gQueueConfig = {0};
static NMQueueStats gQueueStats = {0};
static Boolean gQueueInitialized = false;

/* Internal function prototypes */
static OSErr NMQueueGrow(void);
static void NMQueueShrink(void);
static Boolean NMShouldCompact(void);
static void NMUpdateQueueStats(void);
static int NMCompareByPriority(NMExtendedRecPtr a, NMExtendedRecPtr b);
static Boolean NMIsDuplicate(NMExtendedRecPtr nmExtPtr);

/*
 * Queue Initialization and Cleanup
 */

OSErr NMQueueInit(void)
{
    if (gQueueInitialized) {
        return noErr;
    }

    /* Initialize queue configuration */
    gQueueConfig.maxSize = NM_QUEUE_DEFAULT_SIZE;
    gQueueConfig.autoCompact = true;
    gQueueConfig.priorityOrdering = true;
    gQueueConfig.fifoWithinPriority = true;
    gQueueConfig.maxAge = 3600; /* 1 minute (60 ticks/second) */
    gQueueConfig.allowDuplicates = false;

    /* Initialize queue statistics */
    gQueueStats.totalCount = 0;
    gQueueStats.pendingCount = 0;
    gQueueStats.displayedCount = 0;
    for (int i = 0; i < 4; i++) {
        gQueueStats.priorityCount[i] = 0;
    }
    gQueueStats.oldestTimestamp = 0;
    gQueueStats.newestTimestamp = 0;
    gQueueStats.maxSizeReached = 0;
    gQueueStats.totalProcessed = 0;

    /* Initialize the queue header */
    gNMGlobals.nmQueue.qFlags = 0;
    gNMGlobals.nmQueue.qHead = NULL;
    gNMGlobals.nmQueue.qTail = NULL;
    gNMGlobals.nmCurrentSize = 0;

    gQueueInitialized = true;
    return noErr;
}

void NMQueueCleanup(void)
{
    if (!gQueueInitialized) {
        return;
    }

    /* Flush all notifications */
    NMFlushQueue();

    gQueueInitialized = false;
}

/*
 * Queue Operations
 */

OSErr NMInsertInQueue(NMExtendedRecPtr nmExtPtr)
{
    if (!gQueueInitialized || !nmExtPtr) {
        return nmErrInvalidParameter;
    }

    /* Check for duplicates if not allowed */
    if (!gQueueConfig.allowDuplicates && NMIsDuplicate(nmExtPtr)) {
        return nmErrInUse;
    }

    /* Check if queue is full */
    if (NMQueueIsFull()) {
        if (gQueueConfig.autoCompact && NMShouldCompact()) {
            NMCompactQueue();
        }

        if (NMQueueIsFull()) {
            return nmErrQueueFull;
        }
    }

    /* Set queue element fields */
    nmExtPtr->base.qLink = NULL;
    nmExtPtr->base.qType = nmType;

    /* Insert based on configuration */
    if (gQueueConfig.priorityOrdering) {
        NMQueueInsertSorted(nmExtPtr);
    } else {
        /* Simple FIFO insertion */
        Enqueue((QElemPtr)nmExtPtr, &gNMGlobals.nmQueue);
    }

    /* Update counts and statistics */
    gNMGlobals.nmCurrentSize++;
    gQueueStats.totalCount++;
    gQueueStats.priorityCount[nmExtPtr->priority]++;

    if (nmExtPtr->status == nmStatusPending) {
        gQueueStats.pendingCount++;
    }

    /* Update timestamp tracking */
    if (gQueueStats.oldestTimestamp == 0 ||
        nmExtPtr->timestamp < gQueueStats.oldestTimestamp) {
        gQueueStats.oldestTimestamp = nmExtPtr->timestamp;
    }

    if (nmExtPtr->timestamp > gQueueStats.newestTimestamp) {
        gQueueStats.newestTimestamp = nmExtPtr->timestamp;
    }

    /* Update maximum size reached */
    if (gNMGlobals.nmCurrentSize > gQueueStats.maxSizeReached) {
        gQueueStats.maxSizeReached = gNMGlobals.nmCurrentSize;
    }

    return noErr;
}

OSErr NMRemoveFromQueue(NMExtendedRecPtr nmExtPtr)
{
    OSErr err;

    if (!gQueueInitialized || !nmExtPtr) {
        return nmErrInvalidParameter;
    }

    /* Remove from queue */
    err = Dequeue((QElemPtr)nmExtPtr, &gNMGlobals.nmQueue);
    if (err != noErr) {
        return nmErrNotFound;
    }

    /* Update counts */
    gNMGlobals.nmCurrentSize--;
    gQueueStats.totalCount--;
    gQueueStats.priorityCount[nmExtPtr->priority]--;

    if (nmExtPtr->status == nmStatusPending) {
        gQueueStats.pendingCount--;
    } else if (nmExtPtr->status == nmStatusDisplayed) {
        gQueueStats.displayedCount--;
    }

    gQueueStats.totalProcessed++;

    /* Update statistics */
    NMUpdateQueueStats();

    return noErr;
}

OSErr NMFindInQueue(NMRecPtr nmReqPtr, NMExtendedRecPtr *nmExtPtr)
{
    QElemPtr qElem;

    if (!gQueueInitialized || !nmReqPtr || !nmExtPtr) {
        return nmErrInvalidParameter;
    }

    *nmExtPtr = NULL;

    /* Search through the queue */
    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        NMExtendedRecPtr current = (NMExtendedRecPtr)qElem;

        /* Check if this is the record we're looking for */
        if (&current->base == nmReqPtr) {
            *nmExtPtr = current;
            return noErr;
        }

        qElem = qElem->qLink;
    }

    return nmErrNotFound;
}

/*
 * Queue Manipulation
 */

OSErr NMFlushQueue(void)
{
    QElemPtr qElem, nextElem;
    NMExtendedRecPtr nmExtPtr;

    if (!gQueueInitialized) {
        return nmErrNotInstalled;
    }

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        /* Remove from queue */
        Dequeue(qElem, &gNMGlobals.nmQueue);

        /* Remove from platform system if displayed */
        if (nmExtPtr->status >= nmStatusDisplayed) {
            NMPlatformRemoveNotification(nmExtPtr);
        }

        /* Clean up resources */
        if (nmExtPtr->category) {
            DisposePtr((Ptr)nmExtPtr->category);
        }
        if (nmExtPtr->richContent) {
            DisposeHandle(nmExtPtr->richContent);
        }
        if (!nmExtPtr->persistent) {
            DisposePtr((Ptr)nmExtPtr);
        }

        qElem = nextElem;
    }

    /* Reset counts and statistics */
    gNMGlobals.nmCurrentSize = 0;
    gQueueStats.totalCount = 0;
    gQueueStats.pendingCount = 0;
    gQueueStats.displayedCount = 0;
    for (int i = 0; i < 4; i++) {
        gQueueStats.priorityCount[i] = 0;
    }

    return noErr;
}

OSErr NMFlushCategory(StringPtr category)
{
    QElemPtr qElem, nextElem;
    NMExtendedRecPtr nmExtPtr;

    if (!gQueueInitialized || !category) {
        return nmErrInvalidParameter;
    }

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        /* Check if this notification matches the category */
        if (nmExtPtr->category &&
            EqualString(nmExtPtr->category, category, false, true)) {

            /* Remove this notification */
            NMRemoveFromQueue(nmExtPtr);

            if (nmExtPtr->status >= nmStatusDisplayed) {
                NMPlatformRemoveNotification(nmExtPtr);
            }

            if (!nmExtPtr->persistent) {
                DisposePtr((Ptr)nmExtPtr);
            }
        }

        qElem = nextElem;
    }

    return noErr;
}

OSErr NMFlushApplication(OSType appSignature)
{
    QElemPtr qElem, nextElem;
    NMExtendedRecPtr nmExtPtr;
    OSType currentApp;

    if (!gQueueInitialized) {
        return nmErrInvalidParameter;
    }

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        /* Get application signature from refcon or other means */
        currentApp = (OSType)(nmExtPtr->base.nmRefCon >> 16);

        if (currentApp == appSignature) {
            NMRemoveFromQueue(nmExtPtr);

            if (nmExtPtr->status >= nmStatusDisplayed) {
                NMPlatformRemoveNotification(nmExtPtr);
            }

            if (!nmExtPtr->persistent) {
                DisposePtr((Ptr)nmExtPtr);
            }
        }

        qElem = nextElem;
    }

    return noErr;
}

OSErr NMFlushPriority(NMPriority priority)
{
    QElemPtr qElem, nextElem;
    NMExtendedRecPtr nmExtPtr;

    if (!gQueueInitialized) {
        return nmErrInvalidParameter;
    }

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        if (nmExtPtr->priority == priority) {
            NMRemoveFromQueue(nmExtPtr);

            if (nmExtPtr->status >= nmStatusDisplayed) {
                NMPlatformRemoveNotification(nmExtPtr);
            }

            if (!nmExtPtr->persistent) {
                DisposePtr((Ptr)nmExtPtr);
            }
        }

        qElem = nextElem;
    }

    return noErr;
}

OSErr NMFlushOlderThan(UInt32 timestamp)
{
    QElemPtr qElem, nextElem;
    NMExtendedRecPtr nmExtPtr;

    if (!gQueueInitialized) {
        return nmErrInvalidParameter;
    }

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        if (nmExtPtr->timestamp < timestamp) {
            NMRemoveFromQueue(nmExtPtr);

            if (nmExtPtr->status >= nmStatusDisplayed) {
                NMPlatformRemoveNotification(nmExtPtr);
            }

            if (!nmExtPtr->persistent) {
                DisposePtr((Ptr)nmExtPtr);
            }
        }

        qElem = nextElem;
    }

    return noErr;
}

/*
 * Queue Configuration and Status
 */

OSErr NMGetQueueConfig(NMQueueConfig *config)
{
    if (!gQueueInitialized || !config) {
        return nmErrInvalidParameter;
    }

    *config = gQueueConfig;
    return noErr;
}

OSErr NMSetQueueConfig(const NMQueueConfig *config)
{
    if (!gQueueInitialized || !config) {
        return nmErrInvalidParameter;
    }

    /* Validate configuration */
    if (config->maxSize < 1 || config->maxSize > NM_QUEUE_MAX_SIZE) {
        return nmErrInvalidParameter;
    }

    gQueueConfig = *config;
    gNMGlobals.nmMaxQueueSize = config->maxSize;

    /* If we're now over the limit, compact the queue */
    if (gNMGlobals.nmCurrentSize > config->maxSize) {
        NMCompactQueue();
    }

    return noErr;
}

OSErr NMGetQueueStatus(short *count, short *maxSize)
{
    if (!gQueueInitialized) {
        return nmErrNotInstalled;
    }

    if (count) {
        *count = gNMGlobals.nmCurrentSize;
    }

    if (maxSize) {
        *maxSize = gNMGlobals.nmMaxQueueSize;
    }

    return noErr;
}

OSErr NMGetQueueStats(NMQueueStats *stats)
{
    if (!gQueueInitialized || !stats) {
        return nmErrInvalidParameter;
    }

    NMUpdateQueueStats();
    *stats = gQueueStats;
    return noErr;
}

/*
 * Queue Navigation
 */

NMExtendedRecPtr NMGetFirstInQueue(void)
{
    if (!gQueueInitialized) {
        return NULL;
    }

    return (NMExtendedRecPtr)gNMGlobals.nmQueue.qHead;
}

NMExtendedRecPtr NMGetNextInQueue(NMExtendedRecPtr current)
{
    if (!gQueueInitialized || !current) {
        return NULL;
    }

    return (NMExtendedRecPtr)current->base.qLink;
}

NMExtendedRecPtr NMGetLastInQueue(void)
{
    if (!gQueueInitialized) {
        return NULL;
    }

    return (NMExtendedRecPtr)gNMGlobals.nmQueue.qTail;
}

NMExtendedRecPtr NMGetHighestPriority(void)
{
    QElemPtr qElem;
    NMExtendedRecPtr highest = NULL;
    NMPriority highestPriority = nmPriorityLow;

    if (!gQueueInitialized) {
        return NULL;
    }

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        NMExtendedRecPtr current = (NMExtendedRecPtr)qElem;

        if (current->priority > highestPriority ||
            (current->priority == highestPriority && highest == NULL)) {
            highest = current;
            highestPriority = current->priority;
        }

        qElem = qElem->qLink;
    }

    return highest;
}

short NMCountByPriority(NMPriority priority)
{
    if (!gQueueInitialized || priority < 0 || priority >= 4) {
        return 0;
    }

    return gQueueStats.priorityCount[priority];
}

/*
 * Internal Queue Management
 */

void NMQueueInsertSorted(NMExtendedRecPtr nmExtPtr)
{
    QElemPtr qElem, prevElem = NULL;
    NMExtendedRecPtr current;

    /* Find insertion point based on priority and timestamp */
    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        current = (NMExtendedRecPtr)qElem;

        /* Compare priorities and timestamps */
        if (NMCompareByPriority(nmExtPtr, current) > 0) {
            break;
        }

        prevElem = qElem;
        qElem = qElem->qLink;
    }

    /* Insert at the found position */
    nmExtPtr->base.qLink = qElem;

    if (prevElem) {
        prevElem->qLink = (QElemPtr)nmExtPtr;
    } else {
        gNMGlobals.nmQueue.qHead = (QElemPtr)nmExtPtr;
    }

    if (!qElem) {
        gNMGlobals.nmQueue.qTail = (QElemPtr)nmExtPtr;
    }
}

static int NMCompareByPriority(NMExtendedRecPtr a, NMExtendedRecPtr b)
{
    /* Higher priority goes first */
    if (a->priority > b->priority) {
        return 1;
    }
    if (a->priority < b->priority) {
        return -1;
    }

    /* Within same priority, FIFO ordering */
    if (gQueueConfig.fifoWithinPriority) {
        return (a->timestamp < b->timestamp) ? 1 : -1;
    }

    return 0;
}

Boolean NMQueueIsFull(void)
{
    return gNMGlobals.nmCurrentSize >= gNMGlobals.nmMaxQueueSize;
}

Boolean NMQueueIsEmpty(void)
{
    return gNMGlobals.nmCurrentSize == 0;
}

short NMQueueGetCount(void)
{
    return gNMGlobals.nmCurrentSize;
}

/*
 * Queue Maintenance
 */

OSErr NMCompactQueue(void)
{
    UInt32 currentTime;
    QElemPtr qElem, nextElem;
    NMExtendedRecPtr nmExtPtr;
    short removed = 0;

    if (!gQueueInitialized) {
        return nmErrNotInstalled;
    }

    currentTime = NMGetTimestamp();

    /* Remove old and completed notifications */
    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem && removed < (gNMGlobals.nmCurrentSize / 4)) {
        nextElem = qElem->qLink;
        nmExtPtr = (NMExtendedRecPtr)qElem;

        /* Remove if too old or completed */
        if ((nmExtPtr->status == nmStatusRemoved) ||
            (gQueueConfig.maxAge > 0 &&
             (currentTime - nmExtPtr->timestamp) > gQueueConfig.maxAge)) {

            NMRemoveFromQueue(nmExtPtr);

            if (nmExtPtr->status >= nmStatusDisplayed) {
                NMPlatformRemoveNotification(nmExtPtr);
            }

            if (!nmExtPtr->persistent) {
                DisposePtr((Ptr)nmExtPtr);
            }

            removed++;
        }

        qElem = nextElem;
    }

    NMUpdateQueueStats();
    return noErr;
}

static Boolean NMShouldCompact(void)
{
    float usage = (float)gNMGlobals.nmCurrentSize / gNMGlobals.nmMaxQueueSize;
    return usage >= (NM_QUEUE_COMPACT_THRESHOLD / 100.0f);
}

static void NMUpdateQueueStats(void)
{
    QElemPtr qElem;
    NMExtendedRecPtr nmExtPtr;

    /* Reset counters */
    gQueueStats.pendingCount = 0;
    gQueueStats.displayedCount = 0;
    for (int i = 0; i < 4; i++) {
        gQueueStats.priorityCount[i] = 0;
    }
    gQueueStats.oldestTimestamp = 0;
    gQueueStats.newestTimestamp = 0;

    /* Count current queue contents */
    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        nmExtPtr = (NMExtendedRecPtr)qElem;

        if (nmExtPtr->status == nmStatusPending) {
            gQueueStats.pendingCount++;
        } else if (nmExtPtr->status == nmStatusDisplayed) {
            gQueueStats.displayedCount++;
        }

        gQueueStats.priorityCount[nmExtPtr->priority]++;

        if (gQueueStats.oldestTimestamp == 0 ||
            nmExtPtr->timestamp < gQueueStats.oldestTimestamp) {
            gQueueStats.oldestTimestamp = nmExtPtr->timestamp;
        }

        if (nmExtPtr->timestamp > gQueueStats.newestTimestamp) {
            gQueueStats.newestTimestamp = nmExtPtr->timestamp;
        }

        qElem = qElem->qLink;
    }

    gQueueStats.totalCount = gNMGlobals.nmCurrentSize;
}

static Boolean NMIsDuplicate(NMExtendedRecPtr nmExtPtr)
{
    QElemPtr qElem;
    NMExtendedRecPtr current;

    qElem = gNMGlobals.nmQueue.qHead;
    while (qElem) {
        current = (NMExtendedRecPtr)qElem;

        /* Check for duplicate based on string content and refcon */
        if (current->base.nmRefCon == nmExtPtr->base.nmRefCon &&
            current->base.nmStr && nmExtPtr->base.nmStr &&
            EqualString(current->base.nmStr, nmExtPtr->base.nmStr, false, true)) {
            return true;
        }

        qElem = qElem->qLink;
    }

    return false;
}
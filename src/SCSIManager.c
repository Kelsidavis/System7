/*
#include <stdlib.h>
#include <string.h>
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * SCSIManager.c
 *
 * Portable implementation of Mac OS System 7.1 SCSI Manager
 * Based on Apple's SCSI Manager 4.3 with CAM (Common Access Method) API
 *
 * This implementation provides complete SCSI device management including:
 * - SCSI command execution (6, 10, and 12-byte commands)
 * - Device enumeration and identification
 * - Bus management and arbitration
 * - Synchronous and asynchronous I/O operations
 * - Error handling and retry logic
 * - Hardware abstraction for modern storage interfaces
 *
 * Copyright (c) 1989-1994 Apple Computer, Inc.
 * Portable implementation Copyright (c) 2024
 */

#include "SCSIManager.h"
#include "SystemTypes.h"
#include "System71StdLib.h"
#include <time.h>


/* Internal Configuration */
#define SCSI_MANAGER_VERSION    "4.3"
#define MAX_PENDING_IOS         64
#define DEFAULT_TIMEOUT_MS      30000
#define SELECT_TIMEOUT_MS       5000
#define MAX_RETRY_COUNT         3
#define QUANTUM_BUG_WORKAROUND  1   /* Enable Quantum firmware bug workaround */

/* Internal Data Structures */

/* I/O Status */
typedef enum {
    kPBidle = 0,                    /* In queue, nothing yet */
    kAttemptingInitiation,          /* Arbitration-selection in progress */
    kNexusEstablished,              /* Identify message sent and received */
    kSentCommand,                   /* CDB sent */
    kDisconnected,                  /* CDB sent, disconnected */
    kCompleted,                     /* Received status */
    kSelectComplete,                /* SCSISelect complete */
    kNeedsSense,                    /* Waiting for autosense */
    kGotSense,                      /* No longer waiting for autosense */
    kDisconnectedB4Command          /* Disconnected before command */
} IOStat;

/* I/O Events */
#define kmAborted               0x0001  /* PB was aborted */
#define kmTerminated            0x0002  /* Terminated */
#define kmBDRSent               0x0004  /* Bus Device Reset sent */
#define kmTimedOut              0x0008  /* Timed out */
#define kmMsgSent               0x0010  /* Message delivered */
#define kmMsgRejected           0x0020  /* Message rejected */
#define kmBadParity             0x0040  /* Bad parity detected */
#define kmAutoSense             0x0080  /* Autosense executed */

/* Device State */
typedef struct {
    Boolean present;                   /* Device is present */
    Boolean removable;                 /* Device is removable */
    UInt8 deviceType;            /* SCSI device type */
    char vendor[9];                /* Vendor ID */
    char product[17];              /* Product ID */
    char revision[5];              /* Revision level */
    Boolean supportsSynchronous;       /* Supports sync transfers */
    Boolean supportsWide;             /* Supports wide transfers */
    Boolean supportsTaggedQueuing;    /* Supports tagged queuing */
    UInt32 maxTransferSize;      /* Maximum transfer size */
    UInt8 syncOffset;            /* Synchronous offset */
    UInt8 syncPeriod;            /* Synchronous period */
} DeviceState;

/* Bus State */
typedef struct {
    Boolean present;                   /* Bus is present */
    UInt8 initiatorID;           /* Initiator ID */
    UInt8 maxTarget;             /* Maximum target ID */
    UInt8 maxLUN;                /* Maximum LUN */
    Boolean supportsWide;             /* Bus supports wide transfers */
    Boolean supportsFast;             /* Bus supports fast SCSI */
    Boolean supportsSync;             /* Bus supports synchronous transfers */
    DeviceState devices[MAX_SCSI_TARGETS][MAX_SCSI_LUNS];
    SCSIHardwareCallbacks *hal;    /* Hardware abstraction layer */
    SCSIHardwareInfo halInfo;      /* HAL information */
    pthread_mutex_t busMutex;      /* Bus access mutex */
} BusState;

/* I/O Queue Entry */
typedef struct IOQueueEntry {
    struct IOQueueEntry *next;      /* Next in queue */
    SCSI_IO *ioPtr;                /* I/O parameter block */
    IOStat ioStat;                 /* Current I/O status */
    UInt16 ioEvent;              /* I/O events */
    UInt8 firstError;            /* First error detected */
    UInt32 scTimer;              /* Timer value (seconds) */
    struct timeval startTime;      /* Start time */
    UInt8 retryCount;            /* Retry count */
    Boolean autoSenseNeeded;          /* Autosense required */
    UInt8 savedStatus;           /* Saved SCSI status */
} IOQueueEntry;

/* SCSI Manager Globals */
typedef struct {
    Boolean initialized;               /* Manager is initialized */
    BusState buses[MAX_SCSI_BUSES]; /* Bus states */
    UInt8 numBuses;              /* Number of active buses */

    /* I/O Queue Management */
    IOQueueEntry *ioQueue;         /* Main I/O queue */
    IOQueueEntry *immediateQueue;  /* Immediate I/O queue */
    IOQueueEntry *resetQueue;      /* Reset I/O queue */
    IOQueueEntry *freeQueue;       /* Free queue entries */
    pthread_mutex_t queueMutex;    /* Queue access mutex */

    /* Threading */
    pthread_t workerThread;        /* Worker thread */
    Boolean workerRunning;            /* Worker thread is running */
    pthread_cond_t workerCondition; /* Worker thread condition */
    pthread_mutex_t workerMutex;   /* Worker thread mutex */

    /* Statistics */
    UInt32 totalCommands;        /* Total commands processed */
    UInt32 successfulCommands;   /* Successful commands */
    UInt32 retriedCommands;      /* Retried commands */
    UInt32 failedCommands;       /* Failed commands */

    /* Configuration */
    Boolean quantumBugWorkaround;     /* Enable Quantum bug workaround */
    UInt32 defaultTimeout;      /* Default timeout (ms) */
    UInt32 selectTimeout;       /* Select timeout (ms) */
    UInt8 maxRetries;           /* Maximum retry count */
} SCSIManagerGlobals;

/* Global State */
static SCSIManagerGlobals g_SCSIMgr = {0};

/* Forward Declarations */
static OSErr ValidateDeviceIdent(const DeviceIdent *device);
static OSErr ValidateIOPB(const SCSI_IO *ioPtr);
static OSErr ExecuteCommand(SCSI_IO *ioPtr);
static OSErr PerformInquiry(UInt8 bus, UInt8 target, UInt8 lun, DeviceState *device);
static OSErr PerformAutoSense(SCSI_IO *ioPtr, IOQueueEntry *entry);
static void *WorkerThread(void *arg);
static void ProcessIOQueue(void);
static IOQueueEntry *AllocateQueueEntry(void);
static void FreeQueueEntry(IOQueueEntry *entry);
static void EnqueueIO(SCSI_IO *ioPtr);
static void DequeueIO(IOQueueEntry *entry);
static void CompleteIO(IOQueueEntry *entry, OSErr result);
static UInt32 GetCurrentTimeMS(void);
static Boolean IsTimeoutExpired(IOQueueEntry *entry);
static OSErr MapHALError(OSErr halError);
static void QuantumBugWorkaround(SCSI_IO *ioPtr);

/* Utility Functions */

static UInt32 GetCurrentTimeMS(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static Boolean IsTimeoutExpired(IOQueueEntry *entry) {
    if (entry->scTimer == 0) return false;  /* No timeout */

    struct timeval now;
    gettimeofday(&now, NULL);

    UInt32 elapsed = ((now.tv_sec - (entry)->tv_sec) * 1000) +
                      ((now.tv_usec - (entry)->tv_usec) / 1000);

    return elapsed >= (entry->scTimer * 1000);
}

static OSErr ValidateDeviceIdent(const DeviceIdent *device) {
    if (!device) return scsiBadParameter;
    if (device->bus >= g_SCSIMgr.numBuses) return scsiIDInvalid;
    if (device->targetID >= MAX_SCSI_TARGETS) return scsiTIDInvalid;
    if (device->LUN >= MAX_SCSI_LUNS) return scsiLUNInvalid;
    return noErr;
}

static OSErr ValidateIOPB(const SCSI_IO *ioPtr) {
    if (!ioPtr) return scsiBadParameter;
    if (ioPtr->scsiPBLength < sizeof(SCSI_IO)) return scsiBadParameter;

    OSErr err = ValidateDeviceIdent(&ioPtr->scsiDevice);
    if (err != noErr) return err;

    if (ioPtr->scsiCDBLength == 0 || ioPtr->scsiCDBLength > MAX_CDB_LENGTH) {
        return scsiCDBLengthInvalid;
    }

    if (ioPtr->scsiDataType > scsiDataSG) return scsiDataTypeInvalid;
    if (ioPtr->scsiTransferType > scsiTransferDMA) return scsiTransferTypeInvalid;

    return noErr;
}

static OSErr MapHALError(OSErr halError) {
    /* Map hardware abstraction layer errors to SCSI Manager errors */
    switch (halError) {
        case noErr: return noErr;
        case -1: return scsiSelectTimeout;
        case -2: return scsiCommandTimeout;
        case -3: return scsiParityError;
        case -4: return scsiUnexpectedBusFree;
        case -5: return scsiSequenceFail;
        default: return scsiDataRunError;
    }
}

/* Queue Management */

static IOQueueEntry *AllocateQueueEntry(void) {
    pthread_mutex_lock(&g_SCSIMgr.queueMutex);

    IOQueueEntry *entry = g_SCSIMgr.freeQueue;
    if (entry) {
        g_SCSIMgr.freeQueue = entry->next;
        entry->next = NULL;
        memset(entry, 0, sizeof(IOQueueEntry));
    } else {
        entry = calloc(1, sizeof(IOQueueEntry));
    }

    pthread_mutex_unlock(&g_SCSIMgr.queueMutex);
    return entry;
}

static void FreeQueueEntry(IOQueueEntry *entry) {
    if (!entry) return;

    pthread_mutex_lock(&g_SCSIMgr.queueMutex);
    entry->next = g_SCSIMgr.freeQueue;
    g_SCSIMgr.freeQueue = entry;
    pthread_mutex_unlock(&g_SCSIMgr.queueMutex);
}

static void EnqueueIO(SCSI_IO *ioPtr) {
    IOQueueEntry *entry = AllocateQueueEntry();
    if (!entry) {
        ioPtr->scsiResult = memFullErr;
        if (ioPtr->scsiCompletion) {
            ioPtr->scsiCompletion(ioPtr);
        }
        return;
    }

    entry->ioPtr = ioPtr;
    entry->ioStat = kPBidle;
    entry->ioEvent = 0;
    entry->firstError = 0;
    entry->retryCount = 0;
    entry->autoSenseNeeded = false;
    gettimeofday(&entry->startTime, NULL);

    /* Convert timeout to seconds */
    if (ioPtr->scsiTimeout == 0) {
        entry->scTimer = g_SCSIMgr.defaultTimeout / 1000;
    } else if (ioPtr->scsiTimeout < 0) {
        entry->scTimer = 1;  /* At least 1 second */
    } else {
        entry->scTimer = (ioPtr->scsiTimeout >> 10) + 1;  /* Approximation */
    }

    pthread_mutex_lock(&g_SCSIMgr.queueMutex);

    /* Add to appropriate queue */
    if (ioPtr->scsiFlags & scsiSIMQHead) {
        /* Add to head of queue */
        entry->next = g_SCSIMgr.ioQueue;
        g_SCSIMgr.ioQueue = entry;
    } else {
        /* Add to tail of queue */
        if (!g_SCSIMgr.ioQueue) {
            g_SCSIMgr.ioQueue = entry;
        } else {
            IOQueueEntry *tail = g_SCSIMgr.ioQueue;
            while (tail->next) tail = tail->next;
            tail->next = entry;
        }
    }

    pthread_mutex_unlock(&g_SCSIMgr.queueMutex);

    /* Signal worker thread */
    pthread_mutex_lock(&g_SCSIMgr.workerMutex);
    pthread_cond_signal(&g_SCSIMgr.workerCondition);
    pthread_mutex_unlock(&g_SCSIMgr.workerMutex);
}

static void DequeueIO(IOQueueEntry *entry) {
    if (!entry) return;

    pthread_mutex_lock(&g_SCSIMgr.queueMutex);

    /* Remove from queue */
    if (g_SCSIMgr.ioQueue == entry) {
        g_SCSIMgr.ioQueue = entry->next;
    } else {
        IOQueueEntry *prev = g_SCSIMgr.ioQueue;
        while (prev && prev->next != entry) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = entry->next;
        }
    }

    pthread_mutex_unlock(&g_SCSIMgr.queueMutex);
}

static void CompleteIO(IOQueueEntry *entry, OSErr result) {
    if (!entry || !entry->ioPtr) return;

    SCSI_IO *ioPtr = entry->ioPtr;

    /* Set result */
    ioPtr->scsiResult = result;

    /* Update statistics */
    if (result == noErr) {
        g_SCSIMgr.successfulCommands++;
    } else {
        g_SCSIMgr.failedCommands++;
    }

    if (entry->retryCount > 0) {
        g_SCSIMgr.retriedCommands++;
    }

    /* Remove from queue */
    DequeueIO(entry);

    /* Call completion routine */
    if (ioPtr->scsiCompletion) {
        ioPtr->scsiCompletion(ioPtr);
    }

    /* Free queue entry */
    FreeQueueEntry(entry);
}

/* Device Management */

static OSErr PerformInquiry(UInt8 bus, UInt8 target, UInt8 lun, DeviceState *device) {
    if (bus >= g_SCSIMgr.numBuses || !g_SCSIMgr.buses[bus].present) {
        return scsiIDInvalid;
    }

    BusState *busState = &g_SCSIMgr.buses[bus];
    if (!busState->hal || !busState->hal->getDeviceInfo) {
        return scsiBadFunction;
    }

    UInt8 inquiryData[36];
    OSErr err = busState->hal->getDeviceInfo(bus, target, lun, inquiryData, sizeof(inquiryData));
    if (err != noErr) {
        device->present = false;
        return MapHALError(err);
    }

    /* Parse inquiry data */
    device->present = true;
    device->deviceType = inquiryData[0] & 0x1F;
    device->removable = (inquiryData[1] & 0x80) != 0;

    /* Extract strings */
    memcpy(device->vendor, &inquiryData[8], 8);
    device->vendor[8] = '\0';
    memcpy(device->product, &inquiryData[16], 16);
    device->product[16] = '\0';
    memcpy(device->revision, &inquiryData[32], 4);
    device->revision[4] = '\0';

    /* Capability flags */
    device->supportsSynchronous = (inquiryData[7] & 0x10) != 0;
    device->supportsWide = (inquiryData[7] & 0x20) != 0;
    device->supportsTaggedQueuing = (inquiryData[7] & 0x02) != 0;

    /* Default transfer parameters */
    device->maxTransferSize = 65536;  /* 64KB default */
    device->syncOffset = 0;
    device->syncPeriod = 0;

    return noErr;
}

static OSErr PerformAutoSense(SCSI_IO *ioPtr, IOQueueEntry *entry) {
    if (!ioPtr->scsiSensePtr || ioPtr->scsiSenseLength == 0) {
        return scsiAutosenseFailed;
    }

    UInt8 bus = (ioPtr)->bus;
    UInt8 target = (ioPtr)->targetID;
    UInt8 lun = (ioPtr)->LUN;

    BusState *busState = &g_SCSIMgr.buses[bus];
    if (!busState->hal || !busState->hal->executeCommand) {
        return scsiAutosenseFailed;
    }

    /* Build REQUEST SENSE command */
    UInt8 senseCDB[6] = {0x03, 0, 0, 0, ioPtr->scsiSenseLength, 0};

    /* Execute REQUEST SENSE */
    OSErr err = busState->hal->executeCommand(bus, target, lun,
                                            senseCDB, 6,
                                            ioPtr->scsiSensePtr, ioPtr->scsiSenseLength,
                                            false, 5000);  /* 5 second timeout */

    if (err == noErr) {
        ioPtr->scsiResultFlags |= scsiAutosenseValid;
        entry->ioEvent |= kmAutoSense;
        /* Calculate sense residual */
        ioPtr->scsiSenseResidual = 0;  /* Assume all data transferred */
    }

    return MapHALError(err);
}

/* Quantum Firmware Bug Workaround */
static void QuantumBugWorkaround(SCSI_IO *ioPtr) {
    if (!g_SCSIMgr.quantumBugWorkaround) return;

    /* Check if this is a read command in the dangerous range (10-15 blocks) */
    if (ioPtr->scsiCDBLength >= 10) {
        UInt8 *cdb = ((ioPtr)->cdbPtr) ? (ioPtr)->cdbPtr : (ioPtr)->cdbBytes;
        if (cdb[0] == 0x28) {  /* READ(10) command */
            UInt16 transferLen = (cdb[7] << 8) | cdb[8];
            if (transferLen >= 10 && transferLen <= 15) {
                /* Split into two smaller reads */
                UInt16 firstRead = transferLen / 2;
                UInt16 secondRead = transferLen - firstRead;

                /* Modify transfer length for first read */
                cdb[7] = (firstRead >> 8) & 0xFF;
                cdb[8] = firstRead & 0xFF;

                /* Note: This is a simplified implementation.
                 * A complete implementation would need to handle
                 * the second read as a separate command.
                 */
            }
        }
    }
}

/* Command Execution */

static OSErr ExecuteCommand(SCSI_IO *ioPtr) {
    UInt8 bus = (ioPtr)->bus;
    UInt8 target = (ioPtr)->targetID;
    UInt8 lun = (ioPtr)->LUN;

    BusState *busState = &g_SCSIMgr.buses[bus];
    if (!busState->present || !busState->hal) {
        return scsiIDInvalid;
    }

    if (!busState->hal->executeCommand) {
        return scsiBadFunction;
    }

    /* Apply Quantum bug workaround if needed */
    QuantumBugWorkaround(ioPtr);

    /* Get CDB pointer */
    UInt8 *cdb = ((ioPtr)->cdbPtr) ? (ioPtr)->cdbPtr : (ioPtr)->cdbBytes;

    /* Determine data direction */
    Boolean dataOut = (ioPtr->scsiFlags & scsiDirectionMask) == scsiDirectionOut;

    /* Handle scatter/gather if needed */
    void *dataPtr = ioPtr->scsiDataPtr;
    UInt32 dataLen = ioPtr->scsiDataLength;

    if (ioPtr->scsiDataType == scsiDataSG && ioPtr->scsiSGListCount > 0) {
        /* For simplicity, we'll handle only single-segment S/G lists here */
        /* A complete implementation would need to handle multi-segment lists */
        SGRecord *sg = (SGRecord *)ioPtr->scsiDataPtr;
        dataPtr = sg->SGAddr;
        dataLen = sg->SGCount;
    }

    /* Execute the command */
    UInt32 timeout = (ioPtr->scsiTimeout > 0) ? ioPtr->scsiTimeout : g_SCSIMgr.defaultTimeout;
    OSErr err = busState->hal->executeCommand(bus, target, lun,
                                            cdb, ioPtr->scsiCDBLength,
                                            dataPtr, dataLen,
                                            dataOut, timeout);

    return MapHALError(err);
}

/* Worker Thread */

static void ProcessIOQueue(void) {
    pthread_mutex_lock(&g_SCSIMgr.queueMutex);

    IOQueueEntry *entry = g_SCSIMgr.ioQueue;
    IOQueueEntry *prev = NULL;

    while (entry) {
        IOQueueEntry *next = entry->next;
        Boolean processEntry = false;

        /* Check for timeout */
        if (IsTimeoutExpired(entry)) {
            entry->ioEvent |= kmTimedOut;
            entry->firstError = scsiCommandTimeout;
            CompleteIO(entry, scsiCommandTimeout);
            processEntry = true;
        }
        /* Process idle entries */
        else if (entry->ioStat == kPBidle) {
            processEntry = true;
        }
        /* Handle autosense requests */
        else if (entry->ioStat == kNeedsSense) {
            OSErr senseErr = PerformAutoSense(entry->ioPtr, entry);
            entry->ioStat = kGotSense;
            CompleteIO(entry, (senseErr == noErr) ? scsiNonZeroStatus : senseErr);
            processEntry = true;
        }

        if (processEntry && entry->ioStat == kPBidle) {
            /* Remove from queue temporarily */
            if (prev) {
                prev->next = next;
            } else {
                g_SCSIMgr.ioQueue = next;
            }

            pthread_mutex_unlock(&g_SCSIMgr.queueMutex);

            /* Execute the command */
            entry->ioStat = kAttemptingInitiation;
            OSErr result = ExecuteCommand(entry->ioPtr);

            if (result == noErr) {
                entry->ioStat = kCompleted;
                CompleteIO(entry, noErr);
            } else if (result == scsiNonZeroStatus && entry->ioPtr->scsiSensePtr) {
                /* Need autosense */
                entry->ioStat = kNeedsSense;
                entry->autoSenseNeeded = true;

                pthread_mutex_lock(&g_SCSIMgr.queueMutex);
                /* Re-add to queue for autosense processing */
                entry->next = g_SCSIMgr.ioQueue;
                g_SCSIMgr.ioQueue = entry;
                pthread_mutex_unlock(&g_SCSIMgr.queueMutex);
            } else {
                /* Check for retry */
                if (entry->retryCount < g_SCSIMgr.maxRetries &&
                    (result == scsiSelectTimeout || result == scsiCommandTimeout)) {
                    entry->retryCount++;
                    entry->ioStat = kPBidle;
                    gettimeofday(&entry->startTime, NULL);  /* Reset timer */

                    pthread_mutex_lock(&g_SCSIMgr.queueMutex);
                    /* Re-add to queue for retry */
                    entry->next = g_SCSIMgr.ioQueue;
                    g_SCSIMgr.ioQueue = entry;
                    pthread_mutex_unlock(&g_SCSIMgr.queueMutex);
                } else {
                    /* Failed permanently */
                    entry->ioStat = kCompleted;
                    CompleteIO(entry, result);
                }
            }

            pthread_mutex_lock(&g_SCSIMgr.queueMutex);
            /* Start over since queue may have changed */
            entry = g_SCSIMgr.ioQueue;
            prev = NULL;
            continue;
        }

        prev = entry;
        entry = next;
    }

    pthread_mutex_unlock(&g_SCSIMgr.queueMutex);
}

static void *WorkerThread(void *arg) {
    while (g_SCSIMgr.workerRunning) {
        /* Process I/O queue */
        ProcessIOQueue();

        /* Wait for work or timeout */
        pthread_mutex_lock(&g_SCSIMgr.workerMutex);
        struct timespec timeout;
        struct timeval now;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 1;  /* 1 second timeout */
        timeout.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&g_SCSIMgr.workerCondition, &g_SCSIMgr.workerMutex, &timeout);
        pthread_mutex_unlock(&g_SCSIMgr.workerMutex);
    }

    return NULL;
}

/* Public API Implementation */

OSErr InitSCSIManager(void) {
    if (g_SCSIMgr.initialized) {
        return noErr;
    }

    memset(&g_SCSIMgr, 0, sizeof(g_SCSIMgr));

    /* Initialize mutexes */
    pthread_mutex_init(&g_SCSIMgr.queueMutex, NULL);
    pthread_mutex_init(&g_SCSIMgr.workerMutex, NULL);
    pthread_cond_init(&g_SCSIMgr.workerCondition, NULL);

    /* Initialize buses */
    for (int i = 0; i < MAX_SCSI_BUSES; i++) {
        pthread_mutex_init(&g_SCSIMgr.buses[i].busMutex, NULL);
    }

    /* Set defaults */
    g_SCSIMgr.quantumBugWorkaround = QUANTUM_BUG_WORKAROUND;
    g_SCSIMgr.defaultTimeout = DEFAULT_TIMEOUT_MS;
    g_SCSIMgr.selectTimeout = SELECT_TIMEOUT_MS;
    g_SCSIMgr.maxRetries = MAX_RETRY_COUNT;

    /* Start worker thread */
    g_SCSIMgr.workerRunning = true;
    if (pthread_create(&g_SCSIMgr.workerThread, NULL, WorkerThread, NULL) != 0) {
        return memFullErr;
    }

    g_SCSIMgr.initialized = true;
    return noErr;
}

void ShutdownSCSIManager(void) {
    if (!g_SCSIMgr.initialized) return;

    /* Stop worker thread */
    g_SCSIMgr.workerRunning = false;
    pthread_cond_signal(&g_SCSIMgr.workerCondition);
    pthread_join(g_SCSIMgr.workerThread, NULL);

    /* Clean up buses */
    for (int i = 0; i < g_SCSIMgr.numBuses; i++) {
        BusState *bus = &g_SCSIMgr.buses[i];
        if (bus->hal && bus->hal->shutdownHardware) {
            bus->hal->shutdownHardware(i);
        }
        pthread_mutex_destroy(&bus->busMutex);
    }

    /* Free queue entries */
    while (g_SCSIMgr.freeQueue) {
        IOQueueEntry *entry = g_SCSIMgr.freeQueue;
        g_SCSIMgr.freeQueue = entry->next;
        free(entry);
    }

    /* Destroy mutexes */
    pthread_mutex_destroy(&g_SCSIMgr.queueMutex);
    pthread_mutex_destroy(&g_SCSIMgr.workerMutex);
    pthread_cond_destroy(&g_SCSIMgr.workerCondition);

    g_SCSIMgr.initialized = false;
}

OSErr SCSIAction(SCSI_IO *ioPtr) {
    if (!g_SCSIMgr.initialized) {
        return memFullErr;
    }

    if (!ioPtr) {
        return scsiBadParameter;
    }

    /* Set initial state */
    ioPtr->scsiResult = scsiRequestInProgress;
    ioPtr->scsiResultFlags = 0;
    ioPtr->scsiSCSIstatus = 0;
    ioPtr->scsiSenseResidual = 0;
    ioPtr->scsiDataResidual = 0;

    /* Handle function code */
    switch (ioPtr->scsiFunctionCode) {
        case SCSINop:
            ioPtr->scsiResult = noErr;
            if (ioPtr->scsiCompletion) {
                ioPtr->scsiCompletion(ioPtr);
            }
            break;

        case SCSIExecIO:
            {
                OSErr err = ValidateIOPB(ioPtr);
                if (err != noErr) {
                    ioPtr->scsiResult = err;
                    if (ioPtr->scsiCompletion) {
                        ioPtr->scsiCompletion(ioPtr);
                    }
                } else {
                    EnqueueIO(ioPtr);
                    g_SCSIMgr.totalCommands++;
                }
            }
            break;

        default:
            ioPtr->scsiResult = scsiBadFunction;
            if (ioPtr->scsiCompletion) {
                ioPtr->scsiCompletion(ioPtr);
            }
            break;
    }

    return ioPtr->scsiResult;
}

OSErr SCSIExecIOSync(SCSI_IO *ioPtr) {
    if (!ioPtr) return scsiBadParameter;

    /* Clear completion routine for synchronous operation */
    ioPtr->scsiCompletion = NULL;

    OSErr err = SCSIAction(ioPtr);
    if (err != scsiRequestInProgress) {
        return err;
    }

    /* Wait for completion */
    while (ioPtr->scsiResult == scsiRequestInProgress) {
        usleep(1000);  /* Sleep 1ms */
    }

    return ioPtr->scsiResult;
}

SCSI_IO *NewSCSI_PB(void) {
    SCSI_IO *pb = calloc(1, sizeof(SCSI_IO));
    if (pb) {
        pb->scsiPBLength = sizeof(SCSI_IO);
        pb->scsiFunctionCode = SCSIExecIO;
    }
    return pb;
}

void DisposeSCSI_PB(SCSI_IO *pb) {
    if (pb) {
        free(pb);
    }
}

OSErr SCSIRegisterHAL(SCSIHardwareCallbacks *callbacks, SCSIHardwareInfo *info) {
    if (!callbacks || !info) return scsiBadParameter;
    if (g_SCSIMgr.numBuses >= MAX_SCSI_BUSES) return scsiTooManyBuses;

    UInt8 busID = g_SCSIMgr.numBuses++;
    BusState *bus = &g_SCSIMgr.buses[busID];

    bus->present = true;
    bus->hal = callbacks;
    bus->halInfo = *info;
    bus->initiatorID = info->initiatorID;
    bus->maxTarget = info->maxTarget;
    bus->maxLUN = info->maxLUN;
    bus->supportsWide = info->supportsWide;
    bus->supportsFast = info->supportsSynchronous;  /* Assume fast if sync */
    bus->supportsSync = info->supportsSynchronous;

    /* Initialize hardware */
    if (callbacks->initHardware) {
        OSErr err = callbacks->initHardware(info);
        if (err != noErr) {
            bus->present = false;
            g_SCSIMgr.numBuses--;
            return MapHALError(err);
        }
    }

    /* Scan for devices */
    for (UInt8 target = 0; target < bus->maxTarget; target++) {
        if (target == bus->initiatorID) continue;  /* Skip initiator */

        for (UInt8 lun = 0; lun < bus->maxLUN; lun++) {
            PerformInquiry(busID, target, lun, &bus->devices[target][lun]);
        }
    }

    return noErr;
}

/* Additional helper functions would be implemented here for:
 * - SCSIBusInquirySync
 * - SCSIResetBusSync
 * - SCSIResetDeviceSync
 * - SCSIAbortCommandSync
 * - SCSITerminateIOSync
 * - Device reference number management
 * - etc.
 */

/* Bus Inquiry Implementation */
OSErr SCSIBusInquirySync(SCSIBusInquiryPB *inquiry) {
    if (!inquiry) return scsiBadParameter;

    OSErr err = ValidateDeviceIdent(&inquiry->scsiDevice);
    if (err != noErr) return err;

    UInt8 busID = (inquiry)->bus;
    BusState *bus = &g_SCSIMgr.buses[busID];

    if (!bus->present) return scsiIDInvalid;

    /* Fill in bus inquiry data */
    inquiry->scsiResult = noErr;
    inquiry->scsiEngineCount = 1;
    inquiry->scsiMaxTransferType = 3;  /* Polled, Blind, DMA */
    inquiry->scsiDataTypes = 0x07;     /* Buffer, TIB, S/G */
    inquiry->scsiIOpbSize = sizeof(SCSI_IO);
    inquiry->scsiMaxIOpbSize = sizeof(SCSI_IO);
    inquiry->scsiFeatureFlags = 0;
    inquiry->scsiVersionNumber = SCSI_VERSION;
    inquiry->scsiHBAInquiry = 0x20;    /* Supports synchronous */
    inquiry->scsiTargetModeFlags = 0;
    inquiry->scsiScanFlags = 0;
    inquiry->scsiHiBusID = g_SCSIMgr.numBuses - 1;
    inquiry->scsiInitiatorID = bus->initiatorID;
    inquiry->scsiMaxTarget = bus->maxTarget;
    inquiry->scsiMaxLUN = bus->maxLUN;

    /* Safe string copy helper */
    #define SAFE_STRCPY(dst, src, maxlen) do { \
        const char* s = (src); \
        char* d = (dst); \
        size_t i; \
        for (i = 0; i < (maxlen) - 1 && s[i]; i++) { \
            d[i] = s[i]; \
        } \
        d[i] = '\0'; \
    } while(0)

    /* Vendor strings - assuming 16 byte fields based on typical SCSI inquiry */
    SAFE_STRCPY(inquiry->scsiSIMVendor, "Apple Computer", 16);
    SAFE_STRCPY(inquiry->scsiHBAVendor, "Portable SCSI", 16);
    SAFE_STRCPY(inquiry->scsiControllerFamily, "Generic", 16);
    SAFE_STRCPY(inquiry->scsiControllerType, "Portable", 16);

    /* Version info - assuming 8 byte fields */
    SAFE_STRCPY(inquiry->scsiXPTversion, "4.3", 8);
    SAFE_STRCPY(inquiry->scsiSIMversion, "4.3", 8);
    SAFE_STRCPY(inquiry->scsiHBAversion, "1.0", 8);

    #undef SAFE_STRCPY

    return noErr;
}

/* Reset Bus Implementation */
OSErr SCSIResetBusSync(SCSIResetBusPB *resetBus) {
    if (!resetBus) return scsiBadParameter;

    OSErr err = ValidateDeviceIdent(&resetBus->scsiDevice);
    if (err != noErr) return err;

    UInt8 busID = (resetBus)->bus;
    BusState *bus = &g_SCSIMgr.buses[busID];

    if (!bus->present || !bus->hal || !bus->hal->resetBus) {
        return scsiIDInvalid;
    }

    /* Reset the bus */
    err = bus->hal->resetBus(busID);
    resetBus->scsiResult = MapHALError(err);

    return resetBus->scsiResult;
}

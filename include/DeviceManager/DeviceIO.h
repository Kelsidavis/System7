/*
 * DeviceIO.h
 * System 7.1 Device Manager - Device I/O Operations
 *
 * Defines structures and constants for device I/O operations including
 * parameter blocks, positioning modes, and asynchronous I/O support.
 */

#ifndef DEVICE_IO_H
#define DEVICE_IO_H

#include "DeviceTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * File Positioning Modes
 * ============================================================================= */

#define fsAtMark        0       /* Position at current mark */
#define fsFromStart     1       /* Position from start of file */
#define fsFromLEOF      2       /* Position from logical end of file */
#define fsFromMark      3       /* Position relative to current mark */

/* Positioning mode modifier flags */
#define rdVerify        64      /* Verify after read */
#define rdVerifyBit     6       /* Bit position for rdVerify */

/* =============================================================================
 * Permission Constants
 * ============================================================================= */

#define fsCurPerm       0       /* Use current permission */
#define fsRdPerm        1       /* Read permission */
#define fsWrPerm        2       /* Write permission */
#define fsRdWrPerm      3       /* Read/write permission */
#define fsRdWrShPerm    4       /* Read/write shared permission */

/* =============================================================================
 * Extended I/O Parameter Block
 * ============================================================================= */

typedef struct ExtendedIOParam {
    ParamBlockHeader pb;        /* Standard parameter block header */
    int16_t         ioRefNum;   /* Driver reference number */
    int8_t          ioVersNum;  /* Version number */
    int8_t          ioPermssn;  /* Permission */
    void           *ioMisc;     /* Miscellaneous pointer */
    void           *ioBuffer;   /* I/O buffer pointer */
    int32_t         ioReqCount; /* Requested byte count */
    int32_t         ioActCount; /* Actual byte count */
    int16_t         ioPosMode;  /* Positioning mode */
    int32_t         ioPosOffset;/* Position offset */

    /* Extended fields */
    uint32_t        ioTimeout;  /* Operation timeout (ticks) */
    uint32_t        ioFlags;    /* Operation flags */
    void           *ioUserData; /* User data pointer */
    uint32_t        ioTimestamp;/* Operation timestamp */
} ExtendedIOParam, *ExtendedIOParamPtr;

/* =============================================================================
 * Async I/O Request Structure
 * ============================================================================= */

typedef struct AsyncIORequest {
    ExtendedIOParam param;      /* I/O parameters */
    int16_t         requestID;  /* Unique request identifier */
    uint32_t        priority;   /* Request priority */
    bool            isCancelled;/* True if request was cancelled */
    bool            isCompleted;/* True if request is completed */
    void           *context;    /* Request context */
} AsyncIORequest, *AsyncIORequestPtr;

/* =============================================================================
 * I/O Operation Flags
 * ============================================================================= */

#define kIOFlagNoCache          0x0001  /* Bypass cache */
#define kIOFlagSynchronous      0x0002  /* Force synchronous operation */
#define kIOFlagHighPriority     0x0004  /* High priority operation */
#define kIOFlagLowPriority      0x0008  /* Low priority operation */
#define kIOFlagVerify           0x0010  /* Verify operation */
#define kIOFlagRetry            0x0020  /* Allow retry on error */
#define kIOFlagNotifyComplete   0x0040  /* Send completion notification */
#define kIOFlagReserved         0x0080  /* Reserved for future use */

/* =============================================================================
 * I/O Operation Types
 * ============================================================================= */

typedef enum {
    kIOOperationRead    = 1,    /* Read operation */
    kIOOperationWrite   = 2,    /* Write operation */
    kIOOperationControl = 3,    /* Control operation */
    kIOOperationStatus  = 4,    /* Status operation */
    kIOOperationOpen    = 5,    /* Open operation */
    kIOOperationClose   = 6,    /* Close operation */
    kIOOperationKill    = 7     /* Kill operation */
} IOOperationType;

/* =============================================================================
 * I/O Completion Callback Types
 * ============================================================================= */

/**
 * Standard completion routine
 *
 * @param pb Parameter block that completed
 */
typedef void (*IOCompletionProc)(IOParamPtr pb);

/**
 * Extended completion routine with context
 *
 * @param pb Parameter block that completed
 * @param context User context data
 */
typedef void (*ExtendedIOCompletionProc)(IOParamPtr pb, void *context);

/**
 * Async I/O completion routine
 *
 * @param request Async I/O request that completed
 * @param error Error code from operation
 */
typedef void (*AsyncIOCompletionProc)(AsyncIORequestPtr request, int16_t error);

/* =============================================================================
 * I/O Queue Management
 * ============================================================================= */

/**
 * I/O Request Queue Entry
 */
typedef struct IOQueueEntry {
    struct IOQueueEntry *next;  /* Next entry in queue */
    IOParamPtr          pb;     /* Parameter block */
    uint32_t            priority; /* Request priority */
    uint32_t            timestamp; /* When queued */
    bool                isActive; /* True if being processed */
} IOQueueEntry, *IOQueueEntryPtr;

/**
 * I/O Request Queue
 */
typedef struct IORequestQueue {
    IOQueueEntryPtr     head;     /* First entry */
    IOQueueEntryPtr     tail;     /* Last entry */
    IOQueueEntryPtr     current;  /* Currently processing */
    int32_t             count;    /* Number of entries */
    uint32_t            maxSize;  /* Maximum queue size */
    bool                isPaused; /* True if queue processing is paused */
} IORequestQueue, *IORequestQueuePtr;

/* =============================================================================
 * Device Buffer Management
 * ============================================================================= */

/**
 * Device Buffer Descriptor
 */
typedef struct DeviceBuffer {
    void               *data;      /* Buffer data pointer */
    uint32_t            size;      /* Buffer size */
    uint32_t            used;      /* Bytes used */
    uint32_t            offset;    /* Current offset */
    bool                isReadOnly;/* True if read-only */
    bool                isDirty;   /* True if modified */
    void               *context;   /* Buffer context */
} DeviceBuffer, *DeviceBufferPtr;

/* =============================================================================
 * I/O Statistics
 * ============================================================================= */

typedef struct IOStatistics {
    uint32_t            readOperations;    /* Number of read operations */
    uint32_t            writeOperations;   /* Number of write operations */
    uint32_t            controlOperations; /* Number of control operations */
    uint32_t            statusOperations;  /* Number of status operations */

    uint64_t            bytesRead;         /* Total bytes read */
    uint64_t            bytesWritten;      /* Total bytes written */

    uint32_t            errors;            /* Number of errors */
    uint32_t            retries;           /* Number of retries */
    uint32_t            timeouts;          /* Number of timeouts */

    uint32_t            queueDepth;        /* Current queue depth */
    uint32_t            maxQueueDepth;     /* Maximum queue depth */

    uint32_t            avgResponseTime;   /* Average response time (ticks) */
    uint32_t            maxResponseTime;   /* Maximum response time (ticks) */
} IOStatistics, *IOStatisticsPtr;

/* =============================================================================
 * I/O Operation Functions
 * ============================================================================= */

/**
 * Initialize I/O parameter block
 *
 * @param pb Parameter block to initialize
 * @param operation Type of operation
 * @param refNum Driver reference number
 */
void InitIOParamBlock(IOParamPtr pb, IOOperationType operation, int16_t refNum);

/**
 * Set I/O buffer information
 *
 * @param pb Parameter block
 * @param buffer Buffer pointer
 * @param count Byte count
 */
void SetIOBuffer(IOParamPtr pb, void *buffer, int32_t count);

/**
 * Set file positioning
 *
 * @param pb Parameter block
 * @param mode Positioning mode
 * @param offset Position offset
 */
void SetIOPosition(IOParamPtr pb, int16_t mode, int32_t offset);

/**
 * Set I/O completion routine
 *
 * @param pb Parameter block
 * @param completion Completion routine
 */
void SetIOCompletion(IOParamPtr pb, IOCompletionProc completion);

/**
 * Check if I/O operation is complete
 *
 * @param pb Parameter block
 * @return True if complete
 */
bool IsIOComplete(IOParamPtr pb);

/**
 * Check if I/O operation is in progress
 *
 * @param pb Parameter block
 * @return True if in progress
 */
bool IsIOInProgress(IOParamPtr pb);

/**
 * Get I/O operation result
 *
 * @param pb Parameter block
 * @return Result code
 */
int16_t GetIOResult(IOParamPtr pb);

/* =============================================================================
 * Async I/O Management
 * ============================================================================= */

/**
 * Create an async I/O request
 *
 * @param pb I/O parameter block
 * @param priority Request priority
 * @param completion Completion routine
 * @return Async I/O request or NULL on error
 */
AsyncIORequestPtr CreateAsyncIORequest(IOParamPtr pb, uint32_t priority,
                                       AsyncIOCompletionProc completion);

/**
 * Cancel an async I/O request
 *
 * @param request Request to cancel
 * @return Error code
 */
int16_t CancelAsyncIORequest(AsyncIORequestPtr request);

/**
 * Wait for async I/O request to complete
 *
 * @param request Request to wait for
 * @param timeout Timeout in ticks (0 = no timeout)
 * @return Error code
 */
int16_t WaitForAsyncIO(AsyncIORequestPtr request, uint32_t timeout);

/**
 * Destroy an async I/O request
 *
 * @param request Request to destroy
 */
void DestroyAsyncIORequest(AsyncIORequestPtr request);

/* =============================================================================
 * I/O Queue Operations
 * ============================================================================= */

/**
 * Create an I/O request queue
 *
 * @param maxSize Maximum queue size (0 = unlimited)
 * @return I/O request queue or NULL on error
 */
IORequestQueuePtr CreateIOQueue(uint32_t maxSize);

/**
 * Destroy an I/O request queue
 *
 * @param queue Queue to destroy
 */
void DestroyIOQueue(IORequestQueuePtr queue);

/**
 * Add request to I/O queue
 *
 * @param queue I/O request queue
 * @param pb Parameter block
 * @param priority Request priority
 * @return Error code
 */
int16_t EnqueueIORequest(IORequestQueuePtr queue, IOParamPtr pb, uint32_t priority);

/**
 * Remove next request from I/O queue
 *
 * @param queue I/O request queue
 * @return Parameter block or NULL if empty
 */
IOParamPtr DequeueIORequest(IORequestQueuePtr queue);

/**
 * Peek at next request in I/O queue without removing
 *
 * @param queue I/O request queue
 * @return Parameter block or NULL if empty
 */
IOParamPtr PeekIOQueue(IORequestQueuePtr queue);

/**
 * Get I/O queue size
 *
 * @param queue I/O request queue
 * @return Number of queued requests
 */
int32_t GetIOQueueSize(IORequestQueuePtr queue);

/**
 * Pause I/O queue processing
 *
 * @param queue I/O request queue
 */
void PauseIOQueue(IORequestQueuePtr queue);

/**
 * Resume I/O queue processing
 *
 * @param queue I/O request queue
 */
void ResumeIOQueue(IORequestQueuePtr queue);

/* =============================================================================
 * Buffer Management
 * ============================================================================= */

/**
 * Create a device buffer
 *
 * @param size Buffer size
 * @param isReadOnly True if buffer is read-only
 * @return Device buffer or NULL on error
 */
DeviceBufferPtr CreateDeviceBuffer(uint32_t size, bool isReadOnly);

/**
 * Destroy a device buffer
 *
 * @param buffer Buffer to destroy
 */
void DestroyDeviceBuffer(DeviceBufferPtr buffer);

/**
 * Read from device buffer
 *
 * @param buffer Device buffer
 * @param data Output data buffer
 * @param offset Offset in buffer
 * @param count Number of bytes to read
 * @return Number of bytes read or error code
 */
int32_t ReadDeviceBuffer(DeviceBufferPtr buffer, void *data, uint32_t offset, uint32_t count);

/**
 * Write to device buffer
 *
 * @param buffer Device buffer
 * @param data Input data buffer
 * @param offset Offset in buffer
 * @param count Number of bytes to write
 * @return Number of bytes written or error code
 */
int32_t WriteDeviceBuffer(DeviceBufferPtr buffer, const void *data, uint32_t offset, uint32_t count);

/**
 * Flush device buffer changes
 *
 * @param buffer Device buffer
 * @return Error code
 */
int16_t FlushDeviceBuffer(DeviceBufferPtr buffer);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_IO_H */
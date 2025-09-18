#ifndef RESPONSE_HANDLING_H
#define RESPONSE_HANDLING_H

#include "NotificationManager/NotificationManager.h"
#include "NotificationManager/SystemAlerts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Response Types */
typedef enum {
    responseTypeCallback = 1,       /* Original Mac OS callback */
    responseTypeModern = 2,         /* Modern callback function */
    responseTypePlatform = 3,       /* Platform-specific response */
    responseTypeInternal = 4        /* Internal system response */
} ResponseType;

/* Response Status */
typedef enum {
    responseStatusPending = 0,      /* Response pending */
    responseStatusProcessing = 1,   /* Response being processed */
    responseStatusCompleted = 2,    /* Response completed */
    responseStatusFailed = 3,       /* Response failed */
    responseStatusTimeout = 4       /* Response timed out */
} ResponseStatus;

/* Response Context */
typedef struct {
    ResponseType        type;               /* Type of response */
    ResponseStatus      status;             /* Current status */
    NMExtendedRecPtr    notification;       /* Associated notification */
    UInt32              responseTime;       /* When response occurred */
    UInt32              processingTime;     /* When processing started */
    UInt32              completionTime;     /* When processing completed */
    AlertResponse       alertResponse;      /* Alert response code */
    long                userData;           /* User data */
    void               *context;           /* Response context */
    OSErr               lastError;          /* Last error encountered */
    Boolean             async;              /* Asynchronous processing */
    Boolean             completed;          /* Response completed */
} ResponseContext, *ResponseContextPtr;

/* Response Queue Entry */
typedef struct ResponseQueueEntry {
    QElemPtr                    qLink;      /* Queue link */
    short                       qType;      /* Queue type */
    ResponseContextPtr          context;    /* Response context */
    UInt32                      priority;   /* Processing priority */
    UInt32                      timestamp;  /* Queue timestamp */
    struct ResponseQueueEntry  *next;       /* Next in queue */
} ResponseQueueEntry, *ResponseQueuePtr;

/* Response Handler Registration */
typedef struct {
    ResponseType        type;               /* Response type */
    NMProcPtr           callback;           /* Classic Mac OS callback */
    NotificationCallback modernCallback;   /* Modern callback */
    void               *context;           /* Callback context */
    Boolean             active;             /* Handler active */
    UInt32              registrationTime;   /* When registered */
    long                refCon;             /* Reference constant */
} ResponseHandler, *ResponseHandlerPtr;

/* Response Processing State */
typedef struct {
    Boolean             initialized;        /* Response handling initialized */
    Boolean             enabled;            /* Response processing enabled */
    QHdr                responseQueue;      /* Response processing queue */
    short               queueSize;          /* Current queue size */
    short               maxQueueSize;       /* Maximum queue size */
    UInt32              lastProcessing;     /* Last processing time */
    UInt32              processingInterval; /* Processing interval */
    ResponseHandler     handlers[16];       /* Registered handlers */
    short               handlerCount;       /* Number of handlers */
    Boolean             asyncProcessing;    /* Asynchronous processing */
    UInt32              totalResponses;     /* Total responses processed */
    UInt32              failedResponses;    /* Failed response count */
} ResponseState;

/* Response Handling Initialization */
OSErr NMResponseHandlingInit(void);
void NMResponseHandlingCleanup(void);

/* Response Processing */
OSErr NMTriggerResponse(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse);
OSErr NMProcessResponse(ResponseContextPtr context);
OSErr NMQueueResponse(ResponseContextPtr context);
void NMProcessResponseQueue(void);

/* Response Context Management */
ResponseContextPtr NMCreateResponseContext(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse);
void NMDestroyResponseContext(ResponseContextPtr context);
OSErr NMSetResponseUserData(ResponseContextPtr context, long userData, void *contextData);
OSErr NMGetResponseStatus(ResponseContextPtr context, ResponseStatus *status);

/* Classic Mac OS Response Handling */
OSErr NMExecuteClassicCallback(NMExtendedRecPtr nmExtPtr);
OSErr NMCallOriginalResponse(NMRecPtr nmReqPtr);
OSErr NMSetupCallbackEnvironment(NMRecPtr nmReqPtr);
OSErr NMRestoreCallbackEnvironment(void);

/* Modern Response Handling */
OSErr NMExecuteModernCallback(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse);
OSErr NMRegisterModernHandler(NotificationCallback callback, void *context);
OSErr NMUnregisterModernHandler(NotificationCallback callback);

/* Response Handler Management */
OSErr NMRegisterResponseHandler(ResponseType type, NMProcPtr callback, void *context);
OSErr NMUnregisterResponseHandler(ResponseType type);
OSErr NMFindResponseHandler(ResponseType type, ResponseHandlerPtr *handler);
OSErr NMSetHandlerActive(ResponseType type, Boolean active);

/* Platform Response Integration */
OSErr NMRegisterPlatformResponseHandler(void *platformHandler, void *context);
OSErr NMUnregisterPlatformResponseHandler(void);
OSErr NMTriggerPlatformResponse(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse);

/* Response Queue Management */
OSErr NMInitResponseQueue(void);
void NMCleanupResponseQueue(void);
OSErr NMAddToResponseQueue(ResponseContextPtr context);
OSErr NMRemoveFromResponseQueue(ResponseContextPtr context);
OSErr NMFlushResponseQueue(void);

/* Response Status and Statistics */
OSErr NMGetResponseStatistics(UInt32 *totalResponses, UInt32 *failedResponses, UInt32 *pendingResponses);
OSErr NMGetResponseQueueStatus(short *queueSize, short *maxSize);
OSErr NMResetResponseStatistics(void);

/* Response Timing */
OSErr NMSetResponseTimeout(UInt32 timeout);
UInt32 NMGetResponseTimeout(void);
OSErr NMCheckResponseTimeouts(void);
Boolean NMIsResponseTimedOut(ResponseContextPtr context);

/* Error Handling */
OSErr NMHandleResponseError(ResponseContextPtr context, OSErr error);
OSErr NMRetryResponse(ResponseContextPtr context);
OSErr NMAbortResponse(ResponseContextPtr context);
OSErr NMGetLastResponseError(OSErr *error, StringPtr errorMessage);

/* Response Validation */
OSErr NMValidateResponseContext(ResponseContextPtr context);
OSErr NMValidateCallback(NMProcPtr callback);
Boolean NMIsValidResponse(AlertResponse response);

/* Internal Response Processing */
void NMInternalResponseProcessor(void);
OSErr NMProcessSingleResponse(ResponseQueuePtr entry);
OSErr NMExecuteResponse(ResponseContextPtr context);
void NMCleanupCompletedResponses(void);

/* Response Debugging and Monitoring */
OSErr NMSetResponseLogging(Boolean enabled);
OSErr NMLogResponse(ResponseContextPtr context, StringPtr message);
OSErr NMDumpResponseQueue(void);
OSErr NMValidateResponseQueue(void);

/* Utility Functions */
UInt32 NMGetResponseTimestamp(void);
StringPtr NMGetResponseTypeName(ResponseType type);
StringPtr NMGetResponseStatusName(ResponseStatus status);
StringPtr NMGetAlertResponseName(AlertResponse response);

/* Constants */
#define RESPONSE_QUEUE_MAX_SIZE     100     /* Maximum response queue size */
#define RESPONSE_DEFAULT_TIMEOUT    300     /* Default response timeout (5 seconds) */
#define RESPONSE_PROCESSING_INTERVAL 10     /* Processing interval in ticks */
#define RESPONSE_MAX_HANDLERS       16      /* Maximum response handlers */
#define RESPONSE_RETRY_ATTEMPTS     3       /* Maximum retry attempts */

/* Error Codes */
#define responseErrNotInitialized   -43000  /* Response handling not initialized */
#define responseErrInvalidContext   -43001  /* Invalid response context */
#define responseErrQueueFull        -43002  /* Response queue is full */
#define responseErrHandlerNotFound  -43003  /* Response handler not found */
#define responseErrCallbackFailed   -43004  /* Callback execution failed */
#define responseErrTimeout          -43005  /* Response timed out */
#define responseErrInvalidResponse  -43006  /* Invalid response code */
#define responseErrProcessingFailed -43007  /* Response processing failed */

#ifdef __cplusplus
}
#endif

#endif /* RESPONSE_HANDLING_H */
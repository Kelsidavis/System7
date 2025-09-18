/*
 * ResponseHandling.c
 *
 * Notification response processing and callback management
 * Handles user responses to notifications and executes callbacks
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/ResponseHandling.h"
#include "NotificationManager/NotificationManager.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"

/* Global response handling state */
static ResponseState gResponseState = {0};
static UInt32 gResponseTimeout = RESPONSE_DEFAULT_TIMEOUT;

/* Internal function prototypes */
static OSErr NMInitializeResponseState(void);
static void NMCleanupResponseState(void);
static OSErr NMValidateNotificationForResponse(NMExtendedRecPtr nmExtPtr);
static OSErr NMExecuteCallbackSafely(NMProcPtr callback, NMRecPtr nmReqPtr);
static OSErr NMSetupA5Environment(void);
static void NMRestoreA5Environment(long savedA5);
static ResponseQueuePtr NMCreateResponseQueueEntry(ResponseContextPtr context);
static void NMDestroyResponseQueueEntry(ResponseQueuePtr entry);
static Boolean NMShouldRetryResponse(ResponseContextPtr context);

/*
 * Response Handling Initialization
 */

OSErr NMResponseHandlingInit(void)
{
    OSErr err;

    if (gResponseState.initialized) {
        return noErr;
    }

    /* Initialize response state */
    err = NMInitializeResponseState();
    if (err != noErr) {
        return err;
    }

    /* Initialize response queue */
    err = NMInitResponseQueue();
    if (err != noErr) {
        NMCleanupResponseState();
        return err;
    }

    gResponseState.initialized = true;
    return noErr;
}

void NMResponseHandlingCleanup(void)
{
    if (!gResponseState.initialized) {
        return;
    }

    /* Cleanup response queue */
    NMCleanupResponseQueue();

    /* Cleanup response state */
    NMCleanupResponseState();

    gResponseState.initialized = false;
}

/*
 * Response Processing
 */

OSErr NMTriggerResponse(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse)
{
    ResponseContextPtr context;
    OSErr err;

    if (!gResponseState.initialized) {
        return responseErrNotInitialized;
    }

    if (!nmExtPtr) {
        return nmErrInvalidParameter;
    }

    err = NMValidateNotificationForResponse(nmExtPtr);
    if (err != noErr) {
        return err;
    }

    /* Create response context */
    context = NMCreateResponseContext(nmExtPtr, alertResponse);
    if (!context) {
        return nmErrOutOfMemory;
    }

    /* Process response immediately if synchronous, queue if asynchronous */
    if (gResponseState.asyncProcessing) {
        err = NMQueueResponse(context);
        if (err != noErr) {
            NMDestroyResponseContext(context);
            return err;
        }
    } else {
        err = NMProcessResponse(context);
        NMDestroyResponseContext(context);
    }

    return err;
}

OSErr NMProcessResponse(ResponseContextPtr context)
{
    OSErr err = noErr;
    UInt32 startTime;

    if (!context) {
        return responseErrInvalidContext;
    }

    /* Validate response context */
    err = NMValidateResponseContext(context);
    if (err != noErr) {
        return err;
    }

    startTime = NMGetResponseTimestamp();
    context->processingTime = startTime;
    context->status = responseStatusProcessing;

    /* Execute the appropriate response based on type */
    switch (context->type) {
        case responseTypeCallback:
            if (context->notification->base.nmResp) {
                err = NMExecuteClassicCallback(context->notification);
            }
            break;

        case responseTypeModern:
            if (context->notification->modernCallback) {
                err = NMExecuteModernCallback(context->notification, context->alertResponse);
            }
            break;

        case responseTypePlatform:
            err = NMTriggerPlatformResponse(context->notification, context->alertResponse);
            break;

        case responseTypeInternal:
            /* Internal system response - no callback needed */
            err = noErr;
            break;

        default:
            err = responseErrInvalidContext;
            break;
    }

    /* Update response context */
    context->completionTime = NMGetResponseTimestamp();
    context->lastError = err;

    if (err == noErr) {
        context->status = responseStatusCompleted;
        context->completed = true;
        gResponseState.totalResponses++;
    } else {
        context->status = responseStatusFailed;
        gResponseState.failedResponses++;

        /* Handle response error */
        NMHandleResponseError(context, err);
    }

    return err;
}

OSErr NMQueueResponse(ResponseContextPtr context)
{
    ResponseQueuePtr entry;
    OSErr err;

    if (!gResponseState.initialized) {
        return responseErrNotInitialized;
    }

    if (!context) {
        return responseErrInvalidContext;
    }

    /* Check queue capacity */
    if (gResponseState.queueSize >= gResponseState.maxQueueSize) {
        return responseErrQueueFull;
    }

    /* Create queue entry */
    entry = NMCreateResponseQueueEntry(context);
    if (!entry) {
        return nmErrOutOfMemory;
    }

    /* Add to response queue */
    err = NMAddToResponseQueue(context);
    if (err != noErr) {
        NMDestroyResponseQueueEntry(entry);
        return err;
    }

    /* Set response as pending */
    context->status = responseStatusPending;
    gResponseState.queueSize++;

    return noErr;
}

void NMProcessResponseQueue(void)
{
    ResponseQueuePtr entry, nextEntry;
    OSErr err;

    if (!gResponseState.initialized || !gResponseState.enabled) {
        return;
    }

    /* Check if it's time to process responses */
    UInt32 currentTime = NMGetResponseTimestamp();
    if ((currentTime - gResponseState.lastProcessing) < gResponseState.processingInterval) {
        return;
    }

    gResponseState.lastProcessing = currentTime;

    /* Process timeout checking */
    NMCheckResponseTimeouts();

    /* Process queued responses */
    entry = (ResponseQueuePtr)gResponseState.responseQueue.qHead;
    while (entry) {
        nextEntry = (ResponseQueuePtr)entry->qLink;

        if (entry->context && entry->context->status == responseStatusPending) {
            err = NMProcessSingleResponse(entry);
            if (err != noErr) {
                /* Handle processing error */
                if (NMShouldRetryResponse(entry->context)) {
                    /* Leave in queue for retry */
                } else {
                    /* Remove failed response */
                    NMRemoveFromResponseQueue(entry->context);
                    NMDestroyResponseContext(entry->context);
                    NMDestroyResponseQueueEntry(entry);
                }
            } else {
                /* Remove completed response */
                NMRemoveFromResponseQueue(entry->context);
                NMDestroyResponseContext(entry->context);
                NMDestroyResponseQueueEntry(entry);
            }
        }

        entry = nextEntry;
    }

    /* Cleanup completed responses */
    NMCleanupCompletedResponses();
}

/*
 * Response Context Management
 */

ResponseContextPtr NMCreateResponseContext(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse)
{
    ResponseContextPtr context;

    context = (ResponseContextPtr)NewPtrClear(sizeof(ResponseContext));
    if (!context) {
        return NULL;
    }

    /* Determine response type */
    if (nmExtPtr->modernCallback) {
        context->type = responseTypeModern;
    } else if (nmExtPtr->base.nmResp) {
        context->type = responseTypeCallback;
    } else {
        context->type = responseTypeInternal;
    }

    /* Initialize context */
    context->status = responseStatusPending;
    context->notification = nmExtPtr;
    context->responseTime = NMGetResponseTimestamp();
    context->processingTime = 0;
    context->completionTime = 0;
    context->alertResponse = alertResponse;
    context->userData = nmExtPtr->base.nmRefCon;
    context->context = nmExtPtr->callbackContext;
    context->lastError = noErr;
    context->async = gResponseState.asyncProcessing;
    context->completed = false;

    return context;
}

void NMDestroyResponseContext(ResponseContextPtr context)
{
    if (context) {
        DisposePtr((Ptr)context);
    }
}

/*
 * Classic Mac OS Response Handling
 */

OSErr NMExecuteClassicCallback(NMExtendedRecPtr nmExtPtr)
{
    OSErr err;
    long savedA5;

    if (!nmExtPtr || !nmExtPtr->base.nmResp) {
        return responseErrInvalidContext;
    }

    /* Setup A5 environment for callback */
    err = NMSetupA5Environment();
    if (err != noErr) {
        return err;
    }

    savedA5 = SetCurrentA5();

    /* Execute callback safely */
    err = NMExecuteCallbackSafely(nmExtPtr->base.nmResp, &nmExtPtr->base);

    /* Restore A5 environment */
    NMRestoreA5Environment(savedA5);

    return err;
}

OSErr NMCallOriginalResponse(NMRecPtr nmReqPtr)
{
    if (!nmReqPtr || !nmReqPtr->nmResp) {
        return responseErrInvalidContext;
    }

    /* Call the original Mac OS response routine */
    /* This would involve setting up the proper environment and calling the procedure */

    /* For safety, we'll use a structured exception handling approach */
    OSErr err = noErr;

    /* Set up exception handling */
    /* Platform-specific exception handling would go here */

    /* Call the response procedure */
    nmReqPtr->nmResp(nmReqPtr);

    return err;
}

/*
 * Modern Response Handling
 */

OSErr NMExecuteModernCallback(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse)
{
    if (!nmExtPtr || !nmExtPtr->modernCallback) {
        return responseErrInvalidContext;
    }

    /* Execute modern callback with proper error handling */
    nmExtPtr->modernCallback(&nmExtPtr->base, nmExtPtr->callbackContext);

    return noErr;
}

OSErr NMRegisterModernHandler(NotificationCallback callback, void *context)
{
    int i;

    if (!gResponseState.initialized) {
        return responseErrNotInitialized;
    }

    if (!callback) {
        return nmErrInvalidParameter;
    }

    /* Find empty slot */
    for (i = 0; i < RESPONSE_MAX_HANDLERS; i++) {
        if (!gResponseState.handlers[i].active) {
            gResponseState.handlers[i].type = responseTypeModern;
            gResponseState.handlers[i].modernCallback = callback;
            gResponseState.handlers[i].context = context;
            gResponseState.handlers[i].active = true;
            gResponseState.handlers[i].registrationTime = NMGetResponseTimestamp();
            gResponseState.handlerCount++;
            return noErr;
        }
    }

    return responseErrQueueFull;
}

OSErr NMUnregisterModernHandler(NotificationCallback callback)
{
    int i;

    if (!gResponseState.initialized) {
        return responseErrNotInitialized;
    }

    for (i = 0; i < RESPONSE_MAX_HANDLERS; i++) {
        if (gResponseState.handlers[i].active &&
            gResponseState.handlers[i].modernCallback == callback) {
            gResponseState.handlers[i].active = false;
            gResponseState.handlers[i].modernCallback = NULL;
            gResponseState.handlers[i].context = NULL;
            gResponseState.handlerCount--;
            return noErr;
        }
    }

    return responseErrHandlerNotFound;
}

/*
 * Response Queue Management
 */

OSErr NMInitResponseQueue(void)
{
    gResponseState.responseQueue.qFlags = 0;
    gResponseState.responseQueue.qHead = NULL;
    gResponseState.responseQueue.qTail = NULL;
    gResponseState.queueSize = 0;
    gResponseState.maxQueueSize = RESPONSE_QUEUE_MAX_SIZE;
    gResponseState.lastProcessing = NMGetResponseTimestamp();
    gResponseState.processingInterval = RESPONSE_PROCESSING_INTERVAL;

    return noErr;
}

void NMCleanupResponseQueue(void)
{
    /* Flush all pending responses */
    NMFlushResponseQueue();
}

OSErr NMAddToResponseQueue(ResponseContextPtr context)
{
    ResponseQueuePtr entry;

    if (!context) {
        return responseErrInvalidContext;
    }

    entry = NMCreateResponseQueueEntry(context);
    if (!entry) {
        return nmErrOutOfMemory;
    }

    /* Add to end of queue */
    Enqueue((QElemPtr)entry, &gResponseState.responseQueue);

    return noErr;
}

OSErr NMRemoveFromResponseQueue(ResponseContextPtr context)
{
    ResponseQueuePtr entry;
    OSErr err;

    if (!context) {
        return responseErrInvalidContext;
    }

    /* Find the queue entry for this context */
    entry = (ResponseQueuePtr)gResponseState.responseQueue.qHead;
    while (entry) {
        if (entry->context == context) {
            err = Dequeue((QElemPtr)entry, &gResponseState.responseQueue);
            if (err == noErr) {
                gResponseState.queueSize--;
            }
            return err;
        }
        entry = (ResponseQueuePtr)entry->qLink;
    }

    return responseErrHandlerNotFound;
}

OSErr NMFlushResponseQueue(void)
{
    ResponseQueuePtr entry, nextEntry;

    entry = (ResponseQueuePtr)gResponseState.responseQueue.qHead;
    while (entry) {
        nextEntry = (ResponseQueuePtr)entry->qLink;

        /* Remove from queue */
        Dequeue((QElemPtr)entry, &gResponseState.responseQueue);

        /* Cleanup context and entry */
        if (entry->context) {
            NMDestroyResponseContext(entry->context);
        }
        NMDestroyResponseQueueEntry(entry);

        entry = nextEntry;
    }

    gResponseState.queueSize = 0;
    return noErr;
}

/*
 * Response Timing and Validation
 */

OSErr NMCheckResponseTimeouts(void)
{
    ResponseQueuePtr entry, nextEntry;
    UInt32 currentTime;

    if (!gResponseState.initialized) {
        return responseErrNotInitialized;
    }

    currentTime = NMGetResponseTimestamp();

    entry = (ResponseQueuePtr)gResponseState.responseQueue.qHead;
    while (entry) {
        nextEntry = (ResponseQueuePtr)entry->qLink;

        if (entry->context && NMIsResponseTimedOut(entry->context)) {
            /* Handle timeout */
            entry->context->status = responseStatusTimeout;
            entry->context->lastError = responseErrTimeout;

            /* Remove timed out response */
            NMRemoveFromResponseQueue(entry->context);
            NMDestroyResponseContext(entry->context);
            NMDestroyResponseQueueEntry(entry);

            gResponseState.failedResponses++;
        }

        entry = nextEntry;
    }

    return noErr;
}

Boolean NMIsResponseTimedOut(ResponseContextPtr context)
{
    UInt32 currentTime;

    if (!context || gResponseTimeout == 0) {
        return false;
    }

    currentTime = NMGetResponseTimestamp();
    return (currentTime - context->responseTime) > gResponseTimeout;
}

OSErr NMValidateResponseContext(ResponseContextPtr context)
{
    if (!context) {
        return responseErrInvalidContext;
    }

    if (!context->notification) {
        return responseErrInvalidContext;
    }

    if (!NMIsValidResponse(context->alertResponse)) {
        return responseErrInvalidResponse;
    }

    return noErr;
}

Boolean NMIsValidResponse(AlertResponse response)
{
    switch (response) {
        case alertResponseOK:
        case alertResponseCancel:
        case alertResponseYes:
        case alertResponseNo:
        case alertResponseTimeout:
        case alertResponseError:
        case alertResponseDismissed:
            return true;
        default:
            return false;
    }
}

/*
 * Error Handling
 */

OSErr NMHandleResponseError(ResponseContextPtr context, OSErr error)
{
    if (!context) {
        return responseErrInvalidContext;
    }

    context->lastError = error;

    /* Log error if logging is enabled */
    /* Error logging would be implemented here */

    /* Determine if we should retry */
    if (NMShouldRetryResponse(context)) {
        return NMRetryResponse(context);
    }

    return error;
}

OSErr NMRetryResponse(ResponseContextPtr context)
{
    if (!context) {
        return responseErrInvalidContext;
    }

    /* Reset status for retry */
    context->status = responseStatusPending;
    context->processingTime = 0;
    context->completionTime = 0;

    /* Add back to queue if using async processing */
    if (gResponseState.asyncProcessing) {
        return NMQueueResponse(context);
    } else {
        return NMProcessResponse(context);
    }
}

/*
 * Internal Implementation Functions
 */

static OSErr NMInitializeResponseState(void)
{
    int i;

    gResponseState.enabled = true;
    gResponseState.queueSize = 0;
    gResponseState.maxQueueSize = RESPONSE_QUEUE_MAX_SIZE;
    gResponseState.lastProcessing = 0;
    gResponseState.processingInterval = RESPONSE_PROCESSING_INTERVAL;
    gResponseState.handlerCount = 0;
    gResponseState.asyncProcessing = true;
    gResponseState.totalResponses = 0;
    gResponseState.failedResponses = 0;

    /* Initialize handlers array */
    for (i = 0; i < RESPONSE_MAX_HANDLERS; i++) {
        gResponseState.handlers[i].active = false;
        gResponseState.handlers[i].callback = NULL;
        gResponseState.handlers[i].modernCallback = NULL;
        gResponseState.handlers[i].context = NULL;
    }

    return noErr;
}

static void NMCleanupResponseState(void)
{
    int i;

    /* Cleanup handlers */
    for (i = 0; i < RESPONSE_MAX_HANDLERS; i++) {
        gResponseState.handlers[i].active = false;
        gResponseState.handlers[i].callback = NULL;
        gResponseState.handlers[i].modernCallback = NULL;
        gResponseState.handlers[i].context = NULL;
    }

    gResponseState.handlerCount = 0;
    gResponseState.enabled = false;
}

static OSErr NMValidateNotificationForResponse(NMExtendedRecPtr nmExtPtr)
{
    if (!nmExtPtr) {
        return nmErrInvalidParameter;
    }

    /* Ensure notification is in a valid state for response */
    if (nmExtPtr->status != nmStatusDisplayed && nmExtPtr->status != nmStatusPosted) {
        return nmErrInvalidRecord;
    }

    return noErr;
}

static OSErr NMExecuteCallbackSafely(NMProcPtr callback, NMRecPtr nmReqPtr)
{
    if (!callback || !nmReqPtr) {
        return responseErrInvalidContext;
    }

    /* Execute callback with proper error handling */
    /* Platform-specific safe execution would go here */

    callback(nmReqPtr);

    return noErr;
}

static OSErr NMSetupA5Environment(void)
{
    /* Setup proper A5 world for classic Mac OS callbacks */
    /* This would be platform-specific */
    return noErr;
}

static void NMRestoreA5Environment(long savedA5)
{
    /* Restore A5 world */
    /* This would be platform-specific */
    SetA5(savedA5);
}

static ResponseQueuePtr NMCreateResponseQueueEntry(ResponseContextPtr context)
{
    ResponseQueuePtr entry;

    entry = (ResponseQueuePtr)NewPtrClear(sizeof(ResponseQueueEntry));
    if (!entry) {
        return NULL;
    }

    entry->qLink = NULL;
    entry->qType = 0;
    entry->context = context;
    entry->priority = (context->notification) ? context->notification->priority : nmPriorityNormal;
    entry->timestamp = NMGetResponseTimestamp();
    entry->next = NULL;

    return entry;
}

static void NMDestroyResponseQueueEntry(ResponseQueuePtr entry)
{
    if (entry) {
        DisposePtr((Ptr)entry);
    }
}

static Boolean NMShouldRetryResponse(ResponseContextPtr context)
{
    /* Simple retry logic - could be enhanced */
    return false; /* For now, don't retry failed responses */
}

OSErr NMProcessSingleResponse(ResponseQueuePtr entry)
{
    if (!entry || !entry->context) {
        return responseErrInvalidContext;
    }

    return NMProcessResponse(entry->context);
}

void NMCleanupCompletedResponses(void)
{
    ResponseQueuePtr entry, nextEntry;

    entry = (ResponseQueuePtr)gResponseState.responseQueue.qHead;
    while (entry) {
        nextEntry = (ResponseQueuePtr)entry->qLink;

        if (entry->context &&
            (entry->context->status == responseStatusCompleted ||
             entry->context->status == responseStatusFailed)) {

            /* Remove completed response */
            NMRemoveFromResponseQueue(entry->context);
            NMDestroyResponseContext(entry->context);
            NMDestroyResponseQueueEntry(entry);
        }

        entry = nextEntry;
    }
}

/*
 * Utility Functions
 */

UInt32 NMGetResponseTimestamp(void)
{
    UInt32 ticks;
    Microseconds((UnsignedWide *)&ticks);
    return ticks;
}

StringPtr NMGetResponseTypeName(ResponseType type)
{
    static Str63 typeName;

    switch (type) {
        case responseTypeCallback:
            strcpy((char *)&typeName[1], "Classic Callback");
            break;
        case responseTypeModern:
            strcpy((char *)&typeName[1], "Modern Callback");
            break;
        case responseTypePlatform:
            strcpy((char *)&typeName[1], "Platform Response");
            break;
        case responseTypeInternal:
            strcpy((char *)&typeName[1], "Internal Response");
            break;
        default:
            strcpy((char *)&typeName[1], "Unknown Type");
            break;
    }

    typeName[0] = strlen((char *)&typeName[1]);
    return typeName;
}

/*
 * Weak stubs for platform-specific functions
 */

#pragma weak NMTriggerPlatformResponse
OSErr NMTriggerPlatformResponse(NMExtendedRecPtr nmExtPtr, AlertResponse alertResponse)
{
    return noErr;
}

#pragma weak NMRegisterPlatformResponseHandler
OSErr NMRegisterPlatformResponseHandler(void *platformHandler, void *context)
{
    return noErr;
}

#pragma weak NMUnregisterPlatformResponseHandler
OSErr NMUnregisterPlatformResponseHandler(void)
{
    return noErr;
}
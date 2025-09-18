/*
 * AsyncIO.c
 * System 7.1 Device Manager - Asynchronous I/O Operations
 *
 * Implements asynchronous I/O operations, request queuing, and completion
 * handling for the Device Manager. This module provides the foundation
 * for non-blocking device operations.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "DeviceManager/DeviceIO.h"
#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DriverInterface.h"
#include "DeviceManager/UnitTable.h"
#include "MemoryManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

/* =============================================================================
 * Constants and Configuration
 * ============================================================================= */

#define MAX_ASYNC_REQUESTS          1024
#define MAX_IO_QUEUES              64
#define DEFAULT_QUEUE_SIZE         32
#define ASYNC_WORKER_THREADS       4
#define COMPLETION_CHECK_INTERVAL  10000  /* microseconds */

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

typedef struct AsyncIOManager {
    AsyncIORequestPtr  *requests;        /* Array of async requests */
    uint32_t            requestCount;    /* Number of active requests */
    uint32_t            maxRequests;     /* Maximum requests */
    uint32_t            nextRequestID;   /* Next request ID */

    IORequestQueuePtr  *queues;          /* Array of I/O queues */
    uint32_t            queueCount;      /* Number of queues */
    uint32_t            maxQueues;       /* Maximum queues */

    pthread_mutex_t     requestMutex;    /* Request list mutex */
    pthread_mutex_t     queueMutex;      /* Queue list mutex */
    pthread_cond_t      workerCondition; /* Worker thread condition */

    pthread_t          *workerThreads;   /* Worker thread array */
    uint32_t            numWorkers;      /* Number of worker threads */
    bool                shouldShutdown;  /* Shutdown flag */

    /* Statistics */
    uint32_t            requestsCreated;
    uint32_t            requestsCompleted;
    uint32_t            requestsCancelled;
    uint32_t            queueOverflows;
    uint32_t            workerActivations;
} AsyncIOManager;

/* =============================================================================
 * Global Variables
 * ============================================================================= */

static AsyncIOManager *g_asyncManager = NULL;
static bool g_asyncSystemInitialized = false;

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static int16_t InitializeAsyncIOManager(void);
static void ShutdownAsyncIOManager(void);
static void *AsyncWorkerThread(void *arg);
static int16_t ProcessAsyncRequest(AsyncIORequestPtr request);
static int16_t AddAsyncRequest(AsyncIORequestPtr request);
static int16_t RemoveAsyncRequest(uint32_t requestID);
static AsyncIORequestPtr FindAsyncRequest(uint32_t requestID);
static int16_t ExecuteAsyncOperation(AsyncIORequestPtr request);
static void CompleteAsyncRequest(AsyncIORequestPtr request, int16_t result);
static uint32_t GenerateRequestID(void);
static uint32_t GetTimestamp(void);

/* =============================================================================
 * Async I/O System Initialization
 * ============================================================================= */

int16_t AsyncIO_Initialize(void)
{
    if (g_asyncSystemInitialized) {
        return noErr;
    }

    int16_t error = InitializeAsyncIOManager();
    if (error != noErr) {
        return error;
    }

    g_asyncSystemInitialized = true;
    return noErr;
}

void AsyncIO_Shutdown(void)
{
    if (!g_asyncSystemInitialized) {
        return;
    }

    ShutdownAsyncIOManager();
    g_asyncSystemInitialized = false;
}

static int16_t InitializeAsyncIOManager(void)
{
    g_asyncManager = (AsyncIOManager*)malloc(sizeof(AsyncIOManager));
    if (g_asyncManager == NULL) {
        return memFullErr;
    }

    memset(g_asyncManager, 0, sizeof(AsyncIOManager));

    /* Initialize request array */
    g_asyncManager->maxRequests = MAX_ASYNC_REQUESTS;
    g_asyncManager->requests = (AsyncIORequestPtr*)malloc(
        sizeof(AsyncIORequestPtr) * g_asyncManager->maxRequests);
    if (g_asyncManager->requests == NULL) {
        free(g_asyncManager);
        g_asyncManager = NULL;
        return memFullErr;
    }

    memset(g_asyncManager->requests, 0,
           sizeof(AsyncIORequestPtr) * g_asyncManager->maxRequests);

    /* Initialize queue array */
    g_asyncManager->maxQueues = MAX_IO_QUEUES;
    g_asyncManager->queues = (IORequestQueuePtr*)malloc(
        sizeof(IORequestQueuePtr) * g_asyncManager->maxQueues);
    if (g_asyncManager->queues == NULL) {
        free(g_asyncManager->requests);
        free(g_asyncManager);
        g_asyncManager = NULL;
        return memFullErr;
    }

    memset(g_asyncManager->queues, 0,
           sizeof(IORequestQueuePtr) * g_asyncManager->maxQueues);

    /* Initialize synchronization objects */
    if (pthread_mutex_init(&g_asyncManager->requestMutex, NULL) != 0) {
        free(g_asyncManager->queues);
        free(g_asyncManager->requests);
        free(g_asyncManager);
        g_asyncManager = NULL;
        return dsIOCoreErr;
    }

    if (pthread_mutex_init(&g_asyncManager->queueMutex, NULL) != 0) {
        pthread_mutex_destroy(&g_asyncManager->requestMutex);
        free(g_asyncManager->queues);
        free(g_asyncManager->requests);
        free(g_asyncManager);
        g_asyncManager = NULL;
        return dsIOCoreErr;
    }

    if (pthread_cond_init(&g_asyncManager->workerCondition, NULL) != 0) {
        pthread_mutex_destroy(&g_asyncManager->queueMutex);
        pthread_mutex_destroy(&g_asyncManager->requestMutex);
        free(g_asyncManager->queues);
        free(g_asyncManager->requests);
        free(g_asyncManager);
        g_asyncManager = NULL;
        return dsIOCoreErr;
    }

    /* Initialize worker threads */
    g_asyncManager->numWorkers = ASYNC_WORKER_THREADS;
    g_asyncManager->workerThreads = (pthread_t*)malloc(
        sizeof(pthread_t) * g_asyncManager->numWorkers);
    if (g_asyncManager->workerThreads == NULL) {
        pthread_cond_destroy(&g_asyncManager->workerCondition);
        pthread_mutex_destroy(&g_asyncManager->queueMutex);
        pthread_mutex_destroy(&g_asyncManager->requestMutex);
        free(g_asyncManager->queues);
        free(g_asyncManager->requests);
        free(g_asyncManager);
        g_asyncManager = NULL;
        return memFullErr;
    }

    g_asyncManager->shouldShutdown = false;
    g_asyncManager->nextRequestID = 1;

    /* Start worker threads */
    for (uint32_t i = 0; i < g_asyncManager->numWorkers; i++) {
        if (pthread_create(&g_asyncManager->workerThreads[i], NULL,
                          AsyncWorkerThread, g_asyncManager) != 0) {
            /* Clean up partially created threads */
            g_asyncManager->shouldShutdown = true;
            pthread_cond_broadcast(&g_asyncManager->workerCondition);

            for (uint32_t j = 0; j < i; j++) {
                pthread_join(g_asyncManager->workerThreads[j], NULL);
            }

            free(g_asyncManager->workerThreads);
            pthread_cond_destroy(&g_asyncManager->workerCondition);
            pthread_mutex_destroy(&g_asyncManager->queueMutex);
            pthread_mutex_destroy(&g_asyncManager->requestMutex);
            free(g_asyncManager->queues);
            free(g_asyncManager->requests);
            free(g_asyncManager);
            g_asyncManager = NULL;
            return dsIOCoreErr;
        }
    }

    return noErr;
}

static void ShutdownAsyncIOManager(void)
{
    if (g_asyncManager == NULL) {
        return;
    }

    /* Signal worker threads to shutdown */
    pthread_mutex_lock(&g_asyncManager->requestMutex);
    g_asyncManager->shouldShutdown = true;
    pthread_cond_broadcast(&g_asyncManager->workerCondition);
    pthread_mutex_unlock(&g_asyncManager->requestMutex);

    /* Wait for worker threads to finish */
    for (uint32_t i = 0; i < g_asyncManager->numWorkers; i++) {
        pthread_join(g_asyncManager->workerThreads[i], NULL);
    }

    /* Cancel all pending requests */
    for (uint32_t i = 0; i < g_asyncManager->requestCount; i++) {
        AsyncIORequestPtr request = g_asyncManager->requests[i];
        if (request != NULL) {
            CancelAsyncIORequest(request);
            DestroyAsyncIORequest(request);
        }
    }

    /* Destroy all queues */
    for (uint32_t i = 0; i < g_asyncManager->queueCount; i++) {
        if (g_asyncManager->queues[i] != NULL) {
            DestroyIOQueue(g_asyncManager->queues[i]);
        }
    }

    /* Clean up resources */
    free(g_asyncManager->workerThreads);
    free(g_asyncManager->queues);
    free(g_asyncManager->requests);

    pthread_cond_destroy(&g_asyncManager->workerCondition);
    pthread_mutex_destroy(&g_asyncManager->queueMutex);
    pthread_mutex_destroy(&g_asyncManager->requestMutex);

    free(g_asyncManager);
    g_asyncManager = NULL;
}

/* =============================================================================
 * Async I/O Request Management
 * ============================================================================= */

AsyncIORequestPtr CreateAsyncIORequest(IOParamPtr pb, uint32_t priority,
                                       AsyncIOCompletionProc completion)
{
    if (!g_asyncSystemInitialized || pb == NULL) {
        return NULL;
    }

    AsyncIORequestPtr request = (AsyncIORequestPtr)malloc(sizeof(AsyncIORequest));
    if (request == NULL) {
        return NULL;
    }

    memset(request, 0, sizeof(AsyncIORequest));

    /* Copy parameter block */
    memcpy(&request->param.pb, &pb->pb, sizeof(ParamBlockHeader));
    memcpy(&request->param, pb, sizeof(IOParam));

    /* Initialize request fields */
    request->requestID = GenerateRequestID();
    request->priority = priority;
    request->isCancelled = false;
    request->isCompleted = false;
    request->context = NULL;

    /* Extended fields */
    request->param.ioTimeout = 0;  /* No timeout by default */
    request->param.ioFlags = 0;
    request->param.ioUserData = NULL;
    request->param.ioTimestamp = GetTimestamp();

    /* Add to request list */
    int16_t error = AddAsyncRequest(request);
    if (error != noErr) {
        free(request);
        return NULL;
    }

    g_asyncManager->requestsCreated++;

    return request;
}

int16_t CancelAsyncIORequest(AsyncIORequestPtr request)
{
    if (!g_asyncSystemInitialized || request == NULL) {
        return paramErr;
    }

    pthread_mutex_lock(&g_asyncManager->requestMutex);

    if (!request->isCompleted && !request->isCancelled) {
        request->isCancelled = true;
        request->param.pb.ioResult = abortErr;
        request->isCompleted = true;

        g_asyncManager->requestsCancelled++;

        /* Signal completion if completion routine exists */
        CompleteAsyncRequest(request, abortErr);
    }

    pthread_mutex_unlock(&g_asyncManager->requestMutex);

    return noErr;
}

int16_t WaitForAsyncIO(AsyncIORequestPtr request, uint32_t timeout)
{
    if (!g_asyncSystemInitialized || request == NULL) {
        return paramErr;
    }

    uint32_t startTime = GetTimestamp();
    uint32_t endTime = startTime + timeout;

    /* Poll for completion */
    while (!request->isCompleted) {
        if (timeout > 0 && GetTimestamp() >= endTime) {
            return ioTimeout;
        }

        /* Sleep briefly to avoid busy waiting */
        usleep(COMPLETION_CHECK_INTERVAL);

        /* Process any pending completions */
        ProcessPendingCompletions();
    }

    return request->param.pb.ioResult;
}

void DestroyAsyncIORequest(AsyncIORequestPtr request)
{
    if (request == NULL) {
        return;
    }

    /* Remove from request list if still there */
    RemoveAsyncRequest(request->requestID);

    free(request);
}

/* =============================================================================
 * I/O Queue Operations
 * ============================================================================= */

IORequestQueuePtr CreateIOQueue(uint32_t maxSize)
{
    if (!g_asyncSystemInitialized) {
        return NULL;
    }

    IORequestQueuePtr queue = (IORequestQueuePtr)malloc(sizeof(IORequestQueue));
    if (queue == NULL) {
        return NULL;
    }

    memset(queue, 0, sizeof(IORequestQueue));

    queue->head = NULL;
    queue->tail = NULL;
    queue->current = NULL;
    queue->count = 0;
    queue->maxSize = (maxSize > 0) ? maxSize : DEFAULT_QUEUE_SIZE;
    queue->isPaused = false;

    /* Add to queue list */
    pthread_mutex_lock(&g_asyncManager->queueMutex);

    if (g_asyncManager->queueCount < g_asyncManager->maxQueues) {
        g_asyncManager->queues[g_asyncManager->queueCount] = queue;
        g_asyncManager->queueCount++;
    } else {
        pthread_mutex_unlock(&g_asyncManager->queueMutex);
        free(queue);
        return NULL;
    }

    pthread_mutex_unlock(&g_asyncManager->queueMutex);

    return queue;
}

void DestroyIOQueue(IORequestQueuePtr queue)
{
    if (queue == NULL) {
        return;
    }

    /* Remove all queued entries */
    IOQueueEntryPtr entry = queue->head;
    while (entry != NULL) {
        IOQueueEntryPtr next = entry->next;
        free(entry);
        entry = next;
    }

    /* Remove from queue list */
    pthread_mutex_lock(&g_asyncManager->queueMutex);

    for (uint32_t i = 0; i < g_asyncManager->queueCount; i++) {
        if (g_asyncManager->queues[i] == queue) {
            /* Shift remaining queues down */
            for (uint32_t j = i; j < g_asyncManager->queueCount - 1; j++) {
                g_asyncManager->queues[j] = g_asyncManager->queues[j + 1];
            }
            g_asyncManager->queueCount--;
            break;
        }
    }

    pthread_mutex_unlock(&g_asyncManager->queueMutex);

    free(queue);
}

int16_t EnqueueIORequest(IORequestQueuePtr queue, IOParamPtr pb, uint32_t priority)
{
    if (queue == NULL || pb == NULL) {
        return paramErr;
    }

    if (queue->isPaused) {
        return queueIsPaused;
    }

    if (queue->count >= queue->maxSize) {
        g_asyncManager->queueOverflows++;
        return queueOverflow;
    }

    /* Create queue entry */
    IOQueueEntryPtr entry = (IOQueueEntryPtr)malloc(sizeof(IOQueueEntry));
    if (entry == NULL) {
        return memFullErr;
    }

    entry->next = NULL;
    entry->pb = pb;
    entry->priority = priority;
    entry->timestamp = GetTimestamp();
    entry->isActive = false;

    /* Insert based on priority (higher priority first) */
    if (queue->head == NULL) {
        /* Empty queue */
        queue->head = entry;
        queue->tail = entry;
    } else if (priority > queue->head->priority) {
        /* Insert at head */
        entry->next = queue->head;
        queue->head = entry;
    } else {
        /* Find insertion point */
        IOQueueEntryPtr current = queue->head;
        while (current->next != NULL && current->next->priority >= priority) {
            current = current->next;
        }

        entry->next = current->next;
        current->next = entry;

        if (entry->next == NULL) {
            queue->tail = entry;
        }
    }

    queue->count++;

    /* Signal worker threads */
    pthread_cond_signal(&g_asyncManager->workerCondition);

    return noErr;
}

IOParamPtr DequeueIORequest(IORequestQueuePtr queue)
{
    if (queue == NULL || queue->head == NULL || queue->isPaused) {
        return NULL;
    }

    IOQueueEntryPtr entry = queue->head;
    IOParamPtr pb = entry->pb;

    queue->head = entry->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    queue->count--;
    free(entry);

    return pb;
}

IOParamPtr PeekIOQueue(IORequestQueuePtr queue)
{
    if (queue == NULL || queue->head == NULL) {
        return NULL;
    }

    return queue->head->pb;
}

int32_t GetIOQueueSize(IORequestQueuePtr queue)
{
    return queue ? queue->count : 0;
}

void PauseIOQueue(IORequestQueuePtr queue)
{
    if (queue != NULL) {
        queue->isPaused = true;
    }
}

void ResumeIOQueue(IORequestQueuePtr queue)
{
    if (queue != NULL) {
        queue->isPaused = false;
        /* Signal worker threads in case they're waiting */
        pthread_cond_signal(&g_asyncManager->workerCondition);
    }
}

/* =============================================================================
 * Worker Thread Implementation
 * ============================================================================= */

static void *AsyncWorkerThread(void *arg)
{
    AsyncIOManager *manager = (AsyncIOManager*)arg;

    while (!manager->shouldShutdown) {
        pthread_mutex_lock(&manager->requestMutex);

        /* Wait for work */
        while (!manager->shouldShutdown && manager->requestCount == 0) {
            pthread_cond_wait(&manager->workerCondition, &manager->requestMutex);
        }

        if (manager->shouldShutdown) {
            pthread_mutex_unlock(&manager->requestMutex);
            break;
        }

        /* Find a request to process */
        AsyncIORequestPtr request = NULL;
        for (uint32_t i = 0; i < manager->requestCount; i++) {
            AsyncIORequestPtr candidate = manager->requests[i];
            if (candidate != NULL && !candidate->isCompleted && !candidate->isCancelled) {
                request = candidate;
                break;
            }
        }

        pthread_mutex_unlock(&manager->requestMutex);

        if (request != NULL) {
            manager->workerActivations++;
            ProcessAsyncRequest(request);
        }

        /* Brief yield to other threads */
        usleep(1000);
    }

    return NULL;
}

static int16_t ProcessAsyncRequest(AsyncIORequestPtr request)
{
    if (request == NULL) {
        return paramErr;
    }

    /* Check if request was cancelled */
    if (request->isCancelled) {
        CompleteAsyncRequest(request, abortErr);
        return abortErr;
    }

    /* Execute the I/O operation */
    int16_t result = ExecuteAsyncOperation(request);

    /* Complete the request */
    CompleteAsyncRequest(request, result);

    return result;
}

static int16_t ExecuteAsyncOperation(AsyncIORequestPtr request)
{
    if (request == NULL) {
        return paramErr;
    }

    IOParamPtr pb = (IOParamPtr)&request->param;

    /* Determine operation type from trap word */
    uint16_t trapWord = pb->pb.ioTrap;
    uint8_t operation = trapWord & 0xFF;

    /* Get DCE for the driver */
    DCEHandle dceHandle = GetDCtlEntry(pb->ioRefNum);
    if (dceHandle == NULL) {
        return badUnitErr;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL) {
        return unitEmptyErr;
    }

    /* Execute the appropriate operation */
    int16_t result = noErr;

    switch (operation) {
        case aRdCmd:
            result = CallDriverPrime(pb, dce);
            break;

        case aWrCmd:
            result = CallDriverPrime(pb, dce);
            break;

        default:
            /* For other operations, call appropriate driver routine */
            if (trapWord & (1 << 9)) { /* Control/Status operation */
                CntrlParamPtr cntrlPB = (CntrlParamPtr)pb;
                if (trapWord & (1 << 8)) { /* Status */
                    result = CallDriverStatus(cntrlPB, dce);
                } else { /* Control */
                    result = CallDriverControl(cntrlPB, dce);
                }
            } else {
                /* Open/Close/Kill operation */
                result = CallDriverOpen(pb, dce); /* Default to open */
            }
            break;
    }

    return result;
}

static void CompleteAsyncRequest(AsyncIORequestPtr request, int16_t result)
{
    if (request == NULL) {
        return;
    }

    pthread_mutex_lock(&g_asyncManager->requestMutex);

    request->param.pb.ioResult = result;
    request->isCompleted = true;

    g_asyncManager->requestsCompleted++;

    pthread_mutex_unlock(&g_asyncManager->requestMutex);

    /* Call completion routine if provided */
    /* This would be done in interrupt context in real Mac OS */
    if (request->param.pb.ioCompletion != NULL) {
        IOCompletionProc completion = (IOCompletionProc)request->param.pb.ioCompletion;
        completion((IOParamPtr)&request->param);
    }
}

/* =============================================================================
 * Request Management Helpers
 * ============================================================================= */

static int16_t AddAsyncRequest(AsyncIORequestPtr request)
{
    if (request == NULL) {
        return paramErr;
    }

    pthread_mutex_lock(&g_asyncManager->requestMutex);

    if (g_asyncManager->requestCount >= g_asyncManager->maxRequests) {
        pthread_mutex_unlock(&g_asyncManager->requestMutex);
        return tooManyRequests;
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < g_asyncManager->maxRequests; i++) {
        if (g_asyncManager->requests[i] == NULL) {
            g_asyncManager->requests[i] = request;
            g_asyncManager->requestCount++;
            pthread_cond_signal(&g_asyncManager->workerCondition);
            pthread_mutex_unlock(&g_asyncManager->requestMutex);
            return noErr;
        }
    }

    pthread_mutex_unlock(&g_asyncManager->requestMutex);
    return tooManyRequests;
}

static int16_t RemoveAsyncRequest(uint32_t requestID)
{
    pthread_mutex_lock(&g_asyncManager->requestMutex);

    for (uint32_t i = 0; i < g_asyncManager->maxRequests; i++) {
        AsyncIORequestPtr request = g_asyncManager->requests[i];
        if (request != NULL && request->requestID == requestID) {
            g_asyncManager->requests[i] = NULL;
            g_asyncManager->requestCount--;
            pthread_mutex_unlock(&g_asyncManager->requestMutex);
            return noErr;
        }
    }

    pthread_mutex_unlock(&g_asyncManager->requestMutex);
    return fnfErr;
}

static AsyncIORequestPtr FindAsyncRequest(uint32_t requestID)
{
    pthread_mutex_lock(&g_asyncManager->requestMutex);

    for (uint32_t i = 0; i < g_asyncManager->maxRequests; i++) {
        AsyncIORequestPtr request = g_asyncManager->requests[i];
        if (request != NULL && request->requestID == requestID) {
            pthread_mutex_unlock(&g_asyncManager->requestMutex);
            return request;
        }
    }

    pthread_mutex_unlock(&g_asyncManager->requestMutex);
    return NULL;
}

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

static uint32_t GenerateRequestID(void)
{
    return g_asyncManager->nextRequestID++;
}

static uint32_t GetTimestamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void GetAsyncIOStatistics(void *stats)
{
    if (!g_asyncSystemInitialized || stats == NULL) {
        return;
    }

    /* Copy statistics */
    struct {
        uint32_t requestsCreated;
        uint32_t requestsCompleted;
        uint32_t requestsCancelled;
        uint32_t queueOverflows;
        uint32_t workerActivations;
        uint32_t activeRequests;
        uint32_t activeQueues;
    } *asyncStats = stats;

    pthread_mutex_lock(&g_asyncManager->requestMutex);
    pthread_mutex_lock(&g_asyncManager->queueMutex);

    asyncStats->requestsCreated = g_asyncManager->requestsCreated;
    asyncStats->requestsCompleted = g_asyncManager->requestsCompleted;
    asyncStats->requestsCancelled = g_asyncManager->requestsCancelled;
    asyncStats->queueOverflows = g_asyncManager->queueOverflows;
    asyncStats->workerActivations = g_asyncManager->workerActivations;
    asyncStats->activeRequests = g_asyncManager->requestCount;
    asyncStats->activeQueues = g_asyncManager->queueCount;

    pthread_mutex_unlock(&g_asyncManager->queueMutex);
    pthread_mutex_unlock(&g_asyncManager->requestMutex);
}

uint32_t GetActiveAsyncRequestCount(void)
{
    if (!g_asyncSystemInitialized) {
        return 0;
    }

    return g_asyncManager->requestCount;
}

bool IsAsyncIOSystemReady(void)
{
    return g_asyncSystemInitialized && g_asyncManager != NULL;
}
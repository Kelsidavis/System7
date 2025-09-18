/*
 * DeferredTasks.c
 *
 * Deferred Task Queue Management for System 7.1 Time Manager
 *
 * This file implements deferred task queue management, allowing tasks to be
 * scheduled for execution at a later time without blocking the current
 * execution context. This provides safe asynchronous task execution.
 */

#include "DeferredTasks.h"
#include "TimeManager.h"
#include "TimerTypes.h"
#include "MicrosecondTimer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

/* ===== Deferred Task Queue State ===== */
static DeferredTaskPtr gDeferredTaskQueues[4] = {NULL}; // One queue per priority
static pthread_mutex_t gDeferredTaskMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gDeferredTaskCond = PTHREAD_COND_INITIALIZER;
static pthread_t gDeferredTaskThread;
static bool gDeferredTaskSystemActive = false;
static bool gDeferredTaskShutdown = false;

/* Task statistics */
static DeferredTaskStats gDeferredTaskStats = {0};

/* Task ID generation */
static uint32_t gNextDeferredTaskID = 1;

/* ===== Private Function Prototypes ===== */
static void* DeferredTaskThreadProc(void* param);
static OSErr InsertTaskInQueue(DeferredTaskPtr taskPtr);
static OSErr RemoveTaskFromQueue(DeferredTaskPtr taskPtr);
static DeferredTaskPtr GetNextReadyTask(void);
static OSErr ExecuteDeferredTaskSafe(DeferredTaskPtr taskPtr);
static uint64_t GetCurrentTimeMicros(void);
static uint32_t GenerateDeferredTaskID(void);
static void UpdateDeferredTaskStats(DeferredTaskPtr taskPtr, bool executed);

/* ===== Deferred Task System Initialization ===== */

/**
 * Initialize Deferred Task System
 *
 * Sets up the deferred task queue and execution mechanism.
 */
OSErr InitDeferredTasks(void)
{
    pthread_attr_t attr;
    int result;

    if (gDeferredTaskSystemActive) {
        return NO_ERR;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    if (gDeferredTaskSystemActive) {
        pthread_mutex_unlock(&gDeferredTaskMutex);
        return NO_ERR;
    }

    // Initialize task queues
    for (int i = 0; i < 4; i++) {
        gDeferredTaskQueues[i] = NULL;
    }

    // Initialize statistics
    memset(&gDeferredTaskStats, 0, sizeof(gDeferredTaskStats));

    // Reset shutdown flag
    gDeferredTaskShutdown = false;

    // Initialize thread attributes
    result = pthread_attr_init(&attr);
    if (result != 0) {
        pthread_mutex_unlock(&gDeferredTaskMutex);
        return -1;
    }

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Create deferred task execution thread
    result = pthread_create(&gDeferredTaskThread, &attr, DeferredTaskThreadProc, NULL);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        pthread_mutex_unlock(&gDeferredTaskMutex);
        return -2;
    }

    gDeferredTaskSystemActive = true;

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return NO_ERR;
}

/**
 * Shutdown Deferred Task System
 *
 * Shuts down the deferred task system and executes or cancels all remaining tasks.
 */
void ShutdownDeferredTasks(void)
{
    if (!gDeferredTaskSystemActive) {
        return;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    if (!gDeferredTaskSystemActive) {
        pthread_mutex_unlock(&gDeferredTaskMutex);
        return;
    }

    // Signal shutdown
    gDeferredTaskShutdown = true;
    pthread_cond_broadcast(&gDeferredTaskCond);

    pthread_mutex_unlock(&gDeferredTaskMutex);

    // Wait for thread to complete
    pthread_join(gDeferredTaskThread, NULL);

    pthread_mutex_lock(&gDeferredTaskMutex);

    // Execute or cancel remaining tasks
    FlushDeferredTasks(DEFERRED_PRIORITY_LOW);

    // Clean up any remaining tasks
    for (int priority = 0; priority < 4; priority++) {
        DeferredTaskPtr current = gDeferredTaskQueues[priority];
        while (current) {
            DeferredTaskPtr next = current->next;
            current->isQueued = false;
            current = next;
        }
        gDeferredTaskQueues[priority] = NULL;
    }

    gDeferredTaskSystemActive = false;

    pthread_mutex_unlock(&gDeferredTaskMutex);
}

/* ===== Deferred Task Management Functions ===== */

/**
 * Queue Deferred Task
 *
 * Adds a task to the deferred execution queue.
 */
OSErr QueueDeferredTask(DeferredTaskPtr taskPtr,
                       DeferredProcPtr procedure,
                       void *userData,
                       DeferredPriority priority,
                       uint32_t delay)
{
    OSErr err;
    uint64_t currentTime;

    if (!taskPtr || !procedure) {
        return -1; // Invalid parameters
    }

    if (!gDeferredTaskSystemActive) {
        return -2; // System not initialized
    }

    if (priority > DEFERRED_PRIORITY_CRITICAL) {
        return -3; // Invalid priority
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    // Remove from queue if already queued
    if (taskPtr->isQueued) {
        RemoveTaskFromQueue(taskPtr);
    }

    // Initialize task
    taskPtr->taskID = GenerateDeferredTaskID();
    taskPtr->priority = priority;
    taskPtr->procedure = procedure;
    taskPtr->userData = userData;
    taskPtr->isQueued = false;
    taskPtr->isExecuting = false;
    taskPtr->lastError = NO_ERR;

    // Set timing information
    currentTime = GetCurrentTimeMicros();
    taskPtr->queueTime = currentTime;
    taskPtr->executeTime = currentTime + (delay * 1000ULL); // Convert to microseconds

    // Insert into appropriate priority queue
    err = InsertTaskInQueue(taskPtr);
    if (err == NO_ERR) {
        taskPtr->isQueued = true;
        gDeferredTaskStats.totalQueued++;
        gDeferredTaskStats.currentCount++;

        if (gDeferredTaskStats.currentCount > gDeferredTaskStats.peakCount) {
            gDeferredTaskStats.peakCount = gDeferredTaskStats.currentCount;
        }

        // Signal task thread
        pthread_cond_signal(&gDeferredTaskCond);
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return err;
}

/**
 * Cancel Deferred Task
 *
 * Removes a task from the deferred execution queue.
 */
OSErr CancelDeferredTask(DeferredTaskPtr taskPtr)
{
    OSErr err = NO_ERR;

    if (!taskPtr) {
        return -1;
    }

    if (!gDeferredTaskSystemActive) {
        return -2;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    // Wait for task to finish executing if currently running
    while (taskPtr->isExecuting) {
        pthread_mutex_unlock(&gDeferredTaskMutex);
        usleep(100); // Wait 100 microseconds
        pthread_mutex_lock(&gDeferredTaskMutex);
    }

    // Remove from queue if queued
    if (taskPtr->isQueued) {
        err = RemoveTaskFromQueue(taskPtr);
        if (err == NO_ERR) {
            taskPtr->isQueued = false;
            gDeferredTaskStats.currentCount--;
        }
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return err;
}

/**
 * Execute Deferred Tasks
 *
 * Processes the deferred task queue and executes ready tasks.
 */
uint32_t ExecuteDeferredTasks(uint32_t maxTasks, uint32_t maxTime)
{
    uint32_t tasksExecuted = 0;
    uint64_t startTime, currentTime;
    DeferredTaskPtr taskPtr;

    if (!gDeferredTaskSystemActive) {
        return 0;
    }

    startTime = GetCurrentTimeMicros();

    pthread_mutex_lock(&gDeferredTaskMutex);

    while (tasksExecuted < maxTasks || maxTasks == 0) {
        // Check time limit
        if (maxTime > 0) {
            currentTime = GetCurrentTimeMicros();
            if ((currentTime - startTime) >= maxTime) {
                break;
            }
        }

        // Get next ready task
        taskPtr = GetNextReadyTask();
        if (!taskPtr) {
            break; // No ready tasks
        }

        // Remove from queue
        RemoveTaskFromQueue(taskPtr);
        taskPtr->isQueued = false;
        taskPtr->isExecuting = true;
        gDeferredTaskStats.currentCount--;

        pthread_mutex_unlock(&gDeferredTaskMutex);

        // Execute task
        OSErr err = ExecuteDeferredTaskSafe(taskPtr);

        pthread_mutex_lock(&gDeferredTaskMutex);

        taskPtr->isExecuting = false;
        taskPtr->lastError = err;

        if (err == NO_ERR) {
            gDeferredTaskStats.totalExecuted++;
        } else {
            gDeferredTaskStats.totalFailed++;
        }

        UpdateDeferredTaskStats(taskPtr, err == NO_ERR);
        tasksExecuted++;
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return tasksExecuted;
}

/**
 * Flush Deferred Tasks
 *
 * Executes all pending deferred tasks immediately.
 */
uint32_t FlushDeferredTasks(DeferredPriority priority)
{
    uint32_t tasksExecuted = 0;
    DeferredTaskPtr taskPtr;

    if (!gDeferredTaskSystemActive) {
        return 0;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    // Process all priority levels from specified priority up to critical
    for (int p = priority; p <= DEFERRED_PRIORITY_CRITICAL; p++) {
        while (gDeferredTaskQueues[p] != NULL) {
            taskPtr = gDeferredTaskQueues[p];

            // Remove from queue
            RemoveTaskFromQueue(taskPtr);
            taskPtr->isQueued = false;
            taskPtr->isExecuting = true;
            gDeferredTaskStats.currentCount--;

            pthread_mutex_unlock(&gDeferredTaskMutex);

            // Execute task
            OSErr err = ExecuteDeferredTaskSafe(taskPtr);

            pthread_mutex_lock(&gDeferredTaskMutex);

            taskPtr->isExecuting = false;
            taskPtr->lastError = err;

            if (err == NO_ERR) {
                gDeferredTaskStats.totalExecuted++;
            } else {
                gDeferredTaskStats.totalFailed++;
            }

            UpdateDeferredTaskStats(taskPtr, err == NO_ERR);
            tasksExecuted++;
        }
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return tasksExecuted;
}

/* ===== Task Query Functions ===== */

/**
 * Is Task Queued
 */
bool IsDeferredTaskQueued(DeferredTaskPtr taskPtr)
{
    if (!taskPtr) {
        return false;
    }

    return taskPtr->isQueued;
}

/**
 * Is Task Executing
 */
bool IsDeferredTaskExecuting(DeferredTaskPtr taskPtr)
{
    if (!taskPtr) {
        return false;
    }

    return taskPtr->isExecuting;
}

/**
 * Get Task Queue Position
 */
int32_t GetDeferredTaskPosition(DeferredTaskPtr taskPtr)
{
    int32_t position = -1;
    int32_t count = 0;

    if (!taskPtr || !taskPtr->isQueued) {
        return -1;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    DeferredTaskPtr current = gDeferredTaskQueues[taskPtr->priority];
    while (current) {
        if (current == taskPtr) {
            position = count;
            break;
        }
        count++;
        current = current->next;
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return position;
}

/**
 * Get Deferred Task Count
 */
uint32_t GetDeferredTaskCount(DeferredPriority priority)
{
    uint32_t count = 0;

    if (!gDeferredTaskSystemActive) {
        return 0;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);

    if (priority == DEFERRED_TASK_ALL_PRIORITIES) {
        count = gDeferredTaskStats.currentCount;
    } else {
        DeferredTaskPtr current = gDeferredTaskQueues[priority];
        while (current) {
            count++;
            current = current->next;
        }
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);

    return count;
}

/**
 * Get Deferred Task Statistics
 */
OSErr GetDeferredTaskStats(DeferredTaskStats *stats)
{
    if (!stats) {
        return -1;
    }

    pthread_mutex_lock(&gDeferredTaskMutex);
    *stats = gDeferredTaskStats;
    pthread_mutex_unlock(&gDeferredTaskMutex);

    return NO_ERR;
}

/**
 * Reset Deferred Task Statistics
 */
void ResetDeferredTaskStats(void)
{
    pthread_mutex_lock(&gDeferredTaskMutex);
    memset(&gDeferredTaskStats, 0, sizeof(gDeferredTaskStats));
    pthread_mutex_unlock(&gDeferredTaskMutex);
}

/* ===== Utility Functions ===== */

/**
 * Create Deferred Task
 */
DeferredTaskPtr CreateDeferredTask(DeferredProcPtr procedure,
                                  void *userData,
                                  DeferredPriority priority)
{
    DeferredTaskPtr taskPtr = (DeferredTaskPtr)calloc(1, sizeof(DeferredTask));
    if (!taskPtr) {
        return NULL;
    }

    taskPtr->procedure = procedure;
    taskPtr->userData = userData;
    taskPtr->priority = priority;
    taskPtr->maxRetries = MAX_RETRIES;

    return taskPtr;
}

/**
 * Destroy Deferred Task
 */
OSErr DestroyDeferredTask(DeferredTaskPtr taskPtr)
{
    if (!taskPtr) {
        return -1;
    }

    // Cancel if queued
    if (taskPtr->isQueued) {
        CancelDeferredTask(taskPtr);
    }

    // Wait for execution to complete
    while (taskPtr->isExecuting) {
        usleep(100);
    }

    free(taskPtr);
    return NO_ERR;
}

/* ===== Private Implementation Functions ===== */

/**
 * Deferred Task Thread Procedure
 */
static void* DeferredTaskThreadProc(void* param)
{
    struct timespec timeout;

    (void)param;

    pthread_mutex_lock(&gDeferredTaskMutex);

    while (!gDeferredTaskShutdown) {
        // Calculate timeout for next check (10ms intervals)
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += 10000000; // 10ms
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000;
        }

        // Wait for signal or timeout
        int result = pthread_cond_timedwait(&gDeferredTaskCond, &gDeferredTaskMutex, &timeout);

        if (gDeferredTaskShutdown) {
            break;
        }

        // Execute ready tasks
        pthread_mutex_unlock(&gDeferredTaskMutex);
        ExecuteDeferredTasks(10, 5000); // Execute up to 10 tasks, max 5ms
        pthread_mutex_lock(&gDeferredTaskMutex);
    }

    pthread_mutex_unlock(&gDeferredTaskMutex);
    return NULL;
}

/**
 * Insert Task in Queue
 */
static OSErr InsertTaskInQueue(DeferredTaskPtr taskPtr)
{
    DeferredTaskPtr *head = &gDeferredTaskQueues[taskPtr->priority];
    DeferredTaskPtr current = *head;
    DeferredTaskPtr prev = NULL;

    // Insert in order of execution time
    while (current && current->executeTime <= taskPtr->executeTime) {
        prev = current;
        current = current->next;
    }

    // Insert task
    taskPtr->next = current;
    taskPtr->prev = prev;

    if (prev) {
        prev->next = taskPtr;
    } else {
        *head = taskPtr;
    }

    if (current) {
        current->prev = taskPtr;
    }

    return NO_ERR;
}

/**
 * Remove Task from Queue
 */
static OSErr RemoveTaskFromQueue(DeferredTaskPtr taskPtr)
{
    // Update links
    if (taskPtr->prev) {
        taskPtr->prev->next = taskPtr->next;
    } else {
        // This was the head of its priority queue
        gDeferredTaskQueues[taskPtr->priority] = taskPtr->next;
    }

    if (taskPtr->next) {
        taskPtr->next->prev = taskPtr->prev;
    }

    taskPtr->next = NULL;
    taskPtr->prev = NULL;

    return NO_ERR;
}

/**
 * Get Next Ready Task
 */
static DeferredTaskPtr GetNextReadyTask(void)
{
    uint64_t currentTime = GetCurrentTimeMicros();

    // Check priority queues from highest to lowest
    for (int priority = DEFERRED_PRIORITY_CRITICAL; priority >= DEFERRED_PRIORITY_LOW; priority--) {
        DeferredTaskPtr taskPtr = gDeferredTaskQueues[priority];
        if (taskPtr && taskPtr->executeTime <= currentTime) {
            return taskPtr;
        }
    }

    return NULL;
}

/**
 * Execute Deferred Task Safely
 */
static OSErr ExecuteDeferredTaskSafe(DeferredTaskPtr taskPtr)
{
    if (!taskPtr || !taskPtr->procedure) {
        return -1;
    }

    // Call the deferred procedure
    taskPtr->procedure(taskPtr, taskPtr->userData);

    return NO_ERR;
}

/**
 * Get Current Time in Microseconds
 */
static uint64_t GetCurrentTimeMicros(void)
{
    UnsignedWide timeValue;
    if (GetPlatformTime(&timeValue) == NO_ERR) {
        return timeValue.value / 1000; // Convert to microseconds
    }
    return 0;
}

/**
 * Generate Deferred Task ID
 */
static uint32_t GenerateDeferredTaskID(void)
{
    return __sync_fetch_and_add(&gNextDeferredTaskID, 1);
}

/**
 * Update Deferred Task Statistics
 */
static void UpdateDeferredTaskStats(DeferredTaskPtr taskPtr, bool executed)
{
    uint64_t currentTime = GetCurrentTimeMicros();
    uint64_t delay = currentTime - taskPtr->queueTime;

    // Update average delay
    if (gDeferredTaskStats.totalExecuted == 1) {
        gDeferredTaskStats.averageDelay = delay;
    } else {
        gDeferredTaskStats.averageDelay =
            (gDeferredTaskStats.averageDelay * 7 + delay) / 8; // 8-sample moving average
    }

    // Update maximum delay
    if (delay > gDeferredTaskStats.maxDelay) {
        gDeferredTaskStats.maxDelay = delay;
    }
}
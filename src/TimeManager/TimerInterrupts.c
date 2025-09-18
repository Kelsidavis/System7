/*
 * TimerInterrupts.c
 *
 * Timer Interrupt Simulation and Handling for System 7.1 Time Manager
 *
 * This file implements timer interrupt simulation and task servicing,
 * replacing the VIA Timer 2 interrupt handler from the original implementation
 * with a modern thread-based approach.
 */

#include "TimeManager.h"
#include "TimerTypes.h"
#include "MicrosecondTimer.h"
#include "TimeBase.h"
#include "DeferredTasks.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

/* ===== External References ===== */
extern TimeMgrPrivate *gTimeMgrPrivate;
extern pthread_mutex_t gTimeMgrMutex;
extern bool gTimeMgrInitialized;

/* ===== Timer Interrupt State ===== */
static pthread_t gTimerInterruptThread;
static bool gTimerInterruptActive = false;
static bool gTimerInterruptShutdown = false;
static pthread_mutex_t gInterruptMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gInterruptCond = PTHREAD_COND_INITIALIZER;

/* Timer interrupt statistics */
static struct {
    uint64_t totalInterrupts;
    uint64_t tasksExecuted;
    uint64_t averageLatency;
    uint64_t maxLatency;
    uint32_t currentDepth;
    uint32_t maxDepth;
} gInterruptStats = {0};

/* ===== Private Function Prototypes ===== */
static void* TimerInterruptThreadProc(void* param);
static void ProcessTimerInterrupt(void);
static void ServiceExpiredTasks(void);
static OSErr ExecuteTimerTask(TMTaskPtr tmTaskPtr);
static void UpdateInterruptStatistics(uint64_t startTime, uint32_t tasksProcessed);
static uint64_t GetCurrentTimeNS(void);

/* ===== Timer Interrupt Initialization ===== */

/**
 * Initialize Timer Interrupt System
 *
 * Sets up the timer interrupt simulation thread and related infrastructure.
 */
OSErr InitTimerInterrupts(void)
{
    pthread_attr_t attr;
    struct sched_param param;
    int result;

    if (gTimerInterruptActive) {
        return NO_ERR;
    }

    pthread_mutex_lock(&gInterruptMutex);

    if (gTimerInterruptActive) {
        pthread_mutex_unlock(&gInterruptMutex);
        return NO_ERR;
    }

    // Reset shutdown flag
    gTimerInterruptShutdown = false;

    // Initialize thread attributes for high-priority operation
    result = pthread_attr_init(&attr);
    if (result != 0) {
        pthread_mutex_unlock(&gInterruptMutex);
        return -1;
    }

    // Set thread to joinable
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Try to set real-time priority for accurate timing
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) == 0) {
        param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 2; // High but not maximum
        pthread_attr_setschedparam(&attr, &param);
    }

    // Create the timer interrupt thread
    result = pthread_create(&gTimerInterruptThread, &attr, TimerInterruptThreadProc, NULL);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        pthread_mutex_unlock(&gInterruptMutex);
        return -2;
    }

    gTimerInterruptActive = true;
    pthread_mutex_unlock(&gInterruptMutex);

    return NO_ERR;
}

/**
 * Shutdown Timer Interrupt System
 *
 * Stops the timer interrupt thread and cleans up resources.
 */
void ShutdownTimerInterrupts(void)
{
    if (!gTimerInterruptActive) {
        return;
    }

    pthread_mutex_lock(&gInterruptMutex);

    if (!gTimerInterruptActive) {
        pthread_mutex_unlock(&gInterruptMutex);
        return;
    }

    // Signal shutdown
    gTimerInterruptShutdown = true;
    pthread_cond_signal(&gInterruptCond);

    pthread_mutex_unlock(&gInterruptMutex);

    // Wait for thread to complete
    pthread_join(gTimerInterruptThread, NULL);

    pthread_mutex_lock(&gInterruptMutex);
    gTimerInterruptActive = false;
    pthread_mutex_unlock(&gInterruptMutex);
}

/* ===== Timer Interrupt Handler ===== */

/**
 * Timer2Int - Main Timer Interrupt Handler
 *
 * This function simulates the Timer2Int interrupt handler from the original
 * Time Manager, processing expired timer tasks and scheduling the next interrupt.
 */
void Timer2Int(void)
{
    if (!gTimeMgrInitialized || !gTimeMgrPrivate) {
        return;
    }

    // Signal the interrupt thread to process
    pthread_mutex_lock(&gInterruptMutex);
    pthread_cond_signal(&gInterruptCond);
    pthread_mutex_unlock(&gInterruptMutex);
}

/* ===== Private Implementation Functions ===== */

/**
 * Timer Interrupt Thread Procedure
 *
 * High-priority thread that simulates VIA timer interrupts and processes
 * expired timer tasks. This provides the main timing heartbeat for the system.
 */
static void* TimerInterruptThreadProc(void* param)
{
    struct timespec timeout;
    struct timespec interval;
    uint64_t lastInterruptTime;
    int result;

    (void)param; // Unused parameter

    // Set up for precise timing - 20 microsecond intervals (original VIA resolution)
    interval.tv_sec = 0;
    interval.tv_nsec = 20000; // 20 microseconds

    lastInterruptTime = GetCurrentTimeNS();

    pthread_mutex_lock(&gInterruptMutex);

    while (!gTimerInterruptShutdown) {
        // Calculate absolute timeout for next interrupt
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += interval.tv_nsec;
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000;
        }

        // Wait for interrupt signal or timeout
        result = pthread_cond_timedwait(&gInterruptCond, &gInterruptMutex, &timeout);

        if (gTimerInterruptShutdown) {
            break;
        }

        // Process timer interrupt
        pthread_mutex_unlock(&gInterruptMutex);
        ProcessTimerInterrupt();
        pthread_mutex_lock(&gInterruptMutex);

        // Update timing statistics
        uint64_t currentTime = GetCurrentTimeNS();
        uint64_t elapsed = currentTime - lastInterruptTime;
        lastInterruptTime = currentTime;

        // Adjust timing if we're running behind
        if (elapsed > 25000) { // More than 25 microseconds
            // We're behind schedule - reduce next interval slightly
            if (interval.tv_nsec > 15000) {
                interval.tv_nsec -= 1000;
            }
        } else if (elapsed < 15000) { // Less than 15 microseconds
            // We're ahead of schedule - increase next interval slightly
            if (interval.tv_nsec < 25000) {
                interval.tv_nsec += 1000;
            }
        }
    }

    pthread_mutex_unlock(&gInterruptMutex);
    return NULL;
}

/**
 * Process Timer Interrupt
 *
 * Main interrupt processing routine that handles expired timer tasks
 * and maintains the timer queue.
 */
static void ProcessTimerInterrupt(void)
{
    uint64_t startTime;
    uint32_t tasksProcessed = 0;

    if (!gTimeMgrInitialized || !gTimeMgrPrivate) {
        return;
    }

    startTime = GetCurrentTimeNS();
    gInterruptStats.totalInterrupts++;
    gInterruptStats.currentDepth++;

    if (gInterruptStats.currentDepth > gInterruptStats.maxDepth) {
        gInterruptStats.maxDepth = gInterruptStats.currentDepth;
    }

    // Lock the Time Manager for atomic operations
    if (pthread_mutex_trylock(&gTimeMgrMutex) != 0) {
        // If we can't get the lock immediately, defer processing
        gInterruptStats.currentDepth--;
        return;
    }

    // Update time base with current time
    UnsignedWide currentTime;
    if (GetPlatformTime(&currentTime) == NO_ERR) {
        UpdateTimeBase((uint32_t)(currentTime.value / 1000)); // Convert to microseconds
    }

    // Process expired timer tasks
    ServiceExpiredTasks();

    pthread_mutex_unlock(&gTimeMgrMutex);

    // Update statistics
    UpdateInterruptStatistics(startTime, tasksProcessed);
    gInterruptStats.currentDepth--;
}

/**
 * Service Expired Tasks
 *
 * Processes all expired timer tasks in the active queue.
 */
static void ServiceExpiredTasks(void)
{
    TMTaskPtr currentTask;
    TMTaskPtr nextTask;
    uint32_t tasksProcessed = 0;

    while (gTimeMgrPrivate->activePtr != NULL) {
        currentTask = gTimeMgrPrivate->activePtr;

        // Check if task has expired
        if (currentTask->tmCount > 0) {
            // Task hasn't expired yet
            break;
        }

        // Remove task from active queue
        gTimeMgrPrivate->activePtr = currentTask->qLink;
        currentTask->qLink = NULL;

        // Clear active flag
        CLEAR_TMTASK_ACTIVE(currentTask);
        currentTask->isActive = false;

        // Execute the task if it has a completion routine
        if (currentTask->tmAddr != NULL) {
            // Unlock mutex temporarily to execute user code
            pthread_mutex_unlock(&gTimeMgrMutex);

            OSErr err = ExecuteTimerTask(currentTask);
            if (err != NO_ERR) {
                // Log error or handle failure
            }

            // Re-lock mutex
            pthread_mutex_lock(&gTimeMgrMutex);
        }

        tasksProcessed++;
        gInterruptStats.tasksExecuted++;

        // Prevent infinite loops
        if (tasksProcessed > 1000) {
            break;
        }
    }
}

/**
 * Execute Timer Task
 *
 * Safely executes a timer task's completion routine.
 */
static OSErr ExecuteTimerTask(TMTaskPtr tmTaskPtr)
{
    if (!tmTaskPtr || !tmTaskPtr->tmAddr) {
        return -1;
    }

    // Set up execution context (simulating original register setup)
    // In the original: A0 = service routine address, A1 = TMTask pointer
    // In portable version: just call with TMTask pointer

    // Call the completion routine
    tmTaskPtr->tmAddr(tmTaskPtr);

    return NO_ERR;
}

/**
 * Update Interrupt Statistics
 *
 * Updates timing and performance statistics for the interrupt handler.
 */
static void UpdateInterruptStatistics(uint64_t startTime, uint32_t tasksProcessed)
{
    uint64_t endTime = GetCurrentTimeNS();
    uint64_t latency = endTime - startTime;

    // Update average latency (simple moving average)
    if (gInterruptStats.totalInterrupts == 1) {
        gInterruptStats.averageLatency = latency;
    } else {
        gInterruptStats.averageLatency =
            (gInterruptStats.averageLatency * 7 + latency) / 8; // 8-sample moving average
    }

    // Update maximum latency
    if (latency > gInterruptStats.maxLatency) {
        gInterruptStats.maxLatency = latency;
    }
}

/**
 * Get Current Time in Nanoseconds
 *
 * Returns current time with nanosecond precision.
 */
static uint64_t GetCurrentTimeNS(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }

    // Fallback
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }

    return 0;
}

/* ===== Public Interface Functions ===== */

/**
 * Get Timer Interrupt Statistics
 *
 * Returns performance statistics for the timer interrupt system.
 */
OSErr GetTimerInterruptStats(TimerInterruptStats *stats)
{
    if (!stats) {
        return -1;
    }

    pthread_mutex_lock(&gInterruptMutex);

    stats->totalInterrupts = gInterruptStats.totalInterrupts;
    stats->tasksExecuted = gInterruptStats.tasksExecuted;
    stats->averageLatency = gInterruptStats.averageLatency / 1000; // Convert to microseconds
    stats->maxLatency = gInterruptStats.maxLatency / 1000;         // Convert to microseconds
    stats->currentDepth = gInterruptStats.currentDepth;
    stats->maxDepth = gInterruptStats.maxDepth;

    pthread_mutex_unlock(&gInterruptMutex);

    return NO_ERR;
}

/**
 * Reset Timer Interrupt Statistics
 *
 * Resets all timer interrupt statistics to zero.
 */
void ResetTimerInterruptStats(void)
{
    pthread_mutex_lock(&gInterruptMutex);

    memset(&gInterruptStats, 0, sizeof(gInterruptStats));

    pthread_mutex_unlock(&gInterruptMutex);
}

/**
 * Set Timer Interrupt Priority
 *
 * Adjusts the priority of the timer interrupt thread.
 */
OSErr SetTimerInterruptPriority(int priority)
{
    struct sched_param param;
    int policy;
    int result;

    if (!gTimerInterruptActive) {
        return -1;
    }

    // Get current scheduling policy
    result = pthread_getschedparam(gTimerInterruptThread, &policy, &param);
    if (result != 0) {
        return -2;
    }

    // Set new priority
    param.sched_priority = priority;
    result = pthread_setschedparam(gTimerInterruptThread, policy, &param);
    if (result != 0) {
        return -3;
    }

    return NO_ERR;
}

/**
 * Force Timer Interrupt
 *
 * Forces an immediate timer interrupt for testing or synchronization.
 */
void ForceTimerInterrupt(void)
{
    if (gTimerInterruptActive) {
        pthread_mutex_lock(&gInterruptMutex);
        pthread_cond_signal(&gInterruptCond);
        pthread_mutex_unlock(&gInterruptMutex);
    }
}

/* ===== Timer Interrupt Statistics Structure ===== */
typedef struct TimerInterruptStats {
    uint64_t totalInterrupts;      /* Total interrupts processed */
    uint64_t tasksExecuted;        /* Total tasks executed */
    uint64_t averageLatency;       /* Average interrupt latency (microseconds) */
    uint64_t maxLatency;           /* Maximum interrupt latency (microseconds) */
    uint32_t currentDepth;         /* Current interrupt nesting depth */
    uint32_t maxDepth;             /* Maximum interrupt nesting depth */
} TimerInterruptStats;
/*
 * DeviceInterrupts.c
 * System 7.1 Device Manager - Device Interrupt Handling
 *
 * Implements device interrupt handling and simulation for the Device Manager.
 * This module provides interrupt-driven I/O capabilities and manages
 * completion routines for asynchronous operations.
 *
 * Based on the original System 7.1 DeviceMgr.a assembly source.
 */

#include "DeviceManager/DeviceManager.h"
#include "DeviceManager/DeviceTypes.h"
#include "DeviceManager/DriverInterface.h"
#include "DeviceManager/UnitTable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

/* =============================================================================
 * Constants and Types
 * ============================================================================= */

#define MAX_INTERRUPT_HANDLERS      64
#define INTERRUPT_STACK_SIZE        8192
#define MAX_COMPLETION_QUEUE        256
#define INTERRUPT_PRIORITY_LEVELS   8

typedef enum {
    kInterruptTypeDisk      = 1,
    kInterruptTypeNetwork   = 2,
    kInterruptTypeSerial    = 3,
    kInterruptTypeTimer     = 4,
    kInterruptTypeVIA       = 5,
    kInterruptTypeSCC       = 6,
    kInterruptTypeSCSI      = 7,
    kInterruptTypeCustom    = 8
} InterruptType;

typedef struct InterruptHandler {
    int16_t             refNum;          /* Driver reference number */
    InterruptType       type;            /* Interrupt type */
    uint32_t            priority;        /* Interrupt priority */
    bool                isEnabled;       /* Handler enabled flag */
    uint32_t            interruptCount;  /* Number of interrupts handled */
    void               *context;         /* Handler context */
    struct InterruptHandler *next;       /* Next handler in chain */
} InterruptHandler, *InterruptHandlerPtr;

typedef struct CompletionQueueEntry {
    IOParamPtr          pb;              /* Parameter block */
    int16_t             result;          /* Operation result */
    uint32_t            timestamp;       /* Completion timestamp */
    bool                isValid;         /* Entry is valid */
} CompletionQueueEntry;

/* =============================================================================
 * Global Variables
 * ============================================================================= */

static InterruptHandlerPtr g_interruptHandlers[INTERRUPT_PRIORITY_LEVELS] = {NULL};
static CompletionQueueEntry g_completionQueue[MAX_COMPLETION_QUEUE];
static uint32_t g_completionQueueHead = 0;
static uint32_t g_completionQueueTail = 0;
static uint32_t g_completionQueueCount = 0;

static bool g_interruptsEnabled = false;
static bool g_interruptSystemInitialized = false;
static uint32_t g_interruptNestingLevel = 0;
static uint32_t g_totalInterrupts = 0;

/* Statistics */
static struct {
    uint32_t totalInterrupts;
    uint32_t handledInterrupts;
    uint32_t spuriousInterrupts;
    uint32_t completionRoutinesCalled;
    uint32_t queueOverflows;
    uint32_t maxNestingLevel;
} g_interruptStats = {0};

/* =============================================================================
 * Internal Function Declarations
 * ============================================================================= */

static int16_t RegisterInterruptHandler(int16_t refNum, InterruptType type, uint32_t priority);
static int16_t UnregisterInterruptHandler(int16_t refNum);
static void ProcessInterrupt(InterruptType type, uint32_t data);
static void CallInterruptHandler(InterruptHandlerPtr handler, uint32_t data);
static int16_t QueueCompletion(IOParamPtr pb, int16_t result);
static void ProcessCompletionQueue(void);
static void ExecuteCompletionRoutine(IOParamPtr pb);
static InterruptHandlerPtr FindInterruptHandler(int16_t refNum);
static uint32_t GetCurrentTimestamp(void);
static void SignalHandler(int signum);

/* =============================================================================
 * Interrupt System Initialization
 * ============================================================================= */

int16_t DeviceInterrupts_Initialize(void)
{
    if (g_interruptSystemInitialized) {
        return noErr;
    }

    /* Initialize interrupt handler chains */
    for (int i = 0; i < INTERRUPT_PRIORITY_LEVELS; i++) {
        g_interruptHandlers[i] = NULL;
    }

    /* Initialize completion queue */
    memset(g_completionQueue, 0, sizeof(g_completionQueue));
    g_completionQueueHead = 0;
    g_completionQueueTail = 0;
    g_completionQueueCount = 0;

    /* Reset statistics */
    memset(&g_interruptStats, 0, sizeof(g_interruptStats));

    /* Reset global state */
    g_interruptsEnabled = false;
    g_interruptNestingLevel = 0;
    g_totalInterrupts = 0;

    /* Install signal handlers for interrupt simulation */
#ifdef SIGALRM
    signal(SIGALRM, SignalHandler);
#endif
#ifdef SIGUSR1
    signal(SIGUSR1, SignalHandler);
#endif
#ifdef SIGUSR2
    signal(SIGUSR2, SignalHandler);
#endif

    g_interruptSystemInitialized = true;
    return noErr;
}

void DeviceInterrupts_Shutdown(void)
{
    if (!g_interruptSystemInitialized) {
        return;
    }

    /* Disable interrupts */
    g_interruptsEnabled = false;

    /* Unregister all interrupt handlers */
    for (int i = 0; i < INTERRUPT_PRIORITY_LEVELS; i++) {
        InterruptHandlerPtr handler = g_interruptHandlers[i];
        while (handler != NULL) {
            InterruptHandlerPtr next = handler->next;
            free(handler);
            handler = next;
        }
        g_interruptHandlers[i] = NULL;
    }

    /* Process any remaining completions */
    ProcessCompletionQueue();

    /* Restore signal handlers */
#ifdef SIGALRM
    signal(SIGALRM, SIG_DFL);
#endif
#ifdef SIGUSR1
    signal(SIGUSR1, SIG_DFL);
#endif
#ifdef SIGUSR2
    signal(SIGUSR2, SIG_DFL);
#endif

    g_interruptSystemInitialized = false;
}

void DeviceInterrupts_Enable(void)
{
    if (g_interruptSystemInitialized) {
        g_interruptsEnabled = true;
    }
}

void DeviceInterrupts_Disable(void)
{
    g_interruptsEnabled = false;
}

bool DeviceInterrupts_AreEnabled(void)
{
    return g_interruptsEnabled;
}

/* =============================================================================
 * Interrupt Handler Registration
 * ============================================================================= */

static int16_t RegisterInterruptHandler(int16_t refNum, InterruptType type, uint32_t priority)
{
    if (!g_interruptSystemInitialized) {
        return dsIOCoreErr;
    }

    if (!UnitTable_IsValidRefNum(refNum)) {
        return badUnitErr;
    }

    if (priority >= INTERRUPT_PRIORITY_LEVELS) {
        priority = INTERRUPT_PRIORITY_LEVELS - 1;
    }

    /* Check if handler already exists */
    InterruptHandlerPtr existing = FindInterruptHandler(refNum);
    if (existing != NULL) {
        return dupFNErr; /* Already registered */
    }

    /* Allocate new handler */
    InterruptHandlerPtr handler = (InterruptHandlerPtr)malloc(sizeof(InterruptHandler));
    if (handler == NULL) {
        return memFullErr;
    }

    /* Initialize handler */
    handler->refNum = refNum;
    handler->type = type;
    handler->priority = priority;
    handler->isEnabled = true;
    handler->interruptCount = 0;
    handler->context = NULL;

    /* Add to priority chain */
    handler->next = g_interruptHandlers[priority];
    g_interruptHandlers[priority] = handler;

    return noErr;
}

static int16_t UnregisterInterruptHandler(int16_t refNum)
{
    if (!g_interruptSystemInitialized) {
        return dsIOCoreErr;
    }

    /* Search all priority levels */
    for (int priority = 0; priority < INTERRUPT_PRIORITY_LEVELS; priority++) {
        InterruptHandlerPtr *current = &g_interruptHandlers[priority];

        while (*current != NULL) {
            if ((*current)->refNum == refNum) {
                InterruptHandlerPtr handler = *current;
                *current = handler->next;
                free(handler);
                return noErr;
            }
            current = &(*current)->next;
        }
    }

    return fnfErr; /* Handler not found */
}

static InterruptHandlerPtr FindInterruptHandler(int16_t refNum)
{
    for (int priority = 0; priority < INTERRUPT_PRIORITY_LEVELS; priority++) {
        InterruptHandlerPtr handler = g_interruptHandlers[priority];
        while (handler != NULL) {
            if (handler->refNum == refNum) {
                return handler;
            }
            handler = handler->next;
        }
    }
    return NULL;
}

/* =============================================================================
 * Interrupt Processing
 * ============================================================================= */

int16_t SimulateDeviceInterrupt(int16_t refNum, uint32_t interruptType)
{
    if (!g_interruptSystemInitialized || !g_interruptsEnabled) {
        return noErr; /* Interrupts disabled */
    }

    /* Find the interrupt handler */
    InterruptHandlerPtr handler = FindInterruptHandler(refNum);
    if (handler == NULL) {
        g_interruptStats.spuriousInterrupts++;
        return fnfErr;
    }

    /* Process the interrupt */
    ProcessInterrupt((InterruptType)interruptType, 0);

    return noErr;
}

static void ProcessInterrupt(InterruptType type, uint32_t data)
{
    if (!g_interruptsEnabled) {
        return;
    }

    g_totalInterrupts++;
    g_interruptStats.totalInterrupts++;
    g_interruptNestingLevel++;

    if (g_interruptNestingLevel > g_interruptStats.maxNestingLevel) {
        g_interruptStats.maxNestingLevel = g_interruptNestingLevel;
    }

    /* Process handlers in priority order (highest first) */
    for (int priority = INTERRUPT_PRIORITY_LEVELS - 1; priority >= 0; priority--) {
        InterruptHandlerPtr handler = g_interruptHandlers[priority];

        while (handler != NULL) {
            if (handler->isEnabled && handler->type == type) {
                CallInterruptHandler(handler, data);
                g_interruptStats.handledInterrupts++;
            }
            handler = handler->next;
        }
    }

    g_interruptNestingLevel--;

    /* Process completion queue if we're at the top level */
    if (g_interruptNestingLevel == 0) {
        ProcessCompletionQueue();
    }
}

static void CallInterruptHandler(InterruptHandlerPtr handler, uint32_t data)
{
    if (handler == NULL) {
        return;
    }

    handler->interruptCount++;

    /* Get the DCE for this driver */
    DCEHandle dceHandle = UnitTable_GetDCE(handler->refNum);
    if (dceHandle == NULL) {
        return;
    }

    DCEPtr dce = *dceHandle;
    if (dce == NULL || !(dce->dCtlFlags & Is_Active_Mask)) {
        return;
    }

    /* Check if there are pending I/O operations */
    IOParamPtr pb = DequeueIORequest(dce);
    if (pb != NULL) {
        /* Complete the I/O operation */
        int16_t result = noErr; /* Simulate successful completion */

        /* Update actual count for read/write operations */
        if (pb->ioBuffer != NULL && pb->ioReqCount > 0) {
            pb->ioActCount = pb->ioReqCount; /* Simulate full transfer */
        }

        /* Queue completion */
        QueueCompletion(pb, result);
    }
}

/* =============================================================================
 * Completion Queue Management
 * ============================================================================= */

static int16_t QueueCompletion(IOParamPtr pb, int16_t result)
{
    if (pb == NULL) {
        return paramErr;
    }

    /* Check for queue overflow */
    if (g_completionQueueCount >= MAX_COMPLETION_QUEUE) {
        g_interruptStats.queueOverflows++;
        return queueOverflow;
    }

    /* Add to completion queue */
    CompletionQueueEntry *entry = &g_completionQueue[g_completionQueueTail];
    entry->pb = pb;
    entry->result = result;
    entry->timestamp = GetCurrentTimestamp();
    entry->isValid = true;

    g_completionQueueTail = (g_completionQueueTail + 1) % MAX_COMPLETION_QUEUE;
    g_completionQueueCount++;

    return noErr;
}

static void ProcessCompletionQueue(void)
{
    while (g_completionQueueCount > 0) {
        CompletionQueueEntry *entry = &g_completionQueue[g_completionQueueHead];

        if (entry->isValid && entry->pb != NULL) {
            /* Set result in parameter block */
            entry->pb->pb.ioResult = entry->result;

            /* Execute completion routine */
            ExecuteCompletionRoutine(entry->pb);

            g_interruptStats.completionRoutinesCalled++;
        }

        /* Mark entry as processed */
        entry->isValid = false;
        entry->pb = NULL;

        g_completionQueueHead = (g_completionQueueHead + 1) % MAX_COMPLETION_QUEUE;
        g_completionQueueCount--;
    }
}

static void ExecuteCompletionRoutine(IOParamPtr pb)
{
    if (pb == NULL) {
        return;
    }

    /* Call completion routine if provided */
    if (pb->pb.ioCompletion != NULL) {
        IOCompletionProc completion = (IOCompletionProc)pb->pb.ioCompletion;
        completion(pb);
    }
}

/* =============================================================================
 * Device-Specific Interrupt Functions
 * ============================================================================= */

int16_t RegisterDriverInterrupt(int16_t refNum, uint32_t interruptType)
{
    if (!UnitTable_IsValidRefNum(refNum)) {
        return badUnitErr;
    }

    /* Default priority based on interrupt type */
    uint32_t priority = 4; /* Medium priority */

    switch (interruptType) {
        case kInterruptTypeTimer:
            priority = 7; /* High priority */
            break;
        case kInterruptTypeSCSI:
        case kInterruptTypeDisk:
            priority = 6; /* High priority */
            break;
        case kInterruptTypeNetwork:
        case kInterruptTypeSerial:
            priority = 5; /* Medium-high priority */
            break;
        case kInterruptTypeVIA:
        case kInterruptTypeSCC:
            priority = 4; /* Medium priority */
            break;
        default:
            priority = 3; /* Low-medium priority */
            break;
    }

    return RegisterInterruptHandler(refNum, (InterruptType)interruptType, priority);
}

int16_t UnregisterDriverInterrupt(int16_t refNum)
{
    return UnregisterInterruptHandler(refNum);
}

int16_t EnableDriverInterrupt(int16_t refNum, bool enable)
{
    InterruptHandlerPtr handler = FindInterruptHandler(refNum);
    if (handler == NULL) {
        return fnfErr;
    }

    handler->isEnabled = enable;
    return noErr;
}

/* =============================================================================
 * Timer-Based Interrupt Simulation
 * ============================================================================= */

int16_t StartPeriodicInterrupt(int16_t refNum, uint32_t intervalTicks)
{
    /* Placeholder for timer-based periodic interrupts */
    /* In a real implementation, this would set up a timer */

    return RegisterDriverInterrupt(refNum, kInterruptTypeTimer);
}

int16_t StopPeriodicInterrupt(int16_t refNum)
{
    return UnregisterDriverInterrupt(refNum);
}

void TriggerPeriodicInterrupts(void)
{
    /* Simulate periodic timer interrupts */
    ProcessInterrupt(kInterruptTypeTimer, GetCurrentTimestamp());
}

/* =============================================================================
 * I/O Completion Interface
 * ============================================================================= */

void CompleteAsyncIO(IOParamPtr pb, int16_t result)
{
    if (pb == NULL) {
        return;
    }

    /* Queue the completion */
    QueueCompletion(pb, result);

    /* Process completions if not in interrupt context */
    if (g_interruptNestingLevel == 0) {
        ProcessCompletionQueue();
    }
}

bool IsIOCompletionPending(IOParamPtr pb)
{
    if (pb == NULL) {
        return false;
    }

    /* Check if PB is in the completion queue */
    for (uint32_t i = 0; i < g_completionQueueCount; i++) {
        uint32_t index = (g_completionQueueHead + i) % MAX_COMPLETION_QUEUE;
        if (g_completionQueue[index].isValid && g_completionQueue[index].pb == pb) {
            return true;
        }
    }

    return false;
}

void ProcessPendingCompletions(void)
{
    if (!g_interruptSystemInitialized) {
        return;
    }

    /* Process completions if not in interrupt context */
    if (g_interruptNestingLevel == 0) {
        ProcessCompletionQueue();
    }
}

/* =============================================================================
 * Signal Handler for Interrupt Simulation
 * ============================================================================= */

static void SignalHandler(int signum)
{
    if (!g_interruptsEnabled) {
        return;
    }

    InterruptType type = kInterruptTypeCustom;

    switch (signum) {
#ifdef SIGALRM
        case SIGALRM:
            type = kInterruptTypeTimer;
            break;
#endif
#ifdef SIGUSR1
        case SIGUSR1:
            type = kInterruptTypeDisk;
            break;
#endif
#ifdef SIGUSR2
        case SIGUSR2:
            type = kInterruptTypeNetwork;
            break;
#endif
        default:
            type = kInterruptTypeCustom;
            break;
    }

    ProcessInterrupt(type, 0);
}

/* =============================================================================
 * Statistics and Information
 * ============================================================================= */

void GetInterruptStatistics(void *stats)
{
    if (stats != NULL) {
        memcpy(stats, &g_interruptStats, sizeof(g_interruptStats));
    }
}

uint32_t GetInterruptCount(int16_t refNum)
{
    InterruptHandlerPtr handler = FindInterruptHandler(refNum);
    return handler ? handler->interruptCount : 0;
}

uint32_t GetCompletionQueueDepth(void)
{
    return g_completionQueueCount;
}

uint32_t GetInterruptNestingLevel(void)
{
    return g_interruptNestingLevel;
}

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

static uint32_t GetCurrentTimestamp(void)
{
    /* Simple timestamp based on system time */
    return (uint32_t)time(NULL);
}

bool IsInInterruptContext(void)
{
    return g_interruptNestingLevel > 0;
}

void YieldToInterrupts(void)
{
    /* Allow interrupt processing */
    if (g_interruptNestingLevel == 0) {
        ProcessCompletionQueue();
    }
}

/* =============================================================================
 * Debug and Testing Functions
 * ============================================================================= */

void DumpInterruptHandlers(void)
{
    printf("Interrupt Handlers:\n");
    for (int priority = 0; priority < INTERRUPT_PRIORITY_LEVELS; priority++) {
        InterruptHandlerPtr handler = g_interruptHandlers[priority];
        if (handler != NULL) {
            printf("  Priority %d:\n", priority);
            while (handler != NULL) {
                printf("    RefNum=%d, Type=%d, Enabled=%s, Count=%u\n",
                       handler->refNum, handler->type,
                       handler->isEnabled ? "Yes" : "No",
                       handler->interruptCount);
                handler = handler->next;
            }
        }
    }

    printf("Completion Queue: %u/%u entries\n",
           g_completionQueueCount, MAX_COMPLETION_QUEUE);
    printf("Interrupt Statistics:\n");
    printf("  Total: %u, Handled: %u, Spurious: %u\n",
           g_interruptStats.totalInterrupts,
           g_interruptStats.handledInterrupts,
           g_interruptStats.spuriousInterrupts);
    printf("  Completions: %u, Overflows: %u, Max Nesting: %u\n",
           g_interruptStats.completionRoutinesCalled,
           g_interruptStats.queueOverflows,
           g_interruptStats.maxNestingLevel);
}

int16_t InjectTestInterrupt(int16_t refNum, uint32_t type, uint32_t data)
{
    /* Testing function to inject interrupts */
    if (!g_interruptSystemInitialized) {
        return dsIOCoreErr;
    }

    ProcessInterrupt((InterruptType)type, data);
    return noErr;
}
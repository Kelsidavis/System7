/*
 * ProcessMgr_HAL.c - Hardware Abstraction Layer for Process Manager
 *
 * This HAL layer adapts the classic Mac OS Process Manager for modern
 * x86_64 and ARM64 architectures, providing platform-neutral interfaces
 * for context switching, thread management, and process scheduling.
 */

#include "ProcessMgr.h"
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef __x86_64__
    #include <x86intrin.h>
    #define PLATFORM_NAME "x86_64"
#elif defined(__aarch64__) || defined(__arm64__)
    #define PLATFORM_NAME "ARM64"
#else
    #define PLATFORM_NAME "Unknown"
#endif

/* Modern context structure for platform-independent context switching */
typedef struct ModernContext {
    jmp_buf             registers;         /* CPU registers saved state */
    pthread_t           thread;            /* POSIX thread handle */
    pthread_mutex_t     mutex;              /* Thread synchronization */
    pthread_cond_t      condition;          /* Thread signaling */
    void*               stack;              /* Thread stack */
    size_t              stackSize;         /* Stack size */
    volatile Boolean    shouldYield;       /* Cooperative yield flag */
    volatile Boolean    isRunning;         /* Thread running state */
} ModernContext;

/* Process context mapping */
typedef struct ProcessContextMap {
    ProcessSerialNumber psn;                /* Process identifier */
    ModernContext*      context;            /* Modern context */
    ProcessControlBlock* pcb;               /* Original PCB */
} ProcessContextMap;

/* Global HAL state */
static ProcessContextMap g_contextMap[kPM_MaxProcesses];
static pthread_mutex_t g_schedulerLock = PTHREAD_MUTEX_INITIALIZER;
static volatile Boolean g_schedulerRunning = false;
static pthread_t g_schedulerThread;

/* Timer for periodic scheduling */
static timer_t g_schedulerTimer;
static struct timespec g_scheduleInterval = {0, 10000000}; /* 10ms */

/*
 * Platform-specific context switching
 */
static OSErr HAL_SaveContext(ModernContext* context)
{
    if (!context) return paramErr;

    /* Save current thread context using setjmp */
    if (setjmp(context->registers) == 0) {
        /* Context saved successfully */
        return noErr;
    } else {
        /* Returning from longjmp */
        return noErr;
    }
}

static OSErr HAL_RestoreContext(ModernContext* context)
{
    if (!context) return paramErr;

    /* Restore thread context using longjmp */
    longjmp(context->registers, 1);

    /* Should never reach here */
    return noErr;
}

/*
 * Initialize HAL for Process Manager
 */
OSErr ProcessMgr_HAL_Initialize(void)
{
    OSErr err = noErr;

    /* Initialize context map */
    memset(g_contextMap, 0, sizeof(g_contextMap));

    /* Platform-specific initialization */
    #ifdef __x86_64__
        /* x86_64 specific setup */
        /* Enable SSE for floating point context */
    #elif defined(__aarch64__)
        /* ARM64 specific setup */
        /* Configure NEON/SVE if available */
    #endif

    /* Initialize scheduler lock */
    pthread_mutex_init(&g_schedulerLock, NULL);

    /* Set up timer for cooperative scheduling hints */
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;

    if (timer_create(CLOCK_MONOTONIC, &sev, &g_schedulerTimer) != 0) {
        return memFullErr;
    }

    return err;
}

/*
 * Create modern context for process
 */
OSErr ProcessMgr_HAL_CreateContext(ProcessControlBlock* pcb)
{
    if (!pcb) return paramErr;

    /* Find free context slot */
    int slot = -1;
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        if (g_contextMap[i].pcb == NULL) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return memFullErr;

    /* Allocate modern context */
    ModernContext* context = (ModernContext*)calloc(1, sizeof(ModernContext));
    if (!context) return memFullErr;

    /* Initialize context */
    pthread_mutex_init(&context->mutex, NULL);
    pthread_cond_init(&context->condition, NULL);

    /* Allocate stack for thread */
    context->stackSize = pcb->processStackSize;
    if (context->stackSize == 0) {
        context->stackSize = 1024 * 1024; /* Default 1MB stack */
    }

    context->stack = malloc(context->stackSize);
    if (!context->stack) {
        free(context);
        return memFullErr;
    }

    /* Map context to process */
    g_contextMap[slot].psn = pcb->processID;
    g_contextMap[slot].context = context;
    g_contextMap[slot].pcb = pcb;

    /* Store HAL context pointer in PCB reserved field */
    pcb->processReserved[0] = (UInt32)(uintptr_t)context;

    return noErr;
}

/*
 * Thread function for each process
 */
static void* ProcessThreadFunc(void* arg)
{
    ProcessContextMap* map = (ProcessContextMap*)arg;
    ModernContext* context = map->context;
    ProcessControlBlock* pcb = map->pcb;

    /* Set thread name for debugging */
    char threadName[16];
    snprintf(threadName, sizeof(threadName), "Proc_%08X", pcb->processID.lowLongOfPSN);
    #ifdef __APPLE__
        pthread_setname_np(threadName);
    #elif defined(__linux__)
        pthread_setname_np(pthread_self(), threadName);
    #endif

    /* Main process loop */
    while (context->isRunning) {
        pthread_mutex_lock(&context->mutex);

        /* Wait for scheduler to signal this process */
        while (!context->shouldYield && context->isRunning) {
            pthread_cond_wait(&context->condition, &context->mutex);
        }

        if (!context->isRunning) {
            pthread_mutex_unlock(&context->mutex);
            break;
        }

        /* Process is scheduled to run */
        context->shouldYield = false;
        pthread_mutex_unlock(&context->mutex);

        /* Call process main event loop if exists */
        if (pcb->processEventProc) {
            EventRecord event;
            if (GetNextEvent(everyEvent, &event)) {
                /* Dispatch event to process */
                CallEventProc(pcb->processEventProc, &event);
            }
        }

        /* Simulate WaitNextEvent yield */
        struct timespec yield_time = {0, 1000000}; /* 1ms */
        nanosleep(&yield_time, NULL);
    }

    return NULL;
}

/*
 * Launch process with modern threading
 */
OSErr ProcessMgr_HAL_LaunchProcess(ProcessControlBlock* pcb)
{
    if (!pcb) return paramErr;

    /* Get HAL context */
    ModernContext* context = (ModernContext*)(uintptr_t)pcb->processReserved[0];
    if (!context) return nilHandleErr;

    /* Create POSIX thread for process */
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    /* Set stack size */
    pthread_attr_setstacksize(&attr, context->stackSize);

    /* Set thread as detached */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    /* Find context map entry */
    ProcessContextMap* map = NULL;
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        if (g_contextMap[i].pcb == pcb) {
            map = &g_contextMap[i];
            break;
        }
    }

    if (!map) return paramErr;

    /* Mark as running */
    context->isRunning = true;

    /* Create thread */
    if (pthread_create(&context->thread, &attr, ProcessThreadFunc, map) != 0) {
        pthread_attr_destroy(&attr);
        return memFullErr;
    }

    pthread_attr_destroy(&attr);

    return noErr;
}

/*
 * Context switch between processes
 */
OSErr ProcessMgr_HAL_SwitchContext(ProcessControlBlock* fromProcess,
                                   ProcessControlBlock* toProcess)
{
    if (!toProcess) return paramErr;

    /* Get contexts */
    ModernContext* fromContext = NULL;
    ModernContext* toContext = NULL;

    if (fromProcess) {
        fromContext = (ModernContext*)(uintptr_t)fromProcess->processReserved[0];
    }

    toContext = (ModernContext*)(uintptr_t)toProcess->processReserved[0];
    if (!toContext) return nilHandleErr;

    pthread_mutex_lock(&g_schedulerLock);

    /* Suspend current process */
    if (fromContext && fromContext->isRunning) {
        pthread_mutex_lock(&fromContext->mutex);
        fromContext->shouldYield = false;
        pthread_mutex_unlock(&fromContext->mutex);
    }

    /* Resume target process */
    pthread_mutex_lock(&toContext->mutex);
    toContext->shouldYield = true;
    pthread_cond_signal(&toContext->condition);
    pthread_mutex_unlock(&toContext->mutex);

    /* Update current process */
    gCurrentProcess = toProcess;

    pthread_mutex_unlock(&g_schedulerLock);

    return noErr;
}

/*
 * Terminate process and clean up resources
 */
OSErr ProcessMgr_HAL_TerminateProcess(ProcessControlBlock* pcb)
{
    if (!pcb) return paramErr;

    /* Get HAL context */
    ModernContext* context = (ModernContext*)(uintptr_t)pcb->processReserved[0];
    if (!context) return noErr; /* Already terminated */

    /* Signal thread to exit */
    pthread_mutex_lock(&context->mutex);
    context->isRunning = false;
    context->shouldYield = false;
    pthread_cond_signal(&context->condition);
    pthread_mutex_unlock(&context->mutex);

    /* Wait for thread to finish */
    pthread_join(context->thread, NULL);

    /* Clean up resources */
    pthread_mutex_destroy(&context->mutex);
    pthread_cond_destroy(&context->condition);

    if (context->stack) {
        free(context->stack);
    }

    /* Clear context map */
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        if (g_contextMap[i].context == context) {
            g_contextMap[i].psn.highLongOfPSN = 0;
            g_contextMap[i].psn.lowLongOfPSN = 0;
            g_contextMap[i].context = NULL;
            g_contextMap[i].pcb = NULL;
            break;
        }
    }

    free(context);
    pcb->processReserved[0] = 0;

    return noErr;
}

/*
 * Get current process using thread-local storage
 */
ProcessControlBlock* ProcessMgr_HAL_GetCurrentProcess(void)
{
    pthread_t currentThread = pthread_self();

    /* Find process by thread ID */
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        if (g_contextMap[i].context &&
            pthread_equal(g_contextMap[i].context->thread, currentThread)) {
            return g_contextMap[i].pcb;
        }
    }

    /* Default to system process */
    return gCurrentProcess;
}

/*
 * Yield CPU time cooperatively (WaitNextEvent simulation)
 */
OSErr ProcessMgr_HAL_Yield(void)
{
    /* Modern cooperative yield using nanosleep */
    struct timespec yield_time = {0, 100000}; /* 0.1ms */
    nanosleep(&yield_time, NULL);

    /* Allow other threads to run */
    sched_yield();

    return noErr;
}

/*
 * Platform-specific CPU feature detection
 */
void ProcessMgr_HAL_GetCPUFeatures(CPUFeatures* features)
{
    if (!features) return;

    memset(features, 0, sizeof(CPUFeatures));

    #ifdef __x86_64__
        /* Check x86_64 features */
        features->hasSSE = __builtin_cpu_supports("sse");
        features->hasSSE2 = __builtin_cpu_supports("sse2");
        features->hasAVX = __builtin_cpu_supports("avx");
        features->hasAVX2 = __builtin_cpu_supports("avx2");

        /* Get CPU count */
        features->cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
        features->architecture = kCPUArch_x86_64;

    #elif defined(__aarch64__)
        /* ARM64 features */
        features->hasNEON = 1; /* Always available on ARM64 */
        features->cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
        features->architecture = kCPUArch_ARM64;

        /* Check for Apple Silicon */
        #ifdef __APPLE__
            features->isAppleSilicon = 1;
        #endif
    #endif

    strncpy(features->platformName, PLATFORM_NAME, sizeof(features->platformName) - 1);
}

/*
 * Shutdown HAL and clean up all processes
 */
OSErr ProcessMgr_HAL_Shutdown(void)
{
    /* Stop scheduler timer */
    if (g_schedulerTimer) {
        timer_delete(g_schedulerTimer);
        g_schedulerTimer = 0;
    }

    /* Terminate all processes */
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        if (g_contextMap[i].pcb) {
            ProcessMgr_HAL_TerminateProcess(g_contextMap[i].pcb);
        }
    }

    /* Clean up scheduler lock */
    pthread_mutex_destroy(&g_schedulerLock);

    return noErr;
}
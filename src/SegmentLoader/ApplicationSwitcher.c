/*
 * ApplicationSwitcher.c - MultiFinder Application Switching
 *
 * This file implements MultiFinder-style application switching, background
 * task management, and cooperative multitasking for Mac OS 7.1 applications.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Application Switching Constants
 * ============================================================================ */

#define MAX_RUNNING_APPS        32      /* Maximum concurrent applications */
#define CONTEXT_SWITCH_TIME     16      /* Time slice in ticks (1/60 sec) */
#define BACKGROUND_TIME_SLICE   4       /* Background app time slice */
#define IDLE_THRESHOLD          100     /* Idle detection threshold */

/* Application states */
#define APP_STATE_INACTIVE      0x00    /* Application not running */
#define APP_STATE_FOREGROUND    0x01    /* Foreground application */
#define APP_STATE_BACKGROUND    0x02    /* Background application */
#define APP_STATE_SUSPENDED     0x04    /* Suspended application */
#define APP_STATE_LAUNCHING     0x08    /* Application launching */
#define APP_STATE_TERMINATING   0x10    /* Application terminating */

/* Switch reasons */
#define SWITCH_USER_REQUEST     0       /* User requested switch */
#define SWITCH_TIME_SLICE       1       /* Time slice expired */
#define SWITCH_APP_LAUNCH       2       /* New application launched */
#define SWITCH_APP_TERMINATE    3       /* Application terminated */
#define SWITCH_SYSTEM_EVENT     4       /* System event */

/* ============================================================================
 * Application Switching Data Structures
 * ============================================================================ */

typedef struct ProcessInfo {
    ApplicationControlBlock* acb;       /* Application control block */
    uint32_t                processID;  /* Unique process ID */
    uint32_t                state;      /* Application state */
    uint32_t                priority;   /* Scheduling priority */
    uint32_t                timeSlice;  /* Remaining time slice */
    uint32_t                totalTime;  /* Total CPU time used */
    uint32_t                lastSwitch; /* Last context switch time */
    uint32_t                signature;  /* Application signature */
    Str255                  name;       /* Application name */
    bool                    canBackground; /* Can run in background */
    bool                    needsEvents;   /* Needs event processing */
} ProcessInfo;

typedef struct ApplicationSwitcher {
    ProcessInfo             processes[MAX_RUNNING_APPS];
    uint16_t                processCount;       /* Number of active processes */
    uint16_t                foregroundIndex;   /* Index of foreground app */
    uint16_t                nextProcessID;     /* Next process ID to assign */
    uint32_t                switchTime;        /* Last switch time */
    uint32_t                totalSwitches;     /* Total context switches */
    uint32_t                systemTime;        /* System time counter */
    bool                    multitaskingEnabled; /* Multitasking enabled */
    bool                    cooperative;       /* Cooperative multitasking */
} ApplicationSwitcher;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static ApplicationSwitcher gAppSwitcher;
static bool gSwitcherInitialized = false;
static uint32_t gSystemTicks = 0;  /* Simple tick counter */

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr AddProcessToSwitcher(ApplicationControlBlock* acb, uint16_t* processIndex);
static OSErr RemoveProcessFromSwitcher(ApplicationControlBlock* acb);
static ProcessInfo* FindProcessByACB(ApplicationControlBlock* acb);
static ProcessInfo* FindProcessByID(uint32_t processID);
static OSErr PerformContextSwitch(uint16_t fromIndex, uint16_t toIndex, uint8_t reason);
static OSErr SaveApplicationContext(ProcessInfo* process);
static OSErr RestoreApplicationContext(ProcessInfo* process);
static uint16_t SelectNextProcess(void);
static OSErr UpdateProcessState(ProcessInfo* process, uint32_t newState);
static void DistributeTimeSlices(void);

/* ============================================================================
 * Application Switcher Initialization
 * ============================================================================ */

OSErr InitApplicationSwitcher(void)
{
    if (gSwitcherInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("InitApplicationSwitcher: Initializing application switcher\n");

    /* Clear switcher structure */
    memset(&gAppSwitcher, 0, sizeof(ApplicationSwitcher));

    /* Initialize all process entries */
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        gAppSwitcher.processes[i].processID = 0;  /* Invalid ID */
        gAppSwitcher.processes[i].state = APP_STATE_INACTIVE;
    }

    /* Set initial values */
    gAppSwitcher.processCount = 0;
    gAppSwitcher.foregroundIndex = 0xFFFF;  /* No foreground app */
    gAppSwitcher.nextProcessID = 1;
    gAppSwitcher.multitaskingEnabled = true;
    gAppSwitcher.cooperative = true;  /* Mac OS 7.1 style */

    /* Initialize system time */
    gSystemTicks = 1;

    gSwitcherInitialized = true;

    DEBUG_LOG("InitApplicationSwitcher: Application switcher initialized\n");
    return segNoErr;
}

void TerminateApplicationSwitcher(void)
{
    if (!gSwitcherInitialized) {
        return;
    }

    DEBUG_LOG("TerminateApplicationSwitcher: Terminating application switcher\n");

    /* Terminate all running applications */
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        ProcessInfo* process = &gAppSwitcher.processes[i];
        if (process->state != APP_STATE_INACTIVE && process->acb) {
            TerminateApplication(process->acb, 0);
        }
    }

    /* Log switching statistics */
    DEBUG_LOG("Switching Statistics:\n");
    DEBUG_LOG("  Total Switches: %d\n", gAppSwitcher.totalSwitches);
    DEBUG_LOG("  Active Processes: %d\n", gAppSwitcher.processCount);

    gSwitcherInitialized = false;
}

/* ============================================================================
 * Process Management
 * ============================================================================ */

OSErr RegisterApplication(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segBadFormat;
    }

    if (!gSwitcherInitialized) {
        OSErr err = InitApplicationSwitcher();
        if (err != segNoErr) return err;
    }

    DEBUG_LOG("RegisterApplication: Registering application\n");

    /* Add process to switcher */
    uint16_t processIndex;
    OSErr err = AddProcessToSwitcher(acb, &processIndex);
    if (err != segNoErr) {
        DEBUG_LOG("RegisterApplication: Failed to add process: %d\n", err);
        return err;
    }

    ProcessInfo* process = &gAppSwitcher.processes[processIndex];

    /* Set initial state */
    if (gAppSwitcher.foregroundIndex == 0xFFFF) {
        /* First application becomes foreground */
        gAppSwitcher.foregroundIndex = processIndex;
        process->state = APP_STATE_FOREGROUND;
        process->timeSlice = CONTEXT_SWITCH_TIME;
    } else {
        /* Subsequent applications start in background */
        process->state = APP_STATE_BACKGROUND;
        process->timeSlice = BACKGROUND_TIME_SLICE;
    }

    /* Initialize process info */
    process->priority = 128;  /* Normal priority */
    process->canBackground = true;  /* Assume backgroundable */
    process->needsEvents = true;

    /* Get application name */
    if (acb->appSpec.name[0] > 0) {
        size_t nameLen = acb->appSpec.name[0];
        if (nameLen > 255) nameLen = 255;
        process->name[0] = (unsigned char)nameLen;
        memcpy(&process->name[1], &acb->appSpec.name[1], nameLen);
    } else {
        strcpy((char*)process->name, "\pUnknown App");
    }

    DEBUG_LOG("RegisterApplication: Application registered as process %d\n",
             process->processID);

    return segNoErr;
}

OSErr UnregisterApplication(ApplicationControlBlock* acb)
{
    if (!acb || !gSwitcherInitialized) {
        return segNotApplication;
    }

    DEBUG_LOG("UnregisterApplication: Unregistering application\n");

    ProcessInfo* process = FindProcessByACB(acb);
    if (!process) {
        DEBUG_LOG("UnregisterApplication: Process not found\n");
        return segNotApplication;
    }

    uint16_t processIndex = process - gAppSwitcher.processes;

    /* If this is the foreground app, switch to another */
    if (gAppSwitcher.foregroundIndex == processIndex) {
        uint16_t nextIndex = SelectNextProcess();
        if (nextIndex != 0xFFFF && nextIndex != processIndex) {
            PerformContextSwitch(processIndex, nextIndex, SWITCH_APP_TERMINATE);
        } else {
            gAppSwitcher.foregroundIndex = 0xFFFF;  /* No foreground app */
        }
    }

    /* Remove process from switcher */
    OSErr err = RemoveProcessFromSwitcher(acb);
    if (err != segNoErr) {
        DEBUG_LOG("UnregisterApplication: Failed to remove process: %d\n", err);
        return err;
    }

    DEBUG_LOG("UnregisterApplication: Application unregistered\n");
    return segNoErr;
}

static OSErr AddProcessToSwitcher(ApplicationControlBlock* acb, uint16_t* processIndex)
{
    /* Find free process slot */
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        if (gAppSwitcher.processes[i].state == APP_STATE_INACTIVE) {
            ProcessInfo* process = &gAppSwitcher.processes[i];

            /* Initialize process info */
            memset(process, 0, sizeof(ProcessInfo));
            process->acb = acb;
            process->processID = gAppSwitcher.nextProcessID++;
            process->state = APP_STATE_LAUNCHING;
            process->signature = acb->signature;
            process->lastSwitch = gSystemTicks;

            gAppSwitcher.processCount++;

            if (processIndex) {
                *processIndex = i;
            }

            DEBUG_LOG("AddProcessToSwitcher: Added process %d at index %d\n",
                     process->processID, i);

            return segNoErr;
        }
    }

    DEBUG_LOG("AddProcessToSwitcher: No free process slots\n");
    return segMemFullErr;
}

static OSErr RemoveProcessFromSwitcher(ApplicationControlBlock* acb)
{
    ProcessInfo* process = FindProcessByACB(acb);
    if (!process) {
        return segNotApplication;
    }

    DEBUG_LOG("RemoveProcessFromSwitcher: Removing process %d\n", process->processID);

    /* Clear process info */
    memset(process, 0, sizeof(ProcessInfo));
    process->state = APP_STATE_INACTIVE;

    gAppSwitcher.processCount--;

    return segNoErr;
}

/* ============================================================================
 * Context Switching
 * ============================================================================ */

OSErr SwitchToApplication(ApplicationControlBlock* acb)
{
    if (!acb || !gSwitcherInitialized) {
        return segNotApplication;
    }

    ProcessInfo* targetProcess = FindProcessByACB(acb);
    if (!targetProcess) {
        DEBUG_LOG("SwitchToApplication: Target process not found\n");
        return segNotApplication;
    }

    uint16_t targetIndex = targetProcess - gAppSwitcher.processes;
    uint16_t currentIndex = gAppSwitcher.foregroundIndex;

    DEBUG_LOG("SwitchToApplication: Switching to process %d\n", targetProcess->processID);

    /* Perform the context switch */
    OSErr err = PerformContextSwitch(currentIndex, targetIndex, SWITCH_USER_REQUEST);
    if (err != segNoErr) {
        DEBUG_LOG("SwitchToApplication: Context switch failed: %d\n", err);
        return err;
    }

    DEBUG_LOG("SwitchToApplication: Successfully switched to application\n");
    return segNoErr;
}

static OSErr PerformContextSwitch(uint16_t fromIndex, uint16_t toIndex, uint8_t reason)
{
    if (fromIndex == toIndex && fromIndex != 0xFFFF) {
        return segNoErr;  /* No switch needed */
    }

    DEBUG_LOG("PerformContextSwitch: Switching from %d to %d (reason: %d)\n",
             fromIndex, toIndex, reason);

    /* Save current application context */
    if (fromIndex != 0xFFFF) {
        ProcessInfo* fromProcess = &gAppSwitcher.processes[fromIndex];
        OSErr err = SaveApplicationContext(fromProcess);
        if (err != segNoErr) {
            DEBUG_LOG("PerformContextSwitch: Failed to save context: %d\n", err);
            return err;
        }

        /* Update process state */
        if (fromProcess->state == APP_STATE_FOREGROUND) {
            UpdateProcessState(fromProcess, APP_STATE_BACKGROUND);
        }
    }

    /* Restore new application context */
    if (toIndex != 0xFFFF) {
        ProcessInfo* toProcess = &gAppSwitcher.processes[toIndex];
        OSErr err = RestoreApplicationContext(toProcess);
        if (err != segNoErr) {
            DEBUG_LOG("PerformContextSwitch: Failed to restore context: %d\n", err);
            return err;
        }

        /* Update process state */
        UpdateProcessState(toProcess, APP_STATE_FOREGROUND);
        toProcess->timeSlice = CONTEXT_SWITCH_TIME;
        toProcess->lastSwitch = gSystemTicks;

        /* Update switcher state */
        gAppSwitcher.foregroundIndex = toIndex;

        /* Set as current application */
        SetCurrentApplication(toProcess->acb);
    } else {
        /* No foreground application */
        gAppSwitcher.foregroundIndex = 0xFFFF;
        SetCurrentApplication(NULL);
    }

    /* Update statistics */
    gAppSwitcher.totalSwitches++;
    gAppSwitcher.switchTime = gSystemTicks;

    DEBUG_LOG("PerformContextSwitch: Context switch completed\n");
    return segNoErr;
}

static OSErr SaveApplicationContext(ProcessInfo* process)
{
    if (!process || !process->acb) {
        return segBadFormat;
    }

    DEBUG_LOG("SaveApplicationContext: Saving context for process %d\n", process->processID);

    /* Save A5 world */
    uint32_t savedA5;
    OSErr err = SaveA5World(&savedA5);
    if (err != segNoErr) {
        return err;
    }

    /* TODO: Save additional context */
    /* - CPU registers */
    /* - Stack pointer */
    /* - Memory manager state */
    /* - Resource manager state */

    /* Update CPU time */
    process->totalTime += gSystemTicks - process->lastSwitch;

    return segNoErr;
}

static OSErr RestoreApplicationContext(ProcessInfo* process)
{
    if (!process || !process->acb) {
        return segBadFormat;
    }

    DEBUG_LOG("RestoreApplicationContext: Restoring context for process %d\n",
             process->processID);

    /* Restore A5 world */
    OSErr err = RestoreA5World((uint32_t)process->acb->a5World);
    if (err != segNoErr) {
        return err;
    }

    /* Switch to application heap */
    err = SwitchToApplicationHeap(process->acb);
    if (err != segNoErr) {
        return err;
    }

    /* TODO: Restore additional context */
    /* - CPU registers */
    /* - Stack pointer */
    /* - Memory manager state */
    /* - Resource manager state */

    return segNoErr;
}

/* ============================================================================
 * Process Scheduling
 * ============================================================================ */

OSErr RunApplicationScheduler(void)
{
    if (!gSwitcherInitialized || !gAppSwitcher.multitaskingEnabled) {
        return segNoErr;
    }

    gSystemTicks++;

    /* Check if current process time slice expired */
    if (gAppSwitcher.foregroundIndex != 0xFFFF) {
        ProcessInfo* currentProcess = &gAppSwitcher.processes[gAppSwitcher.foregroundIndex];

        if (currentProcess->timeSlice > 0) {
            currentProcess->timeSlice--;
        }

        /* Switch if time slice expired */
        if (currentProcess->timeSlice == 0 && gAppSwitcher.cooperative) {
            uint16_t nextIndex = SelectNextProcess();
            if (nextIndex != gAppSwitcher.foregroundIndex) {
                PerformContextSwitch(gAppSwitcher.foregroundIndex, nextIndex,
                                   SWITCH_TIME_SLICE);
            } else {
                /* Reset time slice for current process */
                currentProcess->timeSlice = CONTEXT_SWITCH_TIME;
            }
        }
    }

    /* Distribute time slices to background processes */
    if (gSystemTicks % 4 == 0) {  /* Every 4 ticks */
        DistributeTimeSlices();
    }

    return segNoErr;
}

static uint16_t SelectNextProcess(void)
{
    /* Simple round-robin scheduling */
    uint16_t startIndex = (gAppSwitcher.foregroundIndex == 0xFFFF) ? 0 :
                         gAppSwitcher.foregroundIndex + 1;

    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        uint16_t index = (startIndex + i) % MAX_RUNNING_APPS;
        ProcessInfo* process = &gAppSwitcher.processes[index];

        if (process->state == APP_STATE_BACKGROUND ||
            process->state == APP_STATE_FOREGROUND) {
            return index;
        }
    }

    /* No suitable process found */
    return 0xFFFF;
}

static void DistributeTimeSlices(void)
{
    /* Give small time slices to background processes */
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        ProcessInfo* process = &gAppSwitcher.processes[i];

        if (process->state == APP_STATE_BACKGROUND && process->canBackground) {
            /* Give background processes a small time slice */
            if (process->needsEvents || process->timeSlice == 0) {
                process->timeSlice = BACKGROUND_TIME_SLICE;

                /* TODO: Run background process for a short time */
                /* This would involve mini context switches */
            }
        }
    }
}

/* ============================================================================
 * Process State Management
 * ============================================================================ */

static OSErr UpdateProcessState(ProcessInfo* process, uint32_t newState)
{
    if (!process) {
        return segBadFormat;
    }

    uint32_t oldState = process->state;
    process->state = newState;

    DEBUG_LOG("UpdateProcessState: Process %d state changed from %d to %d\n",
             process->processID, oldState, newState);

    /* Handle state transitions */
    switch (newState) {
        case APP_STATE_FOREGROUND:
            if (process->acb) {
                process->acb->inBackground = false;
            }
            break;

        case APP_STATE_BACKGROUND:
            if (process->acb) {
                process->acb->inBackground = true;
            }
            break;

        case APP_STATE_SUSPENDED:
            /* TODO: Suspend application processing */
            break;

        case APP_STATE_TERMINATING:
            /* TODO: Begin termination sequence */
            break;
    }

    return segNoErr;
}

OSErr SuspendApplication(ApplicationControlBlock* acb)
{
    if (!acb || !gSwitcherInitialized) {
        return segNotApplication;
    }

    ProcessInfo* process = FindProcessByACB(acb);
    if (!process) {
        return segNotApplication;
    }

    DEBUG_LOG("SuspendApplication: Suspending process %d\n", process->processID);

    /* Save current state */
    uint32_t savedState = process->state;

    /* Suspend the process */
    OSErr err = UpdateProcessState(process, APP_STATE_SUSPENDED);
    if (err != segNoErr) {
        return err;
    }

    /* If this was the foreground app, switch to another */
    uint16_t processIndex = process - gAppSwitcher.processes;
    if (gAppSwitcher.foregroundIndex == processIndex) {
        uint16_t nextIndex = SelectNextProcess();
        if (nextIndex != 0xFFFF) {
            PerformContextSwitch(processIndex, nextIndex, SWITCH_SYSTEM_EVENT);
        }
    }

    DEBUG_LOG("SuspendApplication: Application suspended\n");
    return segNoErr;
}

OSErr ResumeApplication(ApplicationControlBlock* acb)
{
    if (!acb || !gSwitcherInitialized) {
        return segNotApplication;
    }

    ProcessInfo* process = FindProcessByACB(acb);
    if (!process) {
        return segNotApplication;
    }

    DEBUG_LOG("ResumeApplication: Resuming process %d\n", process->processID);

    /* Resume the process */
    uint32_t newState = (gAppSwitcher.foregroundIndex == 0xFFFF) ?
                       APP_STATE_FOREGROUND : APP_STATE_BACKGROUND;

    OSErr err = UpdateProcessState(process, newState);
    if (err != segNoErr) {
        return err;
    }

    /* If no foreground app, make this one foreground */
    if (gAppSwitcher.foregroundIndex == 0xFFFF) {
        uint16_t processIndex = process - gAppSwitcher.processes;
        gAppSwitcher.foregroundIndex = processIndex;
        SetCurrentApplication(acb);
    }

    DEBUG_LOG("ResumeApplication: Application resumed\n");
    return segNoErr;
}

/* ============================================================================
 * Process Information
 * ============================================================================ */

static ProcessInfo* FindProcessByACB(ApplicationControlBlock* acb)
{
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        if (gAppSwitcher.processes[i].acb == acb) {
            return &gAppSwitcher.processes[i];
        }
    }
    return NULL;
}

static ProcessInfo* FindProcessByID(uint32_t processID)
{
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        if (gAppSwitcher.processes[i].processID == processID) {
            return &gAppSwitcher.processes[i];
        }
    }
    return NULL;
}

OSErr GetRunningApplications(ProcessSerialNumber* apps, uint32_t* count)
{
    if (!count) {
        return segBadFormat;
    }

    if (!gSwitcherInitialized) {
        *count = 0;
        return segNoErr;
    }

    uint32_t appCount = 0;

    for (int i = 0; i < MAX_RUNNING_APPS && appCount < *count; i++) {
        ProcessInfo* process = &gAppSwitcher.processes[i];
        if (process->state != APP_STATE_INACTIVE) {
            if (apps) {
                /* Create process serial number */
                apps[appCount].highLongOfPSN = 0;
                apps[appCount].lowLongOfPSN = process->processID;
            }
            appCount++;
        }
    }

    *count = appCount;

    DEBUG_LOG("GetRunningApplications: Found %d running applications\n", appCount);
    return segNoErr;
}

bool IsApplicationRunning(uint32_t signature)
{
    if (!gSwitcherInitialized) {
        return false;
    }

    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        ProcessInfo* process = &gAppSwitcher.processes[i];
        if (process->state != APP_STATE_INACTIVE &&
            process->signature == signature) {
            return true;
        }
    }

    return false;
}

ApplicationControlBlock* GetApplicationBySignature(uint32_t signature)
{
    if (!gSwitcherInitialized) {
        return NULL;
    }

    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        ProcessInfo* process = &gAppSwitcher.processes[i];
        if (process->state != APP_STATE_INACTIVE &&
            process->signature == signature) {
            return process->acb;
        }
    }

    return NULL;
}

/* ============================================================================
 * Debugging Support
 * ============================================================================ */

void DumpProcessList(void)
{
    if (!gSwitcherInitialized) {
        DEBUG_LOG("DumpProcessList: Switcher not initialized\n");
        return;
    }

    DEBUG_LOG("Application Switcher Process List:\n");
    DEBUG_LOG("  Multitasking: %s\n", gAppSwitcher.multitaskingEnabled ? "Enabled" : "Disabled");
    DEBUG_LOG("  Cooperative: %s\n", gAppSwitcher.cooperative ? "Yes" : "No");
    DEBUG_LOG("  Active Processes: %d\n", gAppSwitcher.processCount);
    DEBUG_LOG("  Foreground Index: %d\n", gAppSwitcher.foregroundIndex);
    DEBUG_LOG("  Total Switches: %d\n", gAppSwitcher.totalSwitches);

    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        ProcessInfo* process = &gAppSwitcher.processes[i];
        if (process->state != APP_STATE_INACTIVE) {
            const char* stateNames[] = {
                "Inactive", "Foreground", "Background", "???", "Suspended",
                "???", "???", "???", "Launching", "???", "???", "???",
                "???", "???", "???", "???", "Terminating"
            };

            DEBUG_LOG("  [%2d] ID=%3d State=%-11s Pri=%3d Time=%5d Slice=%2d '%.*s'\n",
                     i, process->processID,
                     stateNames[process->state & 0x1F],
                     process->priority, process->totalTime, process->timeSlice,
                     process->name[0], &process->name[1]);
        }
    }
}
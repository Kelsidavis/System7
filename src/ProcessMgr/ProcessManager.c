/* #include "SystemTypes.h" */
#include <stdlib.h>
/*
 * RE-AGENT-BANNER
 * ProcessManager.c - Mac OS System 7 Process Manager Core Implementation
 *
 * implemented based on System.rsrc
 *
 * This implements the cooperative multitasking Process Manager for System 7.
 * The Process Manager enables multiple applications to run simultaneously
 * through cooperative scheduling where applications voluntarily yield control
 * by calling WaitNextEvent or GetNextEvent.
 *
 * Key cooperative multitasking features:
 * - Event-driven scheduling through WaitNextEvent
 * - Process Control Blocks for state management
 * - Memory partition management per process
 * - Context switching for 68k processors
 * - MultiFinder integration for background processing
 *
 * Evidence sources:
 * - evidence.process_manager.json: Function analysis from radare2
 * - mappings.process_manager.json: Function name mappings
 * - layouts.process_manager.json: Data structure layouts
 * RE-AGENT-BANNER
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "sys71_stubs.h"

#include "ProcessMgr/ProcessMgr.h"
#include "ProcessMgr/ProcessLogging.h"
#include "SegmentLoader/SegmentLoader.h"
#include "CPU/CPUBackend.h"
#include "CPU/M68KInterp.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "EventManager/AppSwitcher.h"
/* #include <Traps.h> - not available */
/* #include <ToolUtils.h> - not available */


/*
 * Global Process Manager State

 */
ProcessQueue* gProcessQueue = NULL;
ProcessControlBlock* gCurrentProcess = NULL;
ProcessSerialNumber gSystemProcessPSN = {0, 1};
Boolean gMultiFinderActive = false;
static UInt32 gNextProcessID = 2;
static ProcessControlBlock gProcessTable[kPM_MaxProcesses];

/*
 * Process Manager Initialization

 * Address: 0x00000000, extensive cross-references indicate main initialization
 */
OSErr ProcessManager_Initialize(void)
{
    OSErr err = noErr;

    /* Initialize CPU backends */
    err = M68KBackend_Initialize();
    if (err != noErr) {
        return err;
    }

    /* Initialize process queue */
    gProcessQueue = (ProcessQueue*)NewPtr(sizeof(ProcessQueue));
    if (!gProcessQueue) {
        return memFullErr;
    }

    gProcessQueue->queueHead = NULL;
    gProcessQueue->queueTail = NULL;
    gProcessQueue->queueSize = 0;
    gProcessQueue->currentProcess = NULL;

    /* Initialize process table */
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        gProcessTable[i].processID.highLongOfPSN = 0;
        gProcessTable[i].processID.lowLongOfPSN = 0;
        gProcessTable[i].processState = kProcessTerminated;
        gProcessTable[i].processNextProcess = NULL;
    }

    /* Create system process entry */
    gProcessTable[0].processID = gSystemProcessPSN;
    gProcessTable[0].processSignature = 'MACS';
    gProcessTable[0].processType = 'INIT';
    gProcessTable[0].processState = kProcessRunning;
    gProcessTable[0].processMode = kProcessModeCooperative;
    gCurrentProcess = &gProcessTable[0];

    /* Initialize MultiFinder if available */
    err = MultiFinder_Init();

    return err;
}

/*
 * Process Creation

 * Three-argument function for creating process control blocks
 */
OSErr Process_Create(const void* appSpec, Size memorySize, LaunchFlags flags)
{
    ProcessControlBlock* newProcess = NULL;
    int freeSlot = -1;

    /* Find free process slot */
    for (int i = 1; i < kPM_MaxProcesses; i++) {
        if (gProcessTable[i].processState == kProcessTerminated) {
            freeSlot = i;
            break;
        }
    }

    if (freeSlot == -1) {
        return memFullErr; /* No free process slots */
    }

    newProcess = &gProcessTable[freeSlot];

    /* Initialize process control block */
    newProcess->processID.highLongOfPSN = 0;
    newProcess->processID.lowLongOfPSN = gNextProcessID++;
    newProcess->processSignature = 'APPL'; /* Default application signature */
    newProcess->processType = 'APPL';
    newProcess->processState = kProcessSuspended;
    newProcess->processMode = kProcessModeCooperative | kProcessModeCanBackground;

    /* Allocate memory partition */
    newProcess->processLocation = NewPtr(memorySize);
    if (!newProcess->processLocation) {
        newProcess->processState = kProcessTerminated;
        return memFullErr;
    }
    newProcess->processSize = memorySize;

    /* Setup heap zone */
    newProcess->processHeapZone = (THz)newProcess->processLocation;
    InitZone(NULL, (void*)newProcess->processHeapZone, memorySize, NULL, 0);

    /* Initialize stack */
    newProcess->processStackSize = 8192; /* 8K default stack */
    newProcess->processStackBase = NewPtr(newProcess->processStackSize);
    if (!newProcess->processStackBase) {
        DisposePtr(newProcess->processLocation);
        newProcess->processState = kProcessTerminated;
        return memFullErr;
    }

    /* Setup A5 world */
    newProcess->processA5World = (Ptr)newProcess->processHeapZone + 32;

    /* Initialize timing information */
    newProcess->processCreationTime = TickCount();
    newProcess->processLastEventTime = TickCount();
    newProcess->processEventMask = everyEvent;
    newProcess->processPriority = 1; /* Normal priority */

    /* Allocate context save area */
    newProcess->processContextSave = NewPtr(sizeof(ProcessContext));
    if (!newProcess->processContextSave) {
        DisposePtr(newProcess->processLocation);
        DisposePtr(newProcess->processStackBase);
        newProcess->processState = kProcessTerminated;
        return memFullErr;
    }

    return noErr;
}

/*
 * Cooperative Scheduler - Get Next Process

 * Small function for simple round-robin scheduling
 */
OSErr Scheduler_GetNextProcess(ProcessControlBlock** nextProcess)
{
    ProcessControlBlock* current = gCurrentProcess;
    ProcessControlBlock* candidate = NULL;

    if (!gProcessQueue || gProcessQueue->queueSize == 0) {
        *nextProcess = gCurrentProcess; /* Stay with current if no others */
        return noErr;
    }

    /* Simple round-robin cooperative scheduling */
    if (current && current->processNextProcess) {
        candidate = current->processNextProcess;
    } else {
        candidate = gProcessQueue->queueHead;
    }

    /* Find next runnable process */
    ProcessControlBlock* start = candidate;
    do {
        if (candidate &&
            (candidate->processState == kProcessRunning ||
             candidate->processState == kProcessBackground)) {
            *nextProcess = candidate;
            return noErr;
        }

        candidate = candidate ? candidate->processNextProcess : gProcessQueue->queueHead;

        /* Prevent infinite loop */
        if (candidate == start) {
            break;
        }
    } while (candidate != start);

    /* Default to current process if no runnable process found */
    *nextProcess = gCurrentProcess;
    return noErr;
}

/*
 * Context Switching for Cooperative Multitasking

 * Medium-sized function with parameter for context management
 */
OSErr Context_Switch(ProcessControlBlock* targetProcess)
{
    ProcessContext* currentContext;
    ProcessContext* targetContext;

    if (!targetProcess || !gCurrentProcess) {
        return paramErr;
    }

    if (targetProcess == gCurrentProcess) {
        return noErr; /* No switch needed */
    }

    currentContext = (ProcessContext*)gCurrentProcess->processContextSave;
    targetContext = (ProcessContext*)targetProcess->processContextSave;

    if (!currentContext || !targetContext) {
        return memFullErr;
    }

    /*
     * Save current process context (68k specific)
     * In real implementation, this would use assembly to save/restore registers
     */

    /* Save A5 world */
    currentContext->savedA5 = (UInt32)(uintptr_t)gCurrentProcess->processA5World;

    /* Save stack pointer */
    currentContext->savedStackPointer = (UInt32)(uintptr_t)gCurrentProcess->processStackBase;

    /*
     * Note: In actual 68k implementation, would save:
     * - All data registers (D0-D7)
     * - All address registers (A0-A7)
     * - Status register
     * - Program counter
     */

    /* Switch to target process */
    gCurrentProcess = targetProcess;

    /* Restore target process context */
    /* Set A5 world for global access */
    /* Restore stack pointer */
    /* Restore registers and PC */

    /* Update process timing */
    targetProcess->processLastEventTime = TickCount();

    return noErr;
}

/*
 * Launch Application - Main entry point for starting new processes

 */
OSErr LaunchApplication(LaunchParamBlockRec* launchParams)
{
    OSErr err;
    ProcessControlBlock* newProcess = NULL;
    SegmentLoaderContext* segLoader = NULL;
    CPUAddr entryPoint;
    UInt32 targetPSN;

    if (!launchParams) {
        return paramErr;
    }

    /* Remember the next PSN before creating the process */
    targetPSN = gNextProcessID;

    /* Create new process */
    err = Process_Create(launchParams->launchAppSpec,
                        launchParams->launchPreferredSize,
                        launchParams->launchControlFlags);
    if (err != noErr) {
        return err;
    }

    /* Find the newly created process by PSN */
    for (int i = 1; i < kPM_MaxProcesses; i++) {
        if (gProcessTable[i].processID.lowLongOfPSN == targetPSN &&
            gProcessTable[i].processState != kProcessTerminated) {
            newProcess = &gProcessTable[i];
            break;
        }
    }

    if (newProcess == NULL) {
        return memFullErr; /* Could not find newly created process */
    }

    /* Initialize segment loader for this process */
    err = SegmentLoader_Initialize(newProcess, "m68k_interp", &segLoader);
    if (err != noErr) {
        Process_Cleanup(&newProcess->processID);
        return err;
    }

    /* Load CODE 0 and CODE 1 (entry segments) */
    err = EnsureEntrySegmentsLoaded(segLoader);
    if (err != noErr) {
        SegmentLoader_Cleanup(segLoader);
        Process_Cleanup(&newProcess->processID);
        return err;
    }

    /* Install _LoadSeg trap for lazy segment loading */
    err = InstallLoadSegTrap(segLoader);
    if (err != noErr) {
        /* Non-fatal, continue */
    }

    /* Get CODE 1 entry point */
    err = GetSegmentEntryPoint(segLoader, 1, &entryPoint);
    if (err != noErr) {
        SegmentLoader_Cleanup(segLoader);
        Process_Cleanup(&newProcess->processID);
        return err;
    }

    /* Set up stack (user stack pointer) */
    CPUAddr stackTop = (CPUAddr)(uintptr_t)newProcess->processStackBase +
                       newProcess->processStackSize - 16;
    err = segLoader->cpuBackend->SetStacks(segLoader->cpuAS, stackTop, 0);
    if (err != noErr) {
        SegmentLoader_Cleanup(segLoader);
        Process_Cleanup(&newProcess->processID);
        return err;
    }

    /* Add process to scheduler queue */
    if (gProcessQueue->queueHead == NULL) {
        gProcessQueue->queueHead = newProcess;
        gProcessQueue->queueTail = newProcess;
    } else {
        gProcessQueue->queueTail->processNextProcess = newProcess;
        gProcessQueue->queueTail = newProcess;
    }
    gProcessQueue->queueSize++;

    /* Enter the application (this typically doesn't return) */
    if (!(launchParams->launchControlFlags & kLaunchDontSwitch)) {
        err = segLoader->cpuBackend->EnterAt(segLoader->cpuAS, entryPoint,
                                            kEnterApp);
        /* If we get here, the app returned or there was an error */
    }

    return err;
}

/* WaitNextEvent is defined in sys71_stubs.c - no need to redefine it here */

/*
 * MultiFinder Integration

 */
OSErr MultiFinder_Init(void)
{
    /* For now, just enable MultiFinder */
    /* In real System 7, we'd check Gestalt('mfdr', NULL) */
    gMultiFinderActive = true;
    return noErr;
}

/*
 * Process Cleanup

 * Single byte function suggests cleanup finalization
 */
OSErr Process_Cleanup(ProcessSerialNumber* psn)
{
    ProcessControlBlock* process = NULL;

    /* Find process by PSN */
    for (int i = 0; i < kPM_MaxProcesses; i++) {
        if (gProcessTable[i].processID.lowLongOfPSN == psn->lowLongOfPSN &&
            gProcessTable[i].processID.highLongOfPSN == psn->highLongOfPSN) {
            process = &gProcessTable[i];
            break;
        }
    }

    if (!process) {
        return paramErr;
    }

    /* Clean up process resources */
    if (process->processLocation) {
        DisposePtr(process->processLocation);
    }
    if (process->processStackBase) {
        DisposePtr(process->processStackBase);
    }
    if (process->processContextSave) {
        DisposePtr(process->processContextSave);
    }

    /* Mark as terminated */
    process->processState = kProcessTerminated;

    /* Remove from scheduler queue */
    if (!gProcessQueue) {
        return noErr;  /* No queue to remove from */
    }

    ProcessControlBlock* prev = NULL;
    ProcessControlBlock* curr = gProcessQueue->queueHead;

    /* Find the process in the queue and its predecessor */
    while (curr != NULL && curr != process) {
        prev = curr;
        curr = curr->processNextProcess;
    }

    if (curr == process) {
        /* Found the process, unlink it */
        if (prev == NULL) {
            /* Removing head */
            gProcessQueue->queueHead = process->processNextProcess;
        } else {
            /* Removing middle or tail */
            prev->processNextProcess = process->processNextProcess;
        }

        /* Update tail if we removed the last process */
        if (gProcessQueue->queueTail == process) {
            gProcessQueue->queueTail = prev;  /* prev is now the new tail */
        }

        gProcessQueue->queueSize--;
    }

    return noErr;
}

/**
 * Get front process - used by AppSwitcher
 */
ProcessSerialNumber ProcessManager_GetFrontProcess(void) {
    ProcessSerialNumber psn = {0, 0};

    if (!gCurrentProcess) {
        return psn;
    }

    return gCurrentProcess->processID;
}

/**
 * Set front process - bring app to front (used by AppSwitcher)
 */
OSErr ProcessManager_SetFrontProcess(ProcessSerialNumber psn) {
    extern UInt32 TickCount(void);
    ProcessControlBlock* oldFrontProcess;
    ProcessControlBlock* newFrontProcess;

    if (!gProcessQueue) return noErr;

    /* Save old front process */
    oldFrontProcess = gCurrentProcess;

    /* Find the process to make front */
    newFrontProcess = gProcessQueue->queueHead;
    while (newFrontProcess) {
        if (newFrontProcess->processID.highLongOfPSN == psn.highLongOfPSN &&
            newFrontProcess->processID.lowLongOfPSN == psn.lowLongOfPSN) {
            break;
        }
        newFrontProcess = newFrontProcess->processNextProcess;
    }

    /* Process not found */
    if (!newFrontProcess) {
        return procNotFound;
    }

    /* Already front process */
    if (newFrontProcess == oldFrontProcess) {
        return noErr;
    }

    /* Update current process */
    gCurrentProcess = newFrontProcess;

    /* Update process state to foreground if it was background */
    if (newFrontProcess->processMode == PM_BACKGROUND) {
        newFrontProcess->processMode = PM_FOREGROUND;
    }

    /* Update last event time */
    newFrontProcess->processLastEventTime = TickCount();

    /* Send deactivate event to old front process if it exists */
    if (oldFrontProcess) {
        /* Post deactivate event */
        extern OSErr Proc_PostEvent(EventMask what, UInt32 message);
        Proc_PostEvent(activateEvt, 0);  /* message=0 means deactivate */
    }

    /* Send activate event to new front process */
    extern OSErr Proc_PostEvent(EventMask what, UInt32 message);
    Proc_PostEvent(activateEvt, 1);  /* message=1 means activate */

    serial_printf("[ProcessManager] Switched to front process: signature=%c%c%c%c\n",
                 (newFrontProcess->processSignature >> 24) & 0xFF,
                 (newFrontProcess->processSignature >> 16) & 0xFF,
                 (newFrontProcess->processSignature >> 8) & 0xFF,
                 newFrontProcess->processSignature & 0xFF);

    return noErr;
}

/**
 * Get the process queue for app switcher
 * Internal function for AppSwitcher to access process list
 */
ProcessQueue* ProcessManager_GetProcessQueue(void) {
    return gProcessQueue;
}

/*
 */

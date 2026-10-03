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

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "ProcessMgr/ProcessMgr.h"
#include "ProcessMgr/ProcessLogging.h"
#include "SegmentLoader/SegmentLoader.h"
#include "CPU/CPUBackend.h"
#include "CPU/M68KInterp.h"
#include "CPU/M68KToolbox.h"
#include "ResourceManager.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DITLBuilder.h"
#include "DialogManager/DialogHelpers.h"
#include "WindowManager/WindowManager.h"
#include <string.h>
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
    if (err == noErr) {
        /* Nothing runs 68K code yet, so this is what notices if the
         * interpreter stops working. Silent unless it fails. */
        extern void M68K_SelfTest(void);
        M68K_SelfTest();
    }
    if (err != noErr) {
        return err;
    }

    /* Initialize process queue */
    gProcessQueue = (ProcessQueue*)NewPtr(sizeof(ProcessQueue));
    if (!gProcessQueue) {
        /* M68K backend already initialized - non-fatal, just return error */
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
    gProcessTable[0].processSignature = FOURCC('M','A','C','S');
    gProcessTable[0].processType = FOURCC('I','N','I','T');
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
    (void)appSpec;
    (void)flags;
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
    newProcess->processSignature = FOURCC('A','P','P','L'); /* Default application signature */
    newProcess->processType = FOURCC('A','P','P','L');
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
    newProcess->processHeapZone = (THz)__builtin_assume_aligned(
        newProcess->processLocation, _Alignof(Zone));
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

    currentContext = (ProcessContext*)__builtin_assume_aligned(
        gCurrentProcess->processContextSave, _Alignof(ProcessContext));
    targetContext = (ProcessContext*)__builtin_assume_aligned(
        targetProcess->processContextSave, _Alignof(ProcessContext));

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
 * "The application has unexpectedly quit" - with what stopped it, which for
 * an application from another era is most often a call this system does not
 * answer yet.
 */
extern void SysBeep(short duration);

static void Process_ReportUnexpectedQuit(const char* app, const char* why)
{
    char message[200];
    snprintf(message, sizeof(message),
             "The application \322%s\323 has unexpectedly quit (%s).", app, why);
    DITLBuilder b;
    if (!DITL_Begin(&b, 512)) return;
    DITL_AddButton(&b, 86, 250, 106, 320, "OK");
    DITL_AddText(&b, 14, 20, 76, 320, message);
    Handle ditl = DITL_Finish(&b);
    if (!ditl) return;
    Rect bounds = { 0, 0, 120, 340 };
    DialogPtr dlg = NewDialog(NULL, &bounds, (ConstStr255Param)"\0", false, dBoxProc,
                              (WindowPtr)-1, false, 0, ditl);
    if (!dlg) {
        DisposeHandle(ditl);
        return;
    }
    CenterDialogOnScreen(dlg);
    ShowWindow((WindowPtr)dlg);
    SysBeep(1);
    RunModalDialogBox(dlg, 1, 1);
    DisposeDialog(dlg);
}

/*
 * Launch Application - Main entry point for starting new processes

 */
OSErr LaunchApplication(LaunchParamBlockRec* launchParams)
{
    OSErr err;
    ProcessControlBlock* newProcess = NULL;
    SegmentLoaderContext* segLoader = NULL;
    UInt32 targetPSN;

    if (!launchParams || !launchParams->launchAppSpec) {
        return paramErr;
    }

    /* The application's code is in its own resource fork, which goes in
     * front of the System file for as long as it runs. Its CODE resources
     * were looked for in whatever resource file happened to be current -
     * the System's, which has none, so no application ever loaded. */
    SInt16 savedResFile = CurResFile();
    SInt16 appRes = FSpOpenResFile(launchParams->launchAppSpec, fsRdPerm);
    if (appRes < 0) {
        OSErr resErr = ResError();
        return resErr != noErr ? resErr : resFNotFound;
    }
    UseResFile(appRes);

    targetPSN = gNextProcessID;
    err = Process_Create(launchParams->launchAppSpec,
                        launchParams->launchPreferredSize,
                        launchParams->launchControlFlags);
    if (err != noErr) {
        CloseResFile(appRes);
        UseResFile(savedResFile);
        return err;
    }
    for (int i = 1; i < kPM_MaxProcesses; i++) {
        if (gProcessTable[i].processID.lowLongOfPSN == targetPSN &&
            gProcessTable[i].processState != kProcessTerminated) {
            newProcess = &gProcessTable[i];
            break;
        }
    }
    if (newProcess == NULL) {
        CloseResFile(appRes);
        UseResFile(savedResFile);
        return memFullErr;
    }

    err = SegmentLoader_Initialize(newProcess, "m68k_interp", &segLoader);
    if (err != noErr) {
        Process_Cleanup(&newProcess->processID);
        CloseResFile(appRes);
        UseResFile(savedResFile);
        return err;
    }
    segLoader->resFileRefNum = appRes;      /* closed by SegmentLoader_Cleanup */

    /* CODE 0 lays out the A5 world and its jump table; CODE 1 is the
     * segment the program starts in */
    err = EnsureEntrySegmentsLoaded(segLoader);
    if (err == noErr) err = InstallLoadSegTrap(segLoader);

    /* The stack, in the application's own memory: it was the address of a
     * native block, which meant nothing in the 68K space */
    enum { kAppStackSize = 64 * 1024 };
    CPUAddr stackBase = 0, stackTop = 0;
    if (err == noErr) {
        err = segLoader->cpuBackend->AllocateMemory(segLoader->cpuAS, kAppStackSize,
                                                   kCPUMapA5World, &stackBase);
        stackTop = stackBase + kAppStackSize;
    }
    if (err == noErr) err = segLoader->cpuBackend->SetStacks(segLoader->cpuAS, stackTop, 0);
    if (err == noErr) {
        err = M68KToolbox_Prepare(segLoader, launchParams->launchAppSpec->name,
                                  appRes, stackBase, stackTop,
                                  (VRefNum)launchParams->launchAppSpec->vRefNum,
                                  (DirID)launchParams->launchAppSpec->parID);
    }

    /* Into the program the way the Segment Loader goes in: through the first
     * jump table entry, whose instructions start two bytes into it */
    char appName[64];
    {
        const unsigned char* pn = launchParams->launchAppSpec->name;
        int n = pn[0] < sizeof(appName) - 1 ? pn[0] : (int)sizeof(appName) - 1;
        memcpy(appName, &pn[1], (size_t)n);
        appName[n] = '\0';
    }
    const char* why = NULL;
    if (err == noErr) {
        newProcess->processState = kProcessRunning;
        { char line[96]; snprintf(line, sizeof(line), "[PROC] '%s' started\n", appName); serial_puts(line); }
        err = segLoader->cpuBackend->EnterAt(segLoader->cpuAS,
                                            segLoader->a5World.jtBase + 2, kEnterApp);
        M68KAddressSpace* mas = (M68KAddressSpace*)segLoader->cpuAS;
        why = err != noErr ? (mas->faultReason ? mas->faultReason : "an error") : NULL;
        { char line[160]; snprintf(line, sizeof(line), "[PROC] '%s' %s%s\n", appName, why ? "quit: " : "quit", why ? why : ""); serial_puts(line); }
    } else {
        why = "it could not be loaded";
        { char line[96]; snprintf(line, sizeof(line), "[PROC] '%s' could not be loaded (%d)\n", appName, err); serial_puts(line); }
    }

    M68KToolbox_Finish();
    SegmentLoader_Cleanup(segLoader);
    UseResFile(savedResFile);
    Process_Cleanup(&newProcess->processID);

    /* An application that did not end by quitting is reported, as System 7
     * reports one: it was gone with nothing on the screen to say so */
    if (why) {
        Process_ReportUnexpectedQuit(appName, why);
    }

    /* It ended by launching another (_Launch): that one now, in its place */
    FSSpec next;
    if (!why && M68KToolbox_TakePendingLaunch(&next)) {
        LaunchParamBlockRec chained = *launchParams;
        chained.launchAppSpec = &next;
        return LaunchApplication(&chained);
    }
    return err;
}

/* WaitNextEvent is implemented by the Event Manager in event_manager.c. */

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
                 /* processSignature is uint32_t: the arithmetic widens
     * to long, so %c would get an 8-byte argument.  Cast
     * each byte back down to char. */
                 (char)((newFrontProcess->processSignature >> 24) & 0xFF),
                 (char)((newFrontProcess->processSignature >> 16) & 0xFF),
                 (char)((newFrontProcess->processSignature >> 8) & 0xFF),
                 (char)(newFrontProcess->processSignature & 0xFF));

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

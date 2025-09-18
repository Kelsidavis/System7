/*
 * ApplicationLauncher.c - Application Launch and Lifecycle Management
 *
 * This file implements application launching, initialization, termination,
 * and lifecycle management for Mac OS 7.1 applications.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/FileManager/FileManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr PrepareApplicationLaunch(const FSSpec* appSpec,
                                     const AppParameters* params,
                                     uint16_t flags,
                                     ApplicationControlBlock* acb);
static OSErr LoadApplicationResources(ApplicationControlBlock* acb);
static OSErr SetupApplicationMemory(ApplicationControlBlock* acb);
static OSErr LoadMainSegment(ApplicationControlBlock* acb);
static OSErr InitializeApplicationState(ApplicationControlBlock* acb);
static OSErr CallApplicationMain(ApplicationControlBlock* acb);
static void CleanupApplicationResources(ApplicationControlBlock* acb);
static OSErr ParseSizeResource(ApplicationControlBlock* acb);
static OSErr ValidateApplicationFile(const FSSpec* appSpec);

/* ============================================================================
 * Application Launching
 * ============================================================================ */

OSErr LaunchApplication(const FSSpec* appSpec, const AppParameters* params,
                       uint16_t flags, ApplicationControlBlock** outACB)
{
    if (!appSpec || !outACB) {
        return segBadFormat;
    }

    DEBUG_LOG("LaunchApplication: Launching '%s'\n", appSpec->name);

    /* Validate the application file */
    OSErr err = ValidateApplicationFile(appSpec);
    if (err != segNoErr) {
        DEBUG_LOG("LaunchApplication: Application validation failed: %d\n", err);
        return err;
    }

    /* Allocate application control block */
    ApplicationControlBlock* acb = (ApplicationControlBlock*)
        malloc(sizeof(ApplicationControlBlock));
    if (!acb) {
        DEBUG_LOG("LaunchApplication: Failed to allocate ACB\n");
        return segMemFullErr;
    }

    memset(acb, 0, sizeof(ApplicationControlBlock));

    /* Prepare the launch */
    err = PrepareApplicationLaunch(appSpec, params, flags, acb);
    if (err != segNoErr) {
        free(acb);
        DEBUG_LOG("LaunchApplication: Launch preparation failed: %d\n", err);
        return err;
    }

    /* Load application resources */
    err = LoadApplicationResources(acb);
    if (err != segNoErr) {
        CleanupApplicationResources(acb);
        free(acb);
        DEBUG_LOG("LaunchApplication: Resource loading failed: %d\n", err);
        return err;
    }

    /* Setup application memory */
    err = SetupApplicationMemory(acb);
    if (err != segNoErr) {
        CleanupApplicationResources(acb);
        free(acb);
        DEBUG_LOG("LaunchApplication: Memory setup failed: %d\n", err);
        return err;
    }

    /* Load main segment (CODE 1) */
    err = LoadMainSegment(acb);
    if (err != segNoErr) {
        CleanupApplicationResources(acb);
        free(acb);
        DEBUG_LOG("LaunchApplication: Main segment loading failed: %d\n", err);
        return err;
    }

    /* Initialize application state */
    err = InitializeApplicationState(acb);
    if (err != segNoErr) {
        CleanupApplicationResources(acb);
        free(acb);
        DEBUG_LOG("LaunchApplication: State initialization failed: %d\n", err);
        return err;
    }

    /* Set as current application */
    SetCurrentApplication(acb);

    /* Call application main (if not launching in background) */
    if (!(flags & LAUNCH_CONTINUE)) {
        err = CallApplicationMain(acb);
        if (err != segNoErr) {
            DEBUG_LOG("LaunchApplication: Application main failed: %d\n", err);
            /* Don't fail the launch if main returns an error */
        }
    }

    *outACB = acb;

    DEBUG_LOG("LaunchApplication: Successfully launched application\n");
    return segNoErr;
}

static OSErr PrepareApplicationLaunch(const FSSpec* appSpec,
                                     const AppParameters* params,
                                     uint16_t flags,
                                     ApplicationControlBlock* acb)
{
    DEBUG_LOG("PrepareApplicationLaunch: Preparing launch\n");

    /* Copy file spec */
    acb->appSpec = *appSpec;

    /* Copy launch parameters if provided */
    if (params) {
        size_t paramSize = sizeof(AppParameters) +
                          (params->count - 1) * sizeof(AppFile);
        acb->launchParams = (AppParameters*)malloc(paramSize);
        if (!acb->launchParams) {
            return segMemFullErr;
        }
        memcpy(acb->launchParams, params, paramSize);
    }

    /* Set launch flags */
    acb->launchFlags = flags;

    /* Initialize default values */
    acb->type = APPL_TYPE;
    acb->minMemory = 64 * 1024;      /* 64KB minimum */
    acb->preferredMemory = 256 * 1024; /* 256KB preferred */
    acb->heapSize = acb->preferredMemory;
    acb->stackSize = 8 * 1024;       /* 8KB stack */
    acb->isRunning = false;
    acb->inBackground = !!(flags & LAUNCH_CONTINUE);
    acb->exitCode = 0;

    return segNoErr;
}

static OSErr LoadApplicationResources(ApplicationControlBlock* acb)
{
    DEBUG_LOG("LoadApplicationResources: Loading application resources\n");

    /* Open application resource file */
    acb->resFile = FSpOpenResFile(&acb->appSpec, fsRdPerm);
    if (acb->resFile == -1) {
        DEBUG_LOG("LoadApplicationResources: Failed to open resource file\n");
        return segFileNotFound;
    }

    /* Parse SIZE resource for memory requirements */
    OSErr err = ParseSizeResource(acb);
    if (err != segNoErr && err != segResNotFound) {
        DEBUG_LOG("LoadApplicationResources: SIZE resource parsing failed: %d\n", err);
        return err;
    }

    /* Load application info from resources */
    /* TODO: Load BNDL, ICN#, FREF, and other application resources */

    return segNoErr;
}

static OSErr ParseSizeResource(ApplicationControlBlock* acb)
{
    DEBUG_LOG("ParseSizeResource: Parsing SIZE resource\n");

    /* Try to load SIZE resource with ID -1 (preferred) or ID 0 */
    Handle sizeHandle = Get1Resource('SIZE', -1);
    if (!sizeHandle) {
        sizeHandle = Get1Resource('SIZE', 0);
    }

    if (!sizeHandle) {
        DEBUG_LOG("ParseSizeResource: No SIZE resource found\n");
        return segResNotFound;
    }

    LoadResource(sizeHandle);
    if (ResError() != noErr) {
        ReleaseResource(sizeHandle);
        return segResNotFound;
    }

    /* Parse SIZE resource */
    HLock(sizeHandle);
    uint32_t* sizeData = (uint32_t*)*sizeHandle;

    if (GetHandleSize(sizeHandle) >= 8) {
        /* SIZE resource format:
         * 0: flags
         * 4: preferred size
         * 8: minimum size (if present)
         */
        uint32_t flags = sizeData[0];
        acb->preferredMemory = sizeData[1];

        if (GetHandleSize(sizeHandle) >= 12) {
            acb->minMemory = sizeData[2];
        } else {
            acb->minMemory = acb->preferredMemory;
        }

        /* Use preferred memory for heap size */
        acb->heapSize = acb->preferredMemory;

        DEBUG_LOG("ParseSizeResource: Preferred=%d, Minimum=%d\n",
                 acb->preferredMemory, acb->minMemory);
    }

    HUnlock(sizeHandle);
    ReleaseResource(sizeHandle);

    return segNoErr;
}

static OSErr SetupApplicationMemory(ApplicationControlBlock* acb)
{
    DEBUG_LOG("SetupApplicationMemory: Setting up memory (heap: %d, stack: %d)\n",
             acb->heapSize, acb->stackSize);

    /* Create application heap zone */
    acb->appZone = NewZone(acb->heapSize);
    if (!acb->appZone) {
        DEBUG_LOG("SetupApplicationMemory: Failed to create application zone\n");
        return segMemFullErr;
    }

    /* Allocate application stack */
    acb->stackBase = NewPtr(acb->stackSize);
    if (!acb->stackBase) {
        DisposeZone(acb->appZone);
        acb->appZone = NULL;
        DEBUG_LOG("SetupApplicationMemory: Failed to allocate stack\n");
        return segMemFullErr;
    }

    acb->stackTop = acb->stackBase + acb->stackSize;

    /* Setup A5 world */
    OSErr err = SetupA5World(acb);
    if (err != segNoErr) {
        DisposePtr(acb->stackBase);
        DisposeZone(acb->appZone);
        DEBUG_LOG("SetupApplicationMemory: A5 world setup failed: %d\n", err);
        return err;
    }

    DEBUG_LOG("SetupApplicationMemory: Memory setup completed\n");
    return segNoErr;
}

static OSErr LoadMainSegment(ApplicationControlBlock* acb)
{
    DEBUG_LOG("LoadMainSegment: Loading main segment (CODE 1)\n");

    /* Load CODE 1 (main segment) */
    OSErr err = LoadSeg(1);
    if (err != segNoErr) {
        DEBUG_LOG("LoadMainSegment: Failed to load CODE 1: %d\n", err);
        return err;
    }

    /* Find the main segment in our segment table */
    SegmentDescriptor mainSeg;
    err = GetSegmentInfo(1, &mainSeg);
    if (err != segNoErr) {
        DEBUG_LOG("LoadMainSegment: Failed to get main segment info: %d\n", err);
        return err;
    }

    /* Mark as main segment and protected */
    mainSeg.flags |= SEG_MAIN | SEG_PROTECTED;

    DEBUG_LOG("LoadMainSegment: Main segment loaded at 0x%08X\n", mainSeg.codeAddr);
    return segNoErr;
}

static OSErr InitializeApplicationState(ApplicationControlBlock* acb)
{
    DEBUG_LOG("InitializeApplicationState: Initializing application state\n");

    /* Initialize jump table */
    OSErr err = InitJumpTable(acb);
    if (err != segNoErr) {
        DEBUG_LOG("InitializeApplicationState: Jump table init failed: %d\n", err);
        return err;
    }

    /* Platform-specific initialization */
    err = PlatformLoadApplication(acb);
    if (err != segNoErr) {
        DEBUG_LOG("InitializeApplicationState: Platform init failed: %d\n", err);
        return err;
    }

    acb->isRunning = true;

    DEBUG_LOG("InitializeApplicationState: Application state initialized\n");
    return segNoErr;
}

static OSErr CallApplicationMain(ApplicationControlBlock* acb)
{
    DEBUG_LOG("CallApplicationMain: Calling application main entry point\n");

    /* Get main segment info */
    SegmentDescriptor mainSeg;
    OSErr err = GetSegmentInfo(1, &mainSeg);
    if (err != segNoErr) {
        return err;
    }

    /* Call platform-specific entry point */
    err = PlatformCallEntry(acb, (void*)mainSeg.codeAddr);
    if (err != segNoErr) {
        DEBUG_LOG("CallApplicationMain: Entry point call failed: %d\n", err);
        return err;
    }

    DEBUG_LOG("CallApplicationMain: Application main completed\n");
    return segNoErr;
}

/* ============================================================================
 * Application Termination
 * ============================================================================ */

OSErr TerminateApplication(ApplicationControlBlock* acb, int16_t exitCode)
{
    if (!acb) {
        return segNotApplication;
    }

    DEBUG_LOG("TerminateApplication: Terminating application (exit code: %d)\n", exitCode);

    acb->exitCode = exitCode;
    acb->isRunning = false;

    /* Clean up resources */
    CleanupApplicationResources(acb);

    /* Platform-specific cleanup */
    PlatformCleanupApplication(acb);

    /* If this is the current application, clear it */
    if (GetCurrentApplication() == acb) {
        SetCurrentApplication(NULL);
    }

    /* Free application control block */
    free(acb);

    DEBUG_LOG("TerminateApplication: Application terminated\n");
    return segNoErr;
}

static void CleanupApplicationResources(ApplicationControlBlock* acb)
{
    DEBUG_LOG("CleanupApplicationResources: Cleaning up resources\n");

    /* Unload all application segments */
    UnloadApplicationSegments(acb);

    /* Close resource file */
    if (acb->resFile != -1) {
        CloseResFile(acb->resFile);
        acb->resFile = -1;
    }

    /* Clean up memory */
    if (acb->stackBase) {
        DisposePtr(acb->stackBase);
        acb->stackBase = NULL;
    }

    if (acb->appZone) {
        DisposeZone(acb->appZone);
        acb->appZone = NULL;
    }

    if (acb->globalsPtr) {
        DisposePtr(acb->globalsPtr);
        acb->globalsPtr = NULL;
    }

    if (acb->jumpTable) {
        free(acb->jumpTable);
        acb->jumpTable = NULL;
    }

    if (acb->segments) {
        free(acb->segments);
        acb->segments = NULL;
    }

    if (acb->launchParams) {
        free(acb->launchParams);
        acb->launchParams = NULL;
    }

    if (acb->platformData) {
        free(acb->platformData);
        acb->platformData = NULL;
    }
}

/* ============================================================================
 * Application Switching
 * ============================================================================ */

OSErr SwitchToApplication(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segNotApplication;
    }

    DEBUG_LOG("SwitchToApplication: Switching to application\n");

    ApplicationControlBlock* currentApp = GetCurrentApplication();

    /* Save current application state if different */
    if (currentApp && currentApp != acb) {
        /* TODO: Save current application state */
    }

    /* Set new current application */
    SetCurrentApplication(acb);

    /* Restore application state */
    /* TODO: Restore A5 world, memory context, etc. */

    /* Update background status */
    acb->inBackground = false;
    if (currentApp && currentApp != acb) {
        currentApp->inBackground = true;
    }

    DEBUG_LOG("SwitchToApplication: Switched successfully\n");
    return segNoErr;
}

/* ============================================================================
 * Application Information
 * ============================================================================ */

static OSErr ValidateApplicationFile(const FSSpec* appSpec)
{
    DEBUG_LOG("ValidateApplicationFile: Validating '%s'\n", appSpec->name);

    /* Check if file exists */
    FInfo fileInfo;
    OSErr err = FSpGetFInfo(appSpec, &fileInfo);
    if (err != noErr) {
        DEBUG_LOG("ValidateApplicationFile: File not found or inaccessible\n");
        return segFileNotFound;
    }

    /* Check file type */
    if (fileInfo.fdType != APPL_TYPE && fileInfo.fdType != 'APPL') {
        DEBUG_LOG("ValidateApplicationFile: Not an application file (type: '%c%c%c%c')\n",
                 (char)(fileInfo.fdType >> 24), (char)(fileInfo.fdType >> 16),
                 (char)(fileInfo.fdType >> 8), (char)fileInfo.fdType);
        return segNotApplication;
    }

    /* TODO: Additional validation checks */
    /* - Check for CODE resources */
    /* - Verify file integrity */
    /* - Check permissions */

    DEBUG_LOG("ValidateApplicationFile: Application file is valid\n");
    return segNoErr;
}

/* ============================================================================
 * Segment Management for Applications
 * ============================================================================ */

OSErr LoadApplicationSegments(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segNotApplication;
    }

    DEBUG_LOG("LoadApplicationSegments: Loading all segments for application\n");

    /* Save current resource file */
    int16_t savedResFile = CurResFile();
    UseResFile(acb->resFile);

    /* Get list of CODE resources */
    int16_t numResources = Count1Resources('CODE');
    if (numResources <= 0) {
        UseResFile(savedResFile);
        DEBUG_LOG("LoadApplicationSegments: No CODE resources found\n");
        return segResNotFound;
    }

    /* Allocate segment array */
    acb->segments = (SegmentDescriptor*)calloc(numResources, sizeof(SegmentDescriptor));
    if (!acb->segments) {
        UseResFile(savedResFile);
        return segMemFullErr;
    }

    acb->segmentCount = 0;

    /* Load each CODE resource */
    for (int16_t i = 1; i <= numResources; i++) {
        Handle resHandle = Get1IndResource('CODE', i);
        if (resHandle) {
            int16_t resID;
            ResType resType;
            Str255 resName;

            GetResInfo(resHandle, &resID, &resType, resName);
            ReleaseResource(resHandle);

            /* Load the segment */
            OSErr err = LoadSeg(resID);
            if (err == segNoErr) {
                acb->segmentCount++;
                DEBUG_LOG("LoadApplicationSegments: Loaded CODE %d\n", resID);
            } else {
                DEBUG_LOG("LoadApplicationSegments: Failed to load CODE %d: %d\n",
                         resID, err);
            }
        }
    }

    UseResFile(savedResFile);

    DEBUG_LOG("LoadApplicationSegments: Loaded %d segments\n", acb->segmentCount);
    return segNoErr;
}

OSErr UnloadApplicationSegments(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segNotApplication;
    }

    DEBUG_LOG("UnloadApplicationSegments: Unloading application segments\n");

    /* Unload all segments except the main segment */
    for (uint16_t i = 0; i < acb->segmentCount; i++) {
        if (acb->segments[i].segmentID != 1) { /* Don't unload main segment */
            UnloadSeg((void*)acb->segments[i].codeAddr);
        }
    }

    if (acb->segments) {
        free(acb->segments);
        acb->segments = NULL;
    }
    acb->segmentCount = 0;

    DEBUG_LOG("UnloadApplicationSegments: Segments unloaded\n");
    return segNoErr;
}

/* ============================================================================
 * Platform Stubs (to be implemented by platform-specific code)
 * ============================================================================ */

OSErr PlatformLoadApplication(ApplicationControlBlock* acb)
{
    /* Platform-specific application loading */
    DEBUG_LOG("PlatformLoadApplication: Platform loading (stub)\n");
    return segNoErr;
}

void PlatformCleanupApplication(ApplicationControlBlock* acb)
{
    /* Platform-specific application cleanup */
    DEBUG_LOG("PlatformCleanupApplication: Platform cleanup (stub)\n");
}

OSErr PlatformCallEntry(ApplicationControlBlock* acb, void* entryPoint)
{
    /* Platform-specific entry point call */
    DEBUG_LOG("PlatformCallEntry: Calling entry point at 0x%08X (stub)\n", entryPoint);
    /* In a real implementation, this would transfer control to the Mac application */
    return segNoErr;
}
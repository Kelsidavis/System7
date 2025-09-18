/*
 * SegmentLoaderCore.c - Core Segment Loader Implementation
 *
 * This file implements the core segment loading functionality for Mac OS 7.1,
 * including CODE resource loading, segment management, and the main
 * segment loader trap implementations.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static bool gSegmentLoaderInitialized = false;
static ApplicationControlBlock* gCurrentApplication = NULL;
static uint16_t gNextSegmentID = 2;  /* CODE 0 and 1 are special */
static Handle gAppParmHandle = NULL;
static bool gSegHiEnable = true;     /* Enable MoveHHi in LoadSeg */

/* Segment table for tracking loaded segments */
#define MAX_SEGMENTS 256
static SegmentDescriptor gSegmentTable[MAX_SEGMENTS];
static uint16_t gSegmentCount = 0;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr LoadSegmentInternal(uint16_t segmentID, SegmentDescriptor** outSeg);
static OSErr UnloadSegmentInternal(SegmentDescriptor* seg);
static SegmentDescriptor* FindSegmentByID(uint16_t segmentID);
static SegmentDescriptor* FindSegmentByAddress(void* addr);
static OSErr PatchSegmentCode(SegmentDescriptor* seg);
static OSErr RelocateSegment(SegmentDescriptor* seg);
static uint32_t CalculateSegmentOffset(void* routineAddr);

/* ============================================================================
 * Segment Loader Initialization
 * ============================================================================ */

OSErr InitSegmentLoader(void)
{
    if (gSegmentLoaderInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("InitSegmentLoader: Initializing segment loader\n");

    /* Clear segment table */
    memset(gSegmentTable, 0, sizeof(gSegmentTable));
    gSegmentCount = 0;

    /* Initialize global state */
    gCurrentApplication = NULL;
    gNextSegmentID = 2;
    gAppParmHandle = NULL;
    gSegHiEnable = true;

    gSegmentLoaderInitialized = true;

    DEBUG_LOG("InitSegmentLoader: Segment loader initialized successfully\n");
    return segNoErr;
}

void TerminateSegmentLoader(void)
{
    if (!gSegmentLoaderInitialized) {
        return;
    }

    DEBUG_LOG("TerminateSegmentLoader: Terminating segment loader\n");

    /* Unload all segments */
    for (uint16_t i = 0; i < gSegmentCount; i++) {
        UnloadSegmentInternal(&gSegmentTable[i]);
    }

    /* Clean up global state */
    if (gAppParmHandle) {
        DisposeHandle(gAppParmHandle);
        gAppParmHandle = NULL;
    }

    gCurrentApplication = NULL;
    gSegmentCount = 0;
    gSegmentLoaderInitialized = false;

    DEBUG_LOG("TerminateSegmentLoader: Segment loader terminated\n");
}

/* ============================================================================
 * Core Segment Loading Functions
 * ============================================================================ */

OSErr LoadSeg(uint16_t segmentID)
{
    if (!gSegmentLoaderInitialized) {
        OSErr err = InitSegmentLoader();
        if (err != segNoErr) return err;
    }

    DEBUG_LOG("LoadSeg: Loading segment %d\n", segmentID);

    /* Check if segment is already loaded */
    SegmentDescriptor* seg = FindSegmentByID(segmentID);
    if (seg && (seg->flags & SEG_LOADED)) {
        seg->refCount++;
        DEBUG_LOG("LoadSeg: Segment %d already loaded, ref count now %d\n",
                 segmentID, seg->refCount);
        return segNoErr;
    }

    /* Load the segment */
    OSErr err = LoadSegmentInternal(segmentID, &seg);
    if (err != segNoErr) {
        DEBUG_LOG("LoadSeg: Failed to load segment %d, error %d\n", segmentID, err);
        return err;
    }

    DEBUG_LOG("LoadSeg: Successfully loaded segment %d at 0x%08X\n",
             segmentID, seg->codeAddr);
    return segNoErr;
}

OSErr UnloadSeg(void* routineAddr)
{
    if (!gSegmentLoaderInitialized) {
        return segNotApplication;
    }

    DEBUG_LOG("UnloadSeg: Unloading segment containing address 0x%08X\n", routineAddr);

    /* Find segment containing this address */
    SegmentDescriptor* seg = FindSegmentByAddress(routineAddr);
    if (!seg) {
        DEBUG_LOG("UnloadSeg: No segment found for address 0x%08X\n", routineAddr);
        return segSegmentNotFound;
    }

    /* Decrement reference count */
    if (seg->refCount > 0) {
        seg->refCount--;
    }

    /* Don't unload if still referenced or protected */
    if (seg->refCount > 0 || (seg->flags & SEG_PROTECTED)) {
        DEBUG_LOG("UnloadSeg: Segment %d not unloaded (refs: %d, protected: %d)\n",
                 seg->segmentID, seg->refCount, !!(seg->flags & SEG_PROTECTED));
        return segNoErr;
    }

    /* Unload the segment */
    OSErr err = UnloadSegmentInternal(seg);
    if (err != segNoErr) {
        DEBUG_LOG("UnloadSeg: Failed to unload segment %d, error %d\n",
                 seg->segmentID, err);
        return err;
    }

    DEBUG_LOG("UnloadSeg: Successfully unloaded segment %d\n", seg->segmentID);
    return segNoErr;
}

void ExitToShell(void)
{
    DEBUG_LOG("ExitToShell: Application terminating\n");

    if (gCurrentApplication) {
        TerminateApplication(gCurrentApplication, 0);
    }

    /* Platform-specific exit */
    exit(0);
}

OSErr GetAppParms(Str255 apName, int16_t* apRefNum, Handle* apParam)
{
    if (!gSegmentLoaderInitialized) {
        OSErr err = InitSegmentLoader();
        if (err != segNoErr) return err;
    }

    DEBUG_LOG("GetAppParms: Getting application parameters\n");

    /* Set default values */
    if (apName) {
        apName[0] = 0;  /* Empty Pascal string */
    }
    if (apRefNum) {
        *apRefNum = -1;
    }
    if (apParam) {
        *apParam = gAppParmHandle;
    }

    /* Get current application info */
    if (gCurrentApplication) {
        if (apName) {
            /* Copy application name */
            const char* name = gCurrentApplication->appSpec.name;
            size_t len = strlen(name);
            if (len > 255) len = 255;
            apName[0] = (unsigned char)len;
            memcpy(&apName[1], name, len);
        }

        if (apRefNum) {
            *apRefNum = gCurrentApplication->resFile;
        }
    }

    DEBUG_LOG("GetAppParms: Returned name length %d, refNum %d\n",
             apName ? apName[0] : 0, apRefNum ? *apRefNum : -1);

    return segNoErr;
}

/* ============================================================================
 * Internal Segment Loading Functions
 * ============================================================================ */

static OSErr LoadSegmentInternal(uint16_t segmentID, SegmentDescriptor** outSeg)
{
    /* Find or allocate segment descriptor */
    SegmentDescriptor* seg = FindSegmentByID(segmentID);
    if (!seg) {
        if (gSegmentCount >= MAX_SEGMENTS) {
            return segMemFullErr;
        }
        seg = &gSegmentTable[gSegmentCount++];
        memset(seg, 0, sizeof(SegmentDescriptor));
        seg->segmentID = segmentID;
    }

    /* Get current resource file */
    int16_t resFile = CurResFile();
    if (gCurrentApplication && gCurrentApplication->resFile != -1) {
        resFile = gCurrentApplication->resFile;
    }

    DEBUG_LOG("LoadSegmentInternal: Loading CODE %d from resource file %d\n",
             segmentID, resFile);

    /* Load CODE resource */
    Handle codeHandle = LoadCodeResource(resFile, segmentID);
    if (!codeHandle) {
        DEBUG_LOG("LoadSegmentInternal: Failed to load CODE %d resource\n", segmentID);
        return segResNotFound;
    }

    /* Get code size */
    seg->codeSize = GetHandleSize(codeHandle);
    if (seg->codeSize == 0) {
        ReleaseResource(codeHandle);
        return segBadFormat;
    }

    /* Lock handle and get code address */
    HLock(codeHandle);
    seg->codeHandle = codeHandle;
    seg->codeAddr = (uint32_t)*codeHandle;

    /* Move handle high if enabled */
    if (gSegHiEnable && !(seg->flags & SEG_SYSTEM)) {
        MoveHHi(codeHandle);
        seg->codeAddr = (uint32_t)*codeHandle;
    }

    /* Perform relocation and patching */
    OSErr err = RelocateSegment(seg);
    if (err != segNoErr) {
        ReleaseResource(codeHandle);
        return err;
    }

    err = PatchSegmentCode(seg);
    if (err != segNoErr) {
        ReleaseResource(codeHandle);
        return err;
    }

    /* Mark segment as loaded */
    seg->flags |= SEG_LOADED;
    seg->refCount = 1;

    if (outSeg) {
        *outSeg = seg;
    }

    DEBUG_LOG("LoadSegmentInternal: Successfully loaded segment %d, size %d bytes\n",
             segmentID, seg->codeSize);

    return segNoErr;
}

static OSErr UnloadSegmentInternal(SegmentDescriptor* seg)
{
    if (!seg || !(seg->flags & SEG_LOADED)) {
        return segSegmentNotFound;
    }

    DEBUG_LOG("UnloadSegmentInternal: Unloading segment %d\n", seg->segmentID);

    /* Don't unload main segment or protected segments */
    if ((seg->flags & SEG_MAIN) || (seg->flags & SEG_PROTECTED)) {
        DEBUG_LOG("UnloadSegmentInternal: Segment %d is protected from unloading\n",
                 seg->segmentID);
        return segNoErr;
    }

    /* Release the code resource */
    if (seg->codeHandle) {
        HUnlock(seg->codeHandle);
        ReleaseResource(seg->codeHandle);
        seg->codeHandle = NULL;
    }

    /* Clean up segment descriptor */
    seg->codeAddr = 0;
    seg->codeSize = 0;
    seg->flags &= ~SEG_LOADED;
    seg->refCount = 0;

    return segNoErr;
}

/* ============================================================================
 * Segment Management Utility Functions
 * ============================================================================ */

static SegmentDescriptor* FindSegmentByID(uint16_t segmentID)
{
    for (uint16_t i = 0; i < gSegmentCount; i++) {
        if (gSegmentTable[i].segmentID == segmentID) {
            return &gSegmentTable[i];
        }
    }
    return NULL;
}

static SegmentDescriptor* FindSegmentByAddress(void* addr)
{
    uint32_t targetAddr = (uint32_t)addr;

    for (uint16_t i = 0; i < gSegmentCount; i++) {
        SegmentDescriptor* seg = &gSegmentTable[i];
        if ((seg->flags & SEG_LOADED) &&
            targetAddr >= seg->codeAddr &&
            targetAddr < seg->codeAddr + seg->codeSize) {
            return seg;
        }
    }
    return NULL;
}

static OSErr PatchSegmentCode(SegmentDescriptor* seg)
{
    /* This would contain 68k-specific code patching logic */
    /* For now, just mark as successful */
    DEBUG_LOG("PatchSegmentCode: Patching segment %d (placeholder)\n", seg->segmentID);
    return segNoErr;
}

static OSErr RelocateSegment(SegmentDescriptor* seg)
{
    /* This would contain 68k-specific relocation logic */
    /* For now, just mark as successful */
    DEBUG_LOG("RelocateSegment: Relocating segment %d (placeholder)\n", seg->segmentID);
    return segNoErr;
}

/* ============================================================================
 * Resource Loading Functions
 * ============================================================================ */

Handle LoadCodeResource(int16_t resFile, uint16_t segmentID)
{
    /* Save current resource file */
    int16_t savedResFile = CurResFile();

    /* Switch to specified resource file */
    if (resFile != -1) {
        UseResFile(resFile);
    }

    /* Load CODE resource */
    Handle codeHandle = Get1Resource('CODE', segmentID);

    /* Restore resource file */
    UseResFile(savedResFile);

    if (!codeHandle) {
        DEBUG_LOG("LoadCodeResource: Failed to load CODE %d from file %d\n",
                 segmentID, resFile);
        return NULL;
    }

    /* Load the resource if not already loaded */
    LoadResource(codeHandle);
    OSErr err = ResError();
    if (err != noErr) {
        DEBUG_LOG("LoadCodeResource: Failed to load CODE %d resource, error %d\n",
                 segmentID, err);
        ReleaseResource(codeHandle);
        return NULL;
    }

    DEBUG_LOG("LoadCodeResource: Successfully loaded CODE %d, size %d\n",
             segmentID, GetHandleSize(codeHandle));

    return codeHandle;
}

/* ============================================================================
 * Application File Management
 * ============================================================================ */

void CountAppFiles(int16_t* message, int16_t* count)
{
    DEBUG_LOG("CountAppFiles: Counting application files\n");

    /* Set defaults */
    if (message) *message = 0;
    if (count) *count = 0;

    if (!gAppParmHandle) {
        DEBUG_LOG("CountAppFiles: No application parameters\n");
        return;
    }

    /* Parse application parameter block */
    HLock(gAppParmHandle);
    AppParameters* params = (AppParameters*)*gAppParmHandle;

    if (message) *message = params->message;
    if (count) *count = params->count;

    HUnlock(gAppParmHandle);

    DEBUG_LOG("CountAppFiles: message=%d, count=%d\n",
             message ? *message : 0, count ? *count : 0);
}

void GetAppFiles(int16_t index, AppFile* theFile)
{
    DEBUG_LOG("GetAppFiles: Getting file %d\n", index);

    if (!theFile) return;

    /* Clear file info */
    memset(theFile, 0, sizeof(AppFile));

    if (!gAppParmHandle || index < 1) {
        DEBUG_LOG("GetAppFiles: Invalid parameters\n");
        return;
    }

    HLock(gAppParmHandle);
    AppParameters* params = (AppParameters*)*gAppParmHandle;

    if (index <= params->count) {
        *theFile = params->files[index - 1];
        DEBUG_LOG("GetAppFiles: Found file '%.*s'\n",
                 theFile->fName[0], &theFile->fName[1]);
    } else {
        DEBUG_LOG("GetAppFiles: Index %d out of range (count=%d)\n",
                 index, params->count);
    }

    HUnlock(gAppParmHandle);
}

void ClrAppFiles(int16_t index)
{
    DEBUG_LOG("ClrAppFiles: Clearing file %d\n", index);

    if (!gAppParmHandle || index < 1) {
        return;
    }

    HLock(gAppParmHandle);
    AppParameters* params = (AppParameters*)*gAppParmHandle;

    if (index <= params->count) {
        /* Clear the file type to mark as processed */
        params->files[index - 1].fType = 0;
        DEBUG_LOG("ClrAppFiles: Cleared file %d\n", index);
    }

    HUnlock(gAppParmHandle);
}

/* ============================================================================
 * Compatibility Functions
 * ============================================================================ */

void getappparms(char* apName, int16_t* apRefNum, Handle* apParam)
{
    Str255 pascalName;

    /* Call Pascal version */
    OSErr err = GetAppParms(pascalName, apRefNum, apParam);

    if (err == segNoErr && apName) {
        /* Convert Pascal string to C string */
        uint8_t len = pascalName[0];
        if (len > 0) {
            memcpy(apName, &pascalName[1], len);
        }
        apName[len] = '\0';
    }
}

/* ============================================================================
 * Current Application Management
 * ============================================================================ */

ApplicationControlBlock* GetCurrentApplication(void)
{
    return gCurrentApplication;
}

OSErr SetCurrentApplication(ApplicationControlBlock* acb)
{
    gCurrentApplication = acb;

    if (acb && acb->launchParams) {
        /* Update application parameter handle */
        if (gAppParmHandle) {
            DisposeHandle(gAppParmHandle);
        }

        Size paramSize = sizeof(AppParameters) +
                        (acb->launchParams->count - 1) * sizeof(AppFile);
        gAppParmHandle = NewHandle(paramSize);
        if (gAppParmHandle) {
            HLock(gAppParmHandle);
            memcpy(*gAppParmHandle, acb->launchParams, paramSize);
            HUnlock(gAppParmHandle);
        }
    }

    return segNoErr;
}

/* ============================================================================
 * Segment Information Functions
 * ============================================================================ */

OSErr GetSegmentInfo(uint16_t segmentID, SegmentDescriptor* segDesc)
{
    if (!segDesc) return segBadFormat;

    SegmentDescriptor* seg = FindSegmentByID(segmentID);
    if (!seg) {
        return segSegmentNotFound;
    }

    *segDesc = *seg;
    return segNoErr;
}

OSErr LockSegment(uint16_t segmentID)
{
    SegmentDescriptor* seg = FindSegmentByID(segmentID);
    if (!seg) {
        return segSegmentNotFound;
    }

    seg->flags |= SEG_LOCKED;
    if (seg->codeHandle) {
        HLock(seg->codeHandle);
    }

    DEBUG_LOG("LockSegment: Locked segment %d\n", segmentID);
    return segNoErr;
}

OSErr UnlockSegment(uint16_t segmentID)
{
    SegmentDescriptor* seg = FindSegmentByID(segmentID);
    if (!seg) {
        return segSegmentNotFound;
    }

    seg->flags &= ~SEG_LOCKED;
    if (seg->codeHandle) {
        HUnlock(seg->codeHandle);
    }

    DEBUG_LOG("UnlockSegment: Unlocked segment %d\n", segmentID);
    return segNoErr;
}

OSErr SetSegmentPurgeable(uint16_t segmentID, bool purgeable)
{
    SegmentDescriptor* seg = FindSegmentByID(segmentID);
    if (!seg) {
        return segSegmentNotFound;
    }

    if (purgeable) {
        seg->flags |= SEG_PURGEABLE;
    } else {
        seg->flags &= ~SEG_PURGEABLE;
    }

    DEBUG_LOG("SetSegmentPurgeable: Set segment %d purgeable to %d\n",
             segmentID, purgeable);
    return segNoErr;
}
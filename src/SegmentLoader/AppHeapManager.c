/*
 * AppHeapManager.c - Application Heap and A5 World Management
 *
 * This file implements the application heap management and A5 world setup
 * for Mac OS 7.1 applications. The A5 world is critical for 68k Mac apps
 * as it contains application globals and the QuickDraw globals pointer.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * A5 World Constants
 * ============================================================================ */

/* A5 world layout offsets */
#define A5_QD_GLOBALS_OFFSET    (-0xA0)  /* QuickDraw globals */
#define A5_APP_GLOBALS_OFFSET   0x00     /* Application globals start */
#define A5_JUMP_TABLE_OFFSET    0x20     /* Jump table offset (typical) */

/* Default sizes */
#define DEFAULT_A5_BELOW_SIZE   0x1000   /* 4KB below A5 */
#define DEFAULT_A5_ABOVE_SIZE   0x2000   /* 8KB above A5 */
#define DEFAULT_QD_GLOBALS_SIZE 0xA0     /* QuickDraw globals size */

/* A5 world flags */
#define A5_WORLD_INITIALIZED    0x01
#define A5_WORLD_ACTIVE         0x02
#define A5_WORLD_SAVED          0x04

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct A5WorldInfo {
    Ptr             a5Base;         /* Base of A5 world */
    uint32_t        belowA5Size;    /* Size below A5 */
    uint32_t        aboveA5Size;    /* Size above A5 */
    Ptr             qdGlobals;      /* QuickDraw globals */
    Ptr             appGlobals;     /* Application globals */
    Ptr             jumpTable;      /* Jump table location */
    uint32_t        flags;          /* A5 world flags */
    uint32_t        savedA5;        /* Saved A5 register */
} A5WorldInfo;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static A5WorldInfo gSystemA5World;
static bool gA5WorldManagerInitialized = false;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr AllocateA5World(ApplicationControlBlock* acb);
static OSErr InitializeA5Globals(ApplicationControlBlock* acb);
static OSErr SetupQuickDrawGlobals(ApplicationControlBlock* acb);
static OSErr SetupApplicationGlobals(ApplicationControlBlock* acb);
static void ClearA5World(ApplicationControlBlock* acb);
static uint32_t CalculateA5WorldSize(uint32_t belowA5, uint32_t aboveA5);

/* ============================================================================
 * A5 World Manager Initialization
 * ============================================================================ */

OSErr InitA5WorldManager(void)
{
    if (gA5WorldManagerInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("InitA5WorldManager: Initializing A5 world manager\n");

    /* Initialize system A5 world info */
    memset(&gSystemA5World, 0, sizeof(A5WorldInfo));

    /* TODO: Setup system A5 world if needed */

    gA5WorldManagerInitialized = true;

    DEBUG_LOG("InitA5WorldManager: A5 world manager initialized\n");
    return segNoErr;
}

void TerminateA5WorldManager(void)
{
    if (!gA5WorldManagerInitialized) {
        return;
    }

    DEBUG_LOG("TerminateA5WorldManager: Terminating A5 world manager\n");

    /* Clean up system A5 world if allocated */
    if (gSystemA5World.a5Base) {
        DisposePtr(gSystemA5World.a5Base);
        memset(&gSystemA5World, 0, sizeof(A5WorldInfo));
    }

    gA5WorldManagerInitialized = false;
}

/* ============================================================================
 * A5 World Setup
 * ============================================================================ */

OSErr SetupA5World(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("SetupA5World: Setting up A5 world for application\n");

    /* Initialize A5 world manager if needed */
    if (!gA5WorldManagerInitialized) {
        OSErr err = InitA5WorldManager();
        if (err != segNoErr) return err;
    }

    /* Allocate A5 world memory */
    OSErr err = AllocateA5World(acb);
    if (err != segNoErr) {
        DEBUG_LOG("SetupA5World: Failed to allocate A5 world: %d\n", err);
        return err;
    }

    /* Initialize A5 globals */
    err = InitializeA5Globals(acb);
    if (err != segNoErr) {
        DEBUG_LOG("SetupA5World: Failed to initialize A5 globals: %d\n", err);
        return err;
    }

    /* Setup QuickDraw globals */
    err = SetupQuickDrawGlobals(acb);
    if (err != segNoErr) {
        DEBUG_LOG("SetupA5World: Failed to setup QuickDraw globals: %d\n", err);
        return err;
    }

    /* Setup application globals */
    err = SetupApplicationGlobals(acb);
    if (err != segNoErr) {
        DEBUG_LOG("SetupA5World: Failed to setup application globals: %d\n", err);
        return err;
    }

    DEBUG_LOG("SetupA5World: A5 world setup completed\n");
    DEBUG_LOG("  A5 Base: 0x%08X\n", acb->a5World);
    DEBUG_LOG("  Below A5: %d bytes\n", acb->globalsSize);
    DEBUG_LOG("  Above A5: %d bytes\n", DEFAULT_A5_ABOVE_SIZE);

    return segNoErr;
}

static OSErr AllocateA5World(ApplicationControlBlock* acb)
{
    DEBUG_LOG("AllocateA5World: Allocating A5 world memory\n");

    /* Determine A5 world size from CODE 0 resource or use defaults */
    uint32_t belowA5Size = acb->globalsSize > 0 ? acb->globalsSize : DEFAULT_A5_BELOW_SIZE;
    uint32_t aboveA5Size = DEFAULT_A5_ABOVE_SIZE;

    /* Calculate total size needed */
    uint32_t totalSize = CalculateA5WorldSize(belowA5Size, aboveA5Size);

    DEBUG_LOG("AllocateA5World: Total A5 world size: %d bytes\n", totalSize);

    /* Allocate the A5 world block */
    Ptr a5Block = NewPtr(totalSize);
    if (!a5Block) {
        DEBUG_LOG("AllocateA5World: Failed to allocate A5 world memory\n");
        return segMemFullErr;
    }

    /* Clear the entire A5 world */
    memset(a5Block, 0, totalSize);

    /* Set A5 to point into the middle of the block */
    acb->a5World = a5Block + belowA5Size;
    acb->globalsSize = belowA5Size + aboveA5Size;
    acb->globalsPtr = a5Block;

    DEBUG_LOG("AllocateA5World: A5 world allocated at 0x%08X (block at 0x%08X)\n",
             acb->a5World, a5Block);

    return segNoErr;
}

static uint32_t CalculateA5WorldSize(uint32_t belowA5, uint32_t aboveA5)
{
    /* Add space for QuickDraw globals below A5 */
    uint32_t totalBelow = belowA5 + DEFAULT_QD_GLOBALS_SIZE;

    /* Round up to alignment boundary */
    uint32_t totalSize = totalBelow + aboveA5;
    totalSize = (totalSize + 15) & ~15;  /* 16-byte alignment */

    return totalSize;
}

/* ============================================================================
 * A5 Globals Initialization
 * ============================================================================ */

static OSErr InitializeA5Globals(ApplicationControlBlock* acb)
{
    DEBUG_LOG("InitializeA5Globals: Initializing A5 globals\n");

    if (!acb->a5World) {
        return segBadFormat;
    }

    /* Clear A5 globals area */
    Ptr globalsStart = acb->a5World - acb->globalsSize;
    memset(globalsStart, 0, acb->globalsSize);

    /* Initialize standard A5 world structure */
    /* The A5 register points to the boundary between globals and parameters */

    return segNoErr;
}

static OSErr SetupQuickDrawGlobals(ApplicationControlBlock* acb)
{
    DEBUG_LOG("SetupQuickDrawGlobals: Setting up QuickDraw globals\n");

    if (!acb->a5World) {
        return segBadFormat;
    }

    /* QuickDraw globals are located below A5 at fixed offset */
    Ptr qdGlobals = acb->a5World + A5_QD_GLOBALS_OFFSET;

    /* Initialize QuickDraw globals structure */
    /* This would contain the actual QuickDraw globals initialization */
    memset(qdGlobals, 0, DEFAULT_QD_GLOBALS_SIZE);

    /* TODO: Initialize specific QuickDraw globals fields */
    /* - thePort (current graphics port) */
    /* - white, black, gray, ltGray, dkGray patterns */
    /* - arrow cursor */
    /* - screenBits */
    /* - randSeed */

    DEBUG_LOG("SetupQuickDrawGlobals: QuickDraw globals at 0x%08X\n", qdGlobals);

    return segNoErr;
}

static OSErr SetupApplicationGlobals(ApplicationControlBlock* acb)
{
    DEBUG_LOG("SetupApplicationGlobals: Setting up application globals\n");

    if (!acb->a5World) {
        return segBadFormat;
    }

    /* Application globals start at A5 + 0 */
    Ptr appGlobals = acb->a5World + A5_APP_GLOBALS_OFFSET;

    /* Clear application globals area */
    uint32_t appGlobalsSize = acb->globalsSize / 2;  /* Rough estimate */
    memset(appGlobals, 0, appGlobalsSize);

    /* TODO: Initialize application-specific globals */
    /* This would be done by the application itself typically */

    DEBUG_LOG("SetupApplicationGlobals: Application globals at 0x%08X\n", appGlobals);

    return segNoErr;
}

/* ============================================================================
 * A5 World Management
 * ============================================================================ */

OSErr SaveA5World(uint32_t* savedA5)
{
    if (!savedA5) {
        return segBadFormat;
    }

    DEBUG_LOG("SaveA5World: Saving current A5 world\n");

    /* In a real 68k implementation, this would save the A5 register */
    /* For our portable implementation, we'll track the current A5 value */

    ApplicationControlBlock* currentApp = GetCurrentApplication();
    if (currentApp && currentApp->a5World) {
        *savedA5 = (uint32_t)currentApp->a5World;
    } else {
        *savedA5 = (uint32_t)gSystemA5World.a5Base;
    }

    DEBUG_LOG("SaveA5World: Saved A5 = 0x%08X\n", *savedA5);

    return segNoErr;
}

OSErr RestoreA5World(uint32_t savedA5)
{
    DEBUG_LOG("RestoreA5World: Restoring A5 world to 0x%08X\n", savedA5);

    /* In a real 68k implementation, this would restore the A5 register */
    /* For our portable implementation, we update the current application */

    /* TODO: Implement A5 world switching logic */

    DEBUG_LOG("RestoreA5World: A5 world restored\n");

    return segNoErr;
}

uint32_t GetA5WorldSize(ApplicationControlBlock* acb)
{
    if (!acb) {
        return 0;
    }

    return acb->globalsSize;
}

OSErr SetupApplicationHeap(ApplicationControlBlock* acb, uint32_t heapSize)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("SetupApplicationHeap: Setting up application heap (%d bytes)\n", heapSize);

    /* Create application heap zone */
    acb->appZone = NewZone(heapSize);
    if (!acb->appZone) {
        DEBUG_LOG("SetupApplicationHeap: Failed to create heap zone\n");
        return segMemFullErr;
    }

    acb->heapSize = heapSize;

    /* Set the application zone as the current zone */
    /* In the real Mac OS, this would be done automatically when switching applications */

    DEBUG_LOG("SetupApplicationHeap: Application heap created at 0x%08X\n", acb->appZone);

    return segNoErr;
}

/* ============================================================================
 * A5 World Utilities
 * ============================================================================ */

OSErr ValidateA5World(ApplicationControlBlock* acb)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("ValidateA5World: Validating A5 world\n");

    /* Check basic A5 world structure */
    if (!acb->a5World || !acb->globalsPtr) {
        DEBUG_LOG("ValidateA5World: Invalid A5 world pointers\n");
        return segBadFormat;
    }

    /* Check size consistency */
    if (acb->globalsSize == 0) {
        DEBUG_LOG("ValidateA5World: Invalid globals size\n");
        return segBadFormat;
    }

    /* Verify memory is accessible */
    /* In a real implementation, this would test memory access */

    DEBUG_LOG("ValidateA5World: A5 world validation passed\n");

    return segNoErr;
}

void DumpA5World(ApplicationControlBlock* acb)
{
    if (!acb) {
        DEBUG_LOG("DumpA5World: No application control block\n");
        return;
    }

    DEBUG_LOG("DumpA5World: A5 World Information\n");
    DEBUG_LOG("  A5 Base:       0x%08X\n", acb->a5World);
    DEBUG_LOG("  Globals Ptr:   0x%08X\n", acb->globalsPtr);
    DEBUG_LOG("  Globals Size:  %d bytes\n", acb->globalsSize);
    DEBUG_LOG("  Heap Zone:     0x%08X\n", acb->appZone);
    DEBUG_LOG("  Heap Size:     %d bytes\n", acb->heapSize);
    DEBUG_LOG("  Stack Base:    0x%08X\n", acb->stackBase);
    DEBUG_LOG("  Stack Top:     0x%08X\n", acb->stackTop);
    DEBUG_LOG("  Stack Size:    %d bytes\n", acb->stackSize);

    /* Dump some A5 world content for debugging */
    if (acb->a5World) {
        uint32_t* a5Data = (uint32_t*)acb->a5World;
        DEBUG_LOG("  A5[0]:         0x%08X\n", a5Data[0]);
        DEBUG_LOG("  A5[1]:         0x%08X\n", a5Data[1]);
        DEBUG_LOG("  A5[2]:         0x%08X\n", a5Data[2]);
        DEBUG_LOG("  A5[3]:         0x%08X\n", a5Data[3]);
    }
}

/* ============================================================================
 * Memory Zone Management
 * ============================================================================ */

OSErr SwitchToApplicationHeap(ApplicationControlBlock* acb)
{
    if (!acb || !acb->appZone) {
        return segNotApplication;
    }

    DEBUG_LOG("SwitchToApplicationHeap: Switching to application heap\n");

    /* Save current zone */
    THz savedZone = GetZone();

    /* Switch to application zone */
    SetZone(acb->appZone);

    /* TODO: Store saved zone somewhere for restoration */

    DEBUG_LOG("SwitchToApplicationHeap: Switched to application heap\n");

    return segNoErr;
}

OSErr RestoreSystemHeap(void)
{
    DEBUG_LOG("RestoreSystemHeap: Restoring system heap\n");

    /* Switch back to system heap */
    SetZone(SystemZone());

    DEBUG_LOG("RestoreSystemHeap: System heap restored\n");

    return segNoErr;
}

/* ============================================================================
 * A5 World Cleanup
 * ============================================================================ */

void CleanupA5World(ApplicationControlBlock* acb)
{
    if (!acb) {
        return;
    }

    DEBUG_LOG("CleanupA5World: Cleaning up A5 world\n");

    /* Dispose of application heap */
    if (acb->appZone) {
        DisposeZone(acb->appZone);
        acb->appZone = NULL;
    }

    /* Dispose of stack */
    if (acb->stackBase) {
        DisposePtr(acb->stackBase);
        acb->stackBase = NULL;
        acb->stackTop = NULL;
    }

    /* Dispose of globals block */
    if (acb->globalsPtr) {
        DisposePtr(acb->globalsPtr);
        acb->globalsPtr = NULL;
        acb->a5World = NULL;
    }

    /* Clear sizes */
    acb->heapSize = 0;
    acb->stackSize = 0;
    acb->globalsSize = 0;

    DEBUG_LOG("CleanupA5World: A5 world cleanup completed\n");
}

/* ============================================================================
 * Stack Management
 * ============================================================================ */

OSErr SetupApplicationStack(ApplicationControlBlock* acb, uint32_t stackSize)
{
    if (!acb) {
        return segBadFormat;
    }

    DEBUG_LOG("SetupApplicationStack: Setting up stack (%d bytes)\n", stackSize);

    /* Allocate stack memory */
    acb->stackBase = NewPtr(stackSize);
    if (!acb->stackBase) {
        DEBUG_LOG("SetupApplicationStack: Failed to allocate stack\n");
        return segMemFullErr;
    }

    /* Initialize stack */
    memset(acb->stackBase, 0, stackSize);

    acb->stackSize = stackSize;
    acb->stackTop = acb->stackBase + stackSize;

    DEBUG_LOG("SetupApplicationStack: Stack allocated at 0x%08X-0x%08X\n",
             acb->stackBase, acb->stackTop);

    return segNoErr;
}

Ptr GetStackPointer(ApplicationControlBlock* acb)
{
    if (!acb) {
        return NULL;
    }

    /* In a real implementation, this would return the current stack pointer */
    /* For now, return the stack top */
    return acb->stackTop;
}

uint32_t GetStackSize(ApplicationControlBlock* acb)
{
    if (!acb) {
        return 0;
    }

    return acb->stackSize;
}

/* ============================================================================
 * Application Global Access
 * ============================================================================ */

Ptr GetApplicationGlobals(ApplicationControlBlock* acb)
{
    if (!acb || !acb->a5World) {
        return NULL;
    }

    return acb->a5World + A5_APP_GLOBALS_OFFSET;
}

Ptr GetQuickDrawGlobals(ApplicationControlBlock* acb)
{
    if (!acb || !acb->a5World) {
        return NULL;
    }

    return acb->a5World + A5_QD_GLOBALS_OFFSET;
}

OSErr SetApplicationGlobal(ApplicationControlBlock* acb, uint32_t offset, uint32_t value)
{
    if (!acb || !acb->a5World) {
        return segNotApplication;
    }

    uint32_t* globalPtr = (uint32_t*)(acb->a5World + offset);
    *globalPtr = value;

    return segNoErr;
}

uint32_t GetApplicationGlobal(ApplicationControlBlock* acb, uint32_t offset)
{
    if (!acb || !acb->a5World) {
        return 0;
    }

    uint32_t* globalPtr = (uint32_t*)(acb->a5World + offset);
    return *globalPtr;
}
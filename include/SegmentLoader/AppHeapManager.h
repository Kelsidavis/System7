/*
 * AppHeapManager.h - Application Heap and A5 World Management
 *
 * This file defines the API for application heap management and A5 world
 * setup for Mac OS 7.1 applications, including memory layout and globals.
 */

#ifndef _APP_HEAP_MANAGER_H
#define _APP_HEAP_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "ApplicationTypes.h"
#include "../MemoryManager/MemoryManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * A5 World Constants
 * ============================================================================ */

/* A5 world layout offsets */
#define kA5QDGlobalsOffset      (-0xA0)     /* QuickDraw globals offset */
#define kA5AppGlobalsOffset     0x00        /* Application globals start */
#define kA5JumpTableOffset      0x20        /* Jump table offset (typical) */
#define kA5StackOffset          0x1000      /* Stack offset from A5 */

/* Default memory sizes */
#define kDefaultA5BelowSize     0x1000      /* 4KB below A5 */
#define kDefaultA5AboveSize     0x2000      /* 8KB above A5 */
#define kDefaultQDGlobalsSize   0xA0        /* QuickDraw globals size */
#define kDefaultStackSize       0x2000      /* 8KB default stack */
#define kMinStackSize           0x1000      /* 4KB minimum stack */
#define kMaxStackSize           0x10000     /* 64KB maximum stack */

/* A5 world flags */
#define kA5WorldInitialized     0x01        /* A5 world is initialized */
#define kA5WorldActive          0x02        /* A5 world is active */
#define kA5WorldSaved           0x04        /* A5 world is saved */
#define kA5WorldLocked          0x08        /* A5 world is locked */

/* Memory alignment */
#define kA5WorldAlignment       16          /* 16-byte alignment */
#define kStackAlignment         4           /* 4-byte stack alignment */
#define kHeapAlignment          16          /* 16-byte heap alignment */

/* ============================================================================
 * QuickDraw Globals Layout
 * ============================================================================ */

/* QuickDraw globals offsets from A5 */
#define kQDThePort              (-0xA0)     /* Current graphics port */
#define kQDWhite                (-0x9C)     /* White pattern */
#define kQDBlack                (-0x94)     /* Black pattern */
#define kQDGray                 (-0x8C)     /* Gray pattern */
#define kQDLightGray            (-0x84)     /* Light gray pattern */
#define kQDDarkGray             (-0x7C)     /* Dark gray pattern */
#define kQDArrow                (-0x74)     /* Arrow cursor */
#define kQDScreenBits           (-0x54)     /* Main screen bitmap */
#define kQDRandSeed             (-0x3C)     /* Random seed */

/* QuickDraw globals structure */
typedef struct QDGlobals {
    GrafPtr     thePort;                    /* Current graphics port */
    Pattern     white;                      /* White pattern */
    Pattern     black;                      /* Black pattern */
    Pattern     gray;                       /* Gray pattern */
    Pattern     ltGray;                     /* Light gray pattern */
    Pattern     dkGray;                     /* Dark gray pattern */
    Cursor      arrow;                      /* Arrow cursor */
    BitMap      screenBits;                 /* Screen bitmap */
    int32_t     randSeed;                   /* Random number seed */
    /* Additional QuickDraw globals... */
} QDGlobals;

/* ============================================================================
 * A5 World Information
 * ============================================================================ */

typedef struct A5WorldInfo {
    Ptr         a5Base;                     /* Base of A5 world */
    uint32_t    belowA5Size;                /* Size below A5 */
    uint32_t    aboveA5Size;                /* Size above A5 */
    Ptr         qdGlobals;                  /* QuickDraw globals */
    Ptr         appGlobals;                 /* Application globals */
    Ptr         jumpTable;                  /* Jump table location */
    uint32_t    flags;                      /* A5 world flags */
    uint32_t    savedA5;                    /* Saved A5 register */
} A5WorldInfo;

typedef struct AppMemoryInfo {
    /* Heap information */
    THz         appZone;                    /* Application heap zone */
    uint32_t    heapSize;                   /* Heap size */
    uint32_t    heapUsed;                   /* Used heap space */
    uint32_t    heapFree;                   /* Free heap space */

    /* Stack information */
    Ptr         stackBase;                  /* Stack base */
    Ptr         stackTop;                   /* Stack top */
    Ptr         stackCurrent;               /* Current stack pointer */
    uint32_t    stackSize;                  /* Stack size */
    uint32_t    stackUsed;                  /* Used stack space */

    /* A5 world information */
    Ptr         a5World;                    /* A5 world pointer */
    uint32_t    globalsSize;                /* Globals size */
    Ptr         globalsBase;                /* Globals base */

    /* Memory statistics */
    uint32_t    totalMemory;                /* Total allocated memory */
    uint32_t    usedMemory;                 /* Used memory */
    uint32_t    freeMemory;                 /* Free memory */
} AppMemoryInfo;

/* ============================================================================
 * A5 World Manager Initialization
 * ============================================================================ */

/* Initialize A5 world manager */
OSErr InitA5WorldManager(void);

/* Terminate A5 world manager */
void TerminateA5WorldManager(void);

/* ============================================================================
 * A5 World Setup and Management
 * ============================================================================ */

/* Setup A5 world for application */
OSErr SetupA5World(ApplicationControlBlock* acb);

/* Cleanup A5 world */
void CleanupA5World(ApplicationControlBlock* acb);

/* Save current A5 world */
OSErr SaveA5World(uint32_t* savedA5);

/* Restore A5 world */
OSErr RestoreA5World(uint32_t savedA5);

/* Get A5 world size */
uint32_t GetA5WorldSize(ApplicationControlBlock* acb);

/* Validate A5 world */
OSErr ValidateA5World(ApplicationControlBlock* acb);

/* ============================================================================
 * Application Heap Management
 * ============================================================================ */

/* Setup application heap */
OSErr SetupApplicationHeap(ApplicationControlBlock* acb, uint32_t heapSize);

/* Switch to application heap */
OSErr SwitchToApplicationHeap(ApplicationControlBlock* acb);

/* Restore system heap */
OSErr RestoreSystemHeap(void);

/* Get heap information */
OSErr GetHeapInfo(ApplicationControlBlock* acb, uint32_t* used, uint32_t* free);

/* Compact application heap */
OSErr CompactApplicationHeap(ApplicationControlBlock* acb);

/* Purge application heap */
OSErr PurgeApplicationHeap(ApplicationControlBlock* acb);

/* ============================================================================
 * Stack Management
 * ============================================================================ */

/* Setup application stack */
OSErr SetupApplicationStack(ApplicationControlBlock* acb, uint32_t stackSize);

/* Get stack pointer */
Ptr GetStackPointer(ApplicationControlBlock* acb);

/* Get stack size */
uint32_t GetStackSize(ApplicationControlBlock* acb);

/* Check stack overflow */
bool CheckStackOverflow(ApplicationControlBlock* acb);

/* Get stack usage */
uint32_t GetStackUsage(ApplicationControlBlock* acb);

/* ============================================================================
 * Application Globals Management
 * ============================================================================ */

/* Get application globals pointer */
Ptr GetApplicationGlobals(ApplicationControlBlock* acb);

/* Get QuickDraw globals pointer */
Ptr GetQuickDrawGlobals(ApplicationControlBlock* acb);

/* Set application global value */
OSErr SetApplicationGlobal(ApplicationControlBlock* acb, uint32_t offset,
                          uint32_t value);

/* Get application global value */
uint32_t GetApplicationGlobal(ApplicationControlBlock* acb, uint32_t offset);

/* Initialize application globals */
OSErr InitializeApplicationGlobals(ApplicationControlBlock* acb);

/* ============================================================================
 * QuickDraw Globals Management
 * ============================================================================ */

/* Setup QuickDraw globals */
OSErr SetupQuickDrawGlobals(ApplicationControlBlock* acb);

/* Initialize QuickDraw globals */
OSErr InitializeQuickDrawGlobals(QDGlobals* qdGlobals);

/* Get QuickDraw global value */
uint32_t GetQDGlobal(ApplicationControlBlock* acb, int16_t offset);

/* Set QuickDraw global value */
OSErr SetQDGlobal(ApplicationControlBlock* acb, int16_t offset, uint32_t value);

/* Save QuickDraw state */
OSErr SaveQuickDrawState(ApplicationControlBlock* acb, Handle* state);

/* Restore QuickDraw state */
OSErr RestoreQuickDrawState(ApplicationControlBlock* acb, Handle state);

/* ============================================================================
 * Memory Layout Functions
 * ============================================================================ */

/* Calculate memory layout */
OSErr CalculateMemoryLayout(uint32_t heapSize, uint32_t stackSize,
                           uint32_t globalsSize, AppMemoryLayout* layout);

/* Allocate memory layout */
OSErr AllocateMemoryLayout(const AppMemoryLayout* layout,
                          ApplicationControlBlock* acb);

/* Deallocate memory layout */
OSErr DeallocateMemoryLayout(ApplicationControlBlock* acb);

/* Get memory layout info */
OSErr GetMemoryLayoutInfo(ApplicationControlBlock* acb, AppMemoryLayout* layout);

/* ============================================================================
 * Memory Protection and Validation
 * ============================================================================ */

/* Protect application memory */
OSErr ProtectApplicationMemory(ApplicationControlBlock* acb, bool protect);

/* Validate memory pointers */
OSErr ValidateMemoryPointers(ApplicationControlBlock* acb);

/* Check memory bounds */
bool CheckMemoryBounds(ApplicationControlBlock* acb, Ptr address, uint32_t size);

/* Detect memory corruption */
OSErr DetectMemoryCorruption(ApplicationControlBlock* acb);

/* ============================================================================
 * Memory Statistics and Monitoring
 * ============================================================================ */

/* Get application memory info */
OSErr GetApplicationMemoryInfo(ApplicationControlBlock* acb,
                              AppMemoryInfo* memInfo);

/* Monitor memory usage */
OSErr MonitorMemoryUsage(ApplicationControlBlock* acb, bool enable);

/* Get memory statistics */
OSErr GetMemoryStatistics(ApplicationControlBlock* acb, uint32_t* stats);

/* Log memory usage */
void LogMemoryUsage(ApplicationControlBlock* acb, const char* context);

/* ============================================================================
 * Advanced Memory Management
 * ============================================================================ */

/* Grow application heap */
OSErr GrowApplicationHeap(ApplicationControlBlock* acb, uint32_t additionalSize);

/* Shrink application heap */
OSErr ShrinkApplicationHeap(ApplicationControlBlock* acb, uint32_t reduceSize);

/* Move application heap */
OSErr MoveApplicationHeap(ApplicationControlBlock* acb, Ptr newLocation);

/* Defragment application heap */
OSErr DefragmentApplicationHeap(ApplicationControlBlock* acb);

/* ============================================================================
 * Context Switching Support
 * ============================================================================ */

/* Save application memory context */
OSErr SaveApplicationMemoryContext(ApplicationControlBlock* acb,
                                  Handle* context);

/* Restore application memory context */
OSErr RestoreApplicationMemoryContext(ApplicationControlBlock* acb,
                                     Handle context);

/* Switch memory context */
OSErr SwitchMemoryContext(ApplicationControlBlock* fromACB,
                         ApplicationControlBlock* toACB);

/* ============================================================================
 * Memory Zone Management
 * ============================================================================ */

/* Create application zone */
THz CreateApplicationZone(uint32_t size);

/* Destroy application zone */
OSErr DestroyApplicationZone(THz zone);

/* Set zone attributes */
OSErr SetZoneAttributes(THz zone, uint32_t attributes);

/* Get zone attributes */
uint32_t GetZoneAttributes(THz zone);

/* ============================================================================
 * Handle Management
 * ============================================================================ */

/* Create application handle */
Handle CreateApplicationHandle(ApplicationControlBlock* acb, uint32_t size);

/* Dispose application handle */
OSErr DisposeApplicationHandle(ApplicationControlBlock* acb, Handle handle);

/* Get handle owner */
ApplicationControlBlock* GetHandleOwner(Handle handle);

/* Track application handles */
OSErr TrackApplicationHandles(ApplicationControlBlock* acb, bool enable);

/* ============================================================================
 * Memory Debugging Support
 * ============================================================================ */

/* Dump A5 world information */
void DumpA5World(ApplicationControlBlock* acb);

/* Dump heap information */
void DumpHeapInfo(ApplicationControlBlock* acb);

/* Dump stack information */
void DumpStackInfo(ApplicationControlBlock* acb);

/* Validate heap integrity */
OSErr ValidateHeapIntegrity(ApplicationControlBlock* acb);

/* Check for memory leaks */
OSErr CheckMemoryLeaks(ApplicationControlBlock* acb);

/* ============================================================================
 * Platform-Specific Functions
 * ============================================================================ */

/* Platform-specific A5 world setup */
OSErr PlatformSetupA5World(ApplicationControlBlock* acb);

/* Platform-specific memory protection */
OSErr PlatformProtectMemory(Ptr address, uint32_t size, uint32_t protection);

/* Platform-specific cache management */
OSErr PlatformManageMemoryCache(Ptr address, uint32_t size, uint32_t operation);

#ifdef __cplusplus
}
#endif

#endif /* _APP_HEAP_MANAGER_H */
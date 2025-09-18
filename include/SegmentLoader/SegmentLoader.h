/*
 * SegmentLoader.h - Mac OS 7.1 Segment Loader API
 *
 * This file provides the complete Segment Loader API for Mac OS 7.1,
 * enabling loading and execution of Mac applications with CODE resources,
 * segment management, and application lifecycle control.
 */

#ifndef _SEGMENT_LOADER_H
#define _SEGMENT_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "../ResourceManager/ResourceManager.h"
#include "../MemoryManager/MemoryManager.h"
#include "../FileManager/FileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Segment Loader Constants
 * ============================================================================ */

/* Application launch modes */
enum {
    appOpen = 0,        /* Open the document(s) */
    appPrint = 1        /* Print the document(s) */
};

/* Segment flags */
#define SEG_LOADED          0x01    /* Segment is loaded in memory */
#define SEG_LOCKED          0x02    /* Segment is locked in memory */
#define SEG_PURGEABLE       0x04    /* Segment can be purged */
#define SEG_PRELOAD         0x08    /* Segment should be preloaded */
#define SEG_PROTECTED       0x10    /* Segment is protected from unloading */
#define SEG_SYSTEM          0x20    /* System segment */
#define SEG_MAIN            0x40    /* Main segment (CODE 1) */
#define SEG_COMPRESSED      0x80    /* Segment is compressed */

/* Application types */
#define APPL_TYPE           'APPL'  /* Application */
#define INIT_TYPE           'INIT'  /* System extension */
#define CDEV_TYPE           'cdev'  /* Control panel */
#define DRVR_TYPE           'DRVR'  /* Driver */
#define APPE_TYPE           'appe'  /* Apple event handler */

/* Launch flags */
#define LAUNCH_CONTINUE     0x0001  /* Don't switch to launched app */
#define LAUNCH_NO_FILE_FLAGS 0x0002 /* Ignore file flags */
#define LAUNCH_ASYNC        0x0004  /* Launch asynchronously */
#define LAUNCH_START_CLASSIC 0x0008 /* Start in Classic mode */
#define LAUNCH_PRINT        0x0010  /* Print documents */
#define LAUNCH_RESERVED1    0x0020  /* Reserved */
#define LAUNCH_RESERVED2    0x0040  /* Reserved */
#define LAUNCH_NEW_DOCUMENT 0x0080  /* Create new document */

/* Error codes */
enum {
    segNoErr            = 0,
    segMemFullErr       = -108,     /* Not enough memory */
    segFileNotFound     = -43,      /* File not found */
    segResNotFound      = -192,     /* Resource not found */
    segBadFormat        = -197,     /* Bad executable format */
    segDupPSN           = -198,     /* Duplicate process serial number */
    segAppInTrash       = -199,     /* Application is in trash */
    segNoLaunchData     = -200,     /* No launch data */
    segNotRecognized    = -201,     /* File type not recognized */
    segAppDamaged       = -202,     /* Application is damaged */
    segIncompatible     = -203,     /* Incompatible version */
    segNotApplication   = -204,     /* Not an application */
    segSegmentNotFound  = -205,     /* CODE segment not found */
    segJumpTableFull    = -206,     /* Jump table full */
    segBadPatchAddr     = -207,     /* Bad patch address */
    segHeapFull         = -208      /* Application heap full */
};

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/* Application file reference */
typedef struct AppFile {
    int16_t         vRefNum;        /* Volume reference number */
    OSType          fType;          /* File type */
    int16_t         versNum;        /* Version number (high byte) */
    Str255          fName;          /* File name (Pascal string) */
} AppFile;

/* Application parameters */
typedef struct AppParameters {
    int16_t         message;        /* Launch message (appOpen/appPrint) */
    int16_t         count;          /* Number of files */
    AppFile         files[1];       /* Array of files */
} AppParameters;

/* Segment descriptor */
typedef struct SegmentDescriptor {
    uint16_t        segmentID;      /* Segment number (CODE resource ID) */
    uint32_t        codeAddr;       /* Address of code in memory */
    uint32_t        codeSize;       /* Size of code segment */
    uint32_t        dataAddr;       /* Address of global data */
    uint32_t        dataSize;       /* Size of global data */
    uint16_t        flags;          /* Segment flags */
    uint16_t        refCount;       /* Reference count */
    Handle          codeHandle;     /* Handle to code resource */
    void*           jumpTable;      /* Jump table for this segment */
    uint16_t        jumpCount;      /* Number of jump table entries */
} SegmentDescriptor;

/* Jump table entry */
typedef struct JumpTableEntry {
    uint16_t        opcode;         /* Jump instruction opcode */
    uint32_t        address;        /* Target address */
    uint16_t        segmentID;      /* Segment containing target */
    uint16_t        offset;         /* Offset within segment */
} JumpTableEntry;

/* Application control block */
typedef struct ApplicationControlBlock {
    uint32_t        signature;      /* Application signature */
    uint32_t        type;           /* Application type */
    uint32_t        version;        /* Version number */

    /* Memory management */
    THz             appZone;        /* Application heap zone */
    uint32_t        heapSize;       /* Application heap size */
    uint32_t        stackSize;      /* Application stack size */
    Ptr             stackBase;      /* Base of application stack */
    Ptr             stackTop;       /* Top of application stack */

    /* A5 World */
    Ptr             a5World;        /* A5 world base pointer */
    uint32_t        globalsSize;    /* Size of application globals */
    Ptr             globalsPtr;     /* Pointer to application globals */

    /* Segments */
    uint16_t        segmentCount;   /* Number of CODE segments */
    SegmentDescriptor* segments;    /* Array of segment descriptors */
    JumpTableEntry* jumpTable;     /* Application jump table */
    uint16_t        jumpTableSize;  /* Size of jump table */

    /* Resources */
    int16_t         resFile;        /* Application resource file */
    uint32_t        minMemory;      /* Minimum memory requirement */
    uint32_t        preferredMemory; /* Preferred memory size */

    /* Launch info */
    FSSpec          appSpec;        /* Application file spec */
    AppParameters*  launchParams;   /* Launch parameters */
    uint16_t        launchFlags;    /* Launch flags */

    /* State */
    bool            isRunning;      /* Application is running */
    bool            inBackground;   /* Application is in background */
    int16_t         exitCode;       /* Exit code when terminated */

    /* Platform-specific */
    void*           platformData;   /* Platform-specific data */
} ApplicationControlBlock;

/* ============================================================================
 * Core Segment Loader Functions
 * ============================================================================ */

/* Initialize segment loader */
OSErr InitSegmentLoader(void);

/* Terminate segment loader */
void TerminateSegmentLoader(void);

/* Load a CODE segment */
OSErr LoadSeg(uint16_t segmentID);

/* Unload a CODE segment */
OSErr UnloadSeg(void* routineAddr);

/* Get application launch parameters */
OSErr GetAppParms(Str255 apName, int16_t* apRefNum, Handle* apParam);

/* Exit to shell (terminate application) */
void ExitToShell(void);

/* ============================================================================
 * Application Management
 * ============================================================================ */

/* Launch an application */
OSErr LaunchApplication(const FSSpec* appSpec, const AppParameters* params,
                       uint16_t flags, ApplicationControlBlock** acb);

/* Terminate an application */
OSErr TerminateApplication(ApplicationControlBlock* acb, int16_t exitCode);

/* Get current application control block */
ApplicationControlBlock* GetCurrentApplication(void);

/* Switch to application */
OSErr SwitchToApplication(ApplicationControlBlock* acb);

/* ============================================================================
 * Segment Management
 * ============================================================================ */

/* Load all segments for an application */
OSErr LoadApplicationSegments(ApplicationControlBlock* acb);

/* Unload all segments for an application */
OSErr UnloadApplicationSegments(ApplicationControlBlock* acb);

/* Lock a segment in memory */
OSErr LockSegment(uint16_t segmentID);

/* Unlock a segment */
OSErr UnlockSegment(uint16_t segmentID);

/* Get segment information */
OSErr GetSegmentInfo(uint16_t segmentID, SegmentDescriptor* segDesc);

/* Set segment as purgeable */
OSErr SetSegmentPurgeable(uint16_t segmentID, bool purgeable);

/* ============================================================================
 * Jump Table Management
 * ============================================================================ */

/* Initialize jump table for application */
OSErr InitJumpTable(ApplicationControlBlock* acb);

/* Add entry to jump table */
OSErr AddJumpTableEntry(ApplicationControlBlock* acb, uint16_t segmentID,
                       uint32_t offset, uint32_t* entryAddr);

/* Patch jump table entry */
OSErr PatchJumpTableEntry(ApplicationControlBlock* acb, uint32_t entryAddr,
                         uint32_t newTarget);

/* Resolve jump table references */
OSErr ResolveJumpTableRefs(ApplicationControlBlock* acb);

/* ============================================================================
 * A5 World Management
 * ============================================================================ */

/* Setup A5 world for application */
OSErr SetupA5World(ApplicationControlBlock* acb);

/* Save current A5 world */
OSErr SaveA5World(uint32_t* savedA5);

/* Restore A5 world */
OSErr RestoreA5World(uint32_t savedA5);

/* Get A5 world size */
uint32_t GetA5WorldSize(ApplicationControlBlock* acb);

/* ============================================================================
 * Application File Management
 * ============================================================================ */

/* Count application files */
void CountAppFiles(int16_t* message, int16_t* count);

/* Get application file info */
void GetAppFiles(int16_t index, AppFile* theFile);

/* Clear application file */
void ClrAppFiles(int16_t index);

/* ============================================================================
 * Compatibility Functions
 * ============================================================================ */

/* C-style version of GetAppParms */
void getappparms(char* apName, int16_t* apRefNum, Handle* apParam);

/* ============================================================================
 * Internal Functions (for implementation use)
 * ============================================================================ */

/* Load CODE resource */
Handle LoadCodeResource(int16_t resFile, uint16_t segmentID);

/* Setup application heap */
OSErr SetupApplicationHeap(ApplicationControlBlock* acb, uint32_t heapSize);

/* Parse application resources */
OSErr ParseApplicationResources(ApplicationControlBlock* acb);

/* Initialize application globals */
OSErr InitApplicationGlobals(ApplicationControlBlock* acb);

/* Call application entry point */
OSErr CallApplicationEntry(ApplicationControlBlock* acb);

/* Platform-specific application loader */
OSErr PlatformLoadApplication(ApplicationControlBlock* acb);

/* Platform-specific application cleanup */
void PlatformCleanupApplication(ApplicationControlBlock* acb);

#ifdef __cplusplus
}
#endif

#endif /* _SEGMENT_LOADER_H */
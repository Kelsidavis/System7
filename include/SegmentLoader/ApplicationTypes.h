/*
 * ApplicationTypes.h - Application and Segment Data Structures
 *
 * This file defines the core data structures used by the Segment Loader
 * for application management, segment tracking, and process control.
 */

#ifndef _APPLICATION_TYPES_H
#define _APPLICATION_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "../Types.h"
#include "../MemoryManager/MemoryManager.h"
#include "../FileManager/FileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct ApplicationControlBlock ApplicationControlBlock;
typedef struct ProcessControlBlock ProcessControlBlock;
typedef struct SegmentDescriptor SegmentDescriptor;
typedef struct JumpTableEntry JumpTableEntry;

/* ============================================================================
 * Process Serial Number
 * ============================================================================ */

typedef struct ProcessSerialNumber {
    uint32_t    highLongOfPSN;      /* High 32 bits */
    uint32_t    lowLongOfPSN;       /* Low 32 bits */
} ProcessSerialNumber;

/* ============================================================================
 * Launch Parameter Block
 * ============================================================================ */

typedef uint16_t LaunchFlags;

typedef struct LaunchParamBlockRec {
    uint32_t            reserved1;              /* Reserved */
    uint16_t            reserved2;              /* Reserved */
    uint16_t            launchBlockID;          /* Launch block ID */
    uint32_t            launchEPBLength;        /* EPB length */
    uint16_t            launchFileFlags;        /* File flags */
    LaunchFlags         launchControlFlags;     /* Control flags */
    FSSpec              launchAppSpec;          /* Application spec */
    ProcessSerialNumber launchProcessSN;        /* Process serial number */
    uint32_t            launchPreferredSize;    /* Preferred size */
    uint32_t            launchMinimumSize;      /* Minimum size */
    uint32_t            launchAvailableSize;    /* Available size */
    Handle              launchAppParameters;    /* App parameters */
} LaunchParamBlockRec;

/* ============================================================================
 * Application File Information
 * ============================================================================ */

typedef struct AppFile {
    int16_t     vRefNum;        /* Volume reference number */
    OSType      fType;          /* File type */
    int16_t     versNum;        /* Version number (high byte) */
    Str255      fName;          /* File name (Pascal string) */
} AppFile;

typedef struct AppParameters {
    int16_t     message;        /* Launch message (appOpen/appPrint) */
    int16_t     count;          /* Number of files */
    AppFile     files[1];       /* Array of files (variable length) */
} AppParameters;

/* ============================================================================
 * Application Information
 * ============================================================================ */

typedef struct ApplicationInfo {
    uint32_t    signature;          /* Application signature */
    uint32_t    type;               /* Application type */
    uint32_t    version;            /* Version number */
    uint32_t    minMemory;          /* Minimum memory required */
    uint32_t    preferredMemory;    /* Preferred memory size */
    uint32_t    flags;              /* Application flags */
    uint8_t     name[32];           /* Application name (Pascal) */
} ApplicationInfo;

/* ============================================================================
 * Segment Descriptor
 * ============================================================================ */

typedef struct SegmentDescriptor {
    uint16_t    segmentID;          /* Segment number (CODE resource ID) */
    uint32_t    codeAddr;           /* Address of code in memory */
    uint32_t    codeSize;           /* Size of code segment */
    uint32_t    dataAddr;           /* Address of global data */
    uint32_t    dataSize;           /* Size of global data */
    uint16_t    flags;              /* Segment flags */
    uint16_t    refCount;           /* Reference count */
    Handle      codeHandle;         /* Handle to code resource */
    void*       jumpTable;          /* Jump table for this segment */
    uint16_t    jumpCount;          /* Number of jump table entries */
} SegmentDescriptor;

/* ============================================================================
 * Jump Table Entry
 * ============================================================================ */

typedef struct JumpTableEntry {
    uint16_t    opcode;             /* Jump instruction opcode */
    uint32_t    address;            /* Target address */
    uint16_t    segmentID;          /* Segment containing target */
    uint16_t    offset;             /* Offset within segment */
} JumpTableEntry;

/* ============================================================================
 * Application Control Block
 * ============================================================================ */

typedef struct ApplicationControlBlock {
    /* Application identification */
    uint32_t    signature;          /* Application signature */
    uint32_t    type;               /* Application type */
    uint32_t    version;            /* Version number */

    /* Memory management */
    THz         appZone;            /* Application heap zone */
    uint32_t    heapSize;           /* Application heap size */
    uint32_t    stackSize;          /* Application stack size */
    Ptr         stackBase;          /* Base of application stack */
    Ptr         stackTop;           /* Top of application stack */

    /* A5 World */
    Ptr         a5World;            /* A5 world base pointer */
    uint32_t    globalsSize;        /* Size of application globals */
    Ptr         globalsPtr;         /* Pointer to application globals */

    /* Segments */
    uint16_t    segmentCount;       /* Number of CODE segments */
    SegmentDescriptor* segments;    /* Array of segment descriptors */
    JumpTableEntry* jumpTable;     /* Application jump table */
    uint16_t    jumpTableSize;      /* Size of jump table */

    /* Resources */
    int16_t     resFile;            /* Application resource file */
    uint32_t    minMemory;          /* Minimum memory requirement */
    uint32_t    preferredMemory;    /* Preferred memory size */

    /* Launch info */
    FSSpec      appSpec;            /* Application file spec */
    AppParameters* launchParams;    /* Launch parameters */
    uint16_t    launchFlags;        /* Launch flags */

    /* State */
    bool        isRunning;          /* Application is running */
    bool        inBackground;       /* Application is in background */
    int16_t     exitCode;           /* Exit code when terminated */

    /* Platform-specific */
    void*       platformData;       /* Platform-specific data */
} ApplicationControlBlock;

/* ============================================================================
 * Process Control Block (for MultiFinder compatibility)
 * ============================================================================ */

typedef struct ProcessControlBlock {
    /* Process identification */
    ProcessSerialNumber processNumber;  /* Process serial number */
    uint32_t           processType;     /* Process type */
    OSType             processSignature; /* Process signature */
    uint32_t           processMode;     /* Process mode */
    bool               needSuspendResume; /* Needs suspend/resume */
    bool               canBackground;    /* Can run in background */
    bool               doesActivateOnFGSwitch; /* Activate on foreground switch */
    bool               backgroundAndForeground; /* Background and foreground */
    bool               dontSaveScreen;   /* Don't save screen */
    bool               dieOnSwitch;      /* Die when switched out */
    bool               is32BitCompatible; /* 32-bit compatible */
    bool               isHighLevelEventAware; /* High level event aware */
    bool               localAndRemoteHLEvents; /* Local and remote HL events */
    bool               isStationeryAware; /* Stationery aware */
    bool               useTextEditServices; /* Use TextEdit services */
    bool               onlyBackground;   /* Only background */
    bool               acceptSuspendResumeEvents; /* Accept suspend/resume events */
    bool               reserved1;        /* Reserved */

    /* Process info */
    Str255             processName;      /* Process name */
    uint32_t           processLauncher;  /* Process launcher */
    uint32_t           processLaunchDate; /* Launch date */
    uint32_t           processActiveTime; /* Active time */
    FSSpec             processAppSpec;   /* Application spec */

    /* Memory info */
    uint32_t           processLocation;  /* Process location */
    uint32_t           processSize;      /* Process size */
    uint32_t           processFreeMem;   /* Free memory */

    /* Associated ACB */
    ApplicationControlBlock* acb;        /* Application control block */
} ProcessControlBlock;

/* ============================================================================
 * Launch Context
 * ============================================================================ */

typedef struct LaunchContext {
    FSSpec      *appSpec;               /* Application file spec */
    LaunchFlags flags;                  /* Launch flags */
    uint32_t    memorySize;             /* Memory allocation */
    AppParameters *parameters;          /* Initial parameters */
    ProcessSerialNumber parentPSN;      /* Parent process */
    void        *userData;              /* User data */
} LaunchContext;

/* ============================================================================
 * SIZE Resource Structure
 * ============================================================================ */

typedef struct SizeResource {
    uint16_t    flags;                  /* SIZE flags */
    uint32_t    preferredSize;          /* Preferred memory size */
    uint32_t    minimumSize;            /* Minimum memory size */
} SizeResource;

/* ============================================================================
 * Code Resource Header
 * ============================================================================ */

typedef struct CodeResourceHeader {
    uint32_t    aboveA5Size;            /* Size above A5 */
    uint32_t    belowA5Size;            /* Size below A5 */
    uint32_t    jumpTableSize;          /* Jump table size */
    uint32_t    jumpTableOffset;       /* Jump table offset */
    /* Followed by jump table entries and code */
} CodeResourceHeader;

/* ============================================================================
 * Application Event Record
 * ============================================================================ */

typedef struct AppEventRecord {
    uint16_t    what;                   /* Event type */
    uint32_t    message;                /* Event message */
    uint32_t    when;                   /* Event time */
    Point       where;                  /* Event location */
    uint16_t    modifiers;              /* Event modifiers */
} AppEventRecord;

/* ============================================================================
 * Process Information Record
 * ============================================================================ */

typedef struct ProcessInfoRec {
    uint32_t            processInfoLength;      /* Length of this record */
    Str255              processName;            /* Process name */
    ProcessSerialNumber processNumber;          /* Process serial number */
    uint32_t            processType;            /* Process type */
    OSType              processSignature;       /* Process signature */
    uint32_t            processMode;            /* Process mode */
    Ptr                 processLocation;        /* Process location */
    uint32_t            processSize;            /* Process size */
    uint32_t            processFreeMem;         /* Process free memory */
    ProcessSerialNumber processLauncher;        /* Launcher PSN */
    uint32_t            processLaunchDate;      /* Launch date */
    uint32_t            processActiveTime;      /* Active time */
    FSSpecPtr           processAppSpec;         /* Application file spec */
} ProcessInfoRec;

/* ============================================================================
 * Application Memory Layout
 * ============================================================================ */

typedef struct AppMemoryLayout {
    Ptr         codeStart;              /* Start of code area */
    uint32_t    codeSize;               /* Size of code area */
    Ptr         dataStart;              /* Start of data area */
    uint32_t    dataSize;               /* Size of data area */
    Ptr         heapStart;              /* Start of heap area */
    uint32_t    heapSize;               /* Size of heap area */
    Ptr         stackStart;             /* Start of stack area */
    uint32_t    stackSize;              /* Size of stack area */
    Ptr         a5World;                /* A5 world pointer */
    uint32_t    globalsSize;            /* Size of globals */
} AppMemoryLayout;

/* ============================================================================
 * Cache Entry Information
 * ============================================================================ */

typedef struct CacheEntryInfo {
    uint16_t    segmentID;              /* Cached segment ID */
    uint32_t    codeSize;               /* Size of cached code */
    uint32_t    accessCount;            /* Number of accesses */
    uint32_t    lastAccess;             /* Last access time */
    uint8_t     priority;               /* Cache priority */
    bool        isLocked;               /* Entry is locked */
    bool        isRecent;               /* Recently accessed */
} CacheEntryInfo;

#ifdef __cplusplus
}
#endif

#endif /* _APPLICATION_TYPES_H */
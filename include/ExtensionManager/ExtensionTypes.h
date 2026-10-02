/*
 * ExtensionTypes.h - Extension Manager Type Definitions
 *
 * System 7.1 Portable Extension Manager types, constants, and structures
 * for managing system extensions (INITs), device drivers, and controls.
 *
 * Resource Types Supported:
 * - INIT: System extensions (loaded at boot)
 * - CDEF: Control definitions (used by controls)
 * - DRVR: Device drivers
 * - FKEY: Function key resources
 * - WDEF/LDEF/MDEF: Window/List/Menu definitions
 */

#ifndef EXTENSIONTYPES_H
#define EXTENSIONTYPES_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * EXTENSION CONSTANTS
 * ======================================================================== */

#define MAX_EXTENSIONS              64      /* Maximum concurrent extensions */
#define MAX_EXTENSION_NAME          64      /* Max extension name length */
#define MAX_INIT_PRIORITY           1000    /* Max INIT priority level */
#define EXTENSION_SIGNATURE         FOURCC('e', 'x', 't', 'x') /* Extension registry signature */

/* Resource types for extensions */
#define INIT_TYPE                   FOURCC('I', 'N', 'I', 'T') /* System extension */
#define CDEF_TYPE                   FOURCC('C', 'D', 'E', 'F') /* Control definition */
#define DRVR_TYPE                   FOURCC('D', 'R', 'V', 'R') /* Device driver */
#define FKEY_TYPE                   FOURCC('F', 'K', 'E', 'Y') /* Function key resource */
#define WDEF_TYPE                   FOURCC('W', 'D', 'E', 'F') /* Window definition */
#define LDEF_TYPE                   FOURCC('L', 'D', 'E', 'F') /* List definition */
#define MDEF_TYPE                   FOURCC('M', 'D', 'E', 'F') /* Menu definition */

/* ========================================================================
 * EXTENSION STATES
 * ======================================================================== */

typedef enum {
    EXT_STATE_INVALID = 0,          /* Not loaded or error */
    EXT_STATE_DISCOVERED = 1,       /* Found but not loaded */
    EXT_STATE_LOADED = 2,           /* Code loaded into memory */
    EXT_STATE_INITIALIZED = 3,      /* Initialization entry point called */
    EXT_STATE_ACTIVE = 4,           /* Running and operational */
    EXT_STATE_DISABLED = 5,         /* Disabled by user */
    EXT_STATE_SUSPENDED = 6,        /* Temporarily paused */
    EXT_STATE_ERROR = 7             /* Error during initialization */
} ExtensionState;

/* ========================================================================
 * EXTENSION TYPES
 * ======================================================================== */

typedef enum {
    EXTTYPE_UNKNOWN = 0,
    EXTTYPE_INIT = 1,               /* System extension */
    EXTTYPE_CDEF = 2,               /* Control definition */
    EXTTYPE_DRVR = 3,               /* Device driver */
    EXTTYPE_FKEY = 4,               /* Function key */
    EXTTYPE_WDEF = 5,               /* Window definition */
    EXTTYPE_LDEF = 6,               /* List definition */
    EXTTYPE_MDEF = 7                /* Menu definition */
} ExtensionType;

/* ========================================================================
 * EXTENSION TYPE DEFINITIONS
 * ======================================================================== */

/* Device unit number for driver entries */
typedef SInt16 UnitNum;

/* ========================================================================
 * EXTENSION FUNCTION POINTERS
 * ======================================================================== */

/* INIT entry point: called once at system startup
 * Returns error code or 0 on success */
typedef OSErr (*InitEntryProc)(void);

/* INIT startup entry point (alternative):
 * Called with startup event information */
typedef void (*InitEventProc)(SInt16 eventCode);

/* CDEF entry point: handles control behavior */
typedef SInt16 (*ControlDefProc)(SInt16 varCode, ControlHandle theControl,
                                 SInt16 message, SInt32 param);

/* DRVR entry point: called by driver manager */
typedef OSErr (*DriverEntryProc)(UnitNum unitNum, SInt16 csCode,
                                 void *pb);

/* ========================================================================
 * EXTENSION HEADERS
 * ======================================================================== */

/* INIT Resource Header Format */
typedef struct InitHeader {
    SInt16  majorVersion;           /* Major version */
    SInt16  minorVersion;           /* Minor version */
    Str255  name;                   /* Extension name (Pascal string) */
    Str255  description;            /* Human-readable description */
} InitHeader;

/* Extension Resource Structure */
typedef struct ExtensionResource {
    SInt16          resourceID;     /* Resource ID in file */
    OSType          resourceType;   /* 'INIT', 'CDEF', etc. */
    Handle          resourceHandle; /* Loaded resource handle */
    SInt32          resourceSize;   /* Size in bytes */
    Str255          resourceName;   /* Name from resource fork */
} ExtensionResource;

/* ========================================================================
 * EXTENSION ENTRY
 * ======================================================================== */

typedef struct Extension {
    SInt16          refNum;                 /* Extension reference number */
    char            name[MAX_EXTENSION_NAME]; /* Extension name */
    ExtensionType   type;                   /* Extension type */
    ExtensionState  state;                  /* Current state */
    UInt32          flags;                  /* Extension flags */

    /* Resource information */
    OSType          resourceType;           /* Resource type (INIT, CDEF, etc.) */
    SInt16          resourceID;             /* Resource ID */
    Handle          codeHandle;             /* Loaded code */
    SInt32          codeSize;               /* Code size in bytes */

    /* Entry points */
    InitEntryProc   initEntry;              /* Initialization entry point */
    void*           otherEntry;             /* Other entry points (varies by type) */

    /* Metadata */
    SInt16          majorVersion;           /* Version info */
    SInt16          minorVersion;
    SInt16          priority;               /* Load priority (INIT only) */
    SInt32          refCon;                 /* Client reference data */

    /* Statistics */
    SInt32          loadTime;               /* Time loaded (ticks) */
    SInt32          initTime;               /* Time initialized (ticks) */
    OSErr           lastError;              /* Last error code */

    /* List management */
    struct Extension *next;
    struct Extension *prev;
} Extension;

/* Extension Handle (pointer to pointer) */
typedef Extension** ExtensionHandle;

/* ========================================================================
 * EXTENSION REGISTRY
 * ======================================================================== */

typedef struct ExtensionRegistry {
    SInt32 signature;                       /* Validation signature */
    SInt16 extensionCount;                  /* Number of extensions */
    SInt16 activeCount;                     /* Number of active extensions */

    Extension *firstExtension;              /* Linked list head */
    Extension *lastExtension;               /* Linked list tail */

    SInt16 nextRefNum;                      /* Next reference number to assign */
    Boolean autoLoadEnabled;                /* Auto-load new extensions */
    Boolean debugMode;                      /* Debug logging enabled */

    /* Statistics */
    SInt32 totalLoadTime;                   /* Total time spent loading */
    SInt32 totalInitTime;                   /* Total time spent initializing */
    SInt32 lastScanTime;                    /* Last filesystem scan time */
} ExtensionRegistry;

/* ========================================================================
 * EXTENSION FLAGS
 * ======================================================================== */

#define EXT_FLAG_ENABLED            0x0001  /* Extension is enabled */
#define EXT_FLAG_REQUIRED           0x0002  /* Must load or boot fails */
#define EXT_FLAG_SYSTEM             0x0004  /* System extension (not user) */
#define EXT_FLAG_PROTECTED          0x0008  /* Cannot be disabled */
#define EXT_FLAG_PATCHED            0x0010  /* Patches system traps */
#define EXT_FLAG_BACKGROUND         0x0020  /* Can run in background */
#define EXT_FLAG_PERSISTENT         0x0040  /* Survives restart */
#define EXT_FLAG_DEBUG              0x8000  /* Debug version */

/* ========================================================================
 * INIT PRIORITY RANGES
 * ======================================================================== */

/* INIT resources are loaded in priority order */
#define INIT_PRIORITY_CRITICAL      1       /* Boot before everything */
#define INIT_PRIORITY_DRIVERS       50      /* Device drivers */
#define INIT_PRIORITY_PATCHES       100     /* System patches */
#define INIT_PRIORITY_NORMAL        500     /* Normal extensions */
#define INIT_PRIORITY_UTILITIES     800     /* Utilities */
#define INIT_PRIORITY_LAST          999     /* Load last */

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define extNoErr                    0       /* Success */
#define extNotFound                 -600    /* Extension not found */
#define extAlreadyLoaded            -601    /* Already loaded */
#define extMemError                 -602    /* Memory allocation error */
#define extBadResource              -603    /* Invalid resource data */
#define extInitFailed               -604    /* Initialization failed */
#define extVersionMismatch          -605    /* Version incompatibility */
#define extDependencyFailed         -606    /* Dependency not met */
#define extDisabled                 -607    /* Extension is disabled */
#define extMaxExtensions            -608    /* Too many extensions */

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONTYPES_H */

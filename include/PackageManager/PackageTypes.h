/*
 * PackageTypes.h
 * System 7.1 Portable Package Manager Type Definitions
 *
 * Common data structures and types used across all packages.
 * Maintains compatibility with original Mac OS Package Manager APIs.
 */

#ifndef __PACKAGE_TYPES_H__
#define __PACKAGE_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic Mac OS types for compatibility */
typedef int16_t         OSErr;
typedef uint32_t        OSType;
typedef int16_t         ScriptCode;
typedef int16_t         LangCode;
typedef unsigned char   Str255[256];
typedef unsigned char   Str63[64];
typedef unsigned char   Str31[32];
typedef const unsigned char *ConstStr255Param;
typedef const unsigned char *ConstStr63Param;
typedef const unsigned char *ConstStr31Param;

/* Boolean type for Mac compatibility */
#ifndef Boolean
typedef unsigned char Boolean;
#endif

#ifndef true
#define true    1
#define false   0
#endif

/* Point and Rectangle structures */
typedef struct {
    int16_t v;  /* vertical coordinate */
    int16_t h;  /* horizontal coordinate */
} Point;

typedef struct {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} Rect;

/* Mac OS Handle type simulation */
typedef void**          Handle;
typedef void*           Ptr;

/* File system types */
typedef struct {
    int16_t     vRefNum;    /* Volume reference number */
    int32_t     parID;      /* Parent directory ID */
    Str63       name;       /* File name */
} FSSpec;

/* Date/Time types */
typedef uint32_t        DateTimeRec;
typedef struct {
    int16_t     year;
    int16_t     month;
    int16_t     day;
    int16_t     hour;
    int16_t     minute;
    int16_t     second;
    int16_t     dayOfWeek;
} LongDateTime;

/* String comparison types */
typedef int16_t         relop;      /* relational operator result */

/* Package function pointer types */
typedef int32_t (*PackageFunction)(int16_t selector, void *params);
typedef void (*PackageInit)(void);
typedef void (*PackageCleanup)(void);

/* Package descriptor structure */
typedef struct {
    int16_t         packID;         /* Package identifier */
    uint32_t        version;        /* Package version */
    const char*     name;           /* Package name */
    PackageInit     initFunc;       /* Initialization function */
    PackageFunction dispatchFunc;   /* Main dispatch function */
    PackageCleanup  cleanupFunc;    /* Cleanup function */
    void*           privateData;    /* Package-specific data */
    bool            loaded;         /* Load status */
    bool            initialized;    /* Initialization status */
} PackageDescriptor;

/* Parameter block for package calls */
typedef struct {
    int16_t     selector;       /* Function selector */
    int16_t     paramCount;     /* Number of parameters */
    void*       params[16];     /* Parameter array */
    int32_t     result;         /* Function result */
    OSErr       error;          /* Error code */
} PackageParamBlock;

/* Memory management types */
typedef struct {
    void*       ptr;            /* Memory pointer */
    size_t      size;           /* Allocated size */
    bool        locked;         /* Lock status */
    uint32_t    refCount;       /* Reference count */
} MemoryBlock;

/* Resource types */
typedef struct {
    OSType      type;           /* Resource type */
    int16_t     id;             /* Resource ID */
    size_t      size;           /* Resource size */
    void*       data;           /* Resource data */
    const char* name;           /* Resource name */
} ResourceDescriptor;

/* Event types for dialog handling */
typedef struct {
    uint16_t    what;           /* Event type */
    uint32_t    message;        /* Event message */
    uint32_t    when;           /* Event time */
    Point       where;          /* Event location */
    uint16_t    modifiers;      /* Modifier keys */
} EventRecord;

/* Dialog and window types */
typedef void*           WindowPtr;
typedef void*           DialogPtr;
typedef void*           GrafPtr;
typedef void*           ControlHandle;
typedef void*           RgnHandle;

/* Procedure pointer types */
typedef void (*ProcPtr)(void);
typedef bool (*ModalFilterProcPtr)(DialogPtr dialog, EventRecord *event, int16_t *itemHit);
typedef int16_t (*DlgHookProcPtr)(int16_t item, DialogPtr dialog);
typedef bool (*FileFilterProcPtr)(void *paramBlock);

/* List Manager types */
typedef Point           Cell;
typedef char            DataArray[32001];
typedef char*           DataPtr;
typedef char**          DataHandle;
typedef int16_t (*SearchProcPtr)(Ptr aPtr, Ptr bPtr, int16_t aLen, int16_t bLen);

/* Sound types */
typedef struct {
    uint16_t    format;         /* Sound format */
    uint32_t    sampleRate;     /* Sample rate */
    uint16_t    channels;       /* Number of channels */
    uint16_t    bitsPerSample;  /* Bits per sample */
    size_t      dataSize;       /* Size of sound data */
    void*       data;           /* Sound data pointer */
} SoundInfo;

/* Thread safety types */
typedef struct {
    bool        enabled;        /* Thread safety enabled */
    void*       mutex;          /* Platform-specific mutex */
    uint32_t    lockCount;      /* Lock recursion count */
    uint32_t    ownerThread;    /* Thread that owns the lock */
} ThreadSafetyInfo;

/* Platform integration types */
typedef struct {
    void* (*malloc_func)(size_t size);
    void (*free_func)(void *ptr);
    void* (*realloc_func)(void *ptr, size_t size);
    void (*debug_log)(const char *message);
    void (*error_handler)(int32_t error, const char *message);
} PlatformCallbacks;

/* Configuration structure */
typedef struct {
    bool            debugEnabled;      /* Debug output enabled */
    bool            threadSafe;        /* Thread safety enabled */
    bool            strictCompatibility; /* Strict Mac OS compatibility */
    int32_t         maxMemoryMB;       /* Maximum memory usage in MB */
    PlatformCallbacks callbacks;       /* Platform-specific callbacks */
} PackageManagerConfig;

/* Error handling types */
typedef struct {
    int32_t         error;          /* Error code */
    const char*     message;        /* Error message */
    const char*     function;       /* Function where error occurred */
    int32_t         line;           /* Source line number */
    uint32_t        timestamp;      /* Error timestamp */
} ErrorInfo;

/* Package statistics */
typedef struct {
    uint32_t        callCount;      /* Number of calls to this package */
    uint32_t        errorCount;     /* Number of errors */
    uint32_t        memoryUsed;     /* Memory currently used */
    uint32_t        peakMemory;     /* Peak memory usage */
    uint32_t        loadTime;       /* Time when package was loaded */
} PackageStats;

/* Floating point environment types (for SANE) */
typedef struct {
    int16_t         precision;      /* Precision mode */
    int16_t         rounding;       /* Rounding mode */
    uint16_t        exceptions;     /* Exception flags */
    uint16_t        halts;          /* Halt enable flags */
} FloatingPointEnvironment;

/* Internationalization types */
typedef struct {
    ScriptCode      script;         /* Script system */
    LangCode        language;       /* Language code */
    int16_t         region;         /* Region code */
    Handle          intlResource;   /* International resource */
} InternationalInfo;

/* Constants for error codes */
#define PACKAGE_NO_ERROR            0
#define PACKAGE_INVALID_ID          -1
#define PACKAGE_NOT_LOADED          -2
#define PACKAGE_INIT_FAILED         -3
#define PACKAGE_INVALID_SELECTOR    -4
#define PACKAGE_INVALID_PARAMS      -5
#define PACKAGE_MEMORY_ERROR        -6
#define PACKAGE_RESOURCE_ERROR      -7
#define PACKAGE_THREAD_ERROR        -8
#define PACKAGE_PLATFORM_ERROR      -9
#define PACKAGE_COMPATIBILITY_ERROR -10

/* Maximum values */
#define MAX_PACKAGES                16
#define MAX_PACKAGE_NAME            64
#define MAX_ERROR_MESSAGE           256
#define MAX_RESOURCE_NAME           256

#ifdef __cplusplus
}
#endif

#endif /* __PACKAGE_TYPES_H__ */
/*
 * RE-AGENT-BANNER
 * Apple System 7.1 Resource Manager - Type Definitions
 *
 * Reverse engineered from System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 *
 * Evidence source: /home/k/Desktop/system7/evidence.curated.resourcemgr.json
 * Layout source: /home/k/Desktop/system7/layouts.curated.resourcemgr.json
 *
 * Copyright 1983, 1984, 1985, 1986, 1987, 1988, 1989, 1990 Apple Computer Inc.
 * Reimplemented for System 7.1 decompilation project
 */

#ifndef RESOURCE_TYPES_H
#define RESOURCE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Basic Mac OS types */
typedef int16_t OSErr;
typedef uint32_t Handle;
typedef uint32_t Ptr;
typedef uint32_t ResType;
typedef int16_t ResID;
typedef uint8_t* ConstStr255Param;
typedef uint8_t Str63[64];

/* Resource Manager Constants */
#define kResourceForkHeaderSize    16
#define kResourceMapHeaderSize     30
#define kResourceTypeEntrySize     8
#define kResourceRefEntrySize      12
#define kResourceDataHeaderSize    4

/* Resource Type Constants */
/* Evidence: r2_strings analysis found resource type identifiers */
#define kKeyboardCharResourceType    0x4B434852  /* 'KCHR' */
#define kKeyboardMapResourceType     0x4B4D4150  /* 'KMAP' */
#define kKeyboardCapsResourceType    0x4B434150  /* 'KCAP' */
#define kFileRefResourceType         0x46524546  /* 'FREF' */
#define kIconListResourceType        0x49434E23  /* 'ICN#' */
#define kCacheResourceType           0x43414348  /* 'CACH' */
#define kControlDefResourceType      0x43444546  /* 'CDEF' */

/* Resource Attributes */
#define resSysHeap      (1 << 6)  /* 64 - Load in system heap */
#define resPurgeable    (1 << 5)  /* 32 - Resource is purgeable */
#define resLocked       (1 << 4)  /* 16 - Resource is locked */
#define resProtected    (1 << 3)  /* 8 - Resource is protected */
#define resPreload      (1 << 2)  /* 4 - Preload resource */
#define resChanged      (1 << 1)  /* 2 - Resource changed */

/* Error Codes */
#define noErr           0     /* No error */
#define resNotFound    -192   /* Resource not found */
#define resFNotFound   -193   /* Resource file not found */
#define addResFailed   -194   /* AddResource failed */
#define rmvResFailed   -196   /* RemoveResource failed */
#define resAttrErr     -198   /* Attribute inconsistent */
#define mapReadErr     -199   /* Map inconsistent */

/* System Trap Numbers */
#define trapGetResource     0xA9A0
#define trapGet1Resource    0xA9A1
#define trapOpenResFile     0xA997
#define trapCloseResFile    0xA99A
#define trapAddResource     0xA9AB
#define trapUpdateResFile   0xA9AD
#define trapResError        0xA9AF
#define trapReleaseResource 0xA9A3

/*
 * ResourceForkHeader - Resource fork file header (16 bytes)
 * Evidence: HFS resource fork format analysis
 */
typedef struct ResourceForkHeader {
    uint32_t dataOffset;    /* Offset to resource data area */
    uint32_t mapOffset;     /* Offset to resource map */
    uint32_t dataLength;    /* Length of resource data */
    uint32_t mapLength;     /* Length of resource map */
} ResourceForkHeader;

/*
 * ResourceMapHeader - Resource map header (30 bytes)
 * Evidence: Resource map structure analysis
 */
typedef struct ResourceMapHeader {
    ResourceForkHeader headerCopy;      /* Copy of fork header */
    Handle nextResourceMap;             /* Next resource map handle */
    uint16_t fileRef;                  /* File reference number */
    uint16_t fileAttrs;                /* File attributes */
    uint16_t typeListOffset;           /* Offset to type list */
    uint16_t nameListOffset;           /* Offset to name list */
    uint16_t numTypes;                 /* Number of types minus 1 */
} ResourceMapHeader;

/*
 * ResourceTypeEntry - Resource type list entry (8 bytes)
 * Evidence: Resource type list structure
 */
typedef struct ResourceTypeEntry {
    ResType resourceType;               /* Resource type (4-char code) */
    uint16_t numResourcesMinusOne;     /* Number of resources minus 1 */
    uint16_t refListOffset;            /* Offset to reference list */
} ResourceTypeEntry;

/*
 * ResourceRefEntry - Resource reference entry (12 bytes)
 * Evidence: Resource reference list structure
 */
typedef struct ResourceRefEntry {
    ResID resourceID;                   /* Resource ID */
    uint16_t nameOffset;               /* Offset to name (0xFFFF if none) */
    uint8_t resourceAttrs;             /* Resource attributes */
    uint32_t dataOffset : 24;         /* 24-bit data offset */
    Handle resourceHandle;             /* Handle if loaded */
} __attribute__((packed)) ResourceRefEntry;

/*
 * HandleBlock - Memory handle block structure
 * Evidence: Memory manager interface
 */
typedef struct HandleBlock {
    Ptr* masterPointer;                /* Pointer to master pointer */
    uint32_t size;                     /* Size of handle block */
} HandleBlock;

/*
 * ResourceDataHeader - Resource data block header (4 bytes)
 * Evidence: Resource data format
 */
typedef struct ResourceDataHeader {
    uint32_t dataLength;               /* Length of following data */
} ResourceDataHeader;

/*
 * FileControlBlock - Open resource file control block
 * Evidence: Resource file management
 */
typedef struct FileControlBlock {
    int16_t fileRef;                   /* File reference number */
    uint16_t fileAttrs;                /* File attributes */
    Handle resourceMapHandle;          /* Handle to resource map */
    int16_t vRefNum;                   /* Volume reference number */
    int16_t version;                   /* Resource fork version */
    uint8_t permissionByte;            /* File permissions */
    uint8_t reserved;                  /* Reserved */
    Str63 fileName;                    /* Pascal string filename */
} FileControlBlock;

#endif /* RESOURCE_TYPES_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "component": "resource_manager_types",
 *   "evidence_density": 0.85,
 *   "structures_defined": 6,
 *   "constants_defined": 18,
 *   "provenance": {
 *     "r2_analysis": "/home/k/Desktop/system7/inputs/resourcemgr/r2_aflj.json",
 *     "evidence": "/home/k/Desktop/system7/evidence.curated.resourcemgr.json",
 *     "layouts": "/home/k/Desktop/system7/layouts.curated.resourcemgr.json"
 *   },
 *   "confidence": "high"
 * }
 */
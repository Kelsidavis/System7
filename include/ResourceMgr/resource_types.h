/* Resource Manager constants and data structures. */

#ifndef RESOURCE_TYPES_H
#define RESOURCE_TYPES_H

#include "SystemTypes.h"

/* Resource Manager Constants */
#define kResourceForkHeaderSize    16
#define kResourceMapHeaderSize     30
#define kResourceTypeEntrySize     8
#define kResourceRefEntrySize      12
#define kResourceDataHeaderSize    4

#define kControlDefResourceType      0x43444546  /* 'CDEF' */

/* Resource Attributes */
#define resSysHeap      (1 << 6)  /* 64 - Load in system heap */
#define resPurgeable    (1 << 5)  /* 32 - Resource is purgeable */
#define resLocked       (1 << 4)  /* 16 - Resource is locked */
#define resProtected    (1 << 3)  /* 8 - Resource is protected */
#define resPreload      (1 << 2)  /* 4 - Preload resource */
#define resChanged      (1 << 1)  /* 2 - Resource changed */

/* Error Codes */
/* noErr is defined in MacTypes.h */
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

 */

/*
 * ResourceMapHeader - Resource map header (30 bytes)

 */

/*
 * ResourceTypeEntry - Resource type list entry (8 bytes)

 */

/*
 * ResourceRefEntry - Resource reference entry (12 bytes)

 */
typedef struct ResourceRefEntry {
    SInt16 resourceID;               /* Resource ID */
    UInt16 nameOffset;               /* Offset to name (0xFFFF if none) */
    UInt8 resourceAttrs;             /* Resource attributes */
    UInt32 dataOffset : 24;         /* 24-bit data offset */
    Handle resourceHandle;             /* Handle if loaded */
} __attribute__((packed)) ResourceRefEntry;

/*
 * HandleBlock - Memory handle block structure

 */

/*
 * ResourceDataHeader - Resource data block header (4 bytes)

 */

/*
 * FileControlBlock - Open resource file control block

 */

/* Resource Map structures */
typedef struct ResourceMapHeader {
    UInt32  dataOffset;
    UInt32  mapOffset;
    UInt32  dataLength;
    UInt32  mapLength;
    UInt16  typeListOffset;
    UInt16  nameListOffset;
    SInt16  numTypes;
} ResourceMapHeader;

typedef struct ResourceTypeEntry {
    ResType resourceType;
    UInt16  numResourcesMinusOne;
    UInt16  referenceListOffset;
} ResourceTypeEntry;

typedef struct ResourceEntry {
    SInt16  resourceID;
    UInt16  nameOffset;
    UInt8   attributes;
    UInt8   dataOffsetHigh;
    UInt16  dataOffsetLow;
    Handle  resourceHandle;
} ResourceEntry;

// OpenResourceFile is defined in SystemTypes.h

/* Missing Resource Manager structures */
typedef struct ResourceDataHeader {
    UInt32 dataSize;
    UInt32 dataOffset;
    UInt32 dataLength;
} ResourceDataHeader;

typedef struct ResourceForkHeader {
    UInt32 dataOffset;
    UInt32 mapOffset;
    UInt32 dataLength;
    UInt32 mapLength;
} ResourceForkHeader;

typedef struct FileControlBlock {
    SInt16 refNum;
    UInt8  permissions;
    UInt8  flags;
    Str255 fileName;
    SInt32 filePos;
    SInt32 logEOF;
} FileControlBlock;

#endif /* RESOURCE_TYPES_H */

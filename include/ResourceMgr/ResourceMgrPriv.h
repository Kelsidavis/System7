/*
 * ResourceMgrPriv.h - Internal Resource Manager structures
 * Based on Inside Macintosh documentation
 * Clean-room implementation
 */

#ifndef RESOURCE_MGR_PRIV_H
#define RESOURCE_MGR_PRIV_H

#include "SystemTypes.h"

/* Maximum number of open resource files */
#define MAX_RES_FILES 16

/* Resource fork header (at offset 0) */
typedef struct ResourceHeader {
    UInt32 dataOffset;      /* Offset to resource data */
    UInt32 mapOffset;       /* Offset to resource map */
    UInt32 dataLength;      /* Length of resource data */
    UInt32 mapLength;       /* Length of resource map */
} ResourceHeader;

/* Resource map header (at mapOffset) */
typedef struct ResMapHeader {
    /* Copy of resource header */
    UInt32 dataOffset;      /* Copy from header */
    UInt32 mapOffset;       /* Copy from header */
    UInt32 dataLength;      /* Copy from header */
    UInt32 mapLength;       /* Copy from header */

    /* Map-specific fields */
    UInt32 nextMap;         /* Handle to next resource map (always 0 for us) */
    UInt16 fileRef;         /* File reference number */
    UInt16 attributes;      /* Resource file attributes */
    UInt16 typeListOffset;  /* Offset from map start to type list */
    UInt16 nameListOffset;  /* Offset from map start to name list */
} ResMapHeader;

/* Type list entry */
typedef struct TypeListEntry {
    ResType resType;        /* 4-character resource type */
    UInt16  count;          /* Number of resources of this type minus 1 */
    UInt16  refListOffset;  /* Offset from type list start to reference list */
} TypeListEntry;

/* Reference list entry (one per resource) */
typedef struct RefListEntry {
    ResID   resID;          /* Resource ID */
    UInt16  nameOffset;     /* Offset into name list, or 0xFFFF if no name */
    UInt8   attributes;     /* Resource attributes */
    UInt8   dataOffsetHi;   /* High byte of data offset (24-bit total) */
    UInt16  dataOffsetLo;   /* Low word of data offset */
    UInt32  reserved;       /* Reserved for handle (runtime only) */
} RefListEntry;

/* Resource data entry header */
typedef struct ResourceDataEntry {
    UInt32 length;          /* Length of resource data */
    /* Followed by actual resource data */
} ResourceDataEntry;

/* Resource file control block */
typedef struct ResFile {
    Boolean     inUse;          /* Slot is in use */
    SInt16      refNum;         /* File reference number */
    UInt8*      data;           /* Memory-mapped resource fork or buffer */
    UInt32      dataSize;       /* Size of resource fork */
    ResMapHeader* map;          /* Pointer to resource map in memory */
    Handle      mapHandle;      /* Handle to map if loaded separately */
    Str255      fileName;       /* File name for debugging */
} ResFile;

/* Global resource manager state */
typedef struct ResourceMgrGlobals {
    SInt16      curResFile;         /* Current resource file */
    OSErr       resError;           /* Last error */
    Boolean     resLoad;            /* Auto-load resources */
    ResFile     resFiles[MAX_RES_FILES];  /* Open resource files */
    Handle      systemResources;    /* Handle to system resource list */
    UInt32      nextUniqueID;      /* For UniqueID generation */
} ResourceMgrGlobals;

/* Internal functions */
void ResMap_Init(ResFile* file);
TypeListEntry* ResMap_FindType(ResFile* file, ResType type);
RefListEntry* ResMap_FindResource(ResFile* file, ResType type, ResID id);
RefListEntry* ResMap_FindNamedResource(ResFile* file, ResType type, ConstStr255Param name);
OSErr ResFile_Open(const char* path, SInt16* refNum);
void ResFile_Close(SInt16 refNum);
OSErr ResFile_ReadAt(ResFile* file, UInt32 offset, void* dst, UInt32 size);
Handle ResFile_LoadResource(ResFile* file, RefListEntry* ref);

/* Byte swapping for big-endian data */
UInt16 read_be16(const UInt8* p);
UInt32 read_be32(const UInt8* p);
void write_be16(UInt8* p, UInt16 val);
void write_be32(UInt8* p, UInt32 val);

#endif /* RESOURCE_MGR_PRIV_H */
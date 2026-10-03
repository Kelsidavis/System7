/*
 * FileManagerTypes.h - Extended types for File Manager implementation
 *
 * This header extends the basic VCB and FCB types from SystemTypes.h
 * with additional fields needed for the File Manager implementation.
 */

#ifndef __FILEMANAGERTYPES_H__
#define __FILEMANAGERTYPES_H__

#include "SystemTypes.h"
#include "Errors/ErrorCodes.h"
#include <time.h>

/* Extended Volume Control Block for File Manager */
typedef struct VCBExt {
    VCB             base;           /* Base VCB from SystemTypes.h */
    struct VCBExt*  vcbNext;        /* Next VCB in queue */
    void*           vcbCatalogBTCB; /* Catalog B-tree control block */
    void*           vcbExtentsBTCB; /* Extents B-tree control block */
    void*           vcbCTRef;       /* Catalog file reference (BTCB*) */
    void*           vcbXTRef;       /* Extents file reference (BTCB*) */
    UInt32          vcbFilCnt;      /* File count */
    UInt32          vcbDirCnt;      /* Directory count */
    UInt16          vcbDevice;      /* Device number */
} VCBExt;

/* Extended File Control Block for File Manager */
typedef struct FCBExt {
    FCB             base;           /* Base FCB from SystemTypes.h */
    FileRefNum      fcbRefNum;      /* File reference number */
    UInt32          fcbProcessID;   /* Process ID */
    void*           fcbVFSFile;     /* Open VFS file behind this FCB, or NULL */
    UInt32          fcbPLen;        /* Physical length */
} FCBExt;

/* Working Directory Control Block */
typedef struct WDCB {
    WDRefNum        wdRefNum;       /* Working directory reference number */
    VCBExt*         wdVCBPtr;       /* Volume control block pointer */
    UInt32          wdDirID;        /* Directory ID */
    UInt32          wdProcID;       /* Process ID */
    UInt16          wdIndex;        /* WDCB index */
} WDCB;

/* B-tree Control Block */
typedef struct BTCB {
    VCBExt*         btcbVCBPtr;     /* Volume control block pointer */
    UInt32          btcbFileID;     /* File ID */
    UInt32          btcbNodeSize;   /* Node size */
    UInt32          btcbMaxDepth;   /* Maximum depth */
    int (*btcKeyCmp)(const void*, const void*); /* Key comparison function */
} BTCB;

/* Cache Buffer */
typedef struct CacheBuffer {
    VCBExt*         cbVCBPtr;       /* Volume control block pointer */
    UInt32          cbBlockNum;     /* Block number */
    UInt8*          cbData;         /* Data buffer */
    UInt8           cbFlags;        /* Cache flags */
} CacheBuffer;

/* File System Globals */
typedef struct FSGlobals {
    Boolean         initialized;    /* File system initialized */
    VCBExt*         vcbQueue;       /* Head of VCB queue */
    FCBExt*         fcbArray;       /* Array of FCBs */
    UInt16          fcbCount;       /* Number of FCBs */
    UInt16          fcbFree;        /* First free FCB */
    WDCB*           wdcbArray;      /* Array of WDCBs */
    UInt16          wdcbCount;      /* Number of WDCBs */
    UInt16          wdcbFree;       /* First free WDCB */
    VolumeRefNum    defVRefNum;     /* Default volume reference number */
    UInt64          bytesRead;      /* Statistics */
    UInt64          bytesWritten;
} FSGlobals;

/* Platform Hooks */
typedef OSErr (*DeviceEjectProc)(UInt16 device);
typedef OSErr (*DeviceReadProc)(UInt16 device, UInt64 offset, UInt32 count, void* buffer);
typedef OSErr (*DeviceWriteProc)(UInt16 device, UInt64 offset, UInt32 count, const void* buffer);

typedef struct PlatformHooks {
    DeviceEjectProc DeviceEject;
    DeviceReadProc  DeviceRead;
    DeviceWriteProc DeviceWrite;
} PlatformHooks;

/* Additional types */
typedef UInt32 CNodeID;
typedef struct {
    UInt16 xdrStABN;    /* Start allocation block number */
    UInt16 xdrNumABlks; /* Number of allocation blocks */
} ExtDescriptor;

typedef ExtDescriptor ExtDataRec[3];  /* Array of 3 extent descriptors */

/* Extent overflow B-tree key */
typedef struct ExtentKey {
    UInt8  xkrKeyLen;   /* Key length */
    UInt8  xkrFkType;   /* Fork type (0x00 = data, 0xFF = resource) */
    UInt32 xkrFNum;     /* File number (catalog node ID) */
    UInt16 xkrFABN;     /* Starting file allocation block number */
} ExtentKey;

/* Fork types for extent keys */
#define kDataFork       0x00
#define kResourceFork   0xFF

/* Catalog types */
typedef struct {
    UInt8 cdrType;      /* Record type */
    UInt32 dirID;       /* Directory ID */
    UInt32 dirVal;      /* Directory valence (number of items) */
} CatDirRec;

typedef struct {
    UInt8 cdrType;      /* Record type */
    UInt32 fileID;      /* File ID */
} CatFileRec;

/* Extended catalog types for FileManager.c */
typedef struct {
    UInt8 cdrType;      /* Record type */
    UInt32 dirDirID;    /* Directory ID */
    UInt32 dirVal;      /* Directory valence */
    UInt32 dirCrDat;    /* Creation date */
    UInt32 dirMdDat;    /* Modification date */
} CatalogDirRec;

typedef struct {
    UInt8 cdrType;      /* Record type */
    UInt32 filFlNum;    /* File ID */
    UInt32 filCrDat;    /* Creation date */
    UInt32 filMdDat;    /* Modification date */
} CatalogFileRec;

/* Additional error codes */
#ifndef wrPermErr
#define wrPermErr       -61    /* Write permission error */
#define fsRdWrPerm      3      /* Read/write permission */
/* Note: fsAtMark, fsFromStart, fsFromLEOF defined in MacTypes.h */
#define fsFromMark      3      /* From current mark */
#define kioFlAttribDir  0x10   /* Directory attribute */
#define notAFileErr     -1302  /* Not a file error */
#define kioVAtrbOffline 0x0001 /* Volume offline */
#endif

/* Unix error codes */
#ifndef ENOMEM
#define ENOMEM  12
#define EIO     5
#define EMFILE  24
#define ENOENT  2
#define EACCES  13
#define EEXIST  17
#define ENOTDIR 20
#define EISDIR  21
#define ENOSPC  28
#define EROFS   30
#endif

#endif /* __FILEMANAGERTYPES_H__ */

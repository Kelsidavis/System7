/*
 * FileManagerTypes.h - Extended types for File Manager implementation
 *
 * This header extends the basic VCB and FCB types from SystemTypes.h
 * with additional fields needed for the File Manager implementation.
 */

#ifndef __FILEMANAGERTYPES_H__
#define __FILEMANAGERTYPES_H__

#include "SystemTypes.h"

/* pthread inline functions for single-threaded kernel */
static inline int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr) {
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return 0;
}

/* time_t type */
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef long time_t;
#endif

/* Extended Volume Control Block for File Manager */
typedef struct VCBExt {
    VCB             base;           /* Base VCB from SystemTypes.h */
    struct VCBExt*  vcbNext;        /* Next VCB in queue */
    pthread_mutex_t vcbMutex;       /* Volume mutex */
    void*           vcbCatalogBTCB; /* Catalog B-tree control block */
    void*           vcbExtentsBTCB; /* Extents B-tree control block */
    void*           vcbXTRef;       /* Extents overflow B-tree reference */
    UInt32          vcbFilCnt;      /* File count */
    UInt32          vcbDirCnt;      /* Directory count */
    UInt16          vcbDevice;      /* Device number */
    void*           vcbVBMCache;    /* Volume bitmap cache */
} VCBExt;

/* Extended File Control Block for File Manager */
typedef struct FCBExt {
    FCB             base;           /* Base FCB from SystemTypes.h */
    FileRefNum      fcbRefNum;      /* File reference number */
    UInt32          fcbProcessID;   /* Process ID */
    pthread_mutex_t fcbMutex;       /* FCB mutex */
    UInt32          fcbCrPs;        /* Current position */
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
} BTCB;

/* Cache Buffer */
typedef struct CacheBuffer {
    VCBExt*         cbVCBPtr;       /* Volume control block pointer */
    VCBExt*         cbVCB;          /* Alias for cbVCBPtr for compatibility */
    UInt32          cbBlockNum;     /* Block number */
    UInt32          cbBlkNum;       /* Alias for cbBlockNum for compatibility */
    UInt8*          cbData;         /* Data buffer */
    UInt8           cbFlags;        /* Cache flags */
    UInt32          cbRefCnt;       /* Reference count */
    UInt32          cbLastUse;      /* Last use timestamp */
    struct CacheBuffer* cbNext;     /* Next in hash chain */
    struct CacheBuffer* cbPrev;     /* Previous in hash chain */
    struct CacheBuffer* cbFreeNext; /* Next in free list */
    struct CacheBuffer* cbFreePrev; /* Previous in free list */
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
    pthread_mutex_t globalMutex;    /* Global mutex */
    UInt64          bytesRead;      /* Statistics */
    UInt64          bytesWritten;
    /* Cache management */
    struct CacheBuffer* cacheBuffers;  /* Array of cache buffers */
    UInt32          cacheSize;      /* Number of cache buffers */
    struct CacheBuffer** cacheHash; /* Hash table for cache lookup */
    UInt32          cacheHashSize;  /* Size of hash table */
    struct CacheBuffer* cacheFreeList; /* Free list of cache buffers */
    UInt64          cacheHits;      /* Cache hit statistics */
    UInt64          cacheMisses;    /* Cache miss statistics */
} FSGlobals;

/* Platform Hooks */
typedef OSErr (*DeviceEjectProc)(UInt16 device);
typedef OSErr (*DeviceReadProc)(UInt16 device, uint64_t offset, UInt32 bytes, void* buffer);
typedef OSErr (*DeviceWriteProc)(UInt16 device, uint64_t offset, UInt32 bytes, const void* buffer);

typedef struct PlatformHooks {
    DeviceEjectProc DeviceEject;
    DeviceReadProc DeviceRead;
    DeviceWriteProc DeviceWrite;
} PlatformHooks;

/* Additional types */
typedef UInt32 CNodeID;
typedef ExtentDescriptor ExtDataRec[3];  /* Array of 3 extent descriptors */

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
#define kioVAtrbSoftwareLock 0x8000 /* Volume software-locked */
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

/* Additional Mac error codes */
#ifndef ioErr
#define ioErr           -36    /* I/O error */
#define vLckdErr        -46    /* Volume is locked */
#define fBsyErr         -47    /* File is busy */
#define opWrErr         -49    /* File already open for writing */
#define volOffLinErr    -53    /* Volume is offline */
#define permErr         -54    /* Permission error */
#define nsvErr          -35    /* No such volume */
#define fnOpnErr        -38    /* File not open */
#define eofErr          -39    /* End of file */
#define posErr          -40    /* Bad positioning */
#define mFulErr         -41    /* Memory full */
#define tmfoErr         -42    /* Too many files open */
#define wPrErr          -44    /* Disk is write-protected */
#define fLckdErr        -45    /* File is locked */
#define dskFulErr       -34    /* Disk full */
#define dirNFErr        -120   /* Directory not found */
#define tmwdoErr        -121   /* Too many working directories open */
#endif

#endif /* __FILEMANAGERTYPES_H__ */
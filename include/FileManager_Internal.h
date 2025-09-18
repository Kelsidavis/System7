/*
 * FileManager_Internal.h - Internal structures for File Manager implementation
 *
 * This header defines private structures and functions used internally by
 * the File Manager implementation. Based on System 7.1 HFS architecture.
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 */

#ifndef __FILEMANAGER_INTERNAL_H__
#define __FILEMANAGER_INTERNAL_H__

#include "FileManager.h"
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HFS Constants */
#define HFS_SIGNATURE       0x4244      /* 'BD' - HFS signature */
#define MFS_SIGNATURE       0xD2D7      /* MFS signature */
#define HFS_PLUS_SIGNATURE  0x482B      /* 'H+' - HFS Plus signature */

#define BLOCK_SIZE          512         /* Standard block size */
#define MDB_BLOCK           2           /* Master Directory Block location */
#define CATALOG_FILE_ID     4           /* Catalog file ID */
#define EXTENTS_FILE_ID     3           /* Extents file ID */
#define BITMAP_FILE_ID      2           /* Volume bitmap file ID */

#define MAX_FILENAME        31          /* Max HFS filename length */
#define MAX_VOLUMES         32          /* Max mounted volumes */
#define MAX_FCBS            348         /* Max open files (from System 7.1) */
#define MAX_WDCBS           40          /* Max working directories */

#define BTREE_NODE_SIZE     512         /* B-tree node size */
#define BTREE_MAX_DEPTH     8           /* Max B-tree depth */
#define BTREE_MAX_KEY_LEN   37          /* Max catalog key length */

/* File fork types */
#define FORK_DATA           0x00
#define FORK_RSRC           0xFF

/* B-tree node types */
#define NODE_INDEX          0
#define NODE_HEADER         1
#define NODE_MAP            2
#define NODE_LEAF           0xFF

/* Catalog record types */
#define REC_FLDR            1           /* Folder record */
#define REC_FIL             2           /* File record */
#define REC_FLDR_THREAD     3           /* Folder thread record */
#define REC_FIL_THREAD      4           /* File thread record */

/* Allocation strategies */
#define ALLOC_FIRST_FIT     0
#define ALLOC_BEST_FIT      1
#define ALLOC_CONTIG        2

/* Cache flags */
#define CACHE_DIRTY         0x80
#define CACHE_LOCKED        0x40
#define CACHE_IN_USE        0x20

/* FCB flags */
#define FCB_RESOURCE        0x01        /* Resource fork */
#define FCB_WRITE_PERM      0x02        /* Write permission */
#define FCB_DIRTY           0x04        /* Has unsaved changes */
#define FCB_SHARED_WRITE    0x08        /* Shared write access */
#define FCB_FILE_LOCKED     0x10        /* File locked */
#define FCB_OWN_CLUMP       0x20        /* Has its own clump size */

/* VCB flags */
#define VCB_DIRTY           0x8000      /* Volume has unsaved changes */
#define VCB_WRITE_PROTECTED 0x0080      /* Volume is write-protected */
#define VCB_UNMOUNTING      0x0040      /* Volume is being unmounted */
#define VCB_BAD_BLOCKS      0x0200      /* Volume has bad blocks */

/* ============================================================================
 * HFS On-Disk Structures
 * ============================================================================ */

/* Master Directory Block (Volume Header) */
typedef struct MasterDirectoryBlock {
    uint16_t    drSigWord;          /* Volume signature (0x4244 for HFS) */
    uint32_t    drCrDate;           /* Date/time volume created */
    uint32_t    drLsMod;            /* Date/time volume last modified */
    uint16_t    drAtrb;             /* Volume attributes */
    uint16_t    drNmFls;            /* Number of files in root directory */
    uint16_t    drVBMSt;            /* First block of volume bitmap */
    uint16_t    drAllocPtr;         /* Start of next allocation search */
    uint16_t    drNmAlBlks;         /* Number of allocation blocks */
    uint32_t    drAlBlkSiz;         /* Size of allocation block (bytes) */
    uint32_t    drClpSiz;           /* Default clump size */
    uint16_t    drAlBlSt;           /* First allocation block */
    uint32_t    drNxtCNID;          /* Next unused catalog node ID */
    uint16_t    drFreeBks;          /* Number of unused allocation blocks */
    uint8_t     drVN[28];           /* Volume name (Pascal string) */
    uint32_t    drVolBkUp;          /* Date/time of last backup */
    uint16_t    drVSeqNum;          /* Volume backup sequence number */
    uint32_t    drWrCnt;            /* Volume write count */
    uint32_t    drXTClpSiz;         /* Clump size for extents B-tree */
    uint32_t    drCTClpSiz;         /* Clump size for catalog B-tree */
    uint16_t    drNmRtDirs;         /* Number of directories in root */
    uint32_t    drFilCnt;           /* Number of files on volume */
    uint32_t    drDirCnt;           /* Number of directories on volume */
    uint8_t     drFndrInfo[32];     /* Finder information */
    uint16_t    drVCSize;           /* Size of volume cache */
    uint16_t    drVBMCSize;         /* Size of volume bitmap cache */
    uint16_t    drCtlCSize;         /* Size of catalog cache */
    uint32_t    drXTFlSize;         /* Size of extents file */
    ExtDataRec  drXTExtRec;         /* Extents B-tree's extent record */
    uint32_t    drCTFlSize;         /* Size of catalog file */
    ExtDataRec  drCTExtRec;         /* Catalog B-tree's extent record */
} MasterDirectoryBlock;

/* B-tree Node Descriptor */
typedef struct BTNodeDescriptor {
    uint32_t    ndFLink;            /* Forward link to next node */
    uint32_t    ndBLink;            /* Backward link to previous node */
    uint8_t     ndType;             /* Node type */
    int8_t      ndNHeight;          /* Node height (leaf = 1) */
    uint16_t    ndNRecs;            /* Number of records in node */
    uint16_t    ndResv2;            /* Reserved */
} BTNodeDescriptor;

/* B-tree Header Record */
typedef struct BTHeaderRec {
    uint16_t    bthDepth;           /* Current depth of B-tree */
    uint32_t    bthRoot;            /* Node number of root node */
    uint32_t    bthNRecs;           /* Number of leaf records */
    uint32_t    bthFNode;           /* Node number of first leaf */
    uint32_t    bthLNode;           /* Node number of last leaf */
    uint16_t    bthNodeSize;        /* Size of a node */
    uint16_t    bthKeyLen;          /* Maximum key length */
    uint32_t    bthNNodes;          /* Total number of nodes */
    uint32_t    bthFree;            /* Number of free nodes */
    uint8_t     bthResv[76];        /* Reserved */
} BTHeaderRec;

/* Catalog Key */
typedef struct CatalogKey {
    uint8_t     ckrKeyLen;          /* Key length */
    uint8_t     ckrResrv1;          /* Reserved */
    uint32_t    ckrParID;           /* Parent directory ID */
    uint8_t     ckrCName[32];       /* Catalog node name (Pascal string) */
} CatalogKey;

/* Catalog Data Record Type */
typedef struct CatalogDataRec {
    uint8_t     cdrType;            /* Record type */
    uint8_t     cdrResrv;           /* Reserved */
} CatalogDataRec;

/* Catalog Directory Record */
typedef struct CatalogDirRec {
    uint8_t     cdrType;            /* Record type = REC_FLDR */
    uint8_t     cdrResrv;           /* Reserved */
    uint16_t    dirFlags;           /* Directory flags */
    uint16_t    dirVal;             /* Directory valence (number of items) */
    uint32_t    dirDirID;           /* Directory ID */
    uint32_t    dirCrDat;           /* Creation date */
    uint32_t    dirMdDat;           /* Modification date */
    uint32_t    dirBkDat;           /* Backup date */
    FInfo       dirUsrInfo;         /* Finder info */
    FXInfo      dirFndrInfo;        /* Extended Finder info */
    uint32_t    dirResrv[4];        /* Reserved */
} CatalogDirRec;

/* Catalog File Record */
typedef struct CatalogFileRec {
    uint8_t     cdrType;            /* Record type = REC_FIL */
    uint8_t     cdrResrv;           /* Reserved */
    uint8_t     filFlags;           /* File flags */
    uint8_t     filTyp;             /* File type */
    FInfo       filUsrWds;          /* Finder info */
    uint32_t    filFlNum;           /* File ID */
    uint16_t    filStBlk;           /* First allocation block of data fork */
    uint32_t    filLgLen;           /* Logical length of data fork */
    uint32_t    filPyLen;           /* Physical length of data fork */
    uint16_t    filRStBlk;          /* First allocation block of resource fork */
    uint32_t    filRLgLen;          /* Logical length of resource fork */
    uint32_t    filRPyLen;          /* Physical length of resource fork */
    uint32_t    filCrDat;           /* Creation date */
    uint32_t    filMdDat;           /* Modification date */
    uint32_t    filBkDat;           /* Backup date */
    FXInfo      filFndrInfo;        /* Extended Finder info */
    uint16_t    filClpSize;         /* File clump size */
    ExtDataRec  filExtRec;          /* First data fork extent record */
    ExtDataRec  filRExtRec;         /* First resource fork extent record */
    uint32_t    filResrv;           /* Reserved */
} CatalogFileRec;

/* Catalog Thread Record */
typedef struct CatalogThreadRec {
    uint8_t     cdrType;            /* Record type = REC_*_THREAD */
    uint8_t     cdrResrv;           /* Reserved */
    uint8_t     thdResrv[8];        /* Reserved */
    uint32_t    thdParID;           /* Parent ID */
    uint8_t     thdCName[32];       /* Component name (Pascal string) */
} CatalogThreadRec;

/* Extent Key */
typedef struct ExtentKey {
    uint8_t     xkrKeyLen;          /* Key length */
    uint8_t     xkrFkType;          /* Fork type (0=data, 0xFF=resource) */
    uint32_t    xkrFNum;            /* File number */
    uint16_t    xkrFABN;            /* Starting file allocation block */
} ExtentKey;

/* ============================================================================
 * In-Memory Management Structures
 * ============================================================================ */

/* Volume Control Block */
struct VCB {
    struct VCB*     vcbNext;            /* Next VCB in queue */
    uint16_t        vcbFlags;           /* Volume flags */
    uint16_t        vcbSigWord;         /* Volume signature */
    uint32_t        vcbCrDate;          /* Creation date */
    uint32_t        vcbLsMod;           /* Last modification date */
    uint16_t        vcbAtrb;            /* Volume attributes */
    uint16_t        vcbNmFls;           /* Files in root */
    uint16_t        vcbVBMSt;           /* Start of volume bitmap */
    uint16_t        vcbAllocPtr;        /* Next allocation search start */
    uint16_t        vcbNmAlBlks;        /* Number of allocation blocks */
    uint32_t        vcbAlBlkSiz;        /* Allocation block size */
    uint32_t        vcbClpSiz;          /* Default clump size */
    uint16_t        vcbAlBlSt;          /* First allocation block */
    uint32_t        vcbNxtCNID;         /* Next catalog node ID */
    uint16_t        vcbFreeBks;         /* Free allocation blocks */
    uint8_t         vcbVN[28];          /* Volume name */
    int16_t         vcbDrvNum;          /* Drive number */
    int16_t         vcbDRefNum;         /* Driver reference number */
    int16_t         vcbFSID;            /* File system ID */
    VolumeRefNum    vcbVRefNum;         /* Volume reference number */
    void*           vcbMAdr;            /* Pointer to MDB */
    void*           vcbBufAdr;          /* Pointer to volume buffer */
    uint32_t        vcbMLen;            /* MDB length */

    /* Extended fields for HFS */
    uint32_t        vcbVolBkUp;         /* Backup date */
    uint16_t        vcbVSeqNum;         /* Backup sequence */
    uint32_t        vcbWrCnt;           /* Write count */
    uint32_t        vcbFilCnt;          /* File count */
    uint32_t        vcbDirCnt;          /* Directory count */
    uint8_t         vcbFndrInfo[32];    /* Finder info */

    /* B-tree control blocks */
    void*           vcbXTRef;           /* Extents B-tree reference */
    void*           vcbCTRef;           /* Catalog B-tree reference */

    /* Caches */
    void*           vcbVBMCache;        /* Volume bitmap cache */
    void*           vcbCtlCache;        /* Catalog cache */

    /* Platform-specific device handle */
    void*           vcbDevice;          /* Platform device handle */

    /* Thread safety */
    FM_Mutex        vcbMutex;           /* Volume mutex */
};

/* File Control Block */
struct FCB {
    uint32_t        fcbFlNm;            /* File number */
    uint16_t        fcbFlags;           /* File flags */
    uint16_t        fcbTypByt;          /* File type */
    uint16_t        fcbSBlk;            /* First allocation block */
    uint32_t        fcbEOF;             /* Logical end-of-file */
    uint32_t        fcbPLen;            /* Physical length */
    uint32_t        fcbCrPs;            /* Current position */
    struct VCB*     fcbVPtr;            /* Pointer to VCB */
    void*           fcbBfAdr;           /* File buffer address */
    uint16_t        fcbFlPos;           /* Flags and fork type */
    uint32_t        fcbClmpSize;        /* File clump size */
    void*           fcbBTCBPtr;         /* B-tree control block pointer */
    ExtDataRec      fcbExtRec;          /* First extent record */
    uint32_t        fcbFType;           /* File type (for Finder) */
    uint32_t        fcbCatPos;          /* Catalog hint */
    uint32_t        fcbDirID;           /* Parent directory ID */
    uint8_t         fcbCName[32];       /* File name */

    /* Extended fields */
    FileRefNum      fcbRefNum;          /* File reference number */
    uint8_t         fcbOpenCnt;         /* Open count */
    uint32_t        fcbProcessID;       /* Owning process ID */
    uint32_t        fcbLastAccess;      /* Last access time */

    /* Thread safety */
    FM_Mutex        fcbMutex;           /* FCB mutex */
};

/* Working Directory Control Block */
struct WDCB {
    struct VCB*     wdVCBPtr;           /* Pointer to VCB */
    uint32_t        wdDirID;            /* Directory ID */
    uint32_t        wdCatHint;          /* Catalog hint */
    uint32_t        wdProcID;           /* Process ID */
    WDRefNum        wdRefNum;           /* Working directory reference */

    /* Extended fields */
    uint16_t        wdIndex;            /* WDCB index */
    uint8_t         wdVol[28];          /* Volume name */
    uint8_t         wdName[32];         /* Directory name */
};

/* B-tree Control Block */
typedef struct BTCB {
    uint8_t         btcFlags;           /* Flags */
    uint8_t         btcResv;            /* Reserved */
    FileRefNum      btcRefNum;          /* File reference number */
    void*           btcKeyCmp;          /* Key compare routine */
    void*           btcCQPtr;           /* Cache queue pointer */
    void*           btcVarPtr;          /* Variables pointer */
    uint16_t        btcLevel;           /* Current level */
    uint32_t        btcNodeM;           /* Current node mark */
    uint16_t        btcIndexM;          /* Current index mark */

    /* Header record (cached) */
    uint16_t        btcDepth;           /* Tree depth */
    uint32_t        btcRoot;            /* Root node */
    uint32_t        btcNRecs;           /* Number of records */
    uint32_t        btcFNode;           /* First node */
    uint32_t        btcLNode;           /* Last node */
    uint16_t        btcNodeSize;        /* Node size */
    uint16_t        btcKeyLen;          /* Max key length */
    uint32_t        btcNNodes;          /* Total nodes */
    uint32_t        btcFree;            /* Free nodes */

    /* Extended fields */
    void*           btcCache;           /* Node cache */
    uint32_t        btcHint;            /* Search hint */
    FM_Mutex        btcMutex;           /* B-tree mutex */
} BTCB;

/* Cache Buffer */
typedef struct CacheBuffer {
    struct CacheBuffer* cbNext;         /* Next buffer in hash chain */
    struct CacheBuffer* cbPrev;         /* Previous buffer in hash chain */
    struct CacheBuffer* cbFreeNext;     /* Next in free list */
    struct CacheBuffer* cbFreePrev;     /* Previous in free list */
    uint16_t        cbFlags;            /* Buffer flags */
    uint16_t        cbRefCnt;           /* Reference count */
    uint32_t        cbBlkNum;           /* Block number */
    struct VCB*     cbVCB;              /* Volume control block */
    uint32_t        cbLastUse;          /* Last use timestamp */
    uint8_t         cbData[BLOCK_SIZE]; /* Buffer data */
} CacheBuffer;

/* File System Global State */
typedef struct FSGlobals {
    /* Volume management */
    struct VCB*     vcbQueue;           /* VCB queue head */
    uint16_t        vcbCount;           /* Number of mounted volumes */
    VolumeRefNum    defVRefNum;         /* Default volume */

    /* File management */
    struct FCB*     fcbArray;           /* FCB array */
    uint16_t        fcbCount;           /* Number of FCBs */
    uint16_t        fcbFree;            /* First free FCB */

    /* Working directory management */
    struct WDCB*    wdcbArray;          /* WDCB array */
    uint16_t        wdcbCount;          /* Number of WDCBs */
    uint16_t        wdcbFree;           /* First free WDCB */

    /* Cache management */
    CacheBuffer*    cacheBuffers;       /* Cache buffer array */
    uint32_t        cacheSize;          /* Cache size in blocks */
    CacheBuffer*    cacheFreeList;      /* Free buffer list */
    CacheBuffer**   cacheHash;          /* Hash table */
    uint32_t        cacheHashSize;      /* Hash table size */

    /* Process management */
    uint32_t        currentProcess;     /* Current process ID */
    void*           processTable;       /* Process file table */

    /* Statistics */
    uint64_t        bytesRead;          /* Total bytes read */
    uint64_t        bytesWritten;       /* Total bytes written */
    uint32_t        cacheHits;          /* Cache hits */
    uint32_t        cacheMisses;        /* Cache misses */

    /* Global synchronization */
    FM_Mutex        globalMutex;        /* Global file system mutex */
    bool            initialized;        /* Initialization flag */

} FSGlobals;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

/* Volume Management */
VCB* VCB_Alloc(void);
void VCB_Free(VCB* vcb);
VCB* VCB_Find(VolumeRefNum vRefNum);
VCB* VCB_FindByName(const uint8_t* name);
OSErr VCB_Mount(uint16_t drvNum, VCB** newVCB);
OSErr VCB_Unmount(VCB* vcb);
OSErr VCB_Flush(VCB* vcb);
OSErr VCB_Update(VCB* vcb);

/* File Control Block Management */
FCB* FCB_Alloc(void);
void FCB_Free(FCB* fcb);
FCB* FCB_Find(FileRefNum refNum);
FCB* FCB_FindByID(VCB* vcb, uint32_t fileID);
OSErr FCB_Open(VCB* vcb, uint32_t dirID, const uint8_t* name, uint8_t permission, FCB** newFCB);
OSErr FCB_Close(FCB* fcb);
OSErr FCB_Flush(FCB* fcb);

/* Working Directory Management */
WDCB* WDCB_Alloc(void);
void WDCB_Free(WDCB* wdcb);
WDCB* WDCB_Find(WDRefNum wdRefNum);
OSErr WDCB_Create(VCB* vcb, uint32_t dirID, uint32_t procID, WDCB** newWDCB);

/* B-tree Operations */
OSErr BTree_Open(VCB* vcb, uint32_t fileID, BTCB** btcb);
OSErr BTree_Close(BTCB* btcb);
OSErr BTree_Search(BTCB* btcb, const void* key, void* record, uint16_t* recordSize, uint32_t* hint);
OSErr BTree_Insert(BTCB* btcb, const void* key, const void* record, uint16_t recordSize);
OSErr BTree_Delete(BTCB* btcb, const void* key);
OSErr BTree_GetNode(BTCB* btcb, uint32_t nodeNum, void** nodePtr);
OSErr BTree_ReleaseNode(BTCB* btcb, uint32_t nodeNum);
OSErr BTree_FlushNode(BTCB* btcb, uint32_t nodeNum);

/* Catalog Operations */
OSErr Cat_Open(VCB* vcb);
OSErr Cat_Close(VCB* vcb);
OSErr Cat_Lookup(VCB* vcb, uint32_t dirID, const uint8_t* name, void* catData, uint32_t* hint);
OSErr Cat_Create(VCB* vcb, uint32_t dirID, const uint8_t* name, uint8_t type, void* catData);
OSErr Cat_Delete(VCB* vcb, uint32_t dirID, const uint8_t* name);
OSErr Cat_Rename(VCB* vcb, uint32_t dirID, const uint8_t* oldName, const uint8_t* newName);
OSErr Cat_Move(VCB* vcb, uint32_t srcDirID, const uint8_t* name, uint32_t dstDirID);
OSErr Cat_GetInfo(VCB* vcb, uint32_t dirID, const uint8_t* name, CInfoPBRec* pb);
OSErr Cat_SetInfo(VCB* vcb, uint32_t dirID, const uint8_t* name, const CInfoPBRec* pb);
CNodeID Cat_GetNextID(VCB* vcb);

/* Extent Management */
OSErr Ext_Open(VCB* vcb);
OSErr Ext_Close(VCB* vcb);
OSErr Ext_Allocate(VCB* vcb, uint32_t fileID, uint8_t forkType, uint32_t blocks, ExtDataRec extents);
OSErr Ext_Deallocate(VCB* vcb, uint32_t fileID, uint8_t forkType, uint32_t startBlock);
OSErr Ext_Map(VCB* vcb, FCB* fcb, uint32_t fileBlock, uint32_t* physBlock, uint32_t* contiguous);
OSErr Ext_Extend(VCB* vcb, FCB* fcb, uint32_t newSize);
OSErr Ext_Truncate(VCB* vcb, FCB* fcb, uint32_t newSize);

/* Allocation Bitmap Management */
OSErr Alloc_Init(VCB* vcb);
OSErr Alloc_Close(VCB* vcb);
OSErr Alloc_Blocks(VCB* vcb, uint32_t startHint, uint32_t minBlocks, uint32_t maxBlocks,
                   uint32_t* actualStart, uint32_t* actualCount);
OSErr Alloc_Free(VCB* vcb, uint32_t startBlock, uint32_t blockCount);
uint32_t Alloc_CountFree(VCB* vcb);
bool Alloc_Check(VCB* vcb, uint32_t startBlock, uint32_t blockCount);

/* Cache Management */
OSErr Cache_Init(uint32_t cacheSize);
void Cache_Shutdown(void);
OSErr Cache_GetBlock(VCB* vcb, uint32_t blockNum, CacheBuffer** buffer);
OSErr Cache_ReleaseBlock(CacheBuffer* buffer, bool dirty);
OSErr Cache_FlushVolume(VCB* vcb);
OSErr Cache_FlushAll(void);
void Cache_Invalidate(VCB* vcb);

/* I/O Operations */
OSErr IO_ReadBlocks(VCB* vcb, uint32_t startBlock, uint32_t blockCount, void* buffer);
OSErr IO_WriteBlocks(VCB* vcb, uint32_t startBlock, uint32_t blockCount, const void* buffer);
OSErr IO_ReadFork(FCB* fcb, uint32_t offset, uint32_t count, void* buffer, uint32_t* actual);
OSErr IO_WriteFork(FCB* fcb, uint32_t offset, uint32_t count, const void* buffer, uint32_t* actual);

/* Path and Name Utilities */
OSErr Path_Parse(const char* path, VolumeRefNum* vRefNum, DirID* dirID, uint8_t* name);
OSErr Path_Build(VCB* vcb, DirID dirID, const uint8_t* name, char* path, size_t maxLen);
bool Name_Equal(const uint8_t* name1, const uint8_t* name2);
void Name_Copy(uint8_t* dst, const uint8_t* src);
uint16_t Name_Hash(const uint8_t* name);

/* Date/Time Utilities */
uint32_t DateTime_Current(void);
uint32_t DateTime_FromUnix(time_t unixTime);
time_t DateTime_ToUnix(uint32_t macTime);

/* Thread Safety */
void FS_LockGlobal(void);
void FS_UnlockGlobal(void);
void FS_LockVolume(VCB* vcb);
void FS_UnlockVolume(VCB* vcb);
void FS_LockFCB(FCB* fcb);
void FS_UnlockFCB(FCB* fcb);

/* Error Handling */
OSErr Error_Map(int platformError);
const char* Error_String(OSErr err);

/* Debug Support */
#ifdef DEBUG
void Debug_DumpVCB(VCB* vcb);
void Debug_DumpFCB(FCB* fcb);
void Debug_DumpBTree(BTCB* btcb);
void Debug_CheckConsistency(void);
#else
#define Debug_DumpVCB(v)
#define Debug_DumpFCB(f)
#define Debug_DumpBTree(b)
#define Debug_CheckConsistency()
#endif

/* Global File System State */
extern FSGlobals g_FSGlobals;

/* Platform abstraction layer (HAL) hooks */
typedef struct PlatformHooks {
    OSErr (*DeviceOpen)(const char* device, void** handle);
    OSErr (*DeviceClose)(void* handle);
    OSErr (*DeviceRead)(void* handle, uint64_t offset, uint32_t count, void* buffer);
    OSErr (*DeviceWrite)(void* handle, uint64_t offset, uint32_t count, const void* buffer);
    OSErr (*DeviceFlush)(void* handle);
    OSErr (*DeviceGetSize)(void* handle, uint64_t* size);
    OSErr (*DeviceEject)(void* handle);
} PlatformHooks;

extern PlatformHooks g_PlatformHooks;

#ifdef __cplusplus
}
#endif

#endif /* __FILEMANAGER_INTERNAL_H__ */
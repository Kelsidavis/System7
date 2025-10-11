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

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
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

/* Mac error codes not in SystemTypes.h */
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
#define btNoErr         0      /* B-tree no error */
#define btRecNotFnd     -1300  /* B-tree record not found */
#define fxRangeErr      -1400  /* File position beyond mapped range */
#define extFSErr        -1401  /* External file system error */
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

#include "SystemTypes.h"
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

/* Include extended types for File Manager */
#include "FileManagerTypes.h"

/* For compatibility, map extended types to base names */
#define VCB VCBExt
#define FCB FCBExt

/* Now include FileManager.h which will see VCB/FCB as VCBExt/FCBExt */
#include "FileManager.h"

/* ============================================================================
 * HFS On-Disk Structures
 * ============================================================================ */

/* Master Directory Block (Volume Header) */

/* B-tree Node Descriptor */

/* B-tree Header Record */

/* Catalog Key */

/* Catalog Data Record Type */

/* Catalog Directory Record */

/* Catalog File Record */

/* Catalog Thread Record */

/* Extent Key */

/* ============================================================================
 * In-Memory Management Structures
 * ============================================================================ */

/* Volume Control Block */

/* File Control Block */

/* Working Directory Control Block */

/* B-tree Control Block */

/* Cache Buffer */

/* File System Global State */

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

/* Volume Management */
VCB* VCB_Alloc(void);
void VCB_Free(VCB* vcb);
VCB* VCB_Find(VolumeRefNum vRefNum);
VCB* VCB_FindByName(const UInt8* name);
OSErr VCB_Mount(UInt16 drvNum, VCB** newVCB);
OSErr VCB_Unmount(VCB* vcb);
OSErr VCB_Flush(VCB* vcb);
OSErr VCB_Update(VCB* vcb);

/* File Control Block Management */
FCB* FCB_Alloc(void);
void FCB_Free(FCB* fcb);
FCB* FCB_Find(FileRefNum refNum);
FCB* FCB_FindByID(VCB* vcb, UInt32 fileID);
OSErr FCB_Open(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 permission, FCB** newFCB);
OSErr FCB_Close(FCB* fcb);
OSErr FCB_Flush(FCB* fcb);

/* Working Directory Management */
WDCB* WDCB_Alloc(void);
void WDCB_Free(WDCB* wdcb);
WDCB* WDCB_Find(WDRefNum wdRefNum);
OSErr WDCB_Create(VCB* vcb, UInt32 dirID, UInt32 procID, WDCB** newWDCB);

/* B-tree Operations */
OSErr BTree_Open(VCB* vcb, UInt32 fileID, BTCB** btcb);
OSErr BTree_Close(BTCB* btcb);
OSErr BTree_Search(BTCB* btcb, const void* key, void* record, UInt16* recordSize, UInt32* hint);
OSErr BTree_Insert(BTCB* btcb, const void* key, const void* record, UInt16 recordSize);
OSErr BTree_Delete(BTCB* btcb, const void* key);
OSErr BTree_GetNode(BTCB* btcb, UInt32 nodeNum, void** nodePtr);
OSErr BTree_ReleaseNode(BTCB* btcb, UInt32 nodeNum);
OSErr BTree_FlushNode(BTCB* btcb, UInt32 nodeNum);

/* Catalog Operations */
OSErr Cat_Open(VCB* vcb);
OSErr Cat_Close(VCB* vcb);
OSErr Cat_Lookup(VCB* vcb, UInt32 dirID, const UInt8* name, void* catData, UInt32* hint);
OSErr Cat_Create(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 type, void* catData);
OSErr Cat_Delete(VCB* vcb, UInt32 dirID, const UInt8* name);
OSErr Cat_Rename(VCB* vcb, UInt32 dirID, const UInt8* oldName, const UInt8* newName);
OSErr Cat_Move(VCB* vcb, UInt32 srcDirID, const UInt8* name, UInt32 dstDirID);
OSErr Cat_GetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, CInfoPBRec* pb);
OSErr Cat_SetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, const CInfoPBRec* pb);
CNodeID Cat_GetNextID(VCB* vcb);

/* Extent Management */
OSErr Ext_Open(VCB* vcb);
OSErr Ext_Close(VCB* vcb);
OSErr Ext_Allocate(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 blocks, ExtDataRec extents);
OSErr Ext_Deallocate(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 startBlock);
OSErr Ext_Map(VCB* vcb, FCB* fcb, UInt32 fileBlock, UInt32* physBlock, UInt32* contiguous);
OSErr Ext_Extend(VCB* vcb, FCB* fcb, UInt32 newSize);
OSErr Ext_Truncate(VCB* vcb, FCB* fcb, UInt32 newSize);

/* Allocation Bitmap Management */
OSErr Alloc_Init(VCB* vcb);
OSErr Alloc_Close(VCB* vcb);
OSErr Alloc_Blocks(VCB* vcb, UInt32 startHint, UInt32 minBlocks, UInt32 maxBlocks,
                   UInt32* actualStart, UInt32* actualCount);
OSErr Alloc_Free(VCB* vcb, UInt32 startBlock, UInt32 blockCount);
UInt32 Alloc_CountFree(VCB* vcb);
Boolean Alloc_Check(VCB* vcb, UInt32 startBlock, UInt32 blockCount);

/* Cache Management */
OSErr Cache_Init(UInt32 cacheSize);
void Cache_Shutdown(void);
OSErr Cache_GetBlock(VCB* vcb, UInt32 blockNum, CacheBuffer** buffer);
OSErr Cache_ReleaseBlock(CacheBuffer* buffer, Boolean dirty);
OSErr Cache_FlushVolume(VCB* vcb);
OSErr Cache_FlushAll(void);
void Cache_Invalidate(VCB* vcb);

/* I/O Operations */
OSErr IO_ReadBlocks(VCB* vcb, UInt32 startBlock, UInt32 blockCount, void* buffer);
OSErr IO_WriteBlocks(VCB* vcb, UInt32 startBlock, UInt32 blockCount, const void* buffer);
OSErr IO_ReadFork(FCB* fcb, UInt32 offset, UInt32 count, void* buffer, UInt32* actual);
OSErr IO_WriteFork(FCB* fcb, UInt32 offset, UInt32 count, const void* buffer, UInt32* actual);

/* Path and Name Utilities */
OSErr Path_Parse(const char* path, VolumeRefNum* vRefNum, DirID* dirID, UInt8* name);
OSErr Path_Build(VCB* vcb, DirID dirID, const UInt8* name, char* path, size_t maxLen);
Boolean Name_Equal(const UInt8* name1, const UInt8* name2);
void Name_Copy(UInt8* dst, const UInt8* src);
UInt16 Name_Hash(const UInt8* name);

/* Date/Time Utilities */
UInt32 DateTime_Current(void);
UInt32 DateTime_FromUnix(time_t unixTime);
time_t DateTime_ToUnix(UInt32 macTime);

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

extern PlatformHooks g_PlatformHooks;

#ifdef __cplusplus
}
#endif

#endif /* __FILEMANAGER_INTERNAL_H__ */
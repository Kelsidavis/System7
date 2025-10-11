/*
 * FileManagerStubs.c - Stub implementations for File Manager dependencies
 *
 * This file provides minimal stub implementations for File Manager
 * functions and types that are not yet fully implemented.
 */

#include "SystemTypes.h"
#include "FileManager.h"
#include "FileManager_Internal.h"
#include <stdlib.h>
#include <string.h>
#include "FS/FSLogging.h"

/* ============================================================================
 * Global variables
 * ============================================================================ */

/* Global state and hooks are now defined in FileManager.c */
extern FSGlobals g_FSGlobals;
extern PlatformHooks g_PlatformHooks;

/* ============================================================================
 * Stub Implementations
 * ============================================================================ */

/* Volume Management */
VCB* VCB_Alloc(void) {
    FS_LOG_DEBUG("VCB_Alloc stub\n");
    return (VCB*)calloc(1, sizeof(VCB));
}

void VCB_Free(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Free stub\n");
    if (vcb) free(vcb);
}

VCB* VCB_Find(VolumeRefNum vRefNum) {
    FS_LOG_DEBUG("VCB_Find stub: vRefNum=%d\n", vRefNum);
    VCB* vcb = g_FSGlobals.vcbQueue;
    while (vcb) {
        if (vcb->base.vcbVRefNum == vRefNum) {
            return vcb;
        }
        vcb = vcb->vcbNext;
    }
    return NULL;
}

VCB* VCB_FindByName(const UInt8* name) {
    FS_LOG_DEBUG("VCB_FindByName stub\n");
    return NULL;
}

OSErr VCB_Mount(UInt16 drvNum, VCB** newVCB) {
    FS_LOG_DEBUG("VCB_Mount stub: drvNum=%d\n", drvNum);
    return nsvErr;
}

OSErr VCB_Unmount(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Unmount stub\n");
    return noErr;
}

OSErr VCB_Flush(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Flush stub\n");
    return noErr;
}

OSErr VCB_Update(VCB* vcb) {
    FS_LOG_DEBUG("VCB_Update stub\n");
    return noErr;
}

/* File Control Block Management */
FCB* FCB_Alloc(void) {
    FS_LOG_DEBUG("FCB_Alloc stub\n");
    if (g_FSGlobals.fcbFree < g_FSGlobals.fcbCount) {
        FCB* fcb = &g_FSGlobals.fcbArray[g_FSGlobals.fcbFree];
        g_FSGlobals.fcbFree = fcb->fcbRefNum;
        fcb->fcbRefNum = g_FSGlobals.fcbFree + 1;
        return fcb;
    }
    return NULL;
}

void FCB_Free(FCB* fcb) {
    FS_LOG_DEBUG("FCB_Free stub\n");
    if (fcb) {
        memset(fcb, 0, sizeof(FCB));
    }
}

FCB* FCB_Find(FileRefNum refNum) {
    FS_LOG_DEBUG("FCB_Find stub: refNum=%d\n", refNum);
    if (refNum > 0 && refNum <= g_FSGlobals.fcbCount) {
        FCB* fcb = &g_FSGlobals.fcbArray[refNum - 1];
        if (fcb->base.fcbFlNm != 0) {
            return fcb;
        }
    }
    return NULL;
}

FCB* FCB_FindByID(VCB* vcb, UInt32 fileID) {
    FS_LOG_DEBUG("FCB_FindByID stub: fileID=%d\n", fileID);
    return NULL;
}

OSErr FCB_Open(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 permission, FCB** newFCB) {
    FS_LOG_DEBUG("FCB_Open stub\n");
    return fnfErr;
}

OSErr FCB_Close(FCB* fcb) {
    FS_LOG_DEBUG("FCB_Close stub\n");
    return noErr;
}

OSErr FCB_Flush(FCB* fcb) {
    FS_LOG_DEBUG("FCB_Flush stub\n");
    return noErr;
}

/* Working Directory Management */
WDCB* WDCB_Alloc(void) {
    FS_LOG_DEBUG("WDCB_Alloc stub\n");
    return (WDCB*)calloc(1, sizeof(WDCB));
}

void WDCB_Free(WDCB* wdcb) {
    FS_LOG_DEBUG("WDCB_Free stub\n");
    if (wdcb) free(wdcb);
}

WDCB* WDCB_Find(WDRefNum wdRefNum) {
    FS_LOG_DEBUG("WDCB_Find stub: wdRefNum=%d\n", wdRefNum);
    return NULL;
}

OSErr WDCB_Create(VCB* vcb, UInt32 dirID, UInt32 procID, WDCB** newWDCB) {
    FS_LOG_DEBUG("WDCB_Create stub\n");
    return tmwdoErr;
}

/* B-tree Operations */
OSErr BTree_Open(VCB* vcb, UInt32 fileID, BTCB** btcb) {
    FS_LOG_DEBUG("BTree_Open stub: fileID=%d\n", fileID);
    return btNoErr;
}

OSErr BTree_Close(BTCB* btcb) {
    FS_LOG_DEBUG("BTree_Close stub\n");
    return btNoErr;
}

OSErr BTree_Search(BTCB* btcb, const void* key, void* record, UInt16* recordSize, UInt32* hint) {
    FS_LOG_DEBUG("BTree_Search stub\n");
    return btRecNotFnd;
}

OSErr BTree_Insert(BTCB* btcb, const void* key, const void* record, UInt16 recordSize) {
    FS_LOG_DEBUG("BTree_Insert stub\n");
    return btNoErr;
}

OSErr BTree_Delete(BTCB* btcb, const void* key) {
    FS_LOG_DEBUG("BTree_Delete stub\n");
    return btNoErr;
}

OSErr BTree_GetNode(BTCB* btcb, UInt32 nodeNum, void** nodePtr) {
    FS_LOG_DEBUG("BTree_GetNode stub: nodeNum=%d\n", nodeNum);
    return btNoErr;
}

OSErr BTree_ReleaseNode(BTCB* btcb, UInt32 nodeNum) {
    FS_LOG_DEBUG("BTree_ReleaseNode stub: nodeNum=%d\n", nodeNum);
    return btNoErr;
}

OSErr BTree_FlushNode(BTCB* btcb, UInt32 nodeNum) {
    FS_LOG_DEBUG("BTree_FlushNode stub: nodeNum=%d\n", nodeNum);
    return btNoErr;
}

/* Catalog Operations */
OSErr Cat_Open(VCB* vcb) {
    FS_LOG_DEBUG("Cat_Open stub\n");
    return noErr;
}

OSErr Cat_Close(VCB* vcb) {
    FS_LOG_DEBUG("Cat_Close stub\n");
    return noErr;
}

OSErr Cat_Lookup(VCB* vcb, UInt32 dirID, const UInt8* name, void* catData, UInt32* hint) {
    FS_LOG_DEBUG("Cat_Lookup stub: dirID=%d\n", dirID);
    return fnfErr;
}

OSErr Cat_Create(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 type, void* catData) {
    FS_LOG_DEBUG("Cat_Create stub\n");
    return noErr;
}

OSErr Cat_Delete(VCB* vcb, UInt32 dirID, const UInt8* name) {
    FS_LOG_DEBUG("Cat_Delete stub\n");
    return noErr;
}

OSErr Cat_Rename(VCB* vcb, UInt32 dirID, const UInt8* oldName, const UInt8* newName) {
    FS_LOG_DEBUG("Cat_Rename stub\n");
    return noErr;
}

OSErr Cat_Move(VCB* vcb, UInt32 srcDirID, const UInt8* name, UInt32 dstDirID) {
    FS_LOG_DEBUG("Cat_Move stub\n");
    return noErr;
}

OSErr Cat_GetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, CInfoPBRec* pb) {
    FS_LOG_DEBUG("Cat_GetInfo stub\n");
    return fnfErr;
}

OSErr Cat_SetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, const CInfoPBRec* pb) {
    FS_LOG_DEBUG("Cat_SetInfo stub\n");
    return noErr;
}

CNodeID Cat_GetNextID(VCB* vcb) {
    FS_LOG_DEBUG("Cat_GetNextID stub\n");
    static CNodeID nextID = 100;
    return nextID++;
}

/* Extent Management */
OSErr Ext_Allocate(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 blocks, ExtDataRec extents) {
    FS_LOG_DEBUG("Ext_Allocate stub: fileID=%d, blocks=%d\n", fileID, blocks);
    return noErr;
}

OSErr Ext_Deallocate(VCB* vcb, UInt32 fileID, UInt8 forkType, UInt32 startBlock) {
    FS_LOG_DEBUG("Ext_Deallocate stub\n");
    return noErr;
}

/* I/O Operations */
OSErr IO_Format(UInt16 drvNum, const UInt8* volName, UInt32 volSize) {
    FS_LOG_DEBUG("IO_Format stub: drvNum=%d\n", drvNum);
    return noErr;
}

/* All locking functions are now defined in FileManager.c */

/* POSIX stubs for bare metal kernel */
int sched_yield(void) {
    /* In a bare metal kernel, just return success */
    return 0;
}

time_t time(time_t* t) {
    /* Return a fake timestamp */
    time_t fake_time = 0x60000000;  /* Some arbitrary value */
    if (t) *t = fake_time;
    return fake_time;
}

/* Utility Functions */
OSErr FS_CompareNames(const UInt8* name1, const UInt8* name2, Boolean* equal) {
    FS_LOG_DEBUG("FS_CompareNames stub\n");
    *equal = false;
    return noErr;
}

OSErr FS_CopyName(const UInt8* src, UInt8* dst, UInt8 maxLen) {
    FS_LOG_DEBUG("FS_CopyName stub\n");
    if (src && dst) {
        UInt8 len = src[0];
        if (len > maxLen) len = maxLen;
        memcpy(dst, src, len + 1);
    }
    return noErr;
}

UInt32 FS_GetTime(void) {
    FS_LOG_DEBUG("FS_GetTime stub\n");
    return 0;
}

OSErr FS_ValidateName(const UInt8* name) {
    FS_LOG_DEBUG("FS_ValidateName stub\n");
    if (!name || name[0] == 0 || name[0] > 31) {
        return bdNamErr;
    }
    return noErr;
}
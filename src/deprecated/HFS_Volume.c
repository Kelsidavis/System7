#include "MemoryMgr/MemoryManager.h"
#include "SystemTypes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "System71StdLib.h"
/*
 * HFS_Volume.c - HFS Volume Management Implementation
 *
 * This file implements HFS volume operations including mounting, unmounting,
 * and managing Volume Control Blocks (VCBs) and Master Directory Blocks (MDBs).
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 * Derived from System 7 ROM analysis (Ghidra) HFS architecture
 */

#include "FileManager.h"
#include "FileManager_Internal.h"


/* External globals */
extern FSGlobals g_FSGlobals;
extern PlatformHooks g_PlatformHooks;

/* Volume reference number counter */
static VolumeRefNum g_nextVRefNum = -1;

/* ============================================================================
 * VCB Management
 * ============================================================================ */

/**
 * Allocate a new VCB
 */
VCB* VCB_Alloc(void)
{
    VCB* vcb;

    vcb = (VCB*)NewPtrClear(sizeof(VCB));
    if (!vcb) {
        return NULL;
    }

    /* Initialize mutex */
#ifdef PLATFORM_REMOVED_WIN32
    InitializeCriticalSection(&vcb->vcbMutex);
#else
    pthread_mutex_init(&vcb->vcbMutex, NULL);
#endif

    /* Assign unique volume reference number */
    vcb->vcbVRefNum = g_nextVRefNum--;

    return vcb;
}

/**
 * Free a VCB
 */
void VCB_Free(VCB* vcb)
{
    if (!vcb) {
        return;
    }

    /* Close allocation bitmap */
    Alloc_Close(vcb);

    /* Free caches */
    if (vcb->vcbVBMCache) {
        DisposePtr((Ptr)vcb->vcbVBMCache);
    }
    if (vcb->vcbCtlCache) {
        DisposePtr((Ptr)vcb->vcbCtlCache);
    }

    /* Close B-trees */
    if (vcb->vcbXTRef) {
        BTree_Close((BTCB*)vcb->vcbXTRef);
    }
    if (vcb->vcbCTRef) {
        BTree_Close((BTCB*)vcb->vcbCTRef);
    }

    /* Close device */
    if (vcb->vcbDevice && g_PlatformHooks.DeviceClose) {
        g_PlatformHooks.DeviceClose(vcb->vcbDevice);
    }

    /* Destroy mutex */
#ifdef PLATFORM_REMOVED_WIN32
    DeleteCriticalSection(&vcb->vcbMutex);
#else
    pthread_mutex_destroy(&vcb->vcbMutex);
#endif

    DisposePtr((Ptr)vcb);
}

/**
 * Find a VCB by volume reference number
 */
VCB* VCB_Find(VolumeRefNum vRefNum)
{
    VCB* vcb;

    FS_LockGlobal();

    /* Search the VCB queue */
    vcb = g_FSGlobals.vcbQueue;
    while (vcb) {
        if (vcb->vcbVRefNum == vRefNum) {
            break;
        }
        vcb = vcb->vcbNext;
    }

    FS_UnlockGlobal();

    return vcb;
}

/**
 * Find a VCB by volume name
 */
VCB* VCB_FindByName(const UInt8* name)
{
    VCB* vcb;

    if (!name || name[0] == 0) {
        return NULL;
    }

    FS_LockGlobal();

    /* Search the VCB queue */
    vcb = g_FSGlobals.vcbQueue;
    while (vcb) {
        if (Name_Equal(vcb->vcbVN, name)) {
            break;
        }
        vcb = vcb->vcbNext;
    }

    FS_UnlockGlobal();

    return vcb;
}

/* ============================================================================
 * Volume Mounting
 * ============================================================================ */

/**
 * Read and validate the Master Directory Block
 */
static OSErr ReadMDB(VCB* vcb, MasterDirectoryBlock* mdb)
{
    OSErr err;
    UInt8 buffer[BLOCK_SIZE * 2];  /* MDB can span 2 blocks */

    /* Read MDB from block 2 */
    if (!g_PlatformHooks.DeviceRead) {
        return extFSErr;
    }

    err = g_PlatformHooks.DeviceRead(vcb->vcbDevice,
                                     MDB_BLOCK * BLOCK_SIZE,
                                     sizeof(buffer),
                                     buffer);
    if (err != noErr) {
        return err;
    }

    /* Copy MDB structure */
    memcpy(mdb, buffer, sizeof(MasterDirectoryBlock));

    /* Byte-swap if necessary (handle endianness) */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    mdb->drSigWord = __builtin_bswap16(mdb->drSigWord);
    mdb->drCrDate = __builtin_bswap32(mdb->drCrDate);
    mdb->drLsMod = __builtin_bswap32(mdb->drLsMod);
    mdb->drAtrb = __builtin_bswap16(mdb->drAtrb);
    mdb->drNmFls = __builtin_bswap16(mdb->drNmFls);
    mdb->drVBMSt = __builtin_bswap16(mdb->drVBMSt);
    mdb->drAllocPtr = __builtin_bswap16(mdb->drAllocPtr);
    mdb->drNmAlBlks = __builtin_bswap16(mdb->drNmAlBlks);
    mdb->drAlBlkSiz = __builtin_bswap32(mdb->drAlBlkSiz);
    mdb->drClpSiz = __builtin_bswap32(mdb->drClpSiz);
    mdb->drAlBlSt = __builtin_bswap16(mdb->drAlBlSt);
    mdb->drNxtCNID = __builtin_bswap32(mdb->drNxtCNID);
    mdb->drFreeBks = __builtin_bswap16(mdb->drFreeBks);
    mdb->drVolBkUp = __builtin_bswap32(mdb->drVolBkUp);
    mdb->drVSeqNum = __builtin_bswap16(mdb->drVSeqNum);
    mdb->drWrCnt = __builtin_bswap32(mdb->drWrCnt);
    mdb->drXTClpSiz = __builtin_bswap32(mdb->drXTClpSiz);
    mdb->drCTClpSiz = __builtin_bswap32(mdb->drCTClpSiz);
    mdb->drNmRtDirs = __builtin_bswap16(mdb->drNmRtDirs);
    mdb->drFilCnt = __builtin_bswap32(mdb->drFilCnt);
    mdb->drDirCnt = __builtin_bswap32(mdb->drDirCnt);
    mdb->drVCSize = __builtin_bswap16(mdb->drVCSize);
    mdb->drVBMCSize = __builtin_bswap16(mdb->drVBMCSize);
    mdb->drCtlCSize = __builtin_bswap16(mdb->drCtlCSize);
    mdb->drXTFlSize = __builtin_bswap32(mdb->drXTFlSize);
    mdb->drCTFlSize = __builtin_bswap32(mdb->drCTFlSize);

    /* Swap extent records */
    for (int i = 0; i < 3; i++) {
        mdb->drXTExtRec[i].startBlock = __builtin_bswap16(mdb->drXTExtRec[i].startBlock);
        mdb->drXTExtRec[i].blockCount = __builtin_bswap16(mdb->drXTExtRec[i].blockCount);
        mdb->drCTExtRec[i].startBlock = __builtin_bswap16(mdb->drCTExtRec[i].startBlock);
        mdb->drCTExtRec[i].blockCount = __builtin_bswap16(mdb->drCTExtRec[i].blockCount);
    }
#endif

    /* Validate signature */
    if (mdb->drSigWord != HFS_SIGNATURE && mdb->drSigWord != MFS_SIGNATURE) {
        return badMDBErr;
    }

    return noErr;
}

/**
 * Write the Master Directory Block back to disk
 */
static OSErr WriteMDB(VCB* vcb)
{
    MasterDirectoryBlock mdb;
    OSErr err;
    UInt8 buffer[BLOCK_SIZE * 2];

    if (!g_PlatformHooks.DeviceWrite) {
        return extFSErr;
    }

    /* Copy VCB data to MDB */
    memset(&mdb, 0, sizeof(mdb));
    mdb.drSigWord = vcb->vcbSigWord;
    mdb.drCrDate = vcb->vcbCrDate;
    mdb.drLsMod = vcb->vcbLsMod;
    mdb.drAtrb = vcb->vcbAtrb;
    mdb.drNmFls = vcb->vcbNmFls;
    mdb.drVBMSt = vcb->vcbVBMSt;
    mdb.drAllocPtr = vcb->vcbAllocPtr;
    mdb.drNmAlBlks = vcb->vcbNmAlBlks;
    mdb.drAlBlkSiz = vcb->vcbAlBlkSiz;
    mdb.drClpSiz = vcb->vcbClpSiz;
    mdb.drAlBlSt = vcb->vcbAlBlSt;
    mdb.drNxtCNID = vcb->vcbNxtCNID;
    mdb.drFreeBks = vcb->vcbFreeBks;
    memcpy(mdb.drVN, vcb->vcbVN, sizeof(mdb.drVN));
    mdb.drVolBkUp = vcb->vcbVolBkUp;
    mdb.drVSeqNum = vcb->vcbVSeqNum;
    mdb.drWrCnt = vcb->vcbWrCnt;
    mdb.drFilCnt = vcb->vcbFilCnt;
    mdb.drDirCnt = vcb->vcbDirCnt;
    memcpy(mdb.drFndrInfo, vcb->vcbFndrInfo, sizeof(mdb.drFndrInfo));

    /* Byte-swap if necessary */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    MasterDirectoryBlock swapped = mdb;
    swapped.drSigWord = __builtin_bswap16(mdb.drSigWord);
    swapped.drCrDate = __builtin_bswap32(mdb.drCrDate);
    swapped.drLsMod = __builtin_bswap32(mdb.drLsMod);
    swapped.drAtrb = __builtin_bswap16(mdb.drAtrb);
    swapped.drNmFls = __builtin_bswap16(mdb.drNmFls);
    swapped.drVBMSt = __builtin_bswap16(mdb.drVBMSt);
    swapped.drAllocPtr = __builtin_bswap16(mdb.drAllocPtr);
    swapped.drNmAlBlks = __builtin_bswap16(mdb.drNmAlBlks);
    swapped.drAlBlkSiz = __builtin_bswap32(mdb.drAlBlkSiz);
    swapped.drClpSiz = __builtin_bswap32(mdb.drClpSiz);
    swapped.drAlBlSt = __builtin_bswap16(mdb.drAlBlSt);
    swapped.drNxtCNID = __builtin_bswap32(mdb.drNxtCNID);
    swapped.drFreeBks = __builtin_bswap16(mdb.drFreeBks);
    swapped.drVolBkUp = __builtin_bswap32(mdb.drVolBkUp);
    swapped.drVSeqNum = __builtin_bswap16(mdb.drVSeqNum);
    swapped.drWrCnt = __builtin_bswap32(mdb.drWrCnt);
    swapped.drXTClpSiz = __builtin_bswap32(mdb.drXTClpSiz);
    swapped.drCTClpSiz = __builtin_bswap32(mdb.drCTClpSiz);
    swapped.drNmRtDirs = __builtin_bswap16(mdb.drNmRtDirs);
    swapped.drFilCnt = __builtin_bswap32(mdb.drFilCnt);
    swapped.drDirCnt = __builtin_bswap32(mdb.drDirCnt);
    swapped.drVCSize = __builtin_bswap16(mdb.drVCSize);
    swapped.drVBMCSize = __builtin_bswap16(mdb.drVBMCSize);
    swapped.drCtlCSize = __builtin_bswap16(mdb.drCtlCSize);
    swapped.drXTFlSize = __builtin_bswap32(mdb.drXTFlSize);
    swapped.drCTFlSize = __builtin_bswap32(mdb.drCTFlSize);

    for (int i = 0; i < 3; i++) {
        swapped.drXTExtRec[i].startBlock = __builtin_bswap16(mdb.drXTExtRec[i].startBlock);
        swapped.drXTExtRec[i].blockCount = __builtin_bswap16(mdb.drXTExtRec[i].blockCount);
        swapped.drCTExtRec[i].startBlock = __builtin_bswap16(mdb.drCTExtRec[i].startBlock);
        swapped.drCTExtRec[i].blockCount = __builtin_bswap16(mdb.drCTExtRec[i].blockCount);
    }
    mdb = swapped;
#endif

    /* Write to buffer and disk */
    memcpy(buffer, &mdb, sizeof(MasterDirectoryBlock));

    err = g_PlatformHooks.DeviceWrite(vcb->vcbDevice,
                                      MDB_BLOCK * BLOCK_SIZE,
                                      sizeof(buffer),
                                      buffer);
    if (err != noErr) {
        return err;
    }

    /* Write alternate MDB at end of volume */
    uint64_t altMDBOffset = (uint64_t)(vcb->vcbAlBlSt + vcb->vcbNmAlBlks - 2) * BLOCK_SIZE;
    err = g_PlatformHooks.DeviceWrite(vcb->vcbDevice,
                                      altMDBOffset,
                                      sizeof(buffer),
                                      buffer);

    /* Update write count */
    vcb->vcbWrCnt++;
    vcb->vcbLsMod = DateTime_Current();

    return noErr;
}

/**
 * Mount an HFS volume
 */
OSErr VCB_Mount(UInt16 drvNum, VCB** newVCB)
{
    VCB* vcb = NULL;
    MasterDirectoryBlock mdb;
    OSErr err;
    char devicePath[256];

    if (!newVCB) {
        return paramErr;
    }

    *newVCB = NULL;

    /* Allocate VCB */
    vcb = VCB_Alloc();
    if (!vcb) {
        return memFullErr;
    }

    /* Open device */
    if (!g_PlatformHooks.DeviceOpen) {
        VCB_Free(vcb);
        return extFSErr;
    }

    /* Convert drive number to device path (platform-specific) */
    snprintf(devicePath, sizeof(devicePath), "/dev/disk%d", drvNum);

    err = g_PlatformHooks.DeviceOpen(devicePath, &vcb->vcbDevice);
    if (err != noErr) {
        VCB_Free(vcb);
        return err;
    }

    /* Read MDB */
    err = ReadMDB(vcb, &mdb);
    if (err != noErr) {
        VCB_Free(vcb);
        return err;
    }

    /* Copy MDB data to VCB */
    vcb->vcbSigWord = mdb.drSigWord;
    vcb->vcbCrDate = mdb.drCrDate;
    vcb->vcbLsMod = mdb.drLsMod;
    vcb->vcbAtrb = mdb.drAtrb;
    vcb->vcbNmFls = mdb.drNmFls;
    vcb->vcbVBMSt = mdb.drVBMSt;
    vcb->vcbAllocPtr = mdb.drAllocPtr;
    vcb->vcbNmAlBlks = mdb.drNmAlBlks;
    vcb->vcbAlBlkSiz = mdb.drAlBlkSiz;
    vcb->vcbClpSiz = mdb.drClpSiz;
    vcb->vcbAlBlSt = mdb.drAlBlSt;
    vcb->vcbNxtCNID = mdb.drNxtCNID;
    vcb->vcbFreeBks = mdb.drFreeBks;
    memcpy(vcb->vcbVN, mdb.drVN, sizeof(vcb->vcbVN));
    vcb->vcbVolBkUp = mdb.drVolBkUp;
    vcb->vcbVSeqNum = mdb.drVSeqNum;
    vcb->vcbWrCnt = mdb.drWrCnt;
    vcb->vcbFilCnt = mdb.drFilCnt;
    vcb->vcbDirCnt = mdb.drDirCnt;
    memcpy(vcb->vcbFndrInfo, mdb.drFndrInfo, sizeof(vcb->vcbFndrInfo));

    vcb->vcbDrvNum = drvNum;
    vcb->vcbDRefNum = -drvNum - 1;  /* Driver reference number convention */
    vcb->vcbFSID = 0;  /* HFS file system ID */

    /* Check for HFS vs MFS */
    if (vcb->vcbSigWord == MFS_SIGNATURE) {
        /* MFS not supported in this implementation */
        VCB_Free(vcb);
        return extFSErr;
    }

    /* Initialize allocation bitmap */
    err = Alloc_Init(vcb);
    if (err != noErr) {
        VCB_Free(vcb);
        return err;
    }

    /* Open extents B-tree */
    err = BTree_Open(vcb, EXTENTS_FILE_ID, (BTCB**)&vcb->vcbXTRef);
    if (err != noErr) {
        VCB_Free(vcb);
        return err;
    }

    /* Open catalog B-tree */
    err = BTree_Open(vcb, CATALOG_FILE_ID, (BTCB**)&vcb->vcbCTRef);
    if (err != noErr) {
        VCB_Free(vcb);
        return err;
    }

    /* Add to VCB queue */
    FS_LockGlobal();
    vcb->vcbNext = g_FSGlobals.vcbQueue;
    g_FSGlobals.vcbQueue = vcb;
    g_FSGlobals.vcbCount++;
    FS_UnlockGlobal();

    *newVCB = vcb;

    return noErr;
}

/**
 * Unmount an HFS volume
 */
OSErr VCB_Unmount(VCB* vcb)
{
    VCB** prev;
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    /* Set unmounting flag */
    vcb->vcbAtrb |= VCB_UNMOUNTING;

    /* Flush all dirty data */
    err = VCB_Flush(vcb);
    if (err != noErr && err != ioErr) {
        vcb->vcbAtrb &= ~VCB_UNMOUNTING;
        return err;
    }

    /* Remove from VCB queue */
    FS_LockGlobal();

    prev = &g_FSGlobals.vcbQueue;
    while (*prev) {
        if (*prev == vcb) {
            *prev = vcb->vcbNext;
            g_FSGlobals.vcbCount--;
            break;
        }
        prev = &(*prev)->vcbNext;
    }

    /* Update default volume if necessary */
    if (g_FSGlobals.defVRefNum == vcb->vcbVRefNum) {
        g_FSGlobals.defVRefNum = g_FSGlobals.vcbQueue ?
                                  (g_FSGlobals)->vcbVRefNum : 0;
    }

    FS_UnlockGlobal();

    /* Free the VCB */
    VCB_Free(vcb);

    return noErr;
}

/**
 * Flush volume changes to disk
 */
OSErr VCB_Flush(VCB* vcb)
{
    OSErr err;

    if (!vcb) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    /* Check if volume is dirty */
    if (!(vcb->vcbFlags & VCB_DIRTY)) {
        FS_UnlockVolume(vcb);
        return noErr;
    }

    /* Flush cache buffers */
    err = Cache_FlushVolume(vcb);
    if (err != noErr) {
        FS_UnlockVolume(vcb);
        return err;
    }

    /* Flush allocation bitmap */
    if (vcb->vcbVBMCache) {
        /* Write bitmap to disk */
        UInt32 bitmapBlocks = (vcb->vcbNmAlBlks + 4095) / 4096;
        err = IO_WriteBlocks(vcb, vcb->vcbVBMSt, bitmapBlocks, vcb->vcbVBMCache);
        if (err != noErr) {
            FS_UnlockVolume(vcb);
            return err;
        }
    }

    /* Update and write MDB */
    err = WriteMDB(vcb);
    if (err != noErr) {
        FS_UnlockVolume(vcb);
        return err;
    }

    /* Flush device */
    if (g_PlatformHooks.DeviceFlush) {
        err = g_PlatformHooks.DeviceFlush(vcb->vcbDevice);
        if (err != noErr) {
            FS_UnlockVolume(vcb);
            return err;
        }
    }

    /* Clear dirty flag */
    vcb->vcbFlags &= ~VCB_DIRTY;

    FS_UnlockVolume(vcb);

    return noErr;
}

/**
 * Update volume information
 */
OSErr VCB_Update(VCB* vcb)
{
    if (!vcb) {
        return paramErr;
    }

    FS_LockVolume(vcb);

    /* Update modification time */
    vcb->vcbLsMod = DateTime_Current();

    /* Mark as dirty */
    vcb->vcbFlags |= VCB_DIRTY;

    FS_UnlockVolume(vcb);

    return noErr;
}

/* ============================================================================
 * FCB Management
 * ============================================================================ */

/**
 * Allocate a new FCB
 */
FCB* FCB_Alloc(void)
{
    FCB* fcb;
    UInt16 index;

    FS_LockGlobal();

    /* Find a free FCB */
    if (g_FSGlobals.fcbFree >= g_FSGlobals.fcbCount) {
        FS_UnlockGlobal();
        return NULL;
    }

    index = g_FSGlobals.fcbFree;
    fcb = &g_FSGlobals.fcbArray[index];

    /* Update free list */
    g_FSGlobals.fcbFree = (UInt16)fcb->fcbRefNum;

    /* Initialize FCB */
    memset(fcb, 0, sizeof(FCB));
    fcb->fcbRefNum = (FileRefNum)(index + 1);  /* 1-based reference numbers */

    /* Initialize mutex */
#ifdef PLATFORM_REMOVED_WIN32
    InitializeCriticalSection(&fcb->fcbMutex);
#else
    pthread_mutex_init(&fcb->fcbMutex, NULL);
#endif

    FS_UnlockGlobal();

    return fcb;
}

/**
 * Free an FCB
 */
void FCB_Free(FCB* fcb)
{
    UInt16 index;

    if (!fcb) {
        return;
    }

    /* Flush any dirty data */
    if (fcb->fcbFlags & FCB_DIRTY) {
        FCB_Flush(fcb);
    }

    FS_LockGlobal();

    /* Calculate index */
    index = (UInt16)(fcb - g_FSGlobals.fcbArray);

    /* Clear FCB */
    fcb->fcbFlNm = 0;
    fcb->fcbVPtr = NULL;

    /* Add to free list */
    fcb->fcbRefNum = (FileRefNum)g_FSGlobals.fcbFree;
    g_FSGlobals.fcbFree = index;

    /* Destroy mutex */
#ifdef PLATFORM_REMOVED_WIN32
    DeleteCriticalSection(&fcb->fcbMutex);
#else
    pthread_mutex_destroy(&fcb->fcbMutex);
#endif

    FS_UnlockGlobal();
}

/**
 * Find an FCB by reference number
 */
FCB* FCB_Find(FileRefNum refNum)
{
    FCB* fcb;
    UInt16 index;

    if (refNum <= 0 || refNum > g_FSGlobals.fcbCount) {
        return NULL;
    }

    index = (UInt16)(refNum - 1);
    fcb = &g_FSGlobals.fcbArray[index];

    /* Check if FCB is in use */
    if (fcb->fcbFlNm == 0) {
        return NULL;
    }

    return fcb;
}

/**
 * Find an FCB by file ID
 */
FCB* FCB_FindByID(VCB* vcb, UInt32 fileID)
{
    FCB* fcb;

    if (!vcb || fileID == 0) {
        return NULL;
    }

    FS_LockGlobal();

    /* Search all FCBs */
    for (int i = 0; i < g_FSGlobals.fcbCount; i++) {
        fcb = &g_FSGlobals.fcbArray[i];
        if (fcb->fcbFlNm == fileID && fcb->fcbVPtr == vcb) {
            FS_UnlockGlobal();
            return fcb;
        }
    }

    FS_UnlockGlobal();

    return NULL;
}

/**
 * Open a file
 */
OSErr FCB_Open(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 permission, FCB** newFCB)
{
    FCB* fcb = NULL;
    CatalogFileRec fileRec;
    UInt32 hint = 0;
    OSErr err;

    if (!vcb || !name || !newFCB) {
        return paramErr;
    }

    *newFCB = NULL;

    /* Look up file in catalog */
    err = Cat_Lookup(vcb, dirID, name, &fileRec, &hint);
    if (err != noErr) {
        return err;
    }

    /* Check if it's a file (not directory) */
    if (fileRec.cdrType != REC_FIL) {
        return notAFileErr;
    }

    /* Check if already open */
    fcb = FCB_FindByID(vcb, fileRec.filFlNum);
    if (fcb) {
        /* Check permissions */
        if ((permission & fsWrPerm) && !(fcb->fcbFlags & FCB_SHARED_WRITE)) {
            return opWrErr;
        }

        /* Increment open count */
        fcb->fcbOpenCnt++;
        *newFCB = fcb;
        return noErr;
    }

    /* Allocate new FCB */
    fcb = FCB_Alloc();
    if (!fcb) {
        return tmfoErr;
    }

    /* Initialize FCB from catalog record */
    fcb->fcbFlNm = fileRec.filFlNum;
    fcb->fcbFlags = 0;
    fcb->fcbSBlk = fileRec.filStBlk;
    fcb->fcbEOF = fileRec.filLgLen;
    fcb->fcbPLen = fileRec.filPyLen;
    fcb->fcbCrPs = 0;
    fcb->fcbVPtr = vcb;
    fcb->fcbClmpSize = fileRec.filClpSize;
    fcb->fcbDirID = dirID;
    fcb->fcbCatPos = hint;
    fcb->fcbFType = fileRec.filUsrWds.fdType;
    memcpy(fcb->fcbExtRec, fileRec.filExtRec, sizeof(ExtDataRec));
    Name_Copy(fcb->fcbCName, name);

    /* Set permissions */
    if (permission & fsWrPerm) {
        fcb->fcbFlags |= FCB_WRITE_PERM;
    }

    /* Set open count */
    fcb->fcbOpenCnt = 1;

    /* Set process owner */
    fcb->fcbProcessID = g_FSGlobals.currentProcess;

    /* Update last access time */
    fcb->fcbLastAccess = DateTime_Current();

    *newFCB = fcb;

    return noErr;
}

/**
 * Close a file
 */
OSErr FCB_Close(FCB* fcb)
{
    OSErr err;

    if (!fcb) {
        return rfNumErr;
    }

    FS_LockFCB(fcb);

    /* Decrement open count */
    if (fcb->fcbOpenCnt > 0) {
        fcb->fcbOpenCnt--;
    }

    /* If still open by others, just return */
    if (fcb->fcbOpenCnt > 0) {
        FS_UnlockFCB(fcb);
        return noErr;
    }

    /* Flush any dirty data */
    if (fcb->fcbFlags & FCB_DIRTY) {
        err = FCB_Flush(fcb);
        if (err != noErr) {
            FS_UnlockFCB(fcb);
            return err;
        }
    }

    FS_UnlockFCB(fcb);

    /* Free the FCB */
    FCB_Free(fcb);

    return noErr;
}

/**
 * Flush file changes to disk
 */
OSErr FCB_Flush(FCB* fcb)
{
    CatalogFileRec fileRec;
    UInt32 hint;
    OSErr err;

    if (!fcb) {
        return rfNumErr;
    }

    /* Check if dirty */
    if (!(fcb->fcbFlags & FCB_DIRTY)) {
        return noErr;
    }

    /* Update catalog with current file sizes and extent record */
    err = Cat_UpdateFileRecord(fcb->fcbVPtr, fcb->fcbDirID, fcb->fcbCName,
                               fcb->fcbEOF, fcb->fcbPLen, &fcb->fcbExtRec);
    if (err != noErr) {
        FS_LOG_ERROR("FCB_Flush: Cat_UpdateFileRecord failed: %d\n", err);
        return err;
    }

    /* Clear dirty flag */
    fcb->fcbFlags &= ~FCB_DIRTY;

    return noErr;
}

/* ============================================================================
 * WDCB Management
 * ============================================================================ */

/**
 * Allocate a new WDCB
 */
WDCB* WDCB_Alloc(void)
{
    WDCB* wdcb;
    UInt16 index;

    FS_LockGlobal();

    /* Find a free WDCB */
    if (g_FSGlobals.wdcbFree >= g_FSGlobals.wdcbCount) {
        FS_UnlockGlobal();
        return NULL;
    }

    index = g_FSGlobals.wdcbFree;
    wdcb = &g_FSGlobals.wdcbArray[index];

    /* Update free list */
    g_FSGlobals.wdcbFree++;

    /* Initialize WDCB */
    memset(wdcb, 0, sizeof(WDCB));
    wdcb->wdRefNum = -(WDRefNum)(index + 1);  /* Negative reference numbers */
    wdcb->wdIndex = index;

    FS_UnlockGlobal();

    return wdcb;
}

/**
 * Free a WDCB
 */
void WDCB_Free(WDCB* wdcb)
{
    UInt16 index;

    if (!wdcb) {
        return;
    }

    FS_LockGlobal();

    /* Clear WDCB */
    index = wdcb->wdIndex;
    memset(wdcb, 0, sizeof(WDCB));
    wdcb->wdRefNum = -(WDRefNum)(index + 1);
    wdcb->wdIndex = index;

    /* Update free pointer if this is before current free */
    if (index < g_FSGlobals.wdcbFree) {
        g_FSGlobals.wdcbFree = index;
    }

    FS_UnlockGlobal();
}

/**
 * Find a WDCB by reference number
 */
WDCB* WDCB_Find(WDRefNum wdRefNum)
{
    WDCB* wdcb;
    UInt16 index;

    if (wdRefNum >= 0 || -wdRefNum > g_FSGlobals.wdcbCount) {
        return NULL;
    }

    index = (UInt16)(-wdRefNum - 1);
    wdcb = &g_FSGlobals.wdcbArray[index];

    /* Check if WDCB is in use */
    if (!wdcb->wdVCBPtr) {
        return NULL;
    }

    return wdcb;
}

/**
 * Create a new working directory
 */
OSErr WDCB_Create(VCB* vcb, UInt32 dirID, UInt32 procID, WDCB** newWDCB)
{
    WDCB* wdcb;
    CatalogDirRec dirRec;
    UInt32 hint = 0;
    OSErr err;

    if (!vcb || !newWDCB) {
        return paramErr;
    }

    *newWDCB = NULL;

    /* Verify directory exists */
    if (dirID != 2) {  /* Not root directory */
        /* Look up directory (simplified - would need to search by ID) */
        /* For now, we'll just validate it's not zero */
        if (dirID == 0) {
            return dirNFErr;
        }
    }

    /* Allocate WDCB */
    wdcb = WDCB_Alloc();
    if (!wdcb) {
        return tmwdoErr;
    }

    /* Initialize WDCB */
    wdcb->wdVCBPtr = vcb;
    wdcb->wdDirID = dirID;
    wdcb->wdCatHint = hint;
    wdcb->wdProcID = procID;

    /* Copy volume name */
    memcpy(wdcb->wdVol, vcb->vcbVN, vcb->vcbVN[0] + 1);

    *newWDCB = wdcb;

    return noErr;
}

/* ============================================================================
 * Name Utilities
 * ============================================================================ */

/**
 * Compare two Pascal strings (case-insensitive)
 */
Boolean Name_Equal(const UInt8* name1, const UInt8* name2)
{
    UInt8 len1, len2;

    if (!name1 || !name2) {
        return false;
    }

    len1 = name1[0];
    len2 = name2[0];

    if (len1 != len2) {
        return false;
    }

    /* Case-insensitive comparison */
    for (int i = 1; i <= len1; i++) {
        UInt8 c1 = name1[i];
        UInt8 c2 = name2[i];

        /* Convert to uppercase for comparison */
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;

        if (c1 != c2) {
            return false;
        }
    }

    return true;
}

/**
 * Copy a Pascal string
 */
void Name_Copy(UInt8* dst, const UInt8* src)
{
    if (!dst || !src) {
        return;
    }

    UInt8 len = src[0];
    if (len > MAX_FILENAME) {
        len = MAX_FILENAME;
    }

    memcpy(dst, src, len + 1);
}

/**
 * Generate hash for a Pascal string
 */
UInt16 Name_Hash(const UInt8* name)
{
    UInt16 hash = 0;
    UInt8 len;

    if (!name) {
        return 0;
    }

    len = name[0];

    for (int i = 1; i <= len; i++) {
        UInt8 c = name[i];

        /* Convert to uppercase */
        if (c >= 'a' && c <= 'z') c -= 32;

        hash = (hash << 1) ^ c;
    }

    return hash;
}

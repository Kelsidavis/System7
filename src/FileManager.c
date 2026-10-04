#include "MemoryMgr/MemoryManager.h"
/*
 * FileManager.c - Core File Manager Implementation
 *
 * This file implements the main File Manager APIs for System 7.1 compatibility,
 * providing HFS support on modern platforms (ARM64, x86_64).
 *
 * Copyright (c) 2024 - Implementation for System 7.1 Portable
 * Derived from System 7 ROM analysis (Ghidra) architecture
 */

#include "SystemTypes.h"
#include "MacTypes.h"
#include <stdlib.h>
#include <string.h>
#include "System71StdLib.h"
#include "FileManager_Internal.h"
#include "FS/FSLogging.h"
#include "FS/vfs.h"
#include "FS/vfs_ops.h"


/* Global file system state */
FSGlobals g_FSGlobals = {0};

/* Platform hooks (must be set by platform layer) */
PlatformHooks g_PlatformHooks = {0};

/* Internal helper macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

/*
 * CONST_CAST_STRINGPTR - Safe const-cast for FileManager parameter blocks
 * The File Manager API uses non-const StringPtr in parameter blocks for historical
 * reasons, but input filenames are never modified by the implementation. This macro
 * documents the intentional const-cast.
 */
#define CONST_CAST_STRINGPTR(s) ((StringPtr)(uintptr_t)(s))

/* ============================================================================
 * Initialization and Shutdown
 * ============================================================================ */

OSErr FM_Initialize(void)
{

    /* Check if already initialized */
    if (g_FSGlobals.initialized) {
        return noErr;
    }


    /* Allocate FCB array */
    g_FSGlobals.fcbCount = MAX_FCBS;
    g_FSGlobals.fcbArray = (FCB*)NewPtrClear((g_FSGlobals.fcbCount) * (sizeof(FCB)));
    if (!g_FSGlobals.fcbArray) {
        return memFullErr;
    }

    /* An FCB is free while fcbFlNm is 0; see FCB_Alloc. */

    /* Allocate WDCB array */
    g_FSGlobals.wdcbCount = MAX_WDCBS;
    g_FSGlobals.wdcbArray = (WDCB*)NewPtrClear((g_FSGlobals.wdcbCount) * (sizeof(WDCB)));
    if (!g_FSGlobals.wdcbArray) {
        DisposePtr((Ptr)g_FSGlobals.fcbArray);
        return memFullErr;
    }

    /* Initialize WDCB reference numbers (negative, starting from -1) */
    for (int i = 0; i < g_FSGlobals.wdcbCount; i++) {
        g_FSGlobals.wdcbArray[i].wdRefNum = (WDRefNum)(kFirstWDRefNum + i);
        g_FSGlobals.wdcbArray[i].wdIndex = i;
    }
    g_FSGlobals.wdcbFree = 0;


    /* Clear statistics */
    memset(&g_FSGlobals.bytesRead, 0, sizeof(g_FSGlobals.bytesRead));

    g_FSGlobals.initialized = true;


    return noErr;
}

OSErr FM_Shutdown(void)
{
    if (!g_FSGlobals.initialized) {
        return noErr;
    }


    /* Close all open files */
    for (int i = 0; i < g_FSGlobals.fcbCount; i++) {
        if (g_FSGlobals.fcbArray[i].base.fcbFlNm != 0) {
            FCB_Close(&g_FSGlobals.fcbArray[i]);
        }
    }

    /* Unmount all volumes */
    while (g_FSGlobals.vcbQueue) {
        VCB_Unmount(g_FSGlobals.vcbQueue);
    }


    /* Free arrays */
    DisposePtr((Ptr)g_FSGlobals.fcbArray);
    DisposePtr((Ptr)g_FSGlobals.wdcbArray);

    g_FSGlobals.initialized = false;


    return noErr;
}

/* ============================================================================
 * File Operations - Basic
 * ============================================================================ */

OSErr FSOpen(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum)
{
    return HOpenDF(vRefNum, 0, fileName, fsRdWrPerm, refNum);
}

OSErr FSClose(FileRefNum refNum)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.u.ioParam.ioRefNum = refNum;

    return PBCloseSync(&pb);
}

OSErr FSRead(FileRefNum refNum, UInt32* count, void* buffer)
{
    ParamBlockRec pb;

    if (!count || !buffer) {
        return paramErr;
    }

    memset(&pb, 0, sizeof(pb));
    pb.u.ioParam.ioRefNum = refNum;
    pb.u.ioParam.ioBuffer = buffer;
    pb.u.ioParam.ioReqCount = *count;

    OSErr err = PBReadSync(&pb);
    *count = pb.u.ioParam.ioActCount;

    return err;
}

OSErr FSWrite(FileRefNum refNum, UInt32* count, const void* buffer)
{
    ParamBlockRec pb;

    if (!count || !buffer) {
        return paramErr;
    }

    memset(&pb, 0, sizeof(pb));
    pb.u.ioParam.ioRefNum = refNum;
    pb.u.ioParam.ioBuffer = (Ptr)(uintptr_t)buffer;
    pb.u.ioParam.ioReqCount = *count;

    OSErr err = PBWriteSync(&pb);
    *count = pb.u.ioParam.ioActCount;

    return err;
}

/* ============================================================================
 * File Operations - Extended
 * ============================================================================ */

OSErr FSOpenDF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum)
{
    return HOpenDF(vRefNum, 0, fileName, fsRdWrPerm, refNum);
}

OSErr FSOpenRF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum)
{
    return HOpenRF(vRefNum, 0, fileName, fsRdWrPerm, refNum);
}

OSErr FSCreate(ConstStr255Param fileName, VolumeRefNum vRefNum, UInt32 creator, UInt32 fileType)
{
    return HCreate(vRefNum, 0, fileName, creator, fileType);
}

OSErr FSDelete(ConstStr255Param fileName, VolumeRefNum vRefNum)
{
    return HDelete(vRefNum, 0, fileName);
}

OSErr FSRename(ConstStr255Param oldName, ConstStr255Param newName, VolumeRefNum vRefNum)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(oldName);
    pb.ioVRefNum = vRefNum;
    pb.u.ioParam.ioMisc = (Ptr)CONST_CAST_STRINGPTR(newName);

    return PBHRenameSync(&pb);
}

/* ============================================================================
 * File Position and Size
 * ============================================================================ */

OSErr FSGetFPos(FileRefNum refNum, UInt32* position)
{
    FCB* fcb;

    if (!position) {
        return paramErr;
    }

    fcb = FCB_Find(refNum);
    if (!fcb) {
        return rfNumErr;
    }

    *position = fcb->base.fcbCrPs;

    return noErr;
}

OSErr FSSetFPos(FileRefNum refNum, UInt16 posMode, SInt32 posOffset)
{
    FCB* fcb;
    UInt32 newPos;

    fcb = FCB_Find(refNum);
    if (!fcb) {
        return rfNumErr;
    }


    /* Calculate new position based on mode */
    switch (posMode) {
        case fsFromStart:
            if (posOffset < 0) {
                return posErr;
            }
            newPos = (UInt32)posOffset;
            break;

        case fsFromLEOF:
            if (posOffset > 0 || (UInt32)(-posOffset) > fcb->base.fcbEOF) {
                return posErr;
            }
            newPos = fcb->base.fcbEOF + posOffset;
            break;

        case fsFromMark:
            if ((posOffset < 0 && (UInt32)(-posOffset) > fcb->base.fcbCrPs) ||
                (posOffset > 0 && fcb->base.fcbCrPs + posOffset > fcb->base.fcbEOF)) {
                return posErr;
            }
            newPos = fcb->base.fcbCrPs + posOffset;
            break;

        default:
            return paramErr;
    }

    /* Check bounds */
    if (newPos > fcb->base.fcbEOF) {
        return eofErr;
    }

    fcb->base.fcbCrPs = newPos;

    return noErr;
}

OSErr FSGetEOF(FileRefNum refNum, UInt32* eof)
{
    FCB* fcb;

    if (!eof) {
        return paramErr;
    }

    fcb = FCB_Find(refNum);
    if (!fcb) {
        return rfNumErr;
    }

    *eof = fcb->base.fcbEOF;

    return noErr;
}

OSErr FSSetEOF(FileRefNum refNum, UInt32 eof)
{
    FCB* fcb;
    OSErr err;

    fcb = FCB_Find(refNum);
    if (!fcb) {
        return rfNumErr;
    }


    /* Check write permission */
    if (!(fcb->base.fcbFlags & FCB_WRITE_PERM)) {
        return wrPermErr;
    }

    /* The fork is made that long: cut off, or extended with zeros */
    if (fcb->fcbVFSFile) {
        err = VFS_SetFileSize((VFSFile*)fcb->fcbVFSFile, eof) ? noErr : ioErr;
    } else {
        err = rfNumErr;
    }

    if (err == noErr) {
        fcb->base.fcbEOF = eof;
        fcb->base.fcbPLen = eof;
        fcb->fcbPLen = eof;
        fcb->base.fcbFlags |= FCB_DIRTY;

        /* Adjust current position if beyond new EOF */
        if (fcb->base.fcbCrPs > eof) {
            fcb->base.fcbCrPs = eof;
        }
    }


    return err;
}

/* Space is taken as a file is written; there are no blocks to reserve
 * ahead, so the request is met as asked. */
OSErr FSAllocate(FileRefNum refNum, UInt32* count)
{
    if (!count) return paramErr;
    return FCB_Find(refNum) ? noErr : rfNumErr;
}

/* ============================================================================
 * File Information
 * ============================================================================ */

OSErr FSGetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, FInfo* fndrInfo)
{
    return HGetFInfo(vRefNum, 0, fileName, fndrInfo);
}

/* HGetFInfo - High-level wrapper for getting file info by dirID */
OSErr HGetFInfo(short vRefNum, long dirID, ConstStr255Param fileName, FInfo *fndrInfo)
{
    CInfoPBRec pb;

    if (!fndrInfo) {
        return paramErr;
    }

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    pb.u.dirInfo.ioDrDirID = dirID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    OSErr err = PBGetCatInfoSync(&pb);
    if (err == noErr) {
        /* Check if it's a file (not a directory) */
        if (pb.u.hFileInfo.ioFlAttrib & kioFlAttribDir) {
            return fnfErr;  /* It's a directory, not a file */
        }
        *fndrInfo = pb.u.hFileInfo.ioFlFndrInfo;
    }

    return err;
}

OSErr FSSetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, const FInfo* fndrInfo)
{
    return HSetFInfo(vRefNum, 0, fileName, fndrInfo);
}

OSErr FSGetCatInfo(CInfoPBPtr paramBlock)
{
    if (!paramBlock) {
        return paramErr;
    }

    return PBGetCatInfoSync(paramBlock);
}

OSErr FSSetCatInfo(CInfoPBPtr paramBlock)
{
    if (!paramBlock) {
        return paramErr;
    }

    return PBSetCatInfoSync(paramBlock);
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

OSErr FSMakeFSSpec(VolumeRefNum vRefNum, DirID dirID, ConstStr255Param fileName, FSSpec* spec)
{
    VCB* vcb;
    UInt32 dir;

    if (!spec) {
        return paramErr;
    }

    /* The spec names the real volume and folder, never a working directory
     * (Inside Macintosh: Files, FSMakeFSSpec). */
    OSErr err = FM_ResolveDir(vRefNum, dirID, &vcb, &dir);
    if (err != noErr) {
        return err;
    }
    spec->vRefNum = vcb->base.vcbVRefNum;
    spec->parID = (DirID)dir;
    if (fileName && fileName[0] > 0) {
        memcpy(spec->name, fileName, fileName[0] + 1);
    } else {
        spec->name[0] = 0;
    }

    /* Whether it exists: fnfErr leaves a valid spec for a file to create. */
    CInfoPBRec pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = spec->name;
    pb.ioVRefNum = spec->vRefNum;
    pb.u.hFileInfo.ioDirID = (SInt32)dir;
    return PBGetCatInfoSync(&pb);
}

OSErr FSCreateDir(ConstStr255Param dirName, VolumeRefNum vRefNum, DirID* createdDirID)
{
    long id = 0;
    OSErr err = DirCreate(vRefNum, 0, dirName, &id);
    if (err == noErr && createdDirID) {
        *createdDirID = (DirID)id;
    }
    return err;
}

/* DirCreate - make a folder in the folder parentDirID names (0: the root). */
OSErr DirCreate(short vRefNum, long parentDirID, ConstStr255Param directoryName, long *createdDirID)
{
    VCB* vcb;
    UInt32 dir;
    OSErr err = FM_ResolveDir(vRefNum, parentDirID, &vcb, &dir);
    if (err != noErr) return err;

    UInt32 newID = 0;
    err = Cat_Create(vcb, dir, directoryName, REC_FLDR, &newID);
    if (err != noErr) return err;
    vcb->vcbDirCnt++;
    if (createdDirID) *createdDirID = (long)newID;
    return noErr;
}

OSErr FSDeleteDir(ConstStr255Param dirName, VolumeRefNum vRefNum)
{
    VCB* vcb;
    UInt32 dir;
    OSErr err = FM_ResolveDir(vRefNum, 0, &vcb, &dir);
    if (err != noErr) {
        return err;
    }

    /* A folder, and an empty one (Inside Macintosh: Files, HDelete). */
    CInfoPBRec pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(dirName);
    pb.u.hFileInfo.ioDirID = (SInt32)dir;
    err = Cat_GetInfo(vcb, dir, dirName, &pb);
    if (err != noErr) {
        return err;
    }
    if (!(pb.u.hFileInfo.ioFlAttrib & kioFlAttribDir)) {
        return dirNFErr;
    }
    return HDelete(vRefNum, 0, dirName);
}

OSErr FSGetWDInfo(WDRefNum wdRefNum, VolumeRefNum* vRefNum, DirID* dirID, UInt32* procID)
{
    WDCB* wdcb;

    wdcb = WDCB_Find(wdRefNum);
    if (!wdcb) {
        return rfNumErr;
    }

    if (vRefNum) {
        *vRefNum = wdcb->wdVCBPtr->base.vcbVRefNum;
    }

    if (dirID) {
        *dirID = wdcb->wdDirID;
    }

    if (procID) {
        *procID = wdcb->wdProcID;
    }

    return noErr;
}

OSErr FSOpenWD(VolumeRefNum vRefNum, DirID dirID, UInt32 procID, WDRefNum* wdRefNum)
{
    VCB* vcb;
    WDCB* wdcb;
    OSErr err;

    if (!wdRefNum) {
        return paramErr;
    }

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    /* Create a new WDCB */
    err = WDCB_Create(vcb, dirID, procID, &wdcb);
    if (err != noErr) {
        return err;
    }

    *wdRefNum = wdcb->wdRefNum;

    return noErr;
}

OSErr FSCloseWD(WDRefNum wdRefNum)
{
    WDCB* wdcb;

    wdcb = WDCB_Find(wdRefNum);
    if (!wdcb) {
        return rfNumErr;
    }

    WDCB_Free(wdcb);

    return noErr;
}

/* ============================================================================
 * Volume Operations
 * ============================================================================ */

OSErr FSUnmount(VolumeRefNum vRefNum)
{
    VCB* vcb;
    OSErr err;

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    /* Close all files on this volume */
    for (int i = 0; i < g_FSGlobals.fcbCount; i++) {
        FCB* fcb = &g_FSGlobals.fcbArray[i];
        if ((VCB*)fcb->base.fcbVPtr == vcb) {
            FCB_Close(fcb);
        }
    }

    /* Unmount the volume */
    return VCB_Unmount(vcb);
}

OSErr FSEject(VolumeRefNum vRefNum)
{
    VCB* vcb;
    OSErr err;

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    /* Flush and unmount */
    err = VCB_Flush(vcb);
    if (err != noErr) {
        return err;
    }

    /* Platform-specific eject */
    if (g_PlatformHooks.DeviceEject) {
        err = g_PlatformHooks.DeviceEject(vcb->vcbDevice);
    }

    /* Mark as offline */
    vcb->base.vcbAtrb |= kioVAtrbOffline;

    return err;
}

OSErr FSFlushVol(ConstStr255Param volName, VolumeRefNum vRefNum)
{
    VCB* vcb;
    OSErr err;

    if (volName && volName[0] > 0) {
        /* Find by name */
        vcb = VCB_FindByName(volName);
        if (!vcb) {
            return nsvErr;
        }
    } else {
        /* Find by reference number */
        err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
        if (err != noErr) {
            return err;
        }
    }

    return VCB_Flush(vcb);
}

OSErr FSGetVInfo(VolumeRefNum vRefNum, StringPtr volName, UInt16* vRefNumOut, UInt32* freeBytes)
{
    VCB* vcb;
    OSErr err;

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }


    if (volName) {
        memcpy(volName, vcb->base.vcbVN, vcb->base.vcbVN[0] + 1);
    }

    if (vRefNumOut) {
        *vRefNumOut = vcb->base.vcbVRefNum;
    }

    if (freeBytes) {
        *freeBytes = (UInt32)vcb->base.vcbFreeBks * vcb->base.vcbAlBlkSiz;
    }


    return noErr;
}

OSErr FSSetVol(ConstStr255Param volName, VolumeRefNum vRefNum)
{
    VCB* vcb;
    OSErr err;

    if (volName && volName[0] > 0) {
        /* Find by name */
        vcb = VCB_FindByName(volName);
        if (!vcb) {
            return nsvErr;
        }
        g_FSGlobals.defVRefNum = vcb->base.vcbVRefNum;
    } else if (vRefNum != 0) {
        /* Find by reference number */
        err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
        if (err != noErr) {
            return err;
        }
        g_FSGlobals.defVRefNum = vRefNum;
    }

    return noErr;
}

OSErr FSGetVol(StringPtr volName, VolumeRefNum* vRefNum)
{
    VCB* vcb;
    OSErr err;

    if (!vRefNum) {
        return paramErr;
    }

    /* Get default volume */
    err = FM_GetVolumeFromRefNum(g_FSGlobals.defVRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    *vRefNum = g_FSGlobals.defVRefNum;

    if (volName) {
        memcpy(volName, vcb->base.vcbVN, vcb->base.vcbVN[0] + 1);
    }

    return noErr;
}

/* ============================================================================
 * Parameter Block Operations - Synchronous
 * ============================================================================ */

OSErr PBOpenSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    FCB* fcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, 0, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Open the file */
    err = FCB_Open(vcb, dir, paramBlock->ioNamePtr,
                   (paramBlock)->u.ioParam.ioPermssn, false, &fcb);

    if (err == noErr) {
        (paramBlock)->u.ioParam.ioRefNum = fcb->fcbRefNum;
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBCloseSync(ParmBlkPtr paramBlock)
{
    FCB* fcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    fcb = FCB_Find((paramBlock)->u.ioParam.ioRefNum);
    if (!fcb) {
        err = rfNumErr;
    } else {
        err = FCB_Close(fcb);
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBReadSync(ParmBlkPtr paramBlock)
{
    FCB* fcb;
    OSErr err;
    UInt32 actualCount;

    if (!paramBlock) {
        return paramErr;
    }

    fcb = FCB_Find((paramBlock)->u.ioParam.ioRefNum);
    if (!fcb) {
        err = rfNumErr;
    } else {
        /* Handle positioning */
        if ((paramBlock)->u.ioParam.ioPosMode != fsAtMark) {
            err = FSSetFPos(fcb->fcbRefNum, (paramBlock)->u.ioParam.ioPosMode,
                          (paramBlock)->u.ioParam.ioPosOffset);
            if (err != noErr) {
                paramBlock->ioResult = err;
                return err;
            }
        }

        /* Read the data */
        err = IO_ReadFork(fcb, fcb->base.fcbCrPs, (paramBlock)->u.ioParam.ioReqCount,
                         (paramBlock)->u.ioParam.ioBuffer, &actualCount);

        (paramBlock)->u.ioParam.ioActCount = actualCount;
        (paramBlock)->u.ioParam.ioPosOffset = fcb->base.fcbCrPs;

        /* Update statistics */
        g_FSGlobals.bytesRead += actualCount;
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBWriteSync(ParmBlkPtr paramBlock)
{
    FCB* fcb;
    OSErr err;
    UInt32 actualCount;

    if (!paramBlock) {
        return paramErr;
    }

    fcb = FCB_Find((paramBlock)->u.ioParam.ioRefNum);
    if (!fcb) {
        err = rfNumErr;
    } else {
        /* Check write permission */
        if (!(fcb->base.fcbFlags & FCB_WRITE_PERM)) {
            err = wrPermErr;
        } else {
            /* Handle positioning */
            if ((paramBlock)->u.ioParam.ioPosMode != fsAtMark) {
                err = FSSetFPos(fcb->fcbRefNum, (paramBlock)->u.ioParam.ioPosMode,
                              (paramBlock)->u.ioParam.ioPosOffset);
                if (err != noErr) {
                    paramBlock->ioResult = err;
                    return err;
                }
            }

            /* Write the data */
            err = IO_WriteFork(fcb, fcb->base.fcbCrPs, (paramBlock)->u.ioParam.ioReqCount,
                             (paramBlock)->u.ioParam.ioBuffer, &actualCount);

            (paramBlock)->u.ioParam.ioActCount = actualCount;
            (paramBlock)->u.ioParam.ioPosOffset = fcb->base.fcbCrPs;

            /* Update statistics */
            g_FSGlobals.bytesWritten += actualCount;
        }
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBGetCatInfoSync(CInfoPBPtr paramBlock)
{
    VCB* vcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, paramBlock->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }
    paramBlock->u.hFileInfo.ioDirID = (SInt32)dir;

    /* Get catalog info */
    err = Cat_GetInfo(vcb, dir, paramBlock->ioNamePtr, paramBlock);

    paramBlock->ioResult = err;
    return err;
}

OSErr PBSetCatInfoSync(CInfoPBPtr paramBlock)
{
    VCB* vcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, paramBlock->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }
    paramBlock->u.hFileInfo.ioDirID = (SInt32)dir;

    /* Set catalog info */
    err = Cat_SetInfo(vcb, dir, paramBlock->ioNamePtr, paramBlock);

    paramBlock->ioResult = err;
    return err;
}

/* The asynchronous entry points below currently perform the operation
 * synchronously and return its result directly. */

OSErr PBOpenAsync(ParmBlkPtr paramBlock)
{
    return PBOpenSync(paramBlock);
}

OSErr PBCloseAsync(ParmBlkPtr paramBlock)
{
    return PBCloseSync(paramBlock);
}

OSErr PBReadAsync(ParmBlkPtr paramBlock)
{
    return PBReadSync(paramBlock);
}

OSErr PBWriteAsync(ParmBlkPtr paramBlock)
{
    return PBWriteSync(paramBlock);
}

OSErr PBGetCatInfoAsync(CInfoPBPtr paramBlock)
{
    return PBGetCatInfoSync(paramBlock);
}

OSErr PBSetCatInfoAsync(CInfoPBPtr paramBlock)
{
    return PBSetCatInfoSync(paramBlock);
}

/* ============================================================================
 * HFS-specific Parameter Block Operations
 * ============================================================================ */

OSErr PBHOpenDFSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    FCB* fcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Open data fork */
    err = FCB_Open(vcb, dir,
                   paramBlock->ioNamePtr,
                   (paramBlock)->u.ioParam.ioPermssn, false, &fcb);

    if (err == noErr) {
        (paramBlock)->u.ioParam.ioRefNum = fcb->fcbRefNum;
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBHOpenRFSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    FCB* fcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Open resource fork */
    err = FCB_Open(vcb, dir,
                   paramBlock->ioNamePtr,
                   (paramBlock)->u.ioParam.ioPermssn, true, &fcb);

    if (err == noErr) {
        (paramBlock)->u.ioParam.ioRefNum = fcb->fcbRefNum;
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBHCreateSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Create the file in the catalog */
    err = Cat_Create(vcb, dir,
                    paramBlock->ioNamePtr, REC_FIL, NULL);

    /* Update volume file count */
    if (err == noErr) {
        vcb->vcbFilCnt++;
        vcb->base.vcbFlags |= VCB_DIRTY;
    }


    paramBlock->ioResult = err;
    return err;
}

OSErr PBHDeleteSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }


    /* Delete from catalog */
    err = Cat_Delete(vcb, dir,
                    paramBlock->ioNamePtr);

    /* Update volume file count */
    if (err == noErr) {
        vcb->vcbFilCnt--;
        vcb->base.vcbFlags |= VCB_DIRTY;
    }


    paramBlock->ioResult = err;
    return err;
}

OSErr PBHRenameSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    OSErr err;

    if (!paramBlock) {
        return paramErr;
    }

    UInt32 dir;
    err = FM_ResolveDir(paramBlock->ioVRefNum, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID, &vcb, &dir);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }


    /* Rename in catalog */
    err = Cat_Rename(vcb, dir,
                     paramBlock->ioNamePtr,
                     (const UInt8*)(paramBlock)->u.ioParam.ioMisc);


    paramBlock->ioResult = err;
    return err;
}

/* Async versions */
OSErr PBHOpenDFAsync(ParmBlkPtr paramBlock)
{
    return PBHOpenDFSync(paramBlock);
}

OSErr PBHOpenRFAsync(ParmBlkPtr paramBlock)
{
    return PBHOpenRFSync(paramBlock);
}

OSErr PBHCreateAsync(ParmBlkPtr paramBlock)
{
    return PBHCreateSync(paramBlock);
}

OSErr PBHDeleteAsync(ParmBlkPtr paramBlock)
{
    return PBHDeleteSync(paramBlock);
}

OSErr PBHRenameAsync(ParmBlkPtr paramBlock)
{
    return PBHRenameSync(paramBlock);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* The volume and folder a vRefNum and dirID name, as Inside Macintosh:
 * Files has it: vRefNum is a volume, or a working directory standing for
 * one of its folders (or 0, the default volume); a dirID of 0 means that
 * working directory's folder, or else the volume's root. */
OSErr FM_ResolveDir(short vRefNum, long dirID, VCB** vcb, UInt32* dir)
{
    if (!vcb || !dir) {
        return paramErr;
    }
    WDCB* wd = WDCB_Find(vRefNum);
    if (wd) {
        *vcb = wd->wdVCBPtr;
        *dir = dirID ? (UInt32)dirID : wd->wdDirID;
        return noErr;
    }
    OSErr err = FM_GetVolumeFromRefNum(vRefNum, vcb);
    if (err == noErr) {
        *dir = dirID ? (UInt32)dirID : 2;
    }
    return err;
}

OSErr FM_GetVolumeFromRefNum(VolumeRefNum vRefNum, VCB** vcb)
{
    if (!vcb) {
        return paramErr;
    }

    /* Use default volume if vRefNum is 0 */
    if (vRefNum == 0) {
        vRefNum = g_FSGlobals.defVRefNum;
    }

    *vcb = VCB_Find(vRefNum);
    if (!*vcb) {
        return nsvErr;
    }

    return noErr;
}

OSErr FM_GetFCBFromRefNum(FileRefNum refNum, FCB** fcb)
{
    if (!fcb) {
        return paramErr;
    }

    *fcb = FCB_Find(refNum);
    if (!*fcb) {
        return rfNumErr;
    }

    return noErr;
}

Boolean FM_IsDirectory(const FSSpec* spec)
{
    CInfoPBRec pb;

    if (!spec) {
        return false;
    }

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(spec->name);
    pb.ioVRefNum = spec->vRefNum;
    pb.u.dirInfo.ioDrDirID = spec->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    if (PBGetCatInfoSync(&pb) != noErr) {
        return false;
    }

    return (pb.u.hFileInfo.ioFlAttrib & kioFlAttribDir) != 0;
}

/* ============================================================================
 * Process Manager Integration
 * ============================================================================ */

OSErr FM_SetProcessOwner(FileRefNum refNum, UInt32 processID)
{
    FCB* fcb;

    fcb = FCB_Find(refNum);
    if (!fcb) {
        return rfNumErr;
    }

    fcb->fcbProcessID = processID;

    return noErr;
}

OSErr FM_ReleaseProcessFiles(UInt32 processID)
{


    /* Close all files owned by this process */
    for (int i = 0; i < g_FSGlobals.fcbCount; i++) {
        FCB* fcb = &g_FSGlobals.fcbArray[i];
        if (fcb->base.fcbFlNm != 0 && fcb->fcbProcessID == processID) {
            FCB_Close(fcb);
        }
    }

    /* Close all working directories owned by this process */
    for (int i = 0; i < g_FSGlobals.wdcbCount; i++) {
        WDCB* wdcb = &g_FSGlobals.wdcbArray[i];
        if (wdcb->wdVCBPtr && wdcb->wdProcID == processID) {
            WDCB_Free(wdcb);
        }
    }


    return noErr;
}


/* ============================================================================
 * Error Mapping
 * ============================================================================ */

OSErr Error_Map(int platformError)
{
    switch (platformError) {
        case ENOENT:    return fnfErr;
        case EACCES:    return permErr;
        case EEXIST:    return dupFNErr;
        case ENOTDIR:   return dirNFErr;
        case EISDIR:    return notAFileErr;
        case ENOSPC:    return dskFulErr;
        case EROFS:     return wPrErr;
        case EMFILE:    return tmfoErr;
        case ENOMEM:    return memFullErr;
        case EIO:       return ioErr;
        default:        return ioErr;
    }
}

const char* Error_String(OSErr err)
{
    switch (err) {
        case noErr:         return "No error";
        case fnfErr:        return "File not found";
        case vLckdErr:      return "Volume is locked";
        case fBsyErr:       return "File is busy";
        case dupFNErr:      return "Duplicate filename";
        case opWrErr:       return "File already open for writing";
        case paramErr:      return "Invalid parameter";
        case rfNumErr:      return "Invalid reference number";
        case volOffLinErr:  return "Volume is offline";
        case permErr:       return "Permission error";
        case nsvErr:        return "No such volume";
        case ioErr:         return "I/O error";
        case bdNamErr:      return "Bad filename";
        case fnOpnErr:      return "File not open";
        case eofErr:        return "End of file";
        case posErr:        return "Invalid position";
        case mFulErr:       return "Memory full";
        case tmfoErr:       return "Too many files open";
        case wPrErr:        return "Disk is write-protected";
        case fLckdErr:      return "File is locked";
        case dskFulErr:     return "Disk full";
        case dirNFErr:      return "Directory not found";
        case tmwdoErr:      return "Too many working directories";
        default:            return "Unknown error";
    }
}

/* ============================================================================
 * Debug Support
 * ============================================================================ */

void FM_GetStatistics(void* stats)
{
    (void)stats;
    /* Copy statistics structure */
    /* Implementation would fill in a statistics structure */
}

void FM_DumpVolumeInfo(VolumeRefNum vRefNum)
{
    VCB* vcb;
    OSErr err;

    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        FS_LOG_DEBUG("Volume not found: %d\n", vRefNum);
        return;
    }

    FS_LOG_DEBUG("Volume Info:\n");
    FS_LOG_DEBUG("  Name: %.*s\n", vcb->base.vcbVN[0], &vcb->base.vcbVN[1]);
    FS_LOG_DEBUG("  VRefNum: %d\n", vcb->base.vcbVRefNum);
    FS_LOG_DEBUG("  Signature: 0x%04X\n", vcb->base.vcbSigWord);
    FS_LOG_DEBUG("  Files: %u\n", (unsigned)vcb->vcbFilCnt);
    FS_LOG_DEBUG("  Directories: %u\n", (unsigned)vcb->vcbDirCnt);
    FS_LOG_DEBUG("  Free blocks: %u\n", vcb->base.vcbFreeBks);
    FS_LOG_DEBUG("  Block size: %u\n", (unsigned)vcb->base.vcbAlBlkSiz);
}

void FM_DumpOpenFiles(void)
{
    int openCount = 0;

    FS_LOG_DEBUG("Open Files:\n");

    for (int i = 0; i < g_FSGlobals.fcbCount; i++) {
        FCB* fcb = &g_FSGlobals.fcbArray[i];
        if (fcb->base.fcbFlNm != 0) {
            FS_LOG_DEBUG("  RefNum %d: File %u, Pos %u, EOF %u\n",
                   fcb->fcbRefNum,
                   (unsigned)fcb->base.fcbFlNm,
                   (unsigned)fcb->base.fcbCrPs,
                   (unsigned)fcb->base.fcbEOF);
            openCount++;
        }
    }

    FS_LOG_DEBUG("Total open files: %d\n", openCount);
}

/* ============================================================================
 * Calls that name a file by volume, directory and name (Inside Macintosh:
 * Files, "High-Level File Access Routines"), and the FSSpec calls on them.
 *
 * A directory ID of 0 means the volume's root. The FSSpec calls used to go
 * through FSCreate, FSOpen and FSDelete, which take no directory, so every
 * one of them acted on the root whatever folder the spec named.
 * ============================================================================ */

/* A pb naming dirID:name on vRefNum, for Cat_GetInfo and Cat_SetInfo. */
static void H_NamePB(CInfoPBRec* pb, short vRefNum, long dirID, ConstStr255Param name)
{
    memset(pb, 0, sizeof(*pb));
    pb->ioNamePtr = CONST_CAST_STRINGPTR(name);
    pb->ioVRefNum = vRefNum;
    pb->u.hFileInfo.ioDirID = dirID;
}

OSErr HSetFInfo(short vRefNum, long dirID, ConstStr255Param fileName, const FInfo* fndrInfo)
{
    VCB* vcb;
    UInt32 dir;
    if (!fndrInfo) return paramErr;
    OSErr err = FM_ResolveDir(vRefNum, dirID, &vcb, &dir);
    if (err != noErr) return err;
    CInfoPBRec pb;
    H_NamePB(&pb, vRefNum, dir, fileName);
    pb.u.hFileInfo.ioFlFndrInfo = *fndrInfo;
    return Cat_SetInfo(vcb, dir, fileName, &pb);
}

OSErr HCreate(short vRefNum, long dirID, ConstStr255Param fileName, OSType creator, OSType fileType)
{
    VCB* vcb;
    UInt32 dir;
    OSErr err = FM_ResolveDir(vRefNum, dirID, &vcb, &dir);
    if (err != noErr) return err;
    err = Cat_Create(vcb, dir, fileName, REC_FIL, NULL);
    if (err != noErr) return err;
    vcb->vcbFilCnt++;

    FInfo info;
    memset(&info, 0, sizeof(info));
    info.fdType = fileType;
    info.fdCreator = creator;
    return HSetFInfo(vRefNum, (long)dir, fileName, &info);
}

static OSErr H_OpenFork(short vRefNum, long dirID, ConstStr255Param fileName,
                        SInt8 permission, Boolean resourceFork, short* refNum)
{
    VCB* vcb;
    FCB* fcb;
    UInt32 dir;
    if (!refNum) return paramErr;
    OSErr err = FM_ResolveDir(vRefNum, dirID, &vcb, &dir);
    if (err != noErr) return err;
    err = FCB_Open(vcb, dir, fileName, (UInt8)permission, resourceFork, &fcb);
    if (err == noErr) *refNum = fcb->fcbRefNum;
    return err;
}

OSErr HOpenDF(short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission, short* refNum)
{
    return H_OpenFork(vRefNum, dirID, fileName, permission, false, refNum);
}

OSErr HOpenRF(short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission, short* refNum)
{
    return H_OpenFork(vRefNum, dirID, fileName, permission, true, refNum);
}

/* A file, or a folder with nothing in it. */
OSErr HDelete(short vRefNum, long dirID, ConstStr255Param fileName)
{
    VCB* vcb;
    UInt32 dir;
    OSErr err = FM_ResolveDir(vRefNum, dirID, &vcb, &dir);
    if (err != noErr) return err;

    CInfoPBRec pb;
    H_NamePB(&pb, vRefNum, dir, fileName);
    err = Cat_GetInfo(vcb, dir, fileName, &pb);
    if (err != noErr) return err;
    Boolean isDir = (pb.u.hFileInfo.ioFlAttrib & kioFlAttribDir) != 0;
    if (isDir && pb.u.dirInfo.ioDrNmFls > 0) return fBsyErr;

    err = Cat_Delete(vcb, dir, fileName);
    if (err == noErr) {
        if (isDir) { if (vcb->vcbDirCnt) vcb->vcbDirCnt--; }
        else       { if (vcb->vcbFilCnt) vcb->vcbFilCnt--; }
    }
    return err;
}

OSErr FSpCreate(const FSSpec* spec, OSType creator, OSType fileType, ScriptCode scriptTag)
{
    (void)scriptTag;
    if (!spec) return paramErr;
    return HCreate(spec->vRefNum, spec->parID, spec->name, creator, fileType);
}

OSErr FSpOpenDF(const FSSpec* spec, SInt8 permission, FileRefNum* refNum)
{
    if (!spec) return paramErr;
    return HOpenDF(spec->vRefNum, spec->parID, spec->name, permission, refNum);
}

OSErr FSpOpenRF(const FSSpec* spec, SInt8 permission, FileRefNum* refNum)
{
    if (!spec) return paramErr;
    return HOpenRF(spec->vRefNum, spec->parID, spec->name, permission, refNum);
}

OSErr FSpDelete(const FSSpec* spec)
{
    if (!spec) return paramErr;
    return HDelete(spec->vRefNum, spec->parID, spec->name);
}

OSErr FSpGetFInfo(const FSSpec* spec, FInfo* fndrInfo)
{
    if (!spec) return paramErr;
    return HGetFInfo(spec->vRefNum, spec->parID, spec->name, fndrInfo);
}

OSErr FSpSetFInfo(const FSSpec* spec, const FInfo* fndrInfo)
{
    if (!spec) return paramErr;
    return HSetFInfo(spec->vRefNum, spec->parID, spec->name, fndrInfo);
}

OSErr FSpDirCreate(const FSSpec* spec, ScriptCode scriptTag, long* createdDirID)
{
    (void)scriptTag;
    if (!spec) return paramErr;
    return DirCreate(spec->vRefNum, spec->parID, spec->name, createdDirID);
}

/* Move a file or folder into the folder `dest` names, on the same volume.
 * An empty dest name means dest->parID is the folder itself. */
OSErr FSpCatMove(const FSSpec* source, const FSSpec* dest)
{
    if (!source || !dest) return paramErr;
    if (source->vRefNum != dest->vRefNum) return diffVolErr;

    char name[32];
    UInt8 len = source->name[0] > 31 ? 31 : source->name[0];
    memcpy(name, &source->name[1], len);
    name[len] = '\0';

    CatEntry src;
    if (!VFS_Lookup(source->vRefNum, source->parID, name, &src)) return fnfErr;

    DirID target = dest->parID;
    if (dest->name[0] != 0) {
        char dname[32];
        UInt8 dlen = dest->name[0] > 31 ? 31 : dest->name[0];
        memcpy(dname, &dest->name[1], dlen);
        dname[dlen] = '\0';
        CatEntry folder;
        if (!VFS_Lookup(dest->vRefNum, dest->parID, dname, &folder)) return dirNFErr;
        if (folder.kind != kNodeDir) return dirNFErr;
        target = (DirID)folder.id;
    }

    return VFS_Move(source->vRefNum, source->parID, src.id, target, NULL) ? noErr : ioErr;
}

/*
 * Volume size in 512-byte allocation blocks. A volume that is not mounted is
 * nsvErr; this used to answer noErr with an invented 800-block volume.
 */
OSErr PBHGetVInfoSync(void* paramBlock)
{
    if (!paramBlock) return paramErr;
    HParamBlockRec* pb = (HParamBlockRec*)paramBlock;
    VRefNum vref = pb->ioVRefNum ? (VRefNum)pb->ioVRefNum : (VRefNum)g_FSGlobals.defVRefNum;

    VolumeControlBlock info;
    if (!VFS_GetVolumeInfo(vref, &info)) {
        pb->ioResult = nsvErr;
        return nsvErr;
    }
    pb->u.volumeParam.ioVAlBlkSiz = 512;
    pb->u.volumeParam.ioVNmAlBlks = (UInt32)(info.totalBytes / 512);
    pb->ioResult = noErr;
    return noErr;
}

/* The classic names for setting and reading a file's length. */
OSErr SetEOF(short refNum, long logEOF)
{
    if (logEOF < 0) return paramErr;
    return FSSetEOF(refNum, (UInt32)logEOF);
}

OSErr GetEOF(short refNum, long* logEOF)
{
    if (!logEOF) return paramErr;
    UInt32 eof;
    OSErr err = FSGetEOF(refNum, &eof);
    if (err == noErr) *logEOF = (long)eof;
    return err;
}

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
#include <time.h>
#include <sched.h>
#if defined(__i386__) || defined(__x86_64__)
#include "Platform/x86/rtc.h"
#endif
#include "System71StdLib.h"
#include "FileManager_Internal.h"
#include "FS/FSLogging.h"


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

/* Convert between Mac and Unix timestamps */
#define MAC_EPOCH_OFFSET 2082844800UL  /* Seconds between 1904 and 1970 */

/* ============================================================================
 * Initialization and Shutdown
 * ============================================================================ */

OSErr FM_Initialize(void)
{
    OSErr err;

    /* Check if already initialized */
    if (g_FSGlobals.initialized) {
        return noErr;
    }

    /* Initialize global mutex */
#ifdef PLATFORM_REMOVED_WIN32
    InitializeCriticalSection(&g_FSGlobals.globalMutex);
#else
    pthread_mutex_init(&g_FSGlobals.globalMutex, NULL);
#endif

    FS_LockGlobal();

    /* Allocate FCB array */
    g_FSGlobals.fcbCount = MAX_FCBS;
    g_FSGlobals.fcbArray = (FCB*)NewPtrClear((g_FSGlobals.fcbCount) * (sizeof(FCB)));
    if (!g_FSGlobals.fcbArray) {
        FS_UnlockGlobal();
        return memFullErr;
    }

    /* Initialize FCB free list */
    for (int i = 0; i < g_FSGlobals.fcbCount - 1; i++) {
        g_FSGlobals.fcbArray[i].fcbRefNum = (FileRefNum)(i + 1);
    }
    g_FSGlobals.fcbArray[g_FSGlobals.fcbCount - 1].fcbRefNum = -1;
    g_FSGlobals.fcbFree = 0;

    /* Allocate WDCB array */
    g_FSGlobals.wdcbCount = MAX_WDCBS;
    g_FSGlobals.wdcbArray = (WDCB*)NewPtrClear((g_FSGlobals.wdcbCount) * (sizeof(WDCB)));
    if (!g_FSGlobals.wdcbArray) {
        DisposePtr((Ptr)g_FSGlobals.fcbArray);
        FS_UnlockGlobal();
        return memFullErr;
    }

    /* Initialize WDCB reference numbers (negative, starting from -1) */
    for (int i = 0; i < g_FSGlobals.wdcbCount; i++) {
        g_FSGlobals.wdcbArray[i].wdRefNum = -(WDRefNum)(i + 1);
        g_FSGlobals.wdcbArray[i].wdIndex = i;
    }
    g_FSGlobals.wdcbFree = 0;

    /* Initialize cache system */
    err = Cache_Init(1024);  /* 1024 blocks = 512KB default cache */
    if (err != noErr) {
        DisposePtr((Ptr)g_FSGlobals.fcbArray);
        DisposePtr((Ptr)g_FSGlobals.wdcbArray);
        FS_UnlockGlobal();
        return err;
    }

    /* Clear statistics */
    memset(&g_FSGlobals.bytesRead, 0, sizeof(g_FSGlobals.bytesRead));

    g_FSGlobals.initialized = true;

    FS_UnlockGlobal();

    return noErr;
}

OSErr FM_Shutdown(void)
{
    if (!g_FSGlobals.initialized) {
        return noErr;
    }

    FS_LockGlobal();

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

    /* Shutdown cache */
    Cache_Shutdown();

    /* Free arrays */
    DisposePtr((Ptr)g_FSGlobals.fcbArray);
    DisposePtr((Ptr)g_FSGlobals.wdcbArray);

    g_FSGlobals.initialized = false;

    FS_UnlockGlobal();

    /* Destroy global mutex */
#ifdef PLATFORM_REMOVED_WIN32
    DeleteCriticalSection(&g_FSGlobals.globalMutex);
#else
    pthread_mutex_destroy(&g_FSGlobals.globalMutex);
#endif

    return noErr;
}

/* ============================================================================
 * File Operations - Basic
 * ============================================================================ */

OSErr FSOpen(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    pb.u.ioParam.ioPermssn = fsRdWrPerm;

    OSErr err = PBOpenSync(&pb);
    if (err == noErr && refNum) {
        *refNum = pb.u.ioParam.ioRefNum;
    }

    return err;
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
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    pb.u.ioParam.ioPermssn = fsRdWrPerm;
    /* pb.u.fileParam.ioDirID = 0; -- FileParam doesn't have ioDirID */  /* Use default directory */

    OSErr err = PBHOpenDFSync(&pb);
    if (err == noErr && refNum) {
        *refNum = pb.u.ioParam.ioRefNum;
    }

    return err;
}

OSErr FSOpenRF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    pb.u.ioParam.ioPermssn = fsRdWrPerm;
    /* pb.u.fileParam.ioDirID = 0; -- FileParam doesn't have ioDirID */

    OSErr err = PBHOpenRFSync(&pb);
    if (err == noErr && refNum) {
        *refNum = pb.u.ioParam.ioRefNum;
    }

    return err;
}

OSErr FSCreate(ConstStr255Param fileName, VolumeRefNum vRefNum, UInt32 creator, UInt32 fileType)
{
    ParamBlockRec pb;

    /* First create the file */
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    /* pb.u.fileParam.ioDirID = 0; -- FileParam doesn't have ioDirID */

    OSErr err = PBHCreateSync(&pb);
    if (err != noErr) {
        return err;
    }

    /* Then set its type and creator */
    FInfo fndrInfo;
    err = FSGetFInfo(fileName, vRefNum, &fndrInfo);
    if (err == noErr) {
        fndrInfo.fdType = fileType;
        fndrInfo.fdCreator = creator;
        err = FSSetFInfo(fileName, vRefNum, &fndrInfo);
    }

    return err;
}

OSErr FSDelete(ConstStr255Param fileName, VolumeRefNum vRefNum)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    /* pb.u.fileParam.ioDirID = 0; -- FileParam doesn't have ioDirID */

    return PBHDeleteSync(&pb);
}

OSErr FSRename(ConstStr255Param oldName, ConstStr255Param newName, VolumeRefNum vRefNum)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(oldName);
    pb.ioVRefNum = vRefNum;
    pb.u.ioParam.ioMisc = CONST_CAST_STRINGPTR(newName);
    /* pb.u.fileParam.ioDirID = 0; -- FileParam doesn't have ioDirID */

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

    FS_LockFCB(fcb);
    *position = fcb->base.fcbCrPs;
    FS_UnlockFCB(fcb);

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

    FS_LockFCB(fcb);

    /* Calculate new position based on mode */
    switch (posMode) {
        case fsFromStart:
            if (posOffset < 0) {
                FS_UnlockFCB(fcb);
                return posErr;
            }
            newPos = (UInt32)posOffset;
            break;

        case fsFromLEOF:
            if (posOffset > 0 || (UInt32)(-posOffset) > fcb->base.fcbEOF) {
                FS_UnlockFCB(fcb);
                return posErr;
            }
            newPos = fcb->base.fcbEOF + posOffset;
            break;

        case fsFromMark:
            if ((posOffset < 0 && (UInt32)(-posOffset) > fcb->base.fcbCrPs) ||
                (posOffset > 0 && fcb->base.fcbCrPs + posOffset > fcb->base.fcbEOF)) {
                FS_UnlockFCB(fcb);
                return posErr;
            }
            newPos = fcb->base.fcbCrPs + posOffset;
            break;

        default:
            FS_UnlockFCB(fcb);
            return paramErr;
    }

    /* Check bounds */
    if (newPos > fcb->base.fcbEOF) {
        FS_UnlockFCB(fcb);
        return eofErr;
    }

    fcb->base.fcbCrPs = newPos;
    FS_UnlockFCB(fcb);

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

    FS_LockFCB(fcb);
    *eof = fcb->base.fcbEOF;
    FS_UnlockFCB(fcb);

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

    FS_LockFCB(fcb);

    /* Check write permission */
    if (!(fcb->base.fcbFlags & FCB_WRITE_PERM)) {
        FS_UnlockFCB(fcb);
        return wrPermErr;
    }

    /* Extend or truncate as needed */
    if (eof > fcb->base.fcbEOF) {
        err = Ext_Extend((VCBExt*)fcb->base.fcbVPtr, fcb, eof);
    } else if (eof < fcb->base.fcbEOF) {
        err = Ext_Truncate((VCBExt*)fcb->base.fcbVPtr, fcb, eof);
    } else {
        err = noErr;
    }

    if (err == noErr) {
        fcb->base.fcbEOF = eof;
        fcb->base.fcbFlags |= FCB_DIRTY;

        /* Adjust current position if beyond new EOF */
        if (fcb->base.fcbCrPs > eof) {
            fcb->base.fcbCrPs = eof;
        }
    }

    FS_UnlockFCB(fcb);

    return err;
}

OSErr FSAllocate(FileRefNum refNum, UInt32* count)
{
    FCB* fcb;
    OSErr err;
    UInt32 newSize;

    if (!count) {
        return paramErr;
    }

    fcb = FCB_Find(refNum);
    if (!fcb) {
        return rfNumErr;
    }

    FS_LockFCB(fcb);

    /* Check write permission */
    if (!(fcb->base.fcbFlags & FCB_WRITE_PERM)) {
        FS_UnlockFCB(fcb);
        return wrPermErr;
    }

    /* Calculate new size */
    newSize = fcb->fcbPLen + *count;

    /* Try to allocate the space */
    err = Ext_Extend((VCBExt*)fcb->base.fcbVPtr, fcb, newSize);
    if (err == noErr) {
        *count = newSize - fcb->fcbPLen;
        fcb->fcbPLen = newSize;
        fcb->base.fcbFlags |= FCB_DIRTY;
    } else {
        *count = 0;
    }

    FS_UnlockFCB(fcb);

    return err;
}

/* ============================================================================
 * File Information
 * ============================================================================ */

OSErr FSGetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, FInfo* fndrInfo)
{
    CInfoPBRec pb;

    if (!fndrInfo) {
        return paramErr;
    }

    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    pb.u.hFileInfo.ioFDirIndex = 0;

    OSErr err = PBGetCatInfoSync(&pb);
    if (err == noErr) {
        *fndrInfo = pb.u.hFileInfo.ioFlFndrInfo;
    }

    return err;
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
    CInfoPBRec pb;

    if (!fndrInfo) {
        return paramErr;
    }

    /* First get current info */
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = CONST_CAST_STRINGPTR(fileName);
    pb.ioVRefNum = vRefNum;
    pb.u.hFileInfo.ioFDirIndex = 0;

    OSErr err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return err;
    }

    /* Update Finder info */
    pb.u.hFileInfo.ioFlFndrInfo = *fndrInfo;

    /* Write back */
    return PBSetCatInfoSync(&pb);
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
    OSErr err;

    if (!spec) {
        return paramErr;
    }

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    /* Fill in the FSSpec */
    spec->vRefNum = vRefNum;
    spec->parID = dirID;

    if (fileName && fileName[0] > 0) {
        memcpy(spec->name, fileName, fileName[0] + 1);
    } else {
        spec->name[0] = 0;
    }

    /* Verify the file/directory exists */
    CInfoPBRec pb;
    memset(&pb, 0, sizeof(pb));
    pb.ioNamePtr = spec->name;
    pb.ioVRefNum = spec->vRefNum;
    pb.u.dirInfo.ioDrDirID = spec->parID;
    pb.u.hFileInfo.ioFDirIndex = 0;

    err = PBGetCatInfoSync(&pb);

    /* Update parent ID if we got a directory */
    if (err == noErr && (pb.u.hFileInfo.ioFlAttrib & kioFlAttribDir)) {
        spec->parID = pb.u.dirInfo.ioDrParID;
    }

    return err;
}

OSErr FSCreateDir(ConstStr255Param dirName, VolumeRefNum vRefNum, DirID* createdDirID)
{
    VCB* vcb;
    OSErr err;
    CatalogDirRec dirRec;

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    FS_LockVolume(vcb);

    /* Initialize directory record */
    memset(&dirRec, 0, sizeof(dirRec));
    dirRec.cdrType = REC_FLDR;
    dirRec.dirDirID = Cat_GetNextID(vcb);
    dirRec.dirCrDat = DateTime_Current();
    dirRec.dirMdDat = dirRec.dirCrDat;

    /* Create the directory in the catalog */
    err = Cat_Create(vcb, 2, dirName, REC_FLDR, &dirRec);  /* Parent = root (2) */

    if (err == noErr && createdDirID) {
        *createdDirID = dirRec.dirDirID;
    }

    /* Update volume directory count */
    if (err == noErr) {
        vcb->vcbDirCnt++;
        vcb->base.vcbFlags |= VCB_DIRTY;
    }

    FS_UnlockVolume(vcb);

    return err;
}

/* DirCreate - High-level wrapper for creating directory by parent dirID */
OSErr DirCreate(short vRefNum, long parentDirID, ConstStr255Param directoryName, long *createdDirID)
{
    /* For now, delegate to FSCreateDir
     * In a full implementation, this would handle the parentDirID parameter
     * by navigating to that directory first */
    DirID tempDirID;
    OSErr err = FSCreateDir(directoryName, vRefNum, &tempDirID);

    if (err == noErr && createdDirID) {
        *createdDirID = tempDirID;
    }

    return err;
}

OSErr FSDeleteDir(ConstStr255Param dirName, VolumeRefNum vRefNum)
{
    VCB* vcb;
    OSErr err;
    CatalogDirRec dirRec;
    UInt32 hint = 0;

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(vRefNum, &vcb);
    if (err != noErr) {
        return err;
    }

    FS_LockVolume(vcb);

    /* Look up the directory */
    err = Cat_Lookup(vcb, 2, dirName, &dirRec, &hint);  /* Parent = root (2) */
    if (err != noErr) {
        FS_UnlockVolume(vcb);
        return err;
    }

    /* Check if it's a directory */
    if (dirRec.cdrType != REC_FLDR) {
        FS_UnlockVolume(vcb);
        return notAFileErr;
    }

    /* Check if directory is empty */
    if (dirRec.dirVal > 0) {
        FS_UnlockVolume(vcb);
        return fBsyErr;
    }

    /* Delete from catalog */
    err = Cat_Delete(vcb, 2, dirName);

    /* Update volume directory count */
    if (err == noErr) {
        vcb->vcbDirCnt--;
        vcb->base.vcbFlags |= VCB_DIRTY;
    }

    FS_UnlockVolume(vcb);

    return err;
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

OSErr FSMount(UInt16 drvNum, void* buffer)
{
    VCB* vcb;
    OSErr err;

    /* Mount the volume */
    err = VCB_Mount(drvNum, &vcb);
    if (err != noErr) {
        return err;
    }

    /* Set as default volume if first mount */
    if (!g_FSGlobals.defVRefNum) {
        g_FSGlobals.defVRefNum = vcb->base.vcbVRefNum;
    }

    return noErr;
}

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

    FS_LockVolume(vcb);

    if (volName) {
        memcpy(volName, vcb->base.vcbVN, vcb->base.vcbVN[0] + 1);
    }

    if (vRefNumOut) {
        *vRefNumOut = vcb->base.vcbVRefNum;
    }

    if (freeBytes) {
        *freeBytes = (UInt32)vcb->base.vcbFreeBks * vcb->base.vcbAlBlkSiz;
    }

    FS_UnlockVolume(vcb);

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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Open the file */
    err = FCB_Open(vcb, 2, paramBlock->ioNamePtr,
                   (paramBlock)->u.ioParam.ioPermssn, &fcb);

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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Get catalog info */
    err = Cat_GetInfo(vcb, 2, paramBlock->ioNamePtr, paramBlock);

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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Set catalog info */
    err = Cat_SetInfo(vcb, 2, paramBlock->ioNamePtr, paramBlock);

    paramBlock->ioResult = err;
    return err;
}

/* ============================================================================
 * Asynchronous Operations (stubbed for now - would use thread pool)
 * ============================================================================ */

OSErr PBOpenAsync(ParmBlkPtr paramBlock)
{
    /* For now, just call synchronous version */
    /* In a full implementation, this would queue the operation */
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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Open data fork */
    err = FCB_Open(vcb, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID,
                   paramBlock->ioNamePtr,
                   (paramBlock)->u.ioParam.ioPermssn, &fcb);

    if (err == noErr) {
        fcb->base.fcbFlags &= ~FCB_RESOURCE;  /* Clear resource fork flag */
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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    /* Open resource fork */
    err = FCB_Open(vcb, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID,
                   paramBlock->ioNamePtr,
                   (paramBlock)->u.ioParam.ioPermssn, &fcb);

    if (err == noErr) {
        fcb->base.fcbFlags |= FCB_RESOURCE;  /* Set resource fork flag */
        (paramBlock)->u.ioParam.ioRefNum = fcb->fcbRefNum;
    }

    paramBlock->ioResult = err;
    return err;
}

OSErr PBHCreateSync(ParmBlkPtr paramBlock)
{
    VCB* vcb;
    OSErr err;
    CatalogFileRec fileRec;

    if (!paramBlock) {
        return paramErr;
    }

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    FS_LockVolume(vcb);

    /* Initialize file record */
    memset(&fileRec, 0, sizeof(fileRec));
    fileRec.cdrType = REC_FIL;
    fileRec.filFlNum = Cat_GetNextID(vcb);
    fileRec.filCrDat = DateTime_Current();
    fileRec.filMdDat = fileRec.filCrDat;

    /* Create the file in the catalog */
    err = Cat_Create(vcb, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID,
                    paramBlock->ioNamePtr, REC_FIL, &fileRec);

    /* Update volume file count */
    if (err == noErr) {
        vcb->vcbFilCnt++;
        vcb->base.vcbFlags |= VCB_DIRTY;
    }

    FS_UnlockVolume(vcb);

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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    FS_LockVolume(vcb);

    /* Delete from catalog */
    err = Cat_Delete(vcb, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID,
                    paramBlock->ioNamePtr);

    /* Update volume file count */
    if (err == noErr) {
        vcb->vcbFilCnt--;
        vcb->base.vcbFlags |= VCB_DIRTY;
    }

    FS_UnlockVolume(vcb);

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

    /* Find the volume */
    err = FM_GetVolumeFromRefNum(paramBlock->ioVRefNum, &vcb);
    if (err != noErr) {
        paramBlock->ioResult = err;
        return err;
    }

    FS_LockVolume(vcb);

    /* Rename in catalog */
    err = Cat_Rename(vcb, ((HParamBlockRec*)paramBlock)->u.hFileInfo.ioDirID,
                     paramBlock->ioNamePtr,
                     (const UInt8*)(paramBlock)->u.ioParam.ioMisc);

    FS_UnlockVolume(vcb);

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

    FS_LockFCB(fcb);
    fcb->fcbProcessID = processID;
    FS_UnlockFCB(fcb);

    return noErr;
}

OSErr FM_ReleaseProcessFiles(UInt32 processID)
{
    int closedCount = 0;

    FS_LockGlobal();

    /* Close all files owned by this process */
    for (int i = 0; i < g_FSGlobals.fcbCount; i++) {
        FCB* fcb = &g_FSGlobals.fcbArray[i];
        if (fcb->base.fcbFlNm != 0 && fcb->fcbProcessID == processID) {
            FCB_Close(fcb);
            closedCount++;
        }
    }

    /* Close all working directories owned by this process */
    for (int i = 0; i < g_FSGlobals.wdcbCount; i++) {
        WDCB* wdcb = &g_FSGlobals.wdcbArray[i];
        if (wdcb->wdVCBPtr && wdcb->wdProcID == processID) {
            WDCB_Free(wdcb);
        }
    }

    FS_UnlockGlobal();

    return noErr;
}

OSErr FM_YieldToProcess(void)
{
    /* Cooperative multitasking yield point */
    /* In a real implementation, this would yield to Process Manager */

#ifdef PLATFORM_REMOVED_WIN32
    Sleep(0);
#else
    sched_yield();
#endif

    return noErr;
}

/* ============================================================================
 * Thread Safety Implementation
 * ============================================================================ */

void FS_LockGlobal(void)
{
#ifdef PLATFORM_REMOVED_WIN32
    EnterCriticalSection(&g_FSGlobals.globalMutex);
#else
    pthread_mutex_lock(&g_FSGlobals.globalMutex);
#endif
}

void FS_UnlockGlobal(void)
{
#ifdef PLATFORM_REMOVED_WIN32
    LeaveCriticalSection(&g_FSGlobals.globalMutex);
#else
    pthread_mutex_unlock(&g_FSGlobals.globalMutex);
#endif
}

void FS_LockVolume(VCB* vcb)
{
    if (!vcb) return;

#ifdef PLATFORM_REMOVED_WIN32
    EnterCriticalSection(&vcb->base.vcbMutex);
#else
    pthread_mutex_lock(&vcb->vcbMutex);
#endif
}

void FS_UnlockVolume(VCB* vcb)
{
    if (!vcb) return;

#ifdef PLATFORM_REMOVED_WIN32
    LeaveCriticalSection(&vcb->base.vcbMutex);
#else
    pthread_mutex_unlock(&vcb->vcbMutex);
#endif
}

void FS_LockFCB(FCB* fcb)
{
    if (!fcb) return;

#ifdef PLATFORM_REMOVED_WIN32
    EnterCriticalSection(&fcb->fcbMutex);
#else
    pthread_mutex_lock(&fcb->fcbMutex);
#endif
}

void FS_UnlockFCB(FCB* fcb)
{
    if (!fcb) return;

#ifdef PLATFORM_REMOVED_WIN32
    LeaveCriticalSection(&fcb->fcbMutex);
#else
    pthread_mutex_unlock(&fcb->fcbMutex);
#endif
}

/* ============================================================================
 * Date/Time Utilities
 * ============================================================================ */

UInt32 DateTime_Current(void)
{
#if defined(__i386__) || defined(__x86_64__)
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        int y = (int)dt.year;
        int m = (int)dt.month;
        int d = (int)dt.day;
        int hour = (int)dt.hour;
        int minute = (int)dt.minute;
        int second = (int)dt.second;

        int adj_y = y - (m <= 2);
        int era = (adj_y >= 0 ? adj_y : adj_y - 399) / 400;
        unsigned yoe = (unsigned)(adj_y - era * 400);
        unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        int64_t days = (int64_t)(era * 146097 + (int)doe) - 719468;

        int64_t unix_time = days * 86400 + hour * 3600 + minute * 60 + second;
        if (unix_time < 0) {
            unix_time = 0;
        }
        return DateTime_FromUnix((time_t)unix_time);
    }
#endif
    time_t now = time(NULL);
    return DateTime_FromUnix(now);
}

UInt32 DateTime_FromUnix(time_t unixTime)
{
    /* Convert Unix time to Mac time (seconds since 1904) */
    return (UInt32)(unixTime + MAC_EPOCH_OFFSET);
}

time_t DateTime_ToUnix(UInt32 macTime)
{
    /* Convert Mac time to Unix time */
    if (macTime < MAC_EPOCH_OFFSET) {
        return 0;  /* Before Unix epoch */
    }
    return (time_t)(macTime - MAC_EPOCH_OFFSET);
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

/*
 * FileManagerVFS.c - the classic File Manager's volumes, files and catalog,
 * served from the VFS
 *
 * FileManager.c implements the classic calls - FSOpen, PBHCreate,
 * PBGetCatInfo and the rest - on a volume control block, a file control
 * block and a catalog. This file supplies those three from the VFS in
 * src/FS, which is where the volumes and files actually are:
 *
 *   - volumes the VFS mounts register here and are found by refNum;
 *   - a file control block holds an open VFS file, and reads and writes go
 *     through it;
 *   - catalog lookups, creates, deletes, renames and moves are VFS calls.
 *
 * It was FileManagerStubs.c, and began as stubs; the block-device path it
 * also carried - extents, allocation bitmaps, B-trees, a block cache - could
 * no longer run once every file was opened through the VFS, and is gone.
 */

#include "SystemTypes.h"
#include "FileManager.h"
#include "FileManager_Internal.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>
#include "FS/FSLogging.h"
#include "FS/vfs.h"
#include "FS/hfs_types.h"

/* Defined in FileManager.c */
extern FSGlobals g_FSGlobals;

static Boolean Cat_NameToC(const UInt8* name, char out[32]);

/* ============================================================================
 * Volumes
 * ============================================================================ */

VCB* VCB_Find(VolumeRefNum vRefNum) {
    VCB* vcb = g_FSGlobals.vcbQueue;
    while (vcb) {
        if (vcb->base.vcbVRefNum == vRefNum) {
            return vcb;
        }
        vcb = vcb->vcbNext;
    }
    return NULL;
}

/* A volume by name; "Macintosh HD" and "Macintosh HD:" both match, in any case. */
VCB* VCB_FindByName(const UInt8* name) {
    if (!name) return NULL;
    UInt8 len = name[0];
    if (len && name[len] == ':') len--;
    for (VCB* vcb = g_FSGlobals.vcbQueue; vcb; vcb = vcb->vcbNext) {
        const UInt8* vn = vcb->base.vcbVN;
        if (vn[0] != len) continue;
        UInt8 i = 1;
        for (; i <= len; i++) {
            UInt8 a = vn[i], b = name[i];
            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;
            if (a != b) break;
        }
        if (i > len) return vcb;
    }
    return NULL;
}

/* Take a volume off the list and unmount it in the VFS. */
OSErr VCB_Unmount(VCB* vcb) {
    if (!vcb) return paramErr;
    VCB** link = &g_FSGlobals.vcbQueue;
    while (*link && *link != vcb) link = &(*link)->vcbNext;
    if (!*link) return nsvErr;
    *link = vcb->vcbNext;
    if (g_FSGlobals.defVRefNum == vcb->base.vcbVRefNum) {
        g_FSGlobals.defVRefNum = g_FSGlobals.vcbQueue ? g_FSGlobals.vcbQueue->base.vcbVRefNum : 0;
    }
    VFS_Unmount((VRefNum)vcb->base.vcbVRefNum);
    DisposePtr((Ptr)vcb);
    return noErr;
}

/* Nothing is held here to write back: the VFS keeps the volume's state. */
OSErr VCB_Flush(VCB* vcb) {
    return vcb ? noErr : paramErr;
}

/* ============================================================================
 * Working directories
 *
 * FM_Initialize allocates the table, numbered up from kFirstWDRefNum, apart
 * from the VFS's volume numbers; a slot is free while it names no volume. These were stubs: WDCB_Create answered
 * tmwdoErr and WDCB_Find found nothing, so OpenWD never worked.
 * ============================================================================ */

OSErr WDCB_Create(VCB* vcb, UInt32 dirID, UInt32 procID, WDCB** newWDCB) {
    if (!vcb || !newWDCB) return paramErr;
    for (UInt16 i = 0; i < g_FSGlobals.wdcbCount; i++) {
        WDCB* w = &g_FSGlobals.wdcbArray[i];
        if (!w->wdVCBPtr) {
            w->wdVCBPtr = vcb;
            w->wdDirID = dirID ? dirID : 2;
            w->wdProcID = procID;
            *newWDCB = w;
            return noErr;
        }
    }
    return tmwdoErr;
}

WDCB* WDCB_Find(WDRefNum wdRefNum) {
    int idx = (int)wdRefNum - kFirstWDRefNum;
    if (idx < 0 || idx >= (int)g_FSGlobals.wdcbCount) return NULL;
    WDCB* w = &g_FSGlobals.wdcbArray[idx];
    return w->wdVCBPtr ? w : NULL;
}

void WDCB_Free(WDCB* wdcb) {
    if (!wdcb) return;
    wdcb->wdVCBPtr = NULL;
    wdcb->wdDirID = 0;
    wdcb->wdProcID = 0;
}

/* ============================================================================
 * File control blocks
 * ============================================================================ */

/*
 * File control blocks.
 *
 * A slot is free while its fcbFlNm is 0, and its reference number is its
 * index plus one. The free list this replaces handed the first file refNum 2
 * - it read the next free index after advancing to it - so FCB_Find(2)
 * answered a different slot; and FCB_Free zeroed a slot without returning it
 * to the list, so after MAX_FCBS opens nothing more could be opened.
 */
FCB* FCB_Alloc(void) {
    for (SInt16 i = 0; i < (SInt16)g_FSGlobals.fcbCount; i++) {
        FCB* fcb = &g_FSGlobals.fcbArray[i];
        if (fcb->base.fcbFlNm == 0 && !fcb->fcbVFSFile) {
            memset(fcb, 0, sizeof(FCB));
            fcb->fcbRefNum = (FileRefNum)(i + 1);
            return fcb;
        }
    }
    return NULL;
}

void FCB_Free(FCB* fcb) {
    if (fcb) memset(fcb, 0, sizeof(FCB));
}

FCB* FCB_Find(FileRefNum refNum) {
    if (refNum > 0 && refNum <= (FileRefNum)g_FSGlobals.fcbCount) {
        FCB* fcb = &g_FSGlobals.fcbArray[refNum - 1];
        if (fcb->base.fcbFlNm != 0 && fcb->fcbRefNum == refNum) {
            return fcb;
        }
    }
    return NULL;
}

FCB* FCB_FindByID(VCB* vcb, UInt32 fileID) {
    for (UInt16 i = 0; i < g_FSGlobals.fcbCount; i++) {
        FCB* fcb = &g_FSGlobals.fcbArray[i];
        if (fcb->base.fcbFlNm == fileID && (void*)fcb->base.fcbVPtr == (void*)vcb) {
            return fcb;
        }
    }
    return NULL;
}

/*
 * Open a fork of a file on a volume the VFS serves.
 *
 * This was a stub that answered fnfErr, so no classic open - FSOpen,
 * FSOpenDF, FSOpenRF, PBHOpen and the FSp variants built on them - could
 * open any file, and no read or write through them could happen.
 */
OSErr FCB_Open(VCB* vcb, UInt32 dirID, ConstStr255Param name, UInt8 permission,
               Boolean resourceFork, FCB** newFCB) {
    char cname[32];
    if (!vcb || !newFCB) return paramErr;
    *newFCB = NULL;
    if (!Cat_NameToC(name, cname)) return bdNamErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    CatEntry entry;
    if (!VFS_Lookup(vref, dirID ? (DirID)dirID : 2, cname, &entry)) return fnfErr;
    if (entry.kind == kNodeDir) return fnfErr;

    FCB* fcb = FCB_Alloc();
    if (!fcb) return tmfoErr;

    VFSFile* file = VFS_OpenFile(vref, entry.id, resourceFork);
    if (!file) {
        FCB_Free(fcb);
        return ioErr;
    }

    fcb->fcbVFSFile = file;
    fcb->base.fcbFlNm = entry.id;
    fcb->base.fcbVPtr = (void*)vcb;
    fcb->base.fcbVRefNum = (SInt16)vref;
    fcb->base.fcbEOF = VFS_GetFileSize(file);
    fcb->base.fcbPLen = fcb->base.fcbEOF;
    fcb->fcbPLen = fcb->base.fcbEOF;
    fcb->base.fcbCrPs = 0;
    fcb->base.fcbFlags = resourceFork ? FCB_RESOURCE : 0;
    /* fsCurPerm (0) and anything with the write bit set may write. */
    if (permission == 0 || (permission & 2)) {
        fcb->base.fcbFlags |= FCB_WRITE_PERM;
    }

    *newFCB = fcb;
    return noErr;
}

OSErr FCB_Close(FCB* fcb) {
    if (!fcb) return paramErr;
    if (fcb->fcbVFSFile) {
        VFS_CloseFile((VFSFile*)fcb->fcbVFSFile);
    }
    FCB_Free(fcb);
    return noErr;
}

OSErr FCB_Flush(FCB* fcb) {
    return fcb ? noErr : paramErr;
}

/* ============================================================================
 * Classic catalog access, served from the VFS
 *
 * The classic File Manager's own catalog was never implemented: Cat_GetInfo
 * returned fnfErr and VCB_Mount returned nsvErr, so g_FSGlobals.vcbQueue was
 * always empty and VCB_Find always failed. Every entry point that starts by
 * resolving a volume - PBGetCatInfoSync, FSMakeFSSpec, and everything built on
 * them - therefore failed with nsvErr before doing any work.
 *
 * That has quietly killed a feature at a time. Make Alias did nothing until it
 * was rebuilt directly on the VFS, alias_manager.c became unreachable, and the
 * Open and Save dialogs listed no files because SF_PopulateFileList enumerates
 * with PBGetCatInfoSync. Rewriting each caller onto the VFS fixes one symptom
 * and leaves the next one waiting, so the volume registry and the catalog are
 * answered from the VFS here instead - once, for every caller.
 * ============================================================================ */

/* Register a mounted VFS volume so VCB_Find can see it. Called by the VFS as
 * each volume mounts; a volume already registered is left alone. */
void FM_RegisterVFSVolume(SInt16 vref, const char* name)
{
    VCBExt* vcb = g_FSGlobals.vcbQueue;
    while (vcb) {
        if (vcb->base.vcbVRefNum == (SInt16)vref) return;
        vcb = vcb->vcbNext;
    }

    vcb = (VCBExt*)NewPtrClear(sizeof(VCBExt));
    if (!vcb) return;

    vcb->base.vcbVRefNum = (SInt16)vref;
    if (name) {
        size_t len = strlen(name);
        if (len > 27) len = 27;
        vcb->base.vcbVN[0] = (UInt8)len;
        memcpy(&vcb->base.vcbVN[1], name, len);
    }

    vcb->vcbNext = g_FSGlobals.vcbQueue;
    g_FSGlobals.vcbQueue = vcb;

    /* First volume to arrive is the default one, matching the boot volume. */
    if (g_FSGlobals.defVRefNum == 0) {
        g_FSGlobals.defVRefNum = (VolumeRefNum)vref;
    }

    FS_LOG_DEBUG("FM_RegisterVFSVolume: vRefNum=%d name=%s\n", (int)vref,
                 name ? name : "(none)");
}

/* Copy a C name into a Pascal-string buffer supplied by the caller. */
static void Cat_SetName(StringPtr out, const char* name)
{
    if (!out || !name) return;
    size_t len = strlen(name);
    if (len > 31) len = 31;
    out[0] = (UInt8)len;
    memcpy(&out[1], name, len);
}

/* A Pascal name as the C string the VFS takes. False if it is empty or will
 * not fit the VFS's 31 characters. */
static Boolean Cat_NameToC(const UInt8* name, char out[32])
{
    if (!name || name[0] == 0 || name[0] > 31) return false;
    memcpy(out, &name[1], name[0]);
    out[name[0]] = '\0';
    return true;
}

/*
 * Creating, deleting, renaming and moving, served from the VFS like the
 * lookups below.
 *
 * All four were stubs that answered noErr and did nothing, so the classic
 * calls built on them - PBHCreate, FSDelete, PBHRename, CatMove and every
 * FSp variant - reported success for work never done. FSCreate "made" a file
 * that the very next call could not find.
 */
OSErr Cat_Create(VCB* vcb, UInt32 dirID, const UInt8* name, UInt8 type, UInt32* newID)
{
    char cname[32];
    if (!vcb) return paramErr;
    if (!Cat_NameToC(name, cname)) return bdNamErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    DirID dir = dirID ? (DirID)dirID : 2;
    CatEntry existing;
    if (VFS_Lookup(vref, dir, cname, &existing)) return dupFNErr;

    if (type == REC_FLDR) {
        DirID id;
        if (!VFS_CreateFolder(vref, dir, cname, &id)) return ioErr;
        if (newID) *newID = (UInt32)id;
        return noErr;
    }
    FileID id;
    if (!VFS_CreateFile(vref, dir, cname, 0, 0, &id)) return ioErr;
    if (newID) *newID = (UInt32)id;
    return noErr;
}

OSErr Cat_Delete(VCB* vcb, UInt32 dirID, const UInt8* name)
{
    char cname[32];
    if (!vcb) return paramErr;
    if (!Cat_NameToC(name, cname)) return bdNamErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    CatEntry entry;
    if (!VFS_Lookup(vref, dirID ? (DirID)dirID : 2, cname, &entry)) return fnfErr;
    return VFS_Delete(vref, entry.id) ? noErr : ioErr;
}

OSErr Cat_Rename(VCB* vcb, UInt32 dirID, const UInt8* oldName, const UInt8* newName)
{
    char oldC[32], newC[32];
    if (!vcb) return paramErr;
    if (!Cat_NameToC(oldName, oldC) || !Cat_NameToC(newName, newC)) return bdNamErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    DirID dir = dirID ? (DirID)dirID : 2;
    CatEntry entry, clash;
    if (!VFS_Lookup(vref, dir, oldC, &entry)) return fnfErr;
    if (VFS_Lookup(vref, dir, newC, &clash)) return dupFNErr;
    return VFS_Rename(vref, entry.id, newC) ? noErr : ioErr;
}

OSErr Cat_Move(VCB* vcb, UInt32 srcDirID, const UInt8* name, UInt32 dstDirID)
{
    char cname[32];
    if (!vcb) return paramErr;
    if (!Cat_NameToC(name, cname)) return bdNamErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    DirID dst = dstDirID ? (DirID)dstDirID : 2;
    CatEntry entry, clash;
    if (!VFS_Lookup(vref, srcDirID ? (DirID)srcDirID : 2, cname, &entry)) return fnfErr;
    if (VFS_Lookup(vref, dst, cname, &clash)) return dupFNErr;
    return VFS_MoveOverlay(vref, entry.id, dst, entry.name, &entry) ? noErr : ioErr;
}

/* Fill a CInfoPBRec from a VFS catalog entry. */
static void Cat_FillFromEntry(CInfoPBRec* pb, const CatEntry* e)
{
    Boolean isDir = (e->kind == kNodeDir);

    if (pb->ioNamePtr) {
        Cat_SetName(pb->ioNamePtr, e->name);
    }

    if (isDir) {
        pb->u.dirInfo.ioDrDirID = (SInt32)e->id;
        pb->u.dirInfo.ioDrParID = (SInt16)e->parent;
        pb->u.dirInfo.ioDrNmFls = 0;   /* filled below when it is cheap to know */
        pb->u.dirInfo.ioDrCrDat = e->createTime;
        pb->u.dirInfo.ioDrMdDat = e->modTime;
    } else {
        pb->u.hFileInfo.ioDirID = (SInt32)e->id;
        pb->u.hFileInfo.ioFlLgLen = (SInt32)e->size;
        pb->u.hFileInfo.ioFlPyLen = (SInt32)e->size;
        pb->u.hFileInfo.ioFlCrDat = e->createTime;
        pb->u.hFileInfo.ioFlMdDat = e->modTime;
        pb->u.hFileInfo.ioFlFndrInfo.fdType = (OSType)e->type;
        pb->u.hFileInfo.ioFlFndrInfo.fdCreator = (OSType)e->creator;
        pb->u.hFileInfo.ioFlFndrInfo.fdFlags = (SInt16)e->flags;
    }

    /* Bit 4 of ioFlAttrib marks a directory - callers test it to decide
     * whether to recurse, and Standard File uses it to pick the folder icon.
     * Written last: the union's two members share this field, so filling in
     * either one's body has to happen before it is set. */
    pb->u.hFileInfo.ioFlAttrib = isDir ? 0x10 : 0x00;
}

/*
 * Cat_GetInfo - the three lookups PBGetCatInfo supports.
 *
 *   ioFDirIndex > 0   the index'th entry of directory ioDirID
 *   ioFDirIndex == 0  the item named ioNamePtr in directory ioDirID
 *   ioFDirIndex < 0   directory ioDirID itself
 */
OSErr Cat_GetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, CInfoPBRec* pb)
{
    (void)dirID;
    (void)name;

    if (!vcb || !pb) return paramErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    SInt16 index = pb->u.hFileInfo.ioFDirIndex;
    DirID dir = (DirID)pb->u.hFileInfo.ioDirID;
    if (dir == 0) dir = 2;   /* 0 means the root, as in the classic API */

    CatEntry entry;

    if (index > 0) {
        /* Indexed enumeration. VFS_Enumerate hands back the whole directory,
         * so ask for it and take the one entry wanted; directories here are
         * small enough that this is cheaper than keeping a cursor that could
         * go stale between calls. */
        enum { kMaxDirEntries = 128 };
        static CatEntry entries[kMaxDirEntries];
        int count = 0;

        if (!VFS_Enumerate(vref, dir, entries, kMaxDirEntries, &count)) {
            return fnfErr;
        }
        if (index > count) {
            return fnfErr;   /* past the end - how callers stop enumerating */
        }
        entry = entries[index - 1];
    } else if (index == 0) {
        char cname[32];
        if (!pb->ioNamePtr || pb->ioNamePtr[0] == 0) return paramErr;
        size_t len = pb->ioNamePtr[0];
        if (len > sizeof(cname) - 1) len = sizeof(cname) - 1;
        memcpy(cname, &pb->ioNamePtr[1], len);
        cname[len] = '\0';

        if (!VFS_Lookup(vref, dir, cname, &entry)) {
            return fnfErr;
        }
    } else {
        if (!VFS_GetByID(vref, (FileID)dir, &entry)) {
            return fnfErr;
        }
    }

    Cat_FillFromEntry(pb, &entry);
    pb->ioVRefNum = (SInt16)vref;

    /* A directory's item count is worth reporting: the Finder and Standard
     * File both show it, and it costs one enumeration we can already do. */
    if (entry.kind == kNodeDir) {
        enum { kMaxDirEntries = 128 };
        static CatEntry kids[kMaxDirEntries];
        int count = 0;
        if (VFS_Enumerate(vref, (DirID)entry.id, kids, kMaxDirEntries, &count)) {
            pb->u.dirInfo.ioDrNmFls = (UInt16)count;
        }
    }

    return noErr;
}

/* Set a file's type, creator and Finder flags from ioFlFndrInfo.
 *
 * A stub that answered noErr and changed nothing: FSCreate sets a new file's
 * type and creator through it, so every file created through the classic
 * calls came out with both 0. */
OSErr Cat_SetInfo(VCB* vcb, UInt32 dirID, const UInt8* name, const CInfoPBRec* pb)
{
    char cname[32];
    if (!vcb || !pb) return paramErr;
    if (!Cat_NameToC(name, cname)) return bdNamErr;

    VRefNum vref = (VRefNum)vcb->base.vcbVRefNum;
    CatEntry entry;
    if (!VFS_Lookup(vref, dirID ? (DirID)dirID : 2, cname, &entry)) return fnfErr;
    if (entry.kind == kNodeDir) return noErr;   /* folders keep no type or creator */

    const FInfo* info = &pb->u.hFileInfo.ioFlFndrInfo;
    return VFS_SetCatEntryInfo(vref, entry.id, info->fdType, info->fdCreator,
                               (uint16_t)info->fdFlags) ? noErr : ioErr;
}

/* ============================================================================
 * Fork I/O
 * ============================================================================ */

/*
 * Reads and writes of a fork the VFS opened go through the VFS; the block
 * path below is for a volume the File Manager mounted itself. Either way the
 * position that moves is fcb->base.fcbCrPs, the one PBRead and PBWrite read
 * back - the block path used to advance a second copy of it, so successive
 * reads started from the same place.
 */
static OSErr IO_VFSRead(FCB* fcb, UInt32 offset, UInt32 count, void* buffer, UInt32* actual) {
    VFSFile* file = (VFSFile*)fcb->fcbVFSFile;
    if (offset >= fcb->base.fcbEOF) return eofErr;
    Boolean short_ = false;
    if (offset + count > fcb->base.fcbEOF) {
        count = fcb->base.fcbEOF - offset;
        short_ = true;
    }
    uint32_t got = 0;
    if (!VFS_SeekFile(file, offset) || !VFS_ReadFile(file, buffer, count, &got)) return ioErr;
    *actual = got;
    fcb->base.fcbCrPs = offset + got;
    return short_ ? eofErr : noErr;
}

static OSErr IO_VFSWrite(FCB* fcb, UInt32 offset, UInt32 count, const void* buffer, UInt32* actual) {
    VFSFile* file = (VFSFile*)fcb->fcbVFSFile;
    if (!(fcb->base.fcbFlags & FCB_WRITE_PERM)) return wrPermErr;
    uint32_t put = 0;
    if (!VFS_SeekFile(file, offset) || !VFS_WriteFile(file, buffer, count, &put)) return ioErr;
    *actual = put;
    fcb->base.fcbCrPs = offset + put;
    if (fcb->base.fcbCrPs > fcb->base.fcbEOF) {
        fcb->base.fcbEOF = fcb->base.fcbCrPs;
        fcb->base.fcbPLen = fcb->base.fcbEOF;
        fcb->fcbPLen = fcb->base.fcbEOF;
    }
    fcb->base.fcbFlags |= FCB_DIRTY;
    return (put == count) ? noErr : dskFulErr;
}

OSErr IO_ReadFork(FCB* fcb, UInt32 offset, UInt32 count, void* buffer, UInt32* actual) {
    if (!fcb || !buffer || !actual) return paramErr;
    *actual = 0;
    if (!fcb->fcbVFSFile) return rfNumErr;
    return IO_VFSRead(fcb, offset, count, buffer, actual);
}

OSErr IO_WriteFork(FCB* fcb, UInt32 offset, UInt32 count, const void* buffer, UInt32* actual) {
    if (!fcb || !buffer || !actual) return paramErr;
    *actual = 0;
    if (!fcb->fcbVFSFile) return rfNumErr;
    return IO_VFSWrite(fcb, offset, count, buffer, actual);
}

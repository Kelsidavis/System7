/*
 * M68KFiles.c - the File Manager and Standard File for a 68K application
 *
 * A program names files the way the Macintosh File Manager does: a volume
 * reference number or a working directory, a directory ID, and a name that
 * may be a path ("Disk:Folder:File", ":Folder:File"). Those are resolved here
 * to the VFS's volume, directory and leaf name.
 *
 *   volume reference numbers  -vref (the VFS volume, negated)
 *   working directories       -100 and below, each a volume and directory
 *   0                         the default directory: the application's
 *                             folder, until SetVol says otherwise
 *
 * Open files are the program's file reference numbers, from 2000.
 * Parameter blocks are as in Inside Macintosh IV-115 and IV-155; the call's
 * result goes in ioResult and in D0.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "FS/vfs.h"
#include "ResourceManager.h"
#include "StandardFile/StandardFile.h"
#include "System71StdLib.h"

extern void GetDateTime(UInt32* secs);

/* The File Manager's results (IM IV-180) */
enum {
    kNsvErr = -35, kIoErr = -36, kBdNamErr = -37, kEofErr = -39, kPosErr = -40,
    kTmfoErr = -42, kFnfErr = -43, kDupFNErr = -48, kRfNumErr = -51, kDirNFErr = -120,
    kParamErr = -50
};
#define kAnyType 0x3F3F3F3Fu      /* '????' */

enum { kMaxWD = 64, kMaxFCB = 64, kFirstRef = 2000, kFirstWD = -100 };

static struct { VRefNum vref; DirID dir; Boolean used; } gWD[kMaxWD];
static struct { VFSFile* f; VRefNum vref; FileID id; DirID parent; Boolean rsrc; } gFCB[kMaxFCB];
static VRefNum gDefaultVRef;
static DirID gDefaultDir;

/* ------------------------------------------------------------------------
 * Volumes, directories, names
 * ------------------------------------------------------------------------ */

static SInt16 VolRef(VRefNum vref) { return (SInt16)-(SInt32)vref; }

static DirID RootOf(VRefNum vref) {
    VolumeControlBlock vcb;
    return VFS_GetVolumeInfo(vref, &vcb) ? vcb.rootID : 2;
}

static SInt16 WDFor(VRefNum vref, DirID dir) {
    for (int i = 0; i < kMaxWD; i++)
        if (gWD[i].used && gWD[i].vref == vref && gWD[i].dir == dir) return (SInt16)(kFirstWD - i);
    for (int i = 0; i < kMaxWD; i++) {
        if (!gWD[i].used) {
            gWD[i].used = true;
            gWD[i].vref = vref;
            gWD[i].dir = dir;
            return (SInt16)(kFirstWD - i);
        }
    }
    return VolRef(vref);
}

/* The volume and directory a reference number stands for */
static Boolean FromRefNum(SInt16 vRefNum, VRefNum* vref, DirID* dir) {
    if (vRefNum == 0) {
        *vref = gDefaultVRef;
        *dir = gDefaultDir;
        return true;
    }
    if (vRefNum <= kFirstWD) {
        int i = kFirstWD - vRefNum;
        if (i >= kMaxWD || !gWD[i].used) return false;
        *vref = gWD[i].vref;
        *dir = gWD[i].dir;
        return true;
    }
    if (vRefNum < 0) {
        VolumeControlBlock vcb;
        *vref = (VRefNum)(-vRefNum);
        if (!VFS_GetVolumeInfo(*vref, &vcb)) return false;
        *dir = vcb.rootID;
        return true;
    }
    return false;
}

static Boolean VolumeNamed(const char* name, VRefNum* vref) {
    for (VRefNum v = 1; v < 32; v++) {
        VolumeControlBlock vcb;
        if (VFS_GetVolumeInfo(v, &vcb) && strcasecmp(vcb.name, name) == 0) {
            *vref = v;
            return true;
        }
    }
    return false;
}

/*
 * A name, relative to (vRefNum, dirID), to the directory it is in and the
 * leaf name - empty when the name is a directory itself. "Vol:a:b" starts at
 * the volume named; ":a:b" and "a" at the directory given; "::" goes up.
 */
static OSErr Resolve(SInt16 vRefNum, SInt32 dirID, ConstStr255Param pname,
                     VRefNum* vref, DirID* dir, char* leaf) {
    char name[256];
    UInt8 len = pname ? ((const UInt8*)pname)[0] : 0;
    if (len) memcpy(name, (const UInt8*)pname + 1, len);
    name[len] = '\0';
    leaf[0] = '\0';

    if (!FromRefNum(vRefNum, vref, dir)) return kNsvErr;
    if (dirID) *dir = (DirID)dirID;

    char* p = name;
    char* colon = strchr(p, ':');
    if (colon && colon != p) {
        *colon = '\0';
        if (!VolumeNamed(p, vref)) return kNsvErr;
        *dir = RootOf(*vref);
        p = colon + 1;
    } else if (colon == p) {
        p++;
    }
    for (;;) {
        colon = strchr(p, ':');
        if (!colon) break;
        *colon = '\0';
        if (*p == '\0') {
            CatEntry self;                          /* "::" - up a level */
            if (VFS_GetByID(*vref, *dir, &self)) *dir = self.parent;
        } else {
            CatEntry e;
            if (!VFS_Lookup(*vref, *dir, p, &e) || e.kind != kNodeDir) return kDirNFErr;
            *dir = e.id;
        }
        p = colon + 1;
    }
    strncpy(leaf, p, 63);
    leaf[63] = '\0';
    return noErr;
}

static OSErr Find(SInt16 vRefNum, SInt32 dirID, ConstStr255Param name,
                  VRefNum* vref, DirID* dir, CatEntry* e) {
    char leaf[64];
    OSErr err = Resolve(vRefNum, dirID, name, vref, dir, leaf);
    if (err) return err;
    if (!leaf[0]) return kBdNamErr;
    return VFS_Lookup(*vref, *dir, leaf, e) ? noErr : kFnfErr;
}

/* ------------------------------------------------------------------------
 * Parameter blocks
 * ------------------------------------------------------------------------ */

enum {
    kIoResult = 16, kIoNamePtr = 18, kIoVRefNum = 22, kIoRefNum = 24, kIoPermssn = 27,
    kIoMisc = 28, kIoBuffer = 32, kIoReqCount = 36, kIoActCount = 40, kIoPosMode = 44,
    kIoPosOffset = 46, kIoFDirIndex = 28, kIoDirID = 48
};

static Boolean HFSCall(void) { return (gM68KApp->currentTrap & 0x0200) != 0; }

static OSErr Done(OSErr err) {
    W16(A(0) + kIoResult, (UInt16)err);
    D(0) = (UInt32)(SInt32)err;
    return noErr;
}

static void ReadName(UInt32 pb, Str255 name) {
    UInt32 p = R32(pb + kIoNamePtr);
    ReadPString(p, name);
}

static SInt32 PBDirID(UInt32 pb) {
    return HFSCall() ? (SInt32)R32(pb + kIoDirID) : 0;
}

static int FCBIndex(SInt16 ref) {
    int i = ref - kFirstRef;
    return (i >= 0 && i < kMaxFCB && gFCB[i].f) ? i : -1;
}

static OSErr OpenFork(Boolean rsrc) {
    UInt32 pb = A(0);
    Str255 name;
    ReadName(pb, name);
    VRefNum vref;
    DirID dir;
    CatEntry e;
    OSErr err = Find((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, &e);
    if (err) return Done(err);
    if (e.kind == kNodeDir) return Done(kFnfErr);
    int i;
    for (i = 0; i < kMaxFCB && gFCB[i].f; i++) {}
    if (i == kMaxFCB) return Done(kTmfoErr);
    VFSFile* f = VFS_OpenFile(vref, e.id, rsrc);
    if (!f) return Done(kIoErr);
    gFCB[i].f = f;
    gFCB[i].vref = vref;
    gFCB[i].id = e.id;
    gFCB[i].parent = dir;
    gFCB[i].rsrc = rsrc;
    W16(pb + kIoRefNum, (UInt16)(kFirstRef + i));
    return Done(noErr);
}

TRAP(Trap_Open)   { UNUSED; return OpenFork(false); }
TRAP(Trap_OpenRF) { UNUSED; return OpenFork(true); }

TRAP(Trap_Close) {
    UNUSED;
    int i = FCBIndex((SInt16)R16(A(0) + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    VFS_CloseFile(gFCB[i].f);
    gFCB[i].f = NULL;
    return Done(noErr);
}

/* The position a read or write starts from: ioPosMode and ioPosOffset */
static OSErr Position(int i, UInt32 pb) {
    UInt16 mode = R16(pb + kIoPosMode) & 3;
    SInt32 offset = (SInt32)R32(pb + kIoPosOffset);
    SInt32 base = mode == 1 ? 0 : mode == 2 ? (SInt32)VFS_GetFileSize(gFCB[i].f)
                : (SInt32)VFS_GetFilePosition(gFCB[i].f);
    SInt32 at = mode == 0 ? base : base + offset;
    if (at < 0) return kPosErr;
    VFS_SeekFile(gFCB[i].f, (uint32_t)at);
    return noErr;
}

TRAP(Trap_Read) {
    UNUSED;
    UInt32 pb = A(0);
    int i = FCBIndex((SInt16)R16(pb + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    OSErr err = Position(i, pb);
    if (err) return Done(err);
    UInt32 want = R32(pb + kIoReqCount), buf = R32(pb + kIoBuffer), got = 0;
    UInt8 chunk[512];
    while (got < want) {
        UInt32 n = want - got > sizeof(chunk) ? sizeof(chunk) : want - got;
        uint32_t r = 0;
        if (!VFS_ReadFile(gFCB[i].f, chunk, n, &r) || r == 0) break;
        WriteBytes(buf + got, chunk, r);
        got += r;
        if (r < n) break;
    }
    W32(pb + kIoActCount, got);
    W32(pb + kIoPosOffset, VFS_GetFilePosition(gFCB[i].f));
    return Done(got < want ? kEofErr : noErr);
}

TRAP(Trap_Write) {
    UNUSED;
    UInt32 pb = A(0);
    int i = FCBIndex((SInt16)R16(pb + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    OSErr err = Position(i, pb);
    if (err) return Done(err);
    UInt32 want = R32(pb + kIoReqCount), buf = R32(pb + kIoBuffer), done = 0;
    UInt8 chunk[512];
    while (done < want) {
        UInt32 n = want - done > sizeof(chunk) ? sizeof(chunk) : want - done;
        ReadBytes(buf + done, chunk, n);
        uint32_t w = 0;
        if (!VFS_WriteFile(gFCB[i].f, chunk, n, &w)) break;
        done += w;
    }
    W32(pb + kIoActCount, done);
    W32(pb + kIoPosOffset, VFS_GetFilePosition(gFCB[i].f));
    return Done(done < want ? kIoErr : noErr);
}

TRAP(Trap_GetEOF) {
    UNUSED;
    int i = FCBIndex((SInt16)R16(A(0) + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    W32(A(0) + kIoMisc, VFS_GetFileSize(gFCB[i].f));
    return Done(noErr);
}

TRAP(Trap_SetEOF) {
    UNUSED;
    int i = FCBIndex((SInt16)R16(A(0) + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    return Done(VFS_SetFileSize(gFCB[i].f, R32(A(0) + kIoMisc)) ? noErr : kIoErr);
}

TRAP(Trap_GetFPos) {
    UNUSED;
    int i = FCBIndex((SInt16)R16(A(0) + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    W32(A(0) + kIoReqCount, 0);
    W32(A(0) + kIoActCount, 0);
    W16(A(0) + kIoPosMode, 0);
    W32(A(0) + kIoPosOffset, VFS_GetFilePosition(gFCB[i].f));
    return Done(noErr);
}

TRAP(Trap_SetFPos) {
    UNUSED;
    int i = FCBIndex((SInt16)R16(A(0) + kIoRefNum));
    if (i < 0) return Done(kRfNumErr);
    OSErr err = Position(i, A(0));
    W32(A(0) + kIoPosOffset, VFS_GetFilePosition(gFCB[i].f));
    return Done(err);
}

TRAP(Trap_Create) {
    UNUSED;
    UInt32 pb = A(0);
    Str255 name;
    ReadName(pb, name);
    VRefNum vref;
    DirID dir;
    char leaf[64];
    OSErr err = Resolve((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, leaf);
    if (err) return Done(err);
    if (!leaf[0]) return Done(kBdNamErr);
    CatEntry e;
    if (VFS_Lookup(vref, dir, leaf, &e)) return Done(kDupFNErr);
    FileID id;
    return Done(VFS_CreateFile(vref, dir, leaf, kAnyType, kAnyType, &id) ? noErr : kIoErr);
}

TRAP(Trap_Delete) {
    UNUSED;
    UInt32 pb = A(0);
    Str255 name;
    ReadName(pb, name);
    VRefNum vref;
    DirID dir;
    CatEntry e;
    OSErr err = Find((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, &e);
    if (err) return Done(err);
    return Done(VFS_Delete(vref, e.id) ? noErr : kIoErr);
}

TRAP(Trap_Rename) {
    UNUSED;
    UInt32 pb = A(0);
    Str255 name, newName;
    ReadName(pb, name);
    ReadPString(R32(pb + kIoMisc), newName);
    VRefNum vref;
    DirID dir;
    CatEntry e;
    OSErr err = Find((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, &e);
    if (err) return Done(err);
    char leaf[64];
    memcpy(leaf, &newName[1], newName[0] < 63 ? newName[0] : 63);
    leaf[newName[0] < 63 ? newName[0] : 63] = '\0';
    return Done(VFS_Rename(vref, e.id, leaf) ? noErr : kIoErr);
}

/* The nth file in a directory, counting from 1, folders left out */
static Boolean NthFile(VRefNum vref, DirID dir, int n, CatEntry* out) {
    CatEntry entries[128];
    int count = 0;
    if (!VFS_Enumerate(vref, dir, entries, 128, &count)) return false;
    for (int i = 0; i < count; i++) {
        if (entries[i].kind == kNodeDir) continue;
        if (--n == 0) {
            *out = entries[i];
            return true;
        }
    }
    return false;
}

static void PutName(UInt32 pb, const char* name) {
    UInt32 p = R32(pb + kIoNamePtr);
    if (!p) return;
    Str255 s;
    c2pstrcpy(s, name);
    WritePString(p, s);
}

/* The FileParam fields of an entry: Finder info, number, lengths, dates */
static void PutFileInfo(UInt32 pb, const CatEntry* e) {
    W8(pb + 30, 0);                         /* ioFlAttrib */
    W8(pb + 31, 0);
    W32(pb + 32, e->type);
    W32(pb + 36, e->creator);
    W16(pb + 40, e->flags);
    W32(pb + 42, 0);                        /* fdLocation */
    W16(pb + 46, 0);                        /* fdFldr */
    W32(pb + 48, e->id);                    /* ioFlNum */
    W16(pb + 52, 0);
    W32(pb + 54, e->size);
    W32(pb + 58, (e->size + 511) & ~511u);
    W16(pb + 62, 0);
    W32(pb + 64, 0);
    W32(pb + 68, 0);
    W32(pb + 72, e->createTime);
    W32(pb + 76, e->modTime);
}

TRAP(Trap_GetFileInfo) {
    UNUSED;
    UInt32 pb = A(0);
    SInt16 index = (SInt16)R16(pb + kIoFDirIndex);
    VRefNum vref;
    DirID dir;
    CatEntry e;
    if (index > 0) {
        if (!FromRefNum((SInt16)R16(pb + kIoVRefNum), &vref, &dir)) return Done(kNsvErr);
        if (HFSCall() && R32(pb + kIoDirID)) dir = R32(pb + kIoDirID);
        if (!NthFile(vref, dir, index, &e)) return Done(kFnfErr);
        PutName(pb, e.name);
    } else {
        Str255 name;
        ReadName(pb, name);
        OSErr err = Find((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, &e);
        if (err) return Done(err);
    }
    PutFileInfo(pb, &e);
    return Done(noErr);
}

TRAP(Trap_SetFileInfo) {
    UNUSED;
    UInt32 pb = A(0);
    Str255 name;
    ReadName(pb, name);
    VRefNum vref;
    DirID dir;
    CatEntry e;
    OSErr err = Find((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, &e);
    if (err) return Done(err);
    return Done(VFS_SetCatEntryInfo(vref, e.id, R32(pb + 32), R32(pb + 36), R16(pb + 40))
                ? noErr : kIoErr);
}

static void PutVolumeName(UInt32 pb, VRefNum vref) {
    VolumeControlBlock vcb;
    if (VFS_GetVolumeInfo(vref, &vcb)) PutName(pb, vcb.name);
}

/* GetVol: the default volume's name, and the default directory as a
 * working directory - with its directory ID too for HGetVol */
TRAP(Trap_GetVol) {
    UNUSED;
    PutVolumeName(A(0), gDefaultVRef);
    W16(A(0) + kIoVRefNum, (UInt16)WDFor(gDefaultVRef, gDefaultDir));
    if (HFSCall()) W32(A(0) + kIoDirID, gDefaultDir);
    return Done(noErr);
}

TRAP(Trap_SetVol) {
    UNUSED;
    UInt32 pb = A(0);
    Str255 name;
    ReadName(pb, name);
    VRefNum vref;
    DirID dir;
    char leaf[64];
    OSErr err = Resolve((SInt16)R16(pb + kIoVRefNum), PBDirID(pb), name, &vref, &dir, leaf);
    if (err) return Done(err);
    if (leaf[0]) {
        CatEntry e;
        if (!VFS_Lookup(vref, dir, leaf, &e) || e.kind != kNodeDir) return Done(kNsvErr);
        dir = e.id;
    }
    gDefaultVRef = vref;
    gDefaultDir = dir;
    return Done(noErr);
}

/* GetVolInfo: by index (ioVolIndex > 0), else by reference number */
TRAP(Trap_GetVolInfo) {
    UNUSED;
    UInt32 pb = A(0);
    SInt16 index = (SInt16)R16(pb + 28);
    VRefNum vref = 0;
    VolumeControlBlock vcb;
    if (index > 0) {
        for (VRefNum v = 1, n = 0; v < 32; v++) {
            if (VFS_GetVolumeInfo(v, &vcb) && ++n == (VRefNum)index) {
                vref = v;
                break;
            }
        }
    } else {
        DirID dir;
        if (!FromRefNum((SInt16)R16(pb + kIoVRefNum), &vref, &dir)) vref = 0;
    }
    if (!vref || !VFS_GetVolumeInfo(vref, &vcb)) return Done(kNsvErr);
    PutName(pb, vcb.name);
    W16(pb + kIoVRefNum, (UInt16)VolRef(vref));
    UInt32 now = 0;
    GetDateTime(&now);
    W32(pb + 30, now);                              /* ioVCrDate */
    W32(pb + 34, 0);                                /* ioVLsBkUp */
    W16(pb + 38, 0);                                /* ioVAtrb */
    W16(pb + 40, 0);                                /* ioVNmFls */
    W16(pb + 42, 0);
    W16(pb + 44, 0);
    UInt32 blockSize = 4096;
    W16(pb + 46, (UInt16)((vcb.totalBytes / blockSize) > 65535 ? 65535 : vcb.totalBytes / blockSize));
    W32(pb + 48, blockSize);
    W32(pb + 52, blockSize);
    W16(pb + 56, 0);
    W32(pb + 58, 1000);
    W16(pb + 62, (UInt16)((vcb.freeBytes / blockSize) > 65535 ? 65535 : vcb.freeBytes / blockSize));
    return Done(noErr);
}

TRAP(Trap_FileNoOp) { UNUSED; return Done(noErr); }

/* ------------------------------------------------------------------------
 * HFSDispatch: D0 says which call
 * ------------------------------------------------------------------------ */

static OSErr CatInfo(UInt32 pb) {
    SInt16 index = (SInt16)R16(pb + kIoFDirIndex);
    VRefNum vref;
    DirID dir;
    CatEntry e;
    if (!FromRefNum((SInt16)R16(pb + kIoVRefNum), &vref, &dir)) return kNsvErr;
    if (R32(pb + kIoDirID)) dir = R32(pb + kIoDirID);
    if (index > 0) {
        CatEntry entries[128];
        int count = 0;
        if (!VFS_Enumerate(vref, dir, entries, 128, &count) || index > count) return kFnfErr;
        e = entries[index - 1];
        PutName(pb, e.name);
    } else if (index < 0) {
        if (!VFS_GetByID(vref, dir, &e)) {
            if (dir != RootOf(vref)) return kFnfErr;
            VolumeControlBlock vcb;
            VFS_GetVolumeInfo(vref, &vcb);
            memset(&e, 0, sizeof(e));
            strncpy(e.name, vcb.name, sizeof(e.name) - 1);
            e.kind = kNodeDir;
            e.id = dir;
            e.parent = 1;
        }
        PutName(pb, e.name);
    } else {
        Str255 name;
        ReadName(pb, name);
        OSErr err = Find((SInt16)R16(pb + kIoVRefNum), (SInt32)R32(pb + kIoDirID), name,
                         &vref, &dir, &e);
        if (err) return err;
    }
    if (e.kind == kNodeDir) {
        CatEntry entries[128];
        int count = 0;
        VFS_Enumerate(vref, e.id, entries, 128, &count);
        W8(pb + 30, 0x10);                           /* ioFlAttrib: a directory */
        for (int i = 0; i < 16; i++) W8(pb + 32 + i, 0);   /* ioDrUsrWds */
        W32(pb + 48, e.id);                          /* ioDrDirID */
        W16(pb + 52, (UInt16)count);                 /* ioDrNmFls */
        W32(pb + 72, e.createTime);
        W32(pb + 76, e.modTime);
    } else {
        PutFileInfo(pb, &e);
    }
    W32(pb + 100, e.parent);                         /* ioFlParID / ioDrParID */
    return noErr;
}

TRAP(Trap_HFSDispatch) {
    UNUSED;
    UInt32 pb = A(0);
    switch ((UInt16)D(0)) {
        case 1: {                                    /* OpenWD */
            VRefNum vref;
            DirID dir;
            if (!FromRefNum((SInt16)R16(pb + kIoVRefNum), &vref, &dir)) return Done(kNsvErr);
            if (R32(pb + 48)) dir = R32(pb + 48);
            Str255 name;
            ReadName(pb, name);
            if (name[0]) {
                char leaf[64];
                OSErr err = Resolve((SInt16)R16(pb + kIoVRefNum), (SInt32)dir, name, &vref, &dir, leaf);
                if (err) return Done(err);
                if (leaf[0]) {
                    CatEntry e;
                    if (!VFS_Lookup(vref, dir, leaf, &e) || e.kind != kNodeDir) return Done(kDirNFErr);
                    dir = e.id;
                }
            }
            W16(pb + kIoVRefNum, (UInt16)WDFor(vref, dir));
            return Done(noErr);
        }
        case 2:                                      /* CloseWD */
            return Done(noErr);
        case 6: {                                    /* DirCreate */
            Str255 name;
            ReadName(pb, name);
            VRefNum vref;
            DirID dir, made;
            char leaf[64];
            OSErr err = Resolve((SInt16)R16(pb + kIoVRefNum), (SInt32)R32(pb + 48), name, &vref, &dir, leaf);
            if (err) return Done(err);
            if (!VFS_CreateFolder(vref, dir, leaf, &made)) return Done(kDupFNErr);
            W32(pb + 48, made);
            return Done(noErr);
        }
        case 7: {                                    /* GetWDInfo */
            VRefNum vref;
            DirID dir;
            if (!FromRefNum((SInt16)R16(pb + kIoVRefNum), &vref, &dir)) return Done(kNsvErr);
            W16(pb + 32, (UInt16)VolRef(vref));
            W32(pb + 48, dir);
            W32(pb + 28, 0);
            PutVolumeName(pb, vref);
            return Done(noErr);
        }
        case 9:                                      /* GetCatInfo */
            return Done(CatInfo(pb));
        case 10: {                                   /* SetCatInfo */
            Str255 name;
            ReadName(pb, name);
            VRefNum vref;
            DirID dir;
            CatEntry e;
            OSErr err = Find((SInt16)R16(pb + kIoVRefNum), (SInt32)R32(pb + 48), name, &vref, &dir, &e);
            if (err) return Done(err);
            if (e.kind != kNodeDir) VFS_SetCatEntryInfo(vref, e.id, R32(pb + 32), R32(pb + 36), R16(pb + 40));
            return Done(noErr);
        }
        default:
            return Done(kParamErr);
    }
}

/* ------------------------------------------------------------------------
 * FSSpecs: vRefNum, parID, name (IM VI 25-35)
 * ------------------------------------------------------------------------ */

static void ReadSpec(UInt32 a, SInt16* vRefNum, SInt32* parID, Str255 name) {
    *vRefNum = (SInt16)R16(a);
    *parID = (SInt32)R32(a + 2);
    ReadPString(a + 6, name);
}

static OSErr SpecFile(UInt32 spec, VRefNum* vref, DirID* dir, CatEntry* e, char* leaf) {
    SInt16 v;
    SInt32 par;
    Str255 name;
    ReadSpec(spec, &v, &par, name);
    OSErr err = Resolve(v, par, name, vref, dir, leaf);
    if (err) return err;
    if (e && !VFS_Lookup(*vref, *dir, leaf, e)) return kFnfErr;
    return noErr;
}

static OSErr OpenSpec(UInt32 spec, Boolean rsrc, UInt32 refVar) {
    VRefNum vref;
    DirID dir;
    CatEntry e;
    char leaf[64];
    OSErr err = SpecFile(spec, &vref, &dir, &e, leaf);
    if (err) return err;
    int i;
    for (i = 0; i < kMaxFCB && gFCB[i].f; i++) {}
    if (i == kMaxFCB) return kTmfoErr;
    VFSFile* f = VFS_OpenFile(vref, e.id, rsrc);
    if (!f) return kIoErr;
    gFCB[i].f = f;
    gFCB[i].vref = vref;
    gFCB[i].id = e.id;
    gFCB[i].parent = dir;
    gFCB[i].rsrc = rsrc;
    W16(refVar, (UInt16)(kFirstRef + i));
    return noErr;
}

/* HighLevelFSDispatch: D0 the selector, the arguments Pascal-style */
TRAP(Trap_HighLevelFS) {
    UNUSED;
    OSErr err = kParamErr;
    switch ((UInt16)D(0)) {
        case 1: {                                    /* FSMakeFSSpec */
            UInt32 specVar = Pop32();
            Str255 name;
            ReadPString(Pop32(), name);
            SInt32 dirID = (SInt32)Pop32();
            SInt16 vRefNum = (SInt16)Pop16();
            VRefNum vref;
            DirID dir;
            char leaf[64];
            err = Resolve(vRefNum, dirID, name, &vref, &dir, leaf);
            if (!err) {
                CatEntry e;
                Str255 l;
                c2pstrcpy(l, leaf);
                W16(specVar, (UInt16)VolRef(vref));
                W32(specVar + 2, dir);
                WritePString(specVar + 6, l);
                if (!leaf[0] || !VFS_Lookup(vref, dir, leaf, &e)) err = kFnfErr;
            }
            break;
        }
        case 2:
        case 3: {                                    /* FSpOpenDF, FSpOpenRF */
            UInt32 refVar = Pop32();
            (void)Pop16();                           /* permission */
            UInt32 spec = Pop32();
            err = OpenSpec(spec, D(0) == 3, refVar);
            break;
        }
        case 4: {                                    /* FSpCreate */
            (void)Pop16();
            UInt32 type = Pop32(), creator = Pop32();
            UInt32 spec = Pop32();
            VRefNum vref;
            DirID dir;
            char leaf[64];
            CatEntry e;
            FileID id;
            err = SpecFile(spec, &vref, &dir, NULL, leaf);
            if (!err) {
                if (VFS_Lookup(vref, dir, leaf, &e)) err = kDupFNErr;
                else err = VFS_CreateFile(vref, dir, leaf, type, creator, &id) ? noErr : kIoErr;
            }
            break;
        }
        case 5: {                                    /* FSpDirCreate */
            UInt32 idVar = Pop32();
            (void)Pop16();
            UInt32 spec = Pop32();
            VRefNum vref;
            DirID dir, made;
            char leaf[64];
            err = SpecFile(spec, &vref, &dir, NULL, leaf);
            if (!err) err = VFS_CreateFolder(vref, dir, leaf, &made) ? noErr : kDupFNErr;
            if (!err) W32(idVar, made);
            break;
        }
        case 6: {                                    /* FSpDelete */
            UInt32 spec = Pop32();
            VRefNum vref;
            DirID dir;
            CatEntry e;
            char leaf[64];
            err = SpecFile(spec, &vref, &dir, &e, leaf);
            if (!err) err = VFS_Delete(vref, e.id) ? noErr : kIoErr;
            break;
        }
        case 7: {                                    /* FSpGetFInfo */
            UInt32 info = Pop32();
            UInt32 spec = Pop32();
            VRefNum vref;
            DirID dir;
            CatEntry e;
            char leaf[64];
            err = SpecFile(spec, &vref, &dir, &e, leaf);
            if (!err) {
                W32(info, e.type);
                W32(info + 4, e.creator);
                W16(info + 8, e.flags);
                W32(info + 10, 0);
                W16(info + 14, 0);
            }
            break;
        }
        case 8: {                                    /* FSpSetFInfo */
            UInt32 info = Pop32();
            UInt32 spec = Pop32();
            VRefNum vref;
            DirID dir;
            CatEntry e;
            char leaf[64];
            err = SpecFile(spec, &vref, &dir, &e, leaf);
            if (!err) err = VFS_SetCatEntryInfo(vref, e.id, R32(info), R32(info + 4), R16(info + 8))
                            ? noErr : kIoErr;
            break;
        }
        case 11: {                                   /* FSpRename */
            Str255 newName;
            ReadPString(Pop32(), newName);
            UInt32 spec = Pop32();
            VRefNum vref;
            DirID dir;
            CatEntry e;
            char leaf[64];
            err = SpecFile(spec, &vref, &dir, &e, leaf);
            if (!err) {
                char n[64];
                p2cstrcpy(n, newName);
                err = VFS_Rename(vref, e.id, n) ? noErr : kIoErr;
            }
            break;
        }
        default:
            break;
    }
    Result16((UInt16)err);
    return noErr;
}

/* ------------------------------------------------------------------------
 * Resource files the program opens by name
 * ------------------------------------------------------------------------ */

static SInt16 OpenResNamed(SInt16 vRefNum, SInt32 dirID, ConstStr255Param name) {
    VRefNum vref;
    DirID dir;
    char leaf[64];
    if (Resolve(vRefNum, dirID, name, &vref, &dir, leaf) != noErr || !leaf[0]) {
        W16(kLM_ResErr, (UInt16)fnfErr);
        return -1;
    }
    FSSpec spec;
    spec.vRefNum = (SInt16)vref;
    spec.parID = (SInt32)dir;
    c2pstrcpy(spec.name, leaf);
    SInt16 ref = FSpOpenResFile(&spec, 3);     /* fsRdWrPerm */
    M68KTB_SetResErr();
    return ref;
}

/* FUNCTION OpenResFile(fileName: Str255): INTEGER */
TRAP(Trap_OpenResFile) {
    UNUSED;
    Str255 name;
    ReadPString(Pop32(), name);
    Result16((UInt16)OpenResNamed(0, 0, name));
    return noErr;
}

/* FUNCTION OpenRFPerm(fileName: Str255; vRefNum: INTEGER; permission: SignedByte): INTEGER */
TRAP(Trap_OpenRFPerm) {
    UNUSED;
    (void)Pop16();
    SInt16 vRefNum = (SInt16)Pop16();
    Str255 name;
    ReadPString(Pop32(), name);
    Result16((UInt16)OpenResNamed(vRefNum, 0, name));
    return noErr;
}

/* FUNCTION HOpenResFile(vRefNum: INTEGER; dirID: LONGINT; fileName: Str255;
 *   permission: SignedByte): INTEGER */
TRAP(Trap_HOpenResFile) {
    UNUSED;
    (void)Pop16();
    Str255 name;
    ReadPString(Pop32(), name);
    SInt32 dirID = (SInt32)Pop32();
    SInt16 vRefNum = (SInt16)Pop16();
    Result16((UInt16)OpenResNamed(vRefNum, dirID, name));
    return noErr;
}

TRAP(Trap_CloseResFile) {
    UNUSED;
    M68KTB_CloseResFile((SInt16)Pop16());
    return noErr;
}

/* PROCEDURE CreateResFile(fileName: Str255) */
TRAP(Trap_CreateResFile) {
    UNUSED;
    Str255 name;
    ReadPString(Pop32(), name);
    VRefNum vref;
    DirID dir;
    char leaf[64];
    if (Resolve(0, 0, name, &vref, &dir, leaf) == noErr && leaf[0]) {
        FSSpec spec;
        spec.vRefNum = (SInt16)vref;
        spec.parID = (SInt32)dir;
        c2pstrcpy(spec.name, leaf);
        FSpCreateResFile(&spec, kAnyType, kAnyType, 0);
        M68KTB_SetResErr();
        return noErr;
    }
    W16(kLM_ResErr, (UInt16)bdNamErr);
    return noErr;
}

/* ------------------------------------------------------------------------
 * Standard File (Pack 3): the selector comes last, so it is popped first
 * ------------------------------------------------------------------------ */

enum { kLM_SFSaveDisk = 0x0214, kLM_CurDirStore = 0x0398 };

/* Where the dialog left off, as a volume and directory of the VFS */
static void ReplyPlace(const StandardFileReply* r, VRefNum* vref, DirID* dir) {
    VolumeControlBlock vcb;
    *vref = (VRefNum)r->sfFile.vRefNum;
    if (!VFS_GetVolumeInfo(*vref, &vcb)) {
        *vref = gDefaultVRef;
        VFS_GetVolumeInfo(*vref, &vcb);
    }
    *dir = r->sfFile.parID > 0 ? (DirID)r->sfFile.parID : vcb.rootID;
}

/* SFReply: good, copy, fType, vRefNum (a working directory), version, fName */
static void WriteSFReply(UInt32 a, const StandardFileReply* r) {
    VRefNum vref;
    DirID dir;
    ReplyPlace(r, &vref, &dir);
    W8(a + 0, r->sfGood ? 0xFF : 0);
    W8(a + 1, 0);
    W32(a + 2, r->sfType);
    W16(a + 6, (UInt16)WDFor(vref, dir));
    W16(a + 8, 0);
    WritePString(a + 10, r->sfFile.name);
}

/* StandardFileReply (IM VI 26-9) */
static void WriteStandardReply(UInt32 a, const StandardFileReply* r) {
    VRefNum vref;
    DirID dir;
    ReplyPlace(r, &vref, &dir);
    W8(a + 0, r->sfGood ? 0xFF : 0);
    W8(a + 1, r->sfReplacing ? 0xFF : 0);
    W32(a + 2, r->sfType);
    W16(a + 6, (UInt16)VolRef(vref));
    W32(a + 8, dir);
    WritePString(a + 12, r->sfFile.name);
    W16(a + 76, 0);
    W16(a + 78, 0);
    W8(a + 80, 0);
    W8(a + 81, 0);
    W32(a + 82, 0);
    W16(a + 86, 0);
}

static void ReadTypes(UInt32 list, SInt16 n, OSType* types) {
    if (n < 0 || n > 4) n = n < 0 ? -1 : 4;
    for (SInt16 i = 0; i < n; i++) types[i] = list ? R32(list + 4u * (UInt32)i) : 0;
}

TRAP(Trap_Pack3) {
    UNUSED;
    UInt16 selector = Pop16();
    StandardFileReply r;
    memset(&r, 0, sizeof(r));
    /* The dialog opens where the globals say - a program may set them to
     * choose the folder - and they say where it was left */
    StandardFile_SetStartLocation((short)R16(kLM_SFSaveDisk), (long)R32(kLM_CurDirStore));
    switch (selector) {
        case 1:                                      /* SFPutFile */
        case 3: {                                    /* SFPPutFile */
            if (selector == 3) {
                (void)Pop32();                       /* filterProc */
                (void)Pop16();                       /* dlgID */
            }
            UInt32 reply = Pop32();
            (void)Pop32();                           /* dlgHook */
            Str255 orig, prompt;
            ReadPString(Pop32(), orig);
            ReadPString(Pop32(), prompt);
            (void)Pop32();                           /* where */
            StandardPutFile(prompt, orig, &r);
            WriteSFReply(reply, &r);
            break;
        }
        case 2:                                      /* SFGetFile */
        case 4: {                                    /* SFPGetFile */
            if (selector == 4) {
                (void)Pop32();
                (void)Pop16();
            }
            UInt32 reply = Pop32();
            (void)Pop32();                           /* dlgHook */
            UInt32 list = Pop32();
            SInt16 n = (SInt16)Pop16();
            (void)Pop32();                           /* fileFilter */
            (void)Pop32();                           /* prompt */
            (void)Pop32();                           /* where */
            OSType types[4];
            ReadTypes(list, n, types);
            StandardGetFile(NULL, n > 4 ? 4 : n, types, &r);
            WriteSFReply(reply, &r);
            break;
        }
        case 5: {                                    /* StandardPutFile */
            UInt32 reply = Pop32();
            Str255 def, prompt;
            ReadPString(Pop32(), def);
            ReadPString(Pop32(), prompt);
            StandardPutFile(prompt, def, &r);
            WriteStandardReply(reply, &r);
            break;
        }
        case 6: {                                    /* StandardGetFile */
            UInt32 reply = Pop32();
            UInt32 list = Pop32();
            SInt16 n = (SInt16)Pop16();
            (void)Pop32();
            OSType types[4];
            ReadTypes(list, n, types);
            StandardGetFile(NULL, n > 4 ? 4 : n, types, &r);
            WriteStandardReply(reply, &r);
            break;
        }
        default:
            break;
    }
    short sv;
    long sd;
    StandardFile_GetStartLocation(&sv, &sd);
    W16(kLM_SFSaveDisk, (UInt16)sv);
    W32(kLM_CurDirStore, (UInt32)sd);
    Obj_SyncWindows();
    return noErr;
}

/* ------------------------------------------------------------------------ */

/* A name as a program gives one, relative to vRefNum (a volume, a working
 * directory, or 0 for the default) and dirID: the directory it is in, and
 * the leaf. For _Launch. */
OSErr M68KFiles_ResolveName(SInt16 vRefNum, SInt32 dirID, ConstStr255Param name,
                            VRefNum* vref, DirID* dir, char* leaf) {
    return Resolve(vRefNum, dirID, name, vref, dir, leaf);
}

void M68KFiles_Prepare(VRefNum vref, DirID dir) {
    memset(gWD, 0, sizeof(gWD));
    memset(gFCB, 0, sizeof(gFCB));
    gDefaultVRef = vref;
    gDefaultDir = dir ? dir : RootOf(vref);
    W16(kLM_SFSaveDisk, (UInt16)(-VolRef(gDefaultVRef)));
    W32(kLM_CurDirStore, gDefaultDir);
}

void M68KFiles_Finish(void) {
    for (int i = 0; i < kMaxFCB; i++) {
        if (gFCB[i].f) {
            VFS_CloseFile(gFCB[i].f);
            gFCB[i].f = NULL;
        }
    }
}

const M68KTrapEntry kM68KFileTraps[] = {
    { 0xA000, Trap_Open },          { 0xA00A, Trap_OpenRF },        { 0xA001, Trap_Close },
    { 0xA002, Trap_Read },          { 0xA003, Trap_Write },         { 0xA011, Trap_GetEOF },
    { 0xA012, Trap_SetEOF },        { 0xA018, Trap_GetFPos },       { 0xA044, Trap_SetFPos },
    { 0xA008, Trap_Create },        { 0xA009, Trap_Delete },        { 0xA00B, Trap_Rename },
    { 0xA00C, Trap_GetFileInfo },   { 0xA00D, Trap_SetFileInfo },   { 0xA014, Trap_GetVol },
    { 0xA015, Trap_SetVol },        { 0xA007, Trap_GetVolInfo },
    { 0xA013, Trap_FileNoOp },      /* FlushVol */
    { 0xA045, Trap_FileNoOp },      /* FlushFile */
    { 0xA010, Trap_FileNoOp },      /* Allocate */
    { 0xA017, Trap_FileNoOp },      /* Eject */
    { 0xA060, Trap_HFSDispatch },   { 0xAA52, Trap_HighLevelFS },
    { 0xA997, Trap_OpenResFile },   { 0xA9C4, Trap_OpenRFPerm },    { 0xA81A, Trap_HOpenResFile },
    { 0xA99A, Trap_CloseResFile },  { 0xA9B1, Trap_CreateResFile },
    { 0xA9EA, Trap_Pack3 },
};
const int kM68KFileTrapCount = (int)(sizeof(kM68KFileTraps) / sizeof(kM68KFileTraps[0]));

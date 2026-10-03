/*
 * ResourceMgr.c - the Resource Manager
 * Inside Macintosh: More Macintosh Toolbox, chapter 1. Clean-room.
 *
 * Each open resource file is its whole fork, held in memory: the header,
 * the resource data, and the map - type list, reference lists, name list
 * (1-121). GetResource searches the chain the way the Macintosh does: the
 * current file, then each file opened before it, back to the System file.
 * Files outside that chain are searched last, for the system's own lookups
 * of resources it opened for itself (the localized strings).
 *
 * A loaded resource is one handle, the same every time it is asked for,
 * until it is released or its file closes. Changes - AddResource,
 * RemoveResource, SetResInfo, ChangedResource - rebuild the fork in memory;
 * UpdateResFile, WriteResource and CloseResFile write it to the file.
 */

#include "SystemTypes.h"
#include "MacTypes.h"
#include "FileManagerTypes.h"
#include "ResourceManager.h"
#include "FileManager.h"
#include "ResourceMgr/ResourceMgrPriv.h"
#include "ResourceMgr/ResourceLogging.h"
#include "System71StdLib.h"
#include "ToolboxCompat.h"
#include <string.h>

static ResourceMgrGlobals gResMgr = {
    .curResFile = -1,
    .resError = noErr,
    .resLoad = true,
};

UInt16 read_be16(const UInt8* p) { return (UInt16)((p[0] << 8) | p[1]); }
UInt32 read_be32(const UInt8* p) {
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) | ((UInt32)p[2] << 8) | p[3];
}
void write_be16(UInt8* p, UInt16 v) { p[0] = (UInt8)(v >> 8); p[1] = (UInt8)v; }
void write_be32(UInt8* p, UInt32 v) {
    p[0] = (UInt8)(v >> 24); p[1] = (UInt8)(v >> 16); p[2] = (UInt8)(v >> 8); p[3] = (UInt8)v;
}

/* ------------------------------------------------------------------------
 * Loaded resources: which handle stands for which resource of which file
 * ------------------------------------------------------------------------ */

#define RM_HANDLE_CAP 1024
typedef struct {
    Handle h;
    ResType type;
    ResID id;
    SInt16 homeFile;
    Boolean changed;            /* ChangedResource: written from the handle */
} HandleInfo;

static HandleInfo gLoaded[RM_HANDLE_CAP];
static int gLoadedCount;

static HandleInfo* FindHandleInfo(Handle h) {
    if (!h) return NULL;
    for (int i = 0; i < gLoadedCount; i++) if (gLoaded[i].h == h) return &gLoaded[i];
    return NULL;
}

static HandleInfo* FindLoaded(SInt16 file, ResType type, ResID id) {
    for (int i = 0; i < gLoadedCount; i++)
        if (gLoaded[i].homeFile == file && gLoaded[i].type == type && gLoaded[i].id == id)
            return &gLoaded[i];
    return NULL;
}

static void RecordHandleInfo(Handle h, ResType type, ResID id, SInt16 file) {
    if (FindHandleInfo(h) || gLoadedCount >= RM_HANDLE_CAP) return;
    HandleInfo* info = &gLoaded[gLoadedCount++];
    info->h = h;
    info->type = type;
    info->id = id;
    info->homeFile = file;
    info->changed = false;
}

static void HandleInfoForget(Handle h) {
    HandleInfo* info = FindHandleInfo(h);
    if (info) *info = gLoaded[--gLoadedCount];
}

/* ------------------------------------------------------------------------
 * Reading a map
 * ------------------------------------------------------------------------ */

static ResFile* FileFor(SInt16 refNum) {
    if (refNum < 0 || refNum >= MAX_RES_FILES || !gResMgr.resFiles[refNum].inUse) return NULL;
    return &gResMgr.resFiles[refNum];
}

static UInt16 MapTypeListOffset(ResFile* f) { return read_be16((UInt8*)&f->map->typeListOffset); }
static UInt16 MapNameListOffset(ResFile* f) { return read_be16((UInt8*)&f->map->nameListOffset); }

static int MapTypeCount(ResFile* f) {
    if (!f || !f->map) return 0;
    UInt16 tl = MapTypeListOffset(f);
    if (tl == 0xFFFF || (UInt32)tl + 2 > f->mapSize) return 0;
    int n = (SInt16)read_be16((UInt8*)f->map + tl) + 1;
    if (n < 0 || (UInt32)tl + 2 + (UInt32)n * sizeof(TypeListEntry) > f->mapSize) return 0;
    return n;
}

static TypeListEntry* MapType(ResFile* f, int index) {
    return (TypeListEntry*)((UInt8*)f->map + MapTypeListOffset(f) + 2 + index * sizeof(TypeListEntry));
}

TypeListEntry* ResMap_FindType(ResFile* file, ResType type) {
    int n = MapTypeCount(file);
    for (int i = 0; i < n; i++) {
        TypeListEntry* t = MapType(file, i);
        if (read_be32((UInt8*)&t->resType) == type) return t;
    }
    return NULL;
}

/* The count of a type's references, and the first of them; 0 when bad */
static int MapRefs(ResFile* f, TypeListEntry* t, RefListEntry** first) {
    int n = read_be16((UInt8*)&t->count) + 1;
    UInt32 start = (UInt32)MapTypeListOffset(f) + read_be16((UInt8*)&t->refListOffset);
    if (start + (UInt32)n * sizeof(RefListEntry) > f->mapSize) return 0;
    *first = (RefListEntry*)((UInt8*)f->map + start);
    return n;
}

RefListEntry* ResMap_FindResource(ResFile* file, ResType type, ResID id) {
    TypeListEntry* t = file ? ResMap_FindType(file, type) : NULL;
    if (!t) return NULL;
    RefListEntry* refs;
    int n = MapRefs(file, t, &refs);
    for (int i = 0; i < n; i++)
        if ((ResID)read_be16((UInt8*)&refs[i].resID) == id) return &refs[i];
    return NULL;
}

/* The name a reference has, as a Pascal string in the map, or NULL */
static const UInt8* RefName(ResFile* f, RefListEntry* ref) {
    UInt16 off = read_be16((UInt8*)&ref->nameOffset);
    UInt16 nl = MapNameListOffset(f);
    if (off == 0xFFFF || nl == 0xFFFF) return NULL;
    UInt32 at = (UInt32)nl + off;
    if (at >= f->mapSize || at + 1 + ((UInt8*)f->map)[at] > f->mapSize) return NULL;
    return (UInt8*)f->map + at;
}

/* A reference's data: its bytes and length, or NULL */
static const UInt8* RefData(ResFile* f, RefListEntry* ref, UInt32* len) {
    UInt32 off = ((UInt32)ref->dataOffsetHi << 16) | read_be16((UInt8*)&ref->dataOffsetLo);
    UInt32 base = read_be32(f->data);
    UInt32 at = base + off;
    if (base >= f->dataSize || at < base || at + 4 > f->dataSize) return NULL;
    UInt32 n = read_be32(f->data + at);
    if (at + 4 + n > f->dataSize || at + 4 + n < at) return NULL;
    *len = n;
    return f->data + at + 4;
}

Handle ResFile_LoadResource(ResFile* file, RefListEntry* ref) {
    UInt32 len = 0;
    const UInt8* data = file && ref ? RefData(file, ref, &len) : NULL;
    if (!data) {
        gResMgr.resError = mapReadErr;
        return NULL;
    }
    Handle h = NewHandle(len);
    if (!h) {
        gResMgr.resError = memFullErr;
        return NULL;
    }
    if (len) BlockMove(data, *h, (Size)len);
    return h;
}

/* The handle for a reference in a file: the one already loaded, or a new
 * one, or NULL with SetResLoad(false) */
static Handle LoadRef(ResFile* f, ResType type, RefListEntry* ref) {
    ResID id = (ResID)read_be16((UInt8*)&ref->resID);
    HandleInfo* info = FindLoaded(f->refNum, type, id);
    if (info) {
        gResMgr.resError = noErr;
        return info->h;
    }
    if (!gResMgr.resLoad) {
        gResMgr.resError = noErr;
        return NULL;
    }
    Handle h = ResFile_LoadResource(f, ref);
    if (h) {
        RecordHandleInfo(h, type, id, f->refNum);
        gResMgr.resError = noErr;
    }
    return h;
}

/* ------------------------------------------------------------------------
 * The search chain
 * ------------------------------------------------------------------------ */

/* The files to search, in order: the current file and those opened before
 * it, newest first; then, when outside is set, the rest */
static int Chain(ResFile** out, Boolean outside) {
    int n = 0;
    ResFile* cur = FileFor(gResMgr.curResFile);
    UInt32 limit = cur ? cur->openSeq : 0;
    Boolean taken[MAX_RES_FILES] = { false };
    for (;;) {
        int best = -1;
        for (int i = 0; i < MAX_RES_FILES; i++) {
            ResFile* f = &gResMgr.resFiles[i];
            if (!f->inUse || taken[i] || f->openSeq > limit) continue;
            if (best < 0 || f->openSeq > gResMgr.resFiles[best].openSeq) best = i;
        }
        if (best < 0) break;
        taken[best] = true;
        out[n++] = &gResMgr.resFiles[best];
    }
    if (outside) {
        for (int i = 0; i < MAX_RES_FILES; i++)
            if (gResMgr.resFiles[i].inUse && !taken[i]) out[n++] = &gResMgr.resFiles[i];
    }
    return n;
}

static Handle BuiltinPattern(ResID id) {
    static const UInt8 kPatterns[10][8] = {
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55},
        {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00},
        {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
        {0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11},
        {0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0},
        {0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88},
        {0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44}
    };
    Handle h = NewHandle(8);
    if (h) BlockMove(kPatterns[id - 1], *h, 8);
    gResMgr.resError = h ? noErr : memFullErr;
    return h;
}

Handle GetResource(ResType theType, ResID theID) {
    ResFile* files[MAX_RES_FILES];
    int n = Chain(files, true);
    for (int i = 0; i < n; i++) {
        RefListEntry* ref = ResMap_FindResource(files[i], theType, theID);
        if (ref) return LoadRef(files[i], theType, ref);
    }
    /* The standard patterns, when no file has them */
    if (theType == FOURCC('P','A','T',' ') && theID >= 1 && theID <= 10) return BuiltinPattern(theID);
    gResMgr.resError = resNotFound;
    return NULL;
}

Handle Get1Resource(ResType theType, ResID theID) {
    ResFile* f = FileFor(gResMgr.curResFile);
    RefListEntry* ref = f ? ResMap_FindResource(f, theType, theID) : NULL;
    if (!ref) {
        gResMgr.resError = resNotFound;
        return NULL;
    }
    return LoadRef(f, theType, ref);
}

static RefListEntry* FindNamed(ResFile* f, ResType type, ConstStr255Param name) {
    TypeListEntry* t = ResMap_FindType(f, type);
    if (!t) return NULL;
    RefListEntry* refs;
    int n = MapRefs(f, t, &refs);
    for (int i = 0; i < n; i++) {
        const UInt8* s = RefName(f, &refs[i]);
        if (!s || s[0] != name[0]) continue;
        Boolean same = true;
        for (int k = 1; k <= s[0] && same; k++) same = s[k] == name[k];
        if (same) return &refs[i];
    }
    return NULL;
}

Handle GetNamedResource(ResType theType, ConstStr255Param name) {
    if (name && name[0]) {
        ResFile* files[MAX_RES_FILES];
        int n = Chain(files, true);
        for (int i = 0; i < n; i++) {
            RefListEntry* ref = FindNamed(files[i], theType, name);
            if (ref) return LoadRef(files[i], theType, ref);
        }
    }
    gResMgr.resError = resNotFound;
    return NULL;
}

Handle Get1NamedResource(ResType theType, ConstStr255Param name) {
    ResFile* f = FileFor(gResMgr.curResFile);
    RefListEntry* ref = f && name && name[0] ? FindNamed(f, theType, name) : NULL;
    if (!ref) {
        gResMgr.resError = resNotFound;
        return NULL;
    }
    return LoadRef(f, theType, ref);
}

/* ------------------------------------------------------------------------
 * Counting and indexing
 * ------------------------------------------------------------------------ */

static int CountIn(ResFile* f, ResType type) {
    TypeListEntry* t = ResMap_FindType(f, type);
    RefListEntry* refs;
    return t ? MapRefs(f, t, &refs) : 0;
}

SInt16 CountResources(ResType theType) {
    ResFile* files[MAX_RES_FILES];
    int n = Chain(files, false), count = 0;
    for (int i = 0; i < n; i++) count += CountIn(files[i], theType);
    gResMgr.resError = noErr;
    return (SInt16)count;
}

SInt16 Count1Resources(ResType theType) {
    ResFile* f = FileFor(gResMgr.curResFile);
    gResMgr.resError = noErr;
    return (SInt16)(f ? CountIn(f, theType) : 0);
}

/* The index'th of a type through the chain, 1 first */
Handle GetIndResource(ResType theType, SInt16 index) {
    ResFile* files[MAX_RES_FILES];
    int n = Chain(files, false);
    for (int i = 0; i < n && index >= 1; i++) {
        TypeListEntry* t = ResMap_FindType(files[i], theType);
        if (!t) continue;
        RefListEntry* refs;
        int c = MapRefs(files[i], t, &refs);
        if (index <= c) return LoadRef(files[i], theType, &refs[index - 1]);
        index = (SInt16)(index - c);
    }
    gResMgr.resError = resNotFound;
    return NULL;
}

Handle Get1IndResource(ResType theType, SInt16 index) {
    ResFile* f = FileFor(gResMgr.curResFile);
    TypeListEntry* t = f ? ResMap_FindType(f, theType) : NULL;
    RefListEntry* refs;
    int c = t ? MapRefs(f, t, &refs) : 0;
    if (index < 1 || index > c) {
        gResMgr.resError = resNotFound;
        return NULL;
    }
    return LoadRef(f, theType, &refs[index - 1]);
}

/* The distinct types through the chain, in order; the index'th, or the count */
static int ChainTypes(int index, ResType* out) {
    ResFile* files[MAX_RES_FILES];
    int n = Chain(files, false), count = 0;
    enum { kMaxTypes = 512 };
    static ResType seen[kMaxTypes];
    int nseen = 0;
    for (int i = 0; i < n; i++) {
        int tn = MapTypeCount(files[i]);
        for (int k = 0; k < tn; k++) {
            ResType type = read_be32((UInt8*)&MapType(files[i], k)->resType);
            Boolean dup = false;
            for (int s = 0; s < nseen && !dup; s++) dup = seen[s] == type;
            if (dup) continue;
            if (nseen < kMaxTypes) seen[nseen++] = type;
            if (++count == index && out) {
                *out = type;
                return count;
            }
        }
    }
    return count;
}

SInt16 CountTypes(void) {
    gResMgr.resError = noErr;
    return (SInt16)ChainTypes(0, NULL);
}

SInt16 Count1Types(void) {
    gResMgr.resError = noErr;
    return (SInt16)MapTypeCount(FileFor(gResMgr.curResFile));
}

void GetIndType(ResType* theType, SInt16 index) {
    if (!theType) return;
    *theType = 0;
    if (index < 1 || ChainTypes(index, theType) < index) {
        *theType = 0;
        gResMgr.resError = resNotFound;
        return;
    }
    gResMgr.resError = noErr;
}

void Get1IndType(ResType* theType, SInt16 index) {
    if (!theType) return;
    ResFile* f = FileFor(gResMgr.curResFile);
    if (!f || index < 1 || index > MapTypeCount(f)) {
        *theType = 0;
        gResMgr.resError = resNotFound;
        return;
    }
    *theType = read_be32((UInt8*)&MapType(f, index - 1)->resType);
    gResMgr.resError = noErr;
}

/* An ID no file in the chain uses for the type, of at least 128 */
static SInt16 Unique(ResType type, Boolean oneFile) {
    static UInt32 seed = 0x2F6B;
    for (int tries = 0; tries < 20000; tries++) {
        seed = seed * 1103515245u + 12345u;
        SInt16 id = (SInt16)(128 + (seed >> 8) % 32000);
        Boolean used = false;
        if (oneFile) {
            ResFile* f = FileFor(gResMgr.curResFile);
            used = f && ResMap_FindResource(f, type, id);
        } else {
            ResFile* files[MAX_RES_FILES];
            int n = Chain(files, false);
            for (int i = 0; i < n && !used; i++) used = ResMap_FindResource(files[i], type, id) != NULL;
        }
        if (!used) {
            gResMgr.resError = noErr;
            return id;
        }
    }
    return 0;
}

SInt16 UniqueID(ResType theType) { return Unique(theType, false); }
SInt16 Unique1ID(ResType theType) { return Unique(theType, true); }

/* ------------------------------------------------------------------------
 * Information about a resource
 * ------------------------------------------------------------------------ */

static RefListEntry* RefOf(Handle h, ResFile** file) {
    HandleInfo* info = FindHandleInfo(h);
    ResFile* f = info ? FileFor(info->homeFile) : NULL;
    if (file) *file = f;
    return f ? ResMap_FindResource(f, info->type, info->id) : NULL;
}

void GetResInfo(Handle theResource, ResID* theID, ResType* theType, char* name) {
    HandleInfo* info = FindHandleInfo(theResource);
    if (!info) {
        if (theID) *theID = 0;
        if (theType) *theType = 0;
        if (name) name[0] = 0;
        gResMgr.resError = resNotFound;
        return;
    }
    if (theID) *theID = info->id;
    if (theType) *theType = info->type;
    if (name) {
        ResFile* f;
        RefListEntry* ref = RefOf(theResource, &f);
        const UInt8* s = ref ? RefName(f, ref) : NULL;
        if (s) BlockMove(s, name, s[0] + 1);
        else name[0] = 0;
    }
    gResMgr.resError = noErr;
}

SInt16 GetResAttrs(Handle theResource) {
    RefListEntry* ref = RefOf(theResource, NULL);
    gResMgr.resError = ref ? noErr : resNotFound;
    return ref ? ref->attributes : 0;
}

void SetResAttrs(Handle theResource, SInt16 attrs) {
    ResFile* f;
    RefListEntry* ref = RefOf(theResource, &f);
    if (!ref) {
        gResMgr.resError = resNotFound;
        return;
    }
    ref->attributes = (UInt8)attrs;
    if (f->writable) f->dirty = true;
    gResMgr.resError = noErr;
}

Size GetResourceSizeOnDisk(Handle theResource) {
    ResFile* f;
    RefListEntry* ref = RefOf(theResource, &f);
    UInt32 len = 0;
    if (!ref || !RefData(f, ref, &len)) {
        gResMgr.resError = resNotFound;
        return 0;
    }
    gResMgr.resError = noErr;
    return (Size)len;
}

Size GetMaxResourceSize(Handle theResource) {
    Size n = GetResourceSizeOnDisk(theResource);
    Size m = theResource ? GetHandleSize(theResource) : 0;
    return m > n ? m : n;
}

SInt16 HomeResFile(Handle theResource) {
    HandleInfo* info = FindHandleInfo(theResource);
    gResMgr.resError = info ? noErr : resNotFound;
    return info ? info->homeFile : -1;
}

void LoadResource(Handle theResource) {
    gResMgr.resError = FindHandleInfo(theResource) ? noErr : resNotFound;
}

void SetResLoad(Boolean load) { gResMgr.resLoad = load; }

/* The handle is no longer the resource's: the next GetResource reads it
 * afresh (1-91) */
/* A changed resource stays until it is written (1-120) */
void ReleaseResource(Handle theResource) {
    HandleInfo* info = FindHandleInfo(theResource);
    if (!info) {
        gResMgr.resError = resNotFound;
        return;
    }
    if (info->changed) {
        gResMgr.resError = resAttrErr;
        return;
    }
    HandleInfoForget(theResource);
    DisposeHandle(theResource);
    gResMgr.resError = noErr;
}

void DetachResource(Handle theResource) {
    if (!FindHandleInfo(theResource)) {
        gResMgr.resError = resNotFound;
        return;
    }
    HandleInfoForget(theResource);
    gResMgr.resError = noErr;
}

/* ------------------------------------------------------------------------
 * Files
 * ------------------------------------------------------------------------ */

OSErr ResError(void) {
    OSErr err = gResMgr.resError;
    gResMgr.resError = noErr;
    return err;
}

SInt16 CurResFile(void) { return gResMgr.curResFile; }

void UseResFile(SInt16 refNum) {
    if (!FileFor(refNum)) {
        gResMgr.resError = resFNotFound;
        return;
    }
    gResMgr.curResFile = refNum;
    gResMgr.resError = noErr;
}

/* A fork in memory, as an open file in slot i */
static Boolean Attach(int i, UInt8* data, UInt32 size, Handle owner) {
    if (size < sizeof(ResourceHeader)) return false;
    UInt32 mapOffset = read_be32(data + 4), mapLength = read_be32(data + 12);
    if (mapOffset > size || mapLength < sizeof(ResMapHeader) || mapOffset + mapLength > size) return false;
    ResFile* f = &gResMgr.resFiles[i];
    f->inUse = true;
    f->refNum = (SInt16)i;
    f->data = data;
    f->dataSize = size;
    f->map = (ResMapHeader*)(data + mapOffset);
    f->mapSize = mapLength;
    f->mapHandle = owner;
    f->openSeq = gResMgr.nextSeq++;
    f->writable = false;
    f->dirty = false;
    f->fileName[0] = 0;
    f->vRefNum = 0;
    f->dirID = 0;
    return true;
}

static int FreeSlot(void) {
    for (int i = 1; i < MAX_RES_FILES; i++) if (!gResMgr.resFiles[i].inUse) return i;
    return -1;
}

/* The whole fork of a file, read into a handle */
static Handle ReadFork(FileRefNum ref, UInt32* size) {
    UInt8 hdr[16];
    UInt32 n = sizeof(hdr);
    if (FSRead(ref, &n, hdr) != noErr || n != sizeof(hdr)) return NULL;
    UInt32 mapOffset = read_be32(hdr + 4), mapLength = read_be32(hdr + 12);
    if (mapOffset > 0x7FFFFFFF - mapLength) return NULL;
    UInt32 total = mapOffset + mapLength;
    Handle h = NewHandle(total);
    if (!h) return NULL;
    n = total;
    HLock(h);
    OSErr err = FSSetFPos(ref, fsFromStart, 0);
    if (err == noErr) err = FSRead(ref, &n, *h);
    HUnlock(h);
    if (err != noErr || n != total) {
        DisposeHandle(h);
        return NULL;
    }
    *size = total;
    return h;
}

void InitResourceManager(void) {
    for (int i = 0; i < MAX_RES_FILES; i++) {
        gResMgr.resFiles[i].inUse = false;
        gResMgr.resFiles[i].refNum = -1;
    }
    gLoadedCount = 0;
    gResMgr.nextSeq = 0;

    /* The System file from the startup disk, or the resources built in */
    FileRefNum ref;
    UInt32 size = 0;
    Handle fork = NULL;
    if (FSOpenRF(PSTR("System"), 0, &ref) == noErr) {
        fork = ReadFork(ref, &size);
        FSClose(ref);
    }
    if (fork && Attach(0, (UInt8*)*fork, size, fork)) {
        serial_puts("[ResourceMgr] System file loaded from disk\n");
    } else {
        if (fork) DisposeHandle(fork);
        extern const unsigned char patterns_rsrc_data[];
        extern const unsigned int patterns_rsrc_size;
        if (!Attach(0, (UInt8*)(uintptr_t)patterns_rsrc_data, patterns_rsrc_size, NULL)) {
            serial_puts("[ResourceMgr] Warning: no usable System resources\n");
            gResMgr.resFiles[0].inUse = true;
            gResMgr.resFiles[0].refNum = 0;
        } else {
            serial_puts("[ResourceMgr] Using embedded System resources\n");
        }
    }
    gResMgr.curResFile = 0;
    gResMgr.resError = noErr;
}

void ShutdownResourceManager(void) {
    for (int i = 1; i < MAX_RES_FILES; i++)
        if (gResMgr.resFiles[i].inUse) CloseResFile((SInt16)i);
}

static SInt16 OpenResFileIn(short vRefNum, long dirID, ConstStr255Param fileName) {
    /* Opening a file already open answers it again (1-93) */
    for (int i = 1; i < MAX_RES_FILES; i++) {
        ResFile* f = &gResMgr.resFiles[i];
        if (!f->inUse || !f->writable || f->fileName[0] != fileName[0]) continue;
        if (vRefNum && f->vRefNum != vRefNum) continue;
        if (dirID && f->dirID != dirID) continue;
        Boolean same = true;
        for (int k = 1; k <= fileName[0] && same; k++) same = f->fileName[k] == fileName[k];
        if (same) {
            gResMgr.curResFile = (SInt16)i;
            gResMgr.resError = noErr;
            return (SInt16)i;
        }
    }
    int slot = FreeSlot();
    if (slot < 0) {
        gResMgr.resError = tmfoErr;
        return -1;
    }
    FileRefNum ref;
    OSErr err = HOpenRF(vRefNum, dirID, fileName, fsRdPerm, &ref);
    if (err != noErr) {
        gResMgr.resError = err;
        return -1;
    }
    UInt32 size = 0;
    Handle fork = ReadFork(ref, &size);
    FSClose(ref);
    if (!fork || !Attach(slot, (UInt8*)*fork, size, fork)) {
        if (fork) DisposeHandle(fork);
        gResMgr.resError = mapReadErr;
        return -1;
    }
    ResFile* f = &gResMgr.resFiles[slot];
    BlockMove(fileName, f->fileName, fileName[0] + 1);
    f->vRefNum = vRefNum;
    f->dirID = dirID;
    f->writable = true;
    gResMgr.curResFile = (SInt16)slot;
    gResMgr.resError = noErr;
    return (SInt16)slot;
}

SInt16 OpenResFile(ConstStr255Param fileName) {
    return fileName ? OpenResFileIn(0, 0, fileName) : -1;
}

/* The permission is not enforced: a file opened read-only is simply never
 * written, there being nothing changed in it to write */
SInt16 FSpOpenResFile(const FSSpec* spec, SInt8 permission) {
    (void)permission;
    if (!spec) {
        gResMgr.resError = paramErr;
        return -1;
    }
    return OpenResFileIn(spec->vRefNum, spec->parID, spec->name);
}

/* An empty fork (1-121) where there is none: a 256-byte header area with the map at 256, and a
 * map with no types */
void FSpCreateResFile(const FSSpec* spec, OSType creator, OSType fileType, ScriptCode scriptTag) {
    (void)scriptTag;
    if (!spec) {
        gResMgr.resError = paramErr;
        return;
    }
    OSErr err = HCreate(spec->vRefNum, spec->parID, spec->name, creator, fileType);
    if (err != noErr && err != dupFNErr) {
        gResMgr.resError = err;
        return;
    }
    enum { kHeaderArea = 256, kMapSize = 30 };
    UInt8 fork[kHeaderArea + kMapSize];
    memset(fork, 0, sizeof(fork));
    write_be32(fork + 0, kHeaderArea);
    write_be32(fork + 4, kHeaderArea);
    write_be32(fork + 8, 0);
    write_be32(fork + 12, kMapSize);
    memcpy(fork + kHeaderArea, fork, 16);
    write_be16(fork + kHeaderArea + 24, 28);
    write_be16(fork + kHeaderArea + 26, kMapSize);
    write_be16(fork + kHeaderArea + 28, 0xFFFF);
    FileRefNum ref;
    err = HOpenRF(spec->vRefNum, spec->parID, spec->name, fsRdWrPerm, &ref);
    if (err != noErr) {
        gResMgr.resError = err;
        return;
    }
    /* A fork already there is left as it is (1-114) */
    UInt32 eof = 0;
    if (FSGetEOF(ref, &eof) == noErr && eof > 0) {
        FSClose(ref);
        gResMgr.resError = dupFNErr;
        return;
    }
    UInt32 count = sizeof(fork);
    err = FSWrite(ref, &count, fork);
    if (err == noErr && count != sizeof(fork)) err = ioErr;
    OSErr closeErr = FSClose(ref);
    gResMgr.resError = err != noErr ? err : closeErr;
}

/* ------------------------------------------------------------------------
 * Changing a file: its resources listed, changed, and laid out afresh
 * ------------------------------------------------------------------------ */

typedef struct {
    ResType type;
    ResID id;
    UInt8 attrs;
    const UInt8* name;          /* Pascal string, or NULL */
    const UInt8* data;
    UInt32 len;
} Entry;

/* Every resource of a file, with a handle's contents in place of the file's
 * where the resource was changed */
static int ListFile(ResFile* f, Entry* out, int max) {
    int n = 0;
    int tn = MapTypeCount(f);
    for (int t = 0; t < tn; t++) {
        TypeListEntry* te = MapType(f, t);
        ResType type = read_be32((UInt8*)&te->resType);
        RefListEntry* refs;
        int rn = MapRefs(f, te, &refs);
        for (int r = 0; r < rn && n < max; r++) {
            Entry* e = &out[n];
            e->type = type;
            e->id = (ResID)read_be16((UInt8*)&refs[r].resID);
            e->attrs = refs[r].attributes;
            e->name = RefName(f, &refs[r]);
            HandleInfo* info = FindLoaded(f->refNum, type, e->id);
            if (info && info->changed && info->h && *info->h) {
                e->data = (const UInt8*)*info->h;
                e->len = (UInt32)GetHandleSize(info->h);
            } else {
                e->data = RefData(f, &refs[r], &e->len);
                if (!e->data) e->len = 0;
            }
            n++;
        }
    }
    return n;
}

/* The entries laid out as a fork (1-121), replacing the file's */
static OSErr Layout(ResFile* f, const Entry* e, int n) {
    enum { kHeaderArea = 256 };
    /* The types, in first-seen order */
    ResType types[512];
    int tcount = 0;
    for (int i = 0; i < n; i++) {
        Boolean seen = false;
        for (int k = 0; k < tcount && !seen; k++) seen = types[k] == e[i].type;
        if (!seen && tcount < 512) types[tcount++] = e[i].type;
    }
    UInt32 dataLen = 0, namesLen = 0;
    for (int i = 0; i < n; i++) {
        dataLen += 4 + e[i].len;
        if (e[i].name) namesLen += 1u + e[i].name[0];
    }
    UInt32 typeListLen = 2 + 8u * (UInt32)tcount + 12u * (UInt32)n;
    UInt32 mapLen = 28 + typeListLen + namesLen;
    UInt32 total = kHeaderArea + dataLen + mapLen;
    if (total > 0x00FFFFFF) return mapReadErr;
    Handle h = NewHandle(total);
    if (!h) return memFullErr;
    UInt8* p = (UInt8*)*h;
    memset(p, 0, total);

    write_be32(p + 0, kHeaderArea);
    write_be32(p + 4, kHeaderArea + dataLen);
    write_be32(p + 8, dataLen);
    write_be32(p + 12, mapLen);
    UInt8* map = p + kHeaderArea + dataLen;
    memcpy(map, p, 16);
    UInt16 fileAttrs = f->map ? read_be16((UInt8*)&f->map->attributes) : 0;
    write_be16(map + 22, (UInt16)(fileAttrs & ~mapChanged));
    write_be16(map + 24, 28);
    write_be16(map + 26, (UInt16)(28 + typeListLen));
    UInt8* tl = map + 28;
    write_be16(tl, (UInt16)(tcount - 1));
    UInt32 dataAt = 0, nameAt = 0;
    UInt32 refAt = 2 + 8u * (UInt32)tcount;           /* from the type list */
    for (int t = 0; t < tcount; t++) {
        UInt8* te = tl + 2 + 8 * t;
        int count = 0;
        for (int i = 0; i < n; i++) if (e[i].type == types[t]) count++;
        write_be32(te, types[t]);
        write_be16(te + 4, (UInt16)(count - 1));
        write_be16(te + 6, (UInt16)refAt);
        for (int i = 0; i < n; i++) {
            if (e[i].type != types[t]) continue;
            UInt8* ref = tl + refAt;
            write_be16(ref, (UInt16)e[i].id);
            if (e[i].name) {
                write_be16(ref + 2, (UInt16)nameAt);
                UInt8* nm = map + 28 + typeListLen + nameAt;
                memcpy(nm, e[i].name, 1u + e[i].name[0]);
                nameAt += 1u + e[i].name[0];
            } else {
                write_be16(ref + 2, 0xFFFF);
            }
            ref[4] = (UInt8)(e[i].attrs & ~resChanged);
            ref[5] = (UInt8)(dataAt >> 16);
            write_be16(ref + 6, (UInt16)dataAt);
            UInt8* d = p + kHeaderArea + dataAt;
            write_be32(d, e[i].len);
            if (e[i].len) memcpy(d + 4, e[i].data, e[i].len);
            dataAt += 4 + e[i].len;
            refAt += 12;
        }
    }

    Handle old = f->mapHandle;
    f->data = p;
    f->dataSize = total;
    f->map = (ResMapHeader*)map;
    f->mapSize = mapLen;
    f->mapHandle = h;
    if (old) DisposeHandle(old);
    return noErr;
}

/* The file laid out again, with an entry added, one removed, or one renamed */
enum { kEditNone, kEditAdd, kEditRemove, kEditInfo };

static OSErr Rebuild(ResFile* f, int edit, const Entry* add, ResType type, ResID id,
                     ResID newID, ConstStr255Param newName) {
    enum { kMaxEntries = 4096 };
    Entry* list = (Entry*)NewPtr(sizeof(Entry) * kMaxEntries);
    if (!list) return memFullErr;
    int n = ListFile(f, list, kMaxEntries - 1);
    if (edit == kEditAdd && add) {
        list[n++] = *add;
    } else if (edit == kEditRemove || edit == kEditInfo) {
        for (int i = 0; i < n; i++) {
            if (list[i].type != type || list[i].id != id) continue;
            if (edit == kEditRemove) {
                list[i] = list[--n];
            } else {
                list[i].id = newID;
                if (newName) list[i].name = newName[0] ? newName : NULL;
            }
            break;
        }
    }
    /* The data and names point into the old fork until the new one is made */
    OSErr err = Layout(f, list, n);
    DisposePtr((Ptr)list);
    if (err == noErr) {
        f->dirty = true;
        for (int i = 0; i < gLoadedCount; i++)
            if (gLoaded[i].homeFile == f->refNum) gLoaded[i].changed = false;
    }
    return err;
}

/* Changes are made in memory in any file; only a file on disk is written */
static ResFile* CurrentFile(void) {
    ResFile* f = FileFor(gResMgr.curResFile);
    if (!f) gResMgr.resError = resFNotFound;
    return f;
}

/* theData becomes resource (type, id) of the current file (1-98) */
void AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name) {
    ResFile* f = CurrentFile();
    if (!theData || !*theData) {
        gResMgr.resError = addResFailed;
        return;
    }
    if (!f || FindHandleInfo(theData)) {
        gResMgr.resError = addResFailed;
        return;
    }
    Entry e = { theType, theID, 0, (name && name[0]) ? name : NULL,
                (const UInt8*)*theData, (UInt32)GetHandleSize(theData) };
    HLock(theData);
    OSErr err = Rebuild(f, kEditAdd, &e, 0, 0, 0, NULL);
    HUnlock(theData);
    if (err == noErr) RecordHandleInfo(theData, theType, theID, f->refNum);
    gResMgr.resError = err == noErr ? noErr : addResFailed;
}

void RemoveResource(Handle theResource) {
    HandleInfo* info = FindHandleInfo(theResource);
    ResFile* f = info ? FileFor(info->homeFile) : NULL;
    if (!f) {
        gResMgr.resError = rmvResFailed;
        return;
    }
    OSErr err = Rebuild(f, kEditRemove, NULL, info->type, info->id, 0, NULL);
    if (err == noErr) HandleInfoForget(theResource);
    gResMgr.resError = err == noErr ? noErr : rmvResFailed;
}

/* SetResInfo(theResource, theID, name): a new ID, and a new name unless it
 * is NULL (1-104) */
void SetResInfo(Handle theResource, ResID theID, ConstStr255Param name) {
    HandleInfo* info = FindHandleInfo(theResource);
    ResFile* f = info ? FileFor(info->homeFile) : NULL;
    if (!f) {
        gResMgr.resError = resNotFound;
        return;
    }
    OSErr err = Rebuild(f, kEditInfo, NULL, info->type, info->id, theID, name);
    if (err == noErr) info->id = theID;
    gResMgr.resError = err;
}

void ChangedResource(Handle theResource) {
    ResFile* f;
    RefListEntry* ref = RefOf(theResource, &f);
    if (!ref) {
        gResMgr.resError = resNotFound;
        return;
    }
    FindHandleInfo(theResource)->changed = true;
    ref->attributes |= resChanged;
    f->dirty = true;
    gResMgr.resError = noErr;
}

SInt16 GetResFileAttrs(SInt16 refNum) {
    ResFile* f = FileFor(refNum);
    gResMgr.resError = f ? noErr : resFNotFound;
    return f && f->map ? (SInt16)read_be16((UInt8*)&f->map->attributes) : 0;
}

void SetResFileAttrs(SInt16 refNum, SInt16 attrs) {
    ResFile* f = FileFor(refNum);
    if (!f || !f->map) {
        gResMgr.resError = resFNotFound;
        return;
    }
    write_be16((UInt8*)&f->map->attributes, (UInt16)attrs);
    if (f->writable) f->dirty = true;
    gResMgr.resError = noErr;
}

/* The fork as it now is, to the file */
static OSErr WriteFork(ResFile* f) {
    Boolean anyChanged = false;
    for (int i = 0; i < gLoadedCount; i++)
        if (gLoaded[i].homeFile == f->refNum && gLoaded[i].changed) anyChanged = true;
    if (anyChanged) {
        OSErr err = Rebuild(f, kEditNone, NULL, 0, 0, 0, NULL);
        if (err != noErr) return err;
    }
    FileRefNum ref;
    OSErr err = HOpenRF(f->vRefNum, f->dirID, f->fileName, fsRdWrPerm, &ref);
    if (err != noErr) return err;
    UInt32 count = f->dataSize;
    err = FSSetFPos(ref, fsFromStart, 0);
    if (err == noErr) err = FSWrite(ref, &count, f->data);
    if (err == noErr && count != f->dataSize) err = ioErr;
    if (err == noErr) err = SetEOF(ref, (long)f->dataSize);
    OSErr closeErr = FSClose(ref);
    if (err == noErr) err = closeErr;
    if (err == noErr) f->dirty = false;
    return err;
}

void UpdateResFile(SInt16 refNum) {
    ResFile* f = FileFor(refNum);
    if (!f) {
        gResMgr.resError = resFNotFound;
        return;
    }
    gResMgr.resError = (f->writable && f->dirty) ? WriteFork(f) : noErr;
}

void WriteResource(Handle theResource) {
    HandleInfo* info = FindHandleInfo(theResource);
    ResFile* f = info ? FileFor(info->homeFile) : NULL;
    if (!f) {
        gResMgr.resError = resNotFound;
        return;
    }
    gResMgr.resError = (f->writable && info->changed) ? WriteFork(f) : noErr;
}

void ResFile_Close(SInt16 refNum) {
    ResFile* f = FileFor(refNum);
    if (!f) return;
    /* Its resources go with it (1-96) */
    for (int i = gLoadedCount - 1; i >= 0; i--) {
        if (gLoaded[i].homeFile != refNum) continue;
        Handle h = gLoaded[i].h;
        gLoaded[i] = gLoaded[--gLoadedCount];
        DisposeHandle(h);
    }
    if (f->mapHandle) DisposeHandle(f->mapHandle);
    f->inUse = false;
    f->refNum = -1;
    f->data = NULL;
    f->map = NULL;
    f->mapHandle = NULL;
    if (gResMgr.curResFile == refNum) {
        /* The newest file still open before it, as the chain runs */
        int best = 0;
        for (int i = 0; i < MAX_RES_FILES; i++) {
            ResFile* g = &gResMgr.resFiles[i];
            if (g->inUse && g->openSeq < f->openSeq && g->openSeq >= gResMgr.resFiles[best].openSeq)
                best = i;
        }
        gResMgr.curResFile = (SInt16)best;
    }
}

void CloseResFile(SInt16 refNum) {
    ResFile* f = FileFor(refNum);
    if (!f || refNum == 0) {
        gResMgr.resError = badRefNum;
        return;
    }
    OSErr err = (f->writable && f->dirty) ? WriteFork(f) : noErr;
    ResFile_Close(refNum);
    gResMgr.resError = err;
}

/* ------------------------------------------------------------------------
 * Resource files in memory - the localized strings
 * ------------------------------------------------------------------------ */

SInt16 OpenResMemory(const unsigned char* data, UInt32 size) {
    int slot = FreeSlot();
    if (!data || slot < 0) {
        gResMgr.resError = data ? tmfoErr : paramErr;
        return -1;
    }
    if (!Attach(slot, (UInt8*)(uintptr_t)data, size, NULL)) {
        gResMgr.resError = mapReadErr;
        return -1;
    }
    gResMgr.resError = noErr;
    return (SInt16)slot;
}

void CloseResMemory(SInt16 refNum) {
    if (refNum <= 0 || !FileFor(refNum)) {
        gResMgr.resError = badRefNum;
        return;
    }
    ResFile_Close(refNum);
    gResMgr.resError = noErr;
}

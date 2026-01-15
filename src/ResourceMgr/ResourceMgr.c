/*
 * ResourceMgr.c - Resource Manager implementation (read-only)
 * Based on Inside Macintosh: More Macintosh Toolbox
 * Clean-room implementation
 */

#include "SystemTypes.h"
#include "MacTypes.h"
#include "FileManagerTypes.h"
#include "ResourceMgr/ResourceMgr.h"
#include "ResourceMgr/resource_manager.h"
#include "ResourceMgr/ResourceMgrPriv.h"
#include "ResourceMgr/ResourceLogging.h"
#include "System71StdLib.h"

/* External functions we need */
extern Handle NewHandle(UInt32 byteCount);
extern void DisposeHandle(Handle h);
extern void HLock(Handle h);
extern void HUnlock(Handle h);
extern void BlockMove(const void* srcPtr, void* destPtr, Size byteCount);
extern void serial_puts(const char* s);
extern OSErr FSOpenRF(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
extern OSErr FSRead(FileRefNum refNum, UInt32* count, void* buffer);
extern OSErr FSClose(FileRefNum refNum);
extern OSErr FSSetFPos(FileRefNum refNum, UInt16 posMode, SInt32 posOffset);

/* RM_DEBUG: Set to 1 to enable verbose Resource Manager debugging
 * WARNING: Enabling causes severe performance impact on ARM64 */
#define RM_DEBUG 0

#if RM_DEBUG
#define RM_LOG(msg) serial_puts(msg)
#else
#define RM_LOG(msg) ((void)0)
#endif

/* Resource index for fast lookups */
typedef struct {
    ResType type;
    UInt16 firstRefIndex;
    UInt16 count;
} TypeIndex;

typedef struct {
    ResType type;
    ResID id;
    UInt32 refOffset;  /* Offset of RefListEntry in map */
    UInt16 nameOffset;  /* For named resource lookups */
} RefIndex;

static TypeIndex *gTypeIdx = NULL;
static UInt32 gTypeIdxCount = 0;
static RefIndex *gRefIdx = NULL;
static UInt32 gRefIdxCount = 0;

/* Resource cache to avoid duplicate loads */
#define RM_CACHE_CAP 256
typedef struct {
    ResType type;
    ResID id;
    Handle h;
} CacheEntry;

static CacheEntry gCache[RM_CACHE_CAP];
static UInt32 gCacheCount = 0;

/* Side-table for handle metadata */
#define RM_HANDLE_CAP 512
typedef struct {
    Handle h;
    ResType type;
    ResID id;
    UInt16 nameOffset;
    UInt32 dataLen;
    SInt16 homeFile;
} HandleInfo;

static HandleInfo gHandleInfo[RM_HANDLE_CAP];
static UInt32 gHandleCount = 0;

/* Global state */
static ResourceMgrGlobals gResMgr = {
    .curResFile = -1,
    .resError = noErr,
    .resLoad = true,
    .nextUniqueID = 128
};

/* Byte-swapping utilities for big-endian resource data */
UInt16 read_be16(const UInt8* p) {
    return (p[0] << 8) | p[1];
}

UInt32 read_be32(const UInt8* p) {
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8) | p[3];
}

void write_be16(UInt8* p, UInt16 val) {
    p[0] = (val >> 8) & 0xFF;
    p[1] = val & 0xFF;
}

void write_be32(UInt8* p, UInt32 val) {
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
}

/* Shell sort for resource index (no qsort in freestanding) */
static void SortRefIndex(RefIndex* arr, UInt32 n) {
    UInt32 gap, i, j;
    RefIndex temp;

    for (gap = n/2; gap > 0; gap /= 2) {
        for (i = gap; i < n; i++) {
            temp = arr[i];
            for (j = i; j >= gap; j -= gap) {
                /* Compare (type,id) pairs */
                if (arr[j-gap].type > temp.type ||
                    (arr[j-gap].type == temp.type && arr[j-gap].id > temp.id)) {
                    arr[j] = arr[j-gap];
                } else {
                    break;
                }
            }
            arr[j] = temp;
        }
    }
}

/* Binary search for type in index */
static SInt32 FindTypeIndex(ResType type) {
    SInt32 left = 0, right = gTypeIdxCount - 1;

    while (left <= right) {
        SInt32 mid = left + (right - left) / 2;  /* Avoid integer overflow */
        if (gTypeIdx[mid].type == type) {
            return mid;
        } else if (gTypeIdx[mid].type < type) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;  /* Not found */
}

/* Binary search for resource in index */
static SInt32 FindRefIndex(ResType type, ResID id) {
    SInt32 left = 0, right = gRefIdxCount - 1;

    RM_LOG("[FRI] enter\n");

    /* Safety check for NULL or huge index */
    if (!gRefIdx || gRefIdxCount <= 0 || gRefIdxCount > 100000) {
        RM_LOG("[FRI] bad index\n");
        return -1;
    }

    RM_LOG("[FRI] loop\n");

    while (left <= right) {
        SInt32 mid = left + (right - left) / 2;  /* Avoid integer overflow */
        if (gRefIdx[mid].type < type ||
            (gRefIdx[mid].type == type && gRefIdx[mid].id < id)) {
            left = mid + 1;
        } else if (gRefIdx[mid].type > type ||
                   (gRefIdx[mid].type == type && gRefIdx[mid].id > id)) {
            right = mid - 1;
        } else {
            RM_LOG("[FRI] found\n");
            return mid;  /* Found */
        }
    }
    RM_LOG("[FRI] not found\n");
    return -1;  /* Not found */
}

/* Cache operations */
static Handle CacheLookup(ResType type, ResID id) {
    /* Simple hash with linear probing */
    UInt32 hash = ((type << 16) ^ id) % RM_CACHE_CAP;
    UInt32 i;

    for (i = 0; i < RM_CACHE_CAP; i++) {
        UInt32 idx = (hash + i) % RM_CACHE_CAP;
        if (gCache[idx].h && gCache[idx].type == type && gCache[idx].id == id) {
            return gCache[idx].h;
        }
        if (!gCache[idx].h) {
            break;  /* Empty slot, not found */
        }
    }
    return NULL;
}

static void CacheInsert(ResType type, ResID id, Handle h) {
    if (gCacheCount >= RM_CACHE_CAP) return;  /* Cache full */

    UInt32 hash = ((type << 16) ^ id) % RM_CACHE_CAP;
    UInt32 i;

    for (i = 0; i < RM_CACHE_CAP; i++) {
        UInt32 idx = (hash + i) % RM_CACHE_CAP;
        if (!gCache[idx].h) {
            gCache[idx].type = type;
            gCache[idx].id = id;
            gCache[idx].h = h;
            gCacheCount++;
            break;
        }
    }
}

/* Handle info operations */
static void RecordHandleInfo(Handle h, ResType type, ResID id, UInt16 nameOff, UInt32 dataLen, SInt16 homeFile) {
    if (gHandleCount >= RM_HANDLE_CAP) return;

    /* Linear probe hash table */
    UInt32 hash = ((UInt32)(uintptr_t)h >> 4) % RM_HANDLE_CAP;
    UInt32 i;

    for (i = 0; i < RM_HANDLE_CAP; i++) {
        UInt32 idx = (hash + i) % RM_HANDLE_CAP;
        if (!gHandleInfo[idx].h) {
            gHandleInfo[idx].h = h;
            gHandleInfo[idx].type = type;
            gHandleInfo[idx].id = id;
            gHandleInfo[idx].nameOffset = nameOff;
            gHandleInfo[idx].dataLen = dataLen;
            gHandleInfo[idx].homeFile = homeFile;
            gHandleCount++;
            break;
        }
    }
}

static HandleInfo* FindHandleInfo(Handle h) {
    UInt32 hash = ((UInt32)(uintptr_t)h >> 4) % RM_HANDLE_CAP;
    UInt32 i;

    for (i = 0; i < RM_HANDLE_CAP; i++) {
        UInt32 idx = (hash + i) % RM_HANDLE_CAP;
        if (gHandleInfo[idx].h == h) {
            return &gHandleInfo[idx];
        }
        if (!gHandleInfo[idx].h) {
            break;
        }
    }
    return NULL;
}

/* Initialize Resource Manager */
void InitResourceManager(void) {
    int i;

    serial_puts("[ResourceMgr] Initializing Resource Manager\n");

    /* Clear all resource file slots */
    for (i = 0; i < MAX_RES_FILES; i++) {
        gResMgr.resFiles[i].inUse = false;
        gResMgr.resFiles[i].refNum = -1;
    }

    gResMgr.resFiles[0].inUse = true;
    gResMgr.resFiles[0].refNum = 0;
    gResMgr.curResFile = 0;

    /* Try to open System resource file from mounted volume */
    Str255 systemName = "\006System";  /* Pascal string: length byte + "System" */
    FileRefNum fileRef;
    OSErr err = FSOpenRF(systemName, 0, &fileRef);

    if (err == noErr) {
        serial_puts("[ResourceMgr] Opening System file resource fork\n");

        /* Read resource header */
        ResourceHeader header;
        UInt32 readCount = sizeof(ResourceHeader);
        err = FSRead(fileRef, &readCount, &header);

        if (err == noErr && readCount == sizeof(ResourceHeader)) {
            /* Byte-swap header fields */
            UInt32 mapOffset = read_be32((UInt8*)&header.mapOffset);
            UInt32 mapLength = read_be32((UInt8*)&header.mapLength);

            /* Check for integer overflow in size calculation */
            if (mapOffset > UINT32_MAX - mapLength) {
                serial_puts("[ResourceMgr] Integer overflow in totalSize calculation\n");
                FSClose(fileRef);
                return;
            }

            /* Allocate buffer for entire resource fork */
            UInt32 totalSize = mapOffset + mapLength;
            Handle dataHandle = NewHandle(totalSize);

            if (dataHandle) {
                /* Seek to beginning and read entire fork */
                err = FSSetFPos(fileRef, fsFromStart, 0);
                if (err == noErr) {
                    readCount = totalSize;
                    HLock(dataHandle);
                    err = FSRead(fileRef, &readCount, *dataHandle);
                    HUnlock(dataHandle);

                    if (err == noErr && readCount == totalSize) {
                        /* Successfully loaded System file */
                        FSClose(fileRef);
                        gResMgr.resFiles[0].data = (UInt8*)*dataHandle;
                        gResMgr.resFiles[0].dataSize = totalSize;
                        gResMgr.resFiles[0].mapHandle = dataHandle;
                        serial_puts("[ResourceMgr] System file loaded from disk\n");
                        goto parse_resources;
                    }
                }
                DisposeHandle(dataHandle);
            }
        }
        FSClose(fileRef);
        serial_puts("[ResourceMgr] Failed to load System file, using embedded resources\n");
    } else {
        serial_puts("[ResourceMgr] System file not found, using embedded resources\n");
    }

    /* Fallback: use embedded resource data */
    extern const unsigned char patterns_rsrc_data[];
    extern const unsigned int patterns_rsrc_size;

    gResMgr.resFiles[0].data = (UInt8*)(uintptr_t)patterns_rsrc_data;
    gResMgr.resFiles[0].dataSize = patterns_rsrc_size;

parse_resources:

    /* Parse resource header and map */
    if (gResMgr.resFiles[0].dataSize >= sizeof(ResourceHeader)) {
        ResourceHeader* hdr = (ResourceHeader*)gResMgr.resFiles[0].data;
        UInt32 mapOffset = read_be32((UInt8*)&hdr->mapOffset);

        /* Bounds check map offset */
        if (mapOffset + sizeof(ResMapHeader) <= gResMgr.resFiles[0].dataSize) {
            ResMapHeader* map = (ResMapHeader*)(gResMgr.resFiles[0].data + mapOffset);
            gResMgr.resFiles[0].map = map;
            serial_puts("[ResourceMgr] Resource map loaded successfully\n");

            /* Build index for fast lookups */
            UInt16 typeListOff = read_be16((UInt8*)&map->typeListOffset);
            UInt16 nameListOff = read_be16((UInt8*)&map->nameListOffset);
            UInt32 mapSize = gResMgr.resFiles[0].dataSize - mapOffset;
            gResMgr.resFiles[0].mapSize = mapSize;

            /* Validate offsets - Inside Macintosh: offsets are from map start */
            if (typeListOff != 0xFFFF && typeListOff + 2 <= mapSize &&
                (nameListOff == 0xFFFF || nameListOff < mapSize)) {

                UInt8* typeList = (UInt8*)map + typeListOff;
                UInt16 numTypes = read_be16(typeList) + 1;

                /* Bounds check: ensure type list fits in map */
                UInt32 typeListSize = 2 + ((UInt32)numTypes * sizeof(TypeListEntry));
                if (typeListOff + typeListSize > mapSize) {
                    serial_puts("[ResourceMgr] Warning: Type list exceeds map bounds\n");
                    gResMgr.resError = mapReadErr;
                    return;
                }

                typeList += 2;

                /* Count total resources for index allocation */
                UInt32 totalRefs = 0;
                for (i = 0; i < numTypes; i++) {
                    TypeListEntry* te = (TypeListEntry*)(typeList + i * sizeof(TypeListEntry));
                    UInt16 count = read_be16((UInt8*)&te->count) + 1;
                    UInt16 refListOff = read_be16((UInt8*)&te->refListOffset);

                    /* Validate count is reasonable */
                    if (count > 1024) {
                        serial_puts("[ResourceMgr] Warning: Excessive resource count, skipping\n");
                        continue;
                    }

                    /* Bounds check reference list with overflow protection */
                    UInt32 refListStart = typeListOff + refListOff;
                    UInt32 refListSize = (UInt32)count * sizeof(RefListEntry);

                    /* Check for multiplication overflow */
                    if (count > 0 && refListSize / count != sizeof(RefListEntry)) {
                        serial_puts("[ResourceMgr] Warning: Size calculation overflow\n");
                        continue;
                    }

                    /* Check bounds */
                    if (refListStart >= mapSize || refListStart + refListSize > mapSize) {
                        serial_puts("[ResourceMgr] Warning: Reference list exceeds map bounds\n");
                        continue;
                    }

                    /* Check totalRefs won't overflow */
                    if (totalRefs + count < totalRefs) {
                        serial_puts("[ResourceMgr] Warning: Total resource count overflow\n");
                        break;
                    }

                    totalRefs += count;
                }

                /* Check for integer overflow in allocation sizes */
                if (numTypes > SIZE_MAX / sizeof(TypeIndex)) {
                    serial_puts("[ResourceMgr] Integer overflow in typeIdx allocation\n");
                    return;
                }
                if (totalRefs > SIZE_MAX / sizeof(RefIndex)) {
                    serial_puts("[ResourceMgr] Integer overflow in refIdx allocation\n");
                    return;
                }

                /* Allocate index arrays */
                Handle typeIdxH = NewHandle(numTypes * sizeof(TypeIndex));
                Handle refIdxH = NewHandle(totalRefs * sizeof(RefIndex));

                if (!typeIdxH || !refIdxH) {
                    /* Clean up any successfully allocated handles */
                    if (typeIdxH) {
                        DisposeHandle(typeIdxH);
                    }
                    if (refIdxH) {
                        DisposeHandle(refIdxH);
                    }
                    serial_puts("[ResourceMgr] Warning: Could not allocate index\n");
                } else if (typeIdxH && refIdxH) {
                    HLock(typeIdxH);
                    HLock(refIdxH);
                    gTypeIdx = (TypeIndex*)*typeIdxH;
                    gRefIdx = (RefIndex*)*refIdxH;
                    gTypeIdxCount = numTypes;
                    gRefIdxCount = 0;

                    /* Build index */
                    for (i = 0; i < numTypes; i++) {
                        TypeListEntry* te = (TypeListEntry*)(typeList + i * sizeof(TypeListEntry));
                        ResType resType = read_be32((UInt8*)&te->resType);
                        UInt16 count = read_be16((UInt8*)&te->count) + 1;
                        UInt16 refListOff = read_be16((UInt8*)&te->refListOffset);

                        gTypeIdx[i].type = resType;
                        gTypeIdx[i].firstRefIndex = gRefIdxCount;
                        gTypeIdx[i].count = count;

                        /* Index all resources of this type */
                        /* Reference list offset is from start of type list (Inside Macintosh rule) */
                        UInt32 refListStart = typeListOff + refListOff;

                        /* Skip if reference list is out of bounds */
                        if (refListStart + count * sizeof(RefListEntry) > mapSize) {
                            continue;
                        }

                        UInt8* refList = (UInt8*)map + refListStart;

                        for (int j = 0; j < count; j++) {
                            RefListEntry* ref = (RefListEntry*)(refList + j * sizeof(RefListEntry));
                            ResID id = (ResID)read_be16((UInt8*)&ref->resID);
                            UInt16 nameOff = read_be16((UInt8*)&ref->nameOffset);

                            /* Validate name offset */
                            if (nameOff != 0xFFFF && nameListOff != 0xFFFF) {
                                UInt32 actualNameOff = nameListOff + nameOff;
                                if (actualNameOff >= mapSize) {
                                    nameOff = 0xFFFF;  /* Invalid name, mark as none */
                                }
                            }

                            gRefIdx[gRefIdxCount].type = resType;
                            gRefIdx[gRefIdxCount].id = id;
                            gRefIdx[gRefIdxCount].refOffset = (UInt8*)ref - (UInt8*)map;
                            gRefIdx[gRefIdxCount].nameOffset = nameOff;
                            gRefIdxCount++;
                        }
                    }

                    /* Sort reference index for binary search */
                    SortRefIndex(gRefIdx, gRefIdxCount);

                    serial_puts("[ResourceMgr] Built index for ");
                    /* Would print count but no printf in freestanding */
                    serial_puts(" resources\n");
                }
            } else {
                serial_puts("[ResourceMgr] Warning: Invalid type/name list offsets\n");
            }
        } else {
            serial_puts("[ResourceMgr] Warning: Invalid resource map offset\n");
        }
    }

    serial_puts("[ResourceMgr] Resource Manager initialized\n");
    gResMgr.resError = noErr;
}

/* Shutdown Resource Manager */
void ShutdownResourceManager(void) {
    int i;

    for (i = 0; i < MAX_RES_FILES; i++) {
        if (gResMgr.resFiles[i].inUse && i != 0) {  /* Don't close system file */
            ResFile_Close(i);
        }
    }
}

/* Get last error */
OSErr ResError(void) {
    OSErr err = gResMgr.resError;
    gResMgr.resError = noErr;  /* Clear error after reading */
    return err;
}

/* Get current resource file */
SInt16 CurResFile(void) {
    return gResMgr.curResFile;
}

/* Set current resource file */
void UseResFile(SInt16 refNum) {
    if (refNum < 0 || refNum >= MAX_RES_FILES || !gResMgr.resFiles[refNum].inUse) {
        RM_LOG_WARN("UseResFile: bad refNum=%d", refNum);
        gResMgr.resError = badRefNum;
        return;
    }
    RM_LOG_DEBUG("UseResFile: refNum=%d (was %d)", refNum, gResMgr.curResFile);
    gResMgr.curResFile = refNum;
}

/* Find type in resource map */
TypeListEntry* ResMap_FindType(ResFile* file, ResType type) {
    ResMapHeader* map;
    UInt16 typeListOff;
    UInt16 numTypes;
    UInt8* typeList;
    int i;

    RM_LOG("[FT] enter\n");

    if (!file || !file->map) {
        RM_LOG("[FT] bad file\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[FT] get map\n");
    map = file->map;
    RM_LOG("[FT] check map ptr\n");
    /* Safety check - if map is NULL or in invalid memory, skip */
    if (!map) {
        RM_LOG("[FT] map is NULL\n");
        return NULL;
    }
    RM_LOG("[FT] read typeListOff\n");
    typeListOff = read_be16((UInt8*)&map->typeListOffset);
    RM_LOG("[FT] got typeListOff\n");

    RM_LOG("[FT] bounds check\n");
    /* Bounds check: validate type list offset */
    if (typeListOff == 0xFFFF || typeListOff >= file->mapSize) {
        RM_LOG("[FT] bad bounds\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[FT] bounds2\n");
    /* Bounds check: ensure we can read the count */
    if (typeListOff + 2 > file->mapSize) {
        RM_LOG("[FT] bounds2 fail\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[FT] get typeList ptr\n");
    typeList = (UInt8*)map + typeListOff;
    RM_LOG("[FT] read numTypes\n");
    numTypes = read_be16(typeList) + 1;  /* Count is stored as n-1 */
    RM_LOG("[FT] got numTypes\n");

    RM_LOG("[FT] bounds3\n");
    /* Bounds check: ensure type list entries fit in map */
    UInt32 typeListSize = 2 + ((UInt32)numTypes * sizeof(TypeListEntry));
    if (typeListOff + typeListSize > file->mapSize) {
        RM_LOG("[FT] bounds3 fail\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[FT] skip count\n");
    typeList += 2;  /* Skip count */

    RM_LOG("[FT] loop\n");
    for (i = 0; i < numTypes; i++) {
        TypeListEntry* entry = (TypeListEntry*)(typeList + i * sizeof(TypeListEntry));
        ResType entryType = read_be32((UInt8*)&entry->resType);
        if (entryType == type) {
            RM_LOG("[FT] found\n");
            return entry;
        }
    }
    RM_LOG("[FT] not found\n");

    return NULL;
}

/* Find resource by type and ID */
RefListEntry* ResMap_FindResource(ResFile* file, ResType type, ResID id) {
    TypeListEntry* typeEntry;
    UInt16 count;
    UInt16 refListOff;
    UInt8* refList;
    int i;

    RM_LOG("[RMF] enter\n");

    if (!file || !file->map) {
        RM_LOG("[RMF] bad file\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[RMF] FindType\n");
    typeEntry = ResMap_FindType(file, type);
    RM_LOG("[RMF] FindType returned\n");
    if (!typeEntry) {
        RM_LOG("[RMF] typeEntry NULL\n");
        return NULL;
    }

    RM_LOG("[RMF] read count\n");
    count = read_be16((UInt8*)&typeEntry->count) + 1;  /* Count is stored as n-1 */
    RM_LOG("[RMF] read refListOff\n");
    refListOff = read_be16((UInt8*)&typeEntry->refListOffset);
    RM_LOG("[RMF] got offsets\n");

    /* Reference list offset is from start of type list */
    ResMapHeader* map = file->map;
    RM_LOG("[RMF] read typeListOff\n");
    UInt16 typeListOff = read_be16((UInt8*)&map->typeListOffset);
    RM_LOG("[RMF] check typeListOff\n");

    /* Bounds check: ensure type list offset is valid */
    if (typeListOff == 0xFFFF || typeListOff >= file->mapSize) {
        RM_LOG("[RMF] bad typeListOff\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[RMF] calc refListStart\n");
    /* Bounds check: ensure reference list is within map */
    UInt32 refListStart = typeListOff + refListOff;
    UInt32 refListSize = count * sizeof(RefListEntry);
    RM_LOG("[RMF] check refList bounds\n");
    if (refListStart >= file->mapSize || refListStart + refListSize > file->mapSize) {
        RM_LOG("[RMF] bad refList bounds\n");
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    RM_LOG("[RMF] get refList ptr\n");
    refList = (UInt8*)map + refListStart;

    RM_LOG("[RMF] loop\n");
    for (i = 0; i < count; i++) {
        RefListEntry* ref = (RefListEntry*)(refList + i * sizeof(RefListEntry));
        ResID refID = (ResID)read_be16((UInt8*)&ref->resID);
        if (refID == id) {
            RM_LOG("[RMF] found\n");
            return ref;
        }
    }
    RM_LOG("[RMF] not found\n");

    return NULL;
}

/* Load resource data from file */
Handle ResFile_LoadResource(ResFile* file, RefListEntry* ref) {
    UInt32 dataOffset;
    ResourceDataEntry* dataEntry;
    UInt32 dataLength;
    Handle h;
    UInt8* data;

    if (!file || !ref) return NULL;

    /* Reconstruct 24-bit offset */
    dataOffset = ((UInt32)ref->dataOffsetHi << 16) | read_be16((UInt8*)&ref->dataOffsetLo);

    /* Offset is from start of resource data section */
    ResourceHeader* hdr = (ResourceHeader*)file->data;
    UInt32 dataBase = read_be32((UInt8*)&hdr->dataOffset);

    /* Validate dataBase first */
    if (dataBase >= file->dataSize || dataBase + 4 > file->dataSize) {
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    UInt32 actualOffset = dataBase + dataOffset;

    /* Check for overflow and bounds */
    if (actualOffset < dataBase || actualOffset + sizeof(ResourceDataEntry) > file->dataSize) {
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    dataEntry = (ResourceDataEntry*)(file->data + actualOffset);
    dataLength = read_be32((UInt8*)&dataEntry->length);

    if (actualOffset + sizeof(ResourceDataEntry) + dataLength > file->dataSize) {
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    /* Allocate handle and copy data */
    h = NewHandle(dataLength);
    if (!h) {
        gResMgr.resError = noMemForRsrc;
        return NULL;
    }

    HLock(h);
    data = (UInt8*)*h;
    BlockMove((UInt8*)dataEntry + sizeof(ResourceDataEntry), data, dataLength);
    HUnlock(h);

    return h;
}

/* Get resource by type and ID */
Handle GetResource(ResType theType, ResID theID) {
    extern void serial_puts(const char* str);
    int i;
    RefListEntry* ref = NULL;
    ResFile* file = NULL;

    RM_LOG("[GETRES] enter\n");

    RM_LOG_DEBUG("GetResource('%c%c%c%c', %d)",
                 (char)(theType >> 24), (char)(theType >> 16),
                 (char)(theType >> 8), (char)theType, theID);

    /* Built-in PAT patterns (System 7.1 standard patterns 1-10) */
    if (theType == 0x50415420 /* 'PAT ' */ && theID >= 1 && theID <= 10) {
        /* Classic Mac OS 8x8 pixel patterns (8 bytes each) */
        static const UInt8 kBuiltinPatterns[10][8] = {
            /* PAT 1: White */
            {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
            /* PAT 2: Light gray */
            {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55},
            /* PAT 3: Dark gray */
            {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA},
            /* PAT 4: Black */
            {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            /* PAT 5: Horizontal stripes */
            {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00},
            /* PAT 6: Vertical stripes */
            {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
            /* PAT 7: Diagonal stripes (45°) */
            {0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11},
            /* PAT 8: Checkerboard */
            {0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0},
            /* PAT 9: Cross-hatch */
            {0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88},
            /* PAT 10: Dots */
            {0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44}
        };

        Handle h = NewHandle(8);
        if (h) {
            BlockMove(kBuiltinPatterns[theID - 1], *h, 8);
            RM_LOG_DEBUG("Built-in PAT %d provided", theID);
            gResMgr.resError = noErr;
        } else {
            gResMgr.resError = memFullErr;
        }
        return h;
    }

    RM_LOG("[GETRES] cache check\n");

    /* Check cache first for fast hit */
    Handle cached = CacheLookup(theType, theID);
    if (cached) {
        RM_LOG_DEBUG("GetResource: cache hit, handle=%p", cached);
        gResMgr.resError = noErr;
        return cached;
    }

    RM_LOG("[GETRES] cache miss, idx check\n");

    /* Try using index for O(log n) lookup if available */
    RM_LOG("[GETRES] gRefIdxCount check\n");
    if (gRefIdxCount > 0) {
        RM_LOG("[GETRES] FindRefIndex\n");
        SInt32 idx = FindRefIndex(theType, theID);
        RM_LOG("[GETRES] FindRefIndex returned\n");
        if (idx >= 0) {
            RM_LOG("[GETRES] idx found, loop\n");
            /* Found in index - get the resource from any open file */
            for (i = gResMgr.curResFile; i >= 0; i--) {
                if (!gResMgr.resFiles[i].inUse) continue;

                RM_LOG("[GETRES] ResMap_Find\n");
                /* Check if this file has the resource at the indexed offset */
                ref = ResMap_FindResource(&gResMgr.resFiles[i], theType, theID);
                RM_LOG("[GETRES] ResMap_Find returned\n");
                if (ref) {
                    RM_LOG("[GETRES] ref found, set file\n");
                    file = &gResMgr.resFiles[i];
                    RM_LOG("[GETRES] break loop\n");
                    break;
                }
            }
            RM_LOG("[GETRES] idx loop done\n");
        }
    }

    RM_LOG("[GETRES] after idx search\n");
    /* Fall back to linear search if not found via index */
    if (!ref) {
        /* Search all open resource files, starting with current */
        for (i = gResMgr.curResFile; i >= 0; i--) {
            if (!gResMgr.resFiles[i].inUse) continue;

            ref = ResMap_FindResource(&gResMgr.resFiles[i], theType, theID);
            if (ref) {
                file = &gResMgr.resFiles[i];
                break;
            }
        }

        /* If not found in current chain, search other files */
        if (!ref) {
            for (i = 0; i < MAX_RES_FILES; i++) {
                if (!gResMgr.resFiles[i].inUse || i == gResMgr.curResFile) continue;

                ref = ResMap_FindResource(&gResMgr.resFiles[i], theType, theID);
                if (ref) {
                    file = &gResMgr.resFiles[i];
                    break;
                }
            }
        }
    }

    RM_LOG("[GETRES] ref check\n");
    if (!ref) {
        RM_LOG("[GETRES] ref is NULL\n");
        RM_LOG_WARN("GetResource: not found");
        gResMgr.resError = resNotFound;
        return NULL;
    }

    RM_LOG("[GETRES] read reserved\n");
    /* Check if already loaded (stored in reserved field) */
    /* CRITICAL FIX: Use byte-by-byte read to avoid ARM64 misaligned access hang */
    UInt8* reservedPtr = (UInt8*)&ref->reserved;
    UInt32 reservedVal = ((UInt32)reservedPtr[0]) |
                         ((UInt32)reservedPtr[1] << 8) |
                         ((UInt32)reservedPtr[2] << 16) |
                         ((UInt32)reservedPtr[3] << 24);
    Handle h = (Handle)(uintptr_t)reservedVal;
    RM_LOG("[GETRES] got reserved\n");
    if (h) {
        RM_LOG("[GETRES] already loaded\n");
        RM_LOG_DEBUG("GetResource: already loaded, handle=%p", h);
        gResMgr.resError = noErr;
        /* Update cache for next lookup */
        CacheInsert(theType, theID, h);
        return h;
    }

    RM_LOG("[GETRES] need to load\n");
    /* Load the resource */
    if (gResMgr.resLoad) {
        RM_LOG("[GETRES] calling LoadResource\n");
        h = ResFile_LoadResource(file, ref);
        if (h) {
            RM_LOG_INFO("GetResource('%c%c%c%c', %d) = handle %p",
                        (char)(theType >> 24), (char)(theType >> 16),
                        (char)(theType >> 8), (char)theType, theID, h);
            /* Cache the handle (cast through uintptr_t for safety) */
            /* CRITICAL FIX: Use byte-by-byte write to avoid ARM64 misaligned access hang */
            {
                UInt32 hVal = (UInt32)(uintptr_t)h;
                UInt8* rp = (UInt8*)&ref->reserved;
                rp[0] = (UInt8)(hVal);
                rp[1] = (UInt8)(hVal >> 8);
                rp[2] = (UInt8)(hVal >> 16);
                rp[3] = (UInt8)(hVal >> 24);
            }

            /* Get data length and name offset for metadata */
            /* CRITICAL FIX: Use safe byte access for dataOffsetHi */
            UInt8* doHiPtr = (UInt8*)&ref->dataOffsetHi;
            UInt32 dataOffset = ((UInt32)*doHiPtr << 16) |
                               read_be16((UInt8*)&ref->dataOffsetLo);
            ResourceHeader* hdr = (ResourceHeader*)file->data;
            UInt32 dataBase = read_be32((UInt8*)&hdr->dataOffset);
            UInt32 actualOffset = dataBase + dataOffset;
            ResourceDataEntry* dataEntry = (ResourceDataEntry*)(file->data + actualOffset);
            UInt32 dataLength = read_be32((UInt8*)&dataEntry->length);

            /* Get name offset if resource has a name */
            /* CRITICAL FIX: Use safe byte access for nameOffset comparison */
            UInt16 nameOffRaw = read_be16((UInt8*)&ref->nameOffset);
            UInt16 nameOff = nameOffRaw != 0xFFFF ? nameOffRaw : 0;

            /* Insert into cache and record handle info */
            CacheInsert(theType, theID, h);
            RecordHandleInfo(h, theType, theID, nameOff, dataLength, file->refNum);
        }
        return h;
    }

    gResMgr.resError = noErr;
    return NULL;  /* Resource exists but not loaded */
}

/* Get resource from current file only */
Handle Get1Resource(ResType theType, ResID theID) {
    RefListEntry* ref;
    ResFile* file;

    if (gResMgr.curResFile < 0) {
        gResMgr.resError = resFileNotOpen;
        return NULL;
    }

    file = &gResMgr.resFiles[gResMgr.curResFile];
    ref = ResMap_FindResource(file, theType, theID);

    if (!ref) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    /* Check if already loaded */
    /* CRITICAL FIX: Use byte-by-byte read to avoid ARM64 misaligned access hang */
    UInt8* reservedPtr = (UInt8*)&ref->reserved;
    UInt32 reservedVal = ((UInt32)reservedPtr[0]) |
                         ((UInt32)reservedPtr[1] << 8) |
                         ((UInt32)reservedPtr[2] << 16) |
                         ((UInt32)reservedPtr[3] << 24);
    Handle h = (Handle)(uintptr_t)reservedVal;
    if (h) {
        gResMgr.resError = noErr;
        return h;
    }

    /* Load the resource */
    if (gResMgr.resLoad) {
        h = ResFile_LoadResource(file, ref);
        if (h) {
            /* CRITICAL FIX: Use byte-by-byte write to avoid ARM64 misaligned access hang */
            UInt32 hVal = (UInt32)(uintptr_t)h;
            reservedPtr[0] = (UInt8)(hVal);
            reservedPtr[1] = (UInt8)(hVal >> 8);
            reservedPtr[2] = (UInt8)(hVal >> 16);
            reservedPtr[3] = (UInt8)(hVal >> 24);
        }
        return h;
    }

    gResMgr.resError = noErr;
    return NULL;
}

/* Get named resource */
Handle GetNamedResource(ResType theType, ConstStr255Param name) {
    if (!name || name[0] == 0) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    /* Search through all resources of this type for matching name */
    for (int i = gResMgr.curResFile; i >= 0; i--) {
        if (!gResMgr.resFiles[i].inUse) continue;

        ResFile* file = &gResMgr.resFiles[i];
        if (!file->map) continue;

        UInt8* mapData = (UInt8*)file->map;

        /* Get type list */
        ResMapHeader* map = file->map;
        UInt16 typeListOffset = read_be16((UInt8*)&map->typeListOffset);
        if (typeListOffset == 0xFFFF) continue;

        UInt8* typeList = mapData + typeListOffset;
        UInt16 typeCount = read_be16(typeList);  /* Count is n-1 */
        typeList += 2;  /* Skip count */

        /* Validate typeCount doesn't overflow buffer */
        /* Cast to UInt32 before adding 1 to prevent overflow */
        UInt32 typeListSize = 2 + (((UInt32)typeCount + 1) * sizeof(TypeListEntry));
        if (typeListOffset + typeListSize > file->mapSize) {
            continue;  /* Skip corrupted type list */
        }

        /* Find matching type */
        for (UInt16 t = 0; t < typeCount + 1; t++) {  /* Use < instead of <= */
            TypeListEntry* typeEntry = (TypeListEntry*)(typeList + t * sizeof(TypeListEntry));
            if (read_be32((UInt8*)&typeEntry->resType) != theType) continue;

            /* Get reference list for this type */
            UInt16 refListOffset = read_be16((UInt8*)&typeEntry->refListOffset);
            UInt16 resCount = read_be16((UInt8*)&typeEntry->count);  /* Count is n-1 */

            /* Validate refList bounds before accessing */
            UInt32 refListStart = typeListOffset + refListOffset;
            /* Cast to UInt32 before adding 1 to prevent overflow */
            UInt32 refListSize = (((UInt32)resCount + 1)) * sizeof(RefListEntry);
            if (refListStart + refListSize > file->mapSize) {
                continue;  /* Skip corrupted reference list */
            }

            RefListEntry* refList = (RefListEntry*)(mapData + refListStart);

            /* Check each resource of this type */
            for (UInt16 r = 0; r < resCount + 1; r++) {  /* Use < instead of <= */
                RefListEntry* ref = &refList[r];

                /* Check if resource has a name */
                UInt16 nameOffset = read_be16((UInt8*)&ref->nameOffset);
                if (nameOffset == 0xFFFF) continue;

                /* Get name list offset from map */
                UInt16 nameListOffset = read_be16((UInt8*)&map->nameListOffset);
                if (nameListOffset == 0xFFFF) continue;

                /* Bounds check: ensure name is within map */
                UInt32 namePos = (UInt32)nameListOffset + (UInt32)nameOffset;
                if (namePos >= file->mapSize) continue;  /* Name offset out of bounds */

                /* Get the resource name */
                UInt8* resName = mapData + namePos;
                UInt8 nameLen = resName[0];

                /* Validate name length fits within map */
                if (namePos + 1 + nameLen > file->mapSize) continue;

                /* Compare names (Pascal string comparison) */
                if (nameLen == name[0]) {
                    Boolean match = true;
                    for (UInt8 c = 1; c <= nameLen; c++) {
                        if (resName[c] != name[c]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        /* Found it - get the resource ID and load it */
                        ResID id = (ResID)read_be16((UInt8*)&ref->resID);
                        return GetResource(theType, id);
                    }
                }
            }
        }
    }

    gResMgr.resError = resNotFound;
    return NULL;
}

/* Get named resource from current file (stub) */
Handle Get1NamedResource(ResType theType, ConstStr255Param name) {
    (void)theType;
    (void)name;
    gResMgr.resError = resNotFound;
    return NULL;
}

/* Release resource */
void ReleaseResource(Handle theResource) {
    /* In read-only mode, just dispose the handle */
    if (theResource) {
        DisposeHandle(theResource);
    }
}

/* Get resource info */
void GetResInfo(Handle theResource, ResID *theID, ResType *theType, char* name) {
    if (!theResource) {
        gResMgr.resError = resNotFound;
        return;
    }

    /* Look up in handle metadata */
    HandleInfo* info = FindHandleInfo(theResource);
    if (!info) {
        gResMgr.resError = resNotFound;
        if (theID) *theID = 0;
        if (theType) *theType = 0;
        if (name) name[0] = 0;
        return;
    }

    /* Return resource information */
    if (theID) *theID = info->id;
    if (theType) *theType = info->type;

    /* Get name if requested and available */
    if (name) {
        if (info->nameOffset && info->homeFile >= 0) {
            ResFile* file = &gResMgr.resFiles[info->homeFile];
            if (file->map) {
                UInt8* nameData = ((UInt8*)file->map) + info->nameOffset;
                UInt8 nameLen = nameData[0];
                if (nameLen > 0 && nameLen < 255) {
                    BlockMove(nameData, name, nameLen + 1);
                } else {
                    name[0] = 0;
                }
            } else {
                name[0] = 0;
            }
        } else {
            name[0] = 0;
        }
    }

    gResMgr.resError = noErr;
}

/* Get resource size on disk */
Size GetResourceSizeOnDisk(Handle theResource) {
    if (!theResource) {
        gResMgr.resError = resNotFound;
        return 0;
    }

    /* Look up in handle metadata */
    HandleInfo* info = FindHandleInfo(theResource);
    if (!info) {
        gResMgr.resError = resNotFound;
        return 0;
    }

    gResMgr.resError = noErr;
    return info->dataLen;
}

/* Get maximum resource size */
Size GetMaxResourceSize(Handle theResource) {
    return GetResourceSizeOnDisk(theResource);
}

/* Get home file of resource */
SInt16 HomeResFile(Handle theResource) {
    /* Stub - would need to track handle->resource mapping */
    (void)theResource;
    gResMgr.resError = resNotFound;
    return -1;
}

/* Detach resource from file */
void DetachResource(Handle theResource) {
    /* In read-only mode, this is a no-op */
    (void)theResource;
    gResMgr.resError = noErr;
}

/* Load resource data */
void LoadResource(Handle theResource) {
    /* In our implementation, resources are always loaded */
    (void)theResource;
    gResMgr.resError = noErr;
}

/* Set whether to auto-load resources */
void SetResLoad(Boolean load) {
    gResMgr.resLoad = load;
}

/* Open resource file */
SInt16 OpenResFile(ConstStr255Param fileName) {
    SInt16 refNum;
    OSErr err;

    /* Find free slot */
    refNum = -1;
    for (SInt16 i = 1; i < MAX_RES_FILES; i++) {
        if (!gResMgr.resFiles[i].inUse) {
            refNum = i;
            break;
        }
    }

    if (refNum < 0) {
        gResMgr.resError = tmfoErr;  /* Too many files open */
        return -1;
    }

    /* Open resource fork using File Manager */
    FileRefNum fileRef;
    err = FSOpenRF(fileName, 0, &fileRef);
    if (err != noErr) {
        gResMgr.resError = err;
        return -1;
    }

    /* Read resource header */
    ResourceHeader header;
    UInt32 readCount = sizeof(ResourceHeader);
    err = FSRead(fileRef, &readCount, &header);
    if (err != noErr || readCount != sizeof(ResourceHeader)) {
        FSClose(fileRef);
        gResMgr.resError = err;
        return -1;
    }

    /* Byte-swap header fields (resource forks are big-endian) */
    UInt32 mapOffset = read_be32((UInt8*)&header.mapOffset);
    UInt32 mapLength = read_be32((UInt8*)&header.mapLength);

    /* Unused but available for validation if needed */
    (void)read_be32((UInt8*)&header.dataOffset);
    (void)read_be32((UInt8*)&header.dataLength);

    /* Validate header */
    if (mapLength < sizeof(ResMapHeader)) {
        FSClose(fileRef);
        gResMgr.resError = mapReadErr;
        return -1;
    }

    /* Allocate buffer for entire resource fork */
    UInt32 totalSize = mapOffset + mapLength;
    Handle dataHandle = NewHandle(totalSize);
    if (!dataHandle) {
        FSClose(fileRef);
        gResMgr.resError = memFullErr;
        return -1;
    }

    /* Seek to beginning and read entire fork */
    err = FSSetFPos(fileRef, fsFromStart, 0);
    if (err != noErr) {
        DisposeHandle(dataHandle);
        FSClose(fileRef);
        gResMgr.resError = err;
        return -1;
    }

    readCount = totalSize;
    HLock(dataHandle);
    err = FSRead(fileRef, &readCount, *dataHandle);
    HUnlock(dataHandle);

    if (err != noErr || readCount != totalSize) {
        DisposeHandle(dataHandle);
        FSClose(fileRef);
        gResMgr.resError = err;
        return -1;
    }

    /* Done with file - resource data now in memory */
    FSClose(fileRef);

    /* Initialize resource file control block */
    ResFile* resFile = &gResMgr.resFiles[refNum];
    resFile->inUse = true;
    resFile->refNum = refNum;
    resFile->data = (UInt8*)*dataHandle;
    resFile->dataSize = totalSize;
    resFile->map = (ResMapHeader*)(resFile->data + mapOffset);
    resFile->mapSize = mapLength;
    resFile->mapHandle = dataHandle;

    /* Copy filename for debugging */
    UInt8 len = fileName[0];
    if (len > 255) len = 255;
    resFile->fileName[0] = len;
    for (UInt8 i = 0; i < len; i++) {
        resFile->fileName[i + 1] = fileName[i + 1];
    }

    gResMgr.resError = noErr;
    return refNum;
}

/* Close resource file */
void CloseResFile(SInt16 refNum) {
    if (refNum < 0 || refNum >= MAX_RES_FILES || !gResMgr.resFiles[refNum].inUse) {
        gResMgr.resError = badRefNum;
        return;
    }

    if (refNum == 0) {
        /* Can't close system file */
        gResMgr.resError = badRefNum;
        return;
    }

    ResFile_Close(refNum);
}

/* Internal: Close resource file */
void ResFile_Close(SInt16 refNum) {
    ResFile* file = &gResMgr.resFiles[refNum];

    if (file->mapHandle) {
        DisposeHandle(file->mapHandle);
    }

    file->inUse = false;
    file->refNum = -1;
    file->data = NULL;
    file->map = NULL;

    /* If this was current file, switch to system */
    if (gResMgr.curResFile == refNum) {
        gResMgr.curResFile = 0;
    }
}

/* Add resource to in-memory cache (for test/dynamic resources) */
void AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name) {
    if (!theData) {
        gResMgr.resError = paramErr;
        return;
    }

    RM_LOG_INFO("AddResource('%c%c%c%c', %d, handle=%p)",
                (char)(theType >> 24), (char)(theType >> 16),
                (char)(theType >> 8), (char)theType, theID, theData);

    /* Store in cache for immediate retrieval */
    CacheInsert(theType, theID, theData);

    /* Record handle info */
    UInt16 nameOff = (name && name[0] > 0) ? 1 : 0;  /* Simplified name handling */
    extern Size GetHandleSize(Handle h);
    UInt32 dataLen = GetHandleSize(theData);
    RecordHandleInfo(theData, theType, theID, nameOff, dataLen, gResMgr.curResFile);

    gResMgr.resError = noErr;
}

void RemoveResource(Handle theResource) {
    (void)theResource;
    gResMgr.resError = rmvResFailed;
}

void WriteResource(Handle theResource) {
    (void)theResource;
    gResMgr.resError = mapReadErr;
}

void UpdateResFile(SInt16 refNum) {
    (void)refNum;
    gResMgr.resError = mapReadErr;
}

void ChangedResource(Handle theResource) {
    (void)theResource;
    gResMgr.resError = mapReadErr;
}

/* Get/set resource attributes */
SInt16 GetResAttrs(Handle theResource) {
    (void)theResource;
    return 0;
}

void SetResAttrs(Handle theResource, SInt16 attrs) {
    (void)theResource; (void)attrs;
    gResMgr.resError = resAttrErr;
}

/* Count resources */
SInt16 CountResources(ResType theType) {
    SInt16 count = 0;
    int i;

    for (i = 0; i < MAX_RES_FILES; i++) {
        if (!gResMgr.resFiles[i].inUse) continue;

        TypeListEntry* typeEntry = ResMap_FindType(&gResMgr.resFiles[i], theType);
        if (typeEntry) {
            count += read_be16((UInt8*)&typeEntry->count) + 1;
        }
    }

    return count;
}

/* Helper: Get Nth type from resource map (1-indexed) */
static ResType ResMap_GetIndType(ResFile* file, SInt16 index) {
    ResMapHeader* map;
    UInt16 typeListOff;
    UInt16 numTypes;
    UInt8* typeList;

    if (!file || !file->map || index < 1) {
        gResMgr.resError = resNotFound;
        return 0;
    }

    map = file->map;
    typeListOff = read_be16((UInt8*)&map->typeListOffset);

    /* Bounds check */
    if (typeListOff == 0xFFFF || typeListOff >= file->mapSize) {
        gResMgr.resError = mapReadErr;
        return 0;
    }

    if (typeListOff + 2 > file->mapSize) {
        gResMgr.resError = mapReadErr;
        return 0;
    }

    typeList = (UInt8*)map + typeListOff;
    numTypes = read_be16(typeList) + 1;  /* Count is stored as n-1 */

    if (index > numTypes) {
        gResMgr.resError = resNotFound;
        return 0;
    }

    /* Bounds check: ensure type list entries fit in map */
    UInt32 typeListSize = 2 + ((UInt32)numTypes * sizeof(TypeListEntry));
    if (typeListOff + typeListSize > file->mapSize) {
        gResMgr.resError = mapReadErr;
        return 0;
    }

    typeList += 2;  /* Skip count */

    TypeListEntry* entry = (TypeListEntry*)(typeList + (index - 1) * sizeof(TypeListEntry));
    return read_be32((UInt8*)&entry->resType);
}

/* Helper: Get Nth resource of a given type (1-indexed) */
static RefListEntry* ResMap_GetIndResource(ResFile* file, ResType type, SInt16 index) {
    TypeListEntry* typeEntry;
    UInt16 count;
    UInt16 refListOff;
    UInt8* refList;

    if (!file || !file->map || index < 1) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    typeEntry = ResMap_FindType(file, type);
    if (!typeEntry) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    count = read_be16((UInt8*)&typeEntry->count) + 1;  /* Count is stored as n-1 */
    if (index > count) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    refListOff = read_be16((UInt8*)&typeEntry->refListOffset);

    /* Reference list offset is from start of type list */
    ResMapHeader* map = file->map;
    UInt16 typeListOff = read_be16((UInt8*)&map->typeListOffset);

    /* Bounds check */
    if (typeListOff == 0xFFFF || typeListOff >= file->mapSize) {
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    /* Bounds check: ensure reference list is within map */
    UInt32 refListStart = typeListOff + refListOff;
    UInt32 refListSize = count * sizeof(RefListEntry);
    if (refListStart >= file->mapSize || refListStart + refListSize > file->mapSize) {
        gResMgr.resError = mapReadErr;
        return NULL;
    }

    refList = (UInt8*)map + refListStart;

    /* Return the Nth entry */
    return (RefListEntry*)(refList + (index - 1) * sizeof(RefListEntry));
}

SInt16 Count1Resources(ResType theType) {
    if (gResMgr.curResFile < 0) return 0;

    TypeListEntry* typeEntry = ResMap_FindType(&gResMgr.resFiles[gResMgr.curResFile], theType);
    if (typeEntry) {
        return read_be16((UInt8*)&typeEntry->count) + 1;
    }

    return 0;
}

/* Get indexed resource from current file (1-indexed) */
Handle GetIndResource(ResType theType, SInt16 index) {
    ResFile* file;
    RefListEntry* ref;
    Handle h;

    if (gResMgr.curResFile < 0) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    file = &gResMgr.resFiles[gResMgr.curResFile];
    ref = ResMap_GetIndResource(file, theType, index);
    if (!ref) {
        gResMgr.resError = resNotFound;
        return NULL;
    }

    h = ResFile_LoadResource(file, ref);
    if (h) {
        gResMgr.resError = noErr;
    } else {
        gResMgr.resError = resNotFound;
    }
    return h;
}

/* Get indexed resource from 1-file (usually current file) (1-indexed) */
Handle Get1IndResource(ResType theType, SInt16 index) {
    return GetIndResource(theType, index);
}

/* Get Nth type from current file (1-indexed) */
void GetIndType(ResType *theType, SInt16 index) {
    ResType type;

    if (!theType) return;

    if (gResMgr.curResFile < 0) {
        gResMgr.resError = resNotFound;
        *theType = 0;
        return;
    }

    type = ResMap_GetIndType(&gResMgr.resFiles[gResMgr.curResFile], index);
    if (type == 0) {
        gResMgr.resError = resNotFound;
    } else {
        gResMgr.resError = noErr;
    }
    *theType = type;
}

/* Get Nth type from 1-file (1-indexed) */
void Get1IndType(ResType *theType, SInt16 index) {
    GetIndType(theType, index);
}

/* Count total types in current file */
SInt16 CountTypes(void) {
    ResMapHeader* map;
    UInt16 typeListOff;
    UInt8* typeList;

    if (gResMgr.curResFile < 0) {
        gResMgr.resError = resNotFound;
        return 0;
    }

    ResFile* file = &gResMgr.resFiles[gResMgr.curResFile];
    if (!file->map) {
        gResMgr.resError = mapReadErr;
        return 0;
    }

    map = file->map;
    typeListOff = read_be16((UInt8*)&map->typeListOffset);

    /* Bounds check */
    if (typeListOff == 0xFFFF || typeListOff >= file->mapSize) {
        gResMgr.resError = mapReadErr;
        return 0;
    }

    if (typeListOff + 2 > file->mapSize) {
        gResMgr.resError = mapReadErr;
        return 0;
    }

    typeList = (UInt8*)map + typeListOff;
    SInt16 numTypes = read_be16(typeList) + 1;  /* Count is stored as n-1 */

    gResMgr.resError = noErr;
    return numTypes;
}

/* Count types in 1-file (usually current file) */
SInt16 Count1Types(void) {
    return CountTypes();
}

/* Unique ID generation */
SInt16 UniqueID(ResType theType) {
    (void)theType;
    return gResMgr.nextUniqueID++;
}

SInt16 Unique1ID(ResType theType) {
    return UniqueID(theType);
}
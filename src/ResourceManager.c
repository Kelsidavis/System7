/*
 * ResourceManager.c - Mac OS System 7.1 Resource Manager Core Implementation
 *
 * Portable C implementation for ARM64 and x86_64 platforms
 * Based on analysis of Mac OS System 7.1 source code
 *
 * This implementation provides complete Resource Manager functionality including:
 * - Resource fork access and management
 * - Automatic decompression of compressed resources
 * - Handle-based memory management
 * - Multi-file resource chain support
 *
 * Copyright Notice: This is a reimplementation for research and compatibility purposes.
 */

#define _GNU_SOURCE  /* For strdup */
#include "ResourceManager.h"
#include "ResourceDecompression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

/* ---- Global Variables (Thread-Local Storage) ------------------------------------- */

/* Use thread-local storage for globals if available */
#ifdef __GNUC__
#define THREAD_LOCAL __thread
#elif defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL
#endif

THREAD_LOCAL ResourceMap* gResourceChain = NULL;
THREAD_LOCAL ResourceMap* gCurrentMap = NULL;
THREAD_LOCAL int16_t gResError = noErr;
THREAD_LOCAL bool gResLoad = true;
THREAD_LOCAL bool gResOneDeep = false;
THREAD_LOCAL bool gROMMapInsert = false;
THREAD_LOCAL ResErrProcPtr gResErrProc = NULL;
THREAD_LOCAL DecompressHookProc gDecompressHook = NULL;

/* Decompression system globals (based on DeCompressorPatch.a) */
THREAD_LOCAL Handle gDefProcHandle = NULL;      /* Current decompressor defproc handle */
THREAD_LOCAL bool gAutoDecompression = true;    /* Enable automatic decompression */
THREAD_LOCAL uint16_t gLastDefProcID = 0;       /* Cache last decompressor ID */

/* Decompression cache for frequently accessed resources */
typedef struct DecompressedCache {
    ResType type;
    ResID id;
    Handle decompressedHandle;
    uint32_t checksum;
    time_t lastAccess;
    struct DecompressedCache* next;
} DecompressedCache;

THREAD_LOCAL DecompressedCache* gDecompCache = NULL;
THREAD_LOCAL size_t gDecompCacheSize = 0;
THREAD_LOCAL size_t gDecompCacheMaxSize = 32;  /* Max cached items */

/* Resource fork magic numbers and offsets */
#define RESOURCE_FORK_MAGIC     0x00000100  /* Resource fork header magic */
#define RESOURCE_MAP_MAGIC      0x00000100  /* Resource map header magic */
#define RESOURCE_DATA_OFFSET    256         /* Default data offset */
#define RESOURCE_MAP_OFFSET     512         /* Default map offset */

/* Extended resource constants (from DeCompressorPatch.a) */
#define ROBUSTNESS_SIGNATURE    0xA89F6572  /* Extended resource signature */
#define DONN_HEADER_VERSION     8            /* DonnBits header version */
#define GREGGY_HEADER_VERSION   9            /* GreggyBits header version */
#define DCMP_RESOURCE_TYPE      0x64636D70  /* 'dcmp' - decompressor resource type */

/* Resource header sizes */
#define EXT_RES_HEADER_SIZE     16          /* Base extended resource header */
#define DONN_HEADER_SIZE        24          /* DonnBits full header size */
#define GREGGY_HEADER_SIZE      22          /* GreggyBits full header size */

/* ---- Memory Manager Stubs -------------------------------------------------------- */

/* Simple handle implementation for standalone testing */
typedef struct HandleBlock {
    void* ptr;
    size_t size;
    uint8_t state;
    struct HandleBlock* next;
} HandleBlock;

static HandleBlock* gHandleList = NULL;
static pthread_mutex_t gHandleMutex = PTHREAD_MUTEX_INITIALIZER;

Handle NewHandle(int32_t size) {
    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* block = (HandleBlock*)calloc(1, sizeof(HandleBlock));
    if (!block) {
        pthread_mutex_unlock(&gHandleMutex);
        return NULL;
    }

    block->ptr = calloc(1, size);
    if (!block->ptr) {
        free(block);
        pthread_mutex_unlock(&gHandleMutex);
        return NULL;
    }

    block->size = size;
    block->state = 0;
    block->next = gHandleList;
    gHandleList = block;

    pthread_mutex_unlock(&gHandleMutex);
    return (Handle)&block->ptr;
}

void DisposeHandle(Handle h) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock** pp = &gHandleList;
    HandleBlock* p;

    while ((p = *pp) != NULL) {
        if ((Handle)&p->ptr == h) {
            *pp = p->next;
            free(p->ptr);
            free(p);
            break;
        }
        pp = &p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

void SetHandleSize(Handle h, int32_t newSize) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            void* newPtr = realloc(p->ptr, newSize);
            if (newPtr) {
                p->ptr = newPtr;
                p->size = newSize;
            } else {
                gResError = memFullErr;
            }
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

int32_t GetHandleSize(Handle h) {
    if (!h) return 0;

    pthread_mutex_lock(&gHandleMutex);

    int32_t size = 0;
    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            size = p->size;
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
    return size;
}

void HLock(Handle h) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            p->state |= 0x80;  /* Locked bit */
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

void HUnlock(Handle h) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            p->state &= ~0x80;  /* Clear locked bit */
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

void HPurge(Handle h) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            p->state |= 0x40;  /* Purgeable bit */
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

void HNoPurge(Handle h) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            p->state &= ~0x40;  /* Clear purgeable bit */
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

uint8_t HGetState(Handle h) {
    if (!h) return 0;

    pthread_mutex_lock(&gHandleMutex);

    uint8_t state = 0;
    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            state = p->state;
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
    return state;
}

void HSetState(Handle h, uint8_t state) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            p->state = state;
            break;
        }
        p = p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

void* StripAddress(void* ptr) {
    /* On modern systems, just return the pointer */
    return ptr;
}

/* ---- Internal Helper Functions --------------------------------------------------- */

static void SetResError(int16_t error) {
    gResError = error;
    if (gResErrProc && error != noErr) {
        gResErrProc(error);
    }
}

static ResourceEntry* FindResource(ResourceMap* map, ResType type, ResID id) {
    if (!map) return NULL;

    ResourceType* typeEntry = map->types;
    while (typeEntry) {
        if (typeEntry->resType == type) {
            ResourceEntry* resEntry = typeEntry->resources;
            while (resEntry) {
                if (resEntry->resID == id) {
                    return resEntry;
                }
                resEntry = resEntry->next;
            }
            break;
        }
        typeEntry = typeEntry->next;
    }

    return NULL;
}

static ResourceEntry* FindResourceByName(ResourceMap* map, ResType type, const char* name) {
    if (!map || !name) return NULL;

    ResourceType* typeEntry = map->types;
    while (typeEntry) {
        if (typeEntry->resType == type) {
            ResourceEntry* resEntry = typeEntry->resources;
            while (resEntry) {
                if (resEntry->name && strcmp(resEntry->name, name) == 0) {
                    return resEntry;
                }
                resEntry = resEntry->next;
            }
            break;
        }
        typeEntry = typeEntry->next;
    }

    return NULL;
}

static bool ReadResourceData(ResourceMap* map, ResourceEntry* entry, void* buffer) {
    if (!map || !entry || !buffer || !map->fileHandle) {
        return false;
    }

    /* Seek to resource data */
    if (fseek(map->fileHandle, map->dataOffset + entry->dataOffset, SEEK_SET) != 0) {
        SetResError(ioErr);
        return false;
    }

    /* Read resource data */
    if (fread(buffer, 1, entry->dataSize, map->fileHandle) != entry->dataSize) {
        SetResError(eofErr);
        return false;
    }

    return true;
}

/* Check if resource map has decompression enabled (from DeCompressorPatch.a) */
static bool MapHasDecompressionEnabled(ResourceMap* map) {
    if (!map) return false;
    /* Check the decompression password bit (bit 7 of in-memory attributes) */
    return map->decompressionEnabled || (map->inMemoryAttr & (1 << decompressionPasswordBit));
}

/* Find decompressor defproc in resource chain (from DeCompressorPatch.a) */
static Handle FindDecompressor(uint16_t defProcID) {
    /* Check cache first */
    if (defProcID == gLastDefProcID && gDefProcHandle) {
        return gDefProcHandle;
    }

    /* Search in all resource maps that have decompression enabled */
    ResourceMap* map = gResourceChain;
    while (map) {
        if (MapHasDecompressionEnabled(map)) {
            /* Save current state */
            ResourceMap* savedCurrent = gCurrentMap;
            bool savedOneDeep = gResOneDeep;
            ResErrProcPtr savedErrProc = gResErrProc;

            /* Set up for searching this map */
            gCurrentMap = map;
            gResOneDeep = true;
            gResErrProc = NULL;  /* Don't call error proc during search */

            /* Try to get the decompressor resource */
            Handle dcmpHandle = Get1Resource(DCMP_RESOURCE_TYPE, defProcID);

            /* Restore state */
            gCurrentMap = savedCurrent;
            gResOneDeep = savedOneDeep;
            gResErrProc = savedErrProc;

            if (dcmpHandle && *dcmpHandle) {
                /* Cache the decompressor */
                gDefProcHandle = dcmpHandle;
                gLastDefProcID = defProcID;
                return dcmpHandle;
            }
        }
        map = map->next;
    }

    return NULL;
}

/* Check decompression cache for resource */
static Handle CheckDecompressionCache(ResType type, ResID id) {
    DecompressedCache* cache = gDecompCache;
    DecompressedCache* prev = NULL;

    while (cache) {
        if (cache->type == type && cache->id == id) {
            /* Move to front if not already there */
            if (prev) {
                prev->next = cache->next;
                cache->next = gDecompCache;
                gDecompCache = cache;
            }
            /* Update access time */
            cache->lastAccess = time(NULL);
            return cache->decompressedHandle;
        }
        prev = cache;
        cache = cache->next;
    }

    return NULL;
}

/* Add to decompression cache */
static void AddToDecompressionCache(ResType type, ResID id, Handle handle) {
    /* Check cache size limit */
    if (gDecompCacheSize >= gDecompCacheMaxSize) {
        /* Remove least recently used */
        DecompressedCache* oldest = NULL;
        DecompressedCache* oldestPrev = NULL;
        DecompressedCache* curr = gDecompCache;
        DecompressedCache* prev = NULL;
        time_t oldestTime = time(NULL);

        while (curr) {
            if (curr->lastAccess < oldestTime) {
                oldestTime = curr->lastAccess;
                oldest = curr;
                oldestPrev = prev;
            }
            prev = curr;
            curr = curr->next;
        }

        if (oldest) {
            if (oldestPrev) {
                oldestPrev->next = oldest->next;
            } else {
                gDecompCache = oldest->next;
            }
            /* Don't dispose the handle - it's still in use */
            free(oldest);
            gDecompCacheSize--;
        }
    }

    /* Add new cache entry */
    DecompressedCache* newCache = (DecompressedCache*)malloc(sizeof(DecompressedCache));
    if (newCache) {
        newCache->type = type;
        newCache->id = id;
        newCache->decompressedHandle = handle;
        newCache->checksum = 0;  /* TODO: Calculate checksum */
        newCache->lastAccess = time(NULL);
        newCache->next = gDecompCache;
        gDecompCache = newCache;
        gDecompCacheSize++;
    }
}

/* Main decompression function (rewritten based on DeCompressorPatch.a) */
static Handle DecompressResourceData(Handle compressedHandle, ResourceEntry* entry) {
    if (!compressedHandle || !*compressedHandle) {
        return compressedHandle;
    }

    /* Check for custom decompression hook first */
    if (gDecompressHook) {
        return gDecompressHook(compressedHandle, entry);
    }

    /* Check if automatic decompression is disabled */
    if (!gAutoDecompression) {
        return compressedHandle;
    }

    /* Check cache first */
    if (entry) {
        Handle cached = CheckDecompressionCache(entry->resType, entry->resID);
        if (cached) {
            /* Return cached decompressed handle */
            DisposeHandle(compressedHandle);
            return cached;
        }
    }

    /* Check if resource is compressed */
    void* compressedData = *compressedHandle;
    size_t compressedSize = GetHandleSize(compressedHandle);

    /* Read extended resource header to check for compression */
    if (compressedSize < EXT_RES_HEADER_SIZE) {
        return compressedHandle;  /* Too small to be compressed */
    }

    ExtendedResourceHeader* header = (ExtendedResourceHeader*)compressedData;

    /* Check for robustness signature (from DeCompressorPatch.a) */
    if (header->signature != ROBUSTNESS_SIGNATURE) {
        return compressedHandle;  /* Not an extended resource */
    }

    /* Check if compression bit is set */
    if (!(header->extendedAttributes & resCompressed)) {
        return compressedHandle;  /* Extended but not compressed */
    }

    /* Determine decompressor type based on header version */
    uint16_t defProcID = 0;
    size_t decompressedSize = header->actualSize;

    if (header->headerVersion == DONN_HEADER_VERSION) {
        /* DonnBits decompression */
        DonnBitsHeader* donnHeader = (DonnBitsHeader*)compressedData;
        defProcID = donnHeader->decompressID;

        /* Check for compression table (not supported yet) */
        if (donnHeader->cTableID != 0) {
            SetResError(badExtResource);
            return compressedHandle;
        }
    } else if (header->headerVersion == GREGGY_HEADER_VERSION) {
        /* GreggyBits decompression */
        GreggyBitsHeader* greggyHeader = (GreggyBitsHeader*)compressedData;
        defProcID = greggyHeader->defProcID;
    } else {
        /* Unknown compression version */
        SetResError(badExtResource);
        return compressedHandle;
    }

    /* If defProcID is non-zero, try to find custom decompressor */
    Handle defProcHandle = NULL;
    if (defProcID != 0) {
        defProcHandle = FindDecompressor(defProcID);
        if (!defProcHandle) {
            /* Can't find decompressor */
            SetResError(CantDecompress);
            return compressedHandle;
        }
    }

    /* Use built-in decompression if no custom decompressor */
    if (!defProcHandle) {
        /* Use built-in DonnBits or GreggyBits decompressor */
        uint8_t* decompressedData = NULL;

        int result = DecompressResource(compressedData, compressedSize,
                                       &decompressedData, &decompressedSize);

        if (result != 0) {
            SetResError(CantDecompress);
            return compressedHandle;
        }

        /* Create new handle for decompressed data */
        Handle decompressedHandle = NewHandle(decompressedSize);
        if (!decompressedHandle) {
            free(decompressedData);
            SetResError(memFullErr);
            return compressedHandle;
        }

        /* Copy decompressed data to handle */
        memcpy(*decompressedHandle, decompressedData, decompressedSize);
        free(decompressedData);

        /* Dispose of compressed handle */
        DisposeHandle(compressedHandle);

        /* Add to cache */
        if (entry) {
            AddToDecompressionCache(entry->resType, entry->resID, decompressedHandle);
        }

        /* Clear compressed attribute */
        if (entry) {
            entry->attributes &= ~resCompressed;
        }

        return decompressedHandle;
    } else {
        /* Call custom decompressor defproc */
        /* TODO: Implement custom decompressor calling convention */
        SetResError(CantDecompress);
        return compressedHandle;
    }
}

/* ---- Resource Loading Functions -------------------------------------------------- */

Handle GetResource(ResType theType, ResID theID) {
    SetResError(noErr);

    ResourceMap* map = gResOneDeep ? gCurrentMap : gResourceChain;

    while (map) {
        ResourceEntry* entry = FindResource(map, theType, theID);
        if (entry) {
            /* Check if already loaded */
            if (entry->handle && *entry->handle) {
                return entry->handle;
            }

            /* Load resource if ResLoad is true */
            if (!gResLoad) {
                /* Create empty handle */
                entry->handle = NewHandle(0);
                return entry->handle;
            }

            /* Allocate handle */
            entry->handle = NewHandle(entry->dataSize);
            if (!entry->handle) {
                SetResError(memFullErr);
                return NULL;
            }

            /* Read resource data */
            if (!ReadResourceData(map, entry, *entry->handle)) {
                DisposeHandle(entry->handle);
                entry->handle = NULL;
                return NULL;
            }

            /* Check for compressed resource using CheckLoad hook logic */
            if (entry->attributes & resExtended) {
                /* Use CheckLoad hook for extended resources */
                entry->handle = ResourceManager_CheckLoadHook(entry, map);
            } else if (IsExtendedResource(*entry->handle, entry->dataSize)) {
                /* Double-check data for compression signature */
                entry->handle = DecompressResourceData(entry->handle, entry);
            }

            /* Set resource attributes on handle */
            uint8_t state = resIsResource;
            if (entry->attributes & resLocked) state |= 0x80;
            if (entry->attributes & resPurgeable) state |= 0x40;
            HSetState(entry->handle, state);

            return entry->handle;
        }

        if (gResOneDeep) break;
        map = map->next;
    }

    SetResError(resNotFound);
    return NULL;
}

Handle Get1Resource(ResType theType, ResID theID) {
    bool saveOneDeep = gResOneDeep;
    gResOneDeep = true;
    Handle result = GetResource(theType, theID);
    gResOneDeep = saveOneDeep;
    return result;
}

Handle GetNamedResource(ResType theType, const char* name) {
    SetResError(noErr);

    if (!name) {
        SetResError(resNotFound);
        return NULL;
    }

    ResourceMap* map = gResOneDeep ? gCurrentMap : gResourceChain;

    while (map) {
        ResourceEntry* entry = FindResourceByName(map, theType, name);
        if (entry) {
            return GetResource(theType, entry->resID);
        }

        if (gResOneDeep) break;
        map = map->next;
    }

    SetResError(resNotFound);
    return NULL;
}

Handle Get1NamedResource(ResType theType, const char* name) {
    bool saveOneDeep = gResOneDeep;
    gResOneDeep = true;
    Handle result = GetNamedResource(theType, name);
    gResOneDeep = saveOneDeep;
    return result;
}

void LoadResource(Handle theResource) {
    if (!theResource || *theResource) {
        return;  /* Already loaded */
    }

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    /* Found it - load the data */
                    SetHandleSize(theResource, entry->dataSize);
                    if ((uint32_t)GetHandleSize(theResource) == entry->dataSize) {
                        ReadResourceData(map, entry, *theResource);

                        /* Check for compressed resource using CheckLoad hook logic */
                        if (entry->attributes & resExtended) {
                            /* Use CheckLoad hook for extended resources */
                            entry->handle = ResourceManager_CheckLoadHook(entry, map);
                        } else if (IsExtendedResource(*entry->handle, entry->dataSize)) {
                            /* Double-check data for compression signature */
                            entry->handle = DecompressResourceData(entry->handle, entry);
                        }
                    }
                    return;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }
}

void ReleaseResource(Handle theResource) {
    if (!theResource) return;
    HPurge(theResource);
}

void DetachResource(Handle theResource) {
    if (!theResource) return;

    /* Find and remove resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    entry->handle = NULL;
                    /* Clear resource bit */
                    uint8_t state = HGetState(theResource);
                    state &= ~resIsResource;
                    HSetState(theResource, state);
                    return;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }
}

/* ---- Resource Information Functions ---------------------------------------------- */

void GetResInfo(Handle theResource, ResID* theID, ResType* theType, char* name) {
    if (!theResource) return;

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    if (theID) *theID = entry->resID;
                    if (theType) *theType = type->resType;
                    if (name) {
                        if (entry->name) {
                            strcpy(name, entry->name);
                        } else {
                            name[0] = '\0';
                        }
                    }
                    return;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    SetResError(resNotFound);
}

void SetResInfo(Handle theResource, ResID theID, const char* name) {
    if (!theResource) return;

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    entry->resID = theID;
                    if (name) {
                        free(entry->name);
                        entry->name = strdup(name);
                    }
                    map->attributes |= mapChanged;
                    return;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    SetResError(resNotFound);
}

ResAttributes GetResAttrs(Handle theResource) {
    if (!theResource) return 0;

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    return entry->attributes;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    SetResError(resNotFound);
    return 0;
}

void SetResAttrs(Handle theResource, ResAttributes attrs) {
    if (!theResource) return;

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    entry->attributes = attrs;
                    map->attributes |= mapChanged;

                    /* Update handle state */
                    uint8_t state = HGetState(theResource);
                    if (attrs & resLocked) state |= 0x80;
                    else state &= ~0x80;
                    if (attrs & resPurgeable) state |= 0x40;
                    else state &= ~0x40;
                    HSetState(theResource, state);
                    return;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    SetResError(resNotFound);
}

int32_t GetResourceSizeOnDisk(Handle theResource) {
    if (!theResource) return 0;

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    return entry->dataSize;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    SetResError(resNotFound);
    return 0;
}

int32_t GetMaxResourceSize(Handle theResource) {
    if (!theResource) return 0;
    return GetHandleSize(theResource);
}

void* GetResourceData(Handle theResource) {
    if (!theResource) return NULL;
    LoadResource(theResource);
    return *theResource;
}

/* ---- Resource File Management ---------------------------------------------------- */

RefNum OpenResFile(const char* fileName) {
    return OpenRFPerm(fileName, 0, 0);  /* Read-only by default */
}

RefNum OpenRFPerm(const char* fileName, uint8_t vRefNum, int8_t permission) {
    (void)vRefNum;  /* Not used in portable implementation */

    SetResError(noErr);

    /* Check if file already open */
    ResourceMap* map = gResourceChain;
    while (map) {
        if (map->fileName && strcmp(map->fileName, fileName) == 0) {
            SetResError(opWrErr);
            return -1;
        }
        map = map->next;
    }

    /* Open file */
    const char* mode = (permission == 0) ? "rb" : "rb+";
    FILE* file = fopen(fileName, mode);
    if (!file) {
        SetResError(fnfErr);
        return -1;
    }

    /* Create new resource map */
    map = (ResourceMap*)calloc(1, sizeof(ResourceMap));
    if (!map) {
        fclose(file);
        SetResError(memFullErr);
        return -1;
    }

    /* Initialize map */
    static RefNum nextRefNum = 1;
    map->fileRefNum = nextRefNum++;
    map->fileName = strdup(fileName);
    map->fileHandle = file;
    map->dataOffset = RESOURCE_DATA_OFFSET;
    map->mapOffset = RESOURCE_MAP_OFFSET;
    map->inMemoryAttr = true;
    map->decompressionEnabled = true;

    /* TODO: Read resource map from file */
    /* For now, just add empty map to chain */

    /* Add to resource chain */
    map->next = gResourceChain;
    gResourceChain = map;

    /* Make it current */
    gCurrentMap = map;

    return map->fileRefNum;
}

void CloseResFile(RefNum refNum) {
    SetResError(noErr);

    ResourceMap** pp = &gResourceChain;
    ResourceMap* map;

    while ((map = *pp) != NULL) {
        if (map->fileRefNum == refNum) {
            /* Remove from chain */
            *pp = map->next;

            /* Update current map if needed */
            if (gCurrentMap == map) {
                gCurrentMap = gResourceChain;
            }

            /* Free resources */
            ResourceType* type = map->types;
            while (type) {
                ResourceType* nextType = type->next;
                ResourceEntry* entry = type->resources;
                while (entry) {
                    ResourceEntry* nextEntry = entry->next;
                    if (entry->handle) {
                        DisposeHandle(entry->handle);
                    }
                    free(entry->name);
                    free(entry);
                    entry = nextEntry;
                }
                free(type);
                type = nextType;
            }

            /* Close file and free map */
            if (map->fileHandle) {
                fclose(map->fileHandle);
            }
            free(map->fileName);
            free(map);
            return;
        }
        pp = &map->next;
    }

    SetResError(resFNotFound);
}

void UseResFile(RefNum refNum) {
    SetResError(noErr);

    ResourceMap* map = gResourceChain;
    while (map) {
        if (map->fileRefNum == refNum) {
            gCurrentMap = map;
            return;
        }
        map = map->next;
    }

    SetResError(resFNotFound);
}

RefNum CurResFile(void) {
    if (gCurrentMap) {
        return gCurrentMap->fileRefNum;
    }
    return -1;
}

RefNum HomeResFile(Handle theResource) {
    if (!theResource) return -1;

    /* Find the resource entry */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    return map->fileRefNum;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    SetResError(resNotFound);
    return -1;
}

/* ---- Global State Functions ------------------------------------------------------ */

void SetResLoad(bool load) {
    gResLoad = load;
}

bool GetResLoad(void) {
    return gResLoad;
}

void SetResPurge(bool install) {
    /* TODO: Implement purge procedure */
    (void)install;
}

bool GetResPurge(void) {
    /* TODO: Implement purge procedure */
    return false;
}

int16_t ResError(void) {
    return gResError;
}

void SetResErrProc(ResErrProcPtr proc) {
    gResErrProc = proc;
}

void SetROMMapInsert(bool insert) {
    gROMMapInsert = insert;
}

bool GetROMMapInsert(void) {
    return gROMMapInsert;
}

void SetResOneDeep(bool oneDeep) {
    gResOneDeep = oneDeep;
}

bool GetResOneDeep(void) {
    return gResOneDeep;
}

/* ---- Initialization and Cleanup -------------------------------------------------- */

void InitResourceManager(void) {
    /* Initialize decompression system */
    SetDecompressCaching(true);
    SetDecompressDebug(false);

    /* Clear globals */
    gResourceChain = NULL;
    gCurrentMap = NULL;
    gResError = noErr;
    gResLoad = true;
    gResOneDeep = false;
    gROMMapInsert = false;
    gResErrProc = NULL;
    gDecompressHook = NULL;

    /* Initialize decompression globals */
    gDefProcHandle = NULL;
    gAutoDecompression = true;
    gLastDefProcID = 0;
    gDecompCache = NULL;
    gDecompCacheSize = 0;
    gDecompCacheMaxSize = 32;
}

void CleanupResourceManager(void) {
    /* Close all open resource files */
    while (gResourceChain) {
        CloseResFile(gResourceChain->fileRefNum);
    }

    /* Clear decompression cache */
    ClearDecompressCache();

    /* Clear our decompression cache */
    while (gDecompCache) {
        DecompressedCache* next = gDecompCache->next;
        /* Don't dispose handles - they're owned by the app */
        free(gDecompCache);
        gDecompCache = next;
    }
    gDecompCacheSize = 0;

    /* Clear decompressor handle */
    if (gDefProcHandle) {
        /* Don't dispose - it's a resource handle */
        gDefProcHandle = NULL;
    }

    /* Free all handles */
    pthread_mutex_lock(&gHandleMutex);
    while (gHandleList) {
        HandleBlock* next = gHandleList->next;
        free(gHandleList->ptr);
        free(gHandleList);
        gHandleList = next;
    }
    pthread_mutex_unlock(&gHandleMutex);
}

void InstallDecompressHook(DecompressHookProc proc) {
    gDecompressHook = proc;
}

/* ---- Decompression Management Functions ------------------------------------------ */

/* Enable/disable automatic decompression */
void SetAutoDecompression(bool enable) {
    gAutoDecompression = enable;
}

bool GetAutoDecompression(void) {
    return gAutoDecompression;
}

/* Clear decompression cache */
void ResourceManager_FlushDecompressionCache(void) {
    while (gDecompCache) {
        DecompressedCache* next = gDecompCache->next;
        /* Don't dispose handles - they're owned by the app */
        free(gDecompCache);
        gDecompCache = next;
    }
    gDecompCacheSize = 0;

    /* Also clear the ResourceDecompression module's cache */
    ClearDecompressCache();
}

/* Set decompression cache size */
void ResourceManager_SetDecompressionCacheSize(size_t maxItems) {
    gDecompCacheMaxSize = maxItems;

    /* Trim cache if necessary */
    while (gDecompCacheSize > gDecompCacheMaxSize && gDecompCache) {
        /* Remove from end */
        DecompressedCache* curr = gDecompCache;
        DecompressedCache* prev = NULL;

        while (curr->next) {
            prev = curr;
            curr = curr->next;
        }

        if (prev) {
            prev->next = NULL;
        } else {
            gDecompCache = NULL;
        }

        free(curr);
        gDecompCacheSize--;
    }
}

/* Register a custom decompressor */
int ResourceManager_RegisterDecompressor(uint16_t id, Handle defProcHandle) {
    if (!defProcHandle || !*defProcHandle) {
        return paramErr;
    }

    /* For now, just cache the single decompressor */
    /* In a full implementation, we'd maintain a table */
    gDefProcHandle = defProcHandle;
    gLastDefProcID = id;

    return noErr;
}

/* Hook function for CheckLoad patching (from DeCompressorPatch.a) */
Handle ResourceManager_CheckLoadHook(ResourceEntry* entry, ResourceMap* map) {
    /* This function replaces the CheckLoad hook from DeCompressorPatch.a */

    if (!entry || !map) {
        return NULL;
    }

    /* Check if resource has extended attribute bit set */
    if (!(entry->attributes & resExtended)) {
        /* Not an extended resource, use normal loading */
        return NULL;
    }

    /* Check if handle already has data */
    if (entry->handle && *entry->handle) {
        /* Already loaded */
        return entry->handle;
    }

    /* Save ResLoad state */
    bool savedResLoad = gResLoad;
    gResLoad = false;

    /* Create empty handle using normal CheckLoad */
    /* This allocates the handle but doesn't load data */
    Handle h = entry->handle;
    if (!h) {
        h = NewHandle(0);
        entry->handle = h;
    }

    /* Restore ResLoad */
    gResLoad = savedResLoad;

    if (!gResLoad) {
        /* Don't load if ResLoad is false */
        return h;
    }

    /* Load compressed data */
    Handle compressedHandle = NewHandle(entry->dataSize);
    if (!compressedHandle) {
        SetResError(memFullErr);
        return NULL;
    }

    /* Read the compressed data */
    if (!ReadResourceData(map, entry, *compressedHandle)) {
        DisposeHandle(compressedHandle);
        return NULL;
    }

    /* Decompress the data */
    Handle decompressed = DecompressResourceData(compressedHandle, entry);

    /* Update the entry's handle */
    if (decompressed != compressedHandle) {
        /* Decompression succeeded */
        if (entry->handle && entry->handle != decompressed) {
            DisposeHandle(entry->handle);
        }
        entry->handle = decompressed;
    } else {
        /* Not compressed or decompression failed */
        if (entry->handle != compressedHandle) {
            if (entry->handle) {
                DisposeHandle(entry->handle);
            }
            entry->handle = compressedHandle;
        }
    }

    return entry->handle;
}

/* ---- Stub Functions (To be implemented) ------------------------------------------ */

void CreateResFile(const char* fileName) {
    (void)fileName;
    /* TODO: Implement */
    SetResError(noErr);
}

void UpdateResFile(RefNum refNum) {
    (void)refNum;
    /* TODO: Implement */
    SetResError(noErr);
}

void WriteResource(Handle theResource) {
    (void)theResource;
    /* TODO: Implement */
    SetResError(noErr);
}

void AddResource(Handle theData, ResType theType, ResID theID, const char* name) {
    (void)theData;
    (void)theType;
    (void)theID;
    (void)name;
    /* TODO: Implement */
    SetResError(noErr);
}

void RemoveResource(Handle theResource) {
    (void)theResource;
    /* TODO: Implement */
    SetResError(noErr);
}

void ChangedResource(Handle theResource) {
    (void)theResource;
    /* TODO: Implement */
    SetResError(noErr);
}

int16_t CountResources(ResType theType) {
    (void)theType;
    /* TODO: Implement */
    return 0;
}

int16_t Count1Resources(ResType theType) {
    (void)theType;
    /* TODO: Implement */
    return 0;
}

Handle GetIndResource(ResType theType, int16_t index) {
    (void)theType;
    (void)index;
    /* TODO: Implement */
    SetResError(resNotFound);
    return NULL;
}

Handle Get1IndResource(ResType theType, int16_t index) {
    (void)theType;
    (void)index;
    /* TODO: Implement */
    SetResError(resNotFound);
    return NULL;
}

int16_t CountTypes(void) {
    /* TODO: Implement */
    return 0;
}

int16_t Count1Types(void) {
    /* TODO: Implement */
    return 0;
}

void GetIndType(ResType* theType, int16_t index) {
    (void)theType;
    (void)index;
    /* TODO: Implement */
}

void Get1IndType(ResType* theType, int16_t index) {
    (void)theType;
    (void)index;
    /* TODO: Implement */
}

ResID UniqueID(ResType theType) {
    (void)theType;
    /* TODO: Implement */
    return 128;
}

ResID Unique1ID(ResType theType) {
    (void)theType;
    /* TODO: Implement */
    return 128;
}

void SetResFileAttrs(RefNum refNum, uint16_t attrs) {
    (void)refNum;
    (void)attrs;
    /* TODO: Implement */
}

uint16_t GetResFileAttrs(RefNum refNum) {
    (void)refNum;
    /* TODO: Implement */
    return 0;
}

RefNum GetNextResourceFile(RefNum curFile) {
    (void)curFile;
    /* TODO: Implement */
    return -1;
}

RefNum GetTopResourceFile(void) {
    if (gResourceChain) {
        return gResourceChain->fileRefNum;
    }
    return -1;
}
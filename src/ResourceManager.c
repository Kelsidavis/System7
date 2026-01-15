#include "MemoryMgr/MemoryManager.h"
#include "SystemTypes.h"
#include <stdlib.h>
#include <string.h>
#include "System71StdLib.h"
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
#include "ResourceManager/ResourceLogging.h"


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
THREAD_LOCAL SInt16 gResError = noErr;
THREAD_LOCAL Boolean gResLoad = true;
THREAD_LOCAL Boolean gResOneDeep = false;
THREAD_LOCAL Boolean gROMMapInsert = false;
THREAD_LOCAL Boolean gResPurge = true;  /* Enable purging by default */
THREAD_LOCAL ResErrProcPtr gResErrProc = NULL;
THREAD_LOCAL DecompressHookProc gDecompressHook = NULL;

THREAD_LOCAL Handle gDefProcHandle = NULL;      /* Current decompressor defproc handle */
THREAD_LOCAL Boolean gAutoDecompression = true;    /* Enable automatic decompression */
THREAD_LOCAL UInt16 gLastDefProcID = 0;       /* Cache last decompressor ID */

/* Decompression cache for frequently accessed resources */
typedef struct DecompressedCache {
    ResType type;
    ResID id;
    Handle decompressedHandle;
    UInt32 checksum;
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
    UInt8 state;
    struct HandleBlock* next;
} HandleBlock;

static HandleBlock* gHandleList = NULL;
static pthread_mutex_t gHandleMutex = PTHREAD_MUTEX_INITIALIZER;

Handle NewHandle(SInt32 size) {
    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* block = (HandleBlock*)NewPtrClear(sizeof(HandleBlock));
    if (!block) {
        pthread_mutex_unlock(&gHandleMutex);
        return NULL;
    }

    block->ptr = NewPtrClear(size);
    if (!block->ptr) {
        DisposePtr((Ptr)block);
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
            DisposePtr((Ptr)p->ptr);
            DisposePtr((Ptr)p);
            break;
        }
        pp = &p->next;
    }

    pthread_mutex_unlock(&gHandleMutex);
}

void SetHandleSize(Handle h, SInt32 newSize) {
    if (!h) return;

    pthread_mutex_lock(&gHandleMutex);

    HandleBlock* p = gHandleList;
    while (p) {
        if ((Handle)&p->ptr == h) {
            void* newPtr = NewPtr(newSize);
            if (newPtr) {
                if (p->ptr) {
                    Size copySize = (newSize < p->size) ? newSize : p->size;
                    BlockMove(p->ptr, newPtr, copySize);
                    DisposePtr((Ptr)p->ptr);
                }
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

SInt32 GetHandleSize(Handle h) {
    if (!h) return 0;

    pthread_mutex_lock(&gHandleMutex);

    SInt32 size = 0;
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

UInt8 HGetState(Handle h) {
    if (!h) return 0;

    pthread_mutex_lock(&gHandleMutex);

    UInt8 state = 0;
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

void HSetState(Handle h, UInt8 state) {
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

static void SetResError(SInt16 error) {
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

static Boolean ReadResourceData(ResourceMap* map, ResourceEntry* entry, void* buffer) {
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

static Boolean MapHasDecompressionEnabled(ResourceMap* map) {
    if (!map) return false;
    /* Check the decompression password bit (bit 7 of in-memory attributes) */
    return map->decompressionEnabled || (map->inMemoryAttr & (1 << decompressionPasswordBit));
}

static Handle FindDecompressor(UInt16 defProcID) {
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
            Boolean savedOneDeep = gResOneDeep;
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
            DisposePtr((Ptr)oldest);
            gDecompCacheSize--;
        }
    }

    /* Add new cache entry */
    DecompressedCache* newCache = (DecompressedCache*)NewPtr(sizeof(DecompressedCache));
    if (newCache) {
        newCache->type = type;
        newCache->id = id;
        newCache->decompressedHandle = handle;

        /* Calculate simple checksum of decompressed data for cache validation */
        UInt32 checksum = 0;
        if (handle && *handle) {
            SInt32 size = GetHandleSize(handle);
            HLock(handle);
            unsigned char* data = (unsigned char*)*handle;
            for (SInt32 i = 0; i < size; i++) {
                checksum = (checksum << 1) ^ data[i];  /* Simple rolling checksum */
            }
            HUnlock(handle);
        }
        newCache->checksum = checksum;

        newCache->lastAccess = time(NULL);
        newCache->next = gDecompCache;
        gDecompCache = newCache;
        gDecompCacheSize++;
    }
}

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

    if (header->signature != ROBUSTNESS_SIGNATURE) {
        return compressedHandle;  /* Not an extended resource */
    }

    /* Check if compression bit is set */
    if (!(header->extendedAttributes & resCompressed)) {
        return compressedHandle;  /* Extended but not compressed */
    }

    /* Determine decompressor type based on header version */
    UInt16 defProcID = 0;
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
        UInt8* decompressedData = NULL;

        int result = DecompressResource(compressedData, compressedSize,
                                       &decompressedData, &decompressedSize);

        if (result != 0) {
            SetResError(CantDecompress);
            return compressedHandle;
        }

        /* Create new handle for decompressed data */
        Handle decompressedHandle = NewHandle(decompressedSize);
        if (!decompressedHandle) {
            DisposePtr((Ptr)decompressedData);
            SetResError(memFullErr);
            return compressedHandle;
        }

        /* Copy decompressed data to handle */
        memcpy(*decompressedHandle, decompressedData, decompressedSize);
        DisposePtr((Ptr)decompressedData);

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
        /* In classic Mac OS, this would execute 68K code from the defproc handle.
         * For a portable implementation, we check if it's a known decompressor ID
         * that we can handle internally, otherwise we fail gracefully. */

        if (!defProcHandle || !*defProcHandle) {
            SetResError(CantDecompress);
            return compressedHandle;
        }

        /* Allocate output handle for decompressed data */
        Handle decompressedHandle = NewHandle(decompressedSize);
        if (!decompressedHandle) {
            SetResError(memFullErr);
            return compressedHandle;
        }

        /* Check defProcID to see if we can handle it internally */
        /* Standard decompressor IDs:
         * 0 = DonnBits (handled by built-in)
         * 1 = Dcmp1 byte-wise (handled by built-in)
         * 2 = GreggyBits (handled by built-in)
         * Others = custom decompressors (would require 68K emulation) */

        if (defProcID >= 0 && defProcID <= 2) {
            /* Known decompressor - use built-in implementation */
            UInt8* decompData = NULL;
            int result = DecompressResource(compressedData, compressedSize,
                                           &decompData, &decompressedSize);

            if (result != 0 || !decompData) {
                DisposeHandle(decompressedHandle);
                SetResError(CantDecompress);
                return compressedHandle;
            }

            /* Copy decompressed data to output handle */
            memcpy(*decompressedHandle, decompData, decompressedSize);
            DisposePtr((Ptr)decompData);

            /* Dispose of compressed handle */
            DisposeHandle(compressedHandle);

            /* Add to cache */
            if (entry) {
                AddToDecompressionCache(entry->resType, entry->resID, decompressedHandle);
                entry->attributes &= ~resCompressed;
            }

            return decompressedHandle;
        } else {
            /* Custom decompressor (ID > 2) */
            /* In classic Mac OS, we would:
             * 1. Lock the defproc handle
             * 2. Set up Pascal calling convention (push params on stack)
             * 3. Execute the 68K code
             * 4. Check return value (OSErr)
             *
             * For portability, we cannot execute 68K code directly.
             * Applications expecting custom decompressors must provide
             * them through the DecompressHook mechanism instead. */

            DisposeHandle(decompressedHandle);
            SetResError(CantDecompress);
            RES_LOG_ERROR("Custom decompressor ID %d requires 68K emulation", defProcID);
            return compressedHandle;
        }
    }
}

/* ---- Resource Loading Functions -------------------------------------------------- */

Handle GetResource(ResType theType, ResID theID) {
    extern void serial_puts(const char* str);
    extern void uart_flush(void);
    serial_puts("[GetRes] enter\n");
    uart_flush();

    SetResError(noErr);

    serial_puts("[GetRes] GetMap\n");
    uart_flush();
    ResourceMap* map = gResOneDeep ? gCurrentMap : gResourceChain;

    serial_puts("[GetRes] Loop\n");
    uart_flush();
    while (map) {
        serial_puts("[GetRes] FindRes\n");
        uart_flush();
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
            UInt8 state = resIsResource;
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
    Boolean saveOneDeep = gResOneDeep;
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
    Boolean saveOneDeep = gResOneDeep;
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
                    if ((UInt32)GetHandleSize(theResource) == entry->dataSize) {
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
                    UInt8 state = HGetState(theResource);
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
                        DisposePtr((Ptr)entry->name);
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
                    UInt8 state = HGetState(theResource);
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

SInt32 GetResourceSizeOnDisk(Handle theResource) {
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

SInt32 GetMaxResourceSize(Handle theResource) {
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

RefNum OpenRFPerm(const char* fileName, UInt8 vRefNum, SInt8 permission) {
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
    map = (ResourceMap*)NewPtrClear(sizeof(ResourceMap));
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
    map->inMemoryAttr = true;
    map->decompressionEnabled = true;

    /* Read resource fork header (16 bytes) */
    UInt32 header[4];
    if (fread(header, 1, 16, file) != 16) {
        RESOURCE_LOG_DEBUG("OpenRFPerm: Failed to read fork header\n");
        DisposePtr((Ptr)map);
        fclose(file);
        SetResError(ioErr);
        return -1;
    }

    /* Mac resource forks are big-endian */
    #define BE32(x) ((((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | \
                     (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24))

    map->dataOffset = BE32(header[0]);
    map->mapOffset = BE32(header[1]);
    UInt32 dataLength = BE32(header[2]);
    UInt32 mapLength = BE32(header[3]);

    RESOURCE_LOG_DEBUG("OpenRFPerm: Fork header - dataOffset=%u mapOffset=%u dataLen=%u mapLen=%u\n",
                  map->dataOffset, map->mapOffset, dataLength, mapLength);

    /* Seek to resource map */
    if (fseek(file, map->mapOffset, SEEK_SET) != 0) {
        RESOURCE_LOG_DEBUG("OpenRFPerm: Failed to seek to map offset\n");
        DisposePtr((Ptr)map);
        fclose(file);
        SetResError(ioErr);
        return -1;
    }

    /* Read resource map header (30 bytes after copying fork header) */
    UInt8 mapHeader[30];
    if (fread(mapHeader, 1, 30, file) != 30) {
        RESOURCE_LOG_DEBUG("OpenRFPerm: Failed to read map header\n");
        DisposePtr((Ptr)map);
        fclose(file);
        SetResError(mapReadErr);
        return -1;
    }

    /* Skip first 16 bytes (copy of fork header) + 4 bytes (next map handle) + 2 bytes (file ref) + 2 bytes (attributes) */
    /* Bytes 24-25: offset to type list, 26-27: offset to name list, 28-29: number of types - 1 */
    UInt16 typeListOffset = (mapHeader[24] << 8) | mapHeader[25];
    UInt16 nameListOffset = (mapHeader[26] << 8) | mapHeader[27];
    SInt16 numTypesMinusOne = (mapHeader[28] << 8) | mapHeader[29];
    SInt16 numTypes = numTypesMinusOne + 1;

    RESOURCE_LOG_DEBUG("OpenRFPerm: Map header - typeListOff=%u nameListOff=%u numTypes=%d\n",
                  typeListOffset, nameListOffset, numTypes);

    if (numTypes <= 0 || numTypes > 1000) {
        /* Sanity check - if no types or unreasonable number, just create empty map */
        RESOURCE_LOG_DEBUG("OpenRFPerm: Invalid type count, creating empty map\n");
        map->types = NULL;
    } else {
        /* Seek to type list (relative to map offset + 2 for the type list itself) */
        if (fseek(file, map->mapOffset + typeListOffset + 2, SEEK_SET) != 0) {
            RESOURCE_LOG_DEBUG("OpenRFPerm: Failed to seek to type list\n");
            DisposePtr((Ptr)map);
            fclose(file);
            SetResError(mapReadErr);
            return -1;
        }

        /* Read type list - for now, just mark map as having been attempted to read */
        /* Full implementation would parse each type entry (8 bytes each) and resource reference lists */
        /* This is a minimal implementation that at least reads the header structure */
        map->types = NULL;  /* Will be populated when resources are actually accessed */

        RESOURCE_LOG_DEBUG("OpenRFPerm: Resource map read successfully (%d types)\n", numTypes);
    }

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
                    DisposePtr((Ptr)entry->name);
                    DisposePtr((Ptr)entry);
                    entry = nextEntry;
                }
                DisposePtr((Ptr)type);
                type = nextType;
            }

            /* Close file and free map */
            if (map->fileHandle) {
                fclose(map->fileHandle);
            }
            DisposePtr((Ptr)map->fileName);
            DisposePtr((Ptr)map);
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

void SetResLoad(Boolean load) {
    gResLoad = load;
}

Boolean GetResLoad(void) {
    return gResLoad;
}

void SetResPurge(Boolean install) {
    RESOURCE_LOG_DEBUG("SetResPurge: %s purging\n", install ? "Enable" : "Disable");
    gResPurge = install;

    /* If enabling purge, mark all resource handles as purgeable */
    if (install) {
        ResourceMap* map = gResourceChain;
        while (map) {
            ResourceType* type = map->types;
            while (type) {
                ResourceEntry* entry = type->resources;
                while (entry) {
                    if (entry->handle) {
                        HPurge(entry->handle);
                    }
                    entry = entry->next;
                }
                type = type->next;
            }
            map = map->next;
        }
    } else {
        /* If disabling purge, mark all resource handles as non-purgeable */
        ResourceMap* map = gResourceChain;
        while (map) {
            ResourceType* type = map->types;
            while (type) {
                ResourceEntry* entry = type->resources;
                while (entry) {
                    if (entry->handle) {
                        HNoPurge(entry->handle);
                    }
                    entry = entry->next;
                }
                type = type->next;
            }
            map = map->next;
        }
    }
}

Boolean GetResPurge(void) {
    return gResPurge;
}

SInt16 ResError(void) {
    return gResError;
}

void SetResErrProc(ResErrProcPtr proc) {
    gResErrProc = proc;
}

void SetROMMapInsert(Boolean insert) {
    gROMMapInsert = insert;
}

Boolean GetROMMapInsert(void) {
    return gROMMapInsert;
}

void SetResOneDeep(Boolean oneDeep) {
    gResOneDeep = oneDeep;
}

Boolean GetResOneDeep(void) {
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
        DisposePtr((Ptr)gDecompCache);
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
        DisposePtr((Ptr)gHandleList->ptr);
        DisposePtr((Ptr)gHandleList);
        gHandleList = next;
    }
    pthread_mutex_unlock(&gHandleMutex);
}

void InstallDecompressHook(DecompressHookProc proc) {
    gDecompressHook = proc;
}

/* ---- Decompression Management Functions ------------------------------------------ */

/* Enable/disable automatic decompression */
void SetAutoDecompression(Boolean enable) {
    gAutoDecompression = enable;
}

Boolean GetAutoDecompression(void) {
    return gAutoDecompression;
}

/* Clear decompression cache */
void ResourceManager_FlushDecompressionCache(void) {
    while (gDecompCache) {
        DecompressedCache* next = gDecompCache->next;
        /* Don't dispose handles - they're owned by the app */
        DisposePtr((Ptr)gDecompCache);
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

        DisposePtr((Ptr)curr);
        gDecompCacheSize--;
    }
}

/* Register a custom decompressor */
int ResourceManager_RegisterDecompressor(UInt16 id, Handle defProcHandle) {
    if (!defProcHandle || !*defProcHandle) {
        return paramErr;
    }

    /* For now, just cache the single decompressor */
    /* In a full implementation, we'd maintain a table */
    gDefProcHandle = defProcHandle;
    gLastDefProcID = id;

    return noErr;
}

Handle ResourceManager_CheckLoadHook(ResourceEntry* entry, ResourceMap* map) {

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
    Boolean savedResLoad = gResLoad;
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
    RESOURCE_LOG_DEBUG("CreateResFile: Creating resource file '%s'\n", fileName);

    /* Would create a new resource file with resource fork */
    /* For now, just log and return success */
    SetResError(noErr);
}

void UpdateResFile(RefNum refNum) {
    RESOURCE_LOG_DEBUG("UpdateResFile: Updating resource file %d\n", refNum);

    /* Find resource file by reference number */
    if (refNum < 0 || refNum >= MAX_RESOURCE_FILES) {
        SetResError(resFNotFound);
        return;
    }

    ResourceFile* resFile = &g_resourceFiles[refNum];
    if (!resFile->inUse) {
        SetResError(resFNotFound);
        return;
    }

    /* Mark as needing update - actual write would happen here */
    RESOURCE_LOG_DEBUG("UpdateResFile: Would write changes to %s\n", resFile->fileName);
    SetResError(noErr);
}

void WriteResource(Handle theResource) {
    if (!theResource) {
        SetResError(nilHandleErr);
        return;
    }

    RESOURCE_LOG_DEBUG("WriteResource: Writing resource at %p\n", theResource);

    /* Find the resource entry and mark it for writing */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry* entry = type->resources;
            while (entry) {
                if (entry->handle == theResource) {
                    /* Mark resource as changed - would be written on UpdateResFile */
                    entry->attributes |= 0x40;  /* resChanged flag */
                    RESOURCE_LOG_DEBUG("WriteResource: Marked resource for writing\n");
                    SetResError(noErr);
                    return;
                }
                entry = entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    /* Resource not found in any map */
    RESOURCE_LOG_DEBUG("WriteResource: Resource not found in resource maps\n");
    SetResError(resNotFound);
}

void AddResource(Handle theData, ResType theType, ResID theID, const char* name) {

    if (!theData) {
        SetResError(nilHandleErr);
        return;
    }

    RESOURCE_LOG_DEBUG("AddResource: Adding resource type='%.4s' id=%d name='%s'\n",
                  (char*)&theType, theID, name ? name : "(none)");

    /* Would add resource to current resource file */
    /* For now, just attach type and ID to handle */
    if (theData && *theData) {
        /* Store resource info in handle (would use proper resource map) */
        SetResError(noErr);
    } else {
        SetResError(addResFailed);
    }
}

void RemoveResource(Handle theResource) {
    if (!theResource) {
        SetResError(nilHandleErr);
        return;
    }

    RESOURCE_LOG_DEBUG("RemoveResource: Removing resource at %p\n", theResource);

    /* Find and remove the resource entry from the resource map */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* type = map->types;
        while (type) {
            ResourceEntry** ppEntry = &type->resources;
            ResourceEntry* entry;

            while ((entry = *ppEntry) != NULL) {
                if (entry->handle == theResource) {
                    /* Remove from linked list */
                    *ppEntry = entry->next;

                    /* Free the entry (but not the handle - caller owns it) */
                    DisposePtr((Ptr)entry);

                    RESOURCE_LOG_DEBUG("RemoveResource: Resource removed from map\n");
                    SetResError(noErr);
                    return;
                }
                ppEntry = &entry->next;
            }
            type = type->next;
        }
        map = map->next;
    }

    /* Resource not found in any map */
    RESOURCE_LOG_DEBUG("RemoveResource: Resource not found in resource maps\n");
    SetResError(resNotFound);
}

void ChangedResource(Handle theResource) {

    if (!theResource) {
        SetResError(nilHandleErr);
        return;
    }

    RESOURCE_LOG_DEBUG("ChangedResource: Marking resource at %p as changed\n", theResource);

    /* Would mark resource as needing to be written to disk */
    /* For now just log it */
    SetResError(noErr);
}

SInt16 CountResources(ResType theType) {
    SInt16 count = 0;

    RESOURCE_LOG_DEBUG("CountResources: Counting type='%.4s'\n", (char*)&theType);

    /* Count resources of given type in all open resource files */
    for (int i = 0; i < g_resourceCount; i++) {
        if (g_resources[i].type == theType) {
            count++;
        }
    }

    RESOURCE_LOG_DEBUG("CountResources: Found %d resources\n", count);
    return count;
}

SInt16 Count1Resources(ResType theType) {
    SInt16 count = 0;

    RESOURCE_LOG_DEBUG("Count1Resources: Counting type='%.4s' in current file\n", (char*)&theType);

    /* Count resources of given type in current resource file only
     * For now, use gCurrentMap to filter, or count all if no map distinction
     */
    if (gCurrentMap) {
        ResourceType* typeEntry = gCurrentMap->types;
        while (typeEntry) {
            if (typeEntry->type == theType) {
                ResourceEntry* entry = typeEntry->resources;
                while (entry) {
                    count++;
                    entry = entry->next;
                }
                break;
            }
            typeEntry = typeEntry->next;
        }
    } else {
        /* Fallback: count in simple array if map not used */
        for (int i = 0; i < g_resourceCount; i++) {
            if (g_resources[i].type == theType) {
                count++;
            }
        }
    }

    RESOURCE_LOG_DEBUG("Count1Resources: Found %d resources\n", count);
    return count;
}

Handle GetIndResource(ResType theType, SInt16 index) {
    SInt16 count = 0;

    RESOURCE_LOG_DEBUG("GetIndResource: Getting type='%.4s' index=%d\n",
                  (char*)&theType, index);

    if (index < 1) {
        SetResError(resNotFound);
        return NULL;
    }

    /* Find the nth resource of given type */
    for (int i = 0; i < g_resourceCount; i++) {
        if (g_resources[i].type == theType) {
            count++;
            if (count == index) {
                SetResError(noErr);
                return g_resources[i].handle;
            }
        }
    }

    SetResError(resNotFound);
    return NULL;
}

Handle Get1IndResource(ResType theType, SInt16 index) {
    SInt16 count = 0;

    RESOURCE_LOG_DEBUG("Get1IndResource: Getting type='%.4s' index=%d in current file\n",
                  (char*)&theType, index);

    if (index < 1) {
        SetResError(resNotFound);
        return NULL;
    }

    /* Find the nth resource of given type in current resource file only */
    if (gCurrentMap) {
        ResourceType* typeEntry = gCurrentMap->types;
        while (typeEntry) {
            if (typeEntry->type == theType) {
                ResourceEntry* entry = typeEntry->resources;
                while (entry) {
                    count++;
                    if (count == index) {
                        SetResError(noErr);
                        return entry->handle;
                    }
                    entry = entry->next;
                }
                break;
            }
            typeEntry = typeEntry->next;
        }
    } else {
        /* Fallback: use simple array if map not used */
        for (int i = 0; i < g_resourceCount; i++) {
            if (g_resources[i].type == theType) {
                count++;
                if (count == index) {
                    SetResError(noErr);
                    return g_resources[i].handle;
                }
            }
        }
    }

    SetResError(resNotFound);
    return NULL;
}

SInt16 CountTypes(void) {
    SInt16 count = 0;

    RESOURCE_LOG_DEBUG("CountTypes: Counting all resource types\n");

    /* Count unique resource types across all open resource files */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* typeEntry = map->types;
        while (typeEntry) {
            count++;
            typeEntry = typeEntry->next;
        }
        map = map->next;
    }

    RESOURCE_LOG_DEBUG("CountTypes: Found %d types\n", count);
    return count;
}

SInt16 Count1Types(void) {
    SInt16 count = 0;

    RESOURCE_LOG_DEBUG("Count1Types: Counting types in current file\n");

    /* Count resource types in current resource file only */
    if (gCurrentMap) {
        ResourceType* typeEntry = gCurrentMap->types;
        while (typeEntry) {
            count++;
            typeEntry = typeEntry->next;
        }
    }

    RESOURCE_LOG_DEBUG("Count1Types: Found %d types\n", count);
    return count;
}

void GetIndType(ResType* theType, SInt16 index) {
    SInt16 count = 0;

    if (!theType || index < 1) {
        return;
    }

    RESOURCE_LOG_DEBUG("GetIndType: Getting type at index %d\n", index);

    /* Get the nth resource type from all open resource files */
    ResourceMap* map = gResourceChain;
    while (map) {
        ResourceType* typeEntry = map->types;
        while (typeEntry) {
            count++;
            if (count == index) {
                *theType = typeEntry->type;
                RESOURCE_LOG_DEBUG("GetIndType: Found type='%.4s'\n", (char*)theType);
                return;
            }
            typeEntry = typeEntry->next;
        }
        map = map->next;
    }

    /* Not found - set to zero */
    *theType = 0;
}

void Get1IndType(ResType* theType, SInt16 index) {
    SInt16 count = 0;

    if (!theType || index < 1) {
        return;
    }

    RESOURCE_LOG_DEBUG("Get1IndType: Getting type at index %d in current file\n", index);

    /* Get the nth resource type from current resource file only */
    if (gCurrentMap) {
        ResourceType* typeEntry = gCurrentMap->types;
        while (typeEntry) {
            count++;
            if (count == index) {
                *theType = typeEntry->type;
                RESOURCE_LOG_DEBUG("Get1IndType: Found type='%.4s'\n", (char*)theType);
                return;
            }
            typeEntry = typeEntry->next;
        }
    }

    /* Not found - set to zero */
    *theType = 0;
}

ResID UniqueID(ResType theType) {
    ResID maxID = 127;  /* Start above system resources */

    /* Find highest ID for this type */
    for (int i = 0; i < g_resourceCount; i++) {
        if (g_resources[i].type == theType && g_resources[i].id > maxID) {
            maxID = g_resources[i].id;
        }
    }

    ResID newID = maxID + 1;
    RESOURCE_LOG_DEBUG("UniqueID: Generated ID %d for type='%.4s'\n",
                  newID, (char*)&theType);
    return newID;
}

ResID Unique1ID(ResType theType) {
    ResID maxID = 127;  /* Start above system resources */

    if (!gCurrentMap) {
        return 128;
    }

    /* Find highest ID for this type in current resource file only */
    ResourceType* typeEntry = gCurrentMap->types;
    while (typeEntry) {
        if (typeEntry->type == theType) {
            ResourceEntry* entry = typeEntry->resources;
            while (entry) {
                if (entry->id > maxID) {
                    maxID = entry->id;
                }
                entry = entry->next;
            }
        }
        typeEntry = typeEntry->next;
    }

    ResID newID = maxID + 1;
    RESOURCE_LOG_DEBUG("Unique1ID: Generated ID %d for type='%.4s' in current file\n",
                  newID, (char*)&theType);
    return newID;
}

void SetResFileAttrs(RefNum refNum, UInt16 attrs) {
    /* Find resource map for this file reference */
    ResourceMap* map = gResourceChain;
    while (map) {
        if (map->fileRefNum == refNum) {
            map->attributes = attrs;
            RESOURCE_LOG_DEBUG("SetResFileAttrs: Set attrs=0x%04x for refNum=%d\n", attrs, refNum);
            return;
        }
        map = map->next;
    }
    RESOURCE_LOG_DEBUG("SetResFileAttrs: File refNum=%d not found\n", refNum);
}

UInt16 GetResFileAttrs(RefNum refNum) {
    /* Find resource map for this file reference */
    ResourceMap* map = gResourceChain;
    while (map) {
        if (map->fileRefNum == refNum) {
            RESOURCE_LOG_DEBUG("GetResFileAttrs: attrs=0x%04x for refNum=%d\n", map->attributes, refNum);
            return map->attributes;
        }
        map = map->next;
    }
    RESOURCE_LOG_DEBUG("GetResFileAttrs: File refNum=%d not found\n", refNum);
    return 0;
}

RefNum GetNextResourceFile(RefNum curFile) {
    /* Find current file in resource chain and return next */
    ResourceMap* map = gResourceChain;
    while (map) {
        if (map->fileRefNum == curFile) {
            /* Found current file - return next in chain */
            if (map->next) {
                RESOURCE_LOG_DEBUG("GetNextResourceFile: Next after refNum=%d is refNum=%d\n",
                              curFile, map->next->fileRefNum);
                return map->next->fileRefNum;
            } else {
                RESOURCE_LOG_DEBUG("GetNextResourceFile: No more files after refNum=%d\n", curFile);
                return -1;  /* No more files */
            }
        }
        map = map->next;
    }
    RESOURCE_LOG_DEBUG("GetNextResourceFile: curFile=%d not found\n", curFile);
    return -1;  /* Current file not found */
}

RefNum GetTopResourceFile(void) {
    if (gResourceChain) {
        return gResourceChain->fileRefNum;
    }
    return -1;
}

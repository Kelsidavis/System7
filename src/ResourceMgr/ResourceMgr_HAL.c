/*
 * ResourceMgr_HAL.c - Hardware Abstraction Layer for Resource Manager
 *
 * Connects the Resource Manager with Memory Manager and provides
 * platform-specific resource file access for x86_64 and ARM64.
 */

#include "resource_manager.h"
#include "resource_types.h"
#include "../MemoryMgr/MemoryMgr_HAL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #define HAS_MACOS_RESOURCES 1
#endif

/* Resource Manager global state */
typedef struct ResourceFile {
    int16_t             refNum;        /* File reference number */
    char                fileName[256]; /* File path */
    int                 fileDesc;      /* Unix file descriptor */
    ResourceMapPtr      resMap;        /* Resource map in memory */
    Handle              resData;       /* Resource data handle */
    struct ResourceFile* next;         /* Next in chain */
} ResourceFile;

static ResourceFile* gOpenResFiles = NULL;
static int16_t gCurResFile = -1;
static int16_t gNextRefNum = 1;
static pthread_mutex_t gResLock = PTHREAD_MUTEX_INITIALIZER;

/* Resource cache for performance */
typedef struct ResourceCache {
    ResType     type;
    ResID       id;
    Handle      handle;
    int16_t     refNum;
    uint32_t    accessCount;
    struct ResourceCache* next;
} ResourceCache;

static ResourceCache* gResCache = NULL;
static const size_t kMaxCacheEntries = 256;
static size_t gCacheCount = 0;

/* Initialize Resource Manager HAL */
OSErr ResourceMgr_HAL_Initialize(void)
{
    pthread_mutex_lock(&gResLock);

    /* Initialize resource file list */
    gOpenResFiles = NULL;
    gCurResFile = -1;
    gNextRefNum = 1;

    /* Initialize resource cache */
    gResCache = NULL;
    gCacheCount = 0;

    /* Initialize the core Resource Manager */
    OSErr err = ResourceManagerInit();

    pthread_mutex_unlock(&gResLock);
    return err;
}

/* Open a resource file */
int16_t ResourceMgr_HAL_OpenResFile(const char* filePath, uint8_t permission)
{
    pthread_mutex_lock(&gResLock);

    /* Check if file already open */
    ResourceFile* rf = gOpenResFiles;
    while (rf) {
        if (strcmp(rf->fileName, filePath) == 0) {
            pthread_mutex_unlock(&gResLock);
            return rf->refNum;  /* Already open */
        }
        rf = rf->next;
    }

    /* Open the file */
    int flags = (permission == fsRdPerm) ? O_RDONLY : O_RDWR;
    int fd = open(filePath, flags);
    if (fd < 0) {
        pthread_mutex_unlock(&gResLock);
        return -1;
    }

    /* Get file size */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        pthread_mutex_unlock(&gResLock);
        return -1;
    }

    /* Allocate ResourceFile structure */
    rf = (ResourceFile*)calloc(1, sizeof(ResourceFile));
    if (!rf) {
        close(fd);
        pthread_mutex_unlock(&gResLock);
        return -1;
    }

    /* Initialize ResourceFile */
    rf->refNum = gNextRefNum++;
    strncpy(rf->fileName, filePath, sizeof(rf->fileName) - 1);
    rf->fileDesc = fd;

    /* Allocate memory for resource map using Memory Manager */
    rf->resMap = (ResourceMapPtr)NewPtr(sizeof(ResourceMap));
    if (!rf->resMap) {
        free(rf);
        close(fd);
        pthread_mutex_unlock(&gResLock);
        return -1;
    }

    /* Read resource fork header */
    ResourceHeader header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
        /* Not a valid resource file - create empty map */
        memset(rf->resMap, 0, sizeof(ResourceMap));
        rf->resMap->mapDataOffset = sizeof(ResourceHeader);
        rf->resMap->mapLength = sizeof(ResourceMap);
        rf->resMap->typeCount = 0;
    } else {
        /* Read resource map */
        lseek(fd, header.mapOffset, SEEK_SET);
        read(fd, rf->resMap, sizeof(ResourceMap));

        /* Allocate handle for resource data */
        rf->resData = NewHandle(header.dataLength);
        if (rf->resData) {
            /* Read resource data */
            lseek(fd, header.dataOffset, SEEK_SET);
            HLock(rf->resData);
            read(fd, *rf->resData, header.dataLength);
            HUnlock(rf->resData);
        }
    }

    /* Add to open files list */
    rf->next = gOpenResFiles;
    gOpenResFiles = rf;
    gCurResFile = rf->refNum;

    pthread_mutex_unlock(&gResLock);
    return rf->refNum;
}

/* Close a resource file */
void ResourceMgr_HAL_CloseResFile(int16_t refNum)
{
    pthread_mutex_lock(&gResLock);

    ResourceFile* rf = gOpenResFiles;
    ResourceFile* prev = NULL;

    while (rf) {
        if (rf->refNum == refNum) {
            /* Remove from list */
            if (prev) {
                prev->next = rf->next;
            } else {
                gOpenResFiles = rf->next;
            }

            /* Clean up resources */
            if (rf->resData) {
                DisposeHandle(rf->resData);
            }
            if (rf->resMap) {
                DisposePtr((Ptr)rf->resMap);
            }

            /* Close file */
            close(rf->fileDesc);

            /* Update current file if needed */
            if (gCurResFile == refNum) {
                gCurResFile = gOpenResFiles ? gOpenResFiles->refNum : -1;
            }

            /* Free structure */
            free(rf);
            break;
        }
        prev = rf;
        rf = rf->next;
    }

    pthread_mutex_unlock(&gResLock);
}

/* Get a resource - uses Memory Manager for allocation */
Handle ResourceMgr_HAL_GetResource(ResType theType, ResID theID)
{
    pthread_mutex_lock(&gResLock);

    /* Check cache first */
    ResourceCache* cache = gResCache;
    while (cache) {
        if (cache->type == theType && cache->id == theID) {
            cache->accessCount++;
            pthread_mutex_unlock(&gResLock);
            return cache->handle;
        }
        cache = cache->next;
    }

    /* Search open resource files */
    ResourceFile* rf = gOpenResFiles;
    while (rf) {
        if (rf->resMap && rf->resMap->typeCount > 0) {
            /* Search resource map for type and ID */
            /* This is simplified - real implementation would parse the map */

            /* For now, create a dummy resource */
            Handle h = NewHandle(256);  /* Using Memory Manager! */
            if (h) {
                /* Add to cache */
                if (gCacheCount < kMaxCacheEntries) {
                    cache = (ResourceCache*)malloc(sizeof(ResourceCache));
                    if (cache) {
                        cache->type = theType;
                        cache->id = theID;
                        cache->handle = h;
                        cache->refNum = rf->refNum;
                        cache->accessCount = 1;
                        cache->next = gResCache;
                        gResCache = cache;
                        gCacheCount++;
                    }
                }

                pthread_mutex_unlock(&gResLock);
                return h;
            }
        }
        rf = rf->next;
    }

    pthread_mutex_unlock(&gResLock);
    return NULL;
}

/* Load a resource */
void ResourceMgr_HAL_LoadResource(Handle theResource)
{
    if (!theResource) return;

    pthread_mutex_lock(&gResLock);

    /* In our implementation, resources are always loaded */
    /* Real implementation would load from disk if purged */

    pthread_mutex_unlock(&gResLock);
}

/* Release a resource */
void ResourceMgr_HAL_ReleaseResource(Handle theResource)
{
    if (!theResource) return;

    pthread_mutex_lock(&gResLock);

    /* Remove from cache */
    ResourceCache* cache = gResCache;
    ResourceCache* prev = NULL;

    while (cache) {
        if (cache->handle == theResource) {
            if (prev) {
                prev->next = cache->next;
            } else {
                gResCache = cache->next;
            }
            gCacheCount--;

            /* Dispose the handle using Memory Manager */
            DisposeHandle(theResource);

            free(cache);
            break;
        }
        prev = cache;
        cache = cache->next;
    }

    pthread_mutex_unlock(&gResLock);
}

/* Add a resource to current file */
OSErr ResourceMgr_HAL_AddResource(Handle theData, ResType theType, ResID theID, ConstStr255Param name)
{
    if (!theData) return resNotFound;

    pthread_mutex_lock(&gResLock);

    /* Find current resource file */
    ResourceFile* rf = gOpenResFiles;
    while (rf && rf->refNum != gCurResFile) {
        rf = rf->next;
    }

    if (!rf) {
        pthread_mutex_unlock(&gResLock);
        return resFNotFound;
    }

    /* Add to resource map */
    /* Simplified - real implementation would update map structure */

    /* Add to cache */
    if (gCacheCount < kMaxCacheEntries) {
        ResourceCache* cache = (ResourceCache*)malloc(sizeof(ResourceCache));
        if (cache) {
            cache->type = theType;
            cache->id = theID;
            cache->handle = theData;
            cache->refNum = rf->refNum;
            cache->accessCount = 0;
            cache->next = gResCache;
            gResCache = cache;
            gCacheCount++;
        }
    }

    pthread_mutex_unlock(&gResLock);
    return noErr;
}

/* Write resource file */
OSErr ResourceMgr_HAL_WriteResource(Handle theResource)
{
    if (!theResource) return resNotFound;

    pthread_mutex_lock(&gResLock);

    /* Find resource in cache */
    ResourceCache* cache = gResCache;
    while (cache && cache->handle != theResource) {
        cache = cache->next;
    }

    if (!cache) {
        pthread_mutex_unlock(&gResLock);
        return resNotFound;
    }

    /* Find resource file */
    ResourceFile* rf = gOpenResFiles;
    while (rf && rf->refNum != cache->refNum) {
        rf = rf->next;
    }

    if (!rf) {
        pthread_mutex_unlock(&gResLock);
        return resFNotFound;
    }

    /* Write to file */
    /* Simplified - real implementation would update file */

    pthread_mutex_unlock(&gResLock);
    return noErr;
}

/* Get resource size */
Size ResourceMgr_HAL_GetResourceSize(Handle theResource)
{
    if (!theResource) return 0;

    /* Use Memory Manager to get handle size */
    return GetHandleSize(theResource);
}

/* Platform-specific resource loading */
#ifdef HAS_MACOS_RESOURCES
Handle ResourceMgr_HAL_GetMacResource(ResType theType, ResID theID)
{
    /* Use CoreFoundation to load Mac resources on macOS */
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) return NULL;

    /* Convert type to string */
    char typeStr[5] = {0};
    typeStr[0] = (theType >> 24) & 0xFF;
    typeStr[1] = (theType >> 16) & 0xFF;
    typeStr[2] = (theType >> 8) & 0xFF;
    typeStr[3] = theType & 0xFF;

    CFStringRef typeRef = CFStringCreateWithCString(NULL, typeStr, kCFStringEncodingMacRoman);
    CFStringRef nameRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), theID);

    CFURLRef url = CFBundleCopyResourceURL(bundle, nameRef, typeRef, NULL);

    CFRelease(typeRef);
    CFRelease(nameRef);

    if (!url) return NULL;

    /* Load resource data */
    CFDataRef data = NULL;
    Boolean success = CFURLCreateDataAndPropertiesFromResource(NULL, url, &data, NULL, NULL, NULL);
    CFRelease(url);

    if (!success || !data) return NULL;

    /* Create handle with resource data */
    CFIndex length = CFDataGetLength(data);
    Handle h = NewHandle(length);
    if (h) {
        HLock(h);
        CFDataGetBytes(data, CFRangeMake(0, length), (UInt8*)*h);
        HUnlock(h);
    }

    CFRelease(data);
    return h;
}
#endif

/* Clear resource cache */
void ResourceMgr_HAL_ClearCache(void)
{
    pthread_mutex_lock(&gResLock);

    ResourceCache* cache = gResCache;
    while (cache) {
        ResourceCache* next = cache->next;
        /* Don't dispose handles - they may still be in use */
        free(cache);
        cache = next;
    }

    gResCache = NULL;
    gCacheCount = 0;

    pthread_mutex_unlock(&gResLock);
}

/* Shutdown Resource Manager HAL */
void ResourceMgr_HAL_Shutdown(void)
{
    pthread_mutex_lock(&gResLock);

    /* Close all open resource files */
    while (gOpenResFiles) {
        ResourceFile* rf = gOpenResFiles;
        gOpenResFiles = rf->next;

        if (rf->resData) {
            DisposeHandle(rf->resData);
        }
        if (rf->resMap) {
            DisposePtr((Ptr)rf->resMap);
        }

        close(rf->fileDesc);
        free(rf);
    }

    /* Clear cache */
    ResourceMgr_HAL_ClearCache();

    pthread_mutex_unlock(&gResLock);
}
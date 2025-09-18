/*
 * File: VoiceResources.c
 *
 * Contains: Voice resource loading and management implementation
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements voice resource management including
 *              loading, caching, and resource format handling.
 */

#include "../include/SpeechManager/VoiceResources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <zlib.h>

/* ===== Voice Resource Manager Implementation ===== */

/* Maximum limits */
#define MAX_RESOURCES 512
#define MAX_CACHE_SIZE (16 * 1024 * 1024)  /* 16MB default cache */
#define MAX_PATH_LENGTH 1024
#define CHECKSUM_SEED 0x12345678

/* Voice resource manager structure */
struct VoiceResourceManager {
    bool initialized;
    VoiceResourceData resources[MAX_RESOURCES];
    long resourceCount;
    long maxCacheSize;
    long currentCacheSize;
    long cacheHits;
    long cacheMisses;

    /* Callbacks */
    VoiceResourceLoadProc loadCallback;
    void *loadUserData;
    VoiceResourceValidateProc validateCallback;
    void *validateUserData;
    VoiceResourceProgressProc progressCallback;
    void *progressUserData;
    VoiceResourceErrorProc errorCallback;
    void *errorUserData;

    /* Threading support */
    pthread_mutex_t managerMutex;
};

/* Global resource manager instance */
static VoiceResourceManager *gDefaultResourceManager = NULL;

/* ===== Internal Functions ===== */

/*
 * CalculateSimpleChecksum
 * Calculates a simple checksum for data validation
 */
static long CalculateSimpleChecksum(const void *data, long dataSize) {
    if (!data || dataSize <= 0) {
        return 0;
    }

    const unsigned char *bytes = (const unsigned char *)data;
    long checksum = CHECKSUM_SEED;

    for (long i = 0; i < dataSize; i++) {
        checksum = ((checksum << 1) | (checksum >> 31)) ^ bytes[i];
    }

    return checksum;
}

/*
 * DetectResourceFormat
 * Detects voice resource format from data
 */
static VoiceResourceFormat DetectResourceFormat(const void *data, long dataSize) {
    if (!data || dataSize < 16) {
        return kVoiceFormat_Unknown;
    }

    const unsigned char *bytes = (const unsigned char *)data;

    /* Check for classic Mac OS voice format */
    if (dataSize >= 4 && memcmp(bytes, "voic", 4) == 0) {
        return kVoiceFormat_Classic;
    }

    /* Check for other formats based on magic numbers or headers */
    if (dataSize >= 8 && memcmp(bytes, "RIFF", 4) == 0) {
        /* Could be a SAPI voice in RIFF format */
        return kVoiceFormat_SAPI;
    }

    /* Default to custom format */
    return kVoiceFormat_Custom;
}

/*
 * FindResourceBySpec
 * Finds a resource by voice specification
 */
static VoiceResourceData *FindResourceBySpec(VoiceResourceManager *manager,
                                             const VoiceSpec *voice) {
    if (!manager || !voice) {
        return NULL;
    }

    for (long i = 0; i < manager->resourceCount; i++) {
        VoiceResourceData *resource = &manager->resources[i];
        VoiceSpec *spec = &resource->info.voiceSpec;

        if (spec->creator == voice->creator && spec->id == voice->id) {
            return resource;
        }
    }

    return NULL;
}

/*
 * LoadResourceFromFile
 * Loads resource data from a file
 */
static OSErr LoadResourceFromFile(const char *filePath, long offset,
                                  void **resourceData, long *resourceSize) {
    if (!filePath || !resourceData || !resourceSize) {
        return paramErr;
    }

    FILE *file = fopen(filePath, "rb");
    if (!file) {
        return resNotFound;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, offset, SEEK_SET);

    if (offset >= fileSize) {
        fclose(file);
        return paramErr;
    }

    long dataSize = fileSize - offset;
    void *data = malloc(dataSize);
    if (!data) {
        fclose(file);
        return memFullErr;
    }

    size_t bytesRead = fread(data, 1, dataSize, file);
    fclose(file);

    if (bytesRead != dataSize) {
        free(data);
        return resNotFound;
    }

    *resourceData = data;
    *resourceSize = dataSize;

    return noErr;
}

/*
 * UpdateCacheUsage
 * Updates cache size tracking
 */
static void UpdateCacheUsage(VoiceResourceManager *manager, long sizeChange) {
    manager->currentCacheSize += sizeChange;
    if (manager->currentCacheSize < 0) {
        manager->currentCacheSize = 0;
    }
}

/*
 * NotifyProgress
 * Sends progress notification
 */
static void NotifyProgress(VoiceResourceManager *manager, const VoiceSpec *voice,
                           long bytesLoaded, long totalBytes) {
    if (manager->progressCallback) {
        manager->progressCallback(voice, bytesLoaded, totalBytes,
                                  manager->progressUserData);
    }
}

/*
 * NotifyError
 * Sends error notification
 */
static void NotifyError(VoiceResourceManager *manager, const VoiceSpec *voice,
                        OSErr error, const char *message) {
    if (manager->errorCallback) {
        manager->errorCallback(voice, error, message, manager->errorUserData);
    }
}

/* ===== Public API Implementation ===== */

/*
 * CreateVoiceResourceManager
 * Creates a new voice resource manager
 */
OSErr CreateVoiceResourceManager(VoiceResourceManager **manager) {
    if (!manager) {
        return paramErr;
    }

    *manager = malloc(sizeof(VoiceResourceManager));
    if (!*manager) {
        return memFullErr;
    }

    memset(*manager, 0, sizeof(VoiceResourceManager));

    /* Initialize default values */
    (*manager)->initialized = true;
    (*manager)->maxCacheSize = MAX_CACHE_SIZE;
    (*manager)->currentCacheSize = 0;
    (*manager)->resourceCount = 0;
    (*manager)->cacheHits = 0;
    (*manager)->cacheMisses = 0;

    /* Initialize mutex */
    int result = pthread_mutex_init(&(*manager)->managerMutex, NULL);
    if (result != 0) {
        free(*manager);
        *manager = NULL;
        return memFullErr;
    }

    /* Set as default if none exists */
    if (!gDefaultResourceManager) {
        gDefaultResourceManager = *manager;
    }

    return noErr;
}

/*
 * DisposeVoiceResourceManager
 * Disposes of a voice resource manager
 */
OSErr DisposeVoiceResourceManager(VoiceResourceManager *manager) {
    if (!manager) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    /* Unload all resources */
    for (long i = 0; i < manager->resourceCount; i++) {
        VoiceResourceData *resource = &manager->resources[i];
        if (resource->resourceData) {
            free(resource->resourceData);
        }
        if (resource->decompressedData) {
            free(resource->decompressedData);
        }
    }

    pthread_mutex_unlock(&manager->managerMutex);
    pthread_mutex_destroy(&manager->managerMutex);

    /* Clear global reference if this was the default */
    if (gDefaultResourceManager == manager) {
        gDefaultResourceManager = NULL;
    }

    free(manager);
    return noErr;
}

/*
 * LoadVoiceResource
 * Loads a voice resource into memory
 */
OSErr LoadVoiceResource(VoiceResourceManager *manager, const VoiceSpec *voice,
                        VoiceResourceData **resourceData) {
    if (!manager || !voice || !resourceData) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    /* Check if already loaded */
    VoiceResourceData *resource = FindResourceBySpec(manager, voice);
    if (resource && resource->isLoaded) {
        resource->refCount++;
        resource->lastAccessed = time(NULL);
        *resourceData = resource;
        manager->cacheHits++;
        pthread_mutex_unlock(&manager->managerMutex);
        return noErr;
    }

    manager->cacheMisses++;

    /* If not found, create new resource entry */
    if (!resource) {
        if (manager->resourceCount >= MAX_RESOURCES) {
            pthread_mutex_unlock(&manager->managerMutex);
            return memFullErr;
        }

        resource = &manager->resources[manager->resourceCount];
        memset(resource, 0, sizeof(VoiceResourceData));
        resource->info.voiceSpec = *voice;
        manager->resourceCount++;
    }

    /* Load resource data */
    void *data = NULL;
    long dataSize = 0;

    /* Use custom load callback if available */
    OSErr err = noErr;
    if (manager->loadCallback) {
        err = manager->loadCallback(&resource->info, &data, &dataSize,
                                    manager->loadUserData);
    } else {
        /* Default loading from file system */
        char voiceFileName[256];
        snprintf(voiceFileName, sizeof(voiceFileName), "voice_%08X_%08X.dat",
                 voice->creator, voice->id);

        err = LoadResourceFromFile(voiceFileName, 0, &data, &dataSize);
    }

    if (err != noErr || !data) {
        NotifyError(manager, voice, err, "Failed to load voice resource");
        pthread_mutex_unlock(&manager->managerMutex);
        return err;
    }

    /* Validate resource if callback is available */
    if (manager->validateCallback) {
        bool isValid = manager->validateCallback(&resource->info, data, dataSize,
                                                  manager->validateUserData);
        if (!isValid) {
            free(data);
            NotifyError(manager, voice, paramErr, "Voice resource validation failed");
            pthread_mutex_unlock(&manager->managerMutex);
            return paramErr;
        }
    }

    /* Store resource data */
    resource->resourceData = data;
    resource->resourceSize = dataSize;
    resource->isLoaded = true;
    resource->refCount = 1;
    resource->lastAccessed = time(NULL);

    /* Detect format */
    VoiceResourceFormat format = DetectResourceFormat(data, dataSize);
    resource->info.header.format = format;
    resource->info.header.resourceSize = dataSize;
    resource->info.header.checksum = CalculateSimpleChecksum(data, dataSize);

    /* Update cache usage */
    UpdateCacheUsage(manager, dataSize);

    /* Check if decompression is needed */
    if (format == kVoiceFormat_Custom) {
        /* Assume compressed if format is custom */
        resource->isCompressed = true;
    }

    *resourceData = resource;

    NotifyProgress(manager, voice, dataSize, dataSize);

    pthread_mutex_unlock(&manager->managerMutex);
    return noErr;
}

/*
 * UnloadVoiceResource
 * Unloads a voice resource from memory
 */
OSErr UnloadVoiceResource(VoiceResourceManager *manager, const VoiceSpec *voice) {
    if (!manager || !voice) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    VoiceResourceData *resource = FindResourceBySpec(manager, voice);
    if (!resource || !resource->isLoaded) {
        pthread_mutex_unlock(&manager->managerMutex);
        return voiceNotFound;
    }

    /* Decrease reference count */
    resource->refCount--;

    /* Only unload if no more references */
    if (resource->refCount <= 0) {
        if (resource->resourceData) {
            UpdateCacheUsage(manager, -resource->resourceSize);
            free(resource->resourceData);
            resource->resourceData = NULL;
        }

        if (resource->decompressedData) {
            free(resource->decompressedData);
            resource->decompressedData = NULL;
        }

        resource->isLoaded = false;
        resource->resourceSize = 0;
        resource->decompressedSize = 0;
        resource->refCount = 0;
    }

    pthread_mutex_unlock(&manager->managerMutex);
    return noErr;
}

/*
 * GetVoiceResourceInfo
 * Gets information about a voice resource
 */
OSErr GetVoiceResourceInfo(VoiceResourceManager *manager, const VoiceSpec *voice,
                           VoiceResourceInfo *info) {
    if (!manager || !voice || !info) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    VoiceResourceData *resource = FindResourceBySpec(manager, voice);
    if (!resource) {
        pthread_mutex_unlock(&manager->managerMutex);
        return voiceNotFound;
    }

    *info = resource->info;

    pthread_mutex_unlock(&manager->managerMutex);
    return noErr;
}

/*
 * CountVoiceResources
 * Returns the number of managed voice resources
 */
OSErr CountVoiceResources(VoiceResourceManager *manager, long *resourceCount) {
    if (!manager || !resourceCount) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);
    *resourceCount = manager->resourceCount;
    pthread_mutex_unlock(&manager->managerMutex);

    return noErr;
}

/*
 * GetIndVoiceResource
 * Gets voice resource information by index
 */
OSErr GetIndVoiceResource(VoiceResourceManager *manager, long index,
                          VoiceResourceInfo *info) {
    if (!manager || index < 1 || !info) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    if (index > manager->resourceCount) {
        pthread_mutex_unlock(&manager->managerMutex);
        return paramErr;
    }

    *info = manager->resources[index - 1].info;

    pthread_mutex_unlock(&manager->managerMutex);
    return noErr;
}

/*
 * FlushVoiceResourceCache
 * Flushes all cached voice resources
 */
OSErr FlushVoiceResourceCache(VoiceResourceManager *manager) {
    if (!manager) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    for (long i = 0; i < manager->resourceCount; i++) {
        VoiceResourceData *resource = &manager->resources[i];
        if (resource->isLoaded && resource->refCount == 0) {
            if (resource->resourceData) {
                UpdateCacheUsage(manager, -resource->resourceSize);
                free(resource->resourceData);
                resource->resourceData = NULL;
            }

            if (resource->decompressedData) {
                free(resource->decompressedData);
                resource->decompressedData = NULL;
            }

            resource->isLoaded = false;
            resource->resourceSize = 0;
            resource->decompressedSize = 0;
        }
    }

    pthread_mutex_unlock(&manager->managerMutex);
    return noErr;
}

/*
 * GetVoiceResourceCacheStats
 * Gets cache statistics
 */
OSErr GetVoiceResourceCacheStats(VoiceResourceManager *manager, long *totalSize,
                                 long *usedSize, long *hitCount, long *missCount) {
    if (!manager) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);

    if (totalSize) *totalSize = manager->maxCacheSize;
    if (usedSize) *usedSize = manager->currentCacheSize;
    if (hitCount) *hitCount = manager->cacheHits;
    if (missCount) *missCount = manager->cacheMisses;

    pthread_mutex_unlock(&manager->managerMutex);
    return noErr;
}

/*
 * DetectVoiceResourceFormat
 * Detects the format of voice resource data
 */
OSErr DetectVoiceResourceFormat(const void *resourceData, long resourceSize,
                                VoiceResourceFormat *format) {
    if (!resourceData || resourceSize <= 0 || !format) {
        return paramErr;
    }

    *format = DetectResourceFormat(resourceData, resourceSize);
    return noErr;
}

/*
 * CalculateVoiceResourceChecksum
 * Calculates checksum for voice resource data
 */
OSErr CalculateVoiceResourceChecksum(const void *resourceData, long resourceSize,
                                     long *checksum) {
    if (!resourceData || resourceSize <= 0 || !checksum) {
        return paramErr;
    }

    *checksum = CalculateSimpleChecksum(resourceData, resourceSize);
    return noErr;
}

/*
 * ValidateVoiceResourceChecksum
 * Validates checksum for voice resource data
 */
OSErr ValidateVoiceResourceChecksum(const void *resourceData, long resourceSize,
                                    long expectedChecksum, bool *isValid) {
    if (!resourceData || resourceSize <= 0 || !isValid) {
        return paramErr;
    }

    long calculatedChecksum = CalculateSimpleChecksum(resourceData, resourceSize);
    *isValid = (calculatedChecksum == expectedChecksum);

    return noErr;
}

/*
 * CompressVoiceResource
 * Compresses voice resource data
 */
OSErr CompressVoiceResource(const void *sourceData, long sourceSize,
                            void **compressedData, long *compressedSize) {
    if (!sourceData || sourceSize <= 0 || !compressedData || !compressedSize) {
        return paramErr;
    }

    /* Use zlib for compression */
    uLongf destLen = compressBound(sourceSize);
    Bytef *dest = malloc(destLen);
    if (!dest) {
        return memFullErr;
    }

    int result = compress(dest, &destLen, (const Bytef *)sourceData, sourceSize);
    if (result != Z_OK) {
        free(dest);
        return paramErr;
    }

    *compressedData = dest;
    *compressedSize = destLen;

    return noErr;
}

/*
 * DecompressVoiceResource
 * Decompresses voice resource data
 */
OSErr DecompressVoiceResource(const void *compressedData, long compressedSize,
                              void **decompressedData, long *decompressedSize) {
    if (!compressedData || compressedSize <= 0 || !decompressedData || !decompressedSize) {
        return paramErr;
    }

    /* Estimate decompressed size (this is simplified) */
    uLongf destLen = compressedSize * 4; /* Rough estimate */
    Bytef *dest = malloc(destLen);
    if (!dest) {
        return memFullErr;
    }

    int result = uncompress(dest, &destLen, (const Bytef *)compressedData, compressedSize);
    if (result != Z_OK) {
        free(dest);
        return paramErr;
    }

    *decompressedData = dest;
    *decompressedSize = destLen;

    return noErr;
}

/*
 * SetVoiceResourceProgressCallback
 * Sets progress callback for resource operations
 */
OSErr SetVoiceResourceProgressCallback(VoiceResourceManager *manager,
                                       VoiceResourceProgressProc callback, void *userData) {
    if (!manager) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);
    manager->progressCallback = callback;
    manager->progressUserData = userData;
    pthread_mutex_unlock(&manager->managerMutex);

    return noErr;
}

/*
 * SetVoiceResourceErrorCallback
 * Sets error callback for resource operations
 */
OSErr SetVoiceResourceErrorCallback(VoiceResourceManager *manager,
                                    VoiceResourceErrorProc callback, void *userData) {
    if (!manager) {
        return paramErr;
    }

    pthread_mutex_lock(&manager->managerMutex);
    manager->errorCallback = callback;
    manager->errorUserData = userData;
    pthread_mutex_unlock(&manager->managerMutex);

    return noErr;
}
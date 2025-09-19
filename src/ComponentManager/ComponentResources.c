/*
 * ComponentResources.c
 *
 * Component Resources Implementation - System 7.1 Portable
 * Handles component resource loading and management (thng resources)
 *
 * This module provides resource management functionality for the Component Manager,
 * including loading and parsing of component resources, handling of multi-platform
 * components, and resource caching.
 */

#include "ComponentManager/ComponentResources.h"
#include "ComponentManager/ComponentManager_HAL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* Global resource management state */
typedef struct ResourceManagerGlobals {
    bool initialized;
    ComponentResourceFile* resourceFiles;
    ResourceCache globalCache;
    ComponentMutex* resourceMutex;
    uint32_t nextResourceFileId;
    char* defaultResourcePath;
} ResourceManagerGlobals;

static ResourceManagerGlobals gResourceGlobals = {0};

/* Forward declarations */
static OSErr LoadResourceFile(const char* filePath, ComponentResourceFile** resourceFile);
static OSErr ParseResourceFileHeader(ComponentResourceFile* resourceFile);
static OSErr LoadResourceData(ComponentResourceFile* resourceFile, OSType resType, int16_t resID, Handle* resource);
static void FreeResourceFile(ComponentResourceFile* resourceFile);
static OSErr ValidateResourceHeader(ResourceFileHeader* header);
static OSErr CreateResourceHandle(void* data, uint32_t size, Handle* handle);

/*
 * InitComponentResources
 *
 * Initialize the component resource management system.
 */
OSErr InitComponentResources(void)
{
    OSErr result = noErr;

    if (gResourceGlobals.initialized) {
        return noErr; /* Already initialized */
    }

    /* Initialize mutex for thread safety */
    result = HAL_CreateMutex(&gResourceGlobals.resourceMutex);
    if (result != noErr) {
        return result;
    }

    /* Initialize resource cache */
    result = InitResourceCache(&gResourceGlobals.globalCache, 128);
    if (result != noErr) {
        HAL_DestroyMutex(gResourceGlobals.resourceMutex);
        return result;
    }

    /* Set default resource path */
    gResourceGlobals.defaultResourcePath = HAL_AllocateMemory(256);
    if (!gResourceGlobals.defaultResourcePath) {
        CleanupResourceCache(&gResourceGlobals.globalCache);
        HAL_DestroyMutex(gResourceGlobals.resourceMutex);
        return memFullErr;
    }

    /* Default to current directory for resources */
    strcpy(gResourceGlobals.defaultResourcePath, "./Resources/");

    gResourceGlobals.nextResourceFileId = 1000;
    gResourceGlobals.resourceFiles = NULL;
    gResourceGlobals.initialized = true;

    return noErr;
}

/*
 * CleanupComponentResources
 *
 * Cleanup the component resource management system.
 */
void CleanupComponentResources(void)
{
    if (!gResourceGlobals.initialized) {
        return;
    }

    HAL_LockMutex(gResourceGlobals.resourceMutex);

    /* Free all resource files */
    ComponentResourceFile* current = gResourceGlobals.resourceFiles;
    while (current != NULL) {
        ComponentResourceFile* next = current->next;
        FreeResourceFile(current);
        current = next;
    }

    /* Cleanup cache */
    CleanupResourceCache(&gResourceGlobals.globalCache);

    /* Free default path */
    if (gResourceGlobals.defaultResourcePath) {
        HAL_FreeMemory(gResourceGlobals.defaultResourcePath);
        gResourceGlobals.defaultResourcePath = NULL;
    }

    gResourceGlobals.initialized = false;

    HAL_UnlockMutex(gResourceGlobals.resourceMutex);
    HAL_DestroyMutex(gResourceGlobals.resourceMutex);
}

/*
 * OpenComponentResourceFile
 *
 * Open a resource file for a component.
 */
int16_t OpenComponentResourceFile(Component component)
{
    if (!gResourceGlobals.initialized || !component) {
        return -1;
    }

    HAL_LockMutex(gResourceGlobals.resourceMutex);

    /* For this implementation, we'll generate a simple file reference */
    int16_t refNum = (int16_t)gResourceGlobals.nextResourceFileId++;

    HAL_UnlockMutex(gResourceGlobals.resourceMutex);

    return refNum;
}

/*
 * CloseComponentResourceFile
 *
 * Close a component resource file.
 */
OSErr CloseComponentResourceFile(int16_t refNum)
{
    if (!gResourceGlobals.initialized || refNum <= 0) {
        return paramErr;
    }

    HAL_LockMutex(gResourceGlobals.resourceMutex);

    /* Find and remove the resource file */
    ComponentResourceFile* current = gResourceGlobals.resourceFiles;
    ComponentResourceFile* previous = NULL;

    while (current != NULL) {
        if (current->refNum == refNum) {
            if (previous) {
                previous->next = current->next;
            } else {
                gResourceGlobals.resourceFiles = current->next;
            }
            FreeResourceFile(current);
            break;
        }
        previous = current;
        current = current->next;
    }

    HAL_UnlockMutex(gResourceGlobals.resourceMutex);

    return noErr;
}

/*
 * RegisterComponentResourceFile
 *
 * Register a resource file with the component manager.
 */
OSErr RegisterComponentResourceFile(int16_t resRefNum, int16_t global)
{
    if (!gResourceGlobals.initialized) {
        return componentNotFound;
    }

    /* Create a new resource file entry */
    ComponentResourceFile* resourceFile = HAL_AllocateMemory(sizeof(ComponentResourceFile));
    if (!resourceFile) {
        return memFullErr;
    }

    resourceFile->refNum = resRefNum;
    resourceFile->isOpen = true;
    resourceFile->componentCount = 0;
    resourceFile->components = NULL;
    resourceFile->filePath = NULL;
    resourceFile->next = NULL;

    HAL_LockMutex(gResourceGlobals.resourceMutex);

    /* Add to the list */
    resourceFile->next = gResourceGlobals.resourceFiles;
    gResourceGlobals.resourceFiles = resourceFile;

    HAL_UnlockMutex(gResourceGlobals.resourceMutex);

    return noErr;
}

/*
 * LoadComponentResource
 *
 * Load a resource from a resource file.
 */
OSErr LoadComponentResource(int16_t resRefNum, OSType resType, int16_t resID, Handle* resource)
{
    if (!gResourceGlobals.initialized || !resource) {
        return paramErr;
    }

    *resource = NULL;

    /* Check cache first */
    Handle cachedResource = GetCachedResource(&gResourceGlobals.globalCache, resType, resID);
    if (cachedResource) {
        *resource = cachedResource;
        return noErr;
    }

    HAL_LockMutex(gResourceGlobals.resourceMutex);

    /* Find the resource file */
    ComponentResourceFile* resourceFile = gResourceGlobals.resourceFiles;
    while (resourceFile != NULL) {
        if (resourceFile->refNum == resRefNum) {
            break;
        }
        resourceFile = resourceFile->next;
    }

    if (!resourceFile) {
        HAL_UnlockMutex(gResourceGlobals.resourceMutex);
        return resNotFound;
    }

    /* Load the resource data */
    OSErr result = LoadResourceData(resourceFile, resType, resID, resource);

    /* Cache the resource if loaded successfully */
    if (result == noErr && *resource) {
        CacheResource(&gResourceGlobals.globalCache, resType, resID, *resource);
    }

    HAL_UnlockMutex(gResourceGlobals.resourceMutex);

    return result;
}

/*
 * LoadThingResource
 *
 * Load a component ('thng') resource.
 */
OSErr LoadThingResource(int16_t resRefNum, int16_t resID, ComponentResource** resource)
{
    if (!resource) {
        return paramErr;
    }

    Handle resourceHandle = NULL;
    OSErr result = LoadComponentResource(resRefNum, kThingResourceType, resID, &resourceHandle);

    if (result == noErr && resourceHandle) {
        result = ParseComponentResource(resourceHandle, resource);
    }

    return result;
}

/*
 * LoadExtThingResource
 *
 * Load an extended component resource.
 */
OSErr LoadExtThingResource(int16_t resRefNum, int16_t resID, ExtComponentResource** resource)
{
    if (!resource) {
        return paramErr;
    }

    Handle resourceHandle = NULL;
    OSErr result = LoadComponentResource(resRefNum, kThingResourceType, resID, &resourceHandle);

    if (result == noErr && resourceHandle) {
        result = ParseExtComponentResource(resourceHandle, resource);
    }

    return result;
}

/*
 * ParseComponentResource
 *
 * Parse a component resource from a handle.
 */
OSErr ParseComponentResource(Handle resourceHandle, ComponentResource** component)
{
    if (!resourceHandle || !component) {
        return paramErr;
    }

    /* Allocate component resource structure */
    *component = HAL_AllocateMemory(sizeof(ComponentResource));
    if (!*component) {
        return memFullErr;
    }

    /* For this implementation, we'll create a minimal valid component resource */
    memset(*component, 0, sizeof(ComponentResource));

    /* Set default values */
    (*component)->cd.componentType = 'appl';
    (*component)->cd.componentSubType = 'demo';
    (*component)->cd.componentManufacturer = 'sys7';
    (*component)->cd.componentFlags = 0;
    (*component)->cd.componentFlagsMask = 0;

    (*component)->component.resType = 'CODE';
    (*component)->component.resId = 1;

    (*component)->componentName.resType = 'STR ';
    (*component)->componentName.resId = 100;

    (*component)->componentInfo.resType = 'STR ';
    (*component)->componentInfo.resId = 101;

    (*component)->componentIcon.resType = 'ICON';
    (*component)->componentIcon.resId = 128;

    return noErr;
}

/*
 * ParseExtComponentResource
 *
 * Parse an extended component resource from a handle.
 */
OSErr ParseExtComponentResource(Handle resourceHandle, ExtComponentResource** component)
{
    if (!resourceHandle || !component) {
        return paramErr;
    }

    /* Allocate extended component resource structure */
    *component = HAL_AllocateMemory(sizeof(ExtComponentResource));
    if (!*component) {
        return memFullErr;
    }

    /* For this implementation, we'll create a minimal valid extended component resource */
    memset(*component, 0, sizeof(ExtComponentResource));

    /* Set default values */
    (*component)->cd.componentType = 'appl';
    (*component)->cd.componentSubType = 'demo';
    (*component)->cd.componentManufacturer = 'sys7';
    (*component)->cd.componentFlags = 0;
    (*component)->cd.componentFlagsMask = 0;

    (*component)->component.resType = 'CODE';
    (*component)->component.resId = 1;

    (*component)->componentName.resType = 'STR ';
    (*component)->componentName.resId = 100;

    (*component)->componentInfo.resType = 'STR ';
    (*component)->componentInfo.resId = 101;

    (*component)->componentIcon.resType = 'ICON';
    (*component)->componentIcon.resId = 128;

    (*component)->componentVersion = 0x00010000; /* Version 1.0 */
    (*component)->componentRegisterFlags = 0;
    (*component)->componentIconFamily = 129;
    (*component)->count = 1;

    /* Set up platform info for current platform */
    (*component)->platformArray[0].componentFlags = 0;
    (*component)->platformArray[0].component.resType = 'CODE';
    (*component)->platformArray[0].component.resId = 1;
    (*component)->platformArray[0].platformType = GetCurrentPlatformType();

    return noErr;
}

/*
 * ExtractComponentDescription
 *
 * Extract a component description from a resource.
 */
OSErr ExtractComponentDescriptionFromResource(ComponentResource* resource, ComponentDescription* description)
{
    if (!resource || !description) {
        return paramErr;
    }

    *description = resource->cd;
    return noErr;
}

/*
 * GetComponentName
 *
 * Get the name of a component.
 */
OSErr GetComponentName(Component component, Handle* nameHandle, char** nameString)
{
    if (!component) {
        return paramErr;
    }

    /* For this implementation, return a default name */
    if (nameString) {
        *nameString = HAL_AllocateMemory(64);
        if (*nameString) {
            strcpy(*nameString, "Sample Component");
        }
    }

    if (nameHandle) {
        CreateResourceHandle("Sample Component", 16, nameHandle);
    }

    return noErr;
}

/*
 * GetComponentInfo
 *
 * Get component information.
 */
OSErr GetComponentInfoFromResource(Component component, Handle* infoHandle, char** infoString)
{
    if (!component) {
        return paramErr;
    }

    /* For this implementation, return default info */
    if (infoString) {
        *infoString = HAL_AllocateMemory(128);
        if (*infoString) {
            strcpy(*infoString, "Sample component for System 7.1 Portable");
        }
    }

    if (infoHandle) {
        CreateResourceHandle("Sample component for System 7.1 Portable", 40, infoHandle);
    }

    return noErr;
}

/*
 * GetComponentIcon
 *
 * Get component icon.
 */
OSErr GetComponentIconFromResource(Component component, Handle* iconHandle)
{
    if (!component || !iconHandle) {
        return paramErr;
    }

    /* For this implementation, return a minimal icon handle */
    uint8_t iconData[32] = {0}; /* Minimal icon data */
    return CreateResourceHandle(iconData, sizeof(iconData), iconHandle);
}

/*
 * GetComponentIconFamily
 *
 * Get component icon family.
 */
OSErr GetComponentIconFamilyFromResource(Component component, Handle* iconFamilyHandle)
{
    if (!component || !iconFamilyHandle) {
        return paramErr;
    }

    /* For this implementation, return a minimal icon family handle */
    uint8_t iconFamilyData[64] = {0}; /* Minimal icon family data */
    return CreateResourceHandle(iconFamilyData, sizeof(iconFamilyData), iconFamilyHandle);
}

/*
 * GetComponentVersion
 *
 * Get component version.
 */
OSErr GetComponentVersionFromResource(Component component, int32_t* version)
{
    if (!component || !version) {
        return paramErr;
    }

    *version = 0x00010000; /* Version 1.0 */
    return noErr;
}

/*
 * GetCurrentPlatformType
 *
 * Get the current platform type.
 */
int16_t GetCurrentPlatformType(void)
{
#ifdef __powerpc__
    return gestaltPowerPC;
#else
    return gestalt68k;
#endif
}

/*
 * ValidateComponentResource
 *
 * Validate a component resource.
 */
bool ValidateComponentResource(ComponentResource* resource)
{
    if (!resource) {
        return false;
    }

    /* Basic validation */
    if (resource->cd.componentType == 0) {
        return false;
    }

    return true;
}

/*
 * ValidateExtComponentResource
 *
 * Validate an extended component resource.
 */
bool ValidateExtComponentResource(ExtComponentResource* resource)
{
    if (!resource) {
        return false;
    }

    /* Basic validation */
    if (resource->cd.componentType == 0) {
        return false;
    }

    if (resource->count < 0 || resource->count > 16) {
        return false;
    }

    return true;
}

/*
 * InitResourceCache
 *
 * Initialize a resource cache.
 */
OSErr InitResourceCache(ResourceCache* cache, uint32_t maxCount)
{
    if (!cache) {
        return paramErr;
    }

    cache->resources = HAL_AllocateMemory(maxCount * sizeof(Handle));
    cache->types = HAL_AllocateMemory(maxCount * sizeof(OSType));
    cache->ids = HAL_AllocateMemory(maxCount * sizeof(int16_t));
    cache->lastAccess = HAL_AllocateMemory(maxCount * sizeof(uint64_t));

    if (!cache->resources || !cache->types || !cache->ids || !cache->lastAccess) {
        CleanupResourceCache(cache);
        return memFullErr;
    }

    cache->count = 0;
    cache->maxCount = maxCount;
    cache->enabled = true;

    return noErr;
}

/*
 * CleanupResourceCache
 *
 * Cleanup a resource cache.
 */
OSErr CleanupResourceCache(ResourceCache* cache)
{
    if (!cache) {
        return paramErr;
    }

    if (cache->resources) {
        HAL_FreeMemory(cache->resources);
        cache->resources = NULL;
    }

    if (cache->types) {
        HAL_FreeMemory(cache->types);
        cache->types = NULL;
    }

    if (cache->ids) {
        HAL_FreeMemory(cache->ids);
        cache->ids = NULL;
    }

    if (cache->lastAccess) {
        HAL_FreeMemory(cache->lastAccess);
        cache->lastAccess = NULL;
    }

    cache->count = 0;
    cache->maxCount = 0;
    cache->enabled = false;

    return noErr;
}

/*
 * CacheResource
 *
 * Cache a resource.
 */
OSErr CacheResource(ResourceCache* cache, OSType resType, int16_t resID, Handle resource)
{
    if (!cache || !cache->enabled || !resource) {
        return paramErr;
    }

    if (cache->count >= cache->maxCount) {
        /* Cache is full, could implement LRU eviction here */
        return memFullErr;
    }

    cache->resources[cache->count] = resource;
    cache->types[cache->count] = resType;
    cache->ids[cache->count] = resID;
    cache->lastAccess[cache->count] = HAL_GetCurrentTime();
    cache->count++;

    return noErr;
}

/*
 * GetCachedResource
 *
 * Get a cached resource.
 */
Handle GetCachedResource(ResourceCache* cache, OSType resType, int16_t resID)
{
    if (!cache || !cache->enabled) {
        return NULL;
    }

    for (uint32_t i = 0; i < cache->count; i++) {
        if (cache->types[i] == resType && cache->ids[i] == resID) {
            cache->lastAccess[i] = HAL_GetCurrentTime();
            return cache->resources[i];
        }
    }

    return NULL;
}

/*
 * InvalidateResourceCache
 *
 * Invalidate all cached resources.
 */
OSErr InvalidateResourceCache(ResourceCache* cache)
{
    if (!cache) {
        return paramErr;
    }

    cache->count = 0;
    return noErr;
}

/* Helper Functions */

/*
 * LoadResourceData
 *
 * Load resource data from a file.
 */
static OSErr LoadResourceData(ComponentResourceFile* resourceFile, OSType resType, int16_t resID, Handle* resource)
{
    if (!resourceFile || !resource) {
        return paramErr;
    }

    /* For this implementation, create a minimal resource handle */
    uint8_t dummyData[256] = {0};
    return CreateResourceHandle(dummyData, sizeof(dummyData), resource);
}

/*
 * FreeResourceFile
 *
 * Free a resource file structure.
 */
static void FreeResourceFile(ComponentResourceFile* resourceFile)
{
    if (!resourceFile) {
        return;
    }

    if (resourceFile->filePath) {
        HAL_FreeMemory(resourceFile->filePath);
    }

    if (resourceFile->components) {
        HAL_FreeMemory(resourceFile->components);
    }

    HAL_FreeMemory(resourceFile);
}

/*
 * CreateResourceHandle
 *
 * Create a resource handle from data.
 */
static OSErr CreateResourceHandle(void* data, uint32_t size, Handle* handle)
{
    if (!data || !handle) {
        return paramErr;
    }

    *handle = HAL_AllocateMemory(size);
    if (!*handle) {
        return memFullErr;
    }

    memcpy(*handle, data, size);
    return noErr;
}

/*
 * LoadStringResource
 *
 * Load a string resource.
 */
OSErr LoadStringResource(int16_t resRefNum, int16_t resID, char** string)
{
    if (!string) {
        return paramErr;
    }

    /* For this implementation, return a default string */
    *string = HAL_AllocateMemory(64);
    if (!*string) {
        return memFullErr;
    }

    sprintf(*string, "String Resource %d", resID);
    return noErr;
}

/*
 * LoadVersionResource
 *
 * Load version information.
 */
OSErr LoadVersionResource(int16_t resRefNum, int16_t resID, ComponentVersionInfo* versionInfo)
{
    if (!versionInfo) {
        return paramErr;
    }

    /* Set default version info */
    versionInfo->majorVersion = 1;
    versionInfo->minorVersion = 0;
    versionInfo->bugFixVersion = 0;
    versionInfo->stage = 4; /* final */
    versionInfo->revision = 0;

    versionInfo->versionString = HAL_AllocateMemory(32);
    if (versionInfo->versionString) {
        strcpy(versionInfo->versionString, "1.0");
    }

    versionInfo->copyrightString = HAL_AllocateMemory(64);
    if (versionInfo->copyrightString) {
        strcpy(versionInfo->copyrightString, "System 7.1 Portable");
    }

    return noErr;
}

/*
 * CompareVersions
 *
 * Compare two version information structures.
 */
int32_t CompareVersions(ComponentVersionInfo* version1, ComponentVersionInfo* version2)
{
    if (!version1 || !version2) {
        return 0;
    }

    if (version1->majorVersion != version2->majorVersion) {
        return version1->majorVersion - version2->majorVersion;
    }

    if (version1->minorVersion != version2->minorVersion) {
        return version1->minorVersion - version2->minorVersion;
    }

    if (version1->bugFixVersion != version2->bugFixVersion) {
        return version1->bugFixVersion - version2->bugFixVersion;
    }

    return version1->revision - version2->revision;
}

/*
 * DetectResourceFormat
 *
 * Detect the format of a resource file.
 */
OSErr DetectResourceFormat(const char* filePath, ResourceFormat* format)
{
    if (!filePath || !format) {
        return paramErr;
    }

    /* For this implementation, default to Mac OS format */
    *format = kResourceFormatMacOS;
    return noErr;
}

/*
 * EstimateResourceMemoryUsage
 *
 * Estimate memory usage for resources.
 */
OSErr EstimateResourceMemoryUsage(int16_t resRefNum, uint32_t* totalSize)
{
    if (!totalSize) {
        return paramErr;
    }

    /* For this implementation, return a default estimate */
    *totalSize = 64 * 1024; /* 64KB estimate */
    return noErr;
}
/*
 * ComponentResources.h
 *
 * Component Resources API - System 7.1 Portable Implementation
 * Handles component resource loading and management (thng resources)
 */

#ifndef COMPONENTRESOURCES_H
#define COMPONENTRESOURCES_H

#include "ComponentManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resource types */
#define kThingResourceType          'thng'  /* Component resource type */
#define kStringResourceType         'STR '  /* String resource type */
#define kStringListResourceType     'STR#'  /* String list resource type */
#define kIconResourceType           'ICON'  /* Icon resource type */
#define kIconFamilyResourceType     'icns'  /* Icon family resource type */
#define kVersionResourceType        'vers'  /* Version resource type */
#define kCodeResourceType           'CODE'  /* Code resource type */

/* Resource file management */
typedef struct ComponentResourceFile {
    int16_t refNum;                 /* Resource file reference number */
    char* filePath;                 /* Path to resource file */
    bool isOpen;                    /* File is currently open */
    uint32_t componentCount;        /* Number of components in file */
    Component* components;          /* Array of components from this file */
    struct ComponentResourceFile* next;
} ComponentResourceFile;

/* Resource initialization */
OSErr InitComponentResources(void);
void CleanupComponentResources(void);

/* Resource file operations */
int16_t OpenComponentResourceFile(Component component);
OSErr CloseComponentResourceFile(int16_t refNum);
OSErr RegisterComponentResourceFile(int16_t resRefNum, int16_t global);

/* Component resource loading */
OSErr LoadComponentResource(int16_t resRefNum, OSType resType, int16_t resID, Handle* resource);
OSErr LoadThingResource(int16_t resRefNum, int16_t resID, ComponentResource** resource);
OSErr LoadExtThingResource(int16_t resRefNum, int16_t resID, ExtComponentResource** resource);

/* Component resource parsing */
OSErr ParseComponentResource(Handle resourceHandle, ComponentResource** component);
OSErr ParseExtComponentResource(Handle resourceHandle, ExtComponentResource** component);
OSErr ExtractComponentDescriptionFromResource(ComponentResource* resource, ComponentDescription* description);

/* Component metadata extraction */
OSErr GetComponentName(Component component, Handle* nameHandle, char** nameString);
OSErr GetComponentInfoFromResource(Component component, Handle* infoHandle, char** infoString);
OSErr GetComponentIconFromResource(Component component, Handle* iconHandle);
OSErr GetComponentIconFamilyFromResource(Component component, Handle* iconFamilyHandle);
OSErr GetComponentVersionFromResource(Component component, int32_t* version);

/* Platform information handling */
OSErr GetComponentPlatformInfo(ExtComponentResource* resource, int16_t platformType, ComponentPlatformInfo** info);
OSErr SelectBestPlatform(ExtComponentResource* resource, ComponentPlatformInfo** selectedInfo);
int16_t GetCurrentPlatformType(void);

/* Resource creation and modification */
OSErr CreateComponentResource(ComponentDescription* description, ComponentResource** resource);
OSErr CreateExtComponentResource(ComponentDescription* description, ExtComponentResource** resource);
OSErr AddPlatformInfo(ExtComponentResource* resource, ComponentPlatformInfo* platformInfo);

/* Resource validation */
bool ValidateComponentResource(ComponentResource* resource);
bool ValidateExtComponentResource(ExtComponentResource* resource);
OSErr CheckResourceIntegrity(int16_t resRefNum, OSType resType, int16_t resID);

/* String resource utilities */
OSErr LoadStringResource(int16_t resRefNum, int16_t resID, char** string);
OSErr LoadStringListResource(int16_t resRefNum, int16_t resID, char*** strings, int32_t* count);
OSErr CreateStringResource(const char* string, Handle* resourceHandle);

/* Icon resource utilities */
OSErr LoadIconResource(int16_t resRefNum, int16_t resID, void** iconData, uint32_t* iconSize);
OSErr CreateIconFromFile(const char* iconFilePath, Handle* iconHandle);
OSErr ConvertIconFormat(Handle srcIcon, uint32_t srcFormat, Handle* dstIcon, uint32_t dstFormat);

/* Version resource utilities */
typedef struct ComponentVersionInfo {
    int16_t majorVersion;
    int16_t minorVersion;
    int16_t bugFixVersion;
    int16_t stage;              /* development, alpha, beta, final */
    int16_t revision;
    char* versionString;
    char* copyrightString;
} ComponentVersionInfo;

OSErr LoadVersionResource(int16_t resRefNum, int16_t resID, ComponentVersionInfo* versionInfo);
OSErr CreateVersionResource(ComponentVersionInfo* versionInfo, Handle* resourceHandle);
int32_t CompareVersions(ComponentVersionInfo* version1, ComponentVersionInfo* version2);

/* Cross-platform resource format support */
typedef enum {
    kResourceFormatMacOS,       /* Classic Mac OS resource fork */
    kResourceFormatWindows,     /* Windows PE resources */
    kResourceFormatLinux,       /* Linux shared object resources */
    kResourceFormatGeneric      /* Generic binary format */
} ResourceFormat;

OSErr DetectResourceFormat(const char* filePath, ResourceFormat* format);
OSErr ConvertResourceFormat(const char* srcPath, ResourceFormat srcFormat,
                           const char* dstPath, ResourceFormat dstFormat);

/* Resource caching */
typedef struct ResourceCache {
    Handle* resources;
    OSType* types;
    int16_t* ids;
    uint32_t count;
    uint32_t maxCount;
    uint64_t* lastAccess;
    bool enabled;
} ResourceCache;

OSErr InitResourceCache(ResourceCache* cache, uint32_t maxCount);
OSErr CleanupResourceCache(ResourceCache* cache);
OSErr CacheResource(ResourceCache* cache, OSType resType, int16_t resID, Handle resource);
Handle GetCachedResource(ResourceCache* cache, OSType resType, int16_t resID);
OSErr InvalidateResourceCache(ResourceCache* cache);

/* Resource enumeration */
typedef bool (*ResourceEnumeratorFunc)(OSType resType, int16_t resID, Handle resource, void* userData);
OSErr EnumerateResources(int16_t resRefNum, OSType resType, ResourceEnumeratorFunc enumerator, void* userData);
OSErr EnumerateComponentResources(int16_t resRefNum, ResourceEnumeratorFunc enumerator, void* userData);

/* Resource dependency tracking */
typedef struct ResourceDependency {
    struct ResourceDependency* next;
    OSType resType;
    int16_t resID;
    int16_t refNum;
    bool required;
} ResourceDependency;

OSErr AddResourceDependency(Component component, ResourceDependency* dependency);
OSErr RemoveResourceDependency(Component component, ResourceDependency* dependency);
OSErr ResolveResourceDependencies(Component component);

/* Resource localization support */
typedef struct LocalizedResource {
    int16_t languageCode;
    int16_t regionCode;
    Handle resource;
    struct LocalizedResource* next;
} LocalizedResource;

OSErr LoadLocalizedResource(int16_t resRefNum, OSType resType, int16_t resID,
                           int16_t languageCode, int16_t regionCode, Handle* resource);
OSErr GetBestLocalizedResource(LocalizedResource* resources, int16_t preferredLanguage,
                              int16_t preferredRegion, Handle* bestResource);

/* Resource compression support */
typedef enum {
    kResourceCompressionNone,
    kResourceCompressionLZW,
    kResourceCompressionDeflate,
    kResourceCompressionBZip2
} ResourceCompressionType;

OSErr CompressResource(Handle resource, ResourceCompressionType compression, Handle* compressedResource);
OSErr DecompressResource(Handle compressedResource, ResourceCompressionType compression, Handle* resource);
OSErr GetResourceCompressionInfo(Handle resource, ResourceCompressionType* compression, uint32_t* originalSize);

/* Resource security and validation */
OSErr ValidateResourceSignature(int16_t resRefNum, OSType resType, int16_t resID);
OSErr SignResource(Handle resource, const char* privateKeyPath);
OSErr VerifyResourceSignature(Handle resource, const char* publicKeyPath);

/* Resource file format utilities */
typedef struct ResourceFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t resourceOffset;
    uint32_t resourceSize;
    uint32_t resourceCount;
    uint32_t typeListOffset;
    uint32_t nameListOffset;
} ResourceFileHeader;

OSErr ReadResourceFileHeader(int16_t resRefNum, ResourceFileHeader* header);
OSErr WriteResourceFileHeader(int16_t resRefNum, ResourceFileHeader* header);
OSErr ValidateResourceFile(int16_t resRefNum);

/* Memory management for resources */
OSErr LoadResourceToMemory(int16_t resRefNum, OSType resType, int16_t resID, void** data, uint32_t* size);
OSErr SaveResourceFromMemory(int16_t resRefNum, OSType resType, int16_t resID, void* data, uint32_t size);
OSErr EstimateResourceMemoryUsage(int16_t resRefNum, uint32_t* totalSize);

/* Resource debugging and introspection */
typedef struct ResourceDebugInfo {
    OSType resType;
    int16_t resID;
    uint32_t size;
    char* name;
    uint32_t attributes;
    bool isCompressed;
    bool isSigned;
    ResourceCompressionType compression;
} ResourceDebugInfo;

OSErr GetResourceDebugInfo(int16_t resRefNum, OSType resType, int16_t resID, ResourceDebugInfo* info);
OSErr DumpResourceInfo(int16_t resRefNum, char** infoString);
OSErr ValidateAllResources(int16_t resRefNum);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTRESOURCES_H */
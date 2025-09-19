/*
 * ComponentLoader.h
 *
 * Component Loader API - System 7.1 Portable Implementation
 * Handles dynamic loading and unloading of component modules
 */

#ifndef COMPONENTLOADER_H
#define COMPONENTLOADER_H

#include "ComponentManager.h"
#include "ComponentRegistry.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Component module types */
typedef enum {
    kComponentModuleTypeNative,     /* Native dynamic library (.dll, .so, .dylib) */
    kComponentModuleTypeResource,   /* Mac OS resource-based component */
    kComponentModuleTypeScript,     /* Script-based component */
    kComponentModuleTypeNetwork     /* Network-based component */
} ComponentModuleType;

/* Component loader context */
typedef struct ComponentLoaderContext {
    ComponentModuleType moduleType;
    char* modulePath;
    ComponentModuleHandle handle;
    void* entryPoint;
    void* userData;
    uint32_t loadFlags;
    uint32_t securityContext;
} ComponentLoaderContext;

/* Component loading flags */
#define kComponentLoadFlagLazy          (1<<0)  /* Lazy loading - load when first accessed */
#define kComponentLoadFlagGlobal        (1<<1)  /* Global loading - visible to all processes */
#define kComponentLoadFlagSecure        (1<<2)  /* Secure loading - verify signature */
#define kComponentLoadFlagSandbox       (1<<3)  /* Sandbox the component */
#define kComponentLoadFlagNetwork       (1<<4)  /* Allow network access */
#define kComponentLoadFlagDeep          (1<<5)  /* Deep binding - resolve all symbols */

/* Component loader initialization */
OSErr InitComponentLoader(void);
void CleanupComponentLoader(void);

/* Component module loading */
OSErr LoadComponent(ComponentRegistryEntry* entry, uint32_t loadFlags);
OSErr UnloadComponent(ComponentRegistryEntry* entry);
OSErr ReloadComponent(ComponentRegistryEntry* entry);

/* Component module discovery */
OSErr ScanForComponents(const char* searchPath, bool recursive);
OSErr RegisterComponentsInDirectory(const char* directoryPath, bool recursive);

/* Platform-specific loading functions */
OSErr LoadNativeComponent(ComponentLoaderContext* context);
OSErr LoadResourceComponent(ComponentLoaderContext* context);
OSErr LoadScriptComponent(ComponentLoaderContext* context);
OSErr LoadNetworkComponent(ComponentLoaderContext* context);

/* Component entry point resolution */
ComponentRoutine ResolveComponentEntryPoint(ComponentLoaderContext* context);
void* ResolveComponentSymbol(ComponentLoaderContext* context, const char* symbolName);

/* Component validation */
bool ValidateComponentModule(const char* modulePath);
bool VerifyComponentSignature(const char* modulePath);
OSErr CheckComponentCompatibility(ComponentLoaderContext* context);

/* Component metadata extraction */
OSErr ExtractComponentDescription(const char* modulePath, ComponentDescription* description);
OSErr ExtractComponentResources(const char* modulePath, Handle* name, Handle* info, Handle* icon);
int32_t ExtractComponentVersion(const char* modulePath);

/* Resource-based component support */
OSErr LoadComponentFromResource(int16_t resFileRefNum, int16_t resID, ComponentRegistryEntry* entry);
OSErr LoadThingResource(int16_t resFileRefNum, int16_t resID, ComponentResource** resource);
OSErr LoadExtThingResource(int16_t resFileRefNum, int16_t resID, ExtComponentResource** resource);

/* Component hot-plugging support */
typedef void (*ComponentHotPlugCallback)(Component component, bool loaded, void* userData);
OSErr RegisterHotPlugCallback(ComponentHotPlugCallback callback, void* userData);
OSErr UnregisterHotPlugCallback(ComponentHotPlugCallback callback);
OSErr EnableComponentHotPlug(const char* watchPath);
OSErr DisableComponentHotPlug(void);

/* Component dependency management */
typedef struct ComponentDependency {
    struct ComponentDependency* next;
    OSType dependentType;
    OSType dependentSubType;
    OSType dependentManufacturer;
    int32_t minimumVersion;
    bool required;
} ComponentDependency;

OSErr AddComponentDependency(ComponentRegistryEntry* entry, ComponentDependency* dependency);
OSErr RemoveComponentDependency(ComponentRegistryEntry* entry, ComponentDependency* dependency);
OSErr ResolveDependencies(ComponentRegistryEntry* entry);
OSErr CheckDependencies(ComponentRegistryEntry* entry);

/* Component sandboxing (platform-specific) */
#ifdef _WIN32
#include <windows.h>
typedef struct {
    HANDLE jobObject;
    SECURITY_ATTRIBUTES securityAttributes;
    DWORD restrictionFlags;
} ComponentSandbox;
#elif defined(__APPLE__)
typedef struct {
    void* sandboxProfile;
    uint32_t sandboxFlags;
} ComponentSandbox;
#elif defined(__linux__)
typedef struct {
    pid_t namespaceId;
    int restrictionFlags;
} ComponentSandbox;
#else
typedef struct {
    void* placeholder;
} ComponentSandbox;
#endif

OSErr CreateComponentSandbox(ComponentLoaderContext* context, ComponentSandbox* sandbox);
OSErr DestroyComponentSandbox(ComponentSandbox* sandbox);
OSErr ExecuteInSandbox(ComponentSandbox* sandbox, ComponentRoutine routine, ComponentParameters* params);

/* Component search paths */
OSErr AddComponentSearchPath(const char* path);
OSErr RemoveComponentSearchPath(const char* path);
OSErr GetComponentSearchPaths(char*** paths, int32_t* count);
OSErr SetComponentSearchPaths(char** paths, int32_t count);

/* Component caching */
typedef struct ComponentCache {
    char* cachePath;
    uint32_t maxCacheSize;
    uint32_t currentCacheSize;
    uint32_t cacheTimeout;
    bool enableCache;
} ComponentCache;

OSErr InitComponentCache(ComponentCache* cache, const char* cachePath);
OSErr CleanupComponentCache(ComponentCache* cache);
OSErr CacheComponent(ComponentCache* cache, const char* modulePath);
OSErr GetCachedComponent(ComponentCache* cache, const char* modulePath, ComponentLoaderContext* context);
OSErr InvalidateComponentCache(ComponentCache* cache);

/* Error handling and debugging */
typedef enum {
    kComponentLoadErrorNone = 0,
    kComponentLoadErrorFileNotFound,
    kComponentLoadErrorInvalidFormat,
    kComponentLoadErrorMissingSymbol,
    kComponentLoadErrorVersionMismatch,
    kComponentLoadErrorSecurityViolation,
    kComponentLoadErrorDependencyMissing,
    kComponentLoadErrorIncompatiblePlatform,
    kComponentLoadErrorOutOfMemory,
    kComponentLoadErrorUnknown
} ComponentLoadError;

const char* GetComponentLoadErrorString(ComponentLoadError error);
OSErr GetLastComponentLoadError(ComponentLoadError* error, char** errorMessage);

/* Component performance monitoring */
typedef struct ComponentLoadStats {
    uint32_t loadCount;
    uint32_t unloadCount;
    uint32_t failureCount;
    uint64_t totalLoadTime;
    uint64_t totalUnloadTime;
    uint32_t cacheHits;
    uint32_t cacheMisses;
} ComponentLoadStats;

OSErr GetComponentLoadStats(ComponentLoadStats* stats);
OSErr ResetComponentLoadStats(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTLOADER_H */
/*
 * ComponentRegistry.h
 *
 * Component Registry API - System 7.1 Portable Implementation
 * Manages component registration, discovery, and database operations
 */

#ifndef COMPONENTREGISTRY_H
#define COMPONENTREGISTRY_H

#include "ComponentManager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registry entry structure */
typedef struct ComponentRegistryEntry {
    struct ComponentRegistryEntry* next;
    ComponentDescription description;
    ComponentRoutine entryPoint;
    void* moduleHandle;             /* Platform-specific module handle (HMODULE, dlopen handle, etc.) */
    char* modulePath;               /* Path to the component module */
    Handle name;                    /* Component name resource */
    Handle info;                    /* Component info resource */
    Handle icon;                    /* Component icon resource */
    int32_t version;                /* Component version */
    int32_t refcon;                 /* Reference constant */
    int32_t instanceCount;          /* Number of open instances */
    uint32_t registrationFlags;     /* Registration flags */
    bool isGlobal;                  /* Global vs. local registration */
    bool wantsRegisterMessage;      /* Component wants register/unregister messages */
    bool isResourceBased;           /* Registered from resource vs. direct registration */
    int16_t resFileRefNum;          /* Resource file reference number if resource-based */
    uint32_t modificationSeed;      /* Seed for tracking registry changes */
} ComponentRegistryEntry;

/* Registry management functions */
OSErr InitComponentRegistry(void);
void CleanupComponentRegistry(void);

/* Component registration functions */
Component RegisterComponentInternal(ComponentDescription* cd, ComponentRoutine entryPoint,
                                   int16_t global, Handle name, Handle info, Handle icon,
                                   bool isResourceBased, int16_t resFileRefNum);

OSErr UnregisterComponentInternal(Component component);

/* Component search and enumeration */
Component FindFirstComponent(ComponentDescription* looking);
Component FindNextComponentInternal(Component previous, ComponentDescription* looking);
int32_t CountComponentsInternal(ComponentDescription* looking);

/* Component information retrieval */
OSErr GetComponentInfoInternal(Component component, ComponentDescription* cd,
                              Handle componentName, Handle componentInfo, Handle componentIcon);

ComponentRegistryEntry* GetComponentEntry(Component component);
bool ValidateComponent(Component component);

/* Registry database management */
int32_t GetRegistryModificationSeed(void);
void InvalidateRegistryCache(void);

/* Component loading and unloading */
OSErr LoadComponentModule(ComponentRegistryEntry* entry);
OSErr UnloadComponentModule(ComponentRegistryEntry* entry);

/* Component resource management */
OSErr LoadComponentResources(ComponentRegistryEntry* entry);
OSErr UnloadComponentResources(ComponentRegistryEntry* entry);

/* Component capability queries */
bool ComponentMatchesDescription(ComponentRegistryEntry* entry, ComponentDescription* looking);
bool ComponentSupportsSelector(ComponentRegistryEntry* entry, int16_t selector);

/* Registry iteration */
typedef bool (*ComponentRegistryIteratorFunc)(ComponentRegistryEntry* entry, void* userData);
void IterateRegistry(ComponentRegistryIteratorFunc iterator, void* userData);

/* Platform-specific module functions */
#ifdef _WIN32
#include <windows.h>
typedef HMODULE ComponentModuleHandle;
#define COMPONENT_MODULE_INVALID NULL
#elif defined(__APPLE__) || defined(__linux__)
typedef void* ComponentModuleHandle;
#define COMPONENT_MODULE_INVALID NULL
#else
typedef void* ComponentModuleHandle;
#define COMPONENT_MODULE_INVALID NULL
#endif

ComponentModuleHandle LoadComponentPlatformModule(const char* path);
void UnloadComponentPlatformModule(ComponentModuleHandle handle);
ComponentRoutine GetComponentPlatformEntryPoint(ComponentModuleHandle handle, const char* entryName);

/* Resource file registration */
OSErr RegisterComponentsFromResourceFile(int16_t resFileRefNum, int16_t global);
OSErr UnregisterComponentsFromResourceFile(int16_t resFileRefNum);

/* Component validation and security */
bool ValidateComponentModule(const char* path);
bool CheckComponentSecurity(ComponentRegistryEntry* entry);

/* Registry persistence */
OSErr SaveRegistryToFile(const char* path);
OSErr LoadRegistryFromFile(const char* path);

/* Default component management */
OSErr SetDefaultComponentInternal(Component component, int16_t flags);
Component GetDefaultComponent(OSType componentType, OSType componentSubType);

/* Component capture/delegation management */
OSErr CaptureComponentInternal(Component capturedComponent, Component capturingComponent);
OSErr UncaptureComponentInternal(Component component);
Component GetCapturedComponent(Component component);
Component GetCapturingComponent(Component component);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTREGISTRY_H */
/*
 * ComponentRegistry.c
 *
 * Component Registry Implementation - System 7.1 Portable
 * Manages component registration, discovery, and database operations
 */

#include "ComponentRegistry.h"
#include "ComponentLoader.h"
#include "ComponentSecurity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Registry globals */
static struct {
    ComponentRegistryEntry* firstEntry;
    ComponentRegistryEntry* lastEntry;
    uint32_t entryCount;
    uint32_t modificationSeed;
    ComponentMutex* registryMutex;
    bool initialized;
} gRegistry = {0};

/* Default components registry */
static struct {
    OSType componentType;
    OSType componentSubType;
    Component defaultComponent;
} gDefaultComponents[32];
static int32_t gDefaultComponentCount = 0;

/* Captured components registry */
typedef struct CapturedComponent {
    Component captured;
    Component capturing;
    struct CapturedComponent* next;
} CapturedComponent;
static CapturedComponent* gCapturedComponents = NULL;

/* Forward declarations */
static ComponentRegistryEntry* AllocateRegistryEntry(void);
static void FreeRegistryEntry(ComponentRegistryEntry* entry);
static uint32_t GenerateComponentHandle(ComponentRegistryEntry* entry);
static ComponentRegistryEntry* FindEntryByHandle(Component component);
static void AddEntryToRegistry(ComponentRegistryEntry* entry);
static void RemoveEntryFromRegistry(ComponentRegistryEntry* entry);
static bool DescriptionMatches(ComponentDescription* entry, ComponentDescription* looking);
static void IncrementModificationSeed(void);

/*
 * InitComponentRegistry
 *
 * Initialize the component registry.
 */
OSErr InitComponentRegistry(void)
{
    OSErr err;

    if (gRegistry.initialized) {
        return noErr;
    }

    memset(&gRegistry, 0, sizeof(gRegistry));
    memset(gDefaultComponents, 0, sizeof(gDefaultComponents));
    gDefaultComponentCount = 0;
    gCapturedComponents = NULL;

    /* Create registry mutex */
    err = CreateComponentMutex(&gRegistry.registryMutex);
    if (err != noErr) {
        return err;
    }

    gRegistry.modificationSeed = 1;
    gRegistry.initialized = true;

    return noErr;
}

/*
 * CleanupComponentRegistry
 *
 * Clean up the component registry.
 */
void CleanupComponentRegistry(void)
{
    ComponentRegistryEntry* entry;
    ComponentRegistryEntry* next;
    CapturedComponent* captured;
    CapturedComponent* nextCaptured;

    if (!gRegistry.initialized) {
        return;
    }

    LockComponentMutex(gRegistry.registryMutex);

    /* Free all registry entries */
    entry = gRegistry.firstEntry;
    while (entry) {
        next = entry->next;
        FreeRegistryEntry(entry);
        entry = next;
    }

    /* Free captured components list */
    captured = gCapturedComponents;
    while (captured) {
        nextCaptured = captured->next;
        free(captured);
        captured = nextCaptured;
    }

    gRegistry.firstEntry = NULL;
    gRegistry.lastEntry = NULL;
    gRegistry.entryCount = 0;
    gDefaultComponentCount = 0;
    gCapturedComponents = NULL;

    gRegistry.initialized = false;

    UnlockComponentMutex(gRegistry.registryMutex);
    DestroyComponentMutex(gRegistry.registryMutex);
    gRegistry.registryMutex = NULL;
}

/*
 * RegisterComponentInternal
 *
 * Internal component registration function.
 */
Component RegisterComponentInternal(ComponentDescription* cd, ComponentRoutine entryPoint,
                                   int16_t global, Handle name, Handle info, Handle icon,
                                   bool isResourceBased, int16_t resFileRefNum)
{
    ComponentRegistryEntry* entry;
    Component component;

    if (!cd) {
        return NULL;
    }

    LockComponentMutex(gRegistry.registryMutex);

    /* Allocate new registry entry */
    entry = AllocateRegistryEntry();
    if (!entry) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return NULL;
    }

    /* Fill in entry data */
    entry->description = *cd;
    entry->entryPoint = entryPoint;
    entry->name = name;
    entry->info = info;
    entry->icon = icon;
    entry->version = 0;
    entry->refcon = 0;
    entry->instanceCount = 0;
    entry->registrationFlags = 0;
    entry->isGlobal = (global != 0);
    entry->wantsRegisterMessage = (cd->componentFlags & cmpWantsRegisterMessage) != 0;
    entry->isResourceBased = isResourceBased;
    entry->resFileRefNum = resFileRefNum;
    entry->moduleHandle = NULL;
    entry->modulePath = NULL;

    /* Generate component handle */
    component = (Component)GenerateComponentHandle(entry);
    if (!component) {
        FreeRegistryEntry(entry);
        UnlockComponentMutex(gRegistry.registryMutex);
        return NULL;
    }

    /* Add to registry */
    AddEntryToRegistry(entry);
    IncrementModificationSeed();

    UnlockComponentMutex(gRegistry.registryMutex);

    return component;
}

/*
 * UnregisterComponentInternal
 *
 * Internal component unregistration function.
 */
OSErr UnregisterComponentInternal(Component component)
{
    ComponentRegistryEntry* entry;

    if (!component) {
        return badComponentInstance;
    }

    LockComponentMutex(gRegistry.registryMutex);

    entry = FindEntryByHandle(component);
    if (!entry) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return badComponentInstance;
    }

    /* Can't unregister if there are open instances */
    if (entry->instanceCount > 0) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return badComponentInstance;
    }

    /* Remove from registry */
    RemoveEntryFromRegistry(entry);

    /* Unload module if loaded */
    if (entry->moduleHandle) {
        UnloadComponentModule(entry);
    }

    /* Free the entry */
    FreeRegistryEntry(entry);

    IncrementModificationSeed();

    UnlockComponentMutex(gRegistry.registryMutex);

    return noErr;
}

/*
 * FindFirstComponent
 *
 * Find the first component matching the given description.
 */
Component FindFirstComponent(ComponentDescription* looking)
{
    ComponentRegistryEntry* entry;
    Component found = NULL;

    if (!looking) {
        return NULL;
    }

    LockComponentMutex(gRegistry.registryMutex);

    entry = gRegistry.firstEntry;
    while (entry) {
        if (DescriptionMatches(&entry->description, looking)) {
            found = (Component)GenerateComponentHandle(entry);
            break;
        }
        entry = entry->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return found;
}

/*
 * FindNextComponentInternal
 *
 * Find the next component after the given one matching the description.
 */
Component FindNextComponentInternal(Component previous, ComponentDescription* looking)
{
    ComponentRegistryEntry* entry;
    ComponentRegistryEntry* startEntry;
    Component found = NULL;
    bool foundPrevious = false;

    if (!previous || !looking) {
        return NULL;
    }

    LockComponentMutex(gRegistry.registryMutex);

    startEntry = FindEntryByHandle(previous);
    if (!startEntry) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return NULL;
    }

    /* Start searching from the next entry after the previous one */
    entry = startEntry->next;
    while (entry) {
        if (DescriptionMatches(&entry->description, looking)) {
            found = (Component)GenerateComponentHandle(entry);
            break;
        }
        entry = entry->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return found;
}

/*
 * CountComponentsInternal
 *
 * Count components matching the given description.
 */
int32_t CountComponentsInternal(ComponentDescription* looking)
{
    ComponentRegistryEntry* entry;
    int32_t count = 0;

    if (!looking) {
        return 0;
    }

    LockComponentMutex(gRegistry.registryMutex);

    entry = gRegistry.firstEntry;
    while (entry) {
        if (DescriptionMatches(&entry->description, looking)) {
            count++;
        }
        entry = entry->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return count;
}

/*
 * GetComponentInfoInternal
 *
 * Get information about a component.
 */
OSErr GetComponentInfoInternal(Component component, ComponentDescription* cd,
                              Handle componentName, Handle componentInfo, Handle componentIcon)
{
    ComponentRegistryEntry* entry;

    if (!component) {
        return badComponentInstance;
    }

    LockComponentMutex(gRegistry.registryMutex);

    entry = FindEntryByHandle(component);
    if (!entry) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return badComponentInstance;
    }

    /* Copy description if requested */
    if (cd) {
        *cd = entry->description;
    }

    /* Copy handles if requested and available */
    if (componentName && entry->name) {
        *componentName = entry->name;
    }
    if (componentInfo && entry->info) {
        *componentInfo = entry->info;
    }
    if (componentIcon && entry->icon) {
        *componentIcon = entry->icon;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return noErr;
}

/*
 * GetComponentEntry
 *
 * Get the registry entry for a component.
 */
ComponentRegistryEntry* GetComponentEntry(Component component)
{
    ComponentRegistryEntry* entry;

    if (!component) {
        return NULL;
    }

    LockComponentMutex(gRegistry.registryMutex);
    entry = FindEntryByHandle(component);
    UnlockComponentMutex(gRegistry.registryMutex);

    return entry;
}

/*
 * ValidateComponent
 *
 * Validate that a component handle is valid.
 */
bool ValidateComponent(Component component)
{
    ComponentRegistryEntry* entry;
    bool valid = false;

    if (!component) {
        return false;
    }

    LockComponentMutex(gRegistry.registryMutex);
    entry = FindEntryByHandle(component);
    valid = (entry != NULL);
    UnlockComponentMutex(gRegistry.registryMutex);

    return valid;
}

/*
 * GetRegistryModificationSeed
 *
 * Get the registry modification seed.
 */
int32_t GetRegistryModificationSeed(void)
{
    return gRegistry.modificationSeed;
}

/*
 * InvalidateRegistryCache
 *
 * Invalidate any cached registry data.
 */
void InvalidateRegistryCache(void)
{
    IncrementModificationSeed();
}

/*
 * SetDefaultComponentInternal
 *
 * Set a component as the default for its type.
 */
OSErr SetDefaultComponentInternal(Component component, int16_t flags)
{
    ComponentRegistryEntry* entry;
    int32_t i;

    if (!component) {
        return badComponentInstance;
    }

    LockComponentMutex(gRegistry.registryMutex);

    entry = FindEntryByHandle(component);
    if (!entry) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return badComponentInstance;
    }

    /* Look for existing default for this type/subtype combination */
    for (i = 0; i < gDefaultComponentCount; i++) {
        if ((flags & defaultComponentAnySubType) ||
            gDefaultComponents[i].componentSubType == entry->description.componentSubType) {

            if ((flags & defaultComponentAnyManufacturer) ||
                gDefaultComponents[i].componentType == entry->description.componentType) {
                /* Replace existing default */
                gDefaultComponents[i].defaultComponent = component;
                UnlockComponentMutex(gRegistry.registryMutex);
                return noErr;
            }
        }
    }

    /* Add new default if space available */
    if (gDefaultComponentCount < 32) {
        gDefaultComponents[gDefaultComponentCount].componentType = entry->description.componentType;
        gDefaultComponents[gDefaultComponentCount].componentSubType = entry->description.componentSubType;
        gDefaultComponents[gDefaultComponentCount].defaultComponent = component;
        gDefaultComponentCount++;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return noErr;
}

/*
 * GetDefaultComponent
 *
 * Get the default component for a type/subtype.
 */
Component GetDefaultComponent(OSType componentType, OSType componentSubType)
{
    int32_t i;
    Component defaultComp = NULL;

    LockComponentMutex(gRegistry.registryMutex);

    for (i = 0; i < gDefaultComponentCount; i++) {
        if (gDefaultComponents[i].componentType == componentType &&
            (gDefaultComponents[i].componentSubType == componentSubType ||
             componentSubType == kAnyComponentSubType)) {
            defaultComp = gDefaultComponents[i].defaultComponent;
            break;
        }
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return defaultComp;
}

/*
 * CaptureComponentInternal
 *
 * Capture a component for delegation.
 */
OSErr CaptureComponentInternal(Component capturedComponent, Component capturingComponent)
{
    CapturedComponent* capture;

    if (!capturedComponent || !capturingComponent) {
        return badComponentInstance;
    }

    LockComponentMutex(gRegistry.registryMutex);

    /* Check if already captured */
    capture = gCapturedComponents;
    while (capture) {
        if (capture->captured == capturedComponent) {
            capture->capturing = capturingComponent;
            UnlockComponentMutex(gRegistry.registryMutex);
            return noErr;
        }
        capture = capture->next;
    }

    /* Create new capture entry */
    capture = (CapturedComponent*)malloc(sizeof(CapturedComponent));
    if (!capture) {
        UnlockComponentMutex(gRegistry.registryMutex);
        return componentDllLoadErr;
    }

    capture->captured = capturedComponent;
    capture->capturing = capturingComponent;
    capture->next = gCapturedComponents;
    gCapturedComponents = capture;

    UnlockComponentMutex(gRegistry.registryMutex);

    return noErr;
}

/*
 * UncaptureComponentInternal
 *
 * Release a captured component.
 */
OSErr UncaptureComponentInternal(Component component)
{
    CapturedComponent* capture;
    CapturedComponent* prev = NULL;

    if (!component) {
        return badComponentInstance;
    }

    LockComponentMutex(gRegistry.registryMutex);

    capture = gCapturedComponents;
    while (capture) {
        if (capture->captured == component) {
            if (prev) {
                prev->next = capture->next;
            } else {
                gCapturedComponents = capture->next;
            }
            free(capture);
            UnlockComponentMutex(gRegistry.registryMutex);
            return noErr;
        }
        prev = capture;
        capture = capture->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return badComponentInstance;
}

/*
 * GetCapturedComponent
 *
 * Get the captured component for delegation.
 */
Component GetCapturedComponent(Component component)
{
    CapturedComponent* capture;
    Component captured = NULL;

    if (!component) {
        return NULL;
    }

    LockComponentMutex(gRegistry.registryMutex);

    capture = gCapturedComponents;
    while (capture) {
        if (capture->capturing == component) {
            captured = capture->captured;
            break;
        }
        capture = capture->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return captured;
}

/*
 * GetCapturingComponent
 *
 * Get the capturing component for a captured component.
 */
Component GetCapturingComponent(Component component)
{
    CapturedComponent* capture;
    Component capturing = NULL;

    if (!component) {
        return NULL;
    }

    LockComponentMutex(gRegistry.registryMutex);

    capture = gCapturedComponents;
    while (capture) {
        if (capture->captured == component) {
            capturing = capture->capturing;
            break;
        }
        capture = capture->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);

    return capturing;
}

/*
 * IterateRegistry
 *
 * Iterate through all registry entries.
 */
void IterateRegistry(ComponentRegistryIteratorFunc iterator, void* userData)
{
    ComponentRegistryEntry* entry;

    if (!iterator) {
        return;
    }

    LockComponentMutex(gRegistry.registryMutex);

    entry = gRegistry.firstEntry;
    while (entry) {
        if (!iterator(entry, userData)) {
            break;
        }
        entry = entry->next;
    }

    UnlockComponentMutex(gRegistry.registryMutex);
}

/* Private functions */

/*
 * AllocateRegistryEntry
 *
 * Allocate a new registry entry.
 */
static ComponentRegistryEntry* AllocateRegistryEntry(void)
{
    ComponentRegistryEntry* entry;

    entry = (ComponentRegistryEntry*)calloc(1, sizeof(ComponentRegistryEntry));
    if (entry) {
        entry->modificationSeed = gRegistry.modificationSeed;
    }

    return entry;
}

/*
 * FreeRegistryEntry
 *
 * Free a registry entry and its resources.
 */
static void FreeRegistryEntry(ComponentRegistryEntry* entry)
{
    if (!entry) {
        return;
    }

    /* Free allocated strings */
    if (entry->modulePath) {
        free(entry->modulePath);
    }

    /* Note: Handles (name, info, icon) are typically owned by the caller
     * and should not be freed here unless we're managing them */

    free(entry);
}

/*
 * GenerateComponentHandle
 *
 * Generate a component handle from a registry entry.
 */
static uint32_t GenerateComponentHandle(ComponentRegistryEntry* entry)
{
    /* Simple implementation: use the entry pointer as the handle */
    return (uint32_t)(uintptr_t)entry;
}

/*
 * FindEntryByHandle
 *
 * Find a registry entry by its component handle.
 */
static ComponentRegistryEntry* FindEntryByHandle(Component component)
{
    /* Simple implementation: treat handle as entry pointer */
    return (ComponentRegistryEntry*)(uintptr_t)component;
}

/*
 * AddEntryToRegistry
 *
 * Add an entry to the registry linked list.
 */
static void AddEntryToRegistry(ComponentRegistryEntry* entry)
{
    if (!entry) {
        return;
    }

    entry->next = NULL;

    if (gRegistry.lastEntry) {
        gRegistry.lastEntry->next = entry;
        gRegistry.lastEntry = entry;
    } else {
        gRegistry.firstEntry = gRegistry.lastEntry = entry;
    }

    gRegistry.entryCount++;
}

/*
 * RemoveEntryFromRegistry
 *
 * Remove an entry from the registry linked list.
 */
static void RemoveEntryFromRegistry(ComponentRegistryEntry* entry)
{
    ComponentRegistryEntry* current;
    ComponentRegistryEntry* prev = NULL;

    if (!entry) {
        return;
    }

    current = gRegistry.firstEntry;
    while (current) {
        if (current == entry) {
            if (prev) {
                prev->next = current->next;
            } else {
                gRegistry.firstEntry = current->next;
            }

            if (current == gRegistry.lastEntry) {
                gRegistry.lastEntry = prev;
            }

            gRegistry.entryCount--;
            break;
        }
        prev = current;
        current = current->next;
    }
}

/*
 * DescriptionMatches
 *
 * Check if a component description matches search criteria.
 */
static bool DescriptionMatches(ComponentDescription* entry, ComponentDescription* looking)
{
    /* Check component type */
    if (looking->componentType != kAnyComponentType &&
        looking->componentType != entry->componentType) {
        return false;
    }

    /* Check component subtype */
    if (looking->componentSubType != kAnyComponentSubType &&
        looking->componentSubType != entry->componentSubType) {
        return false;
    }

    /* Check manufacturer */
    if (looking->componentManufacturer != kAnyComponentManufacturer &&
        looking->componentManufacturer != entry->componentManufacturer) {
        return false;
    }

    /* Check flags mask */
    if (looking->componentFlagsMask != kAnyComponentFlagsMask) {
        if ((entry->componentFlags & looking->componentFlagsMask) != looking->componentFlags) {
            return false;
        }
    }

    return true;
}

/*
 * IncrementModificationSeed
 *
 * Increment the registry modification seed.
 */
static void IncrementModificationSeed(void)
{
    gRegistry.modificationSeed++;
    if (gRegistry.modificationSeed == 0) {
        gRegistry.modificationSeed = 1;
    }
}
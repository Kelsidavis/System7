/*
 * ComponentInstances.c
 *
 * Component Instance Management Implementation - System 7.1 Portable
 * Manages component instance lifecycle, storage, and properties
 */

#include "ComponentInstances.h"
#include "ComponentRegistry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Instance management globals */
static struct {
    ComponentInstanceData* firstInstance;
    ComponentInstanceData* lastInstance;
    uint32_t instanceCount;
    ComponentMutex* instancesMutex;
    bool initialized;
} gInstanceManager = {0};

/* Forward declarations */
static ComponentInstanceData* AllocateInstanceData(void);
static void FreeInstanceData(ComponentInstanceData* instance);
static uint32_t GenerateInstanceHandle(ComponentInstanceData* instance);
static ComponentInstanceData* FindInstanceByHandle(ComponentInstance instance);
static void AddInstanceToList(ComponentInstanceData* instance);
static void RemoveInstanceFromList(ComponentInstanceData* instance);

/*
 * InitComponentInstances
 *
 * Initialize the component instance manager.
 */
OSErr InitComponentInstances(void)
{
    OSErr err;

    if (gInstanceManager.initialized) {
        return noErr;
    }

    memset(&gInstanceManager, 0, sizeof(gInstanceManager));

    /* Create instances mutex */
    err = CreateComponentMutex(&gInstanceManager.instancesMutex);
    if (err != noErr) {
        return err;
    }

    gInstanceManager.initialized = true;

    return noErr;
}

/*
 * CleanupComponentInstances
 *
 * Clean up the component instance manager.
 */
void CleanupComponentInstances(void)
{
    ComponentInstanceData* instance;
    ComponentInstanceData* next;

    if (!gInstanceManager.initialized) {
        return;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    /* Free all instances */
    instance = gInstanceManager.firstInstance;
    while (instance) {
        next = instance->next;
        FreeInstanceData(instance);
        instance = next;
    }

    gInstanceManager.firstInstance = NULL;
    gInstanceManager.lastInstance = NULL;
    gInstanceManager.instanceCount = 0;
    gInstanceManager.initialized = false;

    UnlockComponentMutex(gInstanceManager.instancesMutex);
    DestroyComponentMutex(gInstanceManager.instancesMutex);
    gInstanceManager.instancesMutex = NULL;
}

/*
 * CreateComponentInstance
 *
 * Create a new component instance.
 */
ComponentInstance CreateComponentInstance(ComponentRegistryEntry* component)
{
    ComponentInstanceData* instance;
    ComponentInstance handle;

    if (!component) {
        return NULL;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    /* Allocate instance data */
    instance = AllocateInstanceData();
    if (!instance) {
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        return NULL;
    }

    /* Initialize instance */
    instance->component = component;
    instance->storage = NULL;
    instance->target = NULL;
    instance->a5World = 0;
    instance->refcon = 0;
    instance->lastError = noErr;
    instance->flags = kComponentInstanceFlagValid;
    instance->openCount = 1;
    instance->userData = NULL;
    instance->mutex = NULL;
    instance->creationTime = (uint64_t)time(NULL);
    instance->lastAccessTime = instance->creationTime;

    /* Create instance mutex if thread safety is required */
    if (CreateComponentMutex(&instance->mutex) != noErr) {
        FreeInstanceData(instance);
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        return NULL;
    }

    /* Generate handle */
    handle = (ComponentInstance)GenerateInstanceHandle(instance);
    if (!handle) {
        FreeInstanceData(instance);
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        return NULL;
    }

    /* Add to instance list */
    AddInstanceToList(instance);

    /* Increment component instance count */
    component->instanceCount++;

    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return handle;
}

/*
 * DestroyComponentInstance
 *
 * Destroy a component instance.
 */
OSErr DestroyComponentInstance(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;

    if (!instance) {
        return badComponentInstance;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    instanceData = FindInstanceByHandle(instance);
    if (!instanceData) {
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        return badComponentInstance;
    }

    /* Check if instance is still open */
    if (instanceData->openCount > 0) {
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        return badComponentInstance;
    }

    /* Remove from instance list */
    RemoveInstanceFromList(instanceData);

    /* Decrement component instance count */
    if (instanceData->component) {
        instanceData->component->instanceCount--;
    }

    /* Free instance data */
    FreeInstanceData(instanceData);

    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return noErr;
}

/*
 * ValidateComponentInstance
 *
 * Validate that a component instance is valid.
 */
OSErr ValidateComponentInstance(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    bool valid = false;

    if (!instance) {
        return badComponentInstance;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    instanceData = FindInstanceByHandle(instance);
    if (instanceData && (instanceData->flags & kComponentInstanceFlagValid)) {
        valid = true;
    }

    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return valid ? noErr : badComponentInstance;
}

/*
 * OpenComponentInstance
 *
 * Open a component instance.
 */
ComponentInstance OpenComponentInstance(Component component)
{
    ComponentRegistryEntry* entry;
    ComponentInstance instance;

    if (!component) {
        return NULL;
    }

    /* Get component entry */
    entry = GetComponentEntry(component);
    if (!entry) {
        return NULL;
    }

    /* Load component if necessary */
    if (!entry->entryPoint && entry->modulePath) {
        if (LoadComponentModule(entry) != noErr) {
            return NULL;
        }
    }

    /* Create instance */
    instance = CreateComponentInstance(entry);
    if (!instance) {
        return NULL;
    }

    /* Send open message to component */
    if (entry->entryPoint) {
        ComponentParameters params;
        params.flags = 0;
        params.paramSize = 4;
        params.what = kComponentOpenSelect;
        params.params[0] = (int32_t)instance;

        ComponentResult result = entry->entryPoint(&params, GetInstanceStorage(instance));
        if (result != noErr) {
            SetInstanceError(instance, result);
        }
    }

    return instance;
}

/*
 * CloseComponentInstance
 *
 * Close a component instance.
 */
OSErr CloseComponentInstance(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    ComponentResult result = noErr;

    if (!instance) {
        return badComponentInstance;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    instanceData = FindInstanceByHandle(instance);
    if (!instanceData) {
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        return badComponentInstance;
    }

    /* Mark as closing */
    instanceData->flags |= kComponentInstanceFlagClosing;

    /* Send close message to component */
    if (instanceData->component && instanceData->component->entryPoint) {
        ComponentParameters params;
        params.flags = 0;
        params.paramSize = 4;
        params.what = kComponentCloseSelect;
        params.params[0] = (int32_t)instance;

        result = instanceData->component->entryPoint(&params, instanceData->storage);
    }

    /* Decrement open count */
    instanceData->openCount--;

    /* Destroy instance if no longer referenced */
    if (instanceData->openCount <= 0) {
        UnlockComponentMutex(gInstanceManager.instancesMutex);
        DestroyComponentInstance(instance);
        return result;
    }

    instanceData->flags &= ~kComponentInstanceFlagClosing;

    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return result;
}

/*
 * GetComponentInstanceData
 *
 * Get the instance data for a component instance.
 */
ComponentInstanceData* GetComponentInstanceData(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;

    if (!instance) {
        return NULL;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return instanceData;
}

/*
 * GetInstanceComponent
 *
 * Get the component for an instance.
 */
ComponentRegistryEntry* GetInstanceComponent(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    ComponentRegistryEntry* component = NULL;

    if (!instance) {
        return NULL;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        component = instanceData->component;
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return component;
}

/*
 * GetInstanceStorage
 *
 * Get the storage handle for an instance.
 */
Handle GetInstanceStorage(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    Handle storage = NULL;

    if (!instance) {
        return NULL;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        storage = instanceData->storage;
        instanceData->lastAccessTime = (uint64_t)time(NULL);
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return storage;
}

/*
 * SetInstanceStorage
 *
 * Set the storage handle for an instance.
 */
OSErr SetInstanceStorage(ComponentInstance instance, Handle storage)
{
    ComponentInstanceData* instanceData;

    if (!instance) {
        return badComponentInstance;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        instanceData->storage = storage;
        instanceData->lastAccessTime = (uint64_t)time(NULL);
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return instanceData ? noErr : badComponentInstance;
}

/*
 * GetInstanceError
 *
 * Get the last error for an instance.
 */
OSErr GetInstanceError(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    OSErr error = badComponentInstance;

    if (!instance) {
        return badComponentInstance;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        error = instanceData->lastError;
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return error;
}

/*
 * SetInstanceError
 *
 * Set the error for an instance.
 */
void SetInstanceError(ComponentInstance instance, OSErr error)
{
    ComponentInstanceData* instanceData;

    if (!instance) {
        return;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        instanceData->lastError = error;
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);
}

/*
 * GetInstanceA5
 *
 * Get the A5 world for an instance (68k compatibility).
 */
int32_t GetInstanceA5(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    int32_t a5 = 0;

    if (!instance) {
        return 0;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        a5 = instanceData->a5World;
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return a5;
}

/*
 * SetInstanceA5
 *
 * Set the A5 world for an instance (68k compatibility).
 */
void SetInstanceA5(ComponentInstance instance, int32_t a5)
{
    ComponentInstanceData* instanceData;

    if (!instance) {
        return;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        instanceData->a5World = a5;
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);
}

/*
 * GetInstanceTarget
 *
 * Get the delegation target for an instance.
 */
ComponentInstance GetInstanceTarget(ComponentInstance instance)
{
    ComponentInstanceData* instanceData;
    ComponentInstance target = NULL;

    if (!instance) {
        return NULL;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        target = instanceData->target;
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return target;
}

/*
 * SetInstanceTarget
 *
 * Set the delegation target for an instance.
 */
OSErr SetInstanceTarget(ComponentInstance instance, ComponentInstance target)
{
    ComponentInstanceData* instanceData;

    if (!instance) {
        return badComponentInstance;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);
    instanceData = FindInstanceByHandle(instance);
    if (instanceData) {
        instanceData->target = target;
        if (target) {
            instanceData->flags |= kComponentInstanceFlagDelegate;
        } else {
            instanceData->flags &= ~kComponentInstanceFlagDelegate;
        }
    }
    UnlockComponentMutex(gInstanceManager.instancesMutex);

    return instanceData ? noErr : badComponentInstance;
}

/*
 * CountInstances
 *
 * Count the number of instances for a component.
 */
int32_t CountInstances(Component component)
{
    ComponentRegistryEntry* entry;
    int32_t count = 0;

    if (!component) {
        return 0;
    }

    entry = GetComponentEntry(component);
    if (entry) {
        count = entry->instanceCount;
    }

    return count;
}

/*
 * IterateInstances
 *
 * Iterate through instances of a specific component.
 */
void IterateInstances(Component component, InstanceIteratorFunc iterator, void* userData)
{
    ComponentInstanceData* instance;
    ComponentRegistryEntry* entry;

    if (!component || !iterator) {
        return;
    }

    entry = GetComponentEntry(component);
    if (!entry) {
        return;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    instance = gInstanceManager.firstInstance;
    while (instance) {
        if (instance->component == entry) {
            ComponentInstance handle = (ComponentInstance)GenerateInstanceHandle(instance);
            if (!iterator(handle, userData)) {
                break;
            }
        }
        instance = instance->next;
    }

    UnlockComponentMutex(gInstanceManager.instancesMutex);
}

/*
 * IterateAllInstances
 *
 * Iterate through all instances.
 */
void IterateAllInstances(InstanceIteratorFunc iterator, void* userData)
{
    ComponentInstanceData* instance;

    if (!iterator) {
        return;
    }

    LockComponentMutex(gInstanceManager.instancesMutex);

    instance = gInstanceManager.firstInstance;
    while (instance) {
        ComponentInstance handle = (ComponentInstance)GenerateInstanceHandle(instance);
        if (!iterator(handle, userData)) {
            break;
        }
        instance = instance->next;
    }

    UnlockComponentMutex(gInstanceManager.instancesMutex);
}

/* Private functions */

/*
 * AllocateInstanceData
 *
 * Allocate a new instance data structure.
 */
static ComponentInstanceData* AllocateInstanceData(void)
{
    return (ComponentInstanceData*)calloc(1, sizeof(ComponentInstanceData));
}

/*
 * FreeInstanceData
 *
 * Free an instance data structure.
 */
static void FreeInstanceData(ComponentInstanceData* instance)
{
    if (!instance) {
        return;
    }

    /* Destroy instance mutex */
    if (instance->mutex) {
        DestroyComponentMutex(instance->mutex);
    }

    /* Note: Storage handle is typically owned by the component
     * and should be freed by the component's close routine */

    free(instance);
}

/*
 * GenerateInstanceHandle
 *
 * Generate an instance handle from instance data.
 */
static uint32_t GenerateInstanceHandle(ComponentInstanceData* instance)
{
    /* Simple implementation: use the instance pointer as the handle */
    return (uint32_t)(uintptr_t)instance;
}

/*
 * FindInstanceByHandle
 *
 * Find instance data by its handle.
 */
static ComponentInstanceData* FindInstanceByHandle(ComponentInstance instance)
{
    /* Simple implementation: treat handle as instance pointer */
    return (ComponentInstanceData*)(uintptr_t)instance;
}

/*
 * AddInstanceToList
 *
 * Add an instance to the instance list.
 */
static void AddInstanceToList(ComponentInstanceData* instance)
{
    if (!instance) {
        return;
    }

    instance->next = NULL;

    if (gInstanceManager.lastInstance) {
        gInstanceManager.lastInstance->next = instance;
        gInstanceManager.lastInstance = instance;
    } else {
        gInstanceManager.firstInstance = gInstanceManager.lastInstance = instance;
    }

    gInstanceManager.instanceCount++;
}

/*
 * RemoveInstanceFromList
 *
 * Remove an instance from the instance list.
 */
static void RemoveInstanceFromList(ComponentInstanceData* instance)
{
    ComponentInstanceData* current;
    ComponentInstanceData* prev = NULL;

    if (!instance) {
        return;
    }

    current = gInstanceManager.firstInstance;
    while (current) {
        if (current == instance) {
            if (prev) {
                prev->next = current->next;
            } else {
                gInstanceManager.firstInstance = current->next;
            }

            if (current == gInstanceManager.lastInstance) {
                gInstanceManager.lastInstance = prev;
            }

            gInstanceManager.instanceCount--;
            break;
        }
        prev = current;
        current = current->next;
    }
}
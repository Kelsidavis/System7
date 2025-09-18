/*
 * ComponentManagerCore.c
 *
 * Core Component Manager Implementation - System 7.1 Portable
 * Provides the main Component Manager API and initialization
 *
 * This module implements the core functionality of the Apple Macintosh
 * Component Manager from System 7.1, adapted for modern platforms.
 */

#include "ComponentManager.h"
#include "ComponentRegistry.h"
#include "ComponentLoader.h"
#include "ComponentDispatch.h"
#include "ComponentInstances.h"
#include "ComponentResources.h"
#include "ComponentNegotiation.h"
#include "ComponentSecurity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Global Component Manager state */
typedef struct ComponentManagerGlobals {
    bool initialized;
    uint32_t initializationTime;
    uint32_t listModificationSeed;
    Component defaultComponents[32];        /* Default components by type */
    int32_t defaultComponentCount;
    ComponentMutex* globalMutex;
    bool securityEnabled;
    ComponentSecurityLevel securityLevel;
} ComponentManagerGlobals;

static ComponentManagerGlobals gComponentMgrGlobals = {0};

/* Forward declarations */
static OSErr InitializeComponentManagerSubsystems(void);
static void CleanupComponentManagerSubsystems(void);
static void IncrementListModificationSeed(void);

/*
 * InitComponentManager
 *
 * Initialize the Component Manager and all its subsystems.
 */
OSErr InitComponentManager(void)
{
    OSErr err = noErr;

    if (gComponentMgrGlobals.initialized) {
        return noErr; /* Already initialized */
    }

    /* Clear globals */
    memset(&gComponentMgrGlobals, 0, sizeof(ComponentManagerGlobals));

    /* Create global mutex */
    err = CreateComponentMutex(&gComponentMgrGlobals.globalMutex);
    if (err != noErr) {
        return err;
    }

    /* Initialize subsystems */
    err = InitializeComponentManagerSubsystems();
    if (err != noErr) {
        CleanupComponentManagerSubsystems();
        return err;
    }

    /* Set initialization time and mark as initialized */
    gComponentMgrGlobals.initializationTime = (uint32_t)time(NULL);
    gComponentMgrGlobals.listModificationSeed = 1;
    gComponentMgrGlobals.initialized = true;

    return noErr;
}

/*
 * CleanupComponentManager
 *
 * Clean up the Component Manager and all its subsystems.
 */
void CleanupComponentManager(void)
{
    if (!gComponentMgrGlobals.initialized) {
        return;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);

    /* Clean up subsystems */
    CleanupComponentManagerSubsystems();

    /* Clear default components */
    memset(gComponentMgrGlobals.defaultComponents, 0, sizeof(gComponentMgrGlobals.defaultComponents));
    gComponentMgrGlobals.defaultComponentCount = 0;

    gComponentMgrGlobals.initialized = false;

    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);
    DestroyComponentMutex(gComponentMgrGlobals.globalMutex);
    gComponentMgrGlobals.globalMutex = NULL;
}

/*
 * RegisterComponent
 *
 * Register a component with the Component Manager.
 */
Component RegisterComponent(ComponentDescription* cd, ComponentRoutine componentEntryPoint,
                          int16_t global, Handle componentName, Handle componentInfo, Handle componentIcon)
{
    Component component;
    OSErr err;

    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    if (!cd || !componentEntryPoint) {
        return NULL;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);

    /* Register the component */
    component = RegisterComponentInternal(cd, componentEntryPoint, global,
                                        componentName, componentInfo, componentIcon,
                                        false, 0);

    if (component) {
        IncrementListModificationSeed();

        /* Send register message if component wants it */
        if (cd->componentFlags & cmpWantsRegisterMessage) {
            ComponentParameters params;
            params.flags = 0;
            params.paramSize = 0;
            params.what = kComponentRegisterSelect;

            /* Call component's register routine */
            componentEntryPoint(&params, NULL);
        }
    }

    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return component;
}

/*
 * RegisterComponentResource
 *
 * Register a component from a resource.
 */
Component RegisterComponentResource(Handle tr, int16_t global)
{
    Component component = NULL;
    ComponentResource* resource;
    OSErr err;

    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    if (!tr) {
        return NULL;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);

    /* Parse the component resource */
    err = ParseComponentResource(tr, &resource);
    if (err == noErr && resource) {
        /* Register the component */
        component = RegisterComponentInternal(&resource->cd, NULL, global,
                                            NULL, NULL, NULL, true, 0);

        if (component) {
            IncrementListModificationSeed();
        }

        free(resource);
    }

    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return component;
}

/*
 * UnregisterComponent
 *
 * Unregister a component from the Component Manager.
 */
OSErr UnregisterComponent(Component aComponent)
{
    OSErr err;
    ComponentRegistryEntry* entry;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return badComponentInstance;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);

    /* Get component entry */
    entry = GetComponentEntry(aComponent);
    if (!entry) {
        UnlockComponentMutex(gComponentMgrGlobals.globalMutex);
        return badComponentInstance;
    }

    /* Send unregister message if component wants it */
    if (entry->wantsRegisterMessage && entry->entryPoint) {
        ComponentParameters params;
        params.flags = 0;
        params.paramSize = 0;
        params.what = kComponentUnregisterSelect;

        entry->entryPoint(&params, NULL);
    }

    /* Unregister the component */
    err = UnregisterComponentInternal(aComponent);
    if (err == noErr) {
        IncrementListModificationSeed();
    }

    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return err;
}

/*
 * FindNextComponent
 *
 * Find the next component matching the given criteria.
 */
Component FindNextComponent(Component aComponent, ComponentDescription* looking)
{
    Component nextComponent;

    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);

    if (aComponent == NULL) {
        /* Find first component */
        nextComponent = FindFirstComponent(looking);
    } else {
        /* Find next component */
        nextComponent = FindNextComponentInternal(aComponent, looking);
    }

    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return nextComponent;
}

/*
 * CountComponents
 *
 * Count the number of components matching the given criteria.
 */
int32_t CountComponents(ComponentDescription* looking)
{
    int32_t count;

    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    count = CountComponentsInternal(looking);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return count;
}

/*
 * GetComponentInfo
 *
 * Get information about a component.
 */
OSErr GetComponentInfo(Component aComponent, ComponentDescription* cd,
                      Handle componentName, Handle componentInfo, Handle componentIcon)
{
    OSErr err;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return badComponentInstance;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = GetComponentInfoInternal(aComponent, cd, componentName, componentInfo, componentIcon);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return err;
}

/*
 * GetComponentListModSeed
 *
 * Get the modification seed for the component list.
 */
int32_t GetComponentListModSeed(void)
{
    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    return gComponentMgrGlobals.listModificationSeed;
}

/*
 * OpenComponent
 *
 * Open a component instance.
 */
ComponentInstance OpenComponent(Component aComponent)
{
    ComponentInstance instance;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return NULL;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    instance = OpenComponentInstance(aComponent);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return instance;
}

/*
 * CloseComponent
 *
 * Close a component instance.
 */
OSErr CloseComponent(ComponentInstance aComponentInstance)
{
    OSErr err;

    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return badComponentInstance;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = CloseComponentInstance(aComponentInstance);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return err;
}

/*
 * GetComponentInstanceError
 *
 * Get the last error for a component instance.
 */
OSErr GetComponentInstanceError(ComponentInstance aComponentInstance)
{
    OSErr err;

    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return badComponentInstance;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = GetInstanceError(aComponentInstance);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return err;
}

/*
 * SetComponentInstanceError
 *
 * Set the error for a component instance.
 */
void SetComponentInstanceError(ComponentInstance aComponentInstance, OSErr theError)
{
    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    SetInstanceError(aComponentInstance, theError);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);
}

/*
 * ComponentFunctionImplemented
 *
 * Check if a component implements a specific function.
 */
int32_t ComponentFunctionImplemented(ComponentInstance ci, int16_t ftnNumber)
{
    ComponentParameters params;
    ComponentResult result;

    if (!ci) {
        return 0;
    }

    params.flags = 0;
    params.paramSize = 2;
    params.what = kComponentCanDoSelect;
    params.params[0] = ftnNumber;

    result = ComponentDispatch(ci, &params);
    return (result != 0) ? 1 : 0;
}

/*
 * GetComponentVersion
 *
 * Get the version of a component.
 */
int32_t GetComponentVersion(ComponentInstance ci)
{
    ComponentParameters params;

    if (!ci) {
        return 0;
    }

    params.flags = 0;
    params.paramSize = 0;
    params.what = kComponentVersionSelect;

    return ComponentDispatch(ci, &params);
}

/*
 * ComponentSetTarget
 *
 * Set the target for component delegation.
 */
int32_t ComponentSetTarget(ComponentInstance ci, ComponentInstance target)
{
    ComponentParameters params;
    OSErr err;

    if (!ci) {
        return badComponentInstance;
    }

    params.flags = 0;
    params.paramSize = 4;
    params.what = kComponentTargetSelect;
    params.params[0] = (int32_t)target;

    /* Also set the target in the instance data */
    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = SetInstanceTarget(ci, target);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    if (err != noErr) {
        return err;
    }

    return ComponentDispatch(ci, &params);
}

/*
 * GetComponentRefcon
 *
 * Get the reference constant for a component.
 */
int32_t GetComponentRefcon(Component aComponent)
{
    ComponentRegistryEntry* entry;
    int32_t refcon = 0;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return 0;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    entry = GetComponentEntry(aComponent);
    if (entry) {
        refcon = entry->refcon;
    }
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return refcon;
}

/*
 * SetComponentRefcon
 *
 * Set the reference constant for a component.
 */
void SetComponentRefcon(Component aComponent, int32_t theRefcon)
{
    ComponentRegistryEntry* entry;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    entry = GetComponentEntry(aComponent);
    if (entry) {
        entry->refcon = theRefcon;
    }
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);
}

/*
 * OpenComponentResFile
 *
 * Open the resource file for a component.
 */
int16_t OpenComponentResFile(Component aComponent)
{
    ComponentRegistryEntry* entry;
    int16_t refNum = -1;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return -1;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    entry = GetComponentEntry(aComponent);
    if (entry && entry->isResourceBased) {
        refNum = entry->resFileRefNum;
    }
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return refNum;
}

/*
 * CloseComponentResFile
 *
 * Close a component resource file.
 */
OSErr CloseComponentResFile(int16_t refnum)
{
    if (!gComponentMgrGlobals.initialized) {
        return badComponentInstance;
    }

    return CloseComponentResourceFile(refnum);
}

/*
 * GetComponentInstanceStorage
 *
 * Get the storage handle for a component instance.
 */
Handle GetComponentInstanceStorage(ComponentInstance aComponentInstance)
{
    Handle storage = NULL;

    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return NULL;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    storage = GetInstanceStorage(aComponentInstance);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return storage;
}

/*
 * SetComponentInstanceStorage
 *
 * Set the storage handle for a component instance.
 */
void SetComponentInstanceStorage(ComponentInstance aComponentInstance, Handle theStorage)
{
    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    SetInstanceStorage(aComponentInstance, theStorage);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);
}

/*
 * GetComponentInstanceA5
 *
 * Get the A5 world for a component instance (68k compatibility).
 */
int32_t GetComponentInstanceA5(ComponentInstance aComponentInstance)
{
    int32_t a5 = 0;

    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return 0;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    a5 = GetInstanceA5(aComponentInstance);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return a5;
}

/*
 * SetComponentInstanceA5
 *
 * Set the A5 world for a component instance (68k compatibility).
 */
void SetComponentInstanceA5(ComponentInstance aComponentInstance, int32_t theA5)
{
    if (!gComponentMgrGlobals.initialized || !aComponentInstance) {
        return;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    SetInstanceA5(aComponentInstance, theA5);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);
}

/*
 * CountComponentInstances
 *
 * Count the number of instances for a component.
 */
int32_t CountComponentInstances(Component aComponent)
{
    int32_t count = 0;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return 0;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    count = CountInstances(aComponent);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return count;
}

/*
 * CallComponentFunction
 *
 * Call a component function with parameters.
 */
int32_t CallComponentFunction(ComponentParameters* params, ComponentFunction func)
{
    if (!params || !func) {
        return badComponentInstance;
    }

    return func();
}

/*
 * CallComponentFunctionWithStorage
 *
 * Call a component function with storage and parameters.
 */
int32_t CallComponentFunctionWithStorage(Handle storage, ComponentParameters* params, ComponentFunction func)
{
    if (!params || !func) {
        return badComponentInstance;
    }

    return func();
}

/*
 * DelegateComponentCall
 *
 * Delegate a component call to another component instance.
 */
int32_t DelegateComponentCall(ComponentParameters* originalParams, ComponentInstance ci)
{
    if (!originalParams || !ci) {
        return badComponentInstance;
    }

    return ComponentDispatch(ci, originalParams);
}

/*
 * SetDefaultComponent
 *
 * Set a component as the default for its type.
 */
OSErr SetDefaultComponent(Component aComponent, int16_t flags)
{
    OSErr err;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return badComponentInstance;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = SetDefaultComponentInternal(aComponent, flags);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return err;
}

/*
 * OpenDefaultComponent
 *
 * Open the default component for a given type and subtype.
 */
ComponentInstance OpenDefaultComponent(OSType componentType, OSType componentSubType)
{
    Component component;
    ComponentDescription desc;
    ComponentInstance instance = NULL;

    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    /* Set up description for search */
    desc.componentType = componentType;
    desc.componentSubType = componentSubType;
    desc.componentManufacturer = kAnyComponentManufacturer;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    LockComponentMutex(gComponentMgrGlobals.globalMutex);

    /* Try to get the default component */
    component = GetDefaultComponent(componentType, componentSubType);
    if (!component) {
        /* No default set, find first matching component */
        component = FindFirstComponent(&desc);
    }

    if (component) {
        instance = OpenComponentInstance(component);
    }

    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return instance;
}

/*
 * CaptureComponent
 *
 * Capture a component for delegation.
 */
Component CaptureComponent(Component capturedComponent, Component capturingComponent)
{
    OSErr err;

    if (!gComponentMgrGlobals.initialized || !capturedComponent || !capturingComponent) {
        return NULL;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = CaptureComponentInternal(capturedComponent, capturingComponent);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return (err == noErr) ? capturedComponent : NULL;
}

/*
 * UncaptureComponent
 *
 * Release a captured component.
 */
OSErr UncaptureComponent(Component aComponent)
{
    OSErr err;

    if (!gComponentMgrGlobals.initialized || !aComponent) {
        return badComponentInstance;
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = UncaptureComponentInternal(aComponent);
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return err;
}

/*
 * RegisterComponentResourceFile
 *
 * Register all components from a resource file.
 */
int32_t RegisterComponentResourceFile(int16_t resRefNum, int16_t global)
{
    OSErr err;
    int32_t count = 0;

    if (!gComponentMgrGlobals.initialized) {
        InitComponentManager();
    }

    LockComponentMutex(gComponentMgrGlobals.globalMutex);
    err = RegisterComponentsFromResourceFile(resRefNum, global);
    if (err == noErr) {
        IncrementListModificationSeed();
        /* Return number of components registered - this would need to be tracked */
        count = 1; /* Placeholder */
    }
    UnlockComponentMutex(gComponentMgrGlobals.globalMutex);

    return count;
}

/*
 * GetComponentIconSuite
 *
 * Get the icon suite for a component.
 */
OSErr GetComponentIconSuite(Component aComponent, Handle* iconSuite)
{
    if (!gComponentMgrGlobals.initialized || !aComponent || !iconSuite) {
        return badComponentInstance;
    }

    /* Implementation would extract icon suite from component resources */
    *iconSuite = NULL;
    return noErr;
}

/* Private functions */

/*
 * InitializeComponentManagerSubsystems
 *
 * Initialize all Component Manager subsystems.
 */
static OSErr InitializeComponentManagerSubsystems(void)
{
    OSErr err;

    /* Initialize subsystems in dependency order */
    err = InitComponentSecurity();
    if (err != noErr) return err;

    err = InitComponentResources();
    if (err != noErr) return err;

    err = InitComponentRegistry();
    if (err != noErr) return err;

    err = InitComponentLoader();
    if (err != noErr) return err;

    err = InitComponentInstances();
    if (err != noErr) return err;

    err = InitComponentDispatch();
    if (err != noErr) return err;

    err = InitComponentNegotiation();
    if (err != noErr) return err;

    return noErr;
}

/*
 * CleanupComponentManagerSubsystems
 *
 * Clean up all Component Manager subsystems.
 */
static void CleanupComponentManagerSubsystems(void)
{
    /* Clean up subsystems in reverse dependency order */
    CleanupComponentNegotiation();
    CleanupComponentDispatch();
    CleanupComponentInstances();
    CleanupComponentLoader();
    CleanupComponentRegistry();
    CleanupComponentResources();
    CleanupComponentSecurity();
}

/*
 * IncrementListModificationSeed
 *
 * Increment the component list modification seed.
 */
static void IncrementListModificationSeed(void)
{
    gComponentMgrGlobals.listModificationSeed++;
    if (gComponentMgrGlobals.listModificationSeed == 0) {
        gComponentMgrGlobals.listModificationSeed = 1;
    }
}
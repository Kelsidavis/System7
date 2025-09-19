/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * ComponentDispatch.c
 *
 * Component call dispatching and delegation
 * Handles routing calls to component instances
 *
 * Based on Mac OS 7.1 Component Manager
 */

#include "ComponentManager/ComponentDispatch.h"
#include "ComponentManager/ComponentInstances.h"
#include "ComponentManager/ComponentRegistry.h"
#include "ComponentManager/ComponentManager_HAL.h"
#include <stdlib.h>
#include <string.h>

/* Dispatch state */
typedef struct DispatchState {
    Boolean initialized;
    ComponentMutex* dispatchMutex;

    /* Performance tracking */
    struct DispatchStats {
        int32_t totalCalls;
        int32_t successfulCalls;
        int32_t failedCalls;
        int32_t delegatedCalls;
    } stats;

    /* Call stack for delegation */
    struct CallFrame {
        ComponentInstance instance;
        ComponentParameters* params;
        struct CallFrame* next;
    }* callStack;
} DispatchState;

static DispatchState g_dispatchState = {0};

/* ========================================================================
 * Initialization
 * ======================================================================== */

OSErr InitComponentDispatch(void) {
    if (g_dispatchState.initialized) {
        return noErr;
    }

    memset(&g_dispatchState, 0, sizeof(DispatchState));

    /* Create dispatch mutex */
    OSErr err = CreateComponentMutex(&g_dispatchState.dispatchMutex);
    if (err != noErr) {
        return err;
    }

    g_dispatchState.initialized = true;
    return noErr;
}

void CleanupComponentDispatch(void) {
    if (!g_dispatchState.initialized) {
        return;
    }

    LockComponentMutex(g_dispatchState.dispatchMutex);

    /* Clean up call stack */
    struct CallFrame* frame = g_dispatchState.callStack;
    while (frame) {
        struct CallFrame* next = frame->next;
        free(frame);
        frame = next;
    }
    g_dispatchState.callStack = NULL;

    g_dispatchState.initialized = false;

    UnlockComponentMutex(g_dispatchState.dispatchMutex);
    DestroyComponentMutex(g_dispatchState.dispatchMutex);
}

/* ========================================================================
 * Main Dispatch Function
 * ======================================================================== */

ComponentResult ComponentDispatch(ComponentInstance ci, ComponentParameters* params) {
    if (!ci || !params) {
        return badComponentInstance;
    }

    LockComponentMutex(g_dispatchState.dispatchMutex);
    g_dispatchState.stats.totalCalls++;

    /* Get instance data */
    ComponentInstanceData* instanceData = GetInstanceData(ci);
    if (!instanceData) {
        g_dispatchState.stats.failedCalls++;
        UnlockComponentMutex(g_dispatchState.dispatchMutex);
        return badComponentInstance;
    }

    /* Get component entry */
    ComponentRegistryEntry* entry = GetComponentEntry(instanceData->component);
    if (!entry || !entry->entryPoint) {
        g_dispatchState.stats.failedCalls++;
        UnlockComponentMutex(g_dispatchState.dispatchMutex);
        return badComponentSelector;
    }

    /* Push call frame for recursion detection */
    struct CallFrame* frame = malloc(sizeof(struct CallFrame));
    if (frame) {
        frame->instance = ci;
        frame->params = params;
        frame->next = g_dispatchState.callStack;
        g_dispatchState.callStack = frame;
    }

    /* Handle special selectors */
    ComponentResult result = 0;
    Handle storage = instanceData->storage;

    switch (params->what) {
        case kComponentOpenSelect:
            result = DispatchOpen(ci, entry->entryPoint, storage);
            break;

        case kComponentCloseSelect:
            result = DispatchClose(ci, entry->entryPoint, storage);
            break;

        case kComponentCanDoSelect:
            result = DispatchCanDo(ci, entry->entryPoint, storage, params);
            break;

        case kComponentVersionSelect:
            result = DispatchVersion(ci, entry->entryPoint, storage);
            break;

        case kComponentRegisterSelect:
            result = DispatchRegister(ci, entry->entryPoint, storage);
            break;

        case kComponentTargetSelect:
            result = DispatchTarget(ci, entry->entryPoint, storage, params);
            break;

        case kComponentUnregisterSelect:
            result = DispatchUnregister(ci, entry->entryPoint, storage);
            break;

        default:
            /* Regular component call */
            result = entry->entryPoint(params, storage);
            break;
    }

    /* Pop call frame */
    if (frame) {
        g_dispatchState.callStack = frame->next;
        free(frame);
    }

    /* Update stats */
    if (result == noErr) {
        g_dispatchState.stats.successfulCalls++;
    } else {
        g_dispatchState.stats.failedCalls++;
    }

    /* Store last error */
    SetInstanceError(ci, result);

    UnlockComponentMutex(g_dispatchState.dispatchMutex);
    return result;
}

/* ========================================================================
 * Special Selector Handlers
 * ======================================================================== */

ComponentResult DispatchOpen(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    ComponentParameters params;
    params.what = kComponentOpenSelect;
    params.paramSize = sizeof(ComponentInstance);
    params.params[0] = (int32_t)ci;

    return entryPoint(&params, storage);
}

ComponentResult DispatchClose(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    ComponentParameters params;
    params.what = kComponentCloseSelect;
    params.paramSize = sizeof(ComponentInstance);
    params.params[0] = (int32_t)ci;

    return entryPoint(&params, storage);
}

ComponentResult DispatchCanDo(ComponentInstance ci, ComponentRoutine entryPoint,
                              Handle storage, ComponentParameters* originalParams) {
    if (originalParams->paramSize < sizeof(int16_t)) {
        return 0;  /* Can't do */
    }

    int16_t selector = (int16_t)originalParams->params[0];

    /* Check standard selectors */
    switch (selector) {
        case kComponentOpenSelect:
        case kComponentCloseSelect:
        case kComponentCanDoSelect:
        case kComponentVersionSelect:
            return 1;  /* Always implemented */

        case kComponentRegisterSelect:
        case kComponentUnregisterSelect:
            /* Check if component wants these */
            {
                ComponentRegistryEntry* entry = GetComponentEntry(GetInstanceComponent(ci));
                if (entry && entry->wantsRegisterMessage) {
                    return 1;
                }
            }
            return 0;

        default:
            /* Ask the component */
            return entryPoint(originalParams, storage);
    }
}

ComponentResult DispatchVersion(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    ComponentParameters params;
    params.what = kComponentVersionSelect;
    params.paramSize = 0;

    ComponentResult result = entryPoint(&params, storage);

    /* If component doesn't implement version, return default */
    if (result == badComponentSelector) {
        return 0x01000000;  /* Version 1.0.0.0 */
    }

    return result;
}

ComponentResult DispatchRegister(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    ComponentParameters params;
    params.what = kComponentRegisterSelect;
    params.paramSize = 0;

    return entryPoint(&params, storage);
}

ComponentResult DispatchUnregister(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    ComponentParameters params;
    params.what = kComponentUnregisterSelect;
    params.paramSize = 0;

    return entryPoint(&params, storage);
}

ComponentResult DispatchTarget(ComponentInstance ci, ComponentRoutine entryPoint,
                               Handle storage, ComponentParameters* originalParams) {
    if (originalParams->paramSize < sizeof(ComponentInstance)) {
        return badComponentSelector;
    }

    ComponentInstance target = (ComponentInstance)originalParams->params[0];

    /* Set the target */
    OSErr err = SetInstanceTarget(ci, target);
    if (err != noErr) {
        return err;
    }

    /* Notify the component */
    return entryPoint(originalParams, storage);
}

/* ========================================================================
 * Delegation Support
 * ======================================================================== */

ComponentResult CallComponentTarget(ComponentInstance ci, ComponentParameters* params) {
    if (!ci || !params) {
        return badComponentInstance;
    }

    /* Get target instance */
    ComponentInstance target = GetInstanceTarget(ci);
    if (!target) {
        return badComponentInstance;
    }

    /* Check for delegation loops */
    struct CallFrame* frame = g_dispatchState.callStack;
    while (frame) {
        if (frame->instance == target && frame->params->what == params->what) {
            /* Delegation loop detected */
            return badComponentSelector;
        }
        frame = frame->next;
    }

    g_dispatchState.stats.delegatedCalls++;

    /* Dispatch to target */
    return ComponentDispatch(target, params);
}

ComponentResult CallNextComponent(ComponentInstance ci, ComponentParameters* params) {
    if (!ci || !params) {
        return badComponentInstance;
    }

    /* Find next component of same type */
    Component currentComp = GetInstanceComponent(ci);
    ComponentRegistryEntry* entry = GetComponentEntry(currentComp);
    if (!entry) {
        return badComponentInstance;
    }

    ComponentDescription desc = entry->description;
    Component nextComp = FindNextComponent(currentComp, &desc);
    if (!nextComp) {
        return badComponentSelector;
    }

    /* Open temporary instance */
    ComponentInstance nextInstance = OpenComponent(nextComp);
    if (!nextInstance) {
        return badComponentInstance;
    }

    /* Call the next component */
    ComponentResult result = ComponentDispatch(nextInstance, params);

    /* Close temporary instance */
    CloseComponent(nextInstance);

    return result;
}

/* ========================================================================
 * Fast Dispatch Functions
 * ======================================================================== */

ComponentResult ComponentDispatchFast(ComponentInstance ci, ComponentParameters* params) {
    /* Fast path without mutex for performance */
    ComponentInstanceData* instanceData = GetInstanceData(ci);
    if (!instanceData) {
        return badComponentInstance;
    }

    ComponentRegistryEntry* entry = GetComponentEntry(instanceData->component);
    if (!entry || !entry->entryPoint) {
        return badComponentSelector;
    }

    return entry->entryPoint(params, instanceData->storage);
}

/* ========================================================================
 * Statistics
 * ======================================================================== */

void GetDispatchStatistics(int32_t* totalCalls, int32_t* successfulCalls,
                          int32_t* failedCalls, int32_t* delegatedCalls) {
    LockComponentMutex(g_dispatchState.dispatchMutex);

    if (totalCalls) *totalCalls = g_dispatchState.stats.totalCalls;
    if (successfulCalls) *successfulCalls = g_dispatchState.stats.successfulCalls;
    if (failedCalls) *failedCalls = g_dispatchState.stats.failedCalls;
    if (delegatedCalls) *delegatedCalls = g_dispatchState.stats.delegatedCalls;

    UnlockComponentMutex(g_dispatchState.dispatchMutex);
}

void ResetDispatchStatistics(void) {
    LockComponentMutex(g_dispatchState.dispatchMutex);
    memset(&g_dispatchState.stats, 0, sizeof(g_dispatchState.stats));
    UnlockComponentMutex(g_dispatchState.dispatchMutex);
}
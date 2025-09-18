/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * EventHandlers.c
 *
 * Apple Event handler registration and dispatch system
 * Provides event routing, handler management, and dispatch table support
 *
 * Based on Mac OS 7.1 Apple Event handler system
 */

#include "AppleEventManager/EventHandlers.h"
#include "AppleEventManager/AppleEventManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

/* ========================================================================
 * External State Access
 * ======================================================================== */

/* Access to global manager state from AppleEventManagerCore.c */
extern struct AEMgrState {
    bool initialized;
    pthread_mutex_t mutex;
    const AppleEvent* currentEvent;
    AppleEvent* currentReply;
    bool eventSuspended;
    AEInteractAllowed interactionLevel;
    int32_t defaultTimeout;
    struct AEHandlerTableEntry* eventHandlers;
    struct AECoercionHandlerEntry* coercionHandlers;
    struct AESpecialHandlerEntry* specialHandlers;
    int32_t eventsProcessed;
    int32_t descriptorsCreated;
    int32_t handlersInstalled;
    int32_t totalHandles;
    Size totalMemoryAllocated;
} g_aeMgrState;

/* ========================================================================
 * Handler Management Internal State
 * ======================================================================== */

static struct {
    /* Pre/post dispatch hooks */
    AEPreDispatchProc preDispatchProc;
    void* preDispatchUserData;
    AEPostDispatchProc postDispatchProc;
    void* postDispatchUserData;

    /* Error handling */
    AEErrorHandlerProc errorHandler;
    void* errorUserData;

    /* Event filtering */
    AEEventFilterProc eventFilter;
    void* filterUserData;

    /* Statistics */
    AEHandlerStats stats;
    AEHandlerPerfInfo* perfInfo;
    int32_t perfInfoCount;
    int32_t perfInfoCapacity;

    /* Default handler */
    EventHandlerProcPtr defaultHandler;
    int32_t defaultHandlerRefcon;
} g_handlerState = {0};

/* ========================================================================
 * Event Handler Registration Functions
 * ======================================================================== */

OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    /* Check if handler already exists */
    pthread_mutex_lock(&g_aeMgrState.mutex);

    AEHandlerTableEntry* current = g_aeMgrState.eventHandlers;
    while (current) {
        if (current->eventClass == theAEEventClass &&
            current->eventID == theAEEventID &&
            current->isSystemHandler == isSysHandler) {
            /* Update existing handler */
            current->handler = handler;
            current->handlerRefcon = handlerRefcon;
            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = current->next;
    }

    /* Create new handler entry */
    AEHandlerTableEntry* newEntry = malloc(sizeof(AEHandlerTableEntry));
    if (!newEntry) {
        pthread_mutex_unlock(&g_aeMgrState.mutex);
        return memFullErr;
    }

    newEntry->eventClass = theAEEventClass;
    newEntry->eventID = theAEEventID;
    newEntry->handler = handler;
    newEntry->handlerRefcon = handlerRefcon;
    newEntry->isSystemHandler = isSysHandler;
    newEntry->next = g_aeMgrState.eventHandlers;

    g_aeMgrState.eventHandlers = newEntry;
    g_aeMgrState.handlersInstalled++;

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return noErr;
}

OSErr AERemoveEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AEHandlerTableEntry** current = &g_aeMgrState.eventHandlers;
    while (*current) {
        if ((*current)->eventClass == theAEEventClass &&
            (*current)->eventID == theAEEventID &&
            (*current)->handler == handler &&
            (*current)->isSystemHandler == isSysHandler) {

            AEHandlerTableEntry* toDelete = *current;
            *current = (*current)->next;
            free(toDelete);
            g_aeMgrState.handlersInstalled--;

            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = &(*current)->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return errAEHandlerNotFound;
}

OSErr AEGetEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr* handler, int32_t* handlerRefcon, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AEHandlerTableEntry* current = g_aeMgrState.eventHandlers;
    while (current) {
        if (current->eventClass == theAEEventClass &&
            current->eventID == theAEEventID &&
            current->isSystemHandler == isSysHandler) {

            *handler = current->handler;
            if (handlerRefcon) *handlerRefcon = current->handlerRefcon;

            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return errAEHandlerNotFound;
}

OSErr AEInstallEventHandler_Extended(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler, int32_t priority) {
    /* For now, ignore priority and use standard installation */
    return AEInstallEventHandler(theAEEventClass, theAEEventID, handler, handlerRefcon, isSysHandler);
}

OSErr AEInstallWildcardHandler(AEEventClass theAEEventClass, EventHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler) {
    /* Install with wildcard event ID */
    return AEInstallEventHandler(theAEEventClass, typeWildCard, handler, handlerRefcon, isSysHandler);
}

OSErr AEInstallDefaultHandler(EventHandlerProcPtr handler, int32_t handlerRefcon) {
    g_handlerState.defaultHandler = handler;
    g_handlerState.defaultHandlerRefcon = handlerRefcon;
    return noErr;
}

/* ========================================================================
 * Handler Enumeration Functions
 * ======================================================================== */

OSErr AEEnumerateEventHandlers(AEHandlerEnumProc enumProc, void* userData) {
    if (!enumProc) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AEHandlerTableEntry* current = g_aeMgrState.eventHandlers;
    while (current) {
        bool shouldContinue = enumProc(current->eventClass, current->eventID,
                                     current->handler, current->handlerRefcon,
                                     current->isSystemHandler, userData);
        if (!shouldContinue) break;
        current = current->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return noErr;
}

bool AEIsHandlerInstalled(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, bool isSysHandler) {
    EventHandlerProcPtr installedHandler;
    OSErr err = AEGetEventHandler(theAEEventClass, theAEEventID, &installedHandler, NULL, isSysHandler);
    return (err == noErr && installedHandler == handler);
}

/* ========================================================================
 * Event Dispatch Functions
 * ======================================================================== */

static OSErr FindEventHandler(const AppleEvent* theAppleEvent, EventHandlerProcPtr* handler, int32_t* handlerRefcon) {
    /* Extract event class and ID from Apple Event */
    AEEventClass eventClass;
    AEEventID eventID;

    OSErr err = AEGetParamPtr(theAppleEvent, keyEventClassAttr, typeType, NULL, &eventClass, sizeof(eventClass), NULL);
    if (err != noErr) return err;

    err = AEGetParamPtr(theAppleEvent, keyEventIDAttr, typeType, NULL, &eventID, sizeof(eventID), NULL);
    if (err != noErr) return err;

    /* First try exact match */
    err = AEGetEventHandler(eventClass, eventID, handler, handlerRefcon, false);
    if (err == noErr) return noErr;

    /* Try system handlers */
    err = AEGetEventHandler(eventClass, eventID, handler, handlerRefcon, true);
    if (err == noErr) return noErr;

    /* Try wildcard event ID */
    err = AEGetEventHandler(eventClass, typeWildCard, handler, handlerRefcon, false);
    if (err == noErr) return noErr;

    /* Try system wildcard */
    err = AEGetEventHandler(eventClass, typeWildCard, handler, handlerRefcon, true);
    if (err == noErr) return noErr;

    /* Try default handler */
    if (g_handlerState.defaultHandler) {
        *handler = g_handlerState.defaultHandler;
        *handlerRefcon = g_handlerState.defaultHandlerRefcon;
        return noErr;
    }

    return errAEHandlerNotFound;
}

OSErr AEDispatchAppleEvent(const AppleEvent* theAppleEvent, AppleEvent* reply, AEHandlerResult* result) {
    if (!theAppleEvent || !result) return errAENotAEDesc;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    *result = kAEHandlerNotFound;

    /* Apply event filter if installed */
    if (g_handlerState.eventFilter) {
        if (!g_handlerState.eventFilter(theAppleEvent, g_handlerState.filterUserData)) {
            *result = kAEHandlerNotFound;
            return errAEEventNotHandled;
        }
    }

    /* Find handler */
    EventHandlerProcPtr handler;
    int32_t handlerRefcon;
    OSErr err = FindEventHandler(theAppleEvent, &handler, &handlerRefcon);
    if (err != noErr) {
        *result = kAEHandlerNotFound;
        return err;
    }

    /* Call pre-dispatch hook */
    if (g_handlerState.preDispatchProc) {
        err = g_handlerState.preDispatchProc(theAppleEvent, reply, g_handlerState.preDispatchUserData);
        if (err != noErr) {
            *result = kAEHandlerFailed;
            return err;
        }
    }

    /* Set up dispatch context */
    const AppleEvent* previousEvent = g_aeMgrState.currentEvent;
    AppleEvent* previousReply = g_aeMgrState.currentReply;
    bool previousSuspended = g_aeMgrState.eventSuspended;

    g_aeMgrState.currentEvent = theAppleEvent;
    g_aeMgrState.currentReply = reply;
    g_aeMgrState.eventSuspended = false;

    /* Record performance info */
    clock_t startTime = clock();

    /* Call the handler */
    err = handler(theAppleEvent, reply, handlerRefcon);

    /* Update performance statistics */
    clock_t endTime = clock();
    int32_t executionTimeMs = (int32_t)((endTime - startTime) * 1000 / CLOCKS_PER_SEC);

    /* Update handler performance info */
    if (g_handlerState.perfInfo) {
        /* Find or create performance entry */
        AEHandlerPerfInfo* perfEntry = NULL;
        for (int32_t i = 0; i < g_handlerState.perfInfoCount; i++) {
            if (g_handlerState.perfInfo[i].handler == handler) {
                perfEntry = &g_handlerState.perfInfo[i];
                break;
            }
        }

        if (!perfEntry && g_handlerState.perfInfoCount < g_handlerState.perfInfoCapacity) {
            perfEntry = &g_handlerState.perfInfo[g_handlerState.perfInfoCount++];
            perfEntry->handler = handler;
            perfEntry->callCount = 0;
            perfEntry->totalTimeMilliseconds = 0;
            perfEntry->averageTimeMilliseconds = 0;
            perfEntry->maxTimeMilliseconds = 0;

            /* Try to get event class/ID */
            AEGetParamPtr(theAppleEvent, keyEventClassAttr, typeType, NULL, &perfEntry->eventClass, sizeof(perfEntry->eventClass), NULL);
            AEGetParamPtr(theAppleEvent, keyEventIDAttr, typeType, NULL, &perfEntry->eventID, sizeof(perfEntry->eventID), NULL);
        }

        if (perfEntry) {
            perfEntry->callCount++;
            perfEntry->totalTimeMilliseconds += executionTimeMs;
            perfEntry->averageTimeMilliseconds = perfEntry->totalTimeMilliseconds / perfEntry->callCount;
            if (executionTimeMs > perfEntry->maxTimeMilliseconds) {
                perfEntry->maxTimeMilliseconds = executionTimeMs;
            }
        }
    }

    /* Determine result */
    if (g_aeMgrState.eventSuspended) {
        *result = kAEHandlerSuspended;
    } else if (err == noErr) {
        *result = kAEHandlerExecuted;
        g_handlerState.stats.eventsHandled++;
    } else {
        *result = kAEHandlerFailed;
        g_handlerState.stats.eventsFailed++;

        /* Call error handler if installed */
        if (g_handlerState.errorHandler) {
            OSErr errorResult = g_handlerState.errorHandler(err, theAppleEvent, reply, g_handlerState.errorUserData);
            if (errorResult == noErr) {
                err = noErr;
                *result = kAEHandlerExecuted;
            }
        }
    }

    /* Call post-dispatch hook */
    if (g_handlerState.postDispatchProc) {
        g_handlerState.postDispatchProc(theAppleEvent, reply, err, g_handlerState.postDispatchUserData);
    }

    /* Restore previous context */
    g_aeMgrState.currentEvent = previousEvent;
    g_aeMgrState.currentReply = previousReply;
    g_aeMgrState.eventSuspended = previousSuspended;

    /* Update statistics */
    g_handlerState.stats.eventsDispatched++;
    g_aeMgrState.eventsProcessed++;

    return err;
}

OSErr AEDispatchToHandler(const AppleEvent* theAppleEvent, AppleEvent* reply, EventHandlerProcPtr handler, int32_t handlerRefcon) {
    if (!theAppleEvent || !handler) return errAENotAEDesc;

    /* Set up context and call handler directly */
    const AppleEvent* previousEvent = g_aeMgrState.currentEvent;
    AppleEvent* previousReply = g_aeMgrState.currentReply;

    g_aeMgrState.currentEvent = theAppleEvent;
    g_aeMgrState.currentReply = reply;

    OSErr err = handler(theAppleEvent, reply, handlerRefcon);

    g_aeMgrState.currentEvent = previousEvent;
    g_aeMgrState.currentReply = previousReply;

    return err;
}

/* ========================================================================
 * Advanced Event Processing Functions
 * ======================================================================== */

OSErr AESuspendTheCurrentEvent(const AppleEvent* theAppleEvent) {
    if (!theAppleEvent) return errAENotAEDesc;
    if (theAppleEvent != g_aeMgrState.currentEvent) return errAEEventNotHandled;

    g_aeMgrState.eventSuspended = true;
    return noErr;
}

OSErr AEResumeTheCurrentEvent(const AppleEvent* theAppleEvent, const AppleEvent* reply, EventHandlerProcPtr dispatcher, int32_t handlerRefcon) {
    if (!theAppleEvent) return errAENotAEDesc;

    g_aeMgrState.eventSuspended = false;

    if (dispatcher == (EventHandlerProcPtr)kAEUseStandardDispatch) {
        /* Redispatch using standard mechanism */
        AEHandlerResult result;
        return AEDispatchAppleEvent(theAppleEvent, (AppleEvent*)reply, &result);
    } else if (dispatcher == (EventHandlerProcPtr)kAENoDispatch) {
        /* No redispatch needed */
        return noErr;
    } else if (dispatcher) {
        /* Call custom dispatcher */
        return dispatcher(theAppleEvent, (AppleEvent*)reply, handlerRefcon);
    }

    return noErr;
}

/* ========================================================================
 * Coercion Handler Functions
 * ======================================================================== */

OSErr AEInstallCoercionHandler(DescType fromType, DescType toType, CoercionHandlerProcPtr handler, int32_t handlerRefcon, bool fromTypeIsDesc, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    /* Check if handler already exists */
    AECoercionHandlerEntry* current = g_aeMgrState.coercionHandlers;
    while (current) {
        if (current->fromType == fromType &&
            current->toType == toType &&
            current->isSystemHandler == isSysHandler) {
            /* Update existing handler */
            current->handler = handler;
            current->handlerRefcon = handlerRefcon;
            current->fromTypeIsDesc = fromTypeIsDesc;
            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = current->next;
    }

    /* Create new coercion handler entry */
    AECoercionHandlerEntry* newEntry = malloc(sizeof(AECoercionHandlerEntry));
    if (!newEntry) {
        pthread_mutex_unlock(&g_aeMgrState.mutex);
        return memFullErr;
    }

    newEntry->fromType = fromType;
    newEntry->toType = toType;
    newEntry->handler = handler;
    newEntry->handlerRefcon = handlerRefcon;
    newEntry->fromTypeIsDesc = fromTypeIsDesc;
    newEntry->isSystemHandler = isSysHandler;
    newEntry->next = g_aeMgrState.coercionHandlers;

    g_aeMgrState.coercionHandlers = newEntry;

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return noErr;
}

OSErr AERemoveCoercionHandler(DescType fromType, DescType toType, CoercionHandlerProcPtr handler, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AECoercionHandlerEntry** current = &g_aeMgrState.coercionHandlers;
    while (*current) {
        if ((*current)->fromType == fromType &&
            (*current)->toType == toType &&
            (*current)->handler == handler &&
            (*current)->isSystemHandler == isSysHandler) {

            AECoercionHandlerEntry* toDelete = *current;
            *current = (*current)->next;
            free(toDelete);

            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = &(*current)->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return errAEHandlerNotFound;
}

OSErr AEGetCoercionHandler(DescType fromType, DescType toType, CoercionHandlerProcPtr* handler, int32_t* handlerRefcon, bool* fromTypeIsDesc, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AECoercionHandlerEntry* current = g_aeMgrState.coercionHandlers;
    while (current) {
        if (current->fromType == fromType &&
            current->toType == toType &&
            current->isSystemHandler == isSysHandler) {

            *handler = current->handler;
            if (handlerRefcon) *handlerRefcon = current->handlerRefcon;
            if (fromTypeIsDesc) *fromTypeIsDesc = current->fromTypeIsDesc;

            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return errAEHandlerNotFound;
}

/* ========================================================================
 * Special Handler Functions
 * ======================================================================== */

OSErr AEInstallSpecialHandler(AEKeyword functionClass, void* handler, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    /* Check if handler already exists */
    AESpecialHandlerEntry* current = g_aeMgrState.specialHandlers;
    while (current) {
        if (current->functionClass == functionClass &&
            current->isSystemHandler == isSysHandler) {
            /* Update existing handler */
            current->handler = handler;
            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = current->next;
    }

    /* Create new special handler entry */
    AESpecialHandlerEntry* newEntry = malloc(sizeof(AESpecialHandlerEntry));
    if (!newEntry) {
        pthread_mutex_unlock(&g_aeMgrState.mutex);
        return memFullErr;
    }

    newEntry->functionClass = functionClass;
    newEntry->handler = handler;
    newEntry->isSystemHandler = isSysHandler;
    newEntry->next = g_aeMgrState.specialHandlers;

    g_aeMgrState.specialHandlers = newEntry;

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return noErr;
}

OSErr AERemoveSpecialHandler(AEKeyword functionClass, void* handler, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AESpecialHandlerEntry** current = &g_aeMgrState.specialHandlers;
    while (*current) {
        if ((*current)->functionClass == functionClass &&
            (*current)->handler == handler &&
            (*current)->isSystemHandler == isSysHandler) {

            AESpecialHandlerEntry* toDelete = *current;
            *current = (*current)->next;
            free(toDelete);

            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = &(*current)->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return errAEHandlerNotFound;
}

OSErr AEGetSpecialHandler(AEKeyword functionClass, void** handler, bool isSysHandler) {
    if (!handler) return errAEHandlerNotFound;
    if (!g_aeMgrState.initialized) return errAENewerVersion;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    AESpecialHandlerEntry* current = g_aeMgrState.specialHandlers;
    while (current) {
        if (current->functionClass == functionClass &&
            current->isSystemHandler == isSysHandler) {

            *handler = current->handler;
            pthread_mutex_unlock(&g_aeMgrState.mutex);
            return noErr;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return errAEHandlerNotFound;
}

/* ========================================================================
 * Hook Management Functions
 * ======================================================================== */

OSErr AEInstallPreDispatchHook(AEPreDispatchProc proc, void* userData) {
    g_handlerState.preDispatchProc = proc;
    g_handlerState.preDispatchUserData = userData;
    return noErr;
}

OSErr AEInstallPostDispatchHook(AEPostDispatchProc proc, void* userData) {
    g_handlerState.postDispatchProc = proc;
    g_handlerState.postDispatchUserData = userData;
    return noErr;
}

OSErr AERemovePreDispatchHook(AEPreDispatchProc proc) {
    if (g_handlerState.preDispatchProc == proc) {
        g_handlerState.preDispatchProc = NULL;
        g_handlerState.preDispatchUserData = NULL;
        return noErr;
    }
    return errAEHandlerNotFound;
}

OSErr AERemovePostDispatchHook(AEPostDispatchProc proc) {
    if (g_handlerState.postDispatchProc == proc) {
        g_handlerState.postDispatchProc = NULL;
        g_handlerState.postDispatchUserData = NULL;
        return noErr;
    }
    return errAEHandlerNotFound;
}

/* ========================================================================
 * Statistics and Monitoring Functions
 * ======================================================================== */

OSErr AEGetHandlerStats(AEHandlerStats* stats) {
    if (!stats) return errAENotAEDesc;

    pthread_mutex_lock(&g_aeMgrState.mutex);

    /* Count handlers */
    int32_t totalHandlers = 0, systemHandlers = 0, userHandlers = 0;
    AEHandlerTableEntry* handler = g_aeMgrState.eventHandlers;
    while (handler) {
        totalHandlers++;
        if (handler->isSystemHandler) {
            systemHandlers++;
        } else {
            userHandlers++;
        }
        handler = handler->next;
    }

    stats->totalHandlers = totalHandlers;
    stats->systemHandlers = systemHandlers;
    stats->userHandlers = userHandlers;
    stats->eventsDispatched = g_handlerState.stats.eventsDispatched;
    stats->eventsHandled = g_handlerState.stats.eventsHandled;
    stats->eventsFailed = g_handlerState.stats.eventsFailed;
    stats->eventsSuspended = g_handlerState.stats.eventsSuspended;
    stats->coercionsPerformed = g_handlerState.stats.coercionsPerformed;
    stats->coercionsFailed = g_handlerState.stats.coercionsFailed;

    pthread_mutex_unlock(&g_aeMgrState.mutex);
    return noErr;
}

void AEResetHandlerStats(void) {
    memset(&g_handlerState.stats, 0, sizeof(AEHandlerStats));
}

/* ========================================================================
 * Error Handling Functions
 * ======================================================================== */

OSErr AEInstallErrorHandler(AEErrorHandlerProc errorHandler, void* userData) {
    g_handlerState.errorHandler = errorHandler;
    g_handlerState.errorUserData = userData;
    return noErr;
}

OSErr AERemoveErrorHandler(AEErrorHandlerProc errorHandler) {
    if (g_handlerState.errorHandler == errorHandler) {
        g_handlerState.errorHandler = NULL;
        g_handlerState.errorUserData = NULL;
        return noErr;
    }
    return errAEHandlerNotFound;
}

/* ========================================================================
 * Event Filtering Functions
 * ======================================================================== */

OSErr AEInstallEventFilter(AEEventFilterProc filterProc, void* userData) {
    g_handlerState.eventFilter = filterProc;
    g_handlerState.filterUserData = userData;
    return noErr;
}

OSErr AERemoveEventFilter(AEEventFilterProc filterProc) {
    if (g_handlerState.eventFilter == filterProc) {
        g_handlerState.eventFilter = NULL;
        g_handlerState.filterUserData = NULL;
        return noErr;
    }
    return errAEHandlerNotFound;
}
#include "MemoryMgr/MemoryManager.h"
/* #include "SystemTypes.h" */
#include "EventManager/EventManagerInternal.h"
#include <stdlib.h>
#include <string.h>
/**
 * @file SystemEvents.c
 * @brief System Event Management Implementation for System 7.1
 *
 * This file provides system-level event handling including update events,
 * activate/deactivate events, suspend/resume events, disk events, and
 * other system notifications essential for Mac OS System 7.1 compatibility.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include <time.h>

#include "EventManager/SystemEvents.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventLogging.h"
#include "QuickDraw/QuickDraw.h"


/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* System state constants */
enum {
    kSystemStateForeground = 1,
    kSystemStateBackground = 2,
    kSystemStateActive = 4,
    kSystemStateInactive = 8,
    kSystemStateVisible = 16,
    kSystemStateHidden = 32,
    kSystemStateSuspended = 64
};

/* Event priority constants */
enum {
    kSystemEventPriorityLow = 0,
    kSystemEventPriorityNormal = 1,
    kSystemEventPriorityHigh = 2,
    kSystemEventPriorityImmediate = 3,
    kSystemEventPriorityCritical = 4
};

/* OS Event subtypes */
enum {
    kOSEventSuspend = 1,
    kOSEventResume = 2,
    kOSEventMouseMoved = 3,
    kOSEventClipboardChange = 4,
    kOSEventMultiFinder = 5
};

/* Event window constants */
enum {
    kActivateEventWindow = 1,
    kUpdateEventWindow = 2
};

/* Disk event constants */
enum {
    kDiskEventInserted = 1,
    kDiskEventEjected = 2,
    kDiskEventMounted = 3,
    kDiskEventUnmounted = 4
};

/* DiskEventInfo definition */
typedef struct DiskEventInfo {
    SInt16 eventType;
    SInt16 driveNumber;
    UInt32 volumeRefNum;
    SInt16 refNum;
    UInt32 eventTime;
    Str255 volumeName;
} DiskEventInfo;

/* AppStateInfo definition */
struct AppStateInfo {
    Boolean suspended;
    Boolean active;
    UInt32 suspendTime;
    UInt32 resumeTime;
    UInt32 currentState;
    UInt32 previousState;
    UInt32 stateChangeTime;
};

/* UpdateRegion definition */
typedef struct UpdateRegion {
    WindowPtr window;
    RgnHandle updateRgn;
    SInt16 updateType;
    SInt16 priority;
    UInt32 updateTime;
    struct UpdateRegion* next;
} UpdateRegion;

/* SystemEventContext definition */
typedef struct SystemEventContext {
    SInt16 eventSubtype;
    void* eventData;
    WindowPtr targetWindow;
    UInt32 timestamp;
    EventRecord baseEvent;
    Boolean consumed;
} SystemEventContext;

/* System event state */
static Boolean g_systemEventsInitialized = false;
static struct AppStateInfo g_appState = {0};
static WindowPtr g_frontWindow = NULL;

/* Update regions */
static UpdateRegion* g_updateRegions = NULL;
static SInt16 g_updateRegionCount = 0;

/* Event callbacks */
typedef struct EventCallback {
    SInt16 eventType;
    SystemEventCallback callback;
    void* userData;
    struct EventCallback* next;
} EventCallback;

static EventCallback* g_eventCallbacks = NULL;

/* Disk event monitoring */
static DiskEventCallback g_diskEventCallback = NULL;
static void* g_diskEventUserData = NULL;

/* External references */
/* PostEvent declared in EventManager.h */
extern UInt32 TickCount(void);

/*---------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/

static void NotifyCallbacks(SInt16 eventType, SystemEventContext* context);
EventRecord GenerateOSEvent(SInt16 eventSubtype, SInt32 message);
static UpdateRegion* FindUpdateRegion(WindowPtr window);
static void RemoveUpdateRegion(UpdateRegion* region);

/* ProcessPendingUpdates implementation */
static void ProcessPendingUpdates(void)
{
    /* Process any pending update regions */
    UpdateRegion* region = g_updateRegions;
    while (region) {
        /* Generate update event for each region */
        Point where = {0, 0};
        GenerateSystemEvent(updateEvt, (SInt32)(uintptr_t)region->window, where, 0);
        region = region->next;
    }
}

/*---------------------------------------------------------------------------
 * Core System Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize system event management
 */
SInt16 InitSystemEvents(void)
{
    if (g_systemEventsInitialized) {
        return noErr;
    }

    /* Initialize application state */
    memset(&g_appState, 0, sizeof(AppStateInfo));
    g_appState.currentState = kSystemStateForeground | kSystemStateActive | kSystemStateVisible;
    g_appState.stateChangeTime = TickCount();

    /* Initialize update regions list */
    g_updateRegions = NULL;
    g_updateRegionCount = 0;

    /* Initialize callbacks */
    g_eventCallbacks = NULL;

    g_systemEventsInitialized = true;
    return noErr;
}

/**
 * Shutdown system event management
 */
void ShutdownSystemEvents(void)
{
    if (!g_systemEventsInitialized) {
        return;
    }

    /* Free update regions */
    UpdateRegion* region = g_updateRegions;
    while (region) {
        UpdateRegion* next = region->next;
        DisposePtr((Ptr)region);
        region = next;
    }
    g_updateRegions = NULL;

    /* Free callbacks */
    EventCallback* callback = g_eventCallbacks;
    while (callback) {
        EventCallback* next = callback->next;
        DisposePtr((Ptr)callback);
        callback = next;
    }
    g_eventCallbacks = NULL;

    g_systemEventsInitialized = false;
}

/**
 * Process system events
 */
void ProcessSystemEvents(void)
{
    if (!g_systemEventsInitialized) {
        return;
    }

    ProcessPendingUpdates();
}

/**
 * Generate system event
 */
SInt16 GenerateSystemEventEx(SInt16 eventType, SInt16 eventSubtype,
                             void* eventData, WindowPtr targetWindow)
{
    if (!g_systemEventsInitialized) {
        return noErr;
    }

    SystemEventContext context = {0};
    context.eventSubtype = eventSubtype;
    context.eventData = eventData;
    context.targetWindow = targetWindow;
    context.timestamp = TickCount();

    /* Create base event record */
    EventRecord baseEvent = {0};
    baseEvent.what = eventType;
    baseEvent.when = context.timestamp;
    baseEvent.where.h = 0;
    baseEvent.where.v = 0;
    baseEvent.modifiers = 0;

    switch (eventType) {
        case updateEvt:
            baseEvent.message = (SInt32)(uintptr_t)targetWindow;
            break;
        case activateEvt:
            baseEvent.message = (SInt32)(uintptr_t)targetWindow;
            if (eventSubtype == kActivateEventWindow) {
                baseEvent.modifiers |= activeFlag;
            }
            break;
        case osEvt:
            baseEvent.message = (eventSubtype << 24);
            break;
        case diskEvt:
            baseEvent.message = eventSubtype;
            break;
    }

    context.baseEvent = baseEvent;

    /* Notify callbacks */
    NotifyCallbacks(eventType, &context);

    /* Post event if not consumed */
    if (!context.consumed) {
        PostEvent(eventType, baseEvent.message);
    }

    return noErr;
}

/**
 * Post system event to queue
 */
SInt16 PostSystemEvent(SInt16 eventType, SInt32 message, SInt16 priority)
{
    /* Priority is informational for now */
    return PostEvent(eventType, message);
}

/*---------------------------------------------------------------------------
 * Update Event Management
 *---------------------------------------------------------------------------*/

/**
 * Request window update
 */
SInt16 RequestWindowUpdate(WindowPtr window, RgnHandle updateRgn,
                           SInt16 updateType, SInt16 priority)
{
    if (!window) {
        return noErr;
    }

    /* Find existing update region for this window */
    UpdateRegion* region = FindUpdateRegion(window);

    if (!region) {
        /* Create new update region */
        region = (UpdateRegion*)NewPtr(sizeof(UpdateRegion));
        if (!region) {
            return -1; /* Memory error */
        }

        memset(region, 0, sizeof(UpdateRegion));
        region->window = window;
        region->updateType = updateType;
        region->priority = priority;
        region->updateTime = TickCount();
        region->next = g_updateRegions;
        g_updateRegions = region;
        g_updateRegionCount++;

        /* Initialize the update region if provided */
        if (updateRgn) {
            region->updateRgn = NewRgn();
            if (region->updateRgn) {
                CopyRgn(updateRgn, region->updateRgn);
            }
        }
    } else {
        /* Merge with existing update region */
        if (updateRgn) {
            if (!region->updateRgn) {
                region->updateRgn = NewRgn();
            }
            if (region->updateRgn) {
                /* Union the new invalid region with existing update region */
                RgnHandle tempRgn = NewRgn();
                if (tempRgn) {
                    CopyRgn(region->updateRgn, tempRgn);
                    UnionRgn(tempRgn, updateRgn, region->updateRgn);
                    DisposeRgn(tempRgn);
                }
            }
        }
        /* Update the timestamp to reflect new invalidation */
        region->updateTime = TickCount();
    }

    /* Generate update event */
    Point where = {0, 0};
    GenerateSystemEvent(updateEvt, (SInt32)(uintptr_t)window, where, 0);

    return noErr;
}

/**
 * Process pending update events
 */
SInt16 ProcessUpdateEvents(void)
{
    SInt16 updatesProcessed = 0;
    UpdateRegion* region = g_updateRegions;

    while (region) {
        UpdateRegion* next = region->next;

        /* Generate update event for this region */
        PostEvent(updateEvt, (SInt32)(uintptr_t)region->window);
        updatesProcessed++;

        /* Remove processed region */
        RemoveUpdateRegion(region);

        region = next;
    }

    return updatesProcessed;
}

/**
 * Check if window needs update
 */
Boolean WindowNeedsUpdate(WindowPtr window)
{
    return FindUpdateRegion(window) != NULL;
}

/**
 * Get window update region
 */
RgnHandle GetWindowUpdateRegion(WindowPtr window)
{
    UpdateRegion* region = FindUpdateRegion(window);
    return region ? region->updateRgn : NULL;
}

/**
 * Validate window update region
 */
void ValidateWindowRegion(WindowPtr window, RgnHandle validRgn)
{
    /* Remove the validated region from pending updates */
    UpdateRegion* region = FindUpdateRegion(window);
    if (region && region->updateRgn) {
        if (validRgn) {
            /* Subtract the validated region from the pending update region */
            RgnHandle tempRgn = NewRgn();
            if (tempRgn) {
                CopyRgn(region->updateRgn, tempRgn);
                DiffRgn(tempRgn, validRgn, region->updateRgn);
                DisposeRgn(tempRgn);
            }

            /* If the update region is now empty, remove it entirely */
            if (EmptyRgn(region->updateRgn)) {
                RemoveUpdateRegion(region);
            }
        } else {
            /* No validRgn specified, clear entire update region */
            RemoveUpdateRegion(region);
        }
    }
}

/**
 * Invalidate window region
 */
void InvalidateWindowRegion(WindowPtr window, RgnHandle invalidRgn)
{
    /* Add to pending updates */
    RequestWindowUpdate(window, invalidRgn, kUpdateEventWindow, kSystemEventPriorityNormal);
}

/**
 * Clear all pending updates for window
 */
void ClearWindowUpdates(WindowPtr window)
{
    UpdateRegion* region = FindUpdateRegion(window);
    if (region) {
        RemoveUpdateRegion(region);
    }
}

/**
 * Find update region for window
 */
static UpdateRegion* FindUpdateRegion(WindowPtr window)
{
    UpdateRegion* region = g_updateRegions;
    while (region) {
        if (region->window == window) {
            return region;
        }
        region = region->next;
    }
    return NULL;
}

/**
 * Remove update region from list
 */
static void RemoveUpdateRegion(UpdateRegion* regionToRemove)
{
    UpdateRegion** current = &g_updateRegions;
    while (*current) {
        if (*current == regionToRemove) {
            *current = regionToRemove->next;
            DisposePtr((Ptr)regionToRemove);
            g_updateRegionCount--;
            break;
        }
        current = &((*current)->next);
    }
}

/*---------------------------------------------------------------------------
 * Window Activation Management
 *---------------------------------------------------------------------------*/

/**
 * Process window activation event
 */
SInt16 ProcessWindowActivation(WindowPtr window, Boolean isActivating)
{
    if (isActivating) {
        g_frontWindow = window;
    } else {
        if (g_frontWindow == window) {
            g_frontWindow = NULL;
        }
    }

    /* Generate activate event */
    EventRecord event = GenerateActivateEvent(window, isActivating, kActivateEventWindow);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Generate activate event
 */
EventRecord GenerateActivateEvent(WindowPtr window, Boolean isActivating, SInt16 activationType)
{
    EventRecord event = {0};

    event.what = activateEvt;
    event.message = (SInt32)(uintptr_t)window;
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = isActivating ? activeFlag : 0;

    return event;
}

/**
 * Check if window is active
 */
Boolean IsWindowActive(WindowPtr window)
{
    return g_frontWindow == window;
}

/**
 * Set window activation state
 */
void SetWindowActivation(WindowPtr window, Boolean isActive)
{
    if (isActive) {
        ProcessWindowActivation(window, true);
    } else {
        ProcessWindowActivation(window, false);
    }
}

/**
 * Get front window
 */
WindowPtr GetFrontWindow(void)
{
    return g_frontWindow;
}

/**
 * Set front window
 */
WindowPtr SetFrontWindow(WindowPtr window)
{
    WindowPtr oldFront = g_frontWindow;

    /* Deactivate old front window */
    if (oldFront && oldFront != window) {
        ProcessWindowActivation(oldFront, false);
    }

    /* Activate new front window */
    if (window) {
        ProcessWindowActivation(window, true);
    }

    return oldFront;
}

/*---------------------------------------------------------------------------
 * Application State Management
 *---------------------------------------------------------------------------*/

/**
 * Process application suspend
 */
SInt16 ProcessApplicationSuspend(void)
{
    UInt16 oldState = g_appState.currentState;
    g_appState.previousState = oldState;
    g_appState.currentState = kSystemStateBackground | kSystemStateSuspended;
    g_appState.stateChangeTime = TickCount();
    g_appState.suspendTime = g_appState.stateChangeTime;

    /* Generate suspend event */
    EventRecord event = GenerateSuspendResumeEvent(true, false);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Process application resume
 */
SInt16 ProcessApplicationResume(Boolean convertClipboard)
{
    UInt16 oldState = g_appState.currentState;
    g_appState.previousState = oldState;
    g_appState.currentState = kSystemStateForeground | kSystemStateActive | kSystemStateVisible;
    g_appState.stateChangeTime = TickCount();

    /* Generate resume event */
    EventRecord event = GenerateSuspendResumeEvent(false, convertClipboard);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Generate suspend/resume event
 */
EventRecord GenerateSuspendResumeEvent(Boolean isSuspend, Boolean convertClipboard)
{
    EventRecord event = {0};

    event.what = osEvt;
    event.message = suspendResumeMessage << 24;
    if (!isSuspend) {
        event.message |= resumeFlag;
    }
    if (convertClipboard) {
        event.message |= convertClipboardFlag;
    }
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = 0;

    return event;
}

/**
 * Check if application is suspended
 */
Boolean IsApplicationSuspended(void)
{
    return (g_appState.currentState & kSystemStateSuspended) != 0;
}

/**
 * Check if application is in foreground
 */
Boolean IsApplicationInForeground(void)
{
    return (g_appState.currentState & kSystemStateForeground) != 0;
}

/**
 * Set application state
 */
void SetApplicationState(UInt16 newState)
{
    g_appState.previousState = g_appState.currentState;
    g_appState.currentState = newState;
    g_appState.stateChangeTime = TickCount();
}

/**
 * Get application state
 */
UInt16 GetApplicationState(void)
{
    return g_appState.currentState;
}

/**
 * Get application state info
 */
AppStateInfo* GetApplicationStateInfo(void)
{
    return &g_appState;
}

/*---------------------------------------------------------------------------
 * Disk Event Management
 *---------------------------------------------------------------------------*/

/**
 * Process disk insertion
 */
SInt16 ProcessDiskInsertion(SInt16 driveNumber, const char* volumeName)
{
    DiskEventInfo diskInfo = {0};
    diskInfo.eventType = kDiskEventInserted;
    diskInfo.driveNumber = driveNumber;
    diskInfo.refNum = driveNumber; /* Simplified */
    diskInfo.eventTime = TickCount();

    if (volumeName) {
        strncpy(diskInfo.volumeName, volumeName, sizeof(diskInfo.volumeName) - 1);
        diskInfo.volumeName[sizeof(diskInfo.volumeName) - 1] = '\0';
    }

    /* Notify callback */
    if (g_diskEventCallback) {
        g_diskEventCallback(&diskInfo);
    }

    /* Generate disk event */
    EventRecord event = GenerateDiskEvent(kDiskEventInserted, driveNumber, driveNumber);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Process disk ejection
 */
SInt16 ProcessDiskEjection(SInt16 driveNumber)
{
    DiskEventInfo diskInfo = {0};
    diskInfo.eventType = kDiskEventEjected;
    diskInfo.driveNumber = driveNumber;
    diskInfo.refNum = driveNumber;
    diskInfo.eventTime = TickCount();

    /* Notify callback */
    if (g_diskEventCallback) {
        g_diskEventCallback(&diskInfo);
    }

    /* Generate disk event */
    EventRecord event = GenerateDiskEvent(kDiskEventEjected, driveNumber, driveNumber);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Generate disk event
 */
EventRecord GenerateDiskEvent(SInt16 eventType, SInt16 driveNumber, SInt16 refNum)
{
    EventRecord event = {0};

    event.what = diskEvt;
    event.message = (eventType << 16) | driveNumber;
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = 0;

    return event;
}

/**
 * Monitor disk events
 */
void MonitorDiskEvents(DiskEventCallback callback, void* userData)
{
    g_diskEventCallback = callback;
    g_diskEventUserData = userData;
}

/*---------------------------------------------------------------------------
 * OS Event Management
 *---------------------------------------------------------------------------*/

/**
 * Process mouse moved event
 */
SInt16 ProcessMouseMovedEvent(Point newPosition)
{
    EventRecord event = GenerateOSEvent(kOSEventMouseMoved, 0);
    event.where = newPosition;
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Generate OS event
 */
EventRecord GenerateOSEvent(SInt16 eventSubtype, SInt32 message)
{
    EventRecord event = {0};

    event.what = osEvt;
    event.message = (eventSubtype << 24) | (message & 0x00FFFFFF);
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = 0;

    return event;
}

/**
 * Process MultiFinder event
 */
SInt16 ProcessMultiFinderEvent(void* eventData)
{
    /* Generate MultiFinder OS event */
    EventRecord event = GenerateOSEvent(kOSEventMultiFinder, 0);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Process clipboard change event
 */
SInt16 ProcessClipboardChangeEvent(void)
{
    EventRecord event = GenerateOSEvent(kOSEventClipboardChange, 0);
    PostEvent(event.what, event.message);

    return noErr;
}

/*---------------------------------------------------------------------------
 * Event Callback Management
 *---------------------------------------------------------------------------*/

/**
 * Register system event callback
 */
void* RegisterSystemEventCallback(SInt16 eventType, SystemEventCallback callback, void* userData)
{
    EventCallback* newCallback = (EventCallback*)NewPtr(sizeof(EventCallback));
    if (!newCallback) {
        return NULL;
    }

    newCallback->eventType = eventType;
    newCallback->callback = callback;
    newCallback->userData = userData;
    newCallback->next = g_eventCallbacks;
    g_eventCallbacks = newCallback;

    return newCallback;
}

/**
 * Unregister system event callback
 */
void UnregisterSystemEventCallback(void* handle)
{
    if (!handle) return;

    EventCallback** current = &g_eventCallbacks;
    while (*current) {
        if (*current == handle) {
            EventCallback* toRemove = *current;
            *current = toRemove->next;
            DisposePtr((Ptr)toRemove);
            break;
        }
        current = &((*current)->next);
    }
}

/**
 * Notify event callbacks
 */
static void NotifyCallbacks(SInt16 eventType, SystemEventContext* context)
{
    EventCallback* callback = g_eventCallbacks;
    while (callback) {
        if (callback->eventType == eventType && callback->callback) {
            callback->callback(callback->userData);
            /* Callbacks don't return consumed status, so we can't set context->consumed */
        }
        callback = callback->next;
    }
}

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Check if event is system event
 */
Boolean IsSystemEvent(const EventRecord* event)
{
    if (!event) return false;

    switch (event->what) {
        case updateEvt:
        case activateEvt:
        case diskEvt:
        case osEvt:
            return true;
        default:
            return false;
    }
}

/**
 * Get system event subtype
 */
SInt16 GetSystemEventSubtype(const EventRecord* event)
{
    if (!event || !IsSystemEvent(event)) {
        return 0;
    }

    switch (event->what) {
        case osEvt:
            return (event->message >> 24) & 0xFF;
        case diskEvt:
            return (event->message >> 16) & 0xFF;
        default:
            return 0;
    }
}

/**
 * Format system event for logging
 */
SInt16 FormatSystemEvent(const EventRecord* event, char* buffer, SInt16 bufferSize)
{
    if (!event || !buffer || bufferSize <= 0) {
        return 0;
    }

    const char* eventName = "Unknown";
    switch (event->what) {
        case nullEvent: eventName = "Null"; break;
        case updateEvt: eventName = "Update"; break;
        case activateEvt: eventName = "Activate"; break;
        case diskEvt: eventName = "Disk"; break;
        case osEvt: eventName = "OS"; break;
    }

    SInt16 len = snprintf(buffer, bufferSize, "%s Event (0x%04X)", eventName, event->what);
    return (len < bufferSize) ? len : bufferSize - 1;
}

/**
 * Get system event priority
 */
SInt16 GetSystemEventPriority(SInt16 eventType, SInt16 eventSubtype)
{
    switch (eventType) {
        case diskEvt:
            return kSystemEventPriorityHigh;
        case activateEvt:
            return kSystemEventPriorityHigh;
        case osEvt:
            if (eventSubtype == kOSEventSuspend || eventSubtype == kOSEventResume) {
                return kSystemEventPriorityCritical;
            }
            return kSystemEventPriorityNormal;
        case updateEvt:
            return kSystemEventPriorityLow;
        default:
            return kSystemEventPriorityNormal;
    }
}

/**
 * Validate system event
 */
Boolean ValidateSystemEvent(const EventRecord* event)
{
    if (!event) return false;

    /* Basic validation */
    return IsSystemEvent(event) && event->when > 0;
}

/**
 * Reset system event state
 */
void ResetSystemEventState(void)
{
    /* Clear update regions */
    UpdateRegion* region = g_updateRegions;
    while (region) {
        UpdateRegion* next = region->next;
        DisposePtr((Ptr)region);
        region = next;
    }
    g_updateRegions = NULL;
    g_updateRegionCount = 0;

    /* Reset application state */
    memset(&g_appState, 0, sizeof(AppStateInfo));
    g_appState.currentState = kSystemStateForeground | kSystemStateActive | kSystemStateVisible;
    g_appState.stateChangeTime = TickCount();

    /* Reset front window */
    g_frontWindow = NULL;
}

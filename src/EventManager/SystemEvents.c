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

#include "EventManager/SystemEvents.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventTypes.h"
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* System event state */
static bool g_systemEventsInitialized = false;
static AppStateInfo g_appState = {0};
static WindowPtr g_frontWindow = NULL;

/* Update regions */
static UpdateRegion* g_updateRegions = NULL;
static int16_t g_updateRegionCount = 0;

/* Event callbacks */
typedef struct EventCallback {
    int16_t eventType;
    SystemEventCallback callback;
    void* userData;
    struct EventCallback* next;
} EventCallback;

static EventCallback* g_eventCallbacks = NULL;

/* Disk event monitoring */
static DiskEventCallback g_diskEventCallback = NULL;
static void* g_diskEventUserData = NULL;

/* External references */
extern int16_t PostEvent(int16_t eventNum, int32_t eventMsg);
extern uint32_t TickCount(void);

/*---------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/

static void ProcessPendingUpdates(void);
static void NotifyCallbacks(int16_t eventType, SystemEventContext* context);
static UpdateRegion* FindUpdateRegion(WindowPtr window);
static void RemoveUpdateRegion(UpdateRegion* region);

/*---------------------------------------------------------------------------
 * Core System Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize system event management
 */
int16_t InitSystemEvents(void)
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
        free(region);
        region = next;
    }
    g_updateRegions = NULL;

    /* Free callbacks */
    EventCallback* callback = g_eventCallbacks;
    while (callback) {
        EventCallback* next = callback->next;
        free(callback);
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
int16_t GenerateSystemEvent(int16_t eventType, int16_t eventSubtype,
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
            baseEvent.message = (int32_t)targetWindow;
            break;
        case activateEvt:
            baseEvent.message = (int32_t)targetWindow;
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

    context.baseEvent = &baseEvent;

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
int16_t PostSystemEvent(int16_t eventType, int32_t message, int16_t priority)
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
int16_t RequestWindowUpdate(WindowPtr window, RgnHandle updateRgn,
                           int16_t updateType, int16_t priority)
{
    if (!window) {
        return noErr;
    }

    /* Find existing update region for this window */
    UpdateRegion* region = FindUpdateRegion(window);

    if (!region) {
        /* Create new update region */
        region = (UpdateRegion*)malloc(sizeof(UpdateRegion));
        if (!region) {
            return noMemErr;
        }

        memset(region, 0, sizeof(UpdateRegion));
        region->window = window;
        region->updateType = updateType;
        region->priority = priority;
        region->updateTime = TickCount();
        region->next = g_updateRegions;
        g_updateRegions = region;
        g_updateRegionCount++;
    }

    /* TODO: Merge with existing update region if updateRgn provided */
    /* For now, just mark that window needs update */

    /* Generate update event */
    GenerateSystemEvent(updateEvt, updateType, NULL, window);

    return noErr;
}

/**
 * Process pending update events
 */
int16_t ProcessUpdateEvents(void)
{
    int16_t updatesProcessed = 0;
    UpdateRegion* region = g_updateRegions;

    while (region) {
        UpdateRegion* next = region->next;

        /* Generate update event for this region */
        PostEvent(updateEvt, (int32_t)region->window);
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
bool WindowNeedsUpdate(WindowPtr window)
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
    if (region) {
        /* TODO: Subtract validRgn from region->updateRgn */
        /* For now, just remove the entire region */
        RemoveUpdateRegion(region);
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
            free(regionToRemove);
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
int16_t ProcessWindowActivation(WindowPtr window, bool isActivating)
{
    WindowPtr oldFrontWindow = g_frontWindow;

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
EventRecord GenerateActivateEvent(WindowPtr window, bool isActivating, int16_t activationType)
{
    EventRecord event = {0};

    event.what = activateEvt;
    event.message = (int32_t)window;
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = isActivating ? activeFlag : 0;

    return event;
}

/**
 * Check if window is active
 */
bool IsWindowActive(WindowPtr window)
{
    return g_frontWindow == window;
}

/**
 * Set window activation state
 */
void SetWindowActivation(WindowPtr window, bool isActive)
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
int16_t ProcessApplicationSuspend(void)
{
    uint16_t oldState = g_appState.currentState;
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
int16_t ProcessApplicationResume(bool convertClipboard)
{
    uint16_t oldState = g_appState.currentState;
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
EventRecord GenerateSuspendResumeEvent(bool isSuspend, bool convertClipboard)
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
bool IsApplicationSuspended(void)
{
    return (g_appState.currentState & kSystemStateSuspended) != 0;
}

/**
 * Check if application is in foreground
 */
bool IsApplicationInForeground(void)
{
    return (g_appState.currentState & kSystemStateForeground) != 0;
}

/**
 * Set application state
 */
void SetApplicationState(uint16_t newState)
{
    g_appState.previousState = g_appState.currentState;
    g_appState.currentState = newState;
    g_appState.stateChangeTime = TickCount();
}

/**
 * Get application state
 */
uint16_t GetApplicationState(void)
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
int16_t ProcessDiskInsertion(int16_t driveNumber, const char* volumeName)
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
        g_diskEventCallback(&diskInfo, g_diskEventUserData);
    }

    /* Generate disk event */
    EventRecord event = GenerateDiskEvent(kDiskEventInserted, driveNumber, driveNumber);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Process disk ejection
 */
int16_t ProcessDiskEjection(int16_t driveNumber)
{
    DiskEventInfo diskInfo = {0};
    diskInfo.eventType = kDiskEventEjected;
    diskInfo.driveNumber = driveNumber;
    diskInfo.refNum = driveNumber;
    diskInfo.eventTime = TickCount();

    /* Notify callback */
    if (g_diskEventCallback) {
        g_diskEventCallback(&diskInfo, g_diskEventUserData);
    }

    /* Generate disk event */
    EventRecord event = GenerateDiskEvent(kDiskEventEjected, driveNumber, driveNumber);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Generate disk event
 */
EventRecord GenerateDiskEvent(int16_t eventType, int16_t driveNumber, int16_t refNum)
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
int16_t ProcessMouseMovedEvent(Point newPosition)
{
    EventRecord event = GenerateOSEvent(kOSEventMouseMoved, 0);
    event.where = newPosition;
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Generate OS event
 */
EventRecord GenerateOSEvent(int16_t eventSubtype, int32_t message)
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
int16_t ProcessMultiFinderEvent(void* eventData)
{
    /* Generate MultiFinder OS event */
    EventRecord event = GenerateOSEvent(kOSEventMultiFinder, 0);
    PostEvent(event.what, event.message);

    return noErr;
}

/**
 * Process clipboard change event
 */
int16_t ProcessClipboardChangeEvent(void)
{
    EventRecord event = GenerateOSEvent(kOSEventClipboard, 0);
    PostEvent(event.what, event.message);

    return noErr;
}

/*---------------------------------------------------------------------------
 * Event Callback Management
 *---------------------------------------------------------------------------*/

/**
 * Register system event callback
 */
void* RegisterSystemEventCallback(int16_t eventType, SystemEventCallback callback, void* userData)
{
    EventCallback* newCallback = (EventCallback*)malloc(sizeof(EventCallback));
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
            free(toRemove);
            break;
        }
        current = &((*current)->next);
    }
}

/**
 * Notify event callbacks
 */
static void NotifyCallbacks(int16_t eventType, SystemEventContext* context)
{
    EventCallback* callback = g_eventCallbacks;
    while (callback) {
        if (callback->eventType == eventType && callback->callback) {
            bool consumed = callback->callback(context, callback->userData);
            if (consumed) {
                context->consumed = true;
            }
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
bool IsSystemEvent(const EventRecord* event)
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
int16_t GetSystemEventSubtype(const EventRecord* event)
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
int16_t FormatSystemEvent(const EventRecord* event, char* buffer, int16_t bufferSize)
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

    int16_t len = snprintf(buffer, bufferSize, "%s Event (0x%04X)", eventName, event->what);
    return (len < bufferSize) ? len : bufferSize - 1;
}

/**
 * Get system event priority
 */
int16_t GetSystemEventPriority(int16_t eventType, int16_t eventSubtype)
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
bool ValidateSystemEvent(const EventRecord* event)
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
        free(region);
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
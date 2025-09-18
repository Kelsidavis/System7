/**
 * @file SystemEvents.h
 * @brief System Event Management for System 7.1 Event Manager
 *
 * This file provides system-level event handling including update events,
 * activate/deactivate events, suspend/resume events, disk events, and
 * other system notifications.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include "EventTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System event priority levels */
enum {
    kSystemEventPriorityLow     = 0,    /* Low priority (background) */
    kSystemEventPriorityNormal  = 1,    /* Normal priority */
    kSystemEventPriorityHigh    = 2,    /* High priority (urgent) */
    kSystemEventPriorityCritical = 3    /* Critical (immediate) */
};

/* Update event types */
enum {
    kUpdateEventWindow      = 1,    /* Window content update */
    kUpdateEventDesktop     = 2,    /* Desktop update */
    kUpdateEventMenuBar     = 3,    /* Menu bar update */
    kUpdateEventCustom      = 100   /* Custom update region */
};

/* Activate event types */
enum {
    kActivateEventWindow    = 1,    /* Window activation */
    kActivateEventApp       = 2,    /* Application activation */
    kActivateEventSystem    = 3     /* System activation */
};

/* Disk event types */
enum {
    kDiskEventInserted      = 1,    /* Disk inserted */
    kDiskEventEjected       = 2,    /* Disk ejected */
    kDiskEventMounted       = 3,    /* Volume mounted */
    kDiskEventUnmounted     = 4,    /* Volume unmounted */
    kDiskEventError         = 5     /* Disk error */
};

/* OS event subtypes */
enum {
    kOSEventSuspend         = 1,    /* Application suspend */
    kOSEventResume          = 2,    /* Application resume */
    kOSEventMouseMoved      = 3,    /* Mouse moved */
    kOSEventMultiFinder     = 4,    /* MultiFinder event */
    kOSEventClipboard       = 5,    /* Clipboard changed */
    kOSEventPowerMgr        = 6,    /* Power management */
    kOSEventNetwork         = 7,    /* Network event */
    kOSEventCustom          = 100   /* Custom OS event */
};

/* System state flags */
enum {
    kSystemStateForeground  = (1 << 0),  /* App is in foreground */
    kSystemStateBackground  = (1 << 1),  /* App is in background */
    kSystemStateSuspended   = (1 << 2),  /* App is suspended */
    kSystemStateVisible     = (1 << 3),  /* App windows are visible */
    kSystemStateActive      = (1 << 4),  /* App is active */
    kSystemStateLowMemory   = (1 << 5),  /* Low memory condition */
    kSystemStateCriticalMem = (1 << 6),  /* Critical memory condition */
    kSystemStatePowerSave   = (1 << 7)   /* Power save mode */
};

/* Update region tracking */
typedef struct UpdateRegion {
    WindowPtr           window;         /* Window needing update */
    RgnHandle           updateRgn;      /* Region to update */
    int16_t             updateType;     /* Type of update */
    uint32_t            updateTime;     /* Time update was requested */
    int16_t             priority;       /* Update priority */
    struct UpdateRegion* next;          /* Next in queue */
} UpdateRegion;

/* Window activation state */
typedef struct WindowActivation {
    WindowPtr   window;             /* Window being activated/deactivated */
    bool        isActivating;       /* true = activating, false = deactivating */
    bool        wasActive;          /* Previous activation state */
    uint32_t    activationTime;     /* Time of activation change */
    int16_t     activationType;     /* Type of activation */
} WindowActivation;

/* Disk event information */
typedef struct DiskEventInfo {
    int16_t     eventType;          /* Type of disk event */
    int16_t     driveNumber;        /* Drive number */
    int16_t     refNum;             /* Volume reference number */
    char        volumeName[64];     /* Volume name */
    uint32_t    eventTime;          /* Time of event */
    int16_t     errorCode;          /* Error code (if any) */
} DiskEventInfo;

/* Application state tracking */
typedef struct AppStateInfo {
    uint16_t    currentState;       /* Current state flags */
    uint16_t    previousState;      /* Previous state flags */
    uint32_t    stateChangeTime;    /* Time of last state change */
    bool        suspendPending;     /* Suspend is pending */
    bool        resumePending;      /* Resume is pending */
    uint32_t    suspendTime;        /* Time app was suspended */
    uint32_t    memoryWarningTime;  /* Time of last memory warning */
} AppStateInfo;

/* System event context */
typedef struct SystemEventContext {
    EventRecord*    baseEvent;      /* Base event record */
    int16_t         eventSubtype;   /* System event subtype */
    void*           eventData;      /* Event-specific data */
    WindowPtr       targetWindow;   /* Target window (if any) */
    int16_t         priority;       /* Event priority */
    uint32_t        timestamp;      /* Event timestamp */
    bool            consumed;       /* Event has been consumed */
} SystemEventContext;

/* Callback function types */
typedef bool (*SystemEventCallback)(SystemEventContext* context, void* userData);
typedef void (*UpdateEventCallback)(WindowPtr window, RgnHandle updateRgn, void* userData);
typedef void (*ActivateEventCallback)(WindowPtr window, bool isActivating, void* userData);
typedef void (*DiskEventCallback)(DiskEventInfo* diskInfo, void* userData);
typedef void (*StateChangeCallback)(uint16_t oldState, uint16_t newState, void* userData);

/*---------------------------------------------------------------------------
 * Core System Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize system event management
 * @return Error code (0 = success)
 */
int16_t InitSystemEvents(void);

/**
 * Shutdown system event management
 */
void ShutdownSystemEvents(void);

/**
 * Process system events
 * Called regularly to handle pending system events
 */
void ProcessSystemEvents(void);

/**
 * Generate system event
 * @param eventType Type of system event
 * @param eventSubtype Event subtype
 * @param eventData Event-specific data
 * @param targetWindow Target window (can be NULL)
 * @return Error code
 */
int16_t GenerateSystemEvent(int16_t eventType, int16_t eventSubtype,
                           void* eventData, WindowPtr targetWindow);

/**
 * Post system event to queue
 * @param eventType Event type
 * @param message Event message
 * @param priority Event priority
 * @return Error code
 */
int16_t PostSystemEvent(int16_t eventType, int32_t message, int16_t priority);

/*---------------------------------------------------------------------------
 * Update Event Management
 *---------------------------------------------------------------------------*/

/**
 * Request window update
 * @param window Window to update
 * @param updateRgn Region to update (NULL for entire window)
 * @param updateType Type of update
 * @param priority Update priority
 * @return Error code
 */
int16_t RequestWindowUpdate(WindowPtr window, RgnHandle updateRgn,
                           int16_t updateType, int16_t priority);

/**
 * Process pending update events
 * @return Number of updates processed
 */
int16_t ProcessUpdateEvents(void);

/**
 * Check if window needs update
 * @param window Window to check
 * @return true if update is needed
 */
bool WindowNeedsUpdate(WindowPtr window);

/**
 * Get window update region
 * @param window Window to check
 * @return Update region handle (can be NULL)
 */
RgnHandle GetWindowUpdateRegion(WindowPtr window);

/**
 * Validate window update region
 * @param window Window to validate
 * @param validRgn Region that has been updated
 */
void ValidateWindowRegion(WindowPtr window, RgnHandle validRgn);

/**
 * Invalidate window region
 * @param window Window to invalidate
 * @param invalidRgn Region to invalidate
 */
void InvalidateWindowRegion(WindowPtr window, RgnHandle invalidRgn);

/**
 * Clear all pending updates for window
 * @param window Window to clear updates for
 */
void ClearWindowUpdates(WindowPtr window);

/*---------------------------------------------------------------------------
 * Window Activation Management
 *---------------------------------------------------------------------------*/

/**
 * Process window activation event
 * @param window Window being activated/deactivated
 * @param isActivating true for activation, false for deactivation
 * @return Error code
 */
int16_t ProcessWindowActivation(WindowPtr window, bool isActivating);

/**
 * Generate activate event
 * @param window Target window
 * @param isActivating true for activation
 * @param activationType Type of activation
 * @return Generated event
 */
EventRecord GenerateActivateEvent(WindowPtr window, bool isActivating, int16_t activationType);

/**
 * Check if window is active
 * @param window Window to check
 * @return true if window is active
 */
bool IsWindowActive(WindowPtr window);

/**
 * Set window activation state
 * @param window Window to modify
 * @param isActive New activation state
 */
void SetWindowActivation(WindowPtr window, bool isActive);

/**
 * Get front window
 * @return Currently active window
 */
WindowPtr GetFrontWindow(void);

/**
 * Set front window
 * @param window Window to bring to front
 * @return Previous front window
 */
WindowPtr SetFrontWindow(WindowPtr window);

/*---------------------------------------------------------------------------
 * Application State Management
 *---------------------------------------------------------------------------*/

/**
 * Process application suspend
 * @return Error code
 */
int16_t ProcessApplicationSuspend(void);

/**
 * Process application resume
 * @param convertClipboard true if clipboard conversion needed
 * @return Error code
 */
int16_t ProcessApplicationResume(bool convertClipboard);

/**
 * Generate suspend/resume event
 * @param isSuspend true for suspend, false for resume
 * @param convertClipboard true if clipboard conversion needed
 * @return Generated event
 */
EventRecord GenerateSuspendResumeEvent(bool isSuspend, bool convertClipboard);

/**
 * Check if application is suspended
 * @return true if application is suspended
 */
bool IsApplicationSuspended(void);

/**
 * Check if application is in foreground
 * @return true if application is in foreground
 */
bool IsApplicationInForeground(void);

/**
 * Set application state
 * @param newState New state flags
 */
void SetApplicationState(uint16_t newState);

/**
 * Get application state
 * @return Current state flags
 */
uint16_t GetApplicationState(void);

/**
 * Get application state info
 * @return Pointer to state information
 */
AppStateInfo* GetApplicationStateInfo(void);

/*---------------------------------------------------------------------------
 * Disk Event Management
 *---------------------------------------------------------------------------*/

/**
 * Process disk insertion
 * @param driveNumber Drive number
 * @param volumeName Volume name
 * @return Error code
 */
int16_t ProcessDiskInsertion(int16_t driveNumber, const char* volumeName);

/**
 * Process disk ejection
 * @param driveNumber Drive number
 * @return Error code
 */
int16_t ProcessDiskEjection(int16_t driveNumber);

/**
 * Generate disk event
 * @param eventType Type of disk event
 * @param driveNumber Drive number
 * @param refNum Volume reference number
 * @return Generated event
 */
EventRecord GenerateDiskEvent(int16_t eventType, int16_t driveNumber, int16_t refNum);

/**
 * Monitor disk events
 * @param callback Callback for disk events
 * @param userData User data for callback
 */
void MonitorDiskEvents(DiskEventCallback callback, void* userData);

/*---------------------------------------------------------------------------
 * OS Event Management
 *---------------------------------------------------------------------------*/

/**
 * Process mouse moved event
 * @param newPosition New mouse position
 * @return Error code
 */
int16_t ProcessMouseMovedEvent(Point newPosition);

/**
 * Generate OS event
 * @param eventSubtype OS event subtype
 * @param message Event message
 * @return Generated event
 */
EventRecord GenerateOSEvent(int16_t eventSubtype, int32_t message);

/**
 * Process MultiFinder event
 * @param eventData MultiFinder event data
 * @return Error code
 */
int16_t ProcessMultiFinderEvent(void* eventData);

/**
 * Process clipboard change event
 * @return Error code
 */
int16_t ProcessClipboardChangeEvent(void);

/*---------------------------------------------------------------------------
 * Memory Management Events
 *---------------------------------------------------------------------------*/

/**
 * Process low memory warning
 * @param memoryLevel Memory level (0-100)
 * @return Error code
 */
int16_t ProcessLowMemoryWarning(int16_t memoryLevel);

/**
 * Process critical memory warning
 * @return Error code
 */
int16_t ProcessCriticalMemoryWarning(void);

/**
 * Check memory warning state
 * @return true if memory warning is active
 */
bool IsMemoryWarningActive(void);

/*---------------------------------------------------------------------------
 * Power Management Events
 *---------------------------------------------------------------------------*/

/**
 * Process power save event
 * @param enterPowerSave true to enter, false to exit
 * @return Error code
 */
int16_t ProcessPowerSaveEvent(bool enterPowerSave);

/**
 * Process sleep event
 * @return Error code
 */
int16_t ProcessSleepEvent(void);

/**
 * Process wake event
 * @return Error code
 */
int16_t ProcessWakeEvent(void);

/**
 * Check if system is in power save mode
 * @return true if in power save mode
 */
bool IsSystemInPowerSave(void);

/*---------------------------------------------------------------------------
 * Event Callback Management
 *---------------------------------------------------------------------------*/

/**
 * Register system event callback
 * @param eventType Event type to monitor
 * @param callback Callback function
 * @param userData User data for callback
 * @return Registration handle
 */
void* RegisterSystemEventCallback(int16_t eventType, SystemEventCallback callback, void* userData);

/**
 * Unregister system event callback
 * @param handle Registration handle
 */
void UnregisterSystemEventCallback(void* handle);

/**
 * Register update event callback
 * @param callback Callback function
 * @param userData User data for callback
 * @return Registration handle
 */
void* RegisterUpdateEventCallback(UpdateEventCallback callback, void* userData);

/**
 * Register activation event callback
 * @param callback Callback function
 * @param userData User data for callback
 * @return Registration handle
 */
void* RegisterActivateEventCallback(ActivateEventCallback callback, void* userData);

/**
 * Register state change callback
 * @param callback Callback function
 * @param userData User data for callback
 * @return Registration handle
 */
void* RegisterStateChangeCallback(StateChangeCallback callback, void* userData);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Check if event is system event
 * @param event Event to check
 * @return true if it's a system event
 */
bool IsSystemEvent(const EventRecord* event);

/**
 * Get system event subtype
 * @param event System event
 * @return Event subtype
 */
int16_t GetSystemEventSubtype(const EventRecord* event);

/**
 * Format system event for logging
 * @param event Event to format
 * @param buffer Buffer for formatted string
 * @param bufferSize Size of buffer
 * @return Length of formatted string
 */
int16_t FormatSystemEvent(const EventRecord* event, char* buffer, int16_t bufferSize);

/**
 * Get system event priority
 * @param eventType Event type
 * @param eventSubtype Event subtype
 * @return Priority level
 */
int16_t GetSystemEventPriority(int16_t eventType, int16_t eventSubtype);

/**
 * Validate system event
 * @param event Event to validate
 * @return true if event is valid
 */
bool ValidateSystemEvent(const EventRecord* event);

/**
 * Reset system event state
 */
void ResetSystemEventState(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_EVENTS_H */
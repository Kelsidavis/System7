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

#include "SystemTypes.h"

#include "EventTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for application state queries. */
typedef struct AppStateInfo AppStateInfo;
typedef void (*DiskEventCallback)(void*);
typedef void (*SystemEventCallback)(void*);
typedef void (*UpdateEventCallback)(void*);
typedef void (*ActivateEventCallback)(void*);
typedef void (*StateChangeCallback)(void*);

/*---------------------------------------------------------------------------
 * Core System Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize system event management
 * @return Error code (0 = success)
 */
SInt16 InitSystemEvents(void);

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
 * @param message Event message
 * @param where Event location
 * @param modifiers Event modifier flags
 */
void GenerateSystemEvent(SInt16 eventType, SInt32 message,
                         Point where, SInt16 modifiers);

/**
 * Post system event to queue
 * @param eventType Event type
 * @param message Event message
 * @param priority Event priority
 * @return Error code
 */
SInt16 PostSystemEvent(SInt16 eventType, SInt32 message, SInt16 priority);

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
SInt16 RequestWindowUpdate(WindowPtr window, RgnHandle updateRgn,
                           SInt16 updateType, SInt16 priority);

/**
 * Process pending update events
 * @return Number of updates processed
 */
SInt16 ProcessUpdateEvents(void);

/**
 * Check if window needs update
 * @param window Window to check
 * @return true if update is needed
 */
Boolean WindowNeedsUpdate(WindowPtr window);

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
SInt16 ProcessWindowActivation(WindowPtr window, Boolean isActivating);

/**
 * Generate activate event
 * @param window Target window
 * @param isActivating true for activation
 * @param activationType Type of activation
 * @return Generated event
 */
EventRecord GenerateActivateEvent(WindowPtr window, Boolean isActivating, SInt16 activationType);

/**
 * Check if window is active
 * @param window Window to check
 * @return true if window is active
 */
Boolean IsWindowActive(WindowPtr window);

/**
 * Set window activation state
 * @param window Window to modify
 * @param isActive New activation state
 */
void SetWindowActivation(WindowPtr window, Boolean isActive);

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
SInt16 ProcessApplicationSuspend(void);

/**
 * Process application resume
 * @param convertClipboard true if clipboard conversion needed
 * @return Error code
 */
SInt16 ProcessApplicationResume(Boolean convertClipboard);

/**
 * Generate suspend/resume event
 * @param isSuspend true for suspend, false for resume
 * @param convertClipboard true if clipboard conversion needed
 * @return Generated event
 */
EventRecord GenerateSuspendResumeEvent(Boolean isSuspend, Boolean convertClipboard);

/**
 * Check if application is suspended
 * @return true if application is suspended
 */
Boolean IsApplicationSuspended(void);

/**
 * Check if application is in foreground
 * @return true if application is in foreground
 */
Boolean IsApplicationInForeground(void);

/**
 * Set application state
 * @param newState New state flags
 */
void SetApplicationState(UInt16 newState);

/**
 * Get application state
 * @return Current state flags
 */
UInt16 GetApplicationState(void);

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
SInt16 ProcessDiskInsertion(SInt16 driveNumber, const char* volumeName);

/**
 * Process disk ejection
 * @param driveNumber Drive number
 * @return Error code
 */
SInt16 ProcessDiskEjection(SInt16 driveNumber);

/**
 * Generate disk event
 * @param eventType Type of disk event
 * @param driveNumber Drive number
 * @param refNum Volume reference number
 * @return Generated event
 */
EventRecord GenerateDiskEvent(SInt16 eventType, SInt16 driveNumber, SInt16 refNum);

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
SInt16 ProcessMouseMovedEvent(Point newPosition);

/**
 * Generate OS event
 * @param eventSubtype OS event subtype
 * @param message Event message
 * @return Generated event
 */
EventRecord GenerateOSEvent(SInt16 eventSubtype, SInt32 message);

/**
 * Process MultiFinder event
 * @param eventData MultiFinder event data
 * @return Error code
 */
SInt16 ProcessMultiFinderEvent(void* eventData);

/**
 * Process clipboard change event
 * @return Error code
 */
SInt16 ProcessClipboardChangeEvent(void);

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
void* RegisterSystemEventCallback(SInt16 eventType, SystemEventCallback callback, void* userData);

/**
 * Unregister system event callback
 * @param handle Registration handle
 */
void UnregisterSystemEventCallback(void* handle);


/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Check if event is system event
 * @param event Event to check
 * @return true if it's a system event
 */
Boolean IsSystemEvent(const EventRecord* event);

/**
 * Get system event subtype
 * @param event System event
 * @return Event subtype
 */
SInt16 GetSystemEventSubtype(const EventRecord* event);

/**
 * Format system event for logging
 * @param event Event to format
 * @param buffer Buffer for formatted string
 * @param bufferSize Size of buffer
 * @return Length of formatted string
 */
SInt16 FormatSystemEvent(const EventRecord* event, char* buffer, SInt16 bufferSize);

/**
 * Get system event priority
 * @param eventType Event type
 * @param eventSubtype Event subtype
 * @return Priority level
 */
SInt16 GetSystemEventPriority(SInt16 eventType, SInt16 eventSubtype);

/**
 * Validate system event
 * @param event Event to validate
 * @return true if event is valid
 */
Boolean ValidateSystemEvent(const EventRecord* event);

/**
 * Reset system event state
 */
void ResetSystemEventState(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_EVENTS_H */

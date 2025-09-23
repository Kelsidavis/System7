/**
 * @file EventManager.h
 * @brief Complete Event Manager Implementation for System 7.1
 *
 * This file provides the main Event Manager API that maintains exact
 * compatibility with Mac OS System 7.1 event handling semantics while
 * providing modern cross-platform abstractions.
 *
 * The Event Manager is the core of all user interaction in System 7.1,
 * handling mouse events, keyboard input, system events, and providing
 * the foundation for all Mac applications.
 *
 * Features:
 * - Complete Mac OS Event Manager API compatibility
 * - Mouse event handling (clicks, drags, movement)
 * - Keyboard event processing (keys, modifiers, auto-repeat)
 * - System events (update, activate, suspend/resume)
 * - Event queue management with priority and filtering
 * - Double-click timing and multi-click detection
 * - Modern input abstraction for cross-platform support
 * - ADB Manager integration for low-level input
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventStructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */

/* Handle is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */

/* Point structure for mouse coordinates */
/* Point type defined in MacTypes.h */

/* Event types defined in EventTypes.h */

/* Event masks defined in EventTypes.h */

/* Event message masks defined in EventTypes.h */

/* OS event message codes defined in EventTypes.h */

/* Modifier key flags defined in EventTypes.h */

/* Obsolete event types defined in EventTypes.h */

/* EventRecord - the fundamental event structure */

/* Event queue element */
// 

/* KeyMap type defined in EventStructs.h */

/* Error codes */
/* Error codes defined elsewhere */;

/* Double-click detection */
// 

/* Mouse tracking state */
// 

/* Keyboard state and auto-repeat */
// 

/* System globals (would normally be at low memory addresses) */

/*---------------------------------------------------------------------------
 * Core Event Manager API
 *---------------------------------------------------------------------------*/

/**
 * Initialize the Event Manager
 * @param numEvents Number of event queue elements to allocate
 * @return Error code (0 = success)
 */
SInt16 InitEvents(SInt16 numEvents);

/**
 * Get next event matching mask (removes from queue)
 * @param eventMask Mask of event types to accept
 * @param theEvent Pointer to event record to fill
 * @return true if event found, false if null event
 */
Boolean GetNextEvent(short eventMask, EventRecord* theEvent);

/**
 * Wait for next event with idle processing
 * @param eventMask Mask of event types to accept
 * @param theEvent Pointer to event record to fill
 * @param sleep Maximum ticks to sleep
 * @param mouseRgn Region for mouse-moved events
 * @return true if event found, false if null event
 */
Boolean WaitNextEvent(short eventMask, EventRecord* theEvent,
                   UInt32 sleep, RgnHandle mouseRgn);

/**
 * Check if event is available (doesn't remove from queue)
 * @param eventMask Mask of event types to accept
 * @param theEvent Pointer to event record to fill
 * @return true if event found, false if null event
 */
Boolean EventAvail(short eventMask, EventRecord* theEvent);

/**
 * Post an event to the queue
 * @param eventNum Event type
 * @param eventMsg Event message
 * @return Error code (0 = success, 1 = not enabled)
 */
SInt16 PostEvent(SInt16 eventNum, SInt32 eventMsg);

/**
 * Post an event with queue element return
 * @param eventCode Event type
 * @param eventMsg Event message
 * @param qEl Pointer to receive queue element pointer
 * @return Error code
 */
SInt16 PPostEvent(SInt16 eventCode, SInt32 eventMsg, EvQEl** qEl);

/**
 * OS-level event checking (low-level)
 * @param mask Event mask
 * @param theEvent Event record to fill
 * @return true if event found
 */
Boolean OSEventAvail(SInt16 mask, EventRecord* theEvent);

/**
 * Get OS event (removes from queue, low-level)
 * @param mask Event mask
 * @param theEvent Event record to fill
 * @return true if event found
 */
Boolean GetOSEvent(SInt16 mask, EventRecord* theEvent);

/**
 * Flush events from queue
 * @param whichMask Mask of events to remove
 * @param stopMask Mask of events that stop flushing
 */
void FlushEvents(SInt16 whichMask, SInt16 stopMask);

/*---------------------------------------------------------------------------
 * Mouse Event API
 *---------------------------------------------------------------------------*/

/**
 * Get current mouse position
 * @param mouseLoc Pointer to Point to fill with position
 */
void GetMouse(Point* mouseLoc);

/**
 * Check if mouse button is currently down
 * @return true if button is down
 */
Boolean Button(void);

/**
 * Check if mouse button is still down (since last check)
 * @return true if button is still down
 */
Boolean StillDown(void);

/**
 * Wait for mouse button release
 * @return true if button was released
 */
Boolean WaitMouseUp(void);

/**
 * Track mouse dragging
 * @param startPt Starting point of drag
 * @param limitRect Rectangle to limit dragging
 * @param slopRect Rectangle for initial slop
 * @param axis Constraint axis (0=none, 1=horizontal, 2=vertical)
 * @return Final mouse position offset
 */
SInt32 DragTheRgn(Point startPt, const struct Rect* limitRect,
                   const struct Rect* slopRect, SInt16 axis);

/*---------------------------------------------------------------------------
 * Keyboard Event API
 *---------------------------------------------------------------------------*/

/**
 * Get current keyboard state
 * @param theKeys 128-bit keymap to fill
 */
void GetKeys(KeyMap theKeys);

/**
 * Translate key code using KCHR resource
 * @param transData Pointer to KCHR resource data
 * @param keycode Key code and modifier flags
 * @param state Pointer to translation state
 * @return Character code or function key code
 */
SInt32 KeyTranslate(const void* transData, UInt16 keycode, UInt32* state);

/**
 * Compatibility name for KeyTranslate
 */
#define KeyTrans KeyTranslate

/**
 * Check for Command-Period abort
 * @return true if user pressed Cmd-Period
 */
Boolean CheckAbort(void);

/*---------------------------------------------------------------------------
 * Timing API
 *---------------------------------------------------------------------------*/

/**
 * Get system tick count
 * @return Ticks since system startup
 */
UInt32 TickCount(void);

/**
 * Get double-click time threshold
 * @return Ticks for double-click timing
 */
UInt32 GetDblTime(void);

/**
 * Get caret blink time
 * @return Ticks for caret blink rate
 */
UInt32 GetCaretTime(void);

/**
 * Set system event mask
 * @param mask New event mask
 */
void SetEventMask(SInt16 mask);

/*---------------------------------------------------------------------------
 * Event Manager Extended API
 *---------------------------------------------------------------------------*/

/**
 * Set key repeat thresholds
 * @param delay Initial delay before repeat (ticks)
 * @param rate Rate of repeat (ticks between repeats)
 */
void SetKeyRepeat(UInt16 delay, UInt16 rate);

/**
 * Get global event manager state
 * @return Pointer to global state structure
 */
EventMgrGlobals* GetEventMgrGlobals(void);

/**
 * Generate system event (for internal use)
 * @param eventType Type of event to generate
 * @param message Event message
 * @param where Event location
 * @param modifiers Event modifiers
 */
void GenerateSystemEvent(SInt16 eventType, SInt32 message, Point where, SInt16 modifiers);

/**
 * Process null event (idle processing)
 */
void ProcessNullEvent(void);

/**
 * Set the front window for event targeting
 * @param window Window to receive events
 */
void SetEventWindow(WindowPtr window);

/**
 * Get the front window for event targeting
 * @return Current front window
 */
WindowPtr GetEventWindow(void);

/*---------------------------------------------------------------------------
 * Modern Input Integration API
 *---------------------------------------------------------------------------*/

/**
 * Initialize modern input system
 * @param platform Platform identifier (X11, Cocoa, Win32, etc.)
 * @return Error code
 */
SInt16 InitModernInput(const char* platform);

/**
 * Process modern input events
 * Should be called regularly from main event loop
 */
void ProcessModernInput(void);

/**
 * Shutdown modern input system
 */
void ShutdownModernInput(void);

/**
 * Enable/disable modern input features
 * @param multiTouch Enable multi-touch support
 * @param gestures Enable gesture recognition
 * @param accessibility Enable accessibility features
 */
void ConfigureModernInput(Boolean multiTouch, Boolean gestures, Boolean accessibility);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_MANAGER_H */

/**
 * @file MouseEvents.h
 * @brief Mouse Event Processing for System 7.1 Event Manager
 *
 * This file provides comprehensive mouse event handling including
 * clicks, drags, movement detection, and modern mouse features
 * like scroll wheels and multi-button mice.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

#ifndef MOUSE_EVENTS_H
#define MOUSE_EVENTS_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

#include "EventTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct MouseTrackingState MouseTrackingState;

/* Mouse button identifiers */

/* Mouse event subtypes */

/* Drag operation types */

/* Mouse tracking state */

/* Multi-click detection */

/* Mouse region tracking */

/* Mouse event context */

/* Callback function types */

/*---------------------------------------------------------------------------
 * Core Mouse Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize mouse event system
 * @return Error code (0 = success)
 */
SInt16 InitMouseEvents(void);

/**
 * Shutdown mouse event system
 */
void ShutdownMouseEvents(void);

/**
 * Process a raw mouse event and generate appropriate Mac events
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param buttonMask Button state bitmask
 * @param modifiers Modifier key state
 * @param timestamp Event timestamp
 * @return Number of events generated
 */
SInt16 ProcessRawMouseEvent(SInt16 x, SInt16 y, SInt16 buttonMask,
                            SInt16 modifiers, UInt32 timestamp);

/**
 * Get current mouse position
 * @param mouseLoc Pointer to Point to receive position
 */
/* Global (screen) coordinates - note this differs from the Mac OS GetMouse,
 * which is port-local. Use GetMouseLocal when you want the port's space. */
void GetMouse(Point* mouseLoc);
void GetMouseLocal(Point* mouseLoc);

/**
 * Get mouse position in local coordinates
 * @param window Target window
 * @param mouseLoc Pointer to Point to receive position
 */
void GetLocalMouse(WindowPtr window, Point* mouseLoc);

/**
 * Check if mouse button is currently pressed
 * @return true if primary button is down
 */
Boolean Button(void);

/**
 * Check if specific mouse button is pressed
 * @param buttonID Button identifier
 * @return true if specified button is down
 */
Boolean ButtonState(SInt16 buttonID);

/**
 * Check if mouse is still down since last check
 * @return true if button is still down
 */
Boolean StillDown(void);

/**
 * Wait for mouse button release
 * @return true when button is released
 */
Boolean WaitMouseUp(void);

/*---------------------------------------------------------------------------
 * Click Detection and Multi-Click Support
 *---------------------------------------------------------------------------*/

/**
 * Initialize click detection
 * @param tolerance Pixel tolerance for multi-clicks
 * @param timeThreshold Time threshold for multi-clicks (ticks)
 */
void InitClickDetection(SInt16 tolerance, UInt32 timeThreshold);

/**
 * Process mouse click and determine click count
 * @param clickPoint Location of click
 * @param timestamp Time of click
 * @return Click count (1, 2, 3, etc.)
 */
SInt16 ProcessMouseClick(Point clickPoint, UInt32 timestamp);

/**
 * Reset click sequence
 */
void ResetClickSequence(void);

/**
 * Get current click count
 * @return Current click count
 */
SInt16 GetClickCount(void);

/**
 * Check if point is within double-click tolerance
 * @param pt1 First point
 * @param pt2 Second point
 * @param tolerance Pixel tolerance
 * @return true if points are within tolerance
 */
Boolean PointsWithinTolerance(Point pt1, Point pt2, SInt16 tolerance);

/*---------------------------------------------------------------------------
 * Mouse Tracking and Dragging
 *---------------------------------------------------------------------------*/

/**
 * Start mouse tracking
 * @param startPoint Starting position
 * @param dragType Type of drag operation
 * @param dragData Optional drag-specific data
 * @return true if tracking started successfully
 */
Boolean StartMouseTracking(Point startPoint, SInt16 dragType, void* dragData);

/**
 * Update mouse tracking
 * @param currentPoint Current mouse position
 * @param modifiers Current modifier keys
 * @return true if tracking should continue
 */
Boolean UpdateMouseTracking(Point currentPoint, SInt16 modifiers);

/**
 * End mouse tracking
 * @param endPoint Final position
 * @return Drag result code
 */
SInt16 EndMouseTracking(Point endPoint);

/**
 * Check if currently in drag operation
 * @return true if dragging
 */
Boolean IsMouseDragging(void);

/**
 * Get current drag type
 * @return Drag type identifier
 */
SInt16 GetDragType(void);

/**
 * Track mouse movement within rectangle
 * @param constraintRect Rectangle to constrain movement
 * @param callback Function to call on movement
 * @param userData User data for callback
 * @return Final mouse position
 */
Point TrackMouseInRect(const Rect* constraintRect, MouseTrackingCallback callback, void* userData);

/*---------------------------------------------------------------------------
 * Mouse Region Management
 *---------------------------------------------------------------------------*/

/**
 * Add mouse tracking region
 * @param bounds Region boundaries
 * @param userData User data for region
 * @return Region handle
 */
MouseRegion* AddMouseRegion(const Rect* bounds, void* userData);

/**
 * Remove mouse tracking region
 * @param region Region to remove
 */
void RemoveMouseRegion(MouseRegion* region);

/**
 * Update mouse region tracking
 * @param mousePos Current mouse position
 */
void UpdateMouseRegions(Point mousePos);

/**
 * Get mouse region at point
 * @param point Point to check
 * @return Region at point, or NULL
 */
MouseRegion* GetMouseRegionAtPoint(Point point);

/*---------------------------------------------------------------------------
 * Modern Mouse Features
 *---------------------------------------------------------------------------*/

/**
 * Process scroll wheel event
 * @param deltaX Horizontal scroll delta
 * @param deltaY Vertical scroll delta
 * @param modifiers Modifier keys
 * @param timestamp Event timestamp
 * @return true if event was processed
 */
Boolean ProcessScrollWheelEvent(SInt16 deltaX, SInt16 deltaY,
                            SInt16 modifiers, UInt32 timestamp);

/**
 * Set mouse acceleration
 * @param acceleration Acceleration factor (1.0 = no acceleration)
 */
void SetMouseAcceleration(float acceleration);

/**
 * Get mouse acceleration
 * @return Current acceleration factor
 */
float GetMouseAcceleration(void);

/**
 * Set mouse sensitivity
 * @param sensitivity Sensitivity factor (1.0 = normal)
 */
void SetMouseSensitivity(float sensitivity);

/**
 * Get mouse sensitivity
 * @return Current sensitivity factor
 */
float GetMouseSensitivity(void);

/**
 * Enable/disable mouse button mapping
 * @param leftHanded true for left-handed button mapping
 */
void SetMouseButtonMapping(Boolean leftHanded);

/*---------------------------------------------------------------------------
 * Event Generation
 *---------------------------------------------------------------------------*/

/**
 * Generate mouse down event
 * @param position Mouse position
 * @param buttonID Button identifier
 * @param clickCount Click count
 * @param modifiers Modifier keys
 * @return Generated event
 */
EventRecord GenerateMouseDownEvent(Point position, SInt16 buttonID,
                                  SInt16 clickCount, SInt16 modifiers);

/**
 * Generate mouse up event
 * @param position Mouse position
 * @param buttonID Button identifier
 * @param modifiers Modifier keys
 * @return Generated event
 */
EventRecord GenerateMouseUpEvent(Point position, SInt16 buttonID, SInt16 modifiers);

/**
 * Generate mouse moved event
 * @param position Mouse position
 * @param modifiers Modifier keys
 * @return Generated event
 */
EventRecord GenerateMouseMovedEvent(Point position, SInt16 modifiers);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Convert screen coordinates to window local coordinates
 * @param window Target window
 * @param globalPt Point in global coordinates
 * @return Point in local coordinates
 */
Point GlobalToLocal(WindowPtr window, Point globalPt);

/**
 * Convert window local coordinates to screen coordinates
 * @param window Source window
 * @param localPt Point in local coordinates
 * @return Point in global coordinates
 */
Point LocalToGlobal(WindowPtr window, Point localPt);

/**
 * Calculate distance between two points
 * @param pt1 First point
 * @param pt2 Second point
 * @return Distance in pixels
 */
SInt16 PointDistance(Point pt1, Point pt2);

/**
 * Check if point is inside rectangle
 * @param pt Point to check
 * @param rect Rectangle to check against
 * @return true if point is inside rectangle
 */
Boolean PointInRect(Point pt, const Rect* rect);

/**
 * Get mouse tracking state
 * @return Pointer to current tracking state
 */
MouseTrackingState* GetMouseTrackingState(void);

/**
 * Reset mouse tracking state
 */
void ResetMouseTrackingState(void);

#ifdef __cplusplus
}
#endif

#endif /* MOUSE_EVENTS_H */

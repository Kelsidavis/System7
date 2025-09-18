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

#include "EventTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mouse button identifiers */
enum {
    kMouseButtonNone    = 0,    /* No button */
    kMouseButtonLeft    = 1,    /* Left (primary) button */
    kMouseButtonRight   = 2,    /* Right (secondary) button */
    kMouseButtonMiddle  = 3,    /* Middle (wheel) button */
    kMouseButtonBack    = 4,    /* Back button */
    kMouseButtonForward = 5,    /* Forward button */
    kMaxMouseButtons    = 8     /* Maximum supported buttons */
};

/* Mouse event subtypes */
enum {
    kMouseEventClick    = 1,    /* Single click */
    kMouseEventDrag     = 2,    /* Drag operation */
    kMouseEventMove     = 3,    /* Movement only */
    kMouseEventWheel    = 4,    /* Scroll wheel */
    kMouseEventEnter    = 5,    /* Mouse entered region */
    kMouseEventExit     = 6     /* Mouse exited region */
};

/* Drag operation types */
enum {
    kDragTypeNone       = 0,    /* No drag */
    kDragTypeContent    = 1,    /* Content drag */
    kDragTypeWindow     = 2,    /* Window drag */
    kDragTypeGrow       = 3,    /* Window resize */
    kDragTypeGoAway     = 4,    /* Close box */
    kDragTypeZoom       = 5,    /* Zoom box */
    kDragTypeScrollBar  = 6,    /* Scroll bar */
    kDragTypeCustom     = 100   /* Custom drag type */
};

/* Mouse tracking state */
typedef struct MouseTrackingState {
    Point    currentPos;        /* Current mouse position */
    Point    lastPos;           /* Previous mouse position */
    Point    startPos;          /* Position where tracking started */
    uint32_t startTime;         /* Time tracking started */
    uint32_t lastMoveTime;      /* Time of last movement */
    uint32_t lastClickTime;     /* Time of last click */
    int16_t  buttonState;       /* Current button state (bitmask) */
    int16_t  lastButtonState;   /* Previous button state */
    bool     isDragging;        /* Currently in drag operation */
    bool     hasMovedSinceClick; /* Moved since last click */
    int16_t  dragType;          /* Type of drag operation */
    void*    dragData;          /* Drag-specific data */
} MouseTrackingState;

/* Multi-click detection */
typedef struct MultiClickState {
    Point    clickLocation;     /* Location of click sequence */
    uint32_t clickTime;         /* Time of current click */
    int16_t  clickCount;        /* Number of clicks in sequence */
    int16_t  maxClickCount;     /* Maximum clicks to track */
    int16_t  clickTolerance;    /* Pixel tolerance for multi-clicks */
    uint32_t clickTimeThreshold; /* Time threshold for multi-clicks */
} MultiClickState;

/* Mouse region tracking */
typedef struct MouseRegion {
    Rect     bounds;            /* Region bounds */
    bool     mouseInside;       /* Mouse currently inside */
    bool     trackingEnabled;   /* Tracking enabled for this region */
    void*    userData;          /* User data for callbacks */
    struct MouseRegion* next;   /* Next region in list */
} MouseRegion;

/* Mouse event context */
typedef struct MouseEventContext {
    EventRecord*        baseEvent;     /* Base event record */
    MouseTrackingState* trackingState; /* Mouse tracking state */
    WindowPtr           targetWindow;  /* Target window */
    Point               localPoint;    /* Point in local coordinates */
    Point               globalPoint;   /* Point in global coordinates */
    int16_t             modifiers;     /* Modifier keys */
    uint32_t            timestamp;     /* Event timestamp */
    bool                consumed;      /* Event has been consumed */
} MouseEventContext;

/* Callback function types */
typedef bool (*MouseEventCallback)(MouseEventContext* context, void* userData);
typedef void (*MouseTrackingCallback)(Point mousePos, bool buttonDown, void* userData);
typedef bool (*DragTrackingCallback)(Point startPos, Point currentPos, int16_t dragType, void* userData);

/*---------------------------------------------------------------------------
 * Core Mouse Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize mouse event system
 * @return Error code (0 = success)
 */
int16_t InitMouseEvents(void);

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
int16_t ProcessRawMouseEvent(int16_t x, int16_t y, int16_t buttonMask,
                            int16_t modifiers, uint32_t timestamp);

/**
 * Get current mouse position
 * @param mouseLoc Pointer to Point to receive position
 */
void GetMouse(Point* mouseLoc);

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
bool Button(void);

/**
 * Check if specific mouse button is pressed
 * @param buttonID Button identifier
 * @return true if specified button is down
 */
bool ButtonState(int16_t buttonID);

/**
 * Check if mouse is still down since last check
 * @return true if button is still down
 */
bool StillDown(void);

/**
 * Wait for mouse button release
 * @return true when button is released
 */
bool WaitMouseUp(void);

/*---------------------------------------------------------------------------
 * Click Detection and Multi-Click Support
 *---------------------------------------------------------------------------*/

/**
 * Initialize click detection
 * @param tolerance Pixel tolerance for multi-clicks
 * @param timeThreshold Time threshold for multi-clicks (ticks)
 */
void InitClickDetection(int16_t tolerance, uint32_t timeThreshold);

/**
 * Process mouse click and determine click count
 * @param clickPoint Location of click
 * @param timestamp Time of click
 * @return Click count (1, 2, 3, etc.)
 */
int16_t ProcessMouseClick(Point clickPoint, uint32_t timestamp);

/**
 * Reset click sequence
 */
void ResetClickSequence(void);

/**
 * Get current click count
 * @return Current click count
 */
int16_t GetClickCount(void);

/**
 * Check if point is within double-click tolerance
 * @param pt1 First point
 * @param pt2 Second point
 * @param tolerance Pixel tolerance
 * @return true if points are within tolerance
 */
bool PointsWithinTolerance(Point pt1, Point pt2, int16_t tolerance);

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
bool StartMouseTracking(Point startPoint, int16_t dragType, void* dragData);

/**
 * Update mouse tracking
 * @param currentPoint Current mouse position
 * @param modifiers Current modifier keys
 * @return true if tracking should continue
 */
bool UpdateMouseTracking(Point currentPoint, int16_t modifiers);

/**
 * End mouse tracking
 * @param endPoint Final position
 * @return Drag result code
 */
int16_t EndMouseTracking(Point endPoint);

/**
 * Check if currently in drag operation
 * @return true if dragging
 */
bool IsMouseDragging(void);

/**
 * Get current drag type
 * @return Drag type identifier
 */
int16_t GetDragType(void);

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
bool ProcessScrollWheelEvent(int16_t deltaX, int16_t deltaY,
                            int16_t modifiers, uint32_t timestamp);

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
void SetMouseButtonMapping(bool leftHanded);

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
EventRecord GenerateMouseDownEvent(Point position, int16_t buttonID,
                                  int16_t clickCount, int16_t modifiers);

/**
 * Generate mouse up event
 * @param position Mouse position
 * @param buttonID Button identifier
 * @param modifiers Modifier keys
 * @return Generated event
 */
EventRecord GenerateMouseUpEvent(Point position, int16_t buttonID, int16_t modifiers);

/**
 * Generate mouse moved event
 * @param position Mouse position
 * @param modifiers Modifier keys
 * @return Generated event
 */
EventRecord GenerateMouseMovedEvent(Point position, int16_t modifiers);

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
int16_t PointDistance(Point pt1, Point pt2);

/**
 * Check if point is inside rectangle
 * @param pt Point to check
 * @param rect Rectangle to check against
 * @return true if point is inside rectangle
 */
bool PointInRect(Point pt, const Rect* rect);

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
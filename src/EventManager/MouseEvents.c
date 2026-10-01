#include "MemoryMgr/MemoryManager.h"
/* #include "SystemTypes.h" */
#include "EventManager/EventManagerInternal.h"
#include <stdlib.h>
#include <string.h>
/**
 * @file MouseEvents.c
 * @brief Mouse Event Processing Implementation for System 7.1
 *
 * This file provides comprehensive mouse event handling including
 * clicks, drags, movement detection, double-click timing, and
 * modern mouse features like scroll wheels and multi-button support.
 *
 * Copyright (c) 2024 System 7.1 Portable Project
 * All rights reserved.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include <time.h>

#include "EventManager/MouseEvents.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventStructs.h"
#include "EventManager/EventLogging.h"
/* Simple integer square root for distance calculations */
static inline int isqrt(int n) {
    if (n < 0) return -1;
    if (n < 2) return n;
    int x = n;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}
#define sqrt(x) isqrt((int)(x))


/*---------------------------------------------------------------------------
 * Global State
 *---------------------------------------------------------------------------*/

/* MouseTrackingState definition */
struct MouseTrackingState {
    Point       currentPos;
    Point       lastPos;
    Point       startPos;
    Point       lastClickPos;
    UInt32      lastClickTime;
    UInt32      startTime;
    UInt32      lastMoveTime;
    UInt16      clickCount;
    UInt8       buttonState;
    Boolean     tracking;
    Boolean     dragging;
    Boolean     isDragging;
    Boolean     hasMovedSinceClick;
    SInt16      dragType;
    void*       dragData;
};

/* MultiClickState definition */
typedef struct MultiClickState {
    Point       position;
    Point       clickLocation;
    UInt32      time;
    UInt32      clickTime;
    UInt32      clickTimeThreshold;
    UInt16      clickCount;
    UInt16      maxClickCount;
    SInt16      clickTolerance;
} MultiClickState;

/* MouseRegion definition */
typedef struct MouseRegion {
    struct MouseRegion* next;
    Rect        bounds;
    Boolean     trackingEnabled;
    Boolean     mouseInside;
    UInt16      regionID;
    void*       userData;
} MouseRegion;

/* Mouse button constants */
enum {
    kMouseButtonLeft = 0,
    kMouseButtonRight = 1,
    kMouseButtonMiddle = 2
};

/* Drag type constants */
enum {
    kDragTypeNone = 0,
    kDragTypeWindow = 1,
    kDragTypeIcon = 2,
    kDragTypeText = 3,
    kDragTypeFile = 4,
    kDragTypeContent = 5
};

/* Mouse constants */
#define kDragStartThreshold 5      /* pixels */
#define kDoubleClickTolerance 5    /* pixels */
#define kMaxClickCount 3           /* triple-click max */
#define kMouseMoveThreshold 2      /* pixels */
#define kMaxMouseButtons 3         /* left, right, middle */

/* Callback type for mouse tracking */
typedef void (*MouseTrackingCallback)(Point mousePos, void* userData);

/* Mouse tracking state */
static struct MouseTrackingState g_mouseTracking = {0};
static MultiClickState g_multiClick = {0};
static Boolean g_mouseInitialized = false;

/* Mouse regions */
static MouseRegion* g_mouseRegions = NULL;

/* Mouse settings */
static float g_mouseAcceleration = 1.0f;
static float g_mouseSensitivity = 1.0f;
static Boolean g_leftHandedMouse = false;

/* Double-click detection */
static SInt16 g_clickCount = 0;

/* Button state tracking */
static SInt16 g_currentButtonState = 0;
static SInt16 g_lastButtonState = 0;

/* External references */
extern void UpdateMouseState(Point newPos, UInt8 buttonState);
/* PostEvent declared in EventManager.h */
extern UInt32 TickCount(void);
extern UInt32 GetDblTime(void);

/*---------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/

#if 0  /* UNUSED forward declarations for commented-out functions */
static SInt16 DetectClickCount(Point clickPos, UInt32 clickTime);
static void UpdateMouseRegionTracking(Point mousePos);
static void ApplyMouseAcceleration(SInt16* deltaX, SInt16* deltaY);
static Boolean IsWithinClickTolerance(Point pt1, Point pt2);
#endif
static SInt16 MapMouseButton(SInt16 buttonID);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/**
 * Calculate distance between two points
 */
SInt16 PointDistance(Point pt1, Point pt2)
{
    SInt16 dx = pt2.h - pt1.h;
    SInt16 dy = pt2.v - pt1.v;
    return (SInt16)sqrt(dx * dx + dy * dy);
}

/**
 * Check if point is inside rectangle
 */
Boolean PointInRect(Point pt, const Rect* rect)
{
    if (!rect) return false;

    return (pt.h >= rect->left && pt.h < rect->right &&
            pt.v >= rect->top && pt.v < rect->bottom);
}

#if 0  /* UNUSED: IsWithinClickTolerance - orphaned by commenting out DetectClickCount */
/**
 * Check if points are within double-click tolerance
 */
static Boolean IsWithinClickTolerance(Point pt1, Point pt2)
{
    SInt16 distance = PointDistance(pt1, pt2);
    return distance <= kDoubleClickTolerance;
}
#endif /* IsWithinClickTolerance */

#if 0  /* UNUSED: ApplyMouseAcceleration - preserved for possible future use */
/**
 * Apply mouse acceleration to movement delta
 */
static void ApplyMouseAcceleration(SInt16* deltaX, SInt16* deltaY)
{
    if (!deltaX || !deltaY) return;

    float magnitude = sqrt((*deltaX) * (*deltaX) + (*deltaY) * (*deltaY));

    if (magnitude > 0 && g_mouseAcceleration != 1.0f) {
        float accelerated = magnitude * g_mouseAcceleration;
        float scale = accelerated / magnitude;

        *deltaX = (SInt16)((*deltaX) * scale * g_mouseSensitivity);
        *deltaY = (SInt16)((*deltaY) * scale * g_mouseSensitivity);
    } else {
        *deltaX = (SInt16)((*deltaX) * g_mouseSensitivity);
        *deltaY = (SInt16)((*deltaY) * g_mouseSensitivity);
    }
}
#endif /* ApplyMouseAcceleration */

/**
 * Map physical button to logical button (for left-handed support)
 */
static SInt16 MapMouseButton(SInt16 buttonID)
{
    if (g_leftHandedMouse) {
        switch (buttonID) {
            case kMouseButtonLeft:
                return kMouseButtonRight;
            case kMouseButtonRight:
                return kMouseButtonLeft;
            default:
                return buttonID;
        }
    }
    return buttonID;
}

#if 0  /* UNUSED: DetectClickCount - preserved for possible future use */
/**
 * Detect multi-click count
 */
static SInt16 DetectClickCount(Point clickPos, UInt32 clickTime)
{
    UInt32 doubleTime = GetDblTime();
    UInt32 timeDiff = clickTime - g_lastClickTime;

    if (timeDiff <= doubleTime && IsWithinClickTolerance(clickPos, g_lastClickPos)) {
        g_clickCount++;
        if (g_clickCount > kMaxClickCount) {
            g_clickCount = kMaxClickCount;
        }
    } else {
        g_clickCount = 1;
    }

    g_lastClickPos = clickPos;
    g_lastClickTime = clickTime;

    return g_clickCount;
}
#endif /* DetectClickCount */

/*---------------------------------------------------------------------------
 * Core Mouse Event API
 *---------------------------------------------------------------------------*/

/**
 * Initialize mouse event system
 */
SInt16 InitMouseEvents(void)
{
    if (g_mouseInitialized) {
        return noErr;
    }

    /* Initialize mouse tracking state */
    memset(&g_mouseTracking, 0, sizeof(MouseTrackingState));
    memset(&g_multiClick, 0, sizeof(MultiClickState));

    /* Set up multi-click detection */
    g_multiClick.maxClickCount = kMaxClickCount;
    g_multiClick.clickTolerance = kDoubleClickTolerance;
    g_multiClick.clickTimeThreshold = kDefaultDoubleClickTime;

    /* Initialize button state */
    g_currentButtonState = 0;
    g_lastButtonState = 0;

    /* Set default mouse settings */
    g_mouseAcceleration = 1.0f;
    g_mouseSensitivity = 1.0f;
    g_leftHandedMouse = false;

    g_mouseInitialized = true;
    return noErr;
}

/**
 * Shutdown mouse event system
 */
void ShutdownMouseEvents(void)
{
    if (!g_mouseInitialized) {
        return;
    }

    /* Free mouse regions */
    MouseRegion* region = g_mouseRegions;
    while (region) {
        MouseRegion* next = region->next;
        DisposePtr((Ptr)region);
        region = next;
    }
    g_mouseRegions = NULL;

    g_mouseInitialized = false;
}

/**
 * Process raw mouse event
 * DISABLED when MODERN_INPUT_ONLY=1 (ModernInput is the sole producer)
 */
SInt16 ProcessRawMouseEvent(SInt16 x, SInt16 y, SInt16 buttonMask,
                            SInt16 modifiers, UInt32 timestamp)
{
#if !MODERN_INPUT_ONLY
    /* Legacy producer - only active when MODERN_INPUT_ONLY is not defined */
    if (!g_mouseInitialized) {
        return 0;
    }

    Point newPos = {y, x}; /* Mac coordinates: v, h */
    Point oldPos = g_mouseTracking.currentPos;
    SInt16 eventsGenerated = 0;

    /* Update tracking state */
    g_mouseTracking.lastPos = g_mouseTracking.currentPos;
    g_mouseTracking.currentPos = newPos;
    g_mouseTracking.lastMoveTime = timestamp;

    /* Check for mouse movement */
    if (newPos.h != oldPos.h || newPos.v != oldPos.v) {
        /* Generate mouse moved event if significant movement */
        SInt16 distance = PointDistance(oldPos, newPos);
        if (distance >= kMouseMoveThreshold) {
            PostEvent(osEvt, (mouseMovedMessage << 24));
            eventsGenerated++;

            /* Update system mouse position */
            UpdateMouseState(newPos, (UInt8)buttonMask);

            /* Update mouse region tracking */
            UpdateMouseRegionTracking(newPos);
        }
    }

    /* Process button state changes */
    SInt16 buttonChanges = buttonMask ^ g_lastButtonState;

    for (SInt16 button = 0; button < kMaxMouseButtons; button++) {
        SInt16 buttonBit = 1 << button;

        if (buttonChanges & buttonBit) {
            SInt16 mappedButton = MapMouseButton(button + 1);

            if (buttonMask & buttonBit) {
                /* Button pressed */
                SInt16 clickCount = DetectClickCount(newPos, timestamp);

                /* Generate mouse down event */
                SInt32 message = 0;
                if (mappedButton == kMouseButtonLeft) {
                    EVT_LOG_DEBUG("MouseEvents: Posting mouseDown event at (%d,%d)\n", newPos.h, newPos.v);
                    PostEvent(mouseDown, message);
                    eventsGenerated++;
                }

                /* Start tracking if this is the primary button */
                if (mappedButton == kMouseButtonLeft) {
                    g_mouseTracking.startPos = newPos;
                    g_mouseTracking.startTime = timestamp;
                    g_mouseTracking.hasMovedSinceClick = false;
                }

            } else {
                /* Button released */
                if (mappedButton == kMouseButtonLeft) {
                    PostEvent(mouseUp, 0);
                    eventsGenerated++;
                }

                /* End tracking */
                if (g_mouseTracking.isDragging) {
                    g_mouseTracking.isDragging = false;
                    g_mouseTracking.dragType = kDragTypeNone;
                }
            }
        }
    }

    /* Update button state */
    g_lastButtonState = g_currentButtonState;
    g_currentButtonState = buttonMask;
    g_lastButtonChangeTime = timestamp;

    /* Check for drag start */
    if ((buttonMask & 1) && !g_mouseTracking.isDragging) {
        SInt16 dragDistance = PointDistance(g_mouseTracking.startPos, newPos);
        if (dragDistance >= kDragStartThreshold) {
            g_mouseTracking.isDragging = true;
            g_mouseTracking.hasMovedSinceClick = true;
            g_mouseTracking.dragType = kDragTypeContent;
        }
    }

    return eventsGenerated;
#else
    /* MODERN_INPUT_ONLY: ModernInput.c is the sole producer */
    (void)x; (void)y; (void)buttonMask; (void)modifiers; (void)timestamp;
    return 0;
#endif
}

/* GetMouse is implemented in PS2Controller.c */

/**
 * Button - Check if primary mouse button is currently pressed
 * Reads ModernInput's gCurrentButtons state (not hardware)
 */
Boolean Button(void)
{
    /* Read the hardware first: the state is otherwise only refreshed by
     * the main loop, so a tracking loop that did not pump never saw the
     * button come up. */
    extern void EventPumpYield(void);
    extern volatile UInt8 gCurrentButtons;
    EventPumpYield();
    return (gCurrentButtons & 1) != 0;
}

/**
 * StillDown - Check if mouse button is still pressed
 * Same as Button() - reads ModernInput's state
 */
Boolean StillDown(void)
{
    return Button();
}

/**
 * Get mouse position in local coordinates
 */
void GetLocalMouse(WindowPtr window, Point* mouseLoc)
{
    if (mouseLoc) {
        *mouseLoc = GlobalToLocal(window, g_mouseTracking.currentPos);
    }
}

/* Button is implemented in PS2Controller.c */

/**
 * Check if specific mouse button is pressed
 */
Boolean ButtonState(SInt16 buttonID)
{
    if (buttonID < 1 || buttonID > kMaxMouseButtons) {
        return false;
    }

    SInt16 mappedButton = MapMouseButton(buttonID);
    return (g_currentButtonState & (1 << (mappedButton - 1))) != 0;
}

/**
 * Wait for mouse button release
 */
/* Wait for the button to come up, and take the mouse-up event off the
 * queue (Inside Macintosh: Toolbox Essentials, 2-111). */
Boolean WaitMouseUp(void)
{
    while (Button()) {
    }
    EventRecord up;
    GetNextEvent(mUpMask, &up);
    return true;
}

/*---------------------------------------------------------------------------
 * Click Detection and Multi-Click Support
 *---------------------------------------------------------------------------*/

/**
 * Initialize click detection
 */
void InitClickDetection(SInt16 tolerance, UInt32 timeThreshold)
{
    g_multiClick.clickTolerance = tolerance;
    g_multiClick.clickTimeThreshold = timeThreshold;
    g_multiClick.clickCount = 0;
}

/**
 * Process mouse click
 */
SInt16 ProcessMouseClick(Point clickPoint, UInt32 timestamp)
{
    UInt32 timeDiff = timestamp - g_multiClick.clickTime;
    SInt16 distance = PointDistance(clickPoint, g_multiClick.clickLocation);

    if (timeDiff <= g_multiClick.clickTimeThreshold &&
        distance <= g_multiClick.clickTolerance) {
        g_multiClick.clickCount++;
        if (g_multiClick.clickCount > g_multiClick.maxClickCount) {
            g_multiClick.clickCount = g_multiClick.maxClickCount;
        }
    } else {
        g_multiClick.clickCount = 1;
    }

    g_multiClick.clickLocation = clickPoint;
    g_multiClick.clickTime = timestamp;

    return g_multiClick.clickCount;
}

/**
 * Reset click sequence
 */
void ResetClickSequence(void)
{
    g_multiClick.clickCount = 0;
    g_clickCount = 0;
}

/**
 * Get current click count
 */
SInt16 GetClickCount(void)
{
    return g_multiClick.clickCount;
}

/**
 * Check if points are within tolerance
 */
Boolean PointsWithinTolerance(Point pt1, Point pt2, SInt16 tolerance)
{
    return PointDistance(pt1, pt2) <= tolerance;
}

/*---------------------------------------------------------------------------
 * Mouse Tracking and Dragging
 *---------------------------------------------------------------------------*/

/**
 * Start mouse tracking
 */
Boolean StartMouseTracking(Point startPoint, SInt16 dragType, void* dragData)
{
    g_mouseTracking.startPos = startPoint;
    g_mouseTracking.startTime = TickCount();
    g_mouseTracking.isDragging = true;
    g_mouseTracking.dragType = dragType;
    g_mouseTracking.dragData = dragData;
    g_mouseTracking.hasMovedSinceClick = false;

    return true;
}

/**
 * Update mouse tracking
 */
Boolean UpdateMouseTracking(Point currentPoint, SInt16 modifiers)
{
    if (!g_mouseTracking.isDragging) {
        return false;
    }

    g_mouseTracking.currentPos = currentPoint;

    /* Check if we've moved since starting */
    if (!g_mouseTracking.hasMovedSinceClick) {
        SInt16 distance = PointDistance(g_mouseTracking.startPos, currentPoint);
        if (distance >= kDragStartThreshold) {
            g_mouseTracking.hasMovedSinceClick = true;
        }
    }

    return Button(); /* Continue tracking while button is down */
}

/**
 * End mouse tracking
 */
SInt16 EndMouseTracking(Point endPoint)
{
    SInt16 result = g_mouseTracking.dragType;

    g_mouseTracking.isDragging = false;
    g_mouseTracking.dragType = kDragTypeNone;
    g_mouseTracking.dragData = NULL;

    return result;
}

/**
 * Check if currently dragging
 */
Boolean IsMouseDragging(void)
{
    return g_mouseTracking.isDragging;
}

/**
 * Get current drag type
 */
SInt16 GetDragType(void)
{
    return g_mouseTracking.dragType;
}

/**
 * Track mouse in rectangle
 */
Point TrackMouseInRect(const Rect* constraintRect, MouseTrackingCallback callback, void* userData)
{
    Point currentPos = g_mouseTracking.currentPos;

    while (Button()) {
        GetMouse(&currentPos);

        /* Constrain to rectangle if specified */
        if (constraintRect) {
            if (currentPos.h < constraintRect->left) currentPos.h = constraintRect->left;
            if (currentPos.h >= constraintRect->right) currentPos.h = constraintRect->right - 1;
            if (currentPos.v < constraintRect->top) currentPos.v = constraintRect->top;
            if (currentPos.v >= constraintRect->bottom) currentPos.v = constraintRect->bottom - 1;
        }

        /* Call callback if provided */
        if (callback) {
            callback(currentPos, userData);
        }

        /* Brief sleep - just yield CPU */
        /* usleep not available in kernel context */
    }

    return currentPos;
}

/*---------------------------------------------------------------------------
 * Mouse Region Management
 *---------------------------------------------------------------------------*/

/**
 * Add mouse tracking region
 */
MouseRegion* AddMouseRegion(const Rect* bounds, void* userData)
{
    if (!bounds) return NULL;

    MouseRegion* region = (MouseRegion*)NewPtr(sizeof(MouseRegion));
    if (!region) return NULL;

    region->bounds = *bounds;
    region->userData = userData;
    region->trackingEnabled = true;
    region->mouseInside = PointInRect(g_mouseTracking.currentPos, bounds);
    region->next = g_mouseRegions;
    g_mouseRegions = region;

    return region;
}

/**
 * Remove mouse tracking region
 */
void RemoveMouseRegion(MouseRegion* region)
{
    if (!region) return;

    MouseRegion** current = &g_mouseRegions;
    while (*current) {
        if (*current == region) {
            *current = region->next;
            DisposePtr((Ptr)region);
            break;
        }
        current = &((*current)->next);
    }
}

#if 0  /* UNUSED: UpdateMouseRegionTracking - preserved for possible future use */
/**
 * Update mouse region tracking
 */
static void UpdateMouseRegionTracking(Point mousePos)
{
    MouseRegion* region = g_mouseRegions;

    while (region) {
        if (region->trackingEnabled) {
            Boolean wasInside = region->mouseInside;
            Boolean isInside = PointInRect(mousePos, &region->bounds);

            if (isInside != wasInside) {
                region->mouseInside = isInside;
                /* Could generate enter/exit events here */
            }
        }
        region = region->next;
    }
}
#endif /* UpdateMouseRegionTracking */

/**
 * Get mouse region at point
 */
MouseRegion* GetMouseRegionAtPoint(Point point)
{
    MouseRegion* region = g_mouseRegions;

    while (region) {
        if (region->trackingEnabled && PointInRect(point, &region->bounds)) {
            return region;
        }
        region = region->next;
    }

    return NULL;
}

/*---------------------------------------------------------------------------
 * Modern Mouse Features
 *---------------------------------------------------------------------------*/

/**
 * Process scroll wheel event
 */
Boolean ProcessScrollWheelEvent(SInt16 deltaX, SInt16 deltaY,
                            SInt16 modifiers, UInt32 timestamp)
{
    /* Generate custom scroll wheel event */
    /* This could be handled as a special OS event */
    SInt32 message = (deltaY << 16) | (deltaX & 0xFFFF);
    PostEvent(osEvt, message);

    return true;
}

/**
 * Set mouse acceleration
 */
void SetMouseAcceleration(float acceleration)
{
    g_mouseAcceleration = acceleration;
    if (g_mouseAcceleration < 0.1f) {
        g_mouseAcceleration = 0.1f;
    }
    if (g_mouseAcceleration > 10.0f) {
        g_mouseAcceleration = 10.0f;
    }
}

/**
 * Get mouse acceleration
 */
float GetMouseAcceleration(void)
{
    return g_mouseAcceleration;
}

/**
 * Set mouse sensitivity
 */
void SetMouseSensitivity(float sensitivity)
{
    g_mouseSensitivity = sensitivity;
    if (g_mouseSensitivity < 0.1f) {
        g_mouseSensitivity = 0.1f;
    }
    if (g_mouseSensitivity > 10.0f) {
        g_mouseSensitivity = 10.0f;
    }
}

/**
 * Get mouse sensitivity
 */
float GetMouseSensitivity(void)
{
    return g_mouseSensitivity;
}

/**
 * Set mouse button mapping
 */
void SetMouseButtonMapping(Boolean leftHanded)
{
    g_leftHandedMouse = leftHanded;
}

/*---------------------------------------------------------------------------
 * Event Generation
 *---------------------------------------------------------------------------*/

/**
 * Generate mouse down event
 */
EventRecord GenerateMouseDownEvent(Point position, SInt16 buttonID,
                                  SInt16 clickCount, SInt16 modifiers)
{
    EventRecord event = {0};

    event.what = mouseDown;
    event.message = (clickCount << 16) | (buttonID & 0xFFFF); /* Encode click count in upper 16 bits, button ID in lower 16 bits */
    event.when = TickCount();
    event.where = position;
    event.modifiers = modifiers;

    if (buttonID == kMouseButtonLeft) {
        event.modifiers |= btnState;
    }

    return event;
}

/**
 * Generate mouse up event
 */
EventRecord GenerateMouseUpEvent(Point position, SInt16 buttonID, SInt16 modifiers)
{
    EventRecord event = {0};

    event.what = mouseUp;
    event.message = 0;
    event.when = TickCount();
    event.where = position;
    event.modifiers = modifiers;

    return event;
}

/**
 * Generate mouse moved event
 */
EventRecord GenerateMouseMovedEvent(Point position, SInt16 modifiers)
{
    EventRecord event = {0};

    event.what = osEvt;
    event.message = (UInt32)mouseMovedMessage << 24;
    event.when = TickCount();
    event.where = position;
    event.modifiers = modifiers;

    return event;
}

/*---------------------------------------------------------------------------
 * Coordinate Conversion Utilities
 *---------------------------------------------------------------------------*/

/* GlobalToLocal is implemented in QuickDraw/Coordinates.c */

/* LocalToGlobal is implemented in QuickDraw/Coordinates.c */

/*---------------------------------------------------------------------------
 * State Access Functions
 *---------------------------------------------------------------------------*/

/**
 * Get mouse tracking state
 */
MouseTrackingState* GetMouseTrackingState(void)
{
    return &g_mouseTracking;
}

/**
 * Reset mouse tracking state
 */
void ResetMouseTrackingState(void)
{
    memset(&g_mouseTracking, 0, sizeof(struct MouseTrackingState));
    ResetClickSequence();
}

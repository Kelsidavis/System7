/*
 * WindowEvents.c - Window Event Handling and Hit Testing
 *
 * This file implements window event handling, hit testing, and user interaction
 * functions. These functions determine how the user interacts with windows through
 * mouse clicks, drags, and other events.
 *
 * Key functions implemented:
 * - Window hit testing (FindWindow)
 * - Mouse tracking in window parts (TrackBox, TrackGoAway)
 * - Update event handling (CheckUpdate, BeginUpdate, EndUpdate)
 * - Window region validation and invalidation
 * - Event coordination and targeting
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"

/* Forward declarations for internal helpers */
static Boolean WM_IsMouseDown(void);
static GrafPtr WM_GetCurrentPort(void);
static GrafPtr WM_GetUpdatePort(WindowPtr window);
static Boolean WM_EmptyRgn(RgnHandle rgn);


/* ============================================================================
 * Window Hit Testing and Finding
 * ============================================================================ */

short FindWindow(Point thePoint, WindowPtr* theWindow) {
    if (theWindow == NULL) return inDesk;

    *theWindow = NULL;

    WM_DEBUG("FindWindow: Testing point (%d, %d)", thePoint.h, thePoint.v);

    WindowManagerState* wmState = GetWindowManagerState();

    /* Check menu bar first */
    if (wmState->wMgrPort && thePoint.v < 20) {  /* Menu bar height = 20 pixels */
        WM_DEBUG("FindWindow: Hit in menu bar");
        return inMenuBar;
    }

    /* Check windows from front to back */
    WindowPtr current = wmState->windowList;
    while (current) {
        if (current->visible && current->strucRgn) {
            /* Check if point is in window's structure region */
            if (Platform_PtInRgn(thePoint, current->strucRgn)) {
                *theWindow = current;

                /* Determine which part of the window was hit */
                short hitResult = Platform_WindowHitTest(current, thePoint);
                if (hitResult != wNoHit) {
                    /* Convert platform hit test to FindWindow result */
                    switch (hitResult) {
                        case wInGoAway:
                            WM_DEBUG("FindWindow: Hit close box");
                            return inGoAway;
                        case wInZoomIn:
                            WM_DEBUG("FindWindow: Hit zoom box (zoom in)");
                            return inZoomIn;
                        case wInZoomOut:
                            WM_DEBUG("FindWindow: Hit zoom box (zoom out)");
                            return inZoomOut;
                        case wInGrow:
                            WM_DEBUG("FindWindow: Hit grow box");
                            return inGrow;
                        case wInDrag:
                            WM_DEBUG("FindWindow: Hit title bar/drag area");
                            return inDrag;
                        case wInContent:
                            WM_DEBUG("FindWindow: Hit content area");
                            return inContent;
                        default:
                            WM_DEBUG("FindWindow: Hit window frame");
                            return inDrag; /* Default to drag for unknown parts */
                    }
                }

                /* Point is in structure but not in any specific part */
                WM_DEBUG("FindWindow: Hit window frame (general)");
                return inDrag;
            }
        }
        current = current->nextWindow;
    }

    /* Check if it's a system window */
    /* TODO: Implement system window checking when needed */

    /* Not in any window - hit desktop */
    WM_DEBUG("FindWindow: Hit desktop");
    return inDesk;
}

/* ============================================================================
 * Mouse Tracking in Window Parts
 * ============================================================================ */

Boolean TrackBox(WindowPtr theWindow, Point thePt, short partCode) {
    if (theWindow == NULL) return false;

    WM_DEBUG("TrackBox: Tracking mouse in window part %d", partCode);

    /* Get the rectangle for the specified part */
    Rect partRect;
    Boolean validPart = false;

    switch (partCode) {
        case inGoAway:
            if (theWindow->goAwayFlag) {
                Platform_GetWindowCloseBoxRect(theWindow, &partRect);
                validPart = true;
            }
            break;
        case inZoomIn:
        case inZoomOut:
            /* Check if window has zoom box */
            /* TODO: Check window definition for zoom capability */
            Platform_GetWindowZoomBoxRect(theWindow, &partRect);
            validPart = true;
            break;
        default:
            WM_DEBUG("TrackBox: Invalid part code %d", partCode);
            return false;
    }

    if (!validPart) {
        WM_DEBUG("TrackBox: Part not available for this window");
        return false;
    }

    /* Check if initial point is in the part */
    if (!WM_PtInRect(thePt, &partRect)) {
        WM_DEBUG("TrackBox: Initial point not in part");
        return false;
    }

    /* Track mouse while button is down */
    Boolean mouseDown = true;
    Boolean inPart = true;
    Boolean lastInPart = true;
    Point currentPt = thePt;

    while (mouseDown) {
        /* Get current mouse position and button state */
        /* TODO: Get actual mouse state from platform */
        mouseDown = WM_IsMouseDown();
        if (mouseDown) {
            Platform_GetMousePosition(&currentPt);
        }

        /* Check if mouse is still in the part */
        inPart = WM_PtInRect(currentPt, &partRect);

        /* Update visual feedback if state changed */
        if (inPart != lastInPart) {
            /* Highlight or unhighlight the part */
            Platform_HighlightWindowPart(theWindow, partCode, inPart);
            lastInPart = inPart;
        }

        /* Brief delay to avoid consuming too much CPU */
        Platform_WaitTicks(1);
    }

    /* Remove highlight */
    Platform_HighlightWindowPart(theWindow, partCode, false);

    /* Return true if mouse was released inside the part */
    Boolean result = inPart;
    WM_DEBUG("TrackBox: Tracking complete, result = %s", result ? "true" : "false");
    return result;
}

Boolean TrackGoAway(WindowPtr theWindow, Point thePt) {
    if (theWindow == NULL || !theWindow->goAwayFlag) return false;

    WM_DEBUG("TrackGoAway: Tracking close box");
    return TrackBox(theWindow, thePt, inGoAway);
}

/* ============================================================================
 * Update Region Management
 * ============================================================================ */

void InvalRect(const Rect* badRect) {
    if (badRect == NULL) return;

    WM_DEBUG("InvalRect: Invalidating rect (%d, %d, %d, %d)",
             badRect->left, badRect->top, badRect->right, badRect->bottom);

    /* Get current graphics port */
    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;

    /* Assume current port is a window */
    WindowPtr window = (WindowPtr)currentPort;

    /* Add rectangle to window's update region */
    if (window->updateRgn) {
        RgnHandle tempRgn = Platform_NewRgn();
        if (tempRgn) {
            Platform_SetRectRgn(tempRgn, badRect);
            Platform_UnionRgn(window->updateRgn, tempRgn, window->updateRgn);
            Platform_DisposeRgn(tempRgn);

            /* Schedule platform update */
            Platform_InvalidateWindowRect(window, badRect);
        }
    }

    WM_DEBUG("InvalRect: Rectangle invalidated");
}

void InvalRgn(RgnHandle badRgn) {
    extern void serial_printf(const char* fmt, ...);

    if (badRgn == NULL) {
        serial_printf("WindowManager: InvalRgn called with NULL region\n");
        return;
    }

    /* Get current graphics port */
    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) {
        serial_printf("WindowManager: InvalRgn - no current port\n");
        return;
    }

    /* Assume current port is a window */
    WindowPtr window = (WindowPtr)currentPort;

    Region* badRgnData = *badRgn;
    serial_printf("WindowManager: InvalRgn window=0x%08x, badRgn bbox=(%d,%d,%d,%d)\n",
                 (unsigned int)window, badRgnData->rgnBBox.left, badRgnData->rgnBBox.top,
                 badRgnData->rgnBBox.right, badRgnData->rgnBBox.bottom);

    /* Add region to window's update region */
    if (window->updateRgn) {
        Region* updateBefore = *(window->updateRgn);
        serial_printf("WindowManager: InvalRgn - BEFORE union, updateRgn bbox=(%d,%d,%d,%d)\n",
                     updateBefore->rgnBBox.left, updateBefore->rgnBBox.top,
                     updateBefore->rgnBBox.right, updateBefore->rgnBBox.bottom);

        Platform_UnionRgn(window->updateRgn, badRgn, window->updateRgn);

        Region* updateAfter = *(window->updateRgn);
        serial_printf("WindowManager: InvalRgn - AFTER union, updateRgn bbox=(%d,%d,%d,%d)\n",
                     updateAfter->rgnBBox.left, updateAfter->rgnBBox.top,
                     updateAfter->rgnBBox.right, updateAfter->rgnBBox.bottom);

        /* Schedule platform update */
        /* TODO: Convert region to rectangle for platform invalidation */
        Rect regionBounds;
        Platform_GetRegionBounds(badRgn, &regionBounds);
        Platform_InvalidateWindowRect(window, &regionBounds);
    } else {
        serial_printf("WindowManager: InvalRgn - window has NULL updateRgn!\n");
    }
}

void ValidRect(const Rect* goodRect) {
    if (goodRect == NULL) return;

    WM_DEBUG("ValidRect: Validating rect (%d, %d, %d, %d)",
             goodRect->left, goodRect->top, goodRect->right, goodRect->bottom);

    /* Get current graphics port */
    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;

    /* Assume current port is a window */
    WindowPtr window = (WindowPtr)currentPort;

    /* Remove rectangle from window's update region */
    if (window->updateRgn) {
        RgnHandle tempRgn = Platform_NewRgn();
        if (tempRgn) {
            Platform_SetRectRgn(tempRgn, goodRect);
            Platform_DiffRgn(window->updateRgn, tempRgn, window->updateRgn);
            Platform_DisposeRgn(tempRgn);
        }
    }

    WM_DEBUG("ValidRect: Rectangle validated");
}

void ValidRgn(RgnHandle goodRgn) {
    if (goodRgn == NULL) return;

    WM_DEBUG("ValidRgn: Validating region");

    /* Get current graphics port */
    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;

    /* Assume current port is a window */
    WindowPtr window = (WindowPtr)currentPort;

    /* Remove region from window's update region */
    if (window->updateRgn) {
        Platform_DiffRgn(window->updateRgn, goodRgn, window->updateRgn);
    }

    WM_DEBUG("ValidRgn: Region validated");
}

/* ============================================================================
 * Update Event Handling
 * ============================================================================ */

void BeginUpdate(WindowPtr theWindow) {
    if (theWindow == NULL) return;

    WM_DEBUG("BeginUpdate: Beginning window update");

    /* Save current port */
    GrafPtr savePort = WM_GetCurrentPort();
    Platform_SetUpdatePort(theWindow, savePort);

    /* Set port to window */
    Platform_SetCurrentPort(&theWindow->port);

    /* Begin platform drawing session */
    Platform_BeginWindowDraw(theWindow);

    /* Set clip region to intersection of visible and update regions */
    if (theWindow->visRgn && theWindow->updateRgn) {
        RgnHandle updateClip = Platform_NewRgn();
        if (updateClip) {
            Platform_IntersectRgn(theWindow->visRgn, theWindow->updateRgn, updateClip);
            Platform_SetClipRgn(updateClip);
            Platform_DisposeRgn(updateClip);
        }
    }

    WM_DEBUG("BeginUpdate: Update session started");
}

void EndUpdate(WindowPtr theWindow) {
    if (theWindow == NULL) return;

    WM_DEBUG("EndUpdate: Ending window update");

    /* Clear the update region */
    if (theWindow->updateRgn) {
        Platform_SetEmptyRgn(theWindow->updateRgn);
    }

    /* End platform drawing session */
    Platform_EndWindowDraw(theWindow);

    /* Restore normal clipping */
    if (theWindow->visRgn) {
        Platform_SetClipRgn(theWindow->visRgn);
    }

    /* Restore previous port */
    GrafPtr savedPort = WM_GetUpdatePort(theWindow);
    if (savedPort) {
        Platform_SetCurrentPort(savedPort);
    }

    WM_DEBUG("EndUpdate: Update session ended");
}

Boolean CheckUpdate(EventRecord* theEvent) {
    if (theEvent == NULL) return false;

    /* Check if this is an update event */
    if (theEvent->what != 6 /* updateEvt */) {
        return false;
    }

    WM_DEBUG("CheckUpdate: Validating update event");

    /* Extract window from event message */
    WindowPtr window = (WindowPtr)(theEvent->message);
    if (window == NULL || !WM_VALID_WINDOW(window)) {
        WM_DEBUG("CheckUpdate: Invalid window in update event");
        return false;
    }

    /* Verify window needs updating */
    if (window->updateRgn == NULL || WM_EmptyRgn(window->updateRgn)) {
        WM_DEBUG("CheckUpdate: Window has no update region");
        return false;
    }

    /* Valid update event - application should handle via BeginUpdate/EndUpdate */
    WM_DEBUG("CheckUpdate: Valid update event for window");
    return true;
}

/* ============================================================================
 * Utility Functions for Region Management
 * ============================================================================ */

long PinRect(const Rect* theRect, Point thePt) {
    if (theRect == NULL) {
        return (long)thePt.h << 16 | (thePt.v & 0xFFFF);
    }

    WM_DEBUG("PinRect: Constraining point (%d, %d) to rect (%d, %d, %d, %d)",
             thePt.h, thePt.v, theRect->left, theRect->top, theRect->right, theRect->bottom);

    Point constrainedPt = thePt;

    /* Constrain horizontal */
    if (constrainedPt.h < theRect->left) {
        constrainedPt.h = theRect->left;
    } else if (constrainedPt.h > theRect->right) {
        constrainedPt.h = theRect->right;
    }

    /* Constrain vertical */
    if (constrainedPt.v < theRect->top) {
        constrainedPt.v = theRect->top;
    } else if (constrainedPt.v > theRect->bottom) {
        constrainedPt.v = theRect->bottom;
    }

    WM_DEBUG("PinRect: Constrained to (%d, %d)", constrainedPt.h, constrainedPt.v);

    /* Return as long with h in high word, v in low word */
    return (long)constrainedPt.h << 16 | (constrainedPt.v & 0xFFFF);
}

long DragGrayRgn(RgnHandle theRgn, Point startPt, const Rect* limitRect,
                 const Rect* slopRect, short axis, DragGrayRgnProcPtr actionProc) {
    if (theRgn == NULL) return 0;

    WM_DEBUG("DragGrayRgn: Starting gray region drag from (%d, %d)", startPt.h, startPt.v);

    Point currentPt = startPt;
    Point lastPt = startPt;
    Point offset = {0, 0};
    Boolean mouseDown = true;

    /* Show initial gray outline */
    Platform_ShowDragOutline(theRgn, startPt);

    while (mouseDown) {
        /* Get current mouse position and state */
        mouseDown = WM_IsMouseDown();
        if (mouseDown) {
            Platform_GetMousePosition(&currentPt);
        }

        /* Calculate offset from start point */
        offset.h = currentPt.h - startPt.h;
        offset.v = currentPt.v - startPt.v;

        /* Apply axis constraint if specified */
        if (axis == 1) { /* Horizontal only */
            offset.v = 0;
            currentPt.v = startPt.v;
        } else if (axis == 2) { /* Vertical only */
            offset.h = 0;
            currentPt.h = startPt.h;
        }

        /* Apply limit rectangle constraint */
        if (limitRect) {
            Point constrainedPt;
            long constrained = PinRect(limitRect, currentPt);
            constrainedPt.h = (short)(constrained >> 16);
            constrainedPt.v = (short)(constrained & 0xFFFF);

            offset.h = constrainedPt.h - startPt.h;
            offset.v = constrainedPt.v - startPt.v;
            currentPt = constrainedPt;
        }

        /* Check slop rectangle for snap-back */
        if (slopRect && !WM_PtInRect(currentPt, slopRect)) {
            /* Mouse is outside slop rect - snap back to start */
            offset.h = 0;
            offset.v = 0;
            currentPt = startPt;
        }

        /* Update drag outline if position changed */
        if (currentPt.h != lastPt.h || currentPt.v != lastPt.v) {
            Platform_UpdateDragOutline(theRgn, currentPt);
            lastPt = currentPt;

            /* Call action procedure if provided */
            if (actionProc) {
                actionProc();
            }
        }

        /* Brief delay */
        Platform_WaitTicks(1);
    }

    /* Hide drag outline */
    Platform_HideDragOutline();

    WM_DEBUG("DragGrayRgn: Drag complete, offset = (%d, %d)", offset.h, offset.v);

    /* Return final offset as long */
    return (long)offset.h << 16 | (offset.v & 0xFFFF);
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/* [WM-051] WM_InvalidateWindowsBelow moved to WindowLayering.c - canonical Z-order invalidation */

Boolean WM_TrackWindowPart(WindowPtr window, Point startPt, short part) {
    if (window == NULL) return false;

    WM_DEBUG("WM_TrackWindowPart: Tracking window part %d", part);

    /* Delegate to appropriate tracking function */
    switch (part) {
        case inGoAway:
            return TrackGoAway(window, startPt);
        case inZoomIn:
        case inZoomOut:
            return TrackBox(window, startPt, part);
        case inGrow:
            /* TODO: Implement grow tracking */
            return false;
        case inDrag:
            /* TODO: Implement drag tracking */
            return false;
        default:
            WM_DEBUG("WM_TrackWindowPart: Unsupported part %d", part);
            return false;
    }
}

/* ============================================================================
 * Platform Abstraction Helpers
 * ============================================================================ */

/* These functions would be implemented by the platform layer */

static Boolean WM_IsMouseDown(void) {
    /* TODO: Implement platform-specific mouse state checking */
    return false;
}

/* [WM-050] Platform_* functions removed - implemented in WindowPlatform.c */

static GrafPtr WM_GetCurrentPort(void) {
    extern void GetPort(GrafPtr* port);
    GrafPtr currentPort;
    GetPort(&currentPort);
    return currentPort;
}

/* [WM-050] Platform port functions removed - stubs only */

static GrafPtr WM_GetUpdatePort(WindowPtr window) {
    /* TODO: Implement platform-specific update port retrieval */
    return NULL;
}

/* [WM-050] Platform_SetClipRgn removed - stub only */

static Boolean WM_EmptyRgn(RgnHandle rgn) {
    /* TODO: Implement platform-specific empty region test */
    return true;
}

/* [WM-050] Platform region/drag functions removed - implemented in WindowPlatform.c or stubs only */

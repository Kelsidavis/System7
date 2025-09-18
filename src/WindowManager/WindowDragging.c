/*
 * WindowDragging.c - Window Dragging and Positioning Implementation
 *
 * This file implements window dragging, positioning, and movement functions.
 * These functions handle user interaction for moving windows around the screen,
 * including constraint checking, drag feedback, and platform coordination.
 *
 * Key functions implemented:
 * - Window dragging (DragWindow)
 * - Window positioning and movement (MoveWindow)
 * - Drag constraint enforcement
 * - Drag feedback and visual tracking
 * - Multi-monitor and screen boundary handling
 * - Window snapping and alignment
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#include "WindowManagerInternal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Dragging Constants and Configuration
 * ============================================================================ */

/* Drag behavior constants */
#define DRAG_THRESHOLD              4    /* Minimum pixels to start drag */
#define SNAP_DISTANCE              8    /* Snap-to-edge distance */
#define SCREEN_EDGE_MARGIN         4    /* Minimum margin from screen edge */
#define TITLE_BAR_DRAG_MARGIN      50   /* Minimum title bar visible */
#define DRAG_UPDATE_INTERVAL       16   /* Update interval in milliseconds */

/* Drag feedback modes */
typedef enum {
    kDragFeedbackNone = 0,      /* No visual feedback */
    kDragFeedbackOutline = 1,   /* Gray outline */
    kDragFeedbackWindow = 2,    /* Move actual window */
    kDragFeedbackSolid = 3      /* Solid gray rectangle */
} DragFeedbackMode;

/* Drag constraint modes */
typedef enum {
    kDragConstraintNone = 0,     /* No constraints */
    kDragConstraintScreen = 1,   /* Constrain to screen */
    kDragConstraintRect = 2,     /* Constrain to rectangle */
    kDragConstraintCustom = 3    /* Custom constraint function */
} DragConstraintMode;

/* Drag state structure */
typedef struct DragState {
    WindowPtr window;           /* Window being dragged */
    Point startPoint;           /* Initial mouse position */
    Point currentPoint;         /* Current mouse position */
    Point windowOffset;         /* Offset from mouse to window origin */
    Rect originalBounds;        /* Original window bounds */
    Rect constraintRect;        /* Constraint rectangle */
    DragFeedbackMode feedback;  /* Visual feedback mode */
    DragConstraintMode constraint; /* Constraint mode */
    Boolean active;             /* True if drag is active */
    Boolean hasMoved;           /* True if window has moved */
    RgnHandle dragRgn;          /* Region for drag feedback */
    unsigned long lastUpdate;   /* Last update time */
} DragState;

/* Global drag state */
static DragState g_dragState = {
    NULL, {0, 0}, {0, 0}, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    kDragFeedbackOutline, kDragConstraintScreen, false, false,
    NULL, 0
};

/* ============================================================================
 * Window Movement Functions
 * ============================================================================ */

void MoveWindow(WindowPtr theWindow, short hGlobal, short vGlobal, Boolean front) {
    if (theWindow == NULL) return;

    WM_DEBUG("MoveWindow: Moving window to (%d, %d), front = %s",
             hGlobal, vGlobal, front ? "true" : "false");

    /* Calculate movement offset */
    Rect currentBounds = theWindow->port.portRect;
    short deltaH = hGlobal - currentBounds.left;
    short deltaV = vGlobal - currentBounds.top;

    /* Check if window actually needs to move */
    if (deltaH == 0 && deltaV == 0) {
        if (front) {
            SelectWindow(theWindow);
        }
        return;
    }

    /* Validate new position */
    Rect newBounds = currentBounds;
    WM_OffsetRect(&newBounds, deltaH, deltaV);

    if (!WM_ValidateWindowPosition(theWindow, &newBounds)) {
        WM_DEBUG("MoveWindow: Invalid window position, constraining");
        WM_ConstrainWindowPosition(theWindow, &newBounds);
        deltaH = newBounds.left - currentBounds.left;
        deltaV = newBounds.top - currentBounds.top;
    }

    /* Save old structure region for invalidation */
    RgnHandle oldStrucRgn = Platform_NewRgn();
    if (oldStrucRgn && theWindow->strucRgn) {
        Platform_CopyRgn(theWindow->strucRgn, oldStrucRgn);
    }

    /* Update window's port rectangle */
    WM_OffsetRect(&theWindow->port.portRect, deltaH, deltaV);

    /* Update window regions */
    if (theWindow->strucRgn) {
        Platform_OffsetRgn(theWindow->strucRgn, deltaH, deltaV);
    }
    if (theWindow->contRgn) {
        Platform_OffsetRgn(theWindow->contRgn, deltaH, deltaV);
    }
    if (theWindow->updateRgn) {
        Platform_OffsetRgn(theWindow->updateRgn, deltaH, deltaV);
    }

    /* Move native platform window */
    Platform_MoveNativeWindow(theWindow, theWindow->port.portRect.left,
                             theWindow->port.portRect.top);

    /* Bring to front if requested */
    if (front) {
        SelectWindow(theWindow);
    }

    /* Invalidate old and new positions */
    if (theWindow->visible) {
        if (oldStrucRgn) {
            WM_InvalidateScreenRegion(oldStrucRgn);
        }
        if (theWindow->strucRgn) {
            WM_InvalidateScreenRegion(theWindow->strucRgn);
        }
    }

    /* Clean up */
    if (oldStrucRgn) {
        Platform_DisposeRgn(oldStrucRgn);
    }

    /* Update window layering if needed */
    WM_UpdateWindowVisibility(theWindow);

    WM_DEBUG("MoveWindow: Window moved successfully to (%d, %d)",
             theWindow->port.portRect.left, theWindow->port.portRect.top);
}

/* ============================================================================
 * Window Dragging Implementation
 * ============================================================================ */

void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect) {
    if (theWindow == NULL) return;

    WM_DEBUG("DragWindow: Starting window drag from (%d, %d)", startPt.h, startPt.v);

    /* Initialize drag state */
    WM_InitializeDragState(theWindow, startPt, boundsRect);

    /* Check if mouse is still down */
    if (!Platform_IsMouseDown()) {
        WM_DEBUG("DragWindow: Mouse not down, aborting drag");
        return;
    }

    /* Start drag feedback */
    WM_StartDragFeedback();

    /* Main drag loop */
    Boolean dragContinues = true;
    Point currentPt = startPt;
    Point lastPt = startPt;

    while (dragContinues) {
        /* Get current mouse state */
        Platform_GetMousePosition(&currentPt);
        dragContinues = Platform_IsMouseDown();

        /* Update drag state */
        g_dragState.currentPoint = currentPt;

        /* Check if mouse has moved enough to register movement */
        if (!g_dragState.hasMoved) {
            short deltaH = abs(currentPt.h - startPt.h);
            short deltaV = abs(currentPt.v - startPt.v);
            if (deltaH >= DRAG_THRESHOLD || deltaV >= DRAG_THRESHOLD) {
                g_dragState.hasMoved = true;
                WM_DEBUG("DragWindow: Drag threshold exceeded, starting movement");
            }
        }

        /* Update drag feedback if mouse moved */
        if (g_dragState.hasMoved && (currentPt.h != lastPt.h || currentPt.v != lastPt.v)) {
            WM_UpdateDragFeedback(currentPt);
            lastPt = currentPt;
        }

        /* Brief delay to avoid consuming too much CPU */
        Platform_WaitTicks(1);
    }

    /* End drag feedback */
    WM_EndDragFeedback();

    /* Apply final window position if moved */
    if (g_dragState.hasMoved) {
        Point finalOffset = WM_CalculateFinalWindowPosition(currentPt);
        short newH = g_dragState.originalBounds.left + finalOffset.h;
        short newV = g_dragState.originalBounds.top + finalOffset.v;

        WM_DEBUG("DragWindow: Applying final position (%d, %d)", newH, newV);
        MoveWindow(theWindow, newH, newV, false);
    }

    /* Clean up drag state */
    WM_CleanupDragState();

    WM_DEBUG("DragWindow: Drag operation completed");
}

/* ============================================================================
 * Drag State Management
 * ============================================================================ */

static void WM_InitializeDragState(WindowPtr window, Point startPt, const Rect* boundsRect) {
    WM_DEBUG("WM_InitializeDragState: Initializing drag state");

    /* Clear previous state */
    WM_CleanupDragState();

    /* Initialize drag state */
    g_dragState.window = window;
    g_dragState.startPoint = startPt;
    g_dragState.currentPoint = startPt;
    g_dragState.originalBounds = window->port.portRect;
    g_dragState.active = true;
    g_dragState.hasMoved = false;

    /* Calculate offset from mouse to window origin */
    g_dragState.windowOffset.h = startPt.h - window->port.portRect.left;
    g_dragState.windowOffset.v = startPt.v - window->port.portRect.top;

    /* Set constraint rectangle */
    if (boundsRect) {
        g_dragState.constraintRect = *boundsRect;
        g_dragState.constraint = kDragConstraintRect;
    } else {
        /* Use screen bounds as default constraint */
        Platform_GetScreenBounds(&g_dragState.constraintRect);
        g_dragState.constraint = kDragConstraintScreen;
    }

    /* Configure drag feedback mode */
    g_dragState.feedback = Platform_GetPreferredDragFeedback();

    /* Create drag region if needed for outline feedback */
    if (g_dragState.feedback == kDragFeedbackOutline) {
        g_dragState.dragRgn = Platform_NewRgn();
        if (g_dragState.dragRgn && window->strucRgn) {
            Platform_CopyRgn(window->strucRgn, g_dragState.dragRgn);
        }
    }

    WM_DEBUG("WM_InitializeDragState: Drag state initialized");
}

static void WM_CleanupDragState(void) {
    if (!g_dragState.active) return;

    WM_DEBUG("WM_CleanupDragState: Cleaning up drag state");

    /* Dispose of drag region */
    if (g_dragState.dragRgn) {
        Platform_DisposeRgn(g_dragState.dragRgn);
        g_dragState.dragRgn = NULL;
    }

    /* Clear state */
    memset(&g_dragState, 0, sizeof(DragState));

    WM_DEBUG("WM_CleanupDragState: Cleanup complete");
}

/* ============================================================================
 * Drag Feedback Management
 * ============================================================================ */

static void WM_StartDragFeedback(void) {
    WM_DEBUG("WM_StartDragFeedback: Starting drag feedback, mode = %d", g_dragState.feedback);

    switch (g_dragState.feedback) {
        case kDragFeedbackOutline:
            if (g_dragState.dragRgn) {
                Platform_ShowDragOutline(g_dragState.dragRgn, g_dragState.startPoint);
            }
            break;

        case kDragFeedbackWindow:
            /* No initial feedback needed - window moves in real time */
            break;

        case kDragFeedbackSolid:
            Platform_ShowDragRect(&g_dragState.originalBounds);
            break;

        case kDragFeedbackNone:
        default:
            /* No feedback */
            break;
    }
}

static void WM_UpdateDragFeedback(Point currentPt) {
    Point windowPos = WM_CalculateConstrainedWindowPosition(currentPt);

    switch (g_dragState.feedback) {
        case kDragFeedbackOutline:
            if (g_dragState.dragRgn) {
                Platform_UpdateDragOutline(g_dragState.dragRgn, windowPos);
            }
            break;

        case kDragFeedbackWindow:
            /* Move actual window in real time */
            MoveWindow(g_dragState.window, windowPos.h, windowPos.v, false);
            break;

        case kDragFeedbackSolid:
            {
                Rect dragRect = g_dragState.originalBounds;
                WM_OffsetRect(&dragRect,
                             windowPos.h - g_dragState.originalBounds.left,
                             windowPos.v - g_dragState.originalBounds.top);
                Platform_UpdateDragRect(&dragRect);
            }
            break;

        case kDragFeedbackNone:
        default:
            /* No feedback updates */
            break;
    }
}

static void WM_EndDragFeedback(void) {
    WM_DEBUG("WM_EndDragFeedback: Ending drag feedback");

    switch (g_dragState.feedback) {
        case kDragFeedbackOutline:
            Platform_HideDragOutline();
            break;

        case kDragFeedbackWindow:
            /* Window is already in final position */
            break;

        case kDragFeedbackSolid:
            Platform_HideDragRect();
            break;

        case kDragFeedbackNone:
        default:
            /* No feedback to hide */
            break;
    }
}

/* ============================================================================
 * Position Calculation and Constraints
 * ============================================================================ */

static Point WM_CalculateConstrainedWindowPosition(Point mousePt) {
    /* Calculate unconstrained window position */
    Point windowPos;
    windowPos.h = mousePt.h - g_dragState.windowOffset.h;
    windowPos.v = mousePt.v - g_dragState.windowOffset.v;

    /* Apply constraints */
    switch (g_dragState.constraint) {
        case kDragConstraintScreen:
            windowPos = WM_ConstrainToScreen(windowPos);
            break;

        case kDragConstraintRect:
            windowPos = WM_ConstrainToRect(windowPos, &g_dragState.constraintRect);
            break;

        case kDragConstraintCustom:
            /* TODO: Implement custom constraint callbacks */
            break;

        case kDragConstraintNone:
        default:
            /* No constraints */
            break;
    }

    return windowPos;
}

static Point WM_CalculateFinalWindowPosition(Point mousePt) {
    Point windowPos = WM_CalculateConstrainedWindowPosition(mousePt);

    /* Apply snapping if enabled */
    if (Platform_IsSnapToEdgesEnabled()) {
        windowPos = WM_ApplySnapToEdges(windowPos);
    }

    /* Return offset from original position */
    Point offset;
    offset.h = windowPos.h - g_dragState.originalBounds.left;
    offset.v = windowPos.v - g_dragState.originalBounds.top;

    return offset;
}

static Point WM_ConstrainToScreen(Point windowPos) {
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Calculate window bounds at proposed position */
    Rect windowBounds = g_dragState.originalBounds;
    WM_OffsetRect(&windowBounds,
                 windowPos.h - g_dragState.originalBounds.left,
                 windowPos.v - g_dragState.originalBounds.top);

    /* Ensure title bar remains visible */
    short titleBarBottom = windowBounds.top + TITLE_BAR_HEIGHT;
    if (titleBarBottom < screenBounds.top + SCREEN_EDGE_MARGIN) {
        windowPos.v = screenBounds.top + SCREEN_EDGE_MARGIN - TITLE_BAR_HEIGHT;
    }

    /* Ensure minimum title bar width is visible */
    if (windowBounds.right < screenBounds.left + TITLE_BAR_DRAG_MARGIN) {
        windowPos.h = screenBounds.left + TITLE_BAR_DRAG_MARGIN - WM_RECT_WIDTH(&g_dragState.originalBounds);
    }
    if (windowBounds.left > screenBounds.right - TITLE_BAR_DRAG_MARGIN) {
        windowPos.h = screenBounds.right - TITLE_BAR_DRAG_MARGIN;
    }

    /* Prevent window from going too far off screen vertically */
    if (windowBounds.top > screenBounds.bottom - TITLE_BAR_HEIGHT) {
        windowPos.v = screenBounds.bottom - TITLE_BAR_HEIGHT;
    }

    return windowPos;
}

static Point WM_ConstrainToRect(Point windowPos, const Rect* constraintRect) {
    if (constraintRect == NULL) return windowPos;

    /* Calculate window bounds at proposed position */
    Rect windowBounds = g_dragState.originalBounds;
    WM_OffsetRect(&windowBounds,
                 windowPos.h - g_dragState.originalBounds.left,
                 windowPos.v - g_dragState.originalBounds.top);

    /* Constrain to rectangle */
    if (windowBounds.left < constraintRect->left) {
        windowPos.h = constraintRect->left;
    }
    if (windowBounds.top < constraintRect->top) {
        windowPos.v = constraintRect->top;
    }
    if (windowBounds.right > constraintRect->right) {
        windowPos.h = constraintRect->right - WM_RECT_WIDTH(&g_dragState.originalBounds);
    }
    if (windowBounds.bottom > constraintRect->bottom) {
        windowPos.v = constraintRect->bottom - WM_RECT_HEIGHT(&g_dragState.originalBounds);
    }

    return windowPos;
}

static Point WM_ApplySnapToEdges(Point windowPos) {
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Calculate window bounds */
    Rect windowBounds = g_dragState.originalBounds;
    WM_OffsetRect(&windowBounds,
                 windowPos.h - g_dragState.originalBounds.left,
                 windowPos.v - g_dragState.originalBounds.top);

    /* Snap to screen edges */
    if (abs(windowBounds.left - screenBounds.left) <= SNAP_DISTANCE) {
        windowPos.h = screenBounds.left;
    }
    if (abs(windowBounds.right - screenBounds.right) <= SNAP_DISTANCE) {
        windowPos.h = screenBounds.right - WM_RECT_WIDTH(&windowBounds);
    }
    if (abs(windowBounds.top - screenBounds.top) <= SNAP_DISTANCE) {
        windowPos.v = screenBounds.top;
    }
    if (abs(windowBounds.bottom - screenBounds.bottom) <= SNAP_DISTANCE) {
        windowPos.v = screenBounds.bottom - WM_RECT_HEIGHT(&windowBounds);
    }

    /* TODO: Snap to other windows if enabled */

    return windowPos;
}

/* ============================================================================
 * Window Position Validation
 * ============================================================================ */

Boolean WM_ValidateWindowPosition(WindowPtr window, const Rect* bounds) {
    if (window == NULL || bounds == NULL) return false;

    /* Check for valid rectangle */
    if (!WM_VALID_RECT(bounds)) {
        WM_DEBUG("WM_ValidateWindowPosition: Invalid rectangle");
        return false;
    }

    /* Check minimum window size */
    short width = bounds->right - bounds->left;
    short height = bounds->bottom - bounds->top;

    if (width < MIN_WINDOW_WIDTH || height < MIN_WINDOW_HEIGHT) {
        WM_DEBUG("WM_ValidateWindowPosition: Window too small (%dx%d)", width, height);
        return false;
    }

    /* Check maximum window size */
    if (width > MAX_WINDOW_WIDTH || height > MAX_WINDOW_HEIGHT) {
        WM_DEBUG("WM_ValidateWindowPosition: Window too large (%dx%d)", width, height);
        return false;
    }

    /* Check screen bounds */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Ensure some part of title bar is visible */
    Rect titleBarRect = *bounds;
    titleBarRect.top -= TITLE_BAR_HEIGHT;
    titleBarRect.bottom = bounds->top;

    if (!WM_RectsIntersect(&titleBarRect, &screenBounds)) {
        WM_DEBUG("WM_ValidateWindowPosition: Title bar not visible");
        return false;
    }

    return true;
}

void WM_ConstrainWindowPosition(WindowPtr window, Rect* bounds) {
    if (window == NULL || bounds == NULL) return;

    WM_DEBUG("WM_ConstrainWindowPosition: Constraining window position");

    /* Constrain size first */
    short width = bounds->right - bounds->left;
    short height = bounds->bottom - bounds->top;

    if (width < MIN_WINDOW_WIDTH) {
        bounds->right = bounds->left + MIN_WINDOW_WIDTH;
    } else if (width > MAX_WINDOW_WIDTH) {
        bounds->right = bounds->left + MAX_WINDOW_WIDTH;
    }

    if (height < MIN_WINDOW_HEIGHT) {
        bounds->bottom = bounds->top + MIN_WINDOW_HEIGHT;
    } else if (height > MAX_WINDOW_HEIGHT) {
        bounds->bottom = bounds->top + MAX_WINDOW_HEIGHT;
    }

    /* Constrain position to screen */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Ensure title bar is visible */
    if (bounds->top > screenBounds.bottom - TITLE_BAR_HEIGHT) {
        short offset = (screenBounds.bottom - TITLE_BAR_HEIGHT) - bounds->top;
        bounds->top += offset;
        bounds->bottom += offset;
    }

    if (bounds->top < screenBounds.top - (height - TITLE_BAR_HEIGHT)) {
        short offset = (screenBounds.top - (height - TITLE_BAR_HEIGHT)) - bounds->top;
        bounds->top += offset;
        bounds->bottom += offset;
    }

    /* Ensure some part of window is horizontally visible */
    if (bounds->right < screenBounds.left + TITLE_BAR_DRAG_MARGIN) {
        short offset = (screenBounds.left + TITLE_BAR_DRAG_MARGIN) - bounds->right;
        bounds->left += offset;
        bounds->right += offset;
    }

    if (bounds->left > screenBounds.right - TITLE_BAR_DRAG_MARGIN) {
        short offset = (screenBounds.right - TITLE_BAR_DRAG_MARGIN) - bounds->left;
        bounds->left += offset;
        bounds->right += offset;
    }

    WM_DEBUG("WM_ConstrainWindowPosition: Constrained to (%d, %d, %d, %d)",
             bounds->left, bounds->top, bounds->right, bounds->bottom);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static Boolean WM_RectsIntersect(const Rect* rect1, const Rect* rect2) {
    if (rect1 == NULL || rect2 == NULL) return false;

    return !(rect1->right <= rect2->left ||
             rect1->left >= rect2->right ||
             rect1->bottom <= rect2->top ||
             rect1->top >= rect2->bottom);
}

static void WM_InvalidateScreenRegion(RgnHandle rgn) {
    if (rgn == NULL) return;

    /* Convert region to screen coordinates and invalidate */
    /* TODO: Implement screen invalidation when graphics system is available */
    WM_DEBUG("WM_InvalidateScreenRegion: Invalidating screen region");
}

/* ============================================================================
 * Platform Abstraction Functions
 * ============================================================================ */

DragFeedbackMode Platform_GetPreferredDragFeedback(void) {
    /* TODO: Query platform for preferred drag feedback mode */
    /* For now, return outline feedback as it's most compatible */
    return kDragFeedbackOutline;
}

Boolean Platform_IsSnapToEdgesEnabled(void) {
    /* TODO: Query platform/user preferences for snap-to-edges */
    return true;
}

void Platform_ShowDragRect(const Rect* rect) {
    /* TODO: Implement platform-specific solid drag rectangle display */
    WM_DEBUG("Platform_ShowDragRect: Showing drag rectangle");
}

void Platform_UpdateDragRect(const Rect* rect) {
    /* TODO: Implement platform-specific drag rectangle update */
    WM_DEBUG("Platform_UpdateDragRect: Updating drag rectangle");
}

void Platform_HideDragRect(void) {
    /* TODO: Implement platform-specific drag rectangle hiding */
    WM_DEBUG("Platform_HideDragRect: Hiding drag rectangle");
}

void Platform_OffsetRgn(RgnHandle rgn, short dh, short dv) {
    /* TODO: Implement platform-specific region offsetting */
    WM_DEBUG("Platform_OffsetRgn: Offsetting region by (%d, %d)", dh, dv);
}
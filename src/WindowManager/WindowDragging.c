/* #include "SystemTypes.h" */
#include <string.h>
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

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManagerInternal.h"
#include <math.h>

/* Forward declarations */
Boolean WM_ValidateWindowPosition(WindowPtr window, const Rect* bounds);
static Point WM_ApplySnapToEdges(Point windowPos);
void WM_ConstrainWindowPosition(WindowPtr window, Rect* bounds);
void WM_UpdateWindowVisibility(WindowPtr window);
void WM_OffsetRect(Rect* rect, short deltaH, short deltaV);
static Point WM_ConstrainToRect(Point windowPos, const Rect* constraintRect);
static Point WM_ConstrainToScreen(Point windowPos);
static Point WM_CalculateConstrainedWindowPosition(Point mousePt);
static void WM_EndDragFeedback(void);
static void WM_UpdateDragFeedback(Point currentPt);
static void WM_StartDragFeedback(void);
static void WM_CleanupDragState(void);


/* ============================================================================
 * Dragging Constants and Configuration
 * ============================================================================ */

/* Drag behavior constants */
#define DRAG_THRESHOLD              4    /* Minimum pixels to start drag */
#define SNAP_DISTANCE              8    /* Snap-to-edge distance */
#define SCREEN_EDGE_MARGIN         4    /* Minimum margin from screen edge */
#define TITLE_BAR_HEIGHT           20   /* Height of title bar */
#define TITLE_BAR_DRAG_MARGIN      50   /* Minimum title bar visible */
#define DRAG_UPDATE_INTERVAL       16   /* Update interval in milliseconds */
#define MIN_WINDOW_WIDTH            100  /* Minimum window width */
#define MIN_WINDOW_HEIGHT           50   /* Minimum window height */
#define MAX_WINDOW_WIDTH            2048 /* Maximum window width */
#define MAX_WINDOW_HEIGHT           1536 /* Maximum window height */

/* Drag feedback modes */
typedef enum {
    kDragFeedbackNone = 0,      /* No visual feedback */
    kDragFeedbackOutline = 1,   /* Gray outline */
    kDragFeedbackWindow = 2,    /* Move actual window */
    kDragFeedbackSolid = 3      /* Solid gray rectangle */
} DragFeedbackMode;

/* Forward declarations for platform functions */
DragFeedbackMode Platform_GetPreferredDragFeedback(void);
Boolean Platform_IsSnapToEdgesEnabled(void);
static void WM_InvalidateScreenRegion(RgnHandle region);
static Boolean WM_RectsIntersect(const Rect* rect1, const Rect* rect2);
static Point WM_CalculateFinalWindowPosition(Point mousePt);
static void WM_InitializeDragState(WindowPtr theWindow, Point startPt, const Rect* bounds);

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

    /* CRITICAL: Get current global position from strucRgn, NOT from portRect
     * portRect is ALWAYS in LOCAL coordinates (0,0,width,height)
     * strucRgn contains the GLOBAL screen position */
    Rect currentGlobalBounds;
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        currentGlobalBounds = (*(theWindow->strucRgn))->rgnBBox;
    } else {
        /* Fallback: use portRect but this is wrong if already corrupted */
        currentGlobalBounds = theWindow->port.portRect;
    }

    /* Calculate movement offset */
    short deltaH = hGlobal - currentGlobalBounds.left;
    short deltaV = vGlobal - currentGlobalBounds.top;

    /* Check if window actually needs to move */
    if (deltaH == 0 && deltaV == 0) {
        if (front) {
            SelectWindow(theWindow);
        }
        return;
    }

    /* Validate new position */
    Rect newBounds = currentGlobalBounds;
    WM_OffsetRect(&newBounds, deltaH, deltaV);

    if (!WM_ValidateWindowPosition(theWindow, &newBounds)) {
        WM_DEBUG("MoveWindow: Invalid window position, constraining");
        WM_ConstrainWindowPosition(theWindow, &newBounds);
        deltaH = newBounds.left - currentGlobalBounds.left;
        deltaV = newBounds.top - currentGlobalBounds.top;
    }

    /* Save old structure region for invalidation */
    RgnHandle oldStrucRgn = Platform_NewRgn();
    if (oldStrucRgn && theWindow->strucRgn) {
        Platform_CopyRgn(theWindow->strucRgn, oldStrucRgn);
    }

    /* CRITICAL: Do NOT modify portRect - it must stay in LOCAL coordinates!
     * Only update the window regions which are in GLOBAL coordinates */

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

    /* CRITICAL: Update portBits.bounds to match new global position!
     * portBits.bounds defines where local coords map to global screen coords */
    theWindow->port.portBits.bounds.left += deltaH;
    theWindow->port.portBits.bounds.top += deltaV;
    theWindow->port.portBits.bounds.right += deltaH;
    theWindow->port.portBits.bounds.bottom += deltaV;

    /* Move native platform window using new global position from strucRgn */
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        Rect newGlobalBounds = (*(theWindow->strucRgn))->rgnBBox;
        Platform_MoveNativeWindow(theWindow, newGlobalBounds.left, newGlobalBounds.top);
    }

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

    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        Rect finalPos = (*(theWindow->strucRgn))->rgnBBox;
        WM_DEBUG("MoveWindow: Window moved successfully to (%d, %d)",
                 finalPos.left, finalPos.top);
    }
}

/* ============================================================================
 * Window Dragging Implementation
 * ============================================================================ */

void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect) {
    if (theWindow == NULL) return;

    serial_printf("DragWindow ENTRY: window=%p from (%d,%d)\n", theWindow, startPt.h, startPt.v);

    /* Get GLOBAL window bounds from strucRgn - System 7 faithful */
    Rect frameG;
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        frameG = (*(theWindow->strucRgn))->rgnBBox;  /* GLOBAL coords */
    } else {
        /* Should never happen if Window Manager initialized properly */
        serial_printf("DragWindow: ERROR - no strucRgn!\n");
        return;
    }

    /* Calculate mouse offset from window origin (GLOBAL coords) */
    Point offset;
    offset.h = startPt.h - frameG.left;
    offset.v = startPt.v - frameG.top;

    /* Use provided bounds or default to screen minus menubar */
    Rect dragBounds;
    if (boundsRect) {
        dragBounds = *boundsRect;
    } else {
        dragBounds.top = 20;     /* menubar height */
        dragBounds.left = 0;
        dragBounds.bottom = 768;
        dragBounds.right = 1024;
    }

    /* System 7 modal drag loop using StillDown/GetMouse */
    extern Boolean StillDown(void);
    extern void GetMouse(Point* mouseLoc);

    Point ptG;
    Point lastPos = startPt;
    Boolean moved = false;
    Rect oldBounds = frameG;

    /* Main modal drag loop - System 7 idiom */
    while (StillDown()) {
        GetMouse(&ptG);  /* Returns GLOBAL coords */

        /* Only process if mouse moved */
        if (ptG.h != lastPos.h || ptG.v != lastPos.v) {
            /* Calculate new window position */
            short newLeft = ptG.h - offset.h;
            short newTop = ptG.v - offset.v;

            /* Constrain to drag bounds */
            short winWidth = frameG.right - frameG.left;
            short winHeight = frameG.bottom - frameG.top;

            if (newLeft < dragBounds.left)
                newLeft = dragBounds.left;
            if (newTop < dragBounds.top)
                newTop = dragBounds.top;
            if (newLeft + winWidth > dragBounds.right)
                newLeft = dragBounds.right - winWidth;
            if (newTop + winHeight > dragBounds.bottom)
                newTop = dragBounds.bottom - winHeight;

            /* Live move for now (outline feedback would be better) */
            if (newLeft != frameG.left || newTop != frameG.top) {
                serial_printf("DragWindow: MoveWindow to (%d,%d)\n", newLeft, newTop);
                MoveWindow(theWindow, newLeft, newTop, false);

                /* Update our tracking of frame position */
                frameG.right = newLeft + winWidth;
                frameG.bottom = newTop + winHeight;
                frameG.left = newLeft;
                frameG.top = newTop;

                moved = true;
            }

            lastPos = ptG;
        }
    }

    /* If window moved, invalidate both old and new regions for updates */
    if (moved) {
        InvalRect(&oldBounds);
        if (theWindow->strucRgn && *(theWindow->strucRgn)) {
            Rect newBounds = (*(theWindow->strucRgn))->rgnBBox;
            InvalRect(&newBounds);
        }
    }

    serial_printf("DragWindow EXIT: moved=%d\n", moved);
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

/* Platform functions are defined in Platform/WindowPlatform.c */

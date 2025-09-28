/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
/*
 * WindowResizing.c - Window Resizing and Zooming Implementation
 *
 * This file implements window resizing, zooming, and size management functions.
 * These functions handle user interaction for changing window sizes, including
 * grow box tracking, zoom box handling, and constraint enforcement.
 *
 * Key functions implemented:
 * - Window resizing (SizeWindow, GrowWindow)
 * - Window zooming (ZoomWindow)
 * - Size constraint enforcement
 * - Grow box tracking and feedback
 * - Window state management for zooming
 * - Aspect ratio and size limit handling
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManagerInternal.h"
#include <math.h>

/* [WM-017] Forward declarations for file-local helpers */
/* Provenance: Standard C practice for static functions called before definition */
typedef struct WindowStateData WindowStateData;
typedef struct ResizeState ResizeState;
static void WM_InitializeResizeState(WindowPtr window, Point startPt, const Rect* bBox);
static void WM_CleanupResizeState(void);
static void WM_InitializeSnapSizes(WindowPtr window);
static void WM_AddSnapSize(short width, short height);
static long WM_CalculateNewSize(Point currentPt);
static Point WM_ApplySnapToSize(short width, short height);
static void WM_StartResizeFeedback(void);
static void WM_UpdateResizeFeedback(long newSize);
static void WM_EndResizeFeedback(void);
static WindowStateData* WM_GetWindowStateData(WindowPtr window);
static WindowStateData* WM_CreateWindowStateData(WindowPtr window);
static void WM_CalculateStandardState(WindowPtr window, Rect* stdState);
static void WM_UpdateWindowUserState(WindowPtr window);
static Boolean WM_ValidateStateChecksum(WindowStateData* stateData);
static void WM_UpdateStateChecksum(WindowStateData* stateData);
static long WM_CalculateStateChecksum(WindowStateData* stateData);
static void WM_AnimateZoom(WindowPtr window, const Rect* fromBounds, const Rect* toBounds);
static void WM_InterpolateRect(const Rect* fromRect, const Rect* toRect, int step, int steps, Rect* result);
static void WM_GenerateResizeUpdateEvents(WindowPtr window, short oldWidth, short oldHeight, short newWidth, short newHeight);

/* ============================================================================
 * Resizing Constants and Configuration
 * ============================================================================ */

/* Size constraint constants */
#define MIN_RESIZE_WIDTH           80     /* Minimum window width */
#define MIN_RESIZE_HEIGHT          60     /* Minimum window height */
#define MAX_RESIZE_WIDTH           2048   /* Maximum window width */
#define MAX_RESIZE_HEIGHT          2048   /* Maximum window height */
#define RESIZE_SNAP_DISTANCE       8     /* Snap-to-size distance */
#define GROW_FEEDBACK_DELAY        50    /* Delay for grow feedback (ms) */

/* Zoom state constants */
#define ZOOM_ANIMATION_STEPS       8     /* Steps in zoom animation */
#define ZOOM_ANIMATION_DELAY       16    /* Delay between steps (ms) */

/* Window state data for zooming */
typedef struct WindowStateData {
    Rect userState;             /* User-defined size and position */
    Rect stdState;              /* Standard (zoomed) size and position */
    Boolean isZoomed;           /* Current zoom state */
    Boolean hasUserState;       /* True if user state is valid */
    Boolean hasStdState;        /* True if standard state is valid */
    long stateChecksum;         /* Checksum for state validation */
} WindowStateData;

/* Resize tracking state */
typedef struct ResizeState {
    WindowPtr window;           /* Window being resized */
    Point startPoint;           /* Initial mouse position */
    Point currentPoint;         /* Current mouse position */
    Rect originalBounds;        /* Original window bounds */
    Rect constraintRect;        /* Size constraints */
    Rect snapSizes[8];          /* Predefined snap sizes */
    short snapCount;            /* Number of snap sizes */
    Boolean active;             /* True if resize is active */
    Boolean hasMoved;           /* True if size has changed */
    Boolean showFeedback;       /* True to show resize feedback */
    unsigned long lastUpdate;   /* Last feedback update time */
} ResizeState;

/* Global resize state */
static ResizeState g_resizeState = {
    NULL, {0, 0}, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    {{0}}, 0, false, false, false, 0
};

/* ============================================================================
 * Window Sizing Functions
 * ============================================================================ */

void SizeWindow(WindowPtr theWindow, short w, short h, Boolean fUpdate) {
    if (theWindow == NULL) return;

    WM_DEBUG("SizeWindow: Resizing window to %dx%d, update = %s",
             w, h, fUpdate ? "true" : "false");

    /* Validate new size */
    if (w < MIN_RESIZE_WIDTH) w = MIN_RESIZE_WIDTH;
    if (h < MIN_RESIZE_HEIGHT) h = MIN_RESIZE_HEIGHT;
    if (w > MAX_RESIZE_WIDTH) w = MAX_RESIZE_WIDTH;
    if (h > MAX_RESIZE_HEIGHT) h = MAX_RESIZE_HEIGHT;

    /* Check if size actually needs to change */
    /* [WM-009] IM:Windows p.2-13: WindowRecord first field is GrafPort */
    Rect currentBounds = theWindow->port.portRect;
    short currentWidth = currentBounds.right - currentBounds.left;
    short currentHeight = currentBounds.bottom - currentBounds.top;

    if (currentWidth == w && currentHeight == h) {
        WM_DEBUG("SizeWindow: No size change needed");
        return;
    }

    /* Save old structure region for invalidation */
    RgnHandle oldStrucRgn = Platform_NewRgn();
    if (oldStrucRgn && theWindow->strucRgn) {
        Platform_CopyRgn(theWindow->strucRgn, oldStrucRgn);
    }

    /* Update window's port rectangle */
    (theWindow)->port.portRect.right = (theWindow)->port.portRect.left + w;
    (theWindow)->port.portRect.bottom = (theWindow)->port.portRect.top + h;

    /* Recalculate window regions */
    Platform_CalculateWindowRegions(theWindow);

    /* Resize native platform window */
    Platform_SizeNativeWindow(theWindow, w, h);

    /* Generate update events for newly exposed areas if requested */
    if (fUpdate && theWindow->visible) {
        WM_GenerateResizeUpdateEvents(theWindow, currentWidth, currentHeight, w, h);
    }

    /* Invalidate old and new window areas */
    if (theWindow->visible) {
        if (oldStrucRgn) {
            WM_InvalidateScreenRegion(oldStrucRgn);
        }
        if (theWindow->strucRgn) {
            WM_InvalidateScreenRegion(theWindow->strucRgn);
        }
    }

    /* Update window visibility */
    WM_UpdateWindowVisibility(theWindow);

    /* Update window state if this was user-initiated */
    WM_UpdateWindowUserState(theWindow);

    /* Clean up */
    if (oldStrucRgn) {
        Platform_DisposeRgn(oldStrucRgn);
    }

    WM_DEBUG("SizeWindow: Window resized successfully to %dx%d", w, h);
}

/* ============================================================================
 * Grow Box Tracking
 * ============================================================================ */

long GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox) {
    if (theWindow == NULL) return 0;

    WM_DEBUG("GrowWindow: Starting grow tracking from (%d, %d)", startPt.h, startPt.v);

    /* Check if window supports growing */
    if (!WM_WindowHasGrowBox(theWindow)) {
        WM_DEBUG("GrowWindow: Window does not have grow box");
        return 0;
    }

    /* Initialize resize state */
    WM_InitializeResizeState(theWindow, startPt, bBox);

    /* Check if mouse is still down */
    if (!Platform_IsMouseDown()) {
        WM_DEBUG("GrowWindow: Mouse not down, aborting grow");
        return 0;
    }

    /* Start resize feedback */
    WM_StartResizeFeedback();

    /* Main resize tracking loop */
    Boolean resizeContinues = true;
    Point currentPt = startPt;
    Point lastPt = startPt;
    long lastSize = 0;

    while (resizeContinues) {
        /* Get current mouse state */
        Platform_GetMousePosition(&currentPt);
        resizeContinues = Platform_IsMouseDown();

        /* Update resize state */
        g_resizeState.currentPoint = currentPt;

        /* Calculate new window size */
        long newSize = WM_CalculateNewSize(currentPt);

        /* Update feedback if size changed */
        if (newSize != lastSize) {
            WM_UpdateResizeFeedback(newSize);
            lastSize = newSize;
            g_resizeState.hasMoved = true;
        }

        /* Brief delay to avoid consuming too much CPU */
        Platform_WaitTicks(1);
    }

    /* End resize feedback */
    WM_EndResizeFeedback();

    /* Calculate final size */
    long finalSize = g_resizeState.hasMoved ? WM_CalculateNewSize(currentPt) : 0;

    /* Clean up resize state */
    WM_CleanupResizeState();

    WM_DEBUG("GrowWindow: Grow tracking completed, result = 0x%08lX", finalSize);
    return finalSize;
}

/* ============================================================================
 * Window Zooming
 * ============================================================================ */

void ZoomWindow(WindowPtr theWindow, short partCode, Boolean front) {
    if (theWindow == NULL) return;

    WM_DEBUG("ZoomWindow: Zooming window, partCode = %d, front = %s",
             partCode, front ? "true" : "false");

    /* Check if window supports zooming */
    if (!WM_WindowHasZoomBox(theWindow)) {
        WM_DEBUG("ZoomWindow: Window does not support zooming");
        return;
    }

    /* Get or create window state data */
    WindowStateData* stateData = WM_GetWindowStateData(theWindow);
    if (stateData == NULL) {
        WM_DEBUG("ZoomWindow: Failed to get window state data");
        return;
    }

    /* Determine zoom direction */
    Boolean shouldZoomOut = (partCode == inZoomOut) ||
                           (partCode == inZoomIn && stateData->isZoomed);

    Rect targetBounds;
    if (shouldZoomOut) {
        /* Zoom out to user state */
        if (stateData->hasUserState) {
            targetBounds = stateData->userState;
        } else {
            /* Use current bounds as user state */
            /* [WM-009] Provenance: IM:Windows p.2-13 */
            targetBounds = theWindow->port.portRect;
        }
        stateData->isZoomed = false;
        WM_DEBUG("ZoomWindow: Zooming out to user state");
    } else {
        /* Zoom in to standard state */
        if (!stateData->hasStdState) {
            /* Calculate standard state */
            WM_CalculateStandardState(theWindow, &stateData->stdState);
            stateData->hasStdState = true;
        }
        targetBounds = stateData->stdState;
        stateData->isZoomed = true;
        WM_DEBUG("ZoomWindow: Zooming in to standard state");
    }

    /* Save current state as appropriate */
    if (!shouldZoomOut && !stateData->hasUserState) {
        /* [WM-009] Provenance: IM:Windows p.2-13 */
        stateData->userState = theWindow->port.portRect;
        stateData->hasUserState = true;
    }

    /* Perform zoom animation */
    if (Platform_IsZoomAnimationEnabled()) {
        /* [WM-009] Provenance: IM:Windows p.2-13 */
        WM_AnimateZoom(theWindow, &theWindow->port.portRect, &targetBounds);
    }

    /* Apply final size and position */
    short newWidth = targetBounds.right - targetBounds.left;
    short newHeight = targetBounds.bottom - targetBounds.top;

    MoveWindow(theWindow, targetBounds.left, targetBounds.top, false);
    SizeWindow(theWindow, newWidth, newHeight, true);

    /* Bring to front if requested */
    if (front) {
        SelectWindow(theWindow);
    }

    /* Update state checksum */
    WM_UpdateStateChecksum(stateData);

    WM_DEBUG("ZoomWindow: Zoom operation completed");
}

/* ============================================================================
 * Resize State Management
 * ============================================================================ */

static void WM_InitializeResizeState(WindowPtr window, Point startPt, const Rect* bBox) {
    WM_DEBUG("WM_InitializeResizeState: Initializing resize state");

    /* Clear previous state */
    WM_CleanupResizeState();

    /* Initialize resize state */
    g_resizeState.window = window;
    g_resizeState.startPoint = startPt;
    g_resizeState.currentPoint = startPt;
    /* [WM-009] Provenance: IM:Windows p.2-13 */
    g_resizeState.originalBounds = window->port.portRect;
    g_resizeState.active = true;
    g_resizeState.hasMoved = false;
    g_resizeState.showFeedback = Platform_IsResizeFeedbackEnabled();

    /* Set size constraints */
    if (bBox) {
        g_resizeState.constraintRect = *bBox;
    } else {
        /* Use default constraints */
        WM_SetRect(&g_resizeState.constraintRect,
                  MIN_RESIZE_WIDTH, MIN_RESIZE_HEIGHT,
                  MAX_RESIZE_WIDTH, MAX_RESIZE_HEIGHT);
    }

    /* Set up snap sizes */
    WM_InitializeSnapSizes(window);

    WM_DEBUG("WM_InitializeResizeState: Resize state initialized");
}

static void WM_CleanupResizeState(void) {
    if (!g_resizeState.active) return;

    WM_DEBUG("WM_CleanupResizeState: Cleaning up resize state");

    /* Clear state */
    memset(&g_resizeState, 0, sizeof(ResizeState));

    WM_DEBUG("WM_CleanupResizeState: Cleanup complete");
}

static void WM_InitializeSnapSizes(WindowPtr window) {
    g_resizeState.snapCount = 0;

    /* Add common snap sizes */
    WM_AddSnapSize(320, 240);   /* Classic small */
    WM_AddSnapSize(640, 480);   /* Classic VGA */
    WM_AddSnapSize(800, 600);   /* Classic SVGA */
    WM_AddSnapSize(1024, 768);  /* Classic XGA */

    /* Add screen-based sizes */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);
    short screenWidth = screenBounds.right - screenBounds.left;
    short screenHeight = screenBounds.bottom - screenBounds.top;

    WM_AddSnapSize(screenWidth / 2, screenHeight / 2);  /* Quarter screen */
    WM_AddSnapSize(screenWidth * 2 / 3, screenHeight * 2 / 3); /* Two-thirds */
    WM_AddSnapSize(screenWidth - 40, screenHeight - 80); /* Almost full screen */

    WM_DEBUG("WM_InitializeSnapSizes: Added %d snap sizes", g_resizeState.snapCount);
}

static void WM_AddSnapSize(short width, short height) {
    if (g_resizeState.snapCount >= 8) return;

    /* Validate size against constraints */
    if (width >= g_resizeState.constraintRect.left &&
        width <= g_resizeState.constraintRect.right &&
        height >= g_resizeState.constraintRect.top &&
        height <= g_resizeState.constraintRect.bottom) {

        Rect* snapRect = &g_resizeState.snapSizes[g_resizeState.snapCount];
        WM_SetRect(snapRect, 0, 0, width, height);
        g_resizeState.snapCount++;
    }
}

/* ============================================================================
 * Size Calculation and Constraints
 * ============================================================================ */

static long WM_CalculateNewSize(Point currentPt) {
    /* Calculate size change from mouse movement */
    short deltaH = currentPt.h - g_resizeState.startPoint.h;
    short deltaV = currentPt.v - g_resizeState.startPoint.v;

    /* Calculate new window size */
    short newWidth = WM_RECT_WIDTH(&g_resizeState.originalBounds) + deltaH;
    short newHeight = WM_RECT_HEIGHT(&g_resizeState.originalBounds) + deltaV;

    /* Apply constraints */
    if (newWidth < g_resizeState.constraintRect.left) {
        newWidth = g_resizeState.constraintRect.left;
    }
    if (newWidth > g_resizeState.constraintRect.right) {
        newWidth = g_resizeState.constraintRect.right;
    }
    if (newHeight < g_resizeState.constraintRect.top) {
        newHeight = g_resizeState.constraintRect.top;
    }
    if (newHeight > g_resizeState.constraintRect.bottom) {
        newHeight = g_resizeState.constraintRect.bottom;
    }

    /* Apply snap sizes if enabled */
    if (Platform_IsSnapToSizeEnabled()) {
        Point snapSize = WM_ApplySnapToSize(newWidth, newHeight);
        newWidth = snapSize.h;
        newHeight = snapSize.v;
    }

    /* Return size as long (width in high word, height in low word) */
    return ((long)newWidth << 16) | (newHeight & 0xFFFF);
}

static Point WM_ApplySnapToSize(short width, short height) {
    Point result = {width, height};

    /* Check against predefined snap sizes */
    for (int i = 0; i < g_resizeState.snapCount; i++) {
        Rect* snapRect = &g_resizeState.snapSizes[i];
        short snapWidth = WM_RECT_WIDTH(snapRect);
        short snapHeight = WM_RECT_HEIGHT(snapRect);

        if (abs(width - snapWidth) <= RESIZE_SNAP_DISTANCE &&
            abs(height - snapHeight) <= RESIZE_SNAP_DISTANCE) {
            result.h = snapWidth;
            result.v = snapHeight;
            WM_DEBUG("WM_ApplySnapToSize: Snapped to %dx%d", snapWidth, snapHeight);
            break;
        }
    }

    return result;
}

/* ============================================================================
 * Resize Feedback
 * ============================================================================ */

static void WM_StartResizeFeedback(void) {
    if (!g_resizeState.showFeedback) return;

    WM_DEBUG("WM_StartResizeFeedback: Starting resize feedback");

    /* Show initial size feedback */
    short width = WM_RECT_WIDTH(&g_resizeState.originalBounds);
    short height = WM_RECT_HEIGHT(&g_resizeState.originalBounds);
    Platform_ShowSizeFeedback(width, height, &g_resizeState.originalBounds);
}

static void WM_UpdateResizeFeedback(long newSize) {
    if (!g_resizeState.showFeedback) return;

    short width = (short)(newSize >> 16);
    short height = (short)(newSize & 0xFFFF);

    /* Calculate new window bounds */
    Rect newBounds = g_resizeState.originalBounds;
    newBounds.right = newBounds.left + width;
    newBounds.bottom = newBounds.top + height;

    Platform_UpdateSizeFeedback(width, height, &newBounds);
}

static void WM_EndResizeFeedback(void) {
    if (!g_resizeState.showFeedback) return;

    WM_DEBUG("WM_EndResizeFeedback: Ending resize feedback");
    Platform_HideSizeFeedback();
}

/* ============================================================================
 * Window State Management
 * ============================================================================ */

static WindowStateData* WM_GetWindowStateData(WindowPtr window) {
    if (window == NULL) return NULL;

    /* Try to get state data from auxiliary window record */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            /* Check if dataHandle contains our state data */
            if ((**auxWin).dialogCItem != NULL) {
                /* Verify this is our state data */
                WindowStateData* stateData = (WindowStateData*)((**auxWin).dialogCItem);
                if (WM_ValidateStateChecksum(stateData)) {
                    return stateData;
                }
            }
        }
    }

    /* Create new state data */
    WindowStateData* stateData = WM_CreateWindowStateData(window);
    if (stateData && auxWin && *auxWin) {
        (**auxWin).dialogCItem = (Handle)stateData;
    }

    return stateData;
}

static WindowStateData* WM_CreateWindowStateData(WindowPtr window) {
    WindowStateData* stateData = (WindowStateData*)calloc(1, sizeof(WindowStateData));
    if (stateData == NULL) return NULL;

    /* Initialize with current window bounds */
    /* [WM-009] Provenance: IM:Windows p.2-13 */
    stateData->userState = window->port.portRect;
    stateData->hasUserState = true;
    stateData->hasStdState = false;
    stateData->isZoomed = false;

    WM_UpdateStateChecksum(stateData);

    WM_DEBUG("WM_CreateWindowStateData: Created new window state data");
    return stateData;
}

static void WM_CalculateStandardState(WindowPtr window, Rect* stdState) {
    if (window == NULL || stdState == NULL) return;

    WM_DEBUG("WM_CalculateStandardState: Calculating standard state");

    /* Get screen bounds */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Calculate standard size (80% of screen, centered) */
    short screenWidth = screenBounds.right - screenBounds.left;
    short screenHeight = screenBounds.bottom - screenBounds.top;
    short stdWidth = (screenWidth * 4) / 5;
    short stdHeight = (screenHeight * 4) / 5;

    /* Center on screen */
    short leftMargin = (screenWidth - stdWidth) / 2;
    short topMargin = (screenHeight - stdHeight) / 2;

    WM_SetRect(stdState,
              screenBounds.left + leftMargin,
              screenBounds.top + topMargin,
              screenBounds.left + leftMargin + stdWidth,
              screenBounds.top + topMargin + stdHeight);

    /* Adjust for menu bar */
    /* [WM-011] Provenance: IM:Toolbox Essentials p.3-8 - menu bar height is 20 pixels */
    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState && wmState->menuBarHeight > 0) {
        stdState->top += wmState->menuBarHeight;
    }

    WM_DEBUG("WM_CalculateStandardState: Standard state = (%d, %d, %d, %d)",
             stdState->left, stdState->top, stdState->right, stdState->bottom);
}

static void WM_UpdateWindowUserState(WindowPtr window) {
    WindowStateData* stateData = WM_GetWindowStateData(window);
    if (stateData == NULL) return;

    /* Update user state only if window is not currently zoomed */
    if (!stateData->isZoomed) {
        /* [WM-009] Provenance: IM:Windows p.2-13 */
        stateData->userState = window->port.portRect;
        stateData->hasUserState = true;
        WM_UpdateStateChecksum(stateData);
        WM_DEBUG("WM_UpdateWindowUserState: Updated user state");
    }
}

static Boolean WM_ValidateStateChecksum(WindowStateData* stateData) {
    if (stateData == NULL) return false;

    long checksum = WM_CalculateStateChecksum(stateData);
    return (checksum == stateData->stateChecksum);
}

static void WM_UpdateStateChecksum(WindowStateData* stateData) {
    if (stateData == NULL) return;

    stateData->stateChecksum = WM_CalculateStateChecksum(stateData);
}

static long WM_CalculateStateChecksum(WindowStateData* stateData) {
    if (stateData == NULL) return 0;

    /* Simple checksum based on state data */
    /* [WM-018] Window state validation */
    long checksum = 0x12345678; /* Magic number */
    checksum ^= stateData->userState.left;
    checksum ^= stateData->userState.top << 8;
    checksum ^= stateData->userState.right << 16;
    checksum ^= stateData->userState.bottom << 24;
    checksum ^= stateData->isZoomed ? 0xAAAAAAAA : 0x55555555;

    return checksum;
}

/* ============================================================================
 * Zoom Animation
 * ============================================================================ */

static void WM_AnimateZoom(WindowPtr window, const Rect* fromBounds, const Rect* toBounds) {
    if (!Platform_IsZoomAnimationEnabled()) return;

    WM_DEBUG("WM_AnimateZoom: Animating zoom transition");

    /* Calculate animation steps */
    for (int step = 1; step <= ZOOM_ANIMATION_STEPS; step++) {
        Rect currentBounds;
        WM_InterpolateRect(fromBounds, toBounds, step, ZOOM_ANIMATION_STEPS, &currentBounds);

        /* Show animation frame */
        Platform_ShowZoomFrame(&currentBounds);

        /* Delay between frames */
        Platform_WaitTicks(ZOOM_ANIMATION_DELAY / 16);
    }

    /* Hide final animation frame */
    Platform_HideZoomFrame();

    WM_DEBUG("WM_AnimateZoom: Zoom animation completed");
}

static void WM_InterpolateRect(const Rect* fromRect, const Rect* toRect,
                              int step, int totalSteps, Rect* result) {
    if (fromRect == NULL || toRect == NULL || result == NULL) return;

    /* Linear interpolation between rectangles */
    float ratio = (float)step / (float)totalSteps;

    result->left = fromRect->left + (short)((toRect->left - fromRect->left) * ratio);
    result->top = fromRect->top + (short)((toRect->top - fromRect->top) * ratio);
    result->right = fromRect->right + (short)((toRect->right - fromRect->right) * ratio);
    result->bottom = fromRect->bottom + (short)((toRect->bottom - fromRect->bottom) * ratio);
}

/* ============================================================================
 * Update Event Generation
 * ============================================================================ */

static void WM_GenerateResizeUpdateEvents(WindowPtr window, short oldWidth, short oldHeight,
                                        short newWidth, short newHeight) {
    if (window == NULL || window->updateRgn == NULL) return;

    WM_DEBUG("WM_GenerateResizeUpdateEvents: Generating update events for resize");

    /* Calculate newly exposed areas */
    if (newWidth > oldWidth) {
        /* Right edge exposed */
        Rect rightRect;
        WM_SetRect(&rightRect,
                  (window)->port.portRect.left + oldWidth,
                  (window)->port.portRect.top,
                  (window)->port.portRect.right,
                  (window)->port.portRect.bottom);

        RgnHandle rightRgn = Platform_NewRgn();
        if (rightRgn) {
            Platform_SetRectRgn(rightRgn, &rightRect);
            Platform_UnionRgn(window->updateRgn, rightRgn, window->updateRgn);
            Platform_DisposeRgn(rightRgn);
        }
    }

    if (newHeight > oldHeight) {
        /* Bottom edge exposed */
        Rect bottomRect;
        WM_SetRect(&bottomRect,
                  (window)->port.portRect.left,
                  (window)->port.portRect.top + oldHeight,
                  (window)->port.portRect.left + oldWidth, /* Don't double-count corner */
                  (window)->port.portRect.bottom);

        RgnHandle bottomRgn = Platform_NewRgn();
        if (bottomRgn) {
            Platform_SetRectRgn(bottomRgn, &bottomRect);
            Platform_UnionRgn(window->updateRgn, bottomRgn, window->updateRgn);
            Platform_DisposeRgn(bottomRgn);
        }
    }

    WM_DEBUG("WM_GenerateResizeUpdateEvents: Update events generated");
}

/* Platform functions are in WindowPlatform.c */

/* Size feedback and zoom frame functions moved to WindowPlatform.c */

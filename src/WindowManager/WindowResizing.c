#include "MemoryMgr/MemoryManager.h"
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
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManagerInternal.h"
#include "QuickDraw/QuickDraw.h"
#include <math.h>

/* [WM-017] Forward declarations for file-local helpers */
/* Provenance: Standard C practice for static functions called before definition */
typedef struct WindowStateData WindowStateData;
typedef struct ResizeState ResizeState;
static void Local_InitializeResizeState(WindowPtr window, Point startPt, const Rect* bBox);
static void Local_CleanupResizeState(void);
static void Local_InitializeSnapSizes(WindowPtr window);
static void Local_AddSnapSize(short width, short height);
/* Future use - resize feedback functions currently unused */
/* static long Local_CalculateNewSize(Point currentPt); */
/* static Point Local_ApplySnapToSize(short width, short height); */
static void Local_StartResizeFeedback(void);
/* static void Local_UpdateResizeFeedback(long newSize); */
static void Local_EndResizeFeedback(void);
WindowStateData* WM_GetWindowStateData(WindowPtr window);
static WindowStateData* WM_CreateWindowStateData(WindowPtr window);
static void WM_CalculateStandardState(WindowPtr window, Rect* stdState);
static void WM_UpdateWindowUserState(WindowPtr window);
static Boolean Local_ValidateStateChecksum(WindowStateData* stateData);
static void Local_UpdateStateChecksum(WindowStateData* stateData);
static long Local_CalculateStateChecksum(WindowStateData* stateData);
static void Local_AnimateZoom(WindowPtr window, const Rect* fromBounds, const Rect* toBounds);
static void Local_InterpolateRect(const Rect* fromRect, const Rect* toRect, int step, int steps, Rect* result);
static void Local_GenerateResizeUpdateEvents(WindowPtr window, short oldWidth, short oldHeight, short newWidth, short newHeight);

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
    Rect currentBounds;         /* Current bounds during resize */
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
    NULL, {0, 0}, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
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

    /* Log detailed info for debugging caller */
    extern void serial_puts(const char *str);
    extern int sprintf(char* buf, const char* fmt, ...);
    char dbgbuf[256];
    unsigned long refCon = (unsigned long)theWindow->refCon;
    /* Format refCon as printable ASCII or hex to avoid null byte truncation */
    char refconStr[16];
    unsigned char b0 = (refCon >> 24) & 0xFF;
    unsigned char b1 = (refCon >> 16) & 0xFF;
    unsigned char b2 = (refCon >> 8) & 0xFF;
    unsigned char b3 = refCon & 0xFF;
    /* Check if all bytes are printable ASCII */
    if (b0 >= 32 && b0 < 127 && b1 >= 32 && b1 < 127 &&
        b2 >= 32 && b2 < 127 && b3 >= 32 && b3 < 127) {
        refconStr[0] = b0; refconStr[1] = b1;
        refconStr[2] = b2; refconStr[3] = b3;
        refconStr[4] = 0;
    } else {
        snprintf(refconStr, sizeof(refconStr), "%02X%02X%02X%02X", b0, b1, b2, b3);
    }
    snprintf(dbgbuf, sizeof(dbgbuf), "[SIZEWND] refCon=0x%08x (%s) current=%dx%d new=%dx%d\n",
            (unsigned int)refCon, refconStr, currentWidth, currentHeight, w, h);
    serial_puts(dbgbuf);

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

    /* NOTE: Do NOT update portBits.bounds here!
     * portBits.bounds is updated later (line 212) after contRgn is recalculated.
     * We use Global Framebuffer approach where portBits.bounds = content's GLOBAL position.
     * Overwriting it here with (0,0,w,h) would break the coordinate system. */

    /* CRITICAL: Directly update window regions for resize
     *
     * Problem: Platform_CalculateWindowRegions has circular dependency - it uses
     * Platform_GetWindowFrameRect which returns the OLD strucRgn.
     *
     * Solution: During resize, window position doesn't change (grow box is bottom-right),
     * so we can directly calculate new regions from old position + new dimensions.
     */
    if (oldStrucRgn && *(oldStrucRgn)) {
        Rect oldStrucRect = (**oldStrucRgn).rgnBBox;

        /* Chrome dimensions */
        const SInt16 kBorder = 1;
        const SInt16 kTitleBar = 20;
        const SInt16 kSeparator = 1;
        const SInt16 kRightBorder = 2;

        /* Calculate old content position */
        SInt16 contentLeft = oldStrucRect.left + kBorder;
        SInt16 contentTop = oldStrucRect.top + kTitleBar + kSeparator;

        /* Calculate new structure rect (frame) with new dimensions */
        Rect newStrucRect;
        newStrucRect.left = oldStrucRect.left;
        newStrucRect.top = oldStrucRect.top;
        newStrucRect.right = contentLeft + w + kRightBorder;
        newStrucRect.bottom = contentTop + h + kBorder;

        /* Calculate new content rect */
        Rect newContRect;
        newContRect.left = contentLeft;
        newContRect.top = contentTop;
        newContRect.right = contentLeft + w;
        newContRect.bottom = contentTop + h;

        /* Update regions */
        extern void Platform_SetRectRgn(RgnHandle rgn, const Rect* rect);
        Platform_SetRectRgn(theWindow->strucRgn, &newStrucRect);
        Platform_SetRectRgn(theWindow->contRgn, &newContRect);
    }

    /* Resize native platform window */
    Platform_SizeNativeWindow(theWindow, w, h);

    /* With Global Framebuffer approach, update portBits.bounds to content area's new GLOBAL position */
    if (theWindow->contRgn && *(theWindow->contRgn)) {
        Rect newContentBounds = (*(theWindow->contRgn))->rgnBBox;

        /* Update bounds to content area's new GLOBAL position */
        SetRect(&theWindow->port.portBits.bounds,
                newContentBounds.left, newContentBounds.top,
                newContentBounds.right, newContentBounds.bottom);

        if (theWindow->refCon == 0x4449534b) {
            snprintf(dbgbuf, sizeof(dbgbuf), "[SIZEWND] Updated bounds: contentRect=(%d,%d,%d,%d)\n",
                    newContentBounds.left, newContentBounds.top, newContentBounds.right, newContentBounds.bottom);
            serial_puts(dbgbuf);
        }
    }

    /* Generate update events for newly exposed areas if requested */
    if (fUpdate && theWindow->visible) {
        Local_GenerateResizeUpdateEvents(theWindow, currentWidth, currentHeight, w, h);
    }

    /* Invalidate old and new window areas, and explicitly erase exposed desktop */
    if (theWindow->visible) {
        /* If window shrank, we need to erase and repaint the newly exposed desktop area */
        /* This is the area that was covered by the window before but isn't covered now */
        if (oldStrucRgn && theWindow->strucRgn) {
            /* Create a region for areas that were covered before but aren't now */
            RgnHandle exposedDesktop = Platform_NewRgn();
            if (exposedDesktop) {
                /* Exposed area = old region minus new region */
                DiffRgn(oldStrucRgn, theWindow->strucRgn, exposedDesktop);

                /* CRITICAL: Erase the exposed desktop area with desktop pattern BEFORE repainting */
                /* This is the same approach used in HideWindow */
                extern void EraseRgn(RgnHandle rgn);
                extern void GetWMgrPort(GrafPtr* port);
                extern void SetPort(GrafPtr port);
                extern void GetPort(GrafPtr* port);

                GrafPtr savePort, wmPort;
                GetPort(&savePort);
                GetWMgrPort(&wmPort);
                SetPort(wmPort);

                EraseRgn(exposedDesktop);

                SetPort(savePort);

                WM_DEBUG("SizeWindow: Erased exposed desktop area");

                Platform_DisposeRgn(exposedDesktop);
            }
        }

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
    extern void serial_puts(const char* str);
    serial_puts("[GW] GrowWindow ENTRY\n");

    if (theWindow == NULL) {
        serial_puts("[GW] GrowWindow: NULL window\n");
        return 0;
    }

    serial_puts("[GW] GrowWindow: Starting grow tracking\n");
    WM_DEBUG("GrowWindow: Starting grow tracking from (%d, %d)", startPt.h, startPt.v);

    /* Check if window supports growing */
    serial_puts("[GW] Checking grow box\n");
    if (!WM_WindowHasGrowBox(theWindow)) {
        serial_puts("[GW] No grow box\n");
        WM_DEBUG("GrowWindow: Window does not have grow box");
        return 0;
    }

    /* Initialize resize state */
    serial_puts("[GW] Initializing resize state\n");
    Local_InitializeResizeState(theWindow, startPt, bBox);
    serial_puts("[GW] Resize state initialized\n");

    /* Check if mouse is still down */
    serial_puts("[GW] Checking mouse\n");
    if (!Platform_IsMouseDown()) {
        serial_puts("[GW] Mouse not down\n");
        WM_DEBUG("GrowWindow: Mouse not down, aborting grow");
        return 0;
    }

    /* Start resize feedback */
    serial_puts("[GW] Starting feedback\n");
    Local_StartResizeFeedback();
    serial_puts("[GW] Feedback started\n");

    /* Get original window bounds for tracking - use strucRgn for GLOBAL coordinates */
    Rect originalBounds;
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        /* Use the actual GLOBAL window bounds from strucRgn */
        originalBounds = (*(theWindow->strucRgn))->rgnBBox;
    } else {
        /* Fallback to portRect but this may be wrong */
        originalBounds.left = 0;
        originalBounds.top = 0;
        originalBounds.right = theWindow->port.portRect.right - theWindow->port.portRect.left;
        originalBounds.bottom = theWindow->port.portRect.bottom - theWindow->port.portRect.top;
    }

    /* Main resize tracking loop */
    serial_puts("[GW] Entering main loop\n");
    Boolean resizeContinues = true;
    Point currentPt = startPt;

    /* Track outline for XOR visual feedback */
    Rect outlineRect = originalBounds;
    Boolean outlineDrawn = false;

    int loopCount = 0;
    const int MAX_LOOP_ITERATIONS = 1000000;  /* Prevent infinite loops */

    while (resizeContinues && loopCount < MAX_LOOP_ITERATIONS) {
        loopCount++;

        /* Poll hardware for new input events (like DragWindow) */
        extern void EventPumpYield(void);
        EventPumpYield();

        /* Get current mouse state using same method as DragWindow */
        extern void GetMouse(Point* mouseLoc);
        extern Boolean StillDown(void);
        GetMouse(&currentPt);  /* Returns GLOBAL coords */
        resizeContinues = StillDown();

        /* Calculate delta from original grow box click */
        short deltaH = currentPt.h - startPt.h;
        short deltaV = currentPt.v - startPt.v;

        /* Log first few iterations to debug */
        if (loopCount <= 5) {
            serial_puts("[GW] Loop: delta=");
            // Can't easily print ints, so just indicate we're updating
            if (deltaH != 0 || deltaV != 0) {
                serial_puts("[GW] Delta detected\n");
            }
        }

        /* Calculate new size */
        short newWidth = (short)(originalBounds.right - originalBounds.left + deltaH);
        short newHeight = (short)(originalBounds.bottom - originalBounds.top + deltaV);

        /* Constrain to minimum size */
        if (newWidth < MIN_RESIZE_WIDTH) newWidth = MIN_RESIZE_WIDTH;
        if (newHeight < MIN_RESIZE_HEIGHT) newHeight = MIN_RESIZE_HEIGHT;

        /* Build new outline rect */
        Rect newOutline = originalBounds;
        newOutline.right = originalBounds.left + newWidth;
        newOutline.bottom = originalBounds.top + newHeight;

        /* Update visual feedback if size changed */
        if (newOutline.right != outlineRect.right || newOutline.bottom != outlineRect.bottom) {
            serial_puts("[GW] Outline size changed\n");

            /* Erase old outline (XOR twice = erase) */
            if (outlineDrawn) {
                extern void InvertRect(const Rect* rect);
                extern void QDPlatform_FlushScreen(void);
                InvertRect(&outlineRect);
                QDPlatform_FlushScreen();
                serial_puts("[GW] Old outline erased\n");
            }

            /* Draw new outline */
            extern void InvertRect(const Rect* rect);
            extern void QDPlatform_FlushScreen(void);
            InvertRect(&newOutline);
            QDPlatform_FlushScreen();
            serial_puts("[GW] New outline drawn\n");

            outlineRect = newOutline;
            outlineDrawn = true;
            g_resizeState.hasMoved = true;
        }

        /* Note: Cannot use Platform_WaitTicks here - it would hang because TickCount()
           doesn't advance while we're blocking in event handling. Instead, let the loop
           iterate freely. The system responsiveness is maintained through the event loop. */
    }

    /* Erase the outline by XOR-ing one more time */
    if (outlineDrawn) {
        extern void InvertRect(const Rect* rect);
        extern void QDPlatform_FlushScreen(void);
        InvertRect(&outlineRect);
        QDPlatform_FlushScreen();
        serial_puts("[GW] Outline erased\n");
    }

    if (loopCount >= MAX_LOOP_ITERATIONS) {
        serial_puts("[GW] Loop hit iteration limit\n");
    } else {
        serial_puts("[GW] Loop exited normally\n");
    }

    serial_puts("[GW] Exited main loop\n");

    /* End resize feedback */
    serial_puts("[GW] Ending feedback\n");
    Local_EndResizeFeedback();
    serial_puts("[GW] Feedback ended\n");

    /* Calculate and apply final size */
    serial_puts("[GW] Calculating final size\n");

    /* Calculate final width and height from outline */
    short finalWidth = outlineRect.right - outlineRect.left;
    short finalHeight = outlineRect.bottom - outlineRect.top;
    short originalWidth = originalBounds.right - originalBounds.left;
    short originalHeight = originalBounds.bottom - originalBounds.top;
    long finalSize = ((long)finalWidth << 16) | (finalHeight & 0xFFFF);

    serial_puts("[GW] Final size calculated\n");
    serial_puts("[GW] Checking if resize is needed\n");

    /* Get the ACTUAL current window dimensions from the portRect */
    short actualWidth = theWindow->port.portRect.right - theWindow->port.portRect.left;
    short actualHeight = theWindow->port.portRect.bottom - theWindow->port.portRect.top;

    /* Log the size comparison for debugging */
    extern void serial_puts(const char *str);
    extern int sprintf(char* buf, const char* fmt, ...);
    char dbgbuf[256];
    snprintf(dbgbuf, sizeof(dbgbuf), "[GW] Size check: final=%dx%d actual=%dx%d original=%dx%d\n",
            finalWidth, finalHeight, actualWidth, actualHeight, originalWidth, originalHeight);
    serial_puts(dbgbuf);

    /* Apply resize ONLY if size actually changed from the ACTUAL dimensions */
    if (finalWidth != actualWidth || finalHeight != actualHeight) {
        snprintf(dbgbuf, sizeof(dbgbuf), "[GW] Size changed, applying resize\n");
        serial_puts(dbgbuf);
        WM_DEBUG("GrowWindow: Applying final resize from %dx%d to %dx%d (actual=%dx%d)",
                 actualWidth, actualHeight, finalWidth, finalHeight, actualWidth, actualHeight);

        /* Apply the resize */
        serial_puts("[GW] >>> Calling SizeWindow from GrowWindow\n");
        SizeWindow(theWindow, finalWidth, finalHeight, true);
        serial_puts("[GW] SizeWindow called\n");

        /* Force complete window redraw - MUST redraw frame/chrome AND content */
        /* PaintBehind is designed to repaint windows after structural changes */
        extern void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn);
        extern void FolderWindow_Draw(WindowPtr w);
        extern void BeginUpdate(WindowPtr window);
        extern void EndUpdate(WindowPtr window);

        serial_puts("[GW] Starting window redraw\n");

        /* CRITICAL: Explicitly redraw window chrome (frame and title bar)
         * PaintBehind doesn't necessarily repaint the chrome of the resized window itself */
        extern void PaintOne(WindowPtr window, RgnHandle clobberedRgn);
        serial_puts("[GW] Calling PaintOne to redraw window chrome\n");
        PaintOne(theWindow, NULL);
        serial_puts("[GW] PaintOne completed - chrome redrawn\n");

        /* Use PaintBehind to repaint windows behind the resized window */
        /* This ensures desktop/windows behind are redrawn for exposed areas */
        serial_puts("[GW] Calling PaintBehind to repaint exposed areas behind window\n");
        PaintBehind(theWindow->nextWindow, NULL);  /* Start with window BEHIND this one */
        serial_puts("[GW] PaintBehind completed\n");

        /* Also redraw folder content if applicable */
        serial_puts("[GW] Checking for folder window content\n");
        Boolean isFolderWindow = (theWindow->refCon == 0x4449534b || theWindow->refCon == 0x54525348);  /* 'DISK' or 'TRSH' */

        if (isFolderWindow) {
            /* Folder windows have special drawing code for the file list */
            serial_puts("[GW] Drawing folder window content\n");
            BeginUpdate(theWindow);
            FolderWindow_Draw(theWindow);
            EndUpdate(theWindow);
        }

        /* Flush screen to ensure all updates are visible */
        extern void QDPlatform_FlushScreen(void);
        QDPlatform_FlushScreen();
        serial_puts("[GW] Screen flushed\n");

        serial_puts("[GW] Resize applied and redrawn\n");
    } else {
        snprintf(dbgbuf, sizeof(dbgbuf), "[GW] No size change: %dx%d == %dx%d, skipping SizeWindow\n",
                finalWidth, finalHeight, actualWidth, actualHeight);
        serial_puts(dbgbuf);
        serial_puts("[GW] No size change detected\n");
    }

    /* Clean up resize state */
    serial_puts("[GW] Cleaning up\n");
    Local_CleanupResizeState();
    serial_puts("[GW] Cleanup complete\n");

    WM_DEBUG("GrowWindow: Grow tracking completed, result = 0x%08lX", finalSize);
    serial_puts("[GW] GrowWindow EXIT\n");
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
        Local_AnimateZoom(theWindow, &theWindow->port.portRect, &targetBounds);
    }

    /* Apply final size and position */
    short newWidth = targetBounds.right - targetBounds.left;
    short newHeight = targetBounds.bottom - targetBounds.top;

    MoveWindow(theWindow, targetBounds.left, targetBounds.top, false);
    extern void serial_puts(const char *str);
    serial_puts("[ZW] >>> Calling SizeWindow from ZoomWindow\n");
    SizeWindow(theWindow, newWidth, newHeight, true);

    /* Bring to front if requested */
    if (front) {
        SelectWindow(theWindow);
    }

    /* Update state checksum */
    Local_UpdateStateChecksum(stateData);

    WM_DEBUG("ZoomWindow: Zoom operation completed");
}

/* ============================================================================
 * Resize State Management
 * ============================================================================ */

static void Local_InitializeResizeState(WindowPtr window, Point startPt, const Rect* bBox) {
    WM_DEBUG("Local_InitializeResizeState: Initializing resize state");

    /* Clear previous state */
    Local_CleanupResizeState();

    /* Initialize resize state */
    g_resizeState.window = window;
    g_resizeState.startPoint = startPt;
    g_resizeState.currentPoint = startPt;
    /* [WM-009] Provenance: IM:Windows p.2-13 */
    g_resizeState.originalBounds = window->port.portRect;
    g_resizeState.currentBounds = window->port.portRect;
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
    Local_InitializeSnapSizes(window);

    WM_DEBUG("Local_InitializeResizeState: Resize state initialized");
}

static void Local_CleanupResizeState(void) {
    if (!g_resizeState.active) return;

    WM_DEBUG("Local_CleanupResizeState: Cleaning up resize state");

    /* Clear state */
    memset(&g_resizeState, 0, sizeof(ResizeState));

    WM_DEBUG("Local_CleanupResizeState: Cleanup complete");
}

static void Local_InitializeSnapSizes(WindowPtr window) {
    g_resizeState.snapCount = 0;

    /* Add common snap sizes */
    Local_AddSnapSize(320, 240);   /* Classic small */
    Local_AddSnapSize(640, 480);   /* Classic VGA */
    Local_AddSnapSize(800, 600);   /* Classic SVGA */
    Local_AddSnapSize(1024, 768);  /* Classic XGA */

    /* Add screen-based sizes */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);
    short screenWidth = screenBounds.right - screenBounds.left;
    short screenHeight = screenBounds.bottom - screenBounds.top;

    Local_AddSnapSize(screenWidth / 2, screenHeight / 2);  /* Quarter screen */
    Local_AddSnapSize(screenWidth * 2 / 3, screenHeight * 2 / 3); /* Two-thirds */
    Local_AddSnapSize(screenWidth - 40, screenHeight - 80); /* Almost full screen */

    WM_DEBUG("Local_InitializeSnapSizes: Added %d snap sizes", g_resizeState.snapCount);
}

static void Local_AddSnapSize(short width, short height) {
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

#if 0 /* Future use - resize feedback system currently unused */
static long Local_CalculateNewSize(Point currentPt) {
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
        Point snapSize = Local_ApplySnapToSize(newWidth, newHeight);
        newWidth = snapSize.h;
        newHeight = snapSize.v;
    }

    /* Return size as long (width in high word, height in low word) */
    return ((long)newWidth << 16) | (newHeight & 0xFFFF);
}
#endif

#if 0 /* Future use - only used by disabled Local_CalculateNewSize */
static Point Local_ApplySnapToSize(short width, short height) {
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
#endif

/* ============================================================================
 * Resize Feedback
 * ============================================================================ */

static void Local_StartResizeFeedback(void) {
    if (!g_resizeState.showFeedback) return;

    WM_DEBUG("WM_StartResizeFeedback: Starting resize feedback");

    /* Show initial size feedback */
    Platform_ShowSizeFeedback(&g_resizeState.originalBounds);
    g_resizeState.currentBounds = g_resizeState.originalBounds;
}

#if 0 /* Future use - resize feedback system currently unused */
static void Local_UpdateResizeFeedback(long newSize) {
    if (!g_resizeState.showFeedback) return;

    short width = (short)(newSize >> 16);
    short height = (short)(newSize & 0xFFFF);

    /* Calculate new window bounds */
    Rect oldBounds = g_resizeState.currentBounds;
    Rect newBounds = g_resizeState.originalBounds;
    newBounds.right = newBounds.left + width;
    newBounds.bottom = newBounds.top + height;

    Platform_UpdateSizeFeedback(&oldBounds, &newBounds);
    g_resizeState.currentBounds = newBounds;
}
#endif

static void Local_EndResizeFeedback(void) {
    if (!g_resizeState.showFeedback) return;

    WM_DEBUG("WM_EndResizeFeedback: Ending resize feedback");
    Platform_HideSizeFeedback(&g_resizeState.currentBounds);
}

/* ============================================================================
 * Window State Management
 * ============================================================================ */

WindowStateData* WM_GetWindowStateData(WindowPtr window) {
    if (window == NULL) return NULL;

    /* Try to get state data from auxiliary window record */
    AuxWinHandle auxWin;
    if (GetAuxWin(window, &auxWin)) {
        if (*auxWin != NULL) {
            /* Check if dataHandle contains our state data */
            if ((**auxWin).dialogCItem != NULL) {
                /* Verify this is our state data */
                WindowStateData* stateData = (WindowStateData*)((**auxWin).dialogCItem);
                if (Local_ValidateStateChecksum(stateData)) {
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
    WindowStateData* stateData = (WindowStateData*)NewPtrClear(sizeof(WindowStateData));
    if (stateData == NULL) return NULL;

    /* Initialize with current window bounds */
    /* [WM-009] Provenance: IM:Windows p.2-13 */
    stateData->userState = window->port.portRect;
    stateData->hasUserState = true;
    stateData->hasStdState = false;
    stateData->isZoomed = false;

    Local_UpdateStateChecksum(stateData);

    WM_DEBUG("WM_CreateWindowStateData: Created new window state data");
    return stateData;
}

static void WM_CalculateStandardState(WindowPtr window, Rect* stdState) {
    if (window == NULL || stdState == NULL) return;

    WM_DEBUG("WM_CalculateStandardState: Calculating standard state");

    /* Get screen bounds */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    extern void serial_puts(const char *str);
    extern int sprintf(char* buf, const char* fmt, ...);
    char dbgbuf[256];
    snprintf(dbgbuf, sizeof(dbgbuf), "[ZW] screenBounds=(%d,%d,%d,%d)\n",
            screenBounds.left, screenBounds.top, screenBounds.right, screenBounds.bottom);
    serial_puts(dbgbuf);

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

    snprintf(dbgbuf, sizeof(dbgbuf), "[ZW] stdState=(%d,%d,%d,%d) size=%dx%d\n",
            stdState->left, stdState->top, stdState->right, stdState->bottom,
            stdState->right - stdState->left, stdState->bottom - stdState->top);
    serial_puts(dbgbuf);
}

static void WM_UpdateWindowUserState(WindowPtr window) {
    WindowStateData* stateData = WM_GetWindowStateData(window);
    if (stateData == NULL) return;

    /* Update user state only if window is not currently zoomed */
    if (!stateData->isZoomed) {
        /* [WM-009] Provenance: IM:Windows p.2-13 */
        stateData->userState = window->port.portRect;
        stateData->hasUserState = true;
        Local_UpdateStateChecksum(stateData);
        WM_DEBUG("WM_UpdateWindowUserState: Updated user state");
    }
}

static Boolean Local_ValidateStateChecksum(WindowStateData* stateData) {
    if (stateData == NULL) return false;

    long checksum = Local_CalculateStateChecksum(stateData);
    return (checksum == stateData->stateChecksum);
}

static void Local_UpdateStateChecksum(WindowStateData* stateData) {
    if (stateData == NULL) return;

    stateData->stateChecksum = Local_CalculateStateChecksum(stateData);
}

static long Local_CalculateStateChecksum(WindowStateData* stateData) {
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

static void Local_AnimateZoom(WindowPtr window, const Rect* fromBounds, const Rect* toBounds) {
    if (!Platform_IsZoomAnimationEnabled()) return;

    WM_DEBUG("WM_AnimateZoom: Animating zoom transition");

    /* Calculate animation steps */
    for (int step = 1; step <= ZOOM_ANIMATION_STEPS; step++) {
        Rect currentBounds;
        Local_InterpolateRect(fromBounds, toBounds, step, ZOOM_ANIMATION_STEPS, &currentBounds);

        /* Show animation frame */
        Platform_ShowZoomFrame(&currentBounds);

        /* Delay between frames */
        Platform_WaitTicks(ZOOM_ANIMATION_DELAY / 16);
    }

    /* Hide final animation frame */
    Platform_HideZoomFrame(toBounds);

    WM_DEBUG("WM_AnimateZoom: Zoom animation completed");
}

static void Local_InterpolateRect(const Rect* fromRect, const Rect* toRect,
                              int step, int totalSteps, Rect* result) {
    if (fromRect == NULL || toRect == NULL || result == NULL) return;

    /* Guard against divide-by-zero */
    if (totalSteps <= 0) {
        *result = *toRect;  /* Just use destination rect */
        return;
    }

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

static void Local_GenerateResizeUpdateEvents(WindowPtr window, short oldWidth, short oldHeight,
                                        short newWidth, short newHeight) {
    if (window == NULL || window->updateRgn == NULL) return;

    WM_DEBUG("Local_GenerateResizeUpdateEvents: Generating update events for resize");

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

    WM_DEBUG("Local_GenerateResizeUpdateEvents: Update events generated");
}

/* Platform functions are in WindowPlatform.c */

/* Size feedback and zoom frame functions moved to WindowPlatform.c */

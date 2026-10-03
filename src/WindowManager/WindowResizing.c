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

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManagerInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include <math.h>

/* [WM-017] Forward declarations for file-local helpers */
/* Provenance: Standard C practice for static functions called before definition */
typedef struct WindowStateData WindowStateData;
WindowStateData* WM_GetWindowStateData(WindowPtr window);
static void WM_CalculateStandardState(WindowPtr window, Rect* stdState);
static void WM_UpdateWindowUserState(WindowPtr window);
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

/* Windows whose offscreen buffer could not be reallocated on a resize. A
 * window without a buffer is otherwise taken to have none by choice (About
 * This Macintosh draws straight to the screen). */
enum { kMaxLostBuffers = 16 };
static WindowPtr gLostBuffers[kMaxLostBuffers];

/* Whether theWindow is listed; with mark, list it. */
Boolean WM_BufferLost(WindowPtr theWindow, Boolean mark) {
    for (int i = 0; i < kMaxLostBuffers; i++) {
        if (gLostBuffers[i] == theWindow) return true;
    }
    if (mark) {
        for (int i = 0; i < kMaxLostBuffers; i++) {
            if (!gLostBuffers[i]) {
                gLostBuffers[i] = theWindow;
                break;
            }
        }
    }
    return false;
}

void WM_ForgetLostBuffer(WindowPtr theWindow) {
    for (int i = 0; i < kMaxLostBuffers; i++) {
        if (gLostBuffers[i] == theWindow) gLostBuffers[i] = NULL;
    }
}


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
    extern int snprintf(char* buf, size_t size, const char* fmt, ...);
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
        Rect oldContRect = (*theWindow->contRgn)->rgnBBox;

        /* The window keeps its own frame: the borders around the content
         * are measured, not assumed. This took every window for a document
         * window with a 20-pixel title bar, so a dialog resized grew one. */
        Rect newContRect = oldContRect;
        newContRect.right = newContRect.left + w;
        newContRect.bottom = newContRect.top + h;
        Rect newStrucRect = oldStrucRect;
        newStrucRect.right = newContRect.right + (oldStrucRect.right - oldContRect.right);
        newStrucRect.bottom = newContRect.bottom + (oldStrucRect.bottom - oldContRect.bottom);

        extern void Platform_SetRectRgn(RgnHandle rgn, const Rect* rect);
        Platform_SetRectRgn(theWindow->strucRgn, &newStrucRect);
        Platform_SetRectRgn(theWindow->contRgn, &newContRect);
    }

    /* Resize native platform window */
    Platform_SizeNativeWindow(theWindow, w, h);

    /* Reallocate the offscreen buffer for the new size.
     *
     * NewGWorld was only ever called when the window was created, and
     * DisposeGWorld only when it was closed - nothing resized the buffer. After
     * a window grew, drawing continued into a GWorld still dimensioned for the
     * ORIGINAL content size, so every update wrote past the end of that
     * allocation and corrupted whatever followed it on the heap.
     *
     * Growing 477x317 to 534x382 overruns by (534*382 - 477*317) pixels, about
     * 211 KB at 32bpp. The damage showed up as the window's own GrafPort coming
     * back full of allocator poison - portRect reading (-12851,-12851,...),
     * i.e. 0xCDCD padding fill and the 0xABAB canary - which then placed icons
     * outside the window, pushed labels off-port, blanked the content and
     * eventually hung. */
    if (theWindow->offscreenGWorld || WM_BufferLost(theWindow, false)) {
        if (theWindow->offscreenGWorld) {
            DisposeGWorld(theWindow->offscreenGWorld);
            theWindow->offscreenGWorld = NULL;
        }

        Rect gwRect;
        gwRect.top = 0;
        gwRect.left = 0;
        gwRect.right = w;
        gwRect.bottom = h;

        GWorldPtr newWorld = NULL;
        if (w > 0 && h > 0 && NewGWorld(&newWorld, 32, &gwRect, NULL, NULL, 0) == noErr) {
            theWindow->offscreenGWorld = newWorld;
            WM_ForgetLostBuffer(theWindow);
        } else {
            /* Left NULL rather than keeping an undersized buffer: drawing
             * goes to the screen through the clip, correct if less smooth.
             * Remembered, so the next resize tries again - a window that
             * failed once at full-screen size used to stay unbuffered for
             * good. */
            WM_BufferLost(theWindow, true);
            serial_puts("[SIZEWND] GWorld realloc failed; drawing unbuffered until the next resize\n");
        }
    }

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

                /* The desktop there, icons included, before the windows
                 * behind: the desk hook, as HideWindow uses. Erasing to the
                 * pattern left out the icons, so the Trash vanished when a
                 * zoomed window shrank back off it. The hook paints only
                 * where no window is. */
                extern DeskHookProc g_deskHook;  /* WindowDisplay.c */
                if (g_deskHook) {
                    g_deskHook(exposedDesktop);
                }

                /* Windows behind that were under the old frame need their
                 * frames back, not just their content. Back to front, so this
                 * window - painted below - ends up on top. */
                if (!EmptyRgn(exposedDesktop) && theWindow->nextWindow) {
                    extern void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn);
                    PaintBehind(theWindow->nextWindow, exposedDesktop);
                }

                Platform_DisposeRgn(exposedDesktop);
            }
        }

        if (oldStrucRgn) {
            WM_InvalidateScreenRegion(oldStrucRgn);
        }
        if (theWindow->strucRgn) {
            WM_InvalidateScreenRegion(theWindow->strucRgn);
        }

        /*
         * Redraw the frame at its new size.
         *
         * WM_InvalidateScreenRegion above adds to each window's updateRgn,
         * which is what update events redraw - and an update event redraws
         * content. The frame is the Window Manager's to draw, so nothing
         * above puts it back: a resized window kept its old content and lost
         * its title bar, its border and its grow box, with whatever had been
         * behind it still showing through where the frame used to be.
         *
         * PaintOne draws the frame and erases the content area under it; the
         * update events just queued then fill the content back in.
         */
        extern void PaintOne(WindowPtr window, RgnHandle clobberedRgn);
        PaintOne(theWindow, theWindow->strucRgn);
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

/*
 * GrowWindow - track the grow box (Inside Macintosh: Toolbox Essentials,
 * 4-106). While the button is down an outline of the window follows the
 * pointer; the content may be no smaller than bBox's left and top and no
 * larger than its right and bottom. Answers 0 if the size is unchanged,
 * else the new content size with the height in the high word and the
 * width in the low. The caller resizes the window, with SizeWindow.
 *
 * This measured the frame and handed it to SizeWindow as a content size,
 * so each grow added the frame's borders; packed width high and height
 * low, which swapped SimpleText's window; ignored bBox; and called
 * SizeWindow itself, then painted the windows behind over the resized
 * one's frame.
 */
long GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox) {
    if (theWindow == NULL || !WM_WindowHasGrowBox(theWindow)) return 0;
    if (!theWindow->strucRgn || !*theWindow->strucRgn) return 0;
    if (!Platform_IsMouseDown()) return 0;

    Rect frame = (*theWindow->strucRgn)->rgnBBox;
    short contentW = theWindow->port.portRect.right - theWindow->port.portRect.left;
    short contentH = theWindow->port.portRect.bottom - theWindow->port.portRect.top;
    short extraW = (frame.right - frame.left) - contentW;
    short extraH = (frame.bottom - frame.top) - contentH;

    short minW = MIN_RESIZE_WIDTH, minH = MIN_RESIZE_HEIGHT;
    short maxW = MAX_RESIZE_WIDTH, maxH = MAX_RESIZE_HEIGHT;
    if (bBox) {
        minW = bBox->left;  minH = bBox->top;
        maxW = bBox->right; maxH = bBox->bottom;
    }

    extern void EventPumpYield(void);
    extern void GetMouse(Point* mouseLoc);
    extern Boolean StillDown(void);
    extern void UpdateCursorDisplay(void);

    short newW = contentW, newH = contentH;
    Rect box = frame;
    Boolean outlineDrawn = false;

    for (long guard = 0; guard < 10000000L; guard++) {
        EventPumpYield();
        UpdateCursorDisplay();
        Point pt;
        GetMouse(&pt);
        Boolean down = StillDown();

        short w = contentW + (pt.h - startPt.h);
        short h = contentH + (pt.v - startPt.v);
        if (w < minW) w = minW;
        if (h < minH) h = minH;
        if (w > maxW) w = maxW;
        if (h > maxH) h = maxH;

        if (!outlineDrawn || w != newW || h != newH) {
            if (outlineDrawn) WM_XorFrame(&box);
            newW = w;
            newH = h;
            box.right = frame.left + newW + extraW;
            box.bottom = frame.top + newH + extraH;
            WM_XorFrame(&box);
            outlineDrawn = true;
        }
        if (!down) break;
    }
    if (outlineDrawn) WM_XorFrame(&box);

    if (newW == contentW && newH == contentH) return 0;
    return ((long)(unsigned short)newH << 16) | (unsigned short)newW;
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
            targetBounds = (*theWindow->contRgn)->rgnBBox;
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
    /* Both states are the content's global rectangle (Inside Macintosh:
     * Toolbox Essentials, 4-121). The user state was kept as the local
     * portRect, (0,0,w,h), so zooming back out put the window's corner at
     * the screen's, under the menu bar. */
    if (!shouldZoomOut) {
        stateData->userState = (*theWindow->contRgn)->rgnBBox;
        stateData->hasUserState = true;
    }

    /* Perform zoom animation */
    if (Platform_IsZoomAnimationEnabled()) {
        /* [WM-009] Provenance: IM:Windows p.2-13 */
        Local_AnimateZoom(theWindow, &(*theWindow->contRgn)->rgnBBox, &targetBounds);
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
 * Window State Management
 * ============================================================================ */

/*
 * The zoom states live in the window's dataHandle, as the WStateData record
 * does (Inside Macintosh: Toolbox Essentials, 4-121). They were looked for
 * in an auxiliary record that does not exist, so each call made new ones -
 * leaking them, and forgetting where the window had been before zooming.
 */
WindowStateData* WM_GetWindowStateData(WindowPtr window) {
    if (window == NULL) return NULL;
    if (window->dataHandle && *window->dataHandle) {
        return (WindowStateData*)HandleDataAligned(window->dataHandle);
    }
    Handle h = NewHandleClear(sizeof(WindowStateData));
    if (!h) return NULL;
    HLock(h);
    window->dataHandle = h;
    WindowStateData* stateData = (WindowStateData*)HandleDataAligned(h);
    if (!stateData) {
        window->dataHandle = NULL;
        HUnlock(h);
        DisposeHandle(h);
        return NULL;
    }
    if (window->contRgn && *window->contRgn) {
        stateData->userState = (*window->contRgn)->rgnBBox;
        stateData->hasUserState = true;
    }
    return stateData;
}

static void WM_CalculateStandardState(WindowPtr window, Rect* stdState) {
    if (window == NULL || stdState == NULL) return;

    WM_DEBUG("WM_CalculateStandardState: Calculating standard state");

    /* Get screen bounds */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    extern void serial_puts(const char *str);
    extern int snprintf(char* buf, size_t size, const char* fmt, ...);
    char dbgbuf[256];
    snprintf(dbgbuf, sizeof(dbgbuf), "[ZW] screenBounds=(%d,%d,%d,%d)\n",
            screenBounds.left, screenBounds.top, screenBounds.right, screenBounds.bottom);
    serial_puts(dbgbuf);

    /* The screen below the menu bar, less the window's own frame and a
     * margin: the frame's thickness on each side is measured from the
     * window. This was 80% of the screen as a frame rectangle, handed to
     * SizeWindow as a content size. */
    short menuBar = 20;
    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState && wmState->menuBarHeight > 0) menuBar = wmState->menuBarHeight;
    Rect f = (*window->strucRgn)->rgnBBox, c = (*window->contRgn)->rgnBBox;
    short leftEdge = c.left - f.left, topEdge = c.top - f.top;
    short rightEdge = f.right - c.right, bottomEdge = f.bottom - c.bottom;
    const short margin = 4;
    WM_SetRect(stdState,
               screenBounds.left + margin + leftEdge,
               screenBounds.top + menuBar + margin + topEdge,
               screenBounds.right - margin - rightEdge,
               screenBounds.bottom - margin - bottomEdge);

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
        if (window->contRgn && *window->contRgn) {
            stateData->userState = (*window->contRgn)->rgnBBox;
            stateData->hasUserState = true;
        }
        Local_UpdateStateChecksum(stateData);
        WM_DEBUG("WM_UpdateWindowUserState: Updated user state");
    }
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
    (void)window;
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

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
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDrawConstants.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WMLogging.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"

/* External logging function */
extern void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...);
extern void serial_puts(const char* str);
extern void serial_putchar(char ch);

static void wm_log_hex_u32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

static void wm_log_memfill(const char* tag, const void* base, size_t length) {
    serial_puts(tag);
    serial_puts(" base=0x");
    wm_log_hex_u32((uint32_t)(uintptr_t)base);
    serial_puts(" len=0x");
    wm_log_hex_u32((uint32_t)length);
    serial_puts(" end=0x");
    wm_log_hex_u32((uint32_t)((uintptr_t)base + length));
    serial_putchar('\n');
}

/* Forward declarations for internal helpers */
static Boolean WM_IsMouseDown(void);
static GrafPtr WM_GetCurrentPort(void);
/* static GrafPtr WM_GetUpdatePort(WindowPtr window); */
static Boolean WM_EmptyRgn(RgnHandle rgn);

/* ============================================================================
 * Public Functions
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
            extern Boolean WM_WindowHasZoomBox(WindowPtr window);
            if (WM_WindowHasZoomBox(theWindow)) {
                Platform_GetWindowZoomBoxRect(theWindow, &partRect);
                validPart = true;
            }
            break;
        case inGrow:
            /* Check if window has grow box (resize capability) */
            extern Boolean WM_WindowHasGrowBox(WindowPtr window);
            if (WM_WindowHasGrowBox(theWindow)) {
                Platform_GetWindowGrowBoxRect(theWindow, &partRect);
                validPart = true;
            }
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

    /* Hide cursor before tracking to prevent cursor save-under from capturing
     * ghost pixels from InvertRect highlighting. We'll show it again after
     * tracking completes and the window/title bar has been redrawn. */
    extern void HideCursor(void);
    extern void ShowCursor(void);
    extern void UpdateCursorDisplay(void);

    HideCursor();

    /* Force immediate cursor erase - HideCursor only sets a flag, we need
     * to actually remove the cursor pixels before InvertRect draws */
    UpdateCursorDisplay();

    /* Track mouse while button is down */
    Boolean buttonDown = true;
    Boolean inPart = true;
    Boolean lastInPart = true;
    Point currentPt = thePt;
    int loopCount = 0;
    const int MAX_TRACKING_ITERATIONS = 500;  /* Timeout after ~500ms at 1ms per iteration */

    extern void serial_puts(const char* str);
    serial_puts("[TB] Entering tracking loop\n");

    /* Process input ONCE before checking button state to ensure gCurrentButtons
     * is up-to-date with the latest PS/2 events */
    extern void ProcessModernInput(void);
    ProcessModernInput();
    serial_puts("[TB] Processed input before loop\n");

    while (buttonDown && loopCount < MAX_TRACKING_ITERATIONS) {
        /* Get current mouse position and button state */
        /* TODO: Get actual mouse state from platform */
        buttonDown = WM_IsMouseDown();

        if (loopCount == 0) {
            char buf[80];
            snprintf(buf, sizeof(buf), "[TB] First iteration: buttonDown=%d\n", buttonDown ? 1 : 0);
            serial_puts(buf);
        }

        loopCount++;
        if (loopCount % 50 == 0 || loopCount < 5) {
            char buf[80];
            snprintf(buf, sizeof(buf), "[TB] Loop %d, buttonDown=%d, partCode=%d\n", loopCount, buttonDown, partCode);
            serial_puts(buf);
        }

        if (buttonDown) {
            Platform_GetMousePosition(&currentPt);
        }

        serial_puts("[TB-LOOP] After mouse check\n");

        /* Check if mouse is still in the part */
        inPart = WM_PtInRect(currentPt, &partRect);

        serial_puts("[TB-LOOP] After inPart check\n");

        /* Update visual feedback if state changed */
        if (inPart != lastInPart) {
            /* Highlight or unhighlight the part */
            serial_puts("[TB-LOOP] About to highlight\n");
            Platform_HighlightWindowPart(theWindow, partCode, inPart);
            serial_puts("[TB-LOOP] After highlight\n");
            lastInPart = inPart;
        }

        serial_puts("[TB-LOOP] About to wait\n");
        /* Brief delay to avoid consuming too much CPU */
        Platform_WaitTicks(1);
        serial_puts("[TB-LOOP] After wait, looping\n");
    }

    /* If we hit the timeout limit, log it */
    if (loopCount >= MAX_TRACKING_ITERATIONS) {
        serial_puts("[TB] WARNING: Tracking loop timeout - exceeded max iterations\n");
        buttonDown = false;  /* Force exit */
    }

    char buf[80];
    snprintf(buf, sizeof(buf), "[TB] Exited loop after %d iterations\n", loopCount);
    serial_puts(buf);

    /* DON'T call Platform_HighlightWindowPart to unhighlight - it will invert again
     * and leave a ghost on the framebuffer. Instead, just invalidate and let the
     * window redraw cleanly from the GWorld buffer which never had the highlight.
     *
     * Platform_HighlightWindowPart(theWindow, partCode, false);  // SKIP THIS
     */

    /* Force immediate window AND desktop redraw to remove all ghost artifacts.
     * This is necessary because:
     * 1. InvertRect draws directly to framebuffer during tracking (for close box feedback)
     * 2. Cursor is drawn to framebuffer and can move anywhere during tracking
     * 3. With GWorld double-buffering, the offscreen buffer doesn't have these pixels
     * 4. Cursor continues to be drawn after window redraw, leaving new ghosts
     * Solution: Redraw window + force desktop manager to redraw background */
    serial_puts("[TB] Calling BeginUpdate\n");
    BeginUpdate(theWindow);
    serial_puts("[TB] BeginUpdate done\n");

    /* Draw window contents if this is a folder window */
    extern Boolean IsFolderWindow(WindowPtr w);
    extern void FolderWindow_Draw(WindowPtr w);
    serial_puts("[TB] Checking if folder window\n");
    if (IsFolderWindow(theWindow)) {
        serial_puts("[TB] Drawing folder window\n");
        FolderWindow_Draw(theWindow);
        serial_puts("[TB] Folder window drawn\n");
    }

    serial_puts("[TB] Calling EndUpdate\n");
    EndUpdate(theWindow);
    serial_puts("[TB] EndUpdate done\n");

    /* TODO: Also need to invalidate desktop/background to clean up cursor ghosts
     * that appear when moving mouse after close button tracking completes.
     * For now, window redraw fixes ghosts within window bounds. */

    /* Manually erase the close box area to remove InvertRect artifacts.
     * We need to draw directly to framebuffer since the title bar chrome
     * is outside the GWorld content buffer. */
    serial_puts("[TB] Checking if close box needs drawing\n");
    if (partCode == inGoAway) {
        extern void Platform_DrawCloseBoxDirect(WindowPtr window);
        serial_puts("[TB] Drawing close box directly\n");
        Platform_DrawCloseBoxDirect(theWindow);
        serial_puts("[TB] Close box drawn\n");
    }

    /* Show cursor now that window/title bar has been redrawn cleanly */
    serial_puts("[TB] Showing cursor\n");
    ShowCursor();
    serial_puts("[TB] Cursor shown\n");

    /* Return true if mouse was released inside the part */
    Boolean result = inPart;
    char resultBuf[60];
    snprintf(resultBuf, sizeof(resultBuf), "[TB] TrackBox returning %d\n", result ? 1 : 0);
    serial_puts(resultBuf);
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

/* Temporarily disable ALL WM logging to prevent heap corruption from variadic serial_logf */
#undef WM_LOG_DEBUG
#undef WM_LOG_TRACE
#undef WM_LOG_WARN
#undef WM_LOG_ERROR
#undef WM_DEBUG
#define WM_LOG_DEBUG(...) do {} while(0)
#define WM_LOG_TRACE(...) do {} while(0)
#define WM_LOG_WARN(...) do {} while(0)
#define WM_LOG_ERROR(...) do {} while(0)
#define WM_DEBUG(...) do {} while(0)

void InvalRgn(RgnHandle badRgn) {

    if (badRgn == NULL) {
        WM_LOG_WARN("WindowManager: InvalRgn called with NULL region\n");
        return;
    }

    /* Get current graphics port */
    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) {
        WM_LOG_WARN("WindowManager: InvalRgn - no current port\n");
        return;
    }

    /* Assume current port is a window */
    WindowPtr window = (WindowPtr)currentPort;

    /* Debug: badRgnData, updateBefore/After retained for instrumentation if needed */
    /* Region* badRgnData = *badRgn; */
    WM_LOG_TRACE("WindowManager: InvalRgn window=0x%08x, badRgn bbox=(%d,%d,%d,%d)\n",
                 (unsigned int)window, badRgnData->rgnBBox.left, badRgnData->rgnBBox.top,
                 badRgnData->rgnBBox.right, badRgnData->rgnBBox.bottom);

    /* Add region to window's update region */
    if (window->updateRgn) {
        /* Region* updateBefore = *(window->updateRgn); */
        WM_LOG_TRACE("WindowManager: InvalRgn - BEFORE union, updateRgn bbox=(%d,%d,%d,%d)\n",
                     updateBefore->rgnBBox.left, updateBefore->rgnBBox.top,
                     updateBefore->rgnBBox.right, updateBefore->rgnBBox.bottom);

        Platform_UnionRgn(window->updateRgn, badRgn, window->updateRgn);

        /* Region* updateAfter = *(window->updateRgn); */
        WM_LOG_TRACE("WindowManager: InvalRgn - AFTER union, updateRgn bbox=(%d,%d,%d,%d)\n",
                     updateAfter->rgnBBox.left, updateAfter->rgnBBox.top,
                     updateAfter->rgnBBox.right, updateAfter->rgnBBox.bottom);

        /* Schedule platform update */
        /* TODO: Convert region to rectangle for platform invalidation */
        Rect regionBounds;
        Platform_GetRegionBounds(badRgn, &regionBounds);
        Platform_InvalidateWindowRect(window, &regionBounds);

        /* Post update event to Event Manager so application can redraw */
        /* PostEvent declared in EventManager.h */
        PostEvent(6 /* updateEvt */, (SInt32)window);
        WM_LOG_DEBUG("WindowManager: InvalRgn - Posted updateEvt for window=0x%08x\n", (unsigned int)window);
    } else {
        WM_LOG_WARN("WindowManager: InvalRgn - window has NULL updateRgn!\n");
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
    serial_puts("[MEM] BeginUpdate before processing window\n");
    MemoryManager_CheckSuspectBlock("pre_BeginUpdate");

    /* Save current port */
    GrafPtr savePort = Platform_GetCurrentPort();
    Platform_SetUpdatePort(savePort);

    /* NOTE: portBits.bounds should already be set correctly to GLOBAL coordinates
     * by InitializeWindowRecord during window creation. DO NOT overwrite it here!
     * portBits.bounds maps local coords to global screen position. */

    /* DEBUG: Log portBits.bounds for control panel windows */
    extern void serial_puts(const char* str);
    extern int sprintf(char* buf, const char* fmt, ...);
    static int beginupd_log = 0;
    if (beginupd_log < 20) {
        char dbgbuf[256];
        sprintf(dbgbuf, "[BEGINUPD] refCon=0x%08x portBits.bounds=(%d,%d,%d,%d) portRect=(%d,%d,%d,%d)\n",
                (unsigned int)theWindow->refCon,
                theWindow->port.portBits.bounds.left, theWindow->port.portBits.bounds.top,
                theWindow->port.portBits.bounds.right, theWindow->port.portBits.bounds.bottom,
                theWindow->port.portRect.left, theWindow->port.portRect.top,
                theWindow->port.portRect.right, theWindow->port.portRect.bottom);
        serial_puts(dbgbuf);
        beginupd_log++;
    }

    /* GWorld rendering disabled - draw directly to window */
    Platform_SetCurrentPort(&theWindow->port);

    /* Begin platform drawing session */
    Platform_BeginWindowDraw(theWindow);

    /* CRITICAL: Set clip region to intersection of CONTENT and update regions
     * (not visRgn, which includes chrome and allows content to overdraw it!) */
    if (theWindow->contRgn && theWindow->updateRgn) {
        RgnHandle updateClip = Platform_NewRgn();
        if (updateClip) {
            Platform_IntersectRgn(theWindow->contRgn, theWindow->updateRgn, updateClip);
            Platform_SetClipRgn(&theWindow->port, updateClip);
            /* FIXME: Platform_DisposeRgn hangs in DisposePtr((Ptr)) - possible heap corruption
             * Skip for now to avoid leak, but this needs investigation */
            /* Platform_DisposeRgn(updateClip); */
        }
    } else if (theWindow->contRgn) {
        /* If no updateRgn, just use contRgn to prevent overdrawing chrome */
        Platform_SetClipRgn(&theWindow->port, theWindow->contRgn);
    }

    /* Erase update region to window background (direct framebuffer) */
    if (theWindow->port.portBits.baseAddr) {
        extern uint32_t fb_pitch;
        uint32_t bytes_per_pixel = 4;

        /* Get window dimensions from portRect (LOCAL coords) */
        Rect portRect = theWindow->port.portRect;
        SInt16 width = portRect.right - portRect.left;
        SInt16 height = portRect.bottom - portRect.top;

        /* CRITICAL FIX: Calculate offset to window's position in framebuffer
         * portBits.bounds contains GLOBAL screen coordinates where window content is
         * We must offset the framebuffer pointer to the window's actual position */
        Rect bounds = theWindow->port.portBits.bounds;
        uint32_t fbOffset = (uint32_t)bounds.top * fb_pitch + (uint32_t)bounds.left * bytes_per_pixel;
        UInt32* windowPixels = (UInt32*)((uint8_t*)theWindow->port.portBits.baseAddr + fbOffset);

        /* Fill window content area with opaque white (0xFFFFFFFF = ARGB white) */
        for (SInt16 y = 0; y < height; y++) {
            for (SInt16 x = 0; x < width; x++) {
                windowPixels[y * (fb_pitch / bytes_per_pixel) + x] = 0xFFFFFFFF;
            }
        }
    }

    WM_DEBUG("BeginUpdate: Update session started");
    serial_puts("[MEM] BeginUpdate after setup window\n");
    MemoryManager_CheckSuspectBlock("after_BeginUpdate");
}

void EndUpdate(WindowPtr theWindow) {
    serial_puts("[EndUpdate] ENTRY\n");
    serial_puts("[MEM] EndUpdate enter\n");
    MemoryManager_CheckSuspectBlock("enter_EndUpdate");
    if (theWindow == NULL) {
        serial_puts("[EndUpdate] NULL window, returning\n");
        return;
    }

    serial_puts("[EndUpdate] About to log debug message\n");
    WM_DEBUG("EndUpdate: Ending window update");
    serial_puts("[EndUpdate] After WM_DEBUG\n");
    MemoryManager_CheckSuspectBlock("post_debug_EndUpdate");

    /* GWorld rendering disabled - using direct framebuffer instead */

    /* Clear the update region */
    serial_puts("[EndUpdate] About to clear update region\n");
    if (theWindow->updateRgn) {
        serial_puts("[EndUpdate] Calling Platform_SetEmptyRgn...\n");
        Platform_SetEmptyRgn(theWindow->updateRgn);
        serial_puts("[EndUpdate] Platform_SetEmptyRgn returned\n");
        MemoryManager_CheckSuspectBlock("after_SetEmptyRgn");
    }

    /* End platform drawing session */
    serial_puts("[EndUpdate] Calling Platform_EndWindowDraw...\n");
    Platform_EndWindowDraw(theWindow);
    serial_puts("[EndUpdate] Platform_EndWindowDraw returned\n");
    MemoryManager_CheckSuspectBlock("after_EndWindowDraw");

    /* CRITICAL: Restore clipping to content region (not visRgn!)
     * to prevent content from overdrawing chrome */
    serial_puts("[EndUpdate] About to restore clipping\n");
    if (theWindow->contRgn) {
        serial_puts("[EndUpdate] Calling Platform_SetClipRgn...\n");
        Platform_SetClipRgn(&theWindow->port, theWindow->contRgn);
        serial_puts("[EndUpdate] Platform_SetClipRgn returned\n");
        MemoryManager_CheckSuspectBlock("after_SetClipRgn");
    }

    /* Restore previous port */
    serial_puts("[EndUpdate] Calling Platform_GetUpdatePort...\n");
    GrafPtr savedPort = Platform_GetUpdatePort(theWindow);
    serial_puts("[EndUpdate] Platform_GetUpdatePort returned\n");
    if (savedPort) {
        serial_puts("[EndUpdate] Calling Platform_SetCurrentPort...\n");
        Platform_SetCurrentPort(savedPort);
        serial_puts("[EndUpdate] Platform_SetCurrentPort returned\n");
        MemoryManager_CheckSuspectBlock("after_SetCurrentPort");
    }

    serial_puts("[EndUpdate] About to final WM_DEBUG\n");
    WM_DEBUG("EndUpdate: Update session ended");
    serial_puts("[EndUpdate] EXIT\n");
    serial_puts("[MEM] EndUpdate exit\n");
    MemoryManager_CheckSuspectBlock("exit_EndUpdate");
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
    Boolean buttonDown = true;

    /* Get region bounds */
    Rect rgnBounds;
    Platform_GetRegionBounds(theRgn, &rgnBounds);

    /* Show initial gray outline at start position */
    Rect dragRect = rgnBounds;
    WM_OffsetRect(&dragRect, startPt.h - rgnBounds.left, startPt.v - rgnBounds.top);
    Platform_ShowDragOutline(&dragRect);

    while (buttonDown) {
        /* Get current mouse position and state */
        buttonDown = WM_IsMouseDown();
        if (buttonDown) {
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
            Rect oldDragRect = rgnBounds;
            WM_OffsetRect(&oldDragRect, lastPt.h - rgnBounds.left, lastPt.v - rgnBounds.top);

            Rect newDragRect = rgnBounds;
            WM_OffsetRect(&newDragRect, currentPt.h - rgnBounds.left, currentPt.v - rgnBounds.top);

            Platform_UpdateDragOutline(&oldDragRect, &newDragRect);
            dragRect = newDragRect;
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
    Platform_HideDragOutline(&dragRect);

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
            /* Grow tracking - returns new size as long (width << 16 | height) */
            {
                extern long GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox);
                /* Use default screen bounds for grow limits */
                long newSize = GrowWindow(window, startPt, NULL);
                /* Return true if window was actually resized */
                return (newSize != 0);
            }
        case inDrag:
            /* Drag tracking - moves window to new position */
            {
                extern void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect);
                /* Use default screen bounds for drag limits */
                DragWindow(window, startPt, NULL);
                /* DragWindow is void, but tracking completed successfully */
                return true;
            }
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
    extern Boolean Button(void);
    return Button();
}

/* [WM-050] Platform_* functions removed - implemented in WindowPlatform.c */

static GrafPtr WM_GetCurrentPort(void) {
    extern void GetPort(GrafPtr* port);
    GrafPtr currentPort;
    GetPort(&currentPort);
    return currentPort;
}

/* [WM-050] Platform port functions removed - stubs only */

/* static GrafPtr WM_GetUpdatePort(WindowPtr window) {
    // TODO: Implement platform-specific update port retrieval
    return NULL;
} */

/* [WM-050] Platform_SetClipRgn removed - stub only */

static Boolean WM_EmptyRgn(RgnHandle rgn) {
    extern Boolean EmptyRgn(RgnHandle rgn);
    if (rgn == NULL) {
        return true;
    }
    return EmptyRgn(rgn);
}

/* [WM-050] Platform region/drag functions removed - implemented in WindowPlatform.c or stubs only */

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

#include "SystemInternal.h"
#include "System71StdLib.h"
#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDrawConstants.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WMLogging.h"
#include "WindowManager/WindowRegions.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "Platform/Framebuffer.h"
#include "TimeManager/TimeBase.h"

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

                /* A negative windowKind is a system window - a desk accessory's,
                 * carrying the negated refNum of the DA that owns it. Inside
                 * Macintosh has FindWindow report the whole window as
                 * inSysWindow and leave the part to SystemClick, which is what
                 * lets an accessory decide for itself what a click on its title
                 * bar or close box means. The caller routes it there. */
                if (current->windowKind < 0) {
                    WM_DEBUG("FindWindow: Hit system (desk accessory) window");
                    return inSysWindow;
                }

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
            if (WM_WindowHasCloseBox(theWindow)) {
                Platform_GetWindowCloseBoxRect(theWindow, &partRect);
                validPart = true;
            }
            break;
        case inZoomIn:
        case inZoomOut:
            /* Check if window has zoom box */
            if (WM_WindowHasZoomBox(theWindow)) {
                Platform_GetWindowZoomBoxRect(theWindow, &partRect);
                validPart = true;
            }
            break;
        case inGrow:
            /* Check if window has grow box (resize capability) */
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
    HideCursor();

    /* Force immediate cursor erase - HideCursor only sets a flag, we need
     * to actually remove the cursor pixels before InvertRect draws */
    UpdateCursorDisplay();

    /* Track mouse while button is down.
     *
     * lastInPart starts false, not true: the press is inside the part by
     * definition, so the first pass through the loop must find a change and
     * draw the highlight. Starting it true meant the press feedback System 7
     * gives you - the close box filling in while you hold it - was never
     * drawn at all unless you first dragged out of the box and back in. */
    Boolean buttonDown = true;
    Boolean inPart = true;
    Boolean lastInPart = false;
    Point currentPt = thePt;

    /* Bound the track by elapsed time, not by iteration count. Nothing paces
     * this loop at a known rate, so a count of iterations says nothing about
     * how long the user is given to make up their mind - the same mistake the
     * drag loop in WindowDragging.c was carrying. This is a safety stop for a
     * button release we never observe, not a limit anyone should reach. */
    const UInt32 kMaxTrackTicks = 60 * 30;   /* 30 seconds */
    const UInt32 trackStartTick = TickCount();

    /* Process input ONCE before checking button state to ensure gCurrentButtons
     * is up-to-date with the latest PS/2 events */
    ProcessModernInput();

    while (buttonDown && (TickCount() - trackStartTick) < kMaxTrackTicks) {
        /* Get current mouse position and button state */
        buttonDown = WM_IsMouseDown();

        if (buttonDown) {
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

    if (buttonDown) {
        WM_DEBUG("TrackBox: tracking timed out with the button still down");
    }

    /* Undo the highlight the same way it was applied: InvertRect is its own
     * inverse, so one more call restores whatever was underneath. This replaces
     * a full window-and-desktop repaint followed by a hand-drawn substitute
     * close box, which did not match the chrome the frame painter draws and
     * left a visibly wrong box behind every cancelled close. */
    if (lastInPart) {
        Platform_HighlightWindowPart(theWindow, partCode, false);
    }

    /* The pointer has usually moved while hidden, so the background saved at
     * the press point is stale and must not be written back over the new
     * location. */
    InvalidateCursor();
    ShowCursor();
    UpdateCursorDisplay();

    /* Return true if mouse was released inside the part */
    /* Only a release inside the part counts. Timing out with the button still
     * held must not be read as a click, or the safety stop becomes a way to
     * close a window by leaning on the mouse. */
    Boolean result = inPart && !buttonDown;
    WM_DEBUG("TrackBox: Tracking complete, result = %s", result ? "true" : "false");
    return result;
}

Boolean TrackGoAway(WindowPtr theWindow, Point thePt) {
    if (!WM_WindowHasCloseBox(theWindow)) return false;

    WM_DEBUG("TrackGoAway: Tracking close box");
    return TrackBox(theWindow, thePt, inGoAway);
}

/* ============================================================================
 * Update Region Management
 * ============================================================================ */

/*
 * InvalWindowRect - invalidate a rectangle in a named window
 *
 * InvalRect takes no window: it invalidates whichever port happens to be
 * current. That is fine inside a window's own drawing, and wrong everywhere
 * else - a menu command, a resize, anything running from the Window Manager's
 * port. The rectangle lands in some other window's update region and the one
 * that changed is never repainted, so the feature looks like it does nothing.
 * SimpleText's Style and Size menus failed exactly that way, and dialog edit
 * fields and the desktop drag have each produced their own version of it.
 *
 * Mac OS added InvalWindowRect for this reason. Callers that know which
 * window they mean should say so.
 */
void InvalWindowRect(WindowPtr window, const Rect* badRect)
{
    GrafPtr savePort;

    if (!window || !badRect) return;

    GetPort(&savePort);
    SetPort((GrafPtr)window);
    InvalRect(badRect);
    SetPort(savePort);
}

/*
 * How far a window's local coordinates are from global ones. The update
 * region is global, like the content and structure regions it is combined
 * with; InvalRect and the rest take the current port's local coordinates
 * (Inside Macintosh: Toolbox Essentials, 4-111) and used to add them
 * unconverted, so anything invalidated in a window not at the screen's
 * origin was redrawn in the wrong place, and ValidRect cleared the wrong
 * area. Measured from contRgn, which BeginUpdate's port swap leaves alone.
 */
static Boolean WM_PortIsWindow(GrafPtr port) {
    WindowManagerState* wm = GetWindowManagerState();
    for (WindowPtr w = wm ? wm->windowList : NULL; w; w = w->nextWindow) {
        if ((GrafPtr)w == port) return true;
    }
    return false;
}

/* Only a window has an update region: the desktop's port is a plain
 * GrafPort, and treating it as a window wrote past its end. */
static Boolean WM_LocalToGlobalOffset(WindowPtr window, short* dh, short* dv) {
    if (!window || !WM_PortIsWindow((GrafPtr)window)) return false;
    if (!window->contRgn || !*window->contRgn) return false;
    *dh = (*window->contRgn)->rgnBBox.left - window->port.portRect.left;
    *dv = (*window->contRgn)->rgnBBox.top - window->port.portRect.top;
    return true;
}

/* A copy of the current port's local rect as a global region, or NULL. */
static RgnHandle WM_GlobalRgnFromLocalRect(WindowPtr window, const Rect* r) {
    short dh, dv;
    if (!WM_LocalToGlobalOffset(window, &dh, &dv)) return NULL;
    RgnHandle rgn = Platform_NewRgn();
    if (!rgn) return NULL;
    Rect g = *r;
    OffsetRect(&g, dh, dv);
    Platform_SetRectRgn(rgn, &g);
    return rgn;
}

static RgnHandle WM_GlobalRgnFromLocalRgn(WindowPtr window, RgnHandle local) {
    short dh, dv;
    if (!WM_LocalToGlobalOffset(window, &dh, &dv)) return NULL;
    RgnHandle rgn = Platform_NewRgn();
    if (!rgn) return NULL;
    CopyRgn(local, rgn);
    OffsetRgn(rgn, dh, dv);
    return rgn;
}

/* Add a region already in global coordinates to a window's update region:
 * for the Window Manager's own use, with content and structure regions. */
void WM_InvalGlobalRgn(WindowPtr window, RgnHandle globalRgn) {
    if (!window || !globalRgn || !*globalRgn) return;
    if (!window->updateRgn) window->updateRgn = Platform_NewRgn();
    if (window->updateRgn) Platform_UnionRgn(window->updateRgn, globalRgn, window->updateRgn);
}

void InvalRect(const Rect* badRect) {
    if (badRect == NULL) return;

    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;
    WindowPtr window = (WindowPtr)currentPort;

    RgnHandle bad = WM_GlobalRgnFromLocalRect(window, badRect);
    if (bad && !window->updateRgn) window->updateRgn = Platform_NewRgn();
    if (bad && window->updateRgn) {
        Platform_UnionRgn(window->updateRgn, bad, window->updateRgn);
    }
    if (bad) Platform_DisposeRgn(bad);
}

void InvalRgn(RgnHandle badRgn) {
    if (badRgn == NULL) return;

    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;
    WindowPtr window = (WindowPtr)currentPort;

    RgnHandle bad = WM_GlobalRgnFromLocalRgn(window, badRgn);
    if (bad && !window->updateRgn) window->updateRgn = Platform_NewRgn();
    if (bad && window->updateRgn) {
        Platform_UnionRgn(window->updateRgn, bad, window->updateRgn);
    }
    if (bad) Platform_DisposeRgn(bad);
}

void ValidRect(const Rect* goodRect) {
    if (goodRect == NULL) return;

    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;
    WindowPtr window = (WindowPtr)currentPort;

    if (WM_PortIsWindow(currentPort) && window->updateRgn) {
        RgnHandle good = WM_GlobalRgnFromLocalRect(window, goodRect);
        if (good) {
            Platform_DiffRgn(window->updateRgn, good, window->updateRgn);
            Platform_DisposeRgn(good);
        }
    }
}

void ValidRgn(RgnHandle goodRgn) {
    if (goodRgn == NULL) return;

    GrafPtr currentPort = WM_GetCurrentPort();
    if (currentPort == NULL) return;
    WindowPtr window = (WindowPtr)currentPort;

    if (WM_PortIsWindow(currentPort) && window->updateRgn) {
        RgnHandle good = WM_GlobalRgnFromLocalRgn(window, goodRgn);
        if (good) {
            Platform_DiffRgn(window->updateRgn, good, window->updateRgn);
            Platform_DisposeRgn(good);
        }
    }
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
    static int beginupd_log = 0;
    if (beginupd_log < 20) {
        char dbgbuf[256];
        snprintf(dbgbuf, sizeof(dbgbuf), "[BEGINUPD] refCon=0x%08x portBits.bounds=(%d,%d,%d,%d) portRect=(%d,%d,%d,%d)\n",
                (unsigned int)theWindow->refCon,
                theWindow->port.portBits.bounds.left, theWindow->port.portBits.bounds.top,
                theWindow->port.portBits.bounds.right, theWindow->port.portBits.bounds.bottom,
                theWindow->port.portRect.left, theWindow->port.portRect.top,
                theWindow->port.portRect.right, theWindow->port.portRect.bottom);
        serial_puts(dbgbuf);
        beginupd_log++;
    }

    /* If window has offscreen GWorld, swap portBits to point to GWorld buffer */
    if (theWindow->offscreenGWorld) {
        /* Get GWorld PixMap */
        PixMapHandle gwPixMap = GetGWorldPixMap((GWorldPtr)theWindow->offscreenGWorld);
        if (gwPixMap && *gwPixMap) {
            /* Swap window's portBits to point to GWorld buffer */
            theWindow->port.portBits.baseAddr = (*gwPixMap)->baseAddr;
            theWindow->port.portBits.rowBytes = (*gwPixMap)->rowBytes & 0x3FFF;
        }

        /* Set port to window (which now points to GWorld buffer) */
        Platform_SetCurrentPort(&theWindow->port);
        WM_DEBUG("BeginUpdate: Swapped portBits to GWorld buffer");
    } else {
        /* No offscreen buffer, draw directly to window (legacy path) */
        Platform_SetCurrentPort(&theWindow->port);
    }

    /* Begin platform drawing session */
    Platform_BeginWindowDraw(theWindow);

    /* CRITICAL: Set clip region to intersection of CONTENT and update regions
     * (not visRgn, which includes chrome and allows content to overdraw it!) */
    if (theWindow->contRgn && theWindow->updateRgn) {
        RgnHandle updateClip = Platform_NewRgn();
        if (updateClip) {
            Platform_IntersectRgn(theWindow->contRgn, theWindow->updateRgn, updateClip);
            if (theWindow->offscreenGWorld) {
                /* Check if current port's clipRgn is valid before calling SetClip */
                if (g_currentPort && g_currentPort->clipRgn) {
                    /* SetClip copies the region, so we can safely dispose after */
                    SetClip(updateClip);
                }
            } else {
                /* Platform_SetClipRgn copies the region data */
                Platform_SetClipRgn(&theWindow->port, updateClip);
            }
            /* SetClip and Platform_SetClipRgn copy the region contents. */
            Platform_DisposeRgn(updateClip);
        }
    } else if (theWindow->contRgn) {
        /* If no updateRgn, just use contRgn to prevent overdrawing chrome */
        if (theWindow->offscreenGWorld) {
            if (g_currentPort && g_currentPort->clipRgn) {
                SetClip(theWindow->contRgn);
            }
        } else {
            Platform_SetClipRgn(&theWindow->port, theWindow->contRgn);
        }
    }

    /* Erase update region to window background */
    if (theWindow->offscreenGWorld) {
        /* Get GWorld bounds and erase to background */
        PixMapHandle pmHandle = GetGWorldPixMap((GWorldPtr)theWindow->offscreenGWorld);
        if (pmHandle && *pmHandle) {
            Rect gwBounds = (*pmHandle)->bounds;

            /* CRITICAL: Manually fill GWorld buffer with white ARGB pixels
             * (EraseRect doesn't properly handle 32-bit ARGB) */
            PixMapPtr pm = *pmHandle;
            if (pm->pixelSize == 32 && pm->baseAddr) {
                void* pixels = pm->baseAddr;
                SInt16 height = gwBounds.bottom - gwBounds.top;
                SInt16 rowBytes = pm->rowBytes & 0x3FFF;

                /* Fill with opaque white (0xFFFFFFFF = ARGB white) - use memset for speed */
                size_t bytesToFill = (size_t)height * (size_t)rowBytes;
                wm_log_memfill("[GWorld] memset", pixels, bytesToFill);
                memset(pixels, 0xFF, bytesToFill);
            } else {
                /* Fall back to EraseRect for non-32-bit modes */
                EraseRect(&gwBounds);
            }
        }
    } else {
        /* Erase through QuickDraw so the clip applies: the port's clip is the
         * update region by now, and the clip engine adds the window's visible
         * region. This used to write white straight into the framebuffer over
         * the update region's bounding box, painting over any window in front
         * - a control panel opened over a zoomed Finder window went blank the
         * moment the Finder window repainted. */
        GrafPtr erasePort;
        GetPort(&erasePort);
        SetPort((GrafPtr)theWindow);
        EraseRect(&theWindow->port.portRect);
        SetPort(erasePort);
    }

    WM_DEBUG("BeginUpdate: Update session started");
    serial_puts("[MEM] BeginUpdate after setup window\n");
    MemoryManager_CheckSuspectBlock("after_BeginUpdate");
}

void EndUpdate(WindowPtr theWindow) {
    serial_puts("[MEM] EndUpdate enter\n");
    MemoryManager_CheckSuspectBlock("enter_EndUpdate");
    if (theWindow == NULL) {
        return;
    }

    WM_DEBUG("EndUpdate: Ending window update");
    MemoryManager_CheckSuspectBlock("post_debug_EndUpdate");

    /* If double-buffering with GWorld, copy offscreen buffer to screen */
    if (theWindow->offscreenGWorld) {
        WM_DEBUG("EndUpdate: Copying offscreen GWorld to screen");

        /* Get the PixMap from the GWorld */
        PixMapHandle gwPixMap = GetGWorldPixMap(theWindow->offscreenGWorld);
        if (gwPixMap && *gwPixMap) {
            /* Lock pixels for access */
            if (LockPixels(gwPixMap)) {
                /* Switch to window port for the blit */
                SetPort((GrafPtr)&theWindow->port);

                /* Copy the entire content area */
                /* Source: local coordinates from GWorld - use GWorld's bounds, not window portRect! */
                Rect srcRect = (*gwPixMap)->bounds;

                /* Destination: use portBits bounds directly (already in global screen coordinates) */
                Rect dstRect = theWindow->port.portBits.bounds;

                /* CRITICAL: Create a proper PixMap for the framebuffer destination
                 * The window's portBits is a BitMap, but the framebuffer is actually 32-bit ARGB.
                 * We need to create a temporary PixMap to describe it properly for CopyBits. */
                /* Clamp destination rectangle to visible framebuffer region and adjust source accordingly */
                Rect clippedDst = dstRect;
                if (clippedDst.left < 0) clippedDst.left = 0;
                if (clippedDst.top < 0) clippedDst.top = 0;
                if (clippedDst.right > (SInt16)fb_width) clippedDst.right = (SInt16)fb_width;
                if (clippedDst.bottom > (SInt16)fb_height) clippedDst.bottom = (SInt16)fb_height;

                /* Calculate adjustments between original and clipped rectangles */
                SInt16 deltaLeft = clippedDst.left - dstRect.left;
                SInt16 deltaTop = clippedDst.top - dstRect.top;
                SInt16 deltaRight = dstRect.right - clippedDst.right;
                SInt16 deltaBottom = dstRect.bottom - clippedDst.bottom;

                /* Apply adjustments to source rectangle to keep content aligned */
                srcRect.left += deltaLeft;
                srcRect.top += deltaTop;
                srcRect.right -= deltaRight;
                srcRect.bottom -= deltaBottom;

                Boolean canBlit = true;
                if (clippedDst.left >= clippedDst.right || clippedDst.top >= clippedDst.bottom ||
                    srcRect.left >= srcRect.right || srcRect.top >= srcRect.bottom) {
                    serial_logf(kLogModuleWindow, kLogLevelDebug,
                                "[COPYBITS] Skipped blit: clipped dst=(%d,%d,%d,%d) src=(%d,%d,%d,%d)\n",
                                clippedDst.left, clippedDst.top, clippedDst.right, clippedDst.bottom,
                                srcRect.left, srcRect.top, srcRect.right, srcRect.bottom);
                    canBlit = false;
                }

                if (canBlit) {
                    dstRect = clippedDst;

                    /*
                     * Blit only what is actually visible.
                     *
                     * This used to copy the whole offscreen buffer to the
                     * window's place on screen with no regard for what was
                     * stacked on top of it, so a window repainting itself
                     * painted straight over the windows in front. The Window
                     * Manager worked around that by refusing to service an
                     * update at all while anything covered any part of the
                     * window (WM_DeferUpdateIfObscured), which left partially
                     * covered windows blank until the cover moved away.
                     *
                     * visRgn is the window's content minus the structure
                     * region of every window in front of it, so copying it
                     * band by band puts back exactly the pixels this window
                     * owns and none that it does not.
                     */
                    /* Recompute rather than trust: the visible region is
                     * only as good as the last time something recalculated
                     * it, and this is the moment it has to be right. */
                    CalcVis(theWindow);

                    /* "No region" and "an empty region" are different
                     * answers and this used to treat them the same, copying
                     * the whole window for both. A window completely covered
                     * by another has an empty visible region, so it repainted
                     * itself over the window on top - which is exactly what
                     * happened when the Trash window opened at the same bounds
                     * as the Macintosh HD window and the one underneath drew
                     * straight over it. */
                    RgnHandle visible = theWindow->visRgn;
                    SInt16 bandCount;

                    /* Only what this update drew: BeginUpdate clipped the
                     * drawing to the update region but filled the whole
                     * buffer, so copying all of it blanked the rest of the
                     * window - a keystroke in a dialog's text field redrew
                     * the field and wiped the buttons. Inside Macintosh's
                     * BeginUpdate narrows the visible region the same way.
                     * A nested EndUpdate finds the region already cleared
                     * and copies nothing, the inner one having done it. */
                    AutoRgnHandle updated = WM_NewAutoRgn();
                    if (visible && *visible && updated.rgn &&
                        theWindow->updateRgn && *theWindow->updateRgn) {
                        SectRgn(visible, theWindow->updateRgn, updated.rgn);
                        visible = updated.rgn;
                    }

                    if (!visible || !*visible) {
                        visible = NULL;   /* nothing known - copy the lot */
                        bandCount = 1;
                    } else if (EmptyRgn(visible)) {
                        bandCount = 0;    /* wholly covered - copy nothing */
                    } else {
                        bandCount = WM_RegionRectCount(visible);
                    }

                    PixMap fbPixMap;
                    /* Offset framebuffer pointer to the window's top-left */
                    SInt16 dstTop = dstRect.top;
                    SInt16 dstLeft = dstRect.left;
                    uint8_t* fbBase = (uint8_t*)framebuffer + (size_t)dstTop * fb_pitch + (size_t)dstLeft * 4u;
                    fbPixMap.baseAddr = (Ptr)fbBase;
                    fbPixMap.rowBytes = (SInt16)(fb_pitch | 0x8000);  /* Set PixMap flag, preserve actual pitch */
                    fbPixMap.bounds = dstRect;
                    fbPixMap.pmVersion = 0;
                    fbPixMap.packType = 0;
                    fbPixMap.packSize = 0;
                    fbPixMap.hRes = 72 << 16;  /* 72 DPI in Fixed */
                    fbPixMap.vRes = 72 << 16;
                    fbPixMap.pixelType = 16;  /* Direct color */
                    fbPixMap.pixelSize = 32;  /* 32-bit ARGB */
                    fbPixMap.cmpCount = 3;    /* R, G, B */
                    fbPixMap.cmpSize = 8;     /* 8 bits per component */
                    fbPixMap.planeBytes = 0;
                    fbPixMap.pmTable = NULL;
                    fbPixMap.pmReserved = 0;

                    for (SInt16 band = 0; band < bandCount; band++) {
                        Rect bandDst = dstRect;

                        if (visible) {
                            Rect visRect;
                            WM_RegionGetRect(visible, band, &visRect);
                            if (!SectRect(&dstRect, &visRect, &bandDst)) {
                                continue;   /* this band is entirely covered */
                            }
                        }

                        /* Take the matching piece of the offscreen buffer:
                         * source and destination differ only by the window's
                         * position on screen. */
                        Rect bandSrc;
                        bandSrc.left   = srcRect.left   + (bandDst.left   - dstRect.left);
                        bandSrc.top    = srcRect.top    + (bandDst.top    - dstRect.top);
                        bandSrc.right  = srcRect.right  - (dstRect.right  - bandDst.right);
                        bandSrc.bottom = srcRect.bottom - (dstRect.bottom - bandDst.bottom);

                        if (bandSrc.left >= bandSrc.right ||
                            bandSrc.top  >= bandSrc.bottom) {
                            continue;
                        }

                        /* The framebuffer PixMap is described relative to the
                         * pixels it points at, so it moves with the band. */
                        fbPixMap.baseAddr = (Ptr)((uint8_t*)framebuffer +
                                                  (size_t)bandDst.top * fb_pitch +
                                                  (size_t)bandDst.left * 4u);
                        fbPixMap.bounds = bandDst;

                        serial_logf(kLogModuleWindow, kLogLevelDebug,
                                   "[COPYBITS] src=(%d,%d,%d,%d) dst=(%d,%d,%d,%d)\n",
                                   bandSrc.left, bandSrc.top, bandSrc.right, bandSrc.bottom,
                                   bandDst.left, bandDst.top, bandDst.right, bandDst.bottom);

                        Pointer_Shield(bandDst.left, bandDst.top, bandDst.right, bandDst.bottom);
                        CopyBits((BitMap*)(*gwPixMap), (BitMap*)&fbPixMap,
                                &bandSrc, &bandDst, srcCopy, NULL);
                    }

                    serial_logf(kLogModuleWindow, kLogLevelDebug, "[COPYBITS] Done\n");
                    WM_DisposeAutoRgn(&updated);
                }

                UnlockPixels(gwPixMap);

                /* Restore original portBits pointers */
                theWindow->port.portBits.baseAddr = (Ptr)framebuffer;
                /* CRITICAL: Set PixMap flag (bit 15) to indicate this is a 32-bit PixMap, not 1-bit BitMap */
                theWindow->port.portBits.rowBytes = (fb_width * 4) | 0x8000;

                if (canBlit) {
                    WM_DEBUG("EndUpdate: Offscreen buffer copied to screen");
                }
            }
        }
    }

    /* Clear the update region */
    if (theWindow->updateRgn) {
        Platform_SetEmptyRgn(theWindow->updateRgn);
        MemoryManager_CheckSuspectBlock("after_SetEmptyRgn");
    }

    /* End platform drawing session */
    Platform_EndWindowDraw(theWindow);
    MemoryManager_CheckSuspectBlock("after_EndWindowDraw");

    /* CRITICAL: Restore clipping to content region (not visRgn!)
     * to prevent content from overdrawing chrome */
    if (theWindow->contRgn) {
        Platform_SetClipRgn(&theWindow->port, theWindow->contRgn);
        MemoryManager_CheckSuspectBlock("after_SetClipRgn");
    }

    /* Restore previous port */
    GrafPtr savedPort = Platform_GetUpdatePort(theWindow);
    if (savedPort) {
        Platform_SetCurrentPort(savedPort);
        MemoryManager_CheckSuspectBlock("after_SetCurrentPort");
    }

    WM_DEBUG("EndUpdate: Update session ended");
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
    WindowPtr window = (WindowPtr)(uintptr_t)(theEvent->message);
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

/* ============================================================================
 * Platform Abstraction Helpers
 * ============================================================================ */

static Boolean WM_IsMouseDown(void) {
    return Button();
}

static GrafPtr WM_GetCurrentPort(void) {
    GrafPtr currentPort;
    GetPort(&currentPort);
    return currentPort;
}

static Boolean WM_EmptyRgn(RgnHandle rgn) {
    if (rgn == NULL) {
        return true;
    }
    return EmptyRgn(rgn);
}

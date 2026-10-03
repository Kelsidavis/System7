#include "SystemInternal.h"
#include "System71StdLib.h"
#include "Platform/include/io.h"
#include <stdio.h>

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WindowRegions.h"
#include "QuickDraw/QuickDraw.h"
#include "ControlManager/ControlTypes.h"
#include "SystemTheme.h"
#include "WindowManager/WMLogging.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "Platform/Framebuffer.h"

/* Color constants */
#define blackColor 33

/* Forward declarations */
static void DumpWindowList(const char* context);
void CheckWindowsNeedingUpdate(void);

/*-----------------------------------------------------------------------*/
/* Window Display Functions                                             */
/*-----------------------------------------------------------------------*/

/* Check windows for update events (called by GetNextEvent) */
void CheckWindowsNeedingUpdate(void) {
    static int call_count = 0;
    call_count++;

    /* Walk all visible windows and post update events for windows with non-empty updateRgn */
    WindowPtr window = FrontWindow();

    if (call_count <= 10 || (call_count % 500) == 0) {
        WM_LOG_TRACE("CheckWindowsNeedingUpdate: #%d, frontWindow=0x%08x\n", call_count, (unsigned int)window);
    }

    /* windowCount is only ever read inside WM_LOG_TRACE, which expands to
     * nothing in this build; (void) it so the compiler does not flag it. */
    int windowCount = 0;
    (void)windowCount;
    while (window) {
        windowCount++;
        Boolean hasUpdateRgn = (window->updateRgn != NULL);
        Boolean isEmpty = hasUpdateRgn ? EmptyRgn(window->updateRgn) : true;

        if (call_count <= 10 || (call_count % 500) == 0) {
            WM_LOG_TRACE("CheckWindowsNeedingUpdate:   Window %d: 0x%08x, visible=%d, updateRgn=0x%08x, empty=%d\n",
                         windowCount, (unsigned int)window, window->visible, (unsigned int)window->updateRgn, isEmpty);
            if (hasUpdateRgn) {
                Region* rgn = *(window->updateRgn);
                WM_LOG_TRACE("CheckWindowsNeedingUpdate:     updateRgn bbox=(%d,%d,%d,%d)\n",
                             rgn->rgnBBox.left, rgn->rgnBBox.top, rgn->rgnBBox.right, rgn->rgnBBox.bottom);
                (void)rgn; /* Used only in WM_LOG_TRACE (debug builds) */
            }
        }
        (void)isEmpty; /* Used only in WM_LOG_TRACE (debug builds) */

        if (window->visible && window->updateRgn && !EmptyRgn(window->updateRgn)) {
            /* Deliberately does NOT post an event here - see
             * WM_FindWindowNeedingUpdate() below and its use in GetNextEvent. */
            WM_LOG_TRACE("CheckWindowsNeedingUpdate: window %p needs update\n", (void*)window);
        }
        window = window->nextWindow;
    }
}

/*
 * WM_FindWindowNeedingUpdate - front-most visible window with a dirty updateRgn.
 *
 * Update events are SYNTHESISED on demand, not queued.
 *
 * This routine used to PostEvent(updateEvt) for every dirty window on every
 * GetNextEvent call. GetNextEvent runs continuously from the main loop and
 * PostEvent does not deduplicate, so a window whose updateRgn stayed dirty
 * filled the 32-entry event queue within microseconds. Once full, PostEvent
 * rejects everything with queueFull - including mouse and keyboard events. The
 * symptom was "update events aren't flowing", which is what the direct
 * FolderWindow_Draw calls scattered around the Window Manager were added to work
 * around, but the damage was wider than that.
 *
 * Classic Mac OS never queues update events either: GetNextEvent reports one
 * when a window has a non-empty updateRgn, and BeginUpdate/EndUpdate clears it.
 * Generating them on demand cannot flood the queue and cannot go stale.
 */
static void WM_AccumulateUpdateRgn(WindowPtr window, RgnHandle rgn);

/*
 * A window used to be skipped for update while anything overlapped it, both
 * here and again in the event dispatcher - one policy written twice, and
 * neither copy could be more precise than a rectangle intersection, so a
 * single pixel of overlap left a window blank until the cover moved away.
 * It stood in for real regions. EndUpdate
 * copies a window's offscreen buffer to the screen band by band through its
 * visible region, so repainting an overlapped window can only put back pixels
 * that window owns, and there is nothing left to defer.
 */
WindowPtr WM_FindWindowNeedingUpdate(void) {
    WindowPtr window = FrontWindow();
    int guard = 0;
    while (window && guard++ < 64) {
        if (window->visible && window->updateRgn && !EmptyRgn(window->updateRgn)) {
            return window;
        }
        window = window->nextWindow;
    }
    return NULL;
}

/*
 * WM_AccumulateUpdateRgn - add a region to a window's update region.
 *
 * Unlike InvalRgn/InvalRect this takes the window explicitly rather than
 * inferring it from the current port, because callers that erase a window
 * (PaintOne) do so from the Window Manager port, not the window's own.
 *
 * rgn is in global coordinates, matching contRgn and updateRgn.
 */
static void WM_AccumulateUpdateRgn(WindowPtr window, RgnHandle rgn) {
    if (!window || !rgn || !*rgn) return;

    if (!window->updateRgn) {
        window->updateRgn = NewRgn();
        if (!window->updateRgn) return;   /* out of memory - drop the update */
    }

    UnionRgn(window->updateRgn, rgn, window->updateRgn);
}


/* ============================================================================
 * Chrome painting
 *
 * A window's frame, title bar and controls are painted straight into the
 * framebuffer rather than through the window's offscreen buffer, so none of it
 * passed through the clipping that EndUpdate applies to window content. A
 * window repainting its chrome - which happens whenever it is activated or
 * deactivated - drew its frame and title bar over whatever was stacked on top
 * of it. Opening a dialog over a document window deactivated the document,
 * and the document's title bar came back over the dialog.
 *
 * Every chrome pixel now goes through one gate that knows which pixels this
 * window actually owns. The region is the window's structure region minus the
 * structure region of every window in front of it - the same relationship
 * CalcVis computes for content.
 * ============================================================================ */

static RgnHandle gChromeClipRgn = NULL;

/* Compute what is visible of a window's frame. The caller owns the region. */
static void WM_BeginChromeClip(WindowPtr window, AutoRgnHandle* holder) {
    *holder = WM_NewAutoRgn();
    if (!holder->rgn || !window->strucRgn) {
        gChromeClipRgn = NULL;
        return;
    }

    CopyRgn(window->strucRgn, holder->rgn);

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr front = wmState ? wmState->windowList : NULL;
    int guard = 0;
    while (front && front != window && guard++ < 64) {
        if (front->visible && front->strucRgn && *(front->strucRgn)) {
            DiffRgn(holder->rgn, front->strucRgn, holder->rgn);
        }
        front = front->nextWindow;
    }

    gChromeClipRgn = holder->rgn;
}

static void WM_EndChromeClip(AutoRgnHandle* holder) {
    gChromeClipRgn = NULL;
    WM_DisposeAutoRgn(holder);
}

/* Paint one pixel of window chrome, if this window owns it. */
static void WM_ChromePixel(int x, int y, uint32_t colour) {
    if (!framebuffer) return;
    if (x < 0 || y < 0 || x >= (int)fb_width || y >= (int)fb_height) return;

    if (gChromeClipRgn) {
        Point pt;
        pt.h = (short)x;
        pt.v = (short)y;
        if (!PtInRgn(pt, gChromeClipRgn)) return;
    }

    Pointer_Shield(x, y, x + 1, y + 1);
    ((uint32_t*)framebuffer)[y * (int)(fb_pitch / 4) + x] = colour;
}


/* Internal helper to draw window frame */
static void DrawWindowFrame(WindowPtr window);

/* Internal helper to draw window controls */
static void DrawWindowControls(WindowPtr window);

void PaintOne(WindowPtr window, RgnHandle clobberedRgn) {
    WM_LOG_TRACE("PaintOne: ENTRY, window=%p, visible=%d\n", window, window ? window->visible : -1);

    if (!window || !window->visible) {
        WM_LOG_TRACE("PaintOne: Early return\n");
        return;
    }

    WM_DEBUG("PaintOne: Painting window");
    WM_LOG_TRACE("PaintOne: About to GetPort/SetPort\n");

    /* Save current port */
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* CRITICAL: Fill window with white in WMgrPort using GLOBAL strucRgn coordinates
     * This ensures we always fill the correct screen position regardless of port state */
    SetPort(wmgrPort);
    WM_LOG_TRACE("PaintOne: Switched to WMgr port for backfill\n");

    /* Clip the Window Manager port to the part of this window no window in
     * front covers (Inside Macintosh: Toolbox Essentials, 4-127). The clip
     * used to be opened to the whole plane, so painting a window that was
     * not in front - ShowWindow, PaintBehind, a resize - filled its content
     * white and drew its frame straight over the windows in front of it. */
    AutoRgnHandle chromeClip = WM_NewAutoRgn();
    if (chromeClip.rgn && window->strucRgn && *window->strucRgn) {
        CopyRgn(window->strucRgn, chromeClip.rgn);
        WindowManagerState* wmState = GetWindowManagerState();
        for (WindowPtr w = wmState ? wmState->windowList : NULL; w && w != window; w = w->nextWindow) {
            if (w->visible && w->strucRgn && *w->strucRgn) {
                DiffRgn(chromeClip.rgn, w->strucRgn, chromeClip.rgn);
            }
        }
        SetClip(chromeClip.rgn);
    }
    WM_DisposeAutoRgn(&chromeClip);

    /* CRITICAL: Fill content region with white background BEFORE drawing chrome
     * This prevents garbage/dotted patterns from appearing in the window content area
     * The application will draw over this white background when handling update events.
     *
     * Every window: this used to skip windows whose refCon was 0, taken for
     * a "desktop background window" that does not exist. A refCon is the
     * application's own; control panels, dialogs, SimpleText and the rest
     * use 0, so their uncovered content was neither cleared nor redrawn, and
     * a control panel brought forward kept the other window's picture. */
    static char dbgbuf[256];
    static int fill_log = 0;

    if (window->contRgn && *(window->contRgn)) {
        Region* rgn = *(window->contRgn);
        if (fill_log < 10) {
            snprintf(dbgbuf, sizeof(dbgbuf), "[PAINTONE] window=%p refCon=0x%08x fill=%d strucRgn=(%d,%d,%d,%d) contRgn=(%d,%d,%d,%d)\n",
                   window, (unsigned int)window->refCon, (window->refCon != 0),
                   (*(window->strucRgn))->rgnBBox.left, (*(window->strucRgn))->rgnBBox.top,
                   (*(window->strucRgn))->rgnBBox.right, (*(window->strucRgn))->rgnBBox.bottom,
                   rgn->rgnBBox.left, rgn->rgnBBox.top,
                   rgn->rgnBBox.right, rgn->rgnBBox.bottom);
            serial_puts(dbgbuf);
            fill_log++;
        }

        {
            if (window->refCon == 0x4449534b && window->contRgn && *(window->contRgn)) {
                char filldbg[256];
                /* Use pointer to avoid struct assignment on ARM64 */
                Rect* fillBBoxPtr = &((*(window->contRgn))->rgnBBox);
                snprintf(filldbg, sizeof(filldbg), "[FILLRGN] DISK: About to fill region bbox=(%d,%d,%d,%d)\n",
                        fillBBoxPtr->left, fillBBoxPtr->top, fillBBoxPtr->right, fillBBoxPtr->bottom);
                serial_puts(filldbg);

                /* Log the current port state when filling */
                GrafPtr currentPort = qd.thePort;
                if (currentPort) {
                    uint8_t* fbStart = (uint8_t*)framebuffer;
                    uint8_t* baseAddr = (uint8_t*)currentPort->portBits.baseAddr;
                    int offset = baseAddr - fbStart;
                    snprintf(dbgbuf, sizeof(dbgbuf), "[FILLRGN] Port state: baseAddr=%p (offset=%d from fb), portRect=(%d,%d,%d,%d)\n",
                            currentPort->portBits.baseAddr, offset,
                            currentPort->portRect.left, currentPort->portRect.top,
                            currentPort->portRect.right, currentPort->portRect.bottom);
                    serial_puts(dbgbuf);
                }
            }

            /* OPTIMIZATION: Use dirty region to only fill/erase necessary areas
             * If clobberedRgn is provided, intersect with window content region to minimize fill operations.
             * This reduces framebuffer writes for incremental updates. */
            RgnHandle erasedRgn = NULL;
            AutoRgnHandle dirtyContent = { NULL, false };

            if (clobberedRgn && *clobberedRgn) {
                /* Calculate intersection of clobbered region with content region */
                dirtyContent = WM_NewAutoRgn();
                if (dirtyContent.rgn) {
                    SectRgn(clobberedRgn, window->contRgn, dirtyContent.rgn);
                    if (dirtyContent.rgn && *(dirtyContent.rgn)) {
                        FillRgn(dirtyContent.rgn, &qd.white);
                        erasedRgn = dirtyContent.rgn;
                    }
                } else {
                    /* Fallback: if dirty region calculation fails, fill entire content region */
                    FillRgn(window->contRgn, &qd.white);
                    erasedRgn = window->contRgn;
                }
            } else {
                /* No dirty region provided, fill entire content region */
                FillRgn(window->contRgn, &qd.white);
                erasedRgn = window->contRgn;
            }

            /* The content we just erased is the application's to redraw, so it
             * has to go into the window's update region. Without this the erase
             * is silently destructive and whether content survives depends on
             * pure ordering: if PaintOne runs before the app's draw the content
             * appears, if it runs after, the window is left permanently blank.
             *
             * Both orderings occur in practice. Booting with a USB tablet
             * attached paints the Finder window before PaintOne and looks fine;
             * booting with only a PS/2 mouse and no input paints it after, and
             * the Macintosh HD window stays blank forever, which
             * is what users see on real hardware.
             *
             * Note this over-invalidates: with regions still being rectangles
             * the union is a bounding box, so we may repaint more
             * than was erased. That is safe - it costs a redraw, never content.
             * EndUpdate empties updateRgn, so this cannot loop.
             *
             * The damage is always recorded here; whether it is safe to repaint
             * yet is decided at service time in WM_FindWindowNeedingUpdate,
             * which defers windows that something is stacked on top of. */
            WM_AccumulateUpdateRgn(window, erasedRgn);
            WM_DisposeAutoRgn(&dirtyContent);
        }
    }

    /* NOW draw chrome on top of backfill */
    WM_LOG_TRACE("PaintOne: Drawing window chrome\n");
    WM_LOG_TRACE("PaintOne: About to call DrawWindowFrame, window=%p\n", window);
    DrawWindowFrame(window);
    WM_LOG_TRACE("PaintOne: DrawWindowFrame returned\n");
    DrawWindowControls(window);
    WM_LOG_TRACE("PaintOne: DrawWindowControls returned\n");

    /* Window Manager draws chrome only - content is application's job */
    /* Application must draw content via BeginUpdate/EndUpdate in update event handler */

    SetPort(savePort);
    WM_LOG_TRACE("PaintOne: EXIT\n");
}

void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn) {
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WM_LOG_TRACE("[PaintBehind] Starting, startWindow=%p\n", startWindow);
    DumpWindowList("PaintBehind - START");

    /* Build list of windows in reverse order (back to front) */
    #define MAX_WINDOWS 32
    WindowPtr windows[MAX_WINDOWS];
    int count = 0;

    /* DEFENSIVE: Track visited windows to detect circular lists */
    WindowPtr visited[MAX_WINDOWS];
    int visitedCount = 0;

    /* Find start position */
    WindowPtr window = startWindow;
    if (!window) {
        window = wmState->windowList;
    }

    /* Collect visible windows */
    while (window && count < MAX_WINDOWS) {
        /* DEFENSIVE: Check for circular list by detecting revisited windows */
        for (int i = 0; i < visitedCount; i++) {
            if (visited[i] == window) {
                WM_LOG_TRACE("[PaintBehind] ERROR: Circular list detected at window %p! Breaking loop.\n", window);
                goto paint_windows;  /* Break out and paint what we have */
            }
        }

        if (visitedCount < MAX_WINDOWS) {
            visited[visitedCount++] = window;
        }

        if (window->visible) {
            windows[count++] = window;
        }
        window = window->nextWindow;
    }

paint_windows:

    /* Paint each window COMPLETELY (chrome + content) from BACK to FRONT
     * This ensures background windows never overdraw foreground windows */

    for (int i = count - 1; i >= 0; i--) {
        WindowPtr w = windows[i];

        /* Phase 1: Paint chrome */
        WM_LOG_TRACE("[PaintBehind] Painting chrome for window %p (index %d of %d)\n", w, i, count);
        {
            char logBuf[160];
            snprintf(logBuf, sizeof(logBuf), "[PaintBehind] Painting window=%p refCon=%p index=%d\n",
                     w, (void*)(intptr_t)w->refCon, i);
            serial_puts(logBuf);
        }
        PaintOne(w, clobberedRgn);

        /* Phase 2: Paint content with proper clipping */
        if (w->contRgn) {
            WM_LOG_TRACE("[PaintBehind] Painting content for window %p\n", w);
            GrafPtr savePort;
            GetPort(&savePort);
            SetPort((GrafPtr)w);

            /* Keep clipRgn separate from visRgn because QuickDraw mutates the
             * clip while calculating visibility and drawing content.
             */
            CalcVis(w);
            if (w->visRgn && *w->visRgn && w->port.clipRgn && *w->port.clipRgn) {
                CopyRgn(w->visRgn, w->port.clipRgn);
            }

            /* Let the window's update event repaint content after chrome. */
            WM_InvalGlobalRgn(w, w->contRgn);

            SetPort(savePort);

            /* Chrome is painted first, then content is constrained by clipRgn. */
        }
    }

    WM_LOG_TRACE("[PaintBehind] Complete, painted %d windows back-to-front\n", count);
}

void CalcVis(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("CalcVis: Calculating visible region");

    /* Start with the window's content region */
    if (window->contRgn && window->visRgn) {
        CopyRgn(window->contRgn, window->visRgn);

        /* Subtract regions of windows in front */
        WindowManagerState* wmState = GetWindowManagerState();
        if (!wmState) return;

        WindowPtr frontWindow = wmState->windowList;
        while (frontWindow && frontWindow != window) {
            if (frontWindow->visible && frontWindow->strucRgn) {
                /* Subtract the structure region of windows in front to prevent overdraw */
                DiffRgn(window->visRgn, frontWindow->strucRgn, window->visRgn);
            }
            frontWindow = frontWindow->nextWindow;
        }
    }
}

/*
 * For QuickDraw: if port is a visible window, the part of its content no
 * window in front covers, global, in out. Worked out afresh each time, so
 * it is never stale. False for any other port.
 */
Boolean WM_PortVisibleRgn(GrafPtr port, RgnHandle out) {
    WindowManagerState* wm = GetWindowManagerState();
    if (!port || !out || !wm) return false;
    for (WindowPtr w = wm->windowList; w; w = w->nextWindow) {
        if ((GrafPtr)w != port) continue;
        /* An invisible window shows nothing (Inside Macintosh: Toolbox
         * Essentials, 4-15). Answered as "not a window", its drawing went
         * unclipped onto the screen: a dialog filled in before being shown
         * left its edit field where it had been created. */
        if (!w->visible) {
            SetEmptyRgn(out);
            return true;
        }
        if (!w->visRgn) return false;
        CalcVis(w);
        CopyRgn(w->visRgn, out);
        return true;
    }
    return false;
}

void CalcVisBehind(WindowPtr startWindow, RgnHandle clobberedRgn) {
    (void)clobberedRgn;
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WM_DEBUG("CalcVisBehind: Recalculating visible regions");

    /* Find start position in window list */
    WindowPtr window = startWindow;
    if (!window) {
        window = wmState->windowList;
    }

    /* Recalculate visible regions for all windows */
    while (window) {
        CalcVis(window);
        window = window->nextWindow;
    }
}

void ClipAbove(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("ClipAbove: Setting clip region");

    /* Create a region that excludes all windows above this one */
    AutoRgnHandle clipRgn = WM_NewAutoRgn();
    if (clipRgn.rgn) {
        /* Start with full screen */
        SetRectRgn(clipRgn.rgn, qd.screenBits.bounds.left, qd.screenBits.bounds.top,
                   qd.screenBits.bounds.right, qd.screenBits.bounds.bottom);

        /* Subtract regions of windows in front */
        WindowPtr frontWindow = FrontWindow();
        while (frontWindow && frontWindow != window) {
            if (frontWindow->visible && frontWindow->strucRgn && *frontWindow->strucRgn) {
                DiffRgn(clipRgn.rgn, frontWindow->strucRgn, clipRgn.rgn);
            }
            frontWindow = frontWindow->nextWindow;
        }

        SetClip(clipRgn.rgn);
    }
    WM_DisposeAutoRgn(&clipRgn);
}

void SaveOld(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("SaveOld: Saving window bits");

    /* Save the bits behind the window */
    /* This would typically copy screen bits to an offscreen buffer */
    /* For now, simplified implementation */
}

void DrawNew(WindowPtr window, Boolean update) {
    if (!window) return;
    WM_LOG_TRACE("DrawNew: ENTRY, window=%p\n", window);

    WM_DEBUG("DrawNew: Drawing window");

    /* Save current port */
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* Draw chrome in WMgr port */
    SetPort(wmgrPort);
    WM_LOG_TRACE("DrawNew: Drawing frame\n");
    DrawWindowFrame(window);
    DrawWindowControls(window);

    /* Switch to window port for content */
    SetPort((GrafPtr)window);

    if (update && window->updateRgn) {
        /* Only draw the update region */
        SetClip(window->updateRgn);
    }

    /* Content backfill is handled by application (Finder) draw code, not here */
    WM_LOG_TRACE("DrawNew: Content backfill handled by application draw code\n");

    SetPort(savePort);
    WM_LOG_TRACE("DrawNew: EXIT\n");
}

static void DrawWindowFrame_Unclipped(WindowPtr window);

/*
 * WM_RedrawWindowChrome - repaint a window's frame, title bar and controls.
 *
 * The title is part of the chrome, and chrome is painted by its own path
 * rather than through the content update region. SetWTitle used to call
 * InvalRect and expect that to bring the new title up, which it never could:
 * an update event repaints content, and the rectangle it invalidated was
 * computed from portRect and so was the top of the content area rather than
 * the title bar at all. Renaming a document left the old name on screen.
 */
void WM_RedrawWindowChrome(WindowPtr window)
{
    if (!window || !window->visible) return;
    DrawWindowFrame(window);
    DrawWindowControls(window);
}

/* Paint this window's chrome, limited to the pixels it actually owns. */
static void DrawWindowFrame(WindowPtr window) {
    AutoRgnHandle chromeClip;

    if (!window || !window->visible) return;

    WM_BeginChromeClip(window, &chromeClip);
    DrawWindowFrame_Unclipped(window);
    WM_EndChromeClip(&chromeClip);
}

static void DrawWindowFrame_Unclipped(WindowPtr window) {
    if (!window) {
        return;
    }

    if (!window->visible) {
        return;
    }

    if (!window->strucRgn) {
        return;
    }

    if (!*window->strucRgn) {
        return;
    }

    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);
    SetPort(wmgrPort);

    /* Set up pen for drawing black frames */
    static const Pattern blackPat = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    PenNormal();  /* Reset pen to normal state */
    PenPat(&blackPat);  /* Use black pattern for frames */
    PenSize(1, 1);  /* 1-pixel pen */

    /* Get window's global bounds from structure region - use explicit field copy to avoid struct assignment on ARM64 */
    Rect frame;
    Rect* srcRect = &((*window->strucRgn)->rgnBBox);
    frame.top = srcRect->top;
    frame.left = srcRect->left;
    frame.bottom = srcRect->bottom;
    frame.right = srcRect->right;

    /* Draw the frame outline through the same gate as the rest of the chrome.
     * This was a QuickDraw FrameRect while everything around it wrote pixels
     * directly, so the outline obeyed one set of rules and the title bar and
     * highlights obeyed another - and neither obeyed the windows in front. */
    {
        uint32_t frameBlack = 0xFF000000;
        for (int x = frame.left; x < frame.right; x++) {
            WM_ChromePixel(x, frame.top, frameBlack);
            WM_ChromePixel(x, frame.bottom - 1, frameBlack);
        }
        for (int y = frame.top; y < frame.bottom; y++) {
            WM_ChromePixel(frame.left, y, frameBlack);
            WM_ChromePixel(frame.right - 1, y, frameBlack);
        }
    }
    /* Add 3D black highlights for depth effect */
    if (framebuffer) {
        uint32_t black = 0xFF000000;

        /* Right side highlight: 2px wide, starting 1px down from top and extending to bottom */
        int highlightStartY = frame.top + 1;
        for (int y = highlightStartY; y < frame.bottom - 1 && y < (int)fb_height; y++) {
            if (y >= 0) {
                /* Draw 2 pixels on the right side (inside the frame) */
                for (int dx = 1; dx <= 2; dx++) {
                    int x = frame.right - 1 - dx;
                    if (x >= 0 && x < (int)fb_width) {
                        WM_ChromePixel(x, y, black);
                    }
                }
            }
        }

        /* Bottom highlight: 2px thick */
        for (int dy = 1; dy <= 2; dy++) {
            int y = frame.bottom - 1 - dy;
            if (y >= 0 && y < (int)fb_height) {
                for (int x = frame.left + 1; x < frame.right - 3 && x < (int)fb_width; x++) {
                    if (x >= 0) {
                        WM_ChromePixel(x, y, black);
                    }
                }
            }
        }
    }

    /* Draw title bar BEFORE filling content area */

    /* A title bar by the window's kind, not by whether it has a title: a
     * document window with an empty title still has one, a dBoxProc dialog
     * with a title does not. */
    if (WM_WindowHasTitleBar(window)) {
        /* Title bar background should be INSIDE the frame, not overlap it */
        Rect titleBar;
        titleBar.left = frame.left + 1;    /* Inset from left frame edge */
        titleBar.top = frame.top + 1;      /* Inset from top frame edge */
        titleBar.right = frame.right - 2;  /* Inset 2px from right to not overlap frame */
        titleBar.bottom = frame.top + 20;  /* Extends to separator line */

        /* Fill title bar background */
        if (window->hilited) {
            /* Active window: solid light grey background with darker horizontal stripes */
            if (framebuffer) {
                uint32_t lightGrey = 0xFFE8E8E8;  /* Solid lighter grey RGB(232,232,232) */
                uint32_t darkGrey = 0xFF808080;   /* Solid darker grey RGB(128,128,128) for stripes */

                /* Fill entire title bar with light grey */
                for (int y = titleBar.top; y < titleBar.bottom && y < (int)fb_height; y++) {
                    if (y >= 0) {
                        for (int x = titleBar.left; x < titleBar.right && x < (int)fb_width; x++) {
                            if (x >= 0) {
                                WM_ChromePixel(x, y, lightGrey);
                            }
                        }
                    }
                }

                /* Draw 6 evenly spaced darker horizontal stripes (every 3 pixels starting at offset 3) */
                int stripePositions[6] = {3, 6, 9, 12, 15, 18};
                for (int i = 0; i < 6; i++) {
                    int y = titleBar.top + stripePositions[i];
                    if (y >= 0 && y < (int)fb_height) {
                        for (int x = titleBar.left; x < titleBar.right && x < (int)fb_width; x++) {
                            if (x >= 0) {
                                WM_ChromePixel(x, y, darkGrey);
                            }
                        }
                    }
                }

                /* Draw 1px themed highlight border inside title bar */
                SystemTheme* theme = GetSystemTheme();
                RGBColor highlight = theme->highlightColor;
                /* Convert 16-bit Mac OS color to 8-bit framebuffer RGB */
                uint32_t highlightColor = 0xFF000000 | ((highlight.red >> 8) << 16) | ((highlight.green >> 8) << 8) | (highlight.blue >> 8);

                /* Top border - 1px inside */
                int y = titleBar.top;
                if (y >= 0 && y < (int)fb_height) {
                    for (int x = titleBar.left; x < titleBar.right && x < (int)fb_width; x++) {
                        if (x >= 0) WM_ChromePixel(x, y, highlightColor);
                    }
                }

                /* Bottom border - 1px inside (at separator line) */
                y = titleBar.bottom - 1;
                if (y >= 0 && y < (int)fb_height) {
                    for (int x = titleBar.left; x < titleBar.right && x < (int)fb_width; x++) {
                        if (x >= 0) WM_ChromePixel(x, y, highlightColor);
                    }
                }

                /* Left border - 1px inside */
                int x = titleBar.left;
                if (x >= 0 && x < (int)fb_width) {
                    for (y = titleBar.top; y < titleBar.bottom && y < (int)fb_height; y++) {
                        if (y >= 0) WM_ChromePixel(x, y, highlightColor);
                    }
                }

                /* Right border - 1px inside */
                x = titleBar.right - 1;
                if (x >= 0 && x < (int)fb_width) {
                    for (y = titleBar.top; y < titleBar.bottom && y < (int)fb_height; y++) {
                        if (y >= 0) WM_ChromePixel(x, y, highlightColor);
                    }
                }
            }
        } else {
            /* Inactive window: white background */
            EraseRect(&titleBar);
        }

        /* Draw System 7 close box - 14x14 at left side
         * Design: Black outline (left/top only for 3D), 1px theme highlight inside, grey fill.
         * Only for a window made with one (goAwayFlag), and only while it is
         * active, as System 7 shows it; it was drawn on every window. */
        if (framebuffer && WM_WindowHasCloseBox(window) && window->hilited) {
            /* Geometry comes from the platform rect so hit testing, press
             * highlighting and this paint cannot drift apart. */
            Rect closeBoxRect;
            Platform_GetWindowCloseBoxRect(window, &closeBoxRect);
            int boxLeft = closeBoxRect.left;
            int boxTop = closeBoxRect.top;
            int boxSize = closeBoxRect.right - closeBoxRect.left;

            uint32_t black = 0xFF000000;
            uint32_t grey = 0xFF808080;  /* Same grey as title bar stripes */

            /* Get theme highlight color if window is active */
            uint32_t highlightColor = grey;
            if (window->hilited) {
                SystemTheme* theme = GetSystemTheme();
                RGBColor highlight = theme->highlightColor;
                highlightColor = 0xFF000000 | ((highlight.red >> 8) << 16) | ((highlight.green >> 8) << 8) | (highlight.blue >> 8);
            }

            /* Draw black outline on LEFT and TOP only (3D effect) */
            /* Top edge */
            for (int x = boxLeft; x < boxLeft + boxSize - 1 && x < (int)fb_width; x++) {
                if (x >= 0 && boxTop >= 0 && boxTop < (int)fb_height) {
                    WM_ChromePixel(x, boxTop, black);
                }
            }
            /* Left edge */
            for (int y = boxTop; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                if (y >= 0 && boxLeft >= 0 && boxLeft < (int)fb_width) {
                    WM_ChromePixel(boxLeft, y, black);
                }
            }

            /* Draw 1px themed highlight border (complete box around grey) */
            /* Top highlight line */
            int y = boxTop + 1;
            if (y >= 0 && y < (int)fb_height) {
                for (int x = boxLeft + 1; x < boxLeft + boxSize - 1 && x < (int)fb_width; x++) {
                    if (x >= 0) WM_ChromePixel(x, y, highlightColor);
                }
            }
            /* Left highlight line */
            int x = boxLeft + 1;
            if (x >= 0 && x < (int)fb_width) {
                for (y = boxTop + 2; y < boxTop + boxSize - 2 && y < (int)fb_height; y++) {
                    if (y >= 0) WM_ChromePixel(x, y, highlightColor);
                }
            }
            /* Right highlight line */
            x = boxLeft + boxSize - 2;
            if (x >= 0 && x < (int)fb_width) {
                for (y = boxTop + 1; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                    if (y >= 0) WM_ChromePixel(x, y, highlightColor);
                }
            }
            /* Bottom highlight line */
            y = boxTop + boxSize - 2;
            if (y >= 0 && y < (int)fb_height) {
                for (x = boxLeft + 1; x < boxLeft + boxSize - 1 && x < (int)fb_width; x++) {
                    if (x >= 0) WM_ChromePixel(x, y, highlightColor);
                }
            }

            /* Fill interior with solid grey (reduced by 1px on bottom and right for shadow) */
            for (y = boxTop + 2; y < boxTop + boxSize - 3 && y < (int)fb_height; y++) {
                if (y >= 0) {
                    for (x = boxLeft + 2; x < boxLeft + boxSize - 3 && x < (int)fb_width; x++) {
                        if (x >= 0) WM_ChromePixel(x, y, grey);
                    }
                }
            }

            /* Draw black 3D shadow on bottom and right edges of grey */
            /* Bottom shadow */
            y = boxTop + boxSize - 3;
            if (y >= 0 && y < (int)fb_height) {
                for (x = boxLeft + 2; x < boxLeft + boxSize - 2 && x < (int)fb_width; x++) {
                    if (x >= 0) WM_ChromePixel(x, y, black);
                }
            }
            /* Right shadow */
            x = boxLeft + boxSize - 3;
            if (x >= 0 && x < (int)fb_width) {
                for (y = boxTop + 2; y < boxTop + boxSize - 3 && y < (int)fb_height; y++) {
                    if (y >= 0) WM_ChromePixel(x, y, black);
                }
            }

            /* Draw light grey separator columns on either side of close box
             * This separates the teal highlight from the dark grey horizontal stripes */
            uint32_t lightGrey = 0xFFE0E0E0;  /* Match title bar background */

            /* Left separator column - same height as black outline */
            int sepX = boxLeft - 1;
            if (sepX >= 0 && sepX < (int)fb_width) {
                for (y = boxTop; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                    if (y >= 0) WM_ChromePixel(sepX, y, lightGrey);
                }
            }

            /* Right separator column - right against the close box */
            sepX = boxLeft + boxSize - 1;
            if (sepX >= 0 && sepX < (int)fb_width) {
                for (y = boxTop; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                    if (y >= 0) WM_ChromePixel(sepX, y, lightGrey);
                }
            }
        }

        /* Draw title bar separator */
        MoveTo(frame.left, frame.top + 20);
        LineTo(frame.right - 1, frame.top + 20);

        /* Draw window title with System 7 lozenge */
        WM_LOG_TRACE("TITLE_DRAW: titleHandle=%p, *titleHandle=%p\n",
                     window->titleHandle, window->titleHandle ? *window->titleHandle : NULL);

        if (window->titleHandle && *window->titleHandle) {
            /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
            HLock((Handle)window->titleHandle);
            unsigned char* titleStr = (unsigned char*)*window->titleHandle;
            unsigned char titleLen = titleStr[0];

            WM_LOG_TRACE("TITLE_DRAW: titleLen=%d\n", titleLen);

            /* Basic validation: just check length is positive and not obviously corrupt */
            if (titleLen > 0 && titleLen < 128) {
                extern short StringWidth(ConstStr255Param str);
                extern void TextFace(short face);

                short textWidth = StringWidth(titleStr);

                /* System 7 lozenge calculations (exact pixel metrics) */
                short barTop = frame.top;
                short barBottom = barTop + 20;
                short barMidX = (frame.left + frame.right) / 2;
                short textLeft = barMidX - textWidth / 2;
                short textBaseline = barTop + 14;

                /* Lozenge rect (before clipping) */
                Rect loz;
                loz.top = barTop + 3;
                loz.bottom = barBottom - 3;
                loz.left = textLeft - 10;
                loz.right = textLeft + textWidth + 10;

                /* Clip lozenge to avoid controls (4px clearance) */
                short ctrlPad = 4;
                Rect cbRect, zbRect;
                Platform_GetWindowCloseBoxRect(window, &cbRect);
                Platform_GetWindowZoomBoxRect(window, &zbRect);
                short closeRight = cbRect.right;
                short zoomLeft = zbRect.left;

                if (loz.left < closeRight + ctrlPad) loz.left = closeRight + ctrlPad;
                if (loz.right > zoomLeft - ctrlPad) loz.right = zoomLeft - ctrlPad;

                if (window->hilited) {
                    /* Active window: draw rectangular area behind text to cover stripes */

                    /* Fill rectangular lozenge with grey at framebuffer level */
                    if (framebuffer) {
                        uint32_t lightGrey = 0xFFE8E8E8;  /* Same as title bar background */

                        /* Simple rectangular fill to cleanly cover stripes */
                        for (int y = loz.top; y < loz.bottom; y++) {
                            if (y >= 0 && y < (int)fb_height) {
                                for (int x = loz.left; x < loz.right; x++) {
                                    if (x >= 0 && x < (int)fb_width) {
                                        WM_ChromePixel(x, y, lightGrey);
                                    }
                                }
                            }
                        }
                    }

                    /* Draw title text in normal black */
                    PenPat(&qd.black);
                    ForeColor(blackColor);  /* Ensure black text */
                    TextFace(0);  /* normal */
                    MoveTo(textLeft, textBaseline);
                    DrawString(titleStr);
                } else {
                    /* Inactive window: no lozenge, gray text */
                    PenPat(&qd.gray);
                    ForeColor(8);  /* Gray color for inactive title */
                    TextFace(0);  /* normal */
                    MoveTo(textLeft, textBaseline);
                    DrawString(titleStr);
                    PenPat(&qd.black);  /* reset to black */
                    ForeColor(blackColor);  /* reset to black */
                }

                WM_LOG_TRACE("TITLE_DRAW: Drew title at baseline %d\n", textBaseline);
            } else {
                WM_LOG_TRACE("TITLE_DRAW: titleLen %d out of range\n", titleLen);
            }
            /* Unlock handle after use */
            HUnlock((Handle)window->titleHandle);
        } else {
            WM_LOG_TRACE("TITLE_DRAW: No titleHandle or empty\n");
        }
    }

    SetPort(savePort);
}

static void DrawWindowControls_Unclipped(WindowPtr window);

/* Paint this window's chrome, limited to the pixels it actually owns. */
static void DrawWindowControls(WindowPtr window) {
    AutoRgnHandle chromeClip;

    if (!window || !window->visible) return;

    WM_BeginChromeClip(window, &chromeClip);
    DrawWindowControls_Unclipped(window);
    WM_EndChromeClip(&chromeClip);
}

static void DrawWindowControls_Unclipped(WindowPtr window) {
    if (!window || !window->visible) return;

    /* Set up WMgr port for global coordinate drawing */
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);
    SetPort(wmgrPort);

    /* Set up pen for drawing black controls */
    static const Pattern blackPat = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    PenNormal();
    PenPat(&blackPat);
    PenSize(1, 1);

    /* CRITICAL: Use global coordinates from strucRgn, not local portRect */
    /* Use explicit field copy to avoid struct assignment on ARM64 */
    Rect frame;
    if (window->strucRgn && *window->strucRgn) {
        Rect* srcRect = &((*window->strucRgn)->rgnBBox);
        frame.top = srcRect->top;
        frame.left = srcRect->left;
        frame.bottom = srcRect->bottom;
        frame.right = srcRect->right;
    } else {
        /* Fallback to portRect if strucRgn not set */
        frame.top = window->port.portRect.top;
        frame.left = window->port.portRect.left;
        frame.bottom = window->port.portRect.bottom;
        frame.right = window->port.portRect.right;
    }
    /* Close box is drawn in DrawWindowFrame, not here */

    /* Draw zoom box: only on a window that has one, and only while active */
    if (window->spareFlag && window->hilited) {
        Rect zoomBox;
        Platform_GetWindowZoomBoxRect(window, &zoomBox);
        FrameRect(&zoomBox);

        if (window->hilited) {
            /* Draw zoom box lines - use explicit field copy to avoid struct assignment on ARM64 */
            Rect innerBox;
            innerBox.top = zoomBox.top;
            innerBox.left = zoomBox.left;
            innerBox.bottom = zoomBox.bottom;
            innerBox.right = zoomBox.right;
            InsetRect(&innerBox, 2, 2);
            FrameRect(&innerBox);
        }
    }

    /* Draw grow box: only on a kind of window that has one. This went by
     * windowKind >= 0, which includes every dialog. */
    if (WM_WindowHasGrowBox(window)) {
        /* Grow box in bottom-right corner */
        Rect growBox;
        SetRect(&growBox, frame.right - 16, frame.bottom - 16,
                frame.right, frame.bottom);

        if (framebuffer) {
            uint32_t black = 0xFF000000;

            /* Draw three diagonal lines from bottom-left to top-right */
            /* Line 1: Full diagonal */
            for (int i = 0; i < 16; i++) {
                int x = growBox.left + i;
                int y = growBox.bottom - 1 - i;
                if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
                    WM_ChromePixel(x, y, black);
                }
            }

            /* Line 2: Offset by 4 pixels */
            for (int i = 0; i < 12; i++) {
                int x = growBox.left + 4 + i;
                int y = growBox.bottom - 1 - i;
                if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
                    WM_ChromePixel(x, y, black);
                }
            }

            /* Line 3: Offset by 8 pixels */
            for (int i = 0; i < 8; i++) {
                int x = growBox.left + 8 + i;
                int y = growBox.bottom - 1 - i;
                if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
                    WM_ChromePixel(x, y, black);
                }
            }
        }
    }

    /* Draw scroll bars if present */
    ControlHandle control = window->controlList;
    while (control) {
        if ((*control)->contrlVis) {
            /* Would call Draw1Control(control) */
            /* For now, simplified implementation */
        }
        control = (*control)->nextControl;
    }

    /* Restore previous port */
    SetPort(savePort);
}


/* ============================================================================
 * Main Window Drawing Function - Draws Chrome Only
 * ============================================================================ */

void DrawWindow(WindowPtr window) {
    if (!window || !window->visible) {
        WM_LOG_TRACE("WindowManager: DrawWindow - window NULL or not visible\n");
        return;
    }

    WM_LOG_TRACE("WindowManager: DrawWindow ENTRY for window '%s'\n",
                  window->titleHandle ? (char*)*window->titleHandle : "Untitled");

    /* Save current port */
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* Draw chrome in WMgr port */
    SetPort(wmgrPort);
    DrawWindowFrame(window);
    DrawWindowControls(window);

    /* Switch to window port for content */
    SetPort((GrafPtr)window);

    /* Fill content area - LOCAL coords - use explicit field copy to avoid struct assignment on ARM64 */
    Rect contentRect;
    contentRect.top = window->port.portRect.top;
    contentRect.left = window->port.portRect.left;
    contentRect.bottom = window->port.portRect.bottom;
    contentRect.right = window->port.portRect.right;
    WM_LOG_TRACE("DrawWindow: Filling content rect (local) (%d,%d,%d,%d)\n",
        contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);
    EraseRect(&contentRect);
    WM_LOG_TRACE("DrawWindow: Content filled\n");

    SetPort(savePort);
    WM_LOG_TRACE("DrawWindow: EXIT\n");
}

void DrawGrowIcon(WindowPtr window) {
    if (!window || !window->visible || window->windowKind < 0) return;

    WM_DEBUG("DrawGrowIcon: Drawing grow icon");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* Draw grow icon in bottom-right corner - use explicit field copy to avoid struct assignment on ARM64 */
    Rect frame;
    frame.top = window->port.portRect.top;
    frame.left = window->port.portRect.left;
    frame.bottom = window->port.portRect.bottom;
    frame.right = window->port.portRect.right;
    Rect growBox;
    SetRect(&growBox, frame.right - 16, frame.bottom - 16,
            frame.right, frame.bottom);

    /* Clear the grow box area first */
    EraseRect(&growBox);

    /* Draw the grow lines */
    MoveTo(growBox.left, growBox.bottom - 1);
    LineTo(growBox.right - 1, growBox.top);
    MoveTo(growBox.left + 4, growBox.bottom - 1);
    LineTo(growBox.right - 1, growBox.top + 4);
    MoveTo(growBox.left + 8, growBox.bottom - 1);
    LineTo(growBox.right - 1, growBox.top + 8);

    SetPort(savePort);
}

/*-----------------------------------------------------------------------*/
/* Window Visibility Functions                                          */
/*-----------------------------------------------------------------------*/

/* Shown, drawn and its update posted - but not made active, which is the
 * difference between ShowWindow and ShowHide. Returns false if it already
 * showed. */
static Boolean WM_ShowWindowOnly(WindowPtr window) {
    if (!window || window->visible) {
        return false;
    }

    window->visible = true;

    /* Calculate window regions (structure and content) */
    WM_CalculateStandardWindowRegions(window, 0);

    /* Calculate visible region */
    CalcVis(window);

    /* CRITICAL: Redraw desktop icons BEFORE painting window to ensure icons appear behind window */
    if (g_deskHook && window->strucRgn) {
        /* Create region for area under window */
        AutoRgnHandle windowRgn = WM_NewAutoRgn();
        if (windowRgn.rgn) {
            CopyRgn(window->strucRgn, windowRgn.rgn);
            g_deskHook(windowRgn.rgn);  /* Redraw desktop icons in this region */
        }
        WM_DisposeAutoRgn(&windowRgn);
    }

    /* Paint the window */
    WM_LOG_TRACE("ShowWindow: About to call PaintOne\n");
    PaintOne(window, NULL);
    WM_LOG_TRACE("ShowWindow: PaintOne returned\n");

    /* Invalidate content region to generate update event for application to draw content */
    if (window->contRgn) {
        WM_LOG_TRACE("ShowWindow: Invalidating content region to trigger update event\n");
        /* InvalRgn operates on current port, so set port to window first */
        GrafPtr savePort;
        GetPort(&savePort);
        SetPort((GrafPtr)window);

        /* Keep clipRgn separate from contRgn because QuickDraw mutates the
         * active clip while the content geometry must remain unchanged.
         */
        CopyRgn(window->contRgn, window->port.clipRgn);

        WM_InvalGlobalRgn(window, window->contRgn);

        SetPort(savePort);
    }

    /* Recalculate regions for windows behind */
    CalcVisBehind(window->nextWindow, window->strucRgn);

    /* Don't call PaintBehind here - background windows are already painted.
     * Calling PaintBehind would cause background windows to paint over the front window. */

    WM_LOG_TRACE("ShowWindow: EXIT\n");
    return true;
}

void ShowWindow(WindowPtr window) {
    if (!WM_ShowWindowOnly(window)) return;

    /* A window shown at the front becomes the active one. Newly created
     * document windows arrive this way rather than through SelectWindow, and
     * without this nothing tells the application it now owns the front. */
    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState && wmState->windowList == window) {
        WM_SetActiveWindow(window);
    }
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

/* The part of window's structure no visible window in front covers. */
static void WM_VisibleStructure(WindowPtr window, RgnHandle out) {
    CopyRgn(window->strucRgn, out);
    WindowManagerState* wm = GetWindowManagerState();
    for (WindowPtr w = wm ? wm->windowList : NULL; w && w != window; w = w->nextWindow) {
        if (w->visible && w->strucRgn && *w->strucRgn) DiffRgn(out, w->strucRgn, out);
    }
}

/* Hidden and what it covered redrawn, with no other window made active. */
static void WM_HideWindowOnly(WindowPtr window) {
    /* Only the part that was showing is uncovered */
    AutoRgnHandle clobbered = WM_NewAutoRgn();
    if (clobbered.rgn && window->strucRgn && *window->strucRgn) {
        WM_VisibleStructure(window, clobbered.rgn);
    }
    window->visible = false;

    /* The desktop and the windows behind, within it. This erased the
     * window's whole area in the Window Manager port's background - white -
     * over any window in front, and never put the desktop back. */
    if (clobbered.rgn && !EmptyRgn(clobbered.rgn)) {
        if (g_deskHook) {
            g_deskHook(clobbered.rgn);
        }
        CalcVisBehind(window->nextWindow, clobbered.rgn);
        PaintBehind(window->nextWindow, clobbered.rgn);
    }
    WM_DisposeAutoRgn(&clobbered);
}

void HideWindow(WindowPtr window) {
    if (!window || !window->visible) {
        return;
    }
    WindowManagerState* wm = GetWindowManagerState();
    Boolean wasActive = window->hilited;
    WM_HideWindowOnly(window);

    /* Hiding the active window makes the next one active (4-91) */
    if (wasActive) {
        window->hilited = false;
        for (WindowPtr w = wm ? wm->windowList : NULL; w; w = w->nextWindow) {
            if (w->visible) {
                HiliteWindow(w, true);
                break;
            }
        }
    }
}

/* ShowHide shows or hides and does nothing else: no window is highlighted
 * or unhighlighted, and none becomes active (IM I-285). Calling ShowWindow
 * and HideWindow for it activated the window it showed. */
void ShowHide(WindowPtr window, Boolean showFlag) {
    if (!window || window->visible == (showFlag != 0)) return;
    if (showFlag) {
        WM_ShowWindowOnly(window);
    } else {
        WM_HideWindowOnly(window);
    }
}

/*-----------------------------------------------------------------------*/
/* Window Highlighting Functions                                        */
/*-----------------------------------------------------------------------*/

void HiliteWindow(WindowPtr window, Boolean fHilite) {
    if (!window) return;

    if (window->hilited == fHilite) {
        WM_LOG_TRACE("[HILITE] Window %p already has hilite=%d, skipping\n", window, fHilite);
        return;
    }

    WM_LOG_TRACE("[HILITE] Window %p: changing hilite %d -> %d\n", window, window->hilited, fHilite);

    window->hilited = fHilite;

    /* Redraw the window frame to show highlight state */
    /* NOTE: DrawWindowFrame and DrawWindowControls set their own ports to WMgrPort */
    MemoryManager_CheckSuspectBlock("HiliteWindow_pre_DrawWindowFrame");
    DrawWindowFrame(window);
    MemoryManager_CheckSuspectBlock("HiliteWindow_post_DrawWindowFrame");
    MemoryManager_CheckSuspectBlock("HiliteWindow_pre_DrawWindowControls");
    DrawWindowControls(window);
    MemoryManager_CheckSuspectBlock("HiliteWindow_post_DrawWindowControls");

    WM_LOG_TRACE("[HILITE] Window %p: frame redrawn with hilite=%d\n", window, fHilite);
}

/*-----------------------------------------------------------------------*/
/* Window Debugging Functions                                           */
/*-----------------------------------------------------------------------*/

static void DumpWindowList(const char* context) {
    (void)context;
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) {
        WM_LOG_TRACE("[WINLIST] %s: No WM state\n", context);
        return;
    }

    WM_LOG_TRACE("[WINLIST] === %s ===\n", context);
    int count = 0;
    WindowPtr w = wmState->windowList;
    while (w && count < 20) {
        WM_LOG_TRACE("[WINLIST]   [%d] Win=%p visible=%d hilited=%d next=%p refCon=0x%08x\n",
                     count, w, w->visible, w->hilited, w->nextWindow, (unsigned int)w->refCon);
        if (w == w->nextWindow) {
            WM_LOG_TRACE("[WINLIST]   ERROR: Circular reference detected!\n");
            break;
        }
        w = w->nextWindow;
        count++;
    }
    WM_LOG_TRACE("[WINLIST] === Total: %d windows ===\n", count);
}

/*-----------------------------------------------------------------------*/
/* Window Ordering Functions                                            */
/*-----------------------------------------------------------------------*/

void BringToFront(WindowPtr window) {
    if (!window) return;

    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WM_DEBUG("BringToFront: Moving window to front");
    DumpWindowList("BringToFront - START");
    MemoryManager_CheckSuspectBlock("BringToFront_start");

    /* CRITICAL: Save the current front window BEFORE modifying the list */
    WindowPtr prevFront = wmState->windowList;

    /* If already at front, just ensure it's hilited */
    if (prevFront == window) {
        WM_LOG_TRACE("[HILITE] Window already at front, ensuring hilited\n");
        HiliteWindow(window, true);
        return;
    }

    /* Remove window from current position */
    /* What was hidden before it comes forward: only that needs drawing */
    AutoRgnHandle exposed = WM_NewAutoRgn();
    if (exposed.rgn && window->strucRgn && *window->strucRgn) {
        WM_VisibleStructure(window, exposed.rgn);
        DiffRgn(window->strucRgn, exposed.rgn, exposed.rgn);
    }

    WindowPtr prev = NULL;
    WindowPtr current = wmState->windowList;

    while (current && current != window) {
        prev = current;
        current = current->nextWindow;
    }

    if (!current) return;  /* Window not in list */

    /* Remove from list - CRITICAL: Handle both cases to prevent circular list! */
    if (prev) {
        prev->nextWindow = window->nextWindow;
    } else {
        /* Window was at front of list - must update windowList head! */
        wmState->windowList = window->nextWindow;
    }

    /* Add to front */
    window->nextWindow = wmState->windowList;
    wmState->windowList = window;

    /* Unhighlight the window that will be demoted (if any) */
    if (prevFront) {
        WM_LOG_TRACE("[HILITE] Unhiliting previous front window %p\n", prevFront);
        MemoryManager_CheckSuspectBlock("BringToFront_pre_unhilite");
        HiliteWindow(prevFront, false);
        MemoryManager_CheckSuspectBlock("BringToFront_post_unhilite");
    }

    /* Now hilite and paint the new front window */
    MemoryManager_CheckSuspectBlock("BringToFront_pre_hilite_new");
    HiliteWindow(window, true);
    MemoryManager_CheckSuspectBlock("BringToFront_post_hilite_new");

    /* Recalculate visible regions */
    CalcVisBehind(window, NULL);
    MemoryManager_CheckSuspectBlock("BringToFront_post_CalcVisBehind");

    /* Draw the window, and in its content only what was covered (Inside
     * Macintosh: Toolbox Essentials, 4-101). This repainted every window,
     * so the whole screen flickered at each click on a window behind. */
    PaintOne(window, exposed.rgn);
    WM_DisposeAutoRgn(&exposed);
    MemoryManager_CheckSuspectBlock("BringToFront_post_PaintBehind");

    DumpWindowList("BringToFront - END");
}

void SendBehind(WindowPtr window, WindowPtr behindWindow) {
    if (!window) return;

    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WM_DEBUG("SendBehind: Moving window behind another");
    DumpWindowList("SendBehind - START");

    /* Remove window from current position */
    WindowPtr prev = NULL;
    WindowPtr current = wmState->windowList;

    while (current && current != window) {
        prev = current;
        current = current->nextWindow;
    }

    if (!current) return;  /* Window not in list */

    /* Remove from list */
    if (prev) {
        prev->nextWindow = window->nextWindow;
    } else {
        wmState->windowList = window->nextWindow;
    }

    /* Insert after behindWindow */
    if (behindWindow) {
        window->nextWindow = behindWindow->nextWindow;
        behindWindow->nextWindow = window;
    } else {
        /* Send to back */
        current = wmState->windowList;
        while (current && current->nextWindow) {
            current = current->nextWindow;
        }
        if (current) {
            current->nextWindow = window;
        } else {
            wmState->windowList = window;
        }
        window->nextWindow = NULL;
    }

    /* Update highlight states */
    WindowPtr front = FrontWindow();
    current = wmState->windowList;
    while (current) {
        HiliteWindow(current, current == front);
        current = current->nextWindow;
    }

    /* Recalculate visible regions */
    CalcVisBehind(window, NULL);

    /* Repaint affected windows */
    /* Every window in the area it covered: those that came forward over it
     * were never redrawn, since this started at the window itself. */
    PaintBehind(NULL, window->strucRgn);

    DumpWindowList("SendBehind - END");
}

/*-----------------------------------------------------------------------*/
/* Window Selection Functions                                           */
/*-----------------------------------------------------------------------*/

/*
 * WM_SetActiveWindow - hand activation from one window to another, or to none.
 *
 * The front window changes in more places than SelectWindow: a document window
 * is created in front of everything, a hidden window is shown, the active
 * window closes and the one below it takes over. Those paths either said
 * nothing or simply nulled out activeWindow, so no application ever learned
 * the front window had moved. SimpleText installs its menu bar from its
 * activate event, so its menus never appeared and Save, Close, Font and Style
 * were unreachable; closing a document left no window active at all. This is
 * now the only place that knows how activation moves, and every front-window
 * change goes through it.
 *
 * Pass NULL to deactivate whatever is active without activating anything.
 */
void WM_SetActiveWindow(WindowPtr window)
{
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WindowPtr previous = wmState->activeWindow;
    if (previous == window) return;

    /* Take it out of the state first: WM_OnDeactivate and HiliteWindow can
     * both draw, and neither should see a window that is half-way out. */
    wmState->activeWindow = NULL;

    if (previous) {
        WM_OnDeactivate(previous);
        HiliteWindow(previous, false);
        /* The message is the window. Whether this is an activate or a
         * deactivate lives in the modifiers - activeFlag is a modifiers bit,
         * and ORing it into the message only corrupts the pointer. */
        PostEventWithModifiers(activateEvt, (UInt32)(uintptr_t)previous, 0);
    }

    if (window) {
        wmState->activeWindow = window;
        WM_OnActivate(window);
        HiliteWindow(window, true);
        PostEventWithModifiers(activateEvt, (UInt32)(uintptr_t)window, activeFlag);
    }
}

/*
 * WM_ActiveWindowClosed - the active window has left the window list.
 *
 * The window is already gone, so it gets no deactivate event - there is
 * nothing left to deactivate. Activation passes to the frontmost window still
 * standing, which is what makes the Finder's menu bar come back when the last
 * document window closes.
 */
void WM_ActiveWindowClosed(void)
{
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    wmState->activeWindow = NULL;

    WindowPtr next = wmState->windowList;
    while (next && !next->visible) {
        next = next->nextWindow;
    }
    WM_SetActiveWindow(next);
}

void SelectWindow(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("SelectWindow: Selecting window");
    DumpWindowList("SelectWindow - START");

    BringToFront(window);
    WM_SetActiveWindow(window);
}

/*-----------------------------------------------------------------------*/
/* Window Query Functions                                               */
/*-----------------------------------------------------------------------*/

WindowPtr FrontWindow(void) {
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) {
        WM_LOG_TRACE("WindowManager: FrontWindow - wmState is NULL\n");
        return NULL;
    }

    static int call_count = 0;
    call_count++;

    /* Find first visible window */
    WindowPtr window = wmState->windowList;

    if (call_count <= 5 || (call_count % 5000) == 0) {
        WM_LOG_TRACE("WindowManager: FrontWindow #%d - list head=%p\n", call_count, window);
    }

    int count = 0;
    while (window) {
        if (call_count <= 5 || (call_count % 5000) == 0) {
            WM_LOG_TRACE("WindowManager: FrontWindow - checking window %p, visible=%d\n", window, window->visible);
        }
        if (window->visible) {
            if (call_count <= 5 || (call_count % 5000) == 0) {
                WM_LOG_TRACE("WindowManager: FrontWindow - returning visible window %p\n", window);
            }
            return window;
        }
        window = window->nextWindow;
        count++;
        if (count > 100) {
            WM_LOG_TRACE("WindowManager: FrontWindow - LOOP DETECTED, breaking\n");
            break;
        }
    }

    if (call_count <= 5 || (call_count % 5000) == 0) {
        WM_LOG_TRACE("WindowManager: FrontWindow - returning NULL (no visible window found)\n");
    }
    return NULL;
}

WindowPtr WM_FindWindowAt(Point pt) {
    WindowManagerState* wmState = GetWindowManagerState();

    WM_DEBUG("WM_FindWindowAt: Finding window at point (%d, %d)", pt.h, pt.v);

    /* Search windows from front to back */
    WindowPtr current = wmState->windowList;
    while (current) {
        if (current->visible && current->strucRgn) {
            if (Platform_PtInRgn(pt, current->strucRgn)) {
                WM_DEBUG("WM_FindWindowAt: Found window");
                return current;
            }
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_FindWindowAt: No window found at point");
    return NULL;
}

WindowPtr WM_GetNextVisibleWindow(WindowPtr window) {
    if (window == NULL) return NULL;

    WindowPtr current = window->nextWindow;
    while (current) {
        if (current->visible) {
            return current;
        }
        current = current->nextWindow;
    }

    return NULL;
}

WindowPtr WM_GetPreviousWindow(WindowPtr window) {
    if (window == NULL) return NULL;

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;

    /* Find the window that points to our window */
    while (current && current->nextWindow != window) {
        current = current->nextWindow;
    }

    return current;
}

/*-----------------------------------------------------------------------*/
/* Desktop Hook and Display Update Functions                            */
/*-----------------------------------------------------------------------*/

/* DeskHook type definition if not in headers */
typedef void (*DeskHookProc)(RgnHandle invalidRgn);

/* DeskHook support */
DeskHookProc g_deskHook = NULL;  /* Non-static so WindowDragging.c can access it */

/* Tracks if display needs updating to avoid constant flashing */
static Boolean gDisplayDirty = true;
static int gUpdateThrottle = 0;

void SetDeskHook(DeskHookProc proc) {
    g_deskHook = proc;
}

/* Public function to mark display as dirty (used by AppSwitcher and others) */
void WM_InvalidateDisplay_Public(void) {
    gDisplayDirty = true;
}

/* Window Manager update pipeline functions */
/* WM_Update is needed by main.c even when other stubs are disabled */
void WM_Update(void) {

    /* Throttle updates to reduce flashing - only update periodically */
    gUpdateThrottle++;
    if (gUpdateThrottle < 1) {  /* Update every frame for testing */
        return;
    }
    gUpdateThrottle = 0;

    /* Create a screen port if qd.thePort is NULL */
    static GrafPort screenPort;
    if (qd.thePort == NULL) {
        /* Initialize the screen port */
        OpenPort(&screenPort);
        screenPort.portBits = qd.screenBits;  /* Use screen bitmap */
        screenPort.portRect = qd.screenBits.bounds;
        qd.thePort = &screenPort;
    }

    /* Only when asked for - the first frame, and the application switcher.
     * This also ran whenever the number of windows changed, filling the whole
     * screen with gray over every window and painting them all again, though
     * opening and closing a window already repaint what they uncover: the
     * windows' contents blanked and stray rows of the gray stayed behind, such
     * as a dotted line across a title bar when Desktop Patterns opened. */
    if (!gDisplayDirty) {
        return;
    }
    gDisplayDirty = false;

    int currentWindowCount = 0;
    {
        for (WindowPtr w = FrontWindow(); w; w = w->nextWindow) {
            currentWindowCount++;
        }
    }

    /* Use QuickDraw to draw desktop */
    GrafPtr savePort;
    GetPort(&savePort);
    QD_SetScreenPort();  /* the desktop is drawn in global coordinates */

    /* 1. Draw desktop pattern first */
    Rect desktopRect;
    SetRect(&desktopRect, 0, 20,
            qd.screenBits.bounds.right,
            qd.screenBits.bounds.bottom);
    if (!g_deskHook) {
        FillRect(&desktopRect, &qd.gray);  /* no Finder yet to paint it */
    }

    /* 2. Call DeskHook BEFORE windows to draw desktop icons behind windows */
    if (g_deskHook) {
        /* Create a region for the desktop */
        AutoRgnHandle desktopRgn = WM_NewAutoRgn();
        if (desktopRgn.rgn) {
            RectRgn(desktopRgn.rgn, &desktopRect);
            g_deskHook(desktopRgn.rgn);
        }
        WM_DisposeAutoRgn(&desktopRgn);
    }

    /* 3. Draw all visible windows on top of desktop icons */
    /* Use Window Manager's PaintOne to properly render windows */
    {
        /* Build window list (back to front order) */
        WindowPtr window = FrontWindow();
        WindowPtr* windowStack = NULL;
        int windowCount = currentWindowCount;

        /* Allocate and fill window stack */
        if (windowCount > 0) {
            /* Check for integer overflow in size calculation */
            if ((size_t)windowCount > SIZE_MAX / sizeof(WindowPtr)) {
                return;  /* Too many windows, cannot allocate */
            }

            windowStack = (WindowPtr*)NewPtr(windowCount * sizeof(WindowPtr));
            if (windowStack) {
                /* Initialize array to NULL to avoid uninitialized access */
                memset(windowStack, 0, windowCount * sizeof(WindowPtr));

                WindowPtr w = window;
                for (int i = 0; i < windowCount && w; i++) {
                    windowStack[i] = w;
                    w = w->nextWindow;
                }

                /* Draw windows from back to front (reverse order) */
                for (int i = windowCount - 1; i >= 0; i--) {
                    WindowPtr wnd = windowStack[i];
                    if (wnd && wnd->visible) {
                        /* Use sophisticated window drawing from WindowDisplay.c */
                        PaintOne(wnd, NULL);
                    }
                }

                DisposePtr((Ptr)windowStack);
            }
        }
    }

    /* Draw menu bar LAST to ensure clean pen position */
    MoveTo(0, 0);  /* Reset pen position before drawing menu bar */
    extern void DrawMenuBar(void);
    DrawMenuBar();  /* Menu Manager draws the menu bar */

    /* Draw application switcher overlay if active */
    extern Boolean AppSwitcher_IsActive(void);
    extern void AppSwitcher_Draw(void);
    if (AppSwitcher_IsActive()) {
        AppSwitcher_Draw();
    }

    /* Mouse cursor is now drawn separately in main.c for better performance */
    SetPort(savePort);
}

/*
 * WM_UpdateWindowVisibility - Update window visibility state
 */
void WM_UpdateWindowVisibility(WindowPtr window) {
    if (!window) {
        WM_LOG_DEBUG("WM_UpdateWindowVisibility: NULL window\n");
        return;
    }

    WM_LOG_DEBUG("WM_UpdateWindowVisibility: Updating visibility for window at %p\n", window);

    /* Update window visibility state */
    if (window->visible) {
        /* Ensure window is drawn */
        GrafPort* port = (GrafPort*)window;
        InvalRect(&port->portRect);
    }
}

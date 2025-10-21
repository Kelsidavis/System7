#include <stdio.h>

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "ControlManager/ControlTypes.h"
#include "SystemTheme.h"
#include "WindowManager/WMLogging.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "sys71_stubs.h"

/* Color constants */
#define blackColor 33

/* External functions */
extern void LineTo(short h, short v);
extern void MoveTo(short h, short v);
extern void FrameRect(const Rect* r);
extern void PaintRect(const Rect* r);

/* External globals */
extern QDGlobals qd;
extern void EraseRect(const Rect* r);
extern void InsetRect(Rect* r, short dh, short dv);
extern void SetRect(Rect* r, short left, short top, short right, short bottom);
extern Boolean PtInRect(Point pt, const Rect* r);
extern Boolean EqualRect(const Rect* rect1, const Rect* rect2);
extern void UnionRect(const Rect* src1, const Rect* src2, Rect* dstRect);
extern Boolean EmptyRect(const Rect* r);
extern void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);
extern void SetRectRgn(RgnHandle rgn, short left, short top, short right, short bottom);
extern void OffsetRgn(RgnHandle rgn, short dh, short dv);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void ClipRect(const Rect* r);
extern void SetClip(RgnHandle rgn);
extern void GetClip(RgnHandle rgn);
extern RgnHandle NewRgn(void);
extern void DisposeRgn(RgnHandle rgn);
extern void SetOrigin(SInt16 h, SInt16 v);
extern void serial_puts(const char* str);

/* Forward declarations */
static void DumpWindowList(const char* context);
void CheckWindowsNeedingUpdate(void);

/*-----------------------------------------------------------------------*/
/* Window Display Functions                                             */
/*-----------------------------------------------------------------------*/

/* Check windows for update events (called by GetNextEvent) */
void CheckWindowsNeedingUpdate(void) {
    /* PostEvent declared in EventManager.h */
    extern WindowPtr FrontWindow(void);
    extern Boolean EmptyRgn(RgnHandle rgn);

    static int call_count = 0;
    call_count++;

    /* Walk all visible windows and post update events for windows with non-empty updateRgn */
    WindowPtr window = FrontWindow();

    if (call_count <= 10 || (call_count % 500) == 0) {
        WM_LOG_TRACE("CheckWindowsNeedingUpdate: #%d, frontWindow=0x%08x\n", call_count, (unsigned int)window);
    }

    int windowCount = 0;
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
            }
        }

        if (window->visible && window->updateRgn && !EmptyRgn(window->updateRgn)) {
            WM_LOG_TRACE("CheckWindowsNeedingUpdate: Posting update event for window 0x%08x\n", (unsigned int)window);
            PostEvent(6 /* updateEvt */, (SInt32)window);
            /* Don't clear updateRgn here - BeginUpdate/EndUpdate will handle it */
        }
        window = window->nextWindow;
    }
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
    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* CRITICAL: Fill window with white in WMgrPort using GLOBAL strucRgn coordinates
     * This ensures we always fill the correct screen position regardless of port state */
    SetPort(wmgrPort);
    WM_LOG_TRACE("PaintOne: Switched to WMgr port for backfill\n");

    /* Reset clip in WMgr port */
    extern RgnHandle NewRgn(void);
    extern void SetRectRgn(RgnHandle rgn, short left, short top, short right, short bottom);
    extern void DisposeRgn(RgnHandle rgn);
    RgnHandle fullClipWMgr = NewRgn();
    SetRectRgn(fullClipWMgr, -32768, -32768, 32767, 32767);
    SetClip(fullClipWMgr);
    DisposeRgn(fullClipWMgr);

    /* CRITICAL: Fill content region with white background BEFORE drawing chrome
     * This prevents garbage/dotted patterns from appearing in the window content area
     * The application will draw over this white background when handling update events
     *
     * EXCEPTION: Skip filling windows with refCon=0 (desktop background window)
     * as filling it with white would erase desktop icons */
    extern void serial_puts(const char* str);
    static char dbgbuf[128];
    static int fill_log = 0;

    if (window->contRgn && *(window->contRgn)) {
        Region* rgn = *(window->contRgn);
        if (fill_log < 10) {
            sprintf(dbgbuf, "[PAINTONE] refCon=0x%08x fill=%d contRgn bbox=(%d,%d,%d,%d)\n",
                   (unsigned int)window->refCon, (window->refCon != 0),
                   rgn->rgnBBox.left, rgn->rgnBBox.top,
                   rgn->rgnBBox.right, rgn->rgnBBox.bottom);
            serial_puts(dbgbuf);
            fill_log++;
        }

        if (window->refCon != 0) {
            extern void FillRgn(RgnHandle rgn, const Pattern* pat);
            extern QDGlobals qd;
            FillRgn(window->contRgn, &qd.white);
        }
    }

    /* NOW draw chrome on top of backfill */
    WM_LOG_TRACE("PaintOne: Drawing window chrome\n");
    WM_LOG_TRACE("PaintOne: About to call DrawWindowFrame, window=%p\n", window);
    DrawWindowFrame(window);
    WM_LOG_TRACE("PaintOne: DrawWindowFrame returned\n");
    DrawWindowControls(window);
    WM_LOG_TRACE("PaintOne: DrawWindowControls returned\n");

    /* Test content drawing temporarily disabled until Font Manager is linked */
    WM_LOG_TRACE("[TEXT] Text drawing disabled - Font Manager not linked\n");

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
            extern void InvalRgn(RgnHandle badRgn);
            GrafPtr savePort;
            GetPort(&savePort);
            SetPort((GrafPtr)w);

            /* CRITICAL: Calculate visible region and clip to it to prevent overdraw! */
            CalcVis(w);
            if (w->visRgn) {
                w->port.clipRgn = w->visRgn;
            }

            InvalRgn(w->contRgn);

            /* WORKAROUND: Directly redraw folder window content since update events may not flow */
            if (w->refCon == 0x4449534b || w->refCon == 0x54525348) {  /* 'DISK' or 'TRSH' */
                extern void FolderWindow_Draw(WindowPtr window);
                WM_LOG_TRACE("[PaintBehind] Drawing content for window %p (index %d of %d), refCon=0x%08x\n",
                             w, i, count, (unsigned int)w->refCon);
                FolderWindow_Draw(w);
            }

            SetPort(savePort);

            /* Phase 3: Redraw chrome on top of content to ensure it's not covered by fills
             * Call PaintOne again to redraw the frame and controls on top */
            WM_LOG_TRACE("[PaintBehind] Redrawing chrome on top of content for window %p\n", w);
            PaintOne(w, clobberedRgn);
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

void CalcVisBehind(WindowPtr startWindow, RgnHandle clobberedRgn) {
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
    RgnHandle clipRgn = NewRgn();
    if (clipRgn) {
        /* Start with full screen */
        SetRectRgn(clipRgn, 0, 0, 1024, 768);

        /* Subtract regions of windows in front */
        WindowPtr frontWindow = FrontWindow();
        while (frontWindow && frontWindow != window) {
            if (frontWindow->visible && frontWindow->strucRgn) {
                /* Would subtract frontWindow->strucRgn from clipRgn */
                /* For now, simplified implementation */
            }
            frontWindow = frontWindow->nextWindow;
        }

        SetClip(clipRgn);
        DisposeRgn(clipRgn);
    }
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
    extern void GetWMgrPort(GrafPtr* port);
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

static void DrawWindowFrame(WindowPtr window) {
    WM_LOG_TRACE("DrawWindowFrame: ENTRY, window=%p\n", window);

    if (!window) {
        WM_LOG_TRACE("DrawWindowFrame: window is NULL, returning\n");
        return;
    }

    WM_LOG_TRACE("DrawWindowFrame: window->visible=%d\n", window->visible);
    if (!window->visible) {
        WM_LOG_TRACE("DrawWindowFrame: window not visible, returning\n");
        return;
    }

    /* CRITICAL: strucRgn must be set to draw the window
     * strucRgn contains GLOBAL screen coordinates
     * portRect contains LOCAL coordinates (0,0,width,height) and must NEVER be used for positioning! */
    WM_LOG_TRACE("DrawWindowFrame: Checking strucRgn=%p\n", window->strucRgn);
    if (!window->strucRgn) {
        WM_LOG_TRACE("WindowManager: DrawWindowFrame - strucRgn is NULL, cannot draw\n");
        return;
    }

    WM_LOG_TRACE("DrawWindowFrame: Checking *strucRgn=%p\n", *window->strucRgn);
    if (!*window->strucRgn) {
        WM_LOG_TRACE("WindowManager: DrawWindowFrame - *strucRgn is NULL, cannot draw\n");
        return;
    }

    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);
    SetPort(wmgrPort);

    WM_LOG_TRACE("WindowManager: DrawWindowFrame START\n");

    /* Set up pen for drawing black frames */
    extern void PenNormal(void);
    extern void PenSize(short width, short height);
    static const Pattern blackPat = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    PenNormal();  /* Reset pen to normal state */
    PenPat(&blackPat);  /* Use black pattern for frames */
    PenSize(1, 1);  /* 1-pixel pen */

    /* Get window's global bounds from structure region */
    Rect frame = (*window->strucRgn)->rgnBBox;

    WM_LOG_TRACE("WindowManager: Frame rect (%d,%d,%d,%d)\n",
                  frame.left, frame.top, frame.right, frame.bottom);

    /* Draw frame outline using QuickDraw (not direct framebuffer) */
    FrameRect(&frame);
    WM_LOG_TRACE("WindowManager: Drew frame using FrameRect\n");

    /* Add 3D black highlights for depth effect */
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;
    if (framebuffer) {
        uint32_t* fb = (uint32_t*)framebuffer;
        int pitch = fb_pitch / 4;
        uint32_t black = 0xFF000000;

        /* Right side highlight: 2px wide, starting 1px down from top and extending to bottom */
        int highlightStartY = frame.top + 1;
        for (int y = highlightStartY; y < frame.bottom - 1 && y < (int)fb_height; y++) {
            if (y >= 0) {
                /* Draw 2 pixels on the right side (inside the frame) */
                for (int dx = 1; dx <= 2; dx++) {
                    int x = frame.right - 1 - dx;
                    if (x >= 0 && x < (int)fb_width) {
                        fb[y * pitch + x] = black;
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
                        fb[y * pitch + x] = black;
                    }
                }
            }
        }
    }

    /* Draw title bar BEFORE filling content area */
    WM_LOG_TRACE("WindowManager: About to check titleWidth=%d\n", window->titleWidth);

    if (window->titleWidth > 0) {
        WM_LOG_TRACE("WindowManager: titleWidth > 0, drawing title bar\n");
        serial_printf("[TITLEBAR] Drawing title bar for window, hilited=%d\n", window->hilited);
        /* Title bar background should be INSIDE the frame, not overlap it */
        Rect titleBar;
        titleBar.left = frame.left + 1;    /* Inset from left frame edge */
        titleBar.top = frame.top + 1;      /* Inset from top frame edge */
        titleBar.right = frame.right - 2;  /* Inset 2px from right to not overlap frame */
        titleBar.bottom = frame.top + 20;  /* Extends to separator line */

        serial_printf("[TITLEBAR] titleBar rect=(%d,%d,%d,%d) frame=(%d,%d,%d,%d)\n",
                      titleBar.left, titleBar.top, titleBar.right, titleBar.bottom,
                      frame.left, frame.top, frame.right, frame.bottom);

        /* Fill title bar background */
        extern void* framebuffer;
        extern uint32_t fb_width, fb_height, fb_pitch;

        if (window->hilited) {
            /* Active window: solid light grey background with darker horizontal stripes */
            if (framebuffer) {
                uint32_t* fb = (uint32_t*)framebuffer;
                int pitch = fb_pitch / 4;
                uint32_t lightGrey = 0xFFE8E8E8;  /* Solid lighter grey RGB(232,232,232) */
                uint32_t darkGrey = 0xFF808080;   /* Solid darker grey RGB(128,128,128) for stripes */

                /* Fill entire title bar with light grey */
                for (int y = titleBar.top; y < titleBar.bottom && y < (int)fb_height; y++) {
                    if (y >= 0) {
                        for (int x = titleBar.left; x < titleBar.right && x < (int)fb_width; x++) {
                            if (x >= 0) {
                                fb[y * pitch + x] = lightGrey;
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
                                fb[y * pitch + x] = darkGrey;
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
                        if (x >= 0) fb[y * pitch + x] = highlightColor;
                    }
                }

                /* Bottom border - 1px inside (at separator line) */
                y = titleBar.bottom - 1;
                if (y >= 0 && y < (int)fb_height) {
                    for (int x = titleBar.left; x < titleBar.right && x < (int)fb_width; x++) {
                        if (x >= 0) fb[y * pitch + x] = highlightColor;
                    }
                }

                /* Left border - 1px inside */
                int x = titleBar.left;
                if (x >= 0 && x < (int)fb_width) {
                    for (y = titleBar.top; y < titleBar.bottom && y < (int)fb_height; y++) {
                        if (y >= 0) fb[y * pitch + x] = highlightColor;
                    }
                }

                /* Right border - 1px inside */
                x = titleBar.right - 1;
                if (x >= 0 && x < (int)fb_width) {
                    for (y = titleBar.top; y < titleBar.bottom && y < (int)fb_height; y++) {
                        if (y >= 0) fb[y * pitch + x] = highlightColor;
                    }
                }
            }
        } else {
            /* Inactive window: white background */
            EraseRect(&titleBar);
        }

        /* Draw System 7 close box - 14x14 at left side
         * Design: Black outline (left/top only for 3D), 1px theme highlight inside, grey fill */
        if (framebuffer) {
            int boxLeft = frame.left + 10;
            int boxTop = frame.top + 4;
            int boxSize = 14;

            uint32_t black = 0xFF000000;
            uint32_t grey = 0xFF808080;  /* Same grey as title bar stripes */
            uint32_t* fb = (uint32_t*)framebuffer;
            int pitch = fb_pitch / 4;

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
                    fb[boxTop * pitch + x] = black;
                }
            }
            /* Left edge */
            for (int y = boxTop; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                if (y >= 0 && boxLeft >= 0 && boxLeft < (int)fb_width) {
                    fb[y * pitch + boxLeft] = black;
                }
            }

            /* Draw 1px themed highlight border (complete box around grey) */
            /* Top highlight line */
            int y = boxTop + 1;
            if (y >= 0 && y < (int)fb_height) {
                for (int x = boxLeft + 1; x < boxLeft + boxSize - 1 && x < (int)fb_width; x++) {
                    if (x >= 0) fb[y * pitch + x] = highlightColor;
                }
            }
            /* Left highlight line */
            int x = boxLeft + 1;
            if (x >= 0 && x < (int)fb_width) {
                for (y = boxTop + 2; y < boxTop + boxSize - 2 && y < (int)fb_height; y++) {
                    if (y >= 0) fb[y * pitch + x] = highlightColor;
                }
            }
            /* Right highlight line */
            x = boxLeft + boxSize - 2;
            if (x >= 0 && x < (int)fb_width) {
                for (y = boxTop + 1; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                    if (y >= 0) fb[y * pitch + x] = highlightColor;
                }
            }
            /* Bottom highlight line */
            y = boxTop + boxSize - 2;
            if (y >= 0 && y < (int)fb_height) {
                for (x = boxLeft + 1; x < boxLeft + boxSize - 1 && x < (int)fb_width; x++) {
                    if (x >= 0) fb[y * pitch + x] = highlightColor;
                }
            }

            /* Fill interior with solid grey (reduced by 1px on bottom and right for shadow) */
            for (y = boxTop + 2; y < boxTop + boxSize - 3 && y < (int)fb_height; y++) {
                if (y >= 0) {
                    for (x = boxLeft + 2; x < boxLeft + boxSize - 3 && x < (int)fb_width; x++) {
                        if (x >= 0) fb[y * pitch + x] = grey;
                    }
                }
            }

            /* Draw black 3D shadow on bottom and right edges of grey */
            /* Bottom shadow */
            y = boxTop + boxSize - 3;
            if (y >= 0 && y < (int)fb_height) {
                for (x = boxLeft + 2; x < boxLeft + boxSize - 2 && x < (int)fb_width; x++) {
                    if (x >= 0) fb[y * pitch + x] = black;
                }
            }
            /* Right shadow */
            x = boxLeft + boxSize - 3;
            if (x >= 0 && x < (int)fb_width) {
                for (y = boxTop + 2; y < boxTop + boxSize - 3 && y < (int)fb_height; y++) {
                    if (y >= 0) fb[y * pitch + x] = black;
                }
            }

            /* Draw light grey separator columns on either side of close box
             * This separates the teal highlight from the dark grey horizontal stripes */
            uint32_t lightGrey = 0xFFE0E0E0;  /* Match title bar background */

            /* Left separator column - same height as black outline */
            int sepX = boxLeft - 1;
            if (sepX >= 0 && sepX < (int)fb_width) {
                for (y = boxTop; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                    if (y >= 0) fb[y * pitch + sepX] = lightGrey;
                }
            }

            /* Right separator column - right against the close box */
            sepX = boxLeft + boxSize - 1;
            if (sepX >= 0 && sepX < (int)fb_width) {
                for (y = boxTop; y < boxTop + boxSize - 1 && y < (int)fb_height; y++) {
                    if (y >= 0) fb[y * pitch + sepX] = lightGrey;
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
                extern void PaintRoundRect(const Rect* r, short ovalWidth, short ovalHeight);
                extern void FrameRoundRect(const Rect* r, short ovalWidth, short ovalHeight);
                extern void InsetRect(Rect* r, short dh, short dv);

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
                short closeRight = frame.left + 4 + 14;  /* close box: 4 inset + 14 size */
                short zoomLeft = frame.right - 4 - 14;   /* zoom box (if present) */

                if (loz.left < closeRight + ctrlPad) loz.left = closeRight + ctrlPad;
                if (loz.right > zoomLeft - ctrlPad) loz.right = zoomLeft - ctrlPad;

                if (window->hilited) {
                    /* Active window: draw rectangular area behind text to cover stripes */

                    /* Fill rectangular lozenge with grey at framebuffer level */
                    if (framebuffer) {
                        uint32_t* fb = (uint32_t*)framebuffer;
                        int pitch = fb_pitch / 4;
                        uint32_t lightGrey = 0xFFE8E8E8;  /* Same as title bar background */

                        /* Simple rectangular fill to cleanly cover stripes */
                        for (int y = loz.top; y < loz.bottom; y++) {
                            if (y >= 0 && y < (int)fb_height) {
                                for (int x = loz.left; x < loz.right; x++) {
                                    if (x >= 0 && x < (int)fb_width) {
                                        fb[y * pitch + x] = lightGrey;
                                    }
                                }
                            }
                        }
                    }

                    /* Draw title text in normal black */
                    WM_LOG_TRACE("*** CODE PATH B: DrawWindowFrame in WindowDisplay.c ***\n");
                    WM_LOG_TRACE("TITLE: About to draw title, titleStr=%p\n", titleStr);
                    PenPat(&qd.black);
                    ForeColor(blackColor);  /* Ensure black text */
                    TextFace(0);  /* normal */
                    MoveTo(textLeft, textBaseline);
                    WM_LOG_TRACE("TITLE: Calling DrawString now\n");
                    DrawString(titleStr);
                    WM_LOG_TRACE("TITLE: DrawString returned\n");
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
    } else {
        WM_LOG_TRACE("WindowManager: titleWidth is 0, skipping title bar\n");
    }

    SetPort(savePort);
}

static void DrawWindowControls(WindowPtr window) {
    if (!window || !window->visible) return;

    /* Set up WMgr port for global coordinate drawing */
    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);
    SetPort(wmgrPort);

    /* Set up pen for drawing black controls */
    extern void PenNormal(void);
    extern void PenSize(short width, short height);
    static const Pattern blackPat = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    PenNormal();
    PenPat(&blackPat);
    PenSize(1, 1);

    /* CRITICAL: Use global coordinates from strucRgn, not local portRect */
    Rect frame;
    if (window->strucRgn && *window->strucRgn) {
        frame = (*window->strucRgn)->rgnBBox;
    } else {
        /* Fallback to portRect if strucRgn not set */
        frame = window->port.portRect;
    }

    /* Close box is drawn in DrawWindowFrame, not here */

    /* Draw zoom box */
    if (window->spareFlag) {
        Rect zoomBox;
        SetRect(&zoomBox, frame.right - 20, frame.top + 4,
                frame.right - 8, frame.top + 16);
        FrameRect(&zoomBox);

        if (window->hilited) {
            /* Draw zoom box lines */
            Rect innerBox = zoomBox;
            InsetRect(&innerBox, 2, 2);
            FrameRect(&innerBox);
        }
    }

    /* Draw grow box */
    if (window->windowKind >= 0) {  /* Document window */
        /* Grow box in bottom-right corner */
        Rect growBox;
        SetRect(&growBox, frame.right - 16, frame.bottom - 16,
                frame.right, frame.bottom);

        extern void* framebuffer;
        extern uint32_t fb_width, fb_height, fb_pitch;

        if (framebuffer) {
            uint32_t* fb = (uint32_t*)framebuffer;
            int pitch = fb_pitch / 4;
            uint32_t black = 0xFF000000;

            /* Draw three diagonal lines from bottom-left to top-right */
            /* Line 1: Full diagonal */
            for (int i = 0; i < 16; i++) {
                int x = growBox.left + i;
                int y = growBox.bottom - 1 - i;
                if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
                    fb[y * pitch + x] = black;
                }
            }

            /* Line 2: Offset by 4 pixels */
            for (int i = 0; i < 12; i++) {
                int x = growBox.left + 4 + i;
                int y = growBox.bottom - 1 - i;
                if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
                    fb[y * pitch + x] = black;
                }
            }

            /* Line 3: Offset by 8 pixels */
            for (int i = 0; i < 8; i++) {
                int x = growBox.left + 8 + i;
                int y = growBox.bottom - 1 - i;
                if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
                    fb[y * pitch + x] = black;
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
    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* Draw chrome in WMgr port */
    SetPort(wmgrPort);
    DrawWindowFrame(window);
    DrawWindowControls(window);

    /* Switch to window port for content */
    SetPort((GrafPtr)window);

    /* Fill content area - LOCAL coords */
    Rect contentRect = window->port.portRect;
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

    /* Draw grow icon in bottom-right corner */
    Rect frame = window->port.portRect;
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

void ShowWindow(WindowPtr window) {
    WM_LOG_TRACE("ShowWindow: ENTRY, window=%p\n", window);

    if (!window || window->visible) {
        WM_LOG_TRACE("ShowWindow: Early return (window=%p, visible=%d)\n", window, window ? window->visible : -1);
        return;
    }

    WM_DEBUG("ShowWindow: Making window visible");
    WM_LOG_TRACE("ShowWindow: About to set visible=true\n");

    window->visible = true;

    /* Calculate window regions (structure and content) */
    WM_LOG_TRACE("ShowWindow: Calculating window regions\n");
    extern void WM_CalculateStandardWindowRegions(WindowPtr window, short varCode);
    WM_CalculateStandardWindowRegions(window, 0);

    /* Calculate visible region */
    WM_LOG_TRACE("ShowWindow: About to call CalcVis\n");
    CalcVis(window);

    /* CRITICAL: Redraw desktop icons BEFORE painting window to ensure icons appear behind window */
    extern DeskHookProc g_deskHook;
    if (g_deskHook && window->strucRgn) {
        extern void serial_puts(const char* str);
        serial_puts("[SHOWWIN] Redrawing desktop icons before window\n");

        /* Create region for area under window */
        extern RgnHandle NewRgn(void);
        extern void DisposeRgn(RgnHandle rgn);
        RgnHandle windowRgn = NewRgn();
        if (windowRgn) {
            CopyRgn(window->strucRgn, windowRgn);
            g_deskHook(windowRgn);  /* Redraw desktop icons in this region */
            DisposeRgn(windowRgn);
        }
    }

    /* Paint the window */
    extern void serial_puts(const char* str);
    serial_puts("[SHOWWIN] About to call PaintOne for chrome\n");
    WM_LOG_TRACE("ShowWindow: About to call PaintOne\n");
    PaintOne(window, NULL);
    WM_LOG_TRACE("ShowWindow: PaintOne returned\n");
    serial_puts("[SHOWWIN] PaintOne returned, chrome should be visible\n");

    /* Invalidate content region to generate update event for application to draw content */
    if (window->contRgn) {
        WM_LOG_TRACE("ShowWindow: Invalidating content region to trigger update event\n");
        extern void InvalRgn(RgnHandle badRgn);
        extern void SetPort(GrafPtr port);

        /* InvalRgn operates on current port, so set port to window first */
        GrafPtr savePort;
        GetPort(&savePort);
        SetPort((GrafPtr)window);

        /* CRITICAL FIX: COPY contRgn to clipRgn, don't ALIAS the handles!
         *
         * BUG: Previous code did `clipRgn = contRgn` which made both handles point to
         * the SAME region. Later code calling ClipRect(&qd.screenBits.bounds) would
         * overwrite clipRgn, which accidentally overwrote contRgn to (0,0,800,600)!
         *
         * FIX: Use CopyRgn to copy region DATA, keeping handles separate.
         * This prevents content from overdrawing chrome while preserving contRgn. */
        CopyRgn(window->contRgn, window->port.clipRgn);

        InvalRgn(window->contRgn);

        /* WORKAROUND: Directly draw folder window content since update events may not flow yet */
        if (window->refCon == 0x4449534b || window->refCon == 0x54525348) {  /* 'DISK' or 'TRSH' */
            extern void FolderWindow_Draw(WindowPtr window);
            FolderWindow_Draw(window);
        }

        SetPort(savePort);
    }

    /* Recalculate regions for windows behind */
    serial_puts("[SHOWWIN] About to CalcVisBehind\n");
    CalcVisBehind(window->nextWindow, window->strucRgn);
    serial_puts("[SHOWWIN] CalcVisBehind returned\n");

    /* Don't call PaintBehind here - background windows are already painted.
     * Calling PaintBehind would cause background windows to paint over the front window. */

    serial_puts("[SHOWWIN] EXIT - window should be fully visible now\n");
    WM_LOG_TRACE("ShowWindow: EXIT\n");
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

void HideWindow(WindowPtr window) {
    WM_LOG_DEBUG("[WM] HideWindow: ENTRY\n");

    if (!window) {
        WM_LOG_DEBUG("[WM] HideWindow: NULL window, returning\n");
        return;
    }

    WM_LOG_DEBUG("[WM] HideWindow: Setting visible=false\n");
    window->visible = false;

    /* Save the region that needs repainting */
    WM_LOG_DEBUG("[WM] HideWindow: About to declare clobberedRgn\n");
    RgnHandle clobberedRgn = NULL;
    WM_LOG_DEBUG("[WM] HideWindow: clobberedRgn declared\n");
    WM_LOG_DEBUG("[WM] HideWindow: window pointer = 0x%08x\n", (unsigned int)P2UL(window));
    WM_LOG_DEBUG("[WM] HideWindow: &(window->strucRgn) = 0x%08x\n", (unsigned int)P2UL(&(window->strucRgn)));
    WM_LOG_DEBUG("[WM] HideWindow: About to read window->strucRgn value...\n");
    RgnHandle strucRgn_value = window->strucRgn;
    WM_LOG_DEBUG("[WM] HideWindow: strucRgn value = 0x%08x\n", (unsigned int)P2UL(strucRgn_value));
    WM_LOG_DEBUG("[WM] HideWindow: About to check if strucRgn is NULL\n");
    if (strucRgn_value) {
        WM_LOG_DEBUG("[WM] HideWindow: strucRgn is NOT NULL, calling NewRgn()\n");
        clobberedRgn = NewRgn();
        WM_LOG_DEBUG("[WM] HideWindow: NewRgn returned 0x%08x\n", (unsigned int)P2UL(clobberedRgn));
        if (clobberedRgn) {
            WM_DEBUG("HideWindow: Calling CopyRgn()");
            CopyRgn(strucRgn_value, clobberedRgn);
            WM_DEBUG("HideWindow: CopyRgn returned");
        }
    }

    /* Erase the window's area with desktop pattern FIRST */
    WM_DEBUG("HideWindow: About to erase region, clobberedRgn=0x%08x", (unsigned int)P2UL(clobberedRgn));
    if (clobberedRgn) {
        extern void EraseRgn(RgnHandle rgn);
        extern void GetWMgrPort(GrafPtr* port);
        extern void SetPort(GrafPtr port);
        extern void GetPort(GrafPtr* port);

        GrafPtr savePort, wmPort;
        WM_DEBUG("HideWindow: Calling GetPort()");
        GetPort(&savePort);
        WM_DEBUG("HideWindow: GetPort returned, savePort=0x%08x", (unsigned int)P2UL(savePort));

        WM_DEBUG("HideWindow: Calling GetWMgrPort()");
        GetWMgrPort(&wmPort);
        WM_DEBUG("HideWindow: GetWMgrPort returned, wmPort=0x%08x", (unsigned int)P2UL(wmPort));

        if (wmPort) {
            WM_DEBUG("HideWindow: Calling SetPort(wmPort)");
            SetPort(wmPort);  /* Set to desktop port for erasing */
            WM_DEBUG("HideWindow: SetPort returned");
        }

        WM_DEBUG("HideWindow: Calling EraseRgn()");
        EraseRgn(clobberedRgn);
        WM_DEBUG("HideWindow: EraseRgn returned");

        WM_DEBUG("HideWindow: Restoring port");
        SetPort(savePort);  /* Restore previous port */
        WM_DEBUG("HideWindow: Port restored");
    }

    /* Recalculate visible regions */
    WM_DEBUG("HideWindow: Calling CalcVisBehind()");
    CalcVisBehind(window->nextWindow, clobberedRgn);
    WM_DEBUG("HideWindow: CalcVisBehind returned");

    /* Repaint windows behind */
    WM_DEBUG("HideWindow: Calling PaintBehind()");
    PaintBehind(window->nextWindow, clobberedRgn);
    WM_DEBUG("HideWindow: PaintBehind returned");

    if (clobberedRgn) {
        WM_DEBUG("HideWindow: Calling DisposeRgn()");
        DisposeRgn(clobberedRgn);
        WM_DEBUG("HideWindow: DisposeRgn returned");
    }

    WM_DEBUG("HideWindow: RETURN");
}

void ShowHide(WindowPtr window, Boolean showFlag) {
    if (showFlag) {
        ShowWindow(window);
    } else {
        HideWindow(window);
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
    serial_puts("[MEM] BringToFront start\n");
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

    /* CRITICAL: Repaint entire window stack from back to front to ensure proper z-order */
    PaintBehind(NULL, NULL);
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
    PaintBehind(window, window->strucRgn);

    DumpWindowList("SendBehind - END");
}

/*-----------------------------------------------------------------------*/
/* Window Selection Functions                                           */
/*-----------------------------------------------------------------------*/

void SelectWindow(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("SelectWindow: Selecting window");
    DumpWindowList("SelectWindow - START");

    /* Bring window to front */
    BringToFront(window);

    /* Generate activate event */
    /* This would post an activateEvt to the event queue */
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

/* FindWindow - Determine which part of the screen was clicked */
/* [WM-050] FindWindow canonical implementation in WindowEvents.c - removed incomplete duplicate */

/*-----------------------------------------------------------------------*/
/* Desktop Hook and Display Update Functions                            */
/*-----------------------------------------------------------------------*/

/* DeskHook type definition if not in headers */
typedef void (*DeskHookProc)(RgnHandle invalidRgn);

/* DeskHook support */
DeskHookProc g_deskHook = NULL;  /* Non-static so WindowDragging.c can access it */

/* Tracks if display needs updating to avoid constant flashing */
static Boolean gDisplayDirty = true;
static int gLastWindowCount = -1;
static int gUpdateThrottle = 0;

void SetDeskHook(DeskHookProc proc) {
    g_deskHook = proc;
}

/* Mark display as needing update when windows change */
static void WM_InvalidateDisplay(void) {
    gDisplayDirty = true;
}

/* Public function to mark display as dirty (used by AppSwitcher and others) */
void WM_InvalidateDisplay_Public(void) {
    gDisplayDirty = true;
}

/* Window Manager update pipeline functions */
/* WM_Update is needed by main.c even when other stubs are disabled */
void WM_Update(void) {
    /* DISABLE throttling to ensure immediate updates */
    extern void serial_puts(const char* str);
    static int update_count = 0;
    if (update_count < 3) {
        serial_puts("[WMUPDATE] WM_Update called\n");
        update_count++;
    }

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

    /* Check if window count has changed */
    int currentWindowCount = 0;
    {
        extern WindowPtr FrontWindow(void);
        WindowPtr w = FrontWindow();
        while (w) {
            currentWindowCount++;
            w = w->nextWindow;
        }
    }

    /* Skip full redraw if nothing changed */
    if (!gDisplayDirty && currentWindowCount == gLastWindowCount) {
        return;
    }

    gLastWindowCount = currentWindowCount;
    gDisplayDirty = false;

    /* Use QuickDraw to draw desktop */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(qd.thePort);  /* Draw to screen port */

    /* 1. Draw desktop pattern first */
    Rect desktopRect;
    SetRect(&desktopRect, 0, 20,
            qd.screenBits.bounds.right,
            qd.screenBits.bounds.bottom);
    FillRect(&desktopRect, &qd.gray);  /* Gray desktop pattern */

    /* 2. Call DeskHook BEFORE windows to draw desktop icons behind windows */
    if (g_deskHook) {
        /* Create a region for the desktop */
        RgnHandle desktopRgn = NewRgn();
        extern void RectRgn(RgnHandle rgn, const Rect* r);
        RectRgn(desktopRgn, &desktopRect);
        g_deskHook(desktopRgn);
        DisposeRgn(desktopRgn);
    }

    /* 3. Draw all visible windows on top of desktop icons */
    /* Use Window Manager's PaintOne to properly render windows */
    {
        extern WindowPtr FrontWindow(void);
        extern void PaintOne(WindowPtr window, RgnHandle clobberedRgn);

        /* Build window list (back to front order) */
        WindowPtr window = FrontWindow();
        WindowPtr* windowStack = NULL;
        int windowCount = currentWindowCount;

        /* Allocate and fill window stack */
        if (windowCount > 0) {
            windowStack = (WindowPtr*)NewPtr(windowCount * sizeof(WindowPtr));
            if (windowStack) {
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
    /* Old cursor drawing code removed to prevent double cursor issue */

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
        extern void InvalRect(const Rect* rect);
        GrafPort* port = (GrafPort*)window;
        InvalRect(&port->portRect);
    }
}

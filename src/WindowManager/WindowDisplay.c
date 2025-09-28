#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "ControlManager/ControlTypes.h"

/* Debug macros */
#ifdef DEBUG_WINDOW_MANAGER
    #define WM_DEBUG(...) serial_printf("WM: " __VA_ARGS__)
#else
    #define WM_DEBUG(...)
#endif

/* External functions */
extern void serial_printf(const char* fmt, ...);
extern void DrawString(const unsigned char* str);
extern void LineTo(short h, short v);
extern void MoveTo(short h, short v);
extern void FrameRect(const Rect* r);
extern void PaintRect(const Rect* r);
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

/*-----------------------------------------------------------------------*/
/* Window Display Functions                                             */
/*-----------------------------------------------------------------------*/

/* Check windows for update events (called by GetNextEvent) */
void CheckWindowsNeedingUpdate(void) {
    extern SInt16 PostEvent(SInt16 eventNum, SInt32 eventMsg);
    extern WindowPtr FrontWindow(void);
    extern Boolean EmptyRgn(RgnHandle rgn);
    extern void serial_printf(const char* fmt, ...);

    static int call_count = 0;
    call_count++;

    /* Walk all visible windows and post update events for windows with non-empty updateRgn */
    WindowPtr window = FrontWindow();

    if (call_count <= 10 || (call_count % 500) == 0) {
        serial_printf("CheckWindowsNeedingUpdate: #%d, frontWindow=0x%08x\n", call_count, (unsigned int)window);
    }

    int windowCount = 0;
    while (window) {
        windowCount++;
        Boolean hasUpdateRgn = (window->updateRgn != NULL);
        Boolean isEmpty = hasUpdateRgn ? EmptyRgn(window->updateRgn) : true;

        if (call_count <= 10 || (call_count % 500) == 0) {
            serial_printf("CheckWindowsNeedingUpdate:   Window %d: 0x%08x, visible=%d, updateRgn=0x%08x, empty=%d\n",
                         windowCount, (unsigned int)window, window->visible, (unsigned int)window->updateRgn, isEmpty);
            if (hasUpdateRgn) {
                Region* rgn = *(window->updateRgn);
                serial_printf("CheckWindowsNeedingUpdate:     updateRgn bbox=(%d,%d,%d,%d)\n",
                             rgn->rgnBBox.left, rgn->rgnBBox.top, rgn->rgnBBox.right, rgn->rgnBBox.bottom);
            }
        }

        if (window->visible && window->updateRgn && !EmptyRgn(window->updateRgn)) {
            serial_printf("CheckWindowsNeedingUpdate: Posting update event for window 0x%08x\n", (unsigned int)window);
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
    extern void serial_printf(const char* fmt, ...);
    serial_printf("PaintOne: ENTRY, window=%p, visible=%d\n", window, window ? window->visible : -1);

    if (!window || !window->visible) {
        serial_printf("PaintOne: Early return\n");
        return;
    }

    WM_DEBUG("PaintOne: Painting window");
    serial_printf("PaintOne: About to GetPort/SetPort\n");

    /* Save current port */
    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* Draw chrome in WMgr port (global coordinates) */
    SetPort(wmgrPort);
    serial_printf("PaintOne: Drawing window chrome in WMgr port\n");
    DrawWindowFrame(window);
    DrawWindowControls(window);

    /* Switch to window port for content (local coordinates) */
    SetPort((GrafPtr)window);
    serial_printf("PaintOne: Switched to window port for content\n");

    /* Fill content area - use LOCAL coords (0,0,width,height) */
    Rect contentRect = {0, 0, window->port.portRect.right, window->port.portRect.bottom};

    serial_printf("PaintOne: Content rect (local) = (%d,%d,%d,%d)\n",
        contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);

    /* Fill with white to make opaque */
    EraseRect(&contentRect);
    serial_printf("PaintOne: Content erased\n");

    /* Window Manager draws chrome only - content is application's job */
    /* Application must draw content via BeginUpdate/EndUpdate in update event handler */

    SetPort(savePort);
    serial_printf("PaintOne: EXIT\n");
}

void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn) {
    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WM_DEBUG("PaintBehind: Starting paint");

    /* Find start position in window list */
    WindowPtr window = startWindow;
    if (!window) {
        window = wmState->windowList;
    }

    /* Paint windows from back to front */
    while (window) {
        if (window->visible) {
            PaintOne(window, clobberedRgn);
        }
        window = window->nextWindow;
    }
}

void CalcVis(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("CalcVis: Calculating visible region");

    /* Start with the window's content region */
    if (window->contRgn && window->visRgn) {
        CopyRgn(window->contRgn, window->visRgn);

        /* Subtract regions of windows in front */
        WindowPtr frontWindow = FrontWindow();
        while (frontWindow && frontWindow != window) {
            if (frontWindow->visible && frontWindow->strucRgn) {
                /* Would subtract frontWindow->strucRgn from window->visRgn */
                /* For now, simplified implementation */
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

    extern void serial_printf(const char* fmt, ...);
    serial_printf("DrawNew: ENTRY, window=%p\n", window);

    WM_DEBUG("DrawNew: Drawing window");

    /* Save current port */
    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);

    /* Draw chrome in WMgr port */
    SetPort(wmgrPort);
    serial_printf("DrawNew: Drawing frame\n");
    DrawWindowFrame(window);
    DrawWindowControls(window);

    /* Switch to window port for content */
    SetPort((GrafPtr)window);

    if (update && window->updateRgn) {
        /* Only draw the update region */
        SetClip(window->updateRgn);
    }

    /* Fill content area - LOCAL coords */
    Rect contentRect = {0, 0, window->port.portRect.right, window->port.portRect.bottom};
    serial_printf("DrawNew: Filling content rect (local) (%d,%d,%d,%d)\n",
        contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);
    EraseRect(&contentRect);
    serial_printf("DrawNew: Content filled\n");

    SetPort(savePort);
    serial_printf("DrawNew: EXIT\n");
}

static void DrawWindowFrame(WindowPtr window) {
    if (!window || !window->visible) return;

    /* CRITICAL: strucRgn must be set to draw the window
     * strucRgn contains GLOBAL screen coordinates
     * portRect contains LOCAL coordinates (0,0,width,height) and must NEVER be used for positioning! */
    if (!window->strucRgn || !*window->strucRgn) {
        serial_printf("WindowManager: DrawWindowFrame - strucRgn not set, cannot draw\n");
        return;
    }

    extern void GetWMgrPort(GrafPtr* port);
    GrafPtr savePort, wmgrPort;
    GetPort(&savePort);
    GetWMgrPort(&wmgrPort);
    SetPort(wmgrPort);

    serial_printf("WindowManager: DrawWindowFrame START\n");

    /* Get window's global bounds from structure region */
    Rect frame = (*window->strucRgn)->rgnBBox;

    serial_printf("WindowManager: Frame rect (%d,%d,%d,%d)\n",
                  frame.left, frame.top, frame.right, frame.bottom);

    /* Draw frame outline first */
    FrameRect(&frame);

    /* Draw title bar BEFORE filling content area */
    serial_printf("WindowManager: About to check titleWidth=%d\n", window->titleWidth);

    if (window->titleWidth > 0) {
        Rect titleBar = frame;
        titleBar.bottom = titleBar.top + 20;

        /* Fill title bar with white background */
        EraseRect(&titleBar);

        /* Draw title bar separator */
        MoveTo(frame.left, frame.top + 20);
        LineTo(frame.right - 1, frame.top + 20);

        /* Draw close box - simple square at left side */
        Rect closeBox;
        SetRect(&closeBox, frame.left + 4, frame.top + 4, frame.left + 14, frame.top + 14);
        FrameRect(&closeBox);

        /* Draw window title */
        if (window->titleHandle) {
            MoveTo(frame.left + 20, frame.top + 15);
            DrawString((unsigned char*)*window->titleHandle);
        }
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

    /* CRITICAL: Use global coordinates from strucRgn, not local portRect */
    Rect frame;
    if (window->strucRgn && *window->strucRgn) {
        frame = (*window->strucRgn)->rgnBBox;
    } else {
        /* Fallback to portRect if strucRgn not set */
        frame = window->port.portRect;
    }

    /* Draw close box */
    if (window->goAwayFlag) {
        Rect closeBox;
        SetRect(&closeBox, frame.left + 8, frame.top + 4,
                frame.left + 20, frame.top + 16);
        FrameRect(&closeBox);

        if (window->hilited) {
            /* Draw close box X */
            MoveTo(closeBox.left + 2, closeBox.top + 2);
            LineTo(closeBox.right - 3, closeBox.bottom - 3);
            MoveTo(closeBox.right - 3, closeBox.top + 2);
            LineTo(closeBox.left + 2, closeBox.bottom - 3);
        }
    }

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

        /* Draw grow lines */
        MoveTo(growBox.left, growBox.bottom - 1);
        LineTo(growBox.right - 1, growBox.top);
        MoveTo(growBox.left + 4, growBox.bottom - 1);
        LineTo(growBox.right - 1, growBox.top + 4);
        MoveTo(growBox.left + 8, growBox.bottom - 1);
        LineTo(growBox.right - 1, growBox.top + 8);
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
        serial_printf("WindowManager: DrawWindow - window NULL or not visible\n");
        return;
    }

    serial_printf("WindowManager: DrawWindow ENTRY for window '%s'\n",
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
    Rect contentRect = {0, 0, window->port.portRect.right, window->port.portRect.bottom};
    serial_printf("DrawWindow: Filling content rect (local) (%d,%d,%d,%d)\n",
        contentRect.left, contentRect.top, contentRect.right, contentRect.bottom);
    EraseRect(&contentRect);
    serial_printf("DrawWindow: Content filled\n");

    SetPort(savePort);
    serial_printf("DrawWindow: EXIT\n");
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
    extern void serial_printf(const char* fmt, ...);
    serial_printf("ShowWindow: ENTRY, window=%p\n", window);

    if (!window || window->visible) {
        serial_printf("ShowWindow: Early return (window=%p, visible=%d)\n", window, window ? window->visible : -1);
        return;
    }

    WM_DEBUG("ShowWindow: Making window visible");
    serial_printf("ShowWindow: About to set visible=true\n");

    window->visible = true;

    /* Calculate window regions (structure and content) */
    serial_printf("ShowWindow: Calculating window regions\n");
    extern void WM_CalculateStandardWindowRegions(WindowPtr window, short varCode);
    WM_CalculateStandardWindowRegions(window, 0);

    /* Calculate visible region */
    serial_printf("ShowWindow: About to call CalcVis\n");
    CalcVis(window);

    /* Paint the window */
    serial_printf("ShowWindow: About to call PaintOne\n");
    PaintOne(window, NULL);
    serial_printf("ShowWindow: PaintOne returned\n");

    /* Invalidate content region to generate update event for application to draw content */
    if (window->contRgn) {
        serial_printf("ShowWindow: Invalidating content region to trigger update event\n");
        extern void InvalRgn(RgnHandle badRgn);
        extern void SetPort(GrafPtr port);

        /* InvalRgn operates on current port, so set port to window first */
        GrafPtr savePort;
        GetPort(&savePort);
        SetPort((GrafPtr)window);
        InvalRgn(window->contRgn);
        SetPort(savePort);
    }

    /* Recalculate regions for windows behind */
    CalcVisBehind(window->nextWindow, window->strucRgn);
    serial_printf("ShowWindow: EXIT\n");
}

void HideWindow(WindowPtr window) {
    if (!window || !window->visible) return;

    WM_DEBUG("HideWindow: Hiding window");

    window->visible = false;

    /* Save the region that needs repainting */
    RgnHandle clobberedRgn = NULL;
    if (window->strucRgn) {
        clobberedRgn = NewRgn();
        if (clobberedRgn) {
            CopyRgn(window->strucRgn, clobberedRgn);
        }
    }

    /* Recalculate visible regions */
    CalcVisBehind(window->nextWindow, clobberedRgn);

    /* Repaint windows behind */
    PaintBehind(window->nextWindow, clobberedRgn);

    if (clobberedRgn) {
        DisposeRgn(clobberedRgn);
    }
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
    if (!window || window->hilited == fHilite) return;

    WM_DEBUG("HiliteWindow: Setting hilite to %d", fHilite);

    window->hilited = fHilite;

    /* Redraw the window frame to show highlight state */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    DrawWindowFrame(window);
    DrawWindowControls(window);

    SetPort(savePort);
}

/*-----------------------------------------------------------------------*/
/* Window Ordering Functions                                            */
/*-----------------------------------------------------------------------*/

void BringToFront(WindowPtr window) {
    if (!window) return;

    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState || wmState->windowList == window) return;

    WM_DEBUG("BringToFront: Moving window to front");

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
    }

    /* Add to front */
    window->nextWindow = wmState->windowList;
    wmState->windowList = window;

    /* Update highlight state */
    HiliteWindow(window, true);

    /* Unhighlight previous front window */
    if (window->nextWindow) {
        HiliteWindow(window->nextWindow, false);
    }

    /* Recalculate visible regions */
    CalcVisBehind(window, NULL);

    /* Redraw if needed */
    PaintOne(window, NULL);
}

void SendBehind(WindowPtr window, WindowPtr behindWindow) {
    if (!window) return;

    WindowManagerState* wmState = GetWindowManagerState();
    if (!wmState) return;

    WM_DEBUG("SendBehind: Moving window behind another");

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
}

/*-----------------------------------------------------------------------*/
/* Window Selection Functions                                           */
/*-----------------------------------------------------------------------*/

void SelectWindow(WindowPtr window) {
    if (!window) return;

    WM_DEBUG("SelectWindow: Selecting window");

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
        serial_printf("WindowManager: FrontWindow - wmState is NULL\n");
        return NULL;
    }

    static int call_count = 0;
    call_count++;

    /* Find first visible window */
    WindowPtr window = wmState->windowList;

    if (call_count <= 5 || (call_count % 5000) == 0) {
        serial_printf("WindowManager: FrontWindow #%d - list head=%p\n", call_count, window);
    }

    int count = 0;
    while (window) {
        if (call_count <= 5 || (call_count % 5000) == 0) {
            serial_printf("WindowManager: FrontWindow - checking window %p, visible=%d\n", window, window->visible);
        }
        if (window->visible) {
            if (call_count <= 5 || (call_count % 5000) == 0) {
                serial_printf("WindowManager: FrontWindow - returning visible window %p\n", window);
            }
            return window;
        }
        window = window->nextWindow;
        count++;
        if (count > 100) {
            serial_printf("WindowManager: FrontWindow - LOOP DETECTED, breaking\n");
            break;
        }
    }

    if (call_count <= 5 || (call_count % 5000) == 0) {
        serial_printf("WindowManager: FrontWindow - returning NULL (no visible window found)\n");
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
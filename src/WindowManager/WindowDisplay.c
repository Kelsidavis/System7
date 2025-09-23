/* #include "SystemTypes.h" */

#include "WindowManager/WindowManager.h"
/* #include "WindowManager/WindowManagerPrivate.h" */
#include "QuickDraw/QuickDraw.h"
/* #include "QuickDraw/QuickDrawPrivate.h" */
#include "Platform/WindowPlatform.h"
#include "WindowManager/ControlManagerTypes.h"

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
extern RgnHandle GetClip(void);
extern RgnHandle NewRgn(void);
extern void DisposeRgn(RgnHandle rgn);

/*-----------------------------------------------------------------------*/
/* Window Display Functions                                             */
/*-----------------------------------------------------------------------*/

/* Internal helper to draw window frame */
static void DrawWindowFrame(WindowPtr window);

/* Internal helper to draw window controls */
static void DrawWindowControls(WindowPtr window);

/* Internal helper to draw window content area */
static void DrawWindowContent(WindowPtr window);

void PaintOne(WindowPtr window, RgnHandle clobberedRgn) {
    if (!window || !window->visible) return;

    WM_DEBUG("PaintOne: Painting window");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(&window->port);

    /* Draw window frame */
    DrawWindowFrame(window);

    /* Draw window controls */
    DrawWindowControls(window);

    /* Draw window content */
    DrawWindowContent(window);

    SetPort(savePort);
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

    WM_DEBUG("DrawNew: Drawing window");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(&window->port);

    if (update && window->updateRgn) {
        /* Only draw the update region */
        SetClip(window->updateRgn);
    }

    DrawWindowFrame(window);
    DrawWindowControls(window);
    DrawWindowContent(window);

    if (update && window->updateRgn) {
        /* Clear the update region */
        SetRectRgn(window->updateRgn, 0, 0, 0, 0);
    }

    SetPort(savePort);
}

static void DrawWindowFrame(WindowPtr window) {
    if (!window || !window->visible) return;

    /* Draw window frame based on window definition */
    Rect frame = window->port.portRect;

    /* Draw outer frame */
    FrameRect(&frame);

    /* Draw title bar */
    if (window->titleWidth > 0) {
        Rect titleBar = frame;
        titleBar.bottom = titleBar.top + 20;

        if (window->hilited) {
            /* Active window - draw with pattern */
            Pattern stripes = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
            /* Would use FillRect with pattern */
            PaintRect(&titleBar);
        } else {
            /* Inactive window - solid white */
            EraseRect(&titleBar);
        }

        /* Draw title bar separator */
        MoveTo(frame.left, frame.top + 20);
        LineTo(frame.right - 1, frame.top + 20);

        /* Draw window title */
        if (window->titleHandle) {
            MoveTo(frame.left + 20, frame.top + 15);
            DrawString((unsigned char*)*window->titleHandle);
        }
    }
}

static void DrawWindowControls(WindowPtr window) {
    if (!window || !window->visible) return;

    Rect frame = window->port.portRect;

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
}

static void DrawWindowContent(WindowPtr window) {
    if (!window || !window->visible) return;

    /* Content drawing is handled by the application */
    /* Here we just ensure the content area is clipped properly */

    if (window->contRgn) {
        SetClip(window->contRgn);
    }

    /* Call the Finder's content drawing for folder windows */
    if (window->refCon == 'DISK' || window->refCon == 'TRSH') {
        extern void FolderWindowProc(WindowPtr window, short message, long param);
        FolderWindowProc(window, 0, 0);  /* wDraw = 0 */
    }
}

/* ============================================================================
 * Main Window Drawing Function - Draws Chrome Only
 * ============================================================================ */

void DrawWindow(WindowPtr window) {
    if (!window || !window->visible) return;

    serial_printf("WindowManager: Drawing chrome for window '%s'\n",
                  window->titleHandle ? (char*)*window->titleHandle : "Untitled");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(&window->port);

    /* Draw window chrome (frame, title bar, controls) */
    DrawWindowFrame(window);
    DrawWindowControls(window);

    /* Do NOT draw content - that's the application's responsibility */

    SetPort(savePort);
}

void DrawGrowIcon(WindowPtr window) {
    if (!window || !window->visible || window->windowKind < 0) return;

    WM_DEBUG("DrawGrowIcon: Drawing grow icon");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(&window->port);

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
    if (!window || window->visible) return;

    WM_DEBUG("ShowWindow: Making window visible");

    window->visible = true;

    /* Calculate visible region */
    CalcVis(window);

    /* Paint the window */
    PaintOne(window, NULL);

    /* Recalculate regions for windows behind */
    CalcVisBehind(window->nextWindow, window->strucRgn);
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
    SetPort(&window->port);

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
    if (!wmState) return NULL;

    /* Find first visible window */
    WindowPtr window = wmState->windowList;
    while (window) {
        if (window->visible) {
            return window;
        }
        window = window->nextWindow;
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
short FindWindow(Point thePoint, WindowPtr* theWindow) {
    extern void serial_printf(const char* fmt, ...);

    if (theWindow) {
        *theWindow = NULL;
    }

    /* Check menu bar first (top 20 pixels) */
    if (thePoint.v >= 0 && thePoint.v < 20) {
        serial_printf("FindWindow: Click in menu bar area at v=%d\n", thePoint.v);
        return inMenuBar;
    }

    /* Check for window hits */
    WindowPtr window = WM_FindWindowAt(thePoint);
    if (window) {
        if (theWindow) {
            *theWindow = window;
        }

        /* For now, assume all window clicks are in content */
        return inContent;
    }

    /* Default to desktop */
    return inDesk;
}
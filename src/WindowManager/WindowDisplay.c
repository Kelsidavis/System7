/*
 * WindowDisplay.c - Window Display and Visibility Management
 *
 * This file implements window display functions including showing/hiding,
 * window activation, title management, front/back ordering, and window
 * drawing coordination. These functions control how windows appear and
 * interact visually on the desktop.
 *
 * Key functions implemented:
 * - Window visibility (ShowWindow, HideWindow, ShowHide)
 * - Window activation (SelectWindow, HiliteWindow)
 * - Window ordering (BringToFront, SendBehind, FrontWindow)
 * - Window titles (SetWTitle, GetWTitle)
 * - Window drawing coordination
 * - Update region management
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#include "WindowManagerInternal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Window Title Management
 * ============================================================================ */

void SetWTitle(WindowPtr theWindow, ConstStr255Param title) {
    if (theWindow == NULL) return;

    WM_DEBUG("SetWTitle: Setting window title");

    /* Allocate title handle if it doesn't exist */
    if (theWindow->titleHandle == NULL) {
        theWindow->titleHandle = (StringHandle)malloc(sizeof(StringHandle));
        if (theWindow->titleHandle == NULL) {
            WM_ERROR("SetWTitle: Failed to allocate title handle");
            return;
        }
        *(theWindow->titleHandle) = NULL;
    }

    /* Calculate title length */
    short titleLen = (title && title[0] > 0) ? title[0] + 1 : 1;

    /* Allocate or reallocate title storage */
    if (*(theWindow->titleHandle) == NULL) {
        *(theWindow->titleHandle) = (unsigned char*)malloc(titleLen);
    } else {
        *(theWindow->titleHandle) = (unsigned char*)realloc(*(theWindow->titleHandle), titleLen);
    }

    if (*(theWindow->titleHandle) == NULL) {
        WM_ERROR("SetWTitle: Failed to allocate title storage");
        return;
    }

    /* Copy the title */
    if (title && title[0] > 0) {
        memcpy(*(theWindow->titleHandle), title, titleLen);
    } else {
        /* Empty title */
        (*(theWindow->titleHandle))[0] = 0;
    }

    /* Calculate title width for display */
    /* TODO: Use actual font metrics when text rendering is available */
    theWindow->titleWidth = (title && title[0] > 0) ? title[0] * 6 : 0;

    /* Update native window title */
    Platform_SetNativeWindowTitle(theWindow, title);

    /* Redraw title bar if window is visible */
    if (theWindow->visible) {
        WM_ScheduleWindowUpdate(theWindow, kUpdateTitle);
    }

    WM_DEBUG("SetWTitle: Title set successfully, width = %d", theWindow->titleWidth);
}

void GetWTitle(WindowPtr theWindow, Str255 title) {
    if (theWindow == NULL || title == NULL) {
        if (title) title[0] = 0; /* Empty string */
        return;
    }

    if (theWindow->titleHandle && *(theWindow->titleHandle)) {
        short len = (*(theWindow->titleHandle))[0];
        if (len > 255) len = 255;
        memcpy(title, *(theWindow->titleHandle), len + 1);
    } else {
        title[0] = 0; /* Empty string */
    }

    WM_DEBUG("GetWTitle: Retrieved title, length = %d", title[0]);
}

/* ============================================================================
 * Window Visibility Management
 * ============================================================================ */

void ShowWindow(WindowPtr theWindow) {
    if (theWindow == NULL) return;
    if (theWindow->visible) return; /* Already visible */

    WM_DEBUG("ShowWindow: Showing window");

    /* Mark window as visible */
    theWindow->visible = true;

    /* Show native platform window */
    Platform_ShowNativeWindow(theWindow, true);

    /* Recalculate window regions */
    Platform_CalculateWindowRegions(theWindow);

    /* Update visibility for all windows */
    WM_UpdateWindowVisibility(theWindow);

    /* If no window is currently active, activate this one */
    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState->activeWindow == NULL) {
        SelectWindow(theWindow);
    }

    /* Schedule complete redraw */
    WM_ScheduleWindowUpdate(theWindow, kUpdateAll);

    WM_DEBUG("ShowWindow: Window is now visible");
}

void HideWindow(WindowPtr theWindow) {
    if (theWindow == NULL) return;
    if (!theWindow->visible) return; /* Already hidden */

    WM_DEBUG("HideWindow: Hiding window");

    WindowManagerState* wmState = GetWindowManagerState();

    /* Mark window as invisible */
    theWindow->visible = false;

    /* Hide native platform window */
    Platform_ShowNativeWindow(theWindow, false);

    /* Invalidate the area the window was occupying */
    if (theWindow->strucRgn) {
        /* TODO: Invalidate screen area for redrawing windows below */
        WM_InvalidateWindowsBelow(theWindow, &theWindow->port.portRect);
    }

    /* If this was the active window, activate next visible window */
    if (wmState->activeWindow == theWindow) {
        wmState->activeWindow = NULL;

        /* Find next visible window to activate */
        WindowPtr nextWindow = WM_GetNextVisibleWindow(theWindow);
        if (nextWindow) {
            SelectWindow(nextWindow);
        }
    }

    /* Update visibility for all windows */
    WM_UpdateWindowVisibility(theWindow);

    WM_DEBUG("HideWindow: Window is now hidden");
}

void ShowHide(WindowPtr theWindow, Boolean showFlag) {
    if (showFlag) {
        ShowWindow(theWindow);
    } else {
        HideWindow(theWindow);
    }
}

/* ============================================================================
 * Window Activation and Highlighting
 * ============================================================================ */

void SelectWindow(WindowPtr theWindow) {
    if (theWindow == NULL) return;

    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState->activeWindow == theWindow) return; /* Already active */

    WM_DEBUG("SelectWindow: Activating window");

    /* Deactivate current window */
    if (wmState->activeWindow) {
        wmState->activeWindow->hilited = false;
        WM_ScheduleWindowUpdate(wmState->activeWindow, kUpdateFrame);
        WM_DEBUG("SelectWindow: Deactivated previous window");
    }

    /* Bring window to front */
    BringToFront(theWindow);

    /* Activate new window */
    wmState->activeWindow = theWindow;
    theWindow->hilited = true;

    /* Update Window Manager port reference */
    if (wmState->wMgrPort) {
        wmState->wMgrPort->activeWindow = theWindow;
    }

    /* Schedule frame redraw for highlight change */
    WM_ScheduleWindowUpdate(theWindow, kUpdateFrame);

    /* Post activation event */
    Platform_PostWindowEvent(theWindow, 1 /* activate */, 1);

    WM_DEBUG("SelectWindow: Window activated successfully");
}

void HiliteWindow(WindowPtr theWindow, Boolean fHilite) {
    if (theWindow == NULL) return;
    if (theWindow->hilited == fHilite) return; /* No change */

    WM_DEBUG("HiliteWindow: %s window", fHilite ? "Highlighting" : "Unhighlighting");

    theWindow->hilited = fHilite;

    /* Schedule frame redraw */
    WM_ScheduleWindowUpdate(theWindow, kUpdateFrame);

    /* Update active window state if needed */
    WindowManagerState* wmState = GetWindowManagerState();
    if (fHilite && wmState->activeWindow != theWindow) {
        wmState->activeWindow = theWindow;
        if (wmState->wMgrPort) {
            wmState->wMgrPort->activeWindow = theWindow;
        }
    } else if (!fHilite && wmState->activeWindow == theWindow) {
        wmState->activeWindow = NULL;
        if (wmState->wMgrPort) {
            wmState->wMgrPort->activeWindow = NULL;
        }
    }
}

/* ============================================================================
 * Window Ordering and Layering
 * ============================================================================ */

void BringToFront(WindowPtr theWindow) {
    if (theWindow == NULL) return;

    WindowManagerState* wmState = GetWindowManagerState();
    if (wmState->windowList == theWindow) return; /* Already at front */

    WM_DEBUG("BringToFront: Moving window to front");

    /* Remove from current position in list */
    if (wmState->windowList == theWindow) {
        wmState->windowList = theWindow->nextWindow;
    } else {
        WindowPtr current = wmState->windowList;
        while (current && current->nextWindow != theWindow) {
            current = current->nextWindow;
        }
        if (current) {
            current->nextWindow = theWindow->nextWindow;
        }
    }

    /* Add to front of list */
    theWindow->nextWindow = wmState->windowList;
    wmState->windowList = theWindow;

    /* Update Window Manager port reference */
    if (wmState->wMgrPort) {
        wmState->wMgrPort->windowList = wmState->windowList;
    }

    /* Bring native window to front */
    Platform_BringNativeWindowToFront(theWindow);

    /* Recalculate window order and visibility */
    WM_RecalculateWindowOrder();

    /* Schedule redraw if visible */
    if (theWindow->visible) {
        WM_ScheduleWindowUpdate(theWindow, kUpdateAll);
    }

    WM_DEBUG("BringToFront: Window moved to front successfully");
}

void SendBehind(WindowPtr theWindow, WindowPtr behindWindow) {
    if (theWindow == NULL) return;

    WM_DEBUG("SendBehind: Moving window behind another");

    WindowManagerState* wmState = GetWindowManagerState();

    /* Remove from current position */
    if (wmState->windowList == theWindow) {
        wmState->windowList = theWindow->nextWindow;
    } else {
        WindowPtr current = wmState->windowList;
        while (current && current->nextWindow != theWindow) {
            current = current->nextWindow;
        }
        if (current) {
            current->nextWindow = theWindow->nextWindow;
        }
    }

    /* Insert behind specified window */
    if (behindWindow == NULL) {
        /* Move to front */
        theWindow->nextWindow = wmState->windowList;
        wmState->windowList = theWindow;
    } else {
        theWindow->nextWindow = behindWindow->nextWindow;
        behindWindow->nextWindow = theWindow;
    }

    /* Update Window Manager port reference */
    if (wmState->wMgrPort) {
        wmState->wMgrPort->windowList = wmState->windowList;
    }

    /* Update native window ordering */
    Platform_SendNativeWindowBehind(theWindow, behindWindow);

    /* Recalculate window order and visibility */
    WM_RecalculateWindowOrder();

    /* Schedule redraw if visible */
    if (theWindow->visible) {
        WM_ScheduleWindowUpdate(theWindow, kUpdateAll);
    }

    WM_DEBUG("SendBehind: Window reordered successfully");
}

WindowPtr FrontWindow(void) {
    WindowManagerState* wmState = GetWindowManagerState();

    /* Find first visible window */
    WindowPtr current = wmState->windowList;
    while (current) {
        if (current->visible) {
            WM_DEBUG("FrontWindow: Found front window");
            return current;
        }
        current = current->nextWindow;
    }

    WM_DEBUG("FrontWindow: No visible windows found");
    return NULL;
}

/* ============================================================================
 * Window Drawing Functions
 * ============================================================================ */

void DrawGrowIcon(WindowPtr theWindow) {
    if (theWindow == NULL || !theWindow->visible) return;

    WM_DEBUG("DrawGrowIcon: Drawing grow icon");

    /* Calculate grow box rectangle */
    Rect growRect;
    Platform_GetWindowGrowBoxRect(theWindow, &growRect);

    /* Begin drawing in window */
    Platform_BeginWindowDraw(theWindow);

    /* Draw the grow icon (diagonal lines pattern) */
    /* TODO: Implement actual grow icon drawing when graphics system is available */
    /* For now, this is a placeholder that coordinates with the platform layer */

    Platform_EndWindowDraw(theWindow);

    WM_DEBUG("DrawGrowIcon: Grow icon drawn");
}

void DrawNew(WindowPeek window, Boolean update) {
    if (window == NULL || !window->visible) return;

    WM_DEBUG("DrawNew: Drawing newly created/shown window");

    /* Calculate all window regions */
    Platform_CalculateWindowRegions((WindowPtr)window);

    /* Draw window frame */
    WM_DrawWindowFrame((WindowPtr)window);

    /* Draw window content if update requested */
    if (update) {
        WM_DrawWindowContent((WindowPtr)window);
    }

    WM_DEBUG("DrawNew: New window drawn");
}

void PaintOne(WindowPeek window, RgnHandle clobberedRgn) {
    if (window == NULL || !window->visible) return;

    WM_DEBUG("PaintOne: Painting single window");

    /* Check if window intersects with clobbered region */
    if (clobberedRgn && window->strucRgn) {
        RgnHandle intersectRgn = Platform_NewRgn();
        if (intersectRgn) {
            Platform_IntersectRgn(window->strucRgn, clobberedRgn, intersectRgn);

            /* Only redraw if there's an intersection */
            if (!Platform_EmptyRgn(intersectRgn)) {
                WM_DrawWindowFrame((WindowPtr)window);
                WM_DrawWindowContent((WindowPtr)window);
            }

            Platform_DisposeRgn(intersectRgn);
        }
    } else {
        /* Redraw entire window */
        WM_DrawWindowFrame((WindowPtr)window);
        WM_DrawWindowContent((WindowPtr)window);
    }

    WM_DEBUG("PaintOne: Window painted");
}

void PaintBehind(WindowPeek startWindow, RgnHandle clobberedRgn) {
    if (startWindow == NULL) return;

    WM_DEBUG("PaintBehind: Painting windows behind specified window");

    /* Paint all windows behind the start window */
    WindowPtr current = startWindow->nextWindow;
    while (current) {
        if (current->visible) {
            PaintOne((WindowPeek)current, clobberedRgn);
        }
        current = current->nextWindow;
    }

    WM_DEBUG("PaintBehind: Background windows painted");
}

void CalcVis(WindowPeek window) {
    if (window == NULL) return;

    WM_DEBUG("CalcVis: Calculating visible region for window");

    /* This would calculate the visible region by subtracting
     * overlapping windows from the window's structure region */
    Platform_CalculateWindowRegions((WindowPtr)window);

    WM_DEBUG("CalcVis: Visible region calculated");
}

void CalcVisBehind(WindowPeek startWindow, RgnHandle clobberedRgn) {
    if (startWindow == NULL) return;

    WM_DEBUG("CalcVisBehind: Calculating visible regions for background windows");

    /* Recalculate visible regions for all windows behind the start window */
    WindowPtr current = startWindow->nextWindow;
    while (current) {
        if (current->visible) {
            CalcVis((WindowPeek)current);
        }
        current = current->nextWindow;
    }

    WM_DEBUG("CalcVisBehind: Background visible regions calculated");
}

void ClipAbove(WindowPeek window) {
    if (window == NULL) return;

    WM_DEBUG("ClipAbove: Setting clip region to exclude windows above");

    /* Set current port's clip region to exclude all windows
     * above the specified window */
    /* TODO: Implement when graphics system is available */

    WM_DEBUG("ClipAbove: Clip region set");
}

void SaveOld(WindowPeek window) {
    if (window == NULL) return;

    WM_DEBUG("SaveOld: Saving old window state");

    /* Save the old visible region before making changes */
    /* TODO: Implement old region saving when needed */

    WM_DEBUG("SaveOld: Old state saved");
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

void WM_RecalculateWindowOrder(void) {
    WM_DEBUG("WM_RecalculateWindowOrder: Recalculating window order");

    WindowManagerState* wmState = GetWindowManagerState();

    /* Recalculate visibility for all windows */
    WindowPtr current = wmState->windowList;
    while (current) {
        if (current->visible) {
            CalcVis((WindowPeek)current);
        }
        current = current->nextWindow;
    }

    WM_DEBUG("WM_RecalculateWindowOrder: Window order recalculated");
}

void WM_UpdateWindowVisibility(WindowPtr window) {
    if (window == NULL) return;

    WM_DEBUG("WM_UpdateWindowVisibility: Updating window visibility");

    /* Update visibility regions for this window and affected windows */
    CalcVis((WindowPeek)window);

    /* Update windows behind this one */
    CalcVisBehind((WindowPeek)window, window->strucRgn);

    WM_DEBUG("WM_UpdateWindowVisibility: Visibility updated");
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

void WM_DrawWindowFrame(WindowPtr window) {
    if (window == NULL || !window->visible) return;

    WM_DEBUG("WM_DrawWindowFrame: Drawing window frame");

    /* Begin drawing */
    Platform_BeginWindowDraw(window);

    /* Call window definition procedure to draw frame */
    if (window->windowDefProc) {
        WindowDefProcPtr wdef = (WindowDefProcPtr)(window->windowDefProc);
        wdef(0, window, wDraw, 0);
    }

    /* End drawing */
    Platform_EndWindowDraw(window);

    WM_DEBUG("WM_DrawWindowFrame: Frame drawn");
}

void WM_DrawWindowContent(WindowPtr window) {
    if (window == NULL || !window->visible) return;

    WM_DEBUG("WM_DrawWindowContent: Drawing window content");

    /* Begin drawing */
    Platform_BeginWindowDraw(window);

    /* Draw window picture if present */
    if (window->windowPic) {
        /* TODO: Draw picture when graphics system is available */
    }

    /* Post update event to application */
    Platform_PostWindowEvent(window, 6 /* update */, 0);

    /* End drawing */
    Platform_EndWindowDraw(window);

    WM_DEBUG("WM_DrawWindowContent: Content drawn");
}

void WM_ScheduleWindowUpdate(WindowPtr window, WindowUpdateFlags flags) {
    if (window == NULL) return;

    WM_DEBUG("WM_ScheduleWindowUpdate: Scheduling update flags 0x%x", flags);

    /* For now, immediately process the update */
    /* In a full implementation, this would queue the update */

    if (flags & kUpdateFrame) {
        Platform_InvalidateWindowFrame(window);
    }

    if (flags & kUpdateContent) {
        Platform_InvalidateWindowContent(window);
    }

    if (flags & kUpdateTitle) {
        Platform_InvalidateWindowFrame(window); /* Title is part of frame */
    }

    WM_DEBUG("WM_ScheduleWindowUpdate: Update scheduled");
}
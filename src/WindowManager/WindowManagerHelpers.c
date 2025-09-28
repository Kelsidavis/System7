/*
 * WindowManagerHelpers.c - Window Manager Helper Functions
 *
 * This file implements helper utilities used throughout the Window Manager
 * including geometry operations, state management, and internal utilities.
 */

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"

/* External functions */
extern void serial_printf(const char* fmt, ...);
extern QDGlobals qd;

/* Forward declarations */
Boolean WM_IsFloatingWindow(WindowPtr window);

/* GetWindowManagerState is defined in WindowManagerCore.c */
extern WindowManagerState* GetWindowManagerState(void);

/* Rectangle helpers */
Boolean WM_PtInRect(Point pt, const Rect* rect) {
    if (!rect) return false;
    return (pt.h >= rect->left && pt.h < rect->right &&
            pt.v >= rect->top && pt.v < rect->bottom);
}

void WM_SetRect(Rect* rect, short left, short top, short right, short bottom) {
    if (rect) {
        rect->left = left;
        rect->top = top;
        rect->right = right;
        rect->bottom = bottom;
    }
}

void WM_OffsetRect(Rect* rect, short dh, short dv) {
    if (rect) {
        rect->left += dh;
        rect->right += dh;
        rect->top += dv;
        rect->bottom += dv;
    }
}

void WM_InsetRect(Rect* rect, short dh, short dv) {
    if (rect) {
        rect->left += dh;
        rect->right -= dh;
        rect->top += dv;
        rect->bottom -= dv;
    }
}

Boolean WM_EmptyRect(const Rect* rect) {
    if (!rect) return true;
    return (rect->left >= rect->right || rect->top >= rect->bottom);
}

Boolean WM_RectsIntersect(const Rect* rect1, const Rect* rect2) {
    if (!rect1 || !rect2) return false;
    return !(rect1->right <= rect2->left || rect1->left >= rect2->right ||
             rect1->bottom <= rect2->top || rect1->top >= rect2->bottom);
}

void WM_IntersectRect(const Rect* src1, const Rect* src2, Rect* dst) {
    if (!src1 || !src2 || !dst) return;

    dst->left = (src1->left > src2->left) ? src1->left : src2->left;
    dst->top = (src1->top > src2->top) ? src1->top : src2->top;
    dst->right = (src1->right < src2->right) ? src1->right : src2->right;
    dst->bottom = (src1->bottom < src2->bottom) ? src1->bottom : src2->bottom;

    if (dst->left >= dst->right || dst->top >= dst->bottom) {
        /* Empty intersection */
        dst->left = dst->right = dst->top = dst->bottom = 0;
    }
}

/* Window validation helpers */
Boolean WM_ValidateWindow(WindowPtr window) {
    if (!window) return false;

    /* Check if window is in the window list */
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr current = wmState->windowList;
    while (current) {
        if (current == window) return true;
        current = current->nextWindow;
    }
    return false;
}

Boolean WM_ValidateRect(const Rect* rect) {
    if (!rect) return false;
    return (rect->right > rect->left && rect->bottom > rect->top);
}

/* Window geometry helpers */
short WM_GetRectWidth(const Rect* rect) {
    if (!rect) return 0;
    return rect->right - rect->left;
}

short WM_GetRectHeight(const Rect* rect) {
    if (!rect) return 0;
    return rect->bottom - rect->top;
}

/* Window state management - Generic stubs */
/* [WM-017] Note: These are generic stubs. WindowResizing.c has typed versions */
/* These exist for compatibility but should not conflict with file-local implementations */

/* Window constraints */
void WM_ConstrainToScreen(Rect* rect) {
    if (!rect) return;

    Rect screenBounds = qd.screenBits.bounds;
    screenBounds.top += 20;  /* Account for menu bar */

    /* Ensure window is at least partially on screen */
    if (rect->left >= screenBounds.right - 20) {
        WM_OffsetRect(rect, screenBounds.right - rect->right - 20, 0);
    }
    if (rect->right <= screenBounds.left + 20) {
        WM_OffsetRect(rect, screenBounds.left - rect->left + 20, 0);
    }
    if (rect->top >= screenBounds.bottom - 20) {
        WM_OffsetRect(rect, 0, screenBounds.bottom - rect->bottom - 20);
    }
    if (rect->bottom <= screenBounds.top + 20) {
        WM_OffsetRect(rect, 0, screenBounds.top - rect->top + 20);
    }
}

void WM_ConstrainToRect(Rect* rect, const Rect* bounds) {
    if (!rect || !bounds) return;

    /* Keep rect within bounds */
    if (rect->left < bounds->left) {
        WM_OffsetRect(rect, bounds->left - rect->left, 0);
    }
    if (rect->right > bounds->right) {
        WM_OffsetRect(rect, bounds->right - rect->right, 0);
    }
    if (rect->top < bounds->top) {
        WM_OffsetRect(rect, 0, bounds->top - rect->top);
    }
    if (rect->bottom > bounds->bottom) {
        WM_OffsetRect(rect, 0, bounds->bottom - rect->bottom);
    }
}

/* Pascal string utilities */
short GetPascalStringLength(const unsigned char* str) {
    if (!str) return 0;
    return str[0];
}

/* CopyPascalString is defined in WindowManagerCore.c */
extern void CopyPascalString(const unsigned char* src, unsigned char* dst);

/* Debug output macros implementation */
void WM_DebugPrint(const char* fmt, ...) {
#ifdef DEBUG_WINDOW_MANAGER
    serial_printf("WM_DEBUG: ");
    /* Would need proper varargs handling here */
    serial_printf("%s\n", fmt);
#endif
}

void WM_ErrorPrint(const char* fmt, ...) {
    serial_printf("WM_ERROR: ");
    /* Would need proper varargs handling here */
    serial_printf("%s\n", fmt);
}

/* Window visibility calculation */
void WM_CalculateWindowVisibility(WindowPtr window) {
    if (!window) return;

    /* Calculate visible region based on windows above */
    if (!window->port.visRgn) {
        window->port.visRgn = NewRgn();
    }

    /* Start with full window region */
    CopyRgn(window->strucRgn, window->port.visRgn);

    /* Subtract regions of windows above */
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr above = wmState->windowList;
    while (above && above != window) {
        if (above->visible && above->strucRgn) {
            /* Would need DiffRgn here */
            Platform_DiffRgn(window->port.visRgn, above->strucRgn, window->port.visRgn);
        }
        above = above->nextWindow;
    }
}

/* Window layer management */
short WM_GetWindowLayer(WindowPtr window) {
    if (!window) return 0;

    /* Determine window layer based on window type */
    if (window->windowKind < 0) {
        return 3;  /* System window */
    }

    /* Check if floating window */
    if (WM_IsFloatingWindow(window)) {
        return 2;  /* Floating window */
    }

    /* Check if modal dialog */
    if (window->windowDefProc == dBoxProc ||
        window->windowDefProc == plainDBox ||
        window->windowDefProc == altDBoxProc) {
        return 1;  /* Modal dialog */
    }

    return 0;  /* Normal window */
}

void WM_SetWindowLayer(WindowPtr window, short layer) {
    if (!window) return;
    /* Would store layer information in window */
}

Boolean WM_IsFloatingWindow(WindowPtr window) {
    if (!window) return false;
    /* Check for floating window proc - simplified check */
    return false;  /* No floating windows in basic implementation */
}

Boolean WM_IsAlertDialog(WindowPtr window) {
    if (!window) return false;
    return (window->windowDefProc == dBoxProc ||
            window->windowDefProc == plainDBox ||
            window->windowDefProc == altDBoxProc);
}

/* Window overlap testing */
Boolean WM_WindowsOverlap(WindowPtr window1, WindowPtr window2) {
    if (!window1 || !window2) return false;
    if (!window1->strucRgn || !window2->strucRgn) return false;

    /* Check if structure regions overlap */
    Rect bounds1, bounds2;
    Platform_GetRegionBounds(window1->strucRgn, &bounds1);
    Platform_GetRegionBounds(window2->strucRgn, &bounds2);

    return WM_RectsIntersect(&bounds1, &bounds2);
}

/* Window invalidation */
void WM_InvalidateScreenRegion(RgnHandle rgn) {
    if (!rgn) return;

    /* Mark region as needing redraw */
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr window = wmState->windowList;

    while (window) {
        if (window->visible && window->strucRgn) {
            /* Check if window intersects region */
            RgnHandle tempRgn = NewRgn();
            Platform_IntersectRgn(window->strucRgn, rgn, tempRgn);

            if (!Platform_EmptyRgn(tempRgn)) {
                /* Add to window's update region */
                if (!window->updateRgn) {
                    window->updateRgn = NewRgn();
                }
                Platform_UnionRgn(window->updateRgn, tempRgn, window->updateRgn);
            }

            DisposeRgn(tempRgn);
        }
        window = window->nextWindow;
    }
}

/* Window state validation */
UInt32 WM_CalculateStateChecksum(WindowPtr window) {
    if (!window) return 0;

    /* Simple checksum of window state */
    UInt32 checksum = 0;
    checksum += window->port.portRect.left;
    checksum += window->port.portRect.top;
    checksum += window->port.portRect.right;
    checksum += window->port.portRect.bottom;
    checksum += window->visible ? 1 : 0;
    checksum += window->hilited ? 2 : 0;

    return checksum;
}

void WM_UpdateStateChecksum(WindowPtr window) {
    if (!window) return;
    /* Store checksum for later validation */
}

Boolean WM_ValidateStateChecksum(WindowPtr window) {
    if (!window) return false;
    /* Validate stored checksum */
    return true;
}

/* Drag/resize feedback */
void WM_StartDragFeedback(WindowPtr window, Point startPt) {
    /* Initialize drag feedback */
}

void WM_UpdateDragFeedback(Point currentPt) {
    /* Update drag feedback */
}

void WM_EndDragFeedback(void) {
    /* Clean up drag feedback */
}

void WM_InitializeDragState(WindowPtr window, Point startPt) {
    WindowManagerState* wmState = GetWindowManagerState();
    wmState->isDragging = true;
    wmState->dragOffset.h = startPt.h - window->port.portRect.left;
    wmState->dragOffset.v = startPt.v - window->port.portRect.top;
}

void WM_CleanupDragState(void) {
    WindowManagerState* wmState = GetWindowManagerState();
    wmState->isDragging = false;
}

void WM_StartResizeFeedback(WindowPtr window, Point startPt) {
    /* Initialize resize feedback */
}

void WM_UpdateResizeFeedback(Point currentPt) {
    /* Update resize feedback */
}

void WM_EndResizeFeedback(void) {
    /* Clean up resize feedback */
}

void WM_InitializeResizeState(WindowPtr window, Point startPt) {
    WindowManagerState* wmState = GetWindowManagerState();
    wmState->isGrowing = true;
}

void WM_CleanupResizeState(void) {
    WindowManagerState* wmState = GetWindowManagerState();
    wmState->isGrowing = false;
}

void WM_GenerateResizeUpdateEvents(WindowPtr window, const Rect* oldBounds, const Rect* newBounds) {
    if (!window) return;

    /* Generate update events for resized window */
    if (!window->updateRgn) {
        window->updateRgn = NewRgn();
    }

    /* Mark entire window as needing update */
    RectRgn(window->updateRgn, &window->port.portRect);
}

/* Zoom animation */
void WM_AnimateZoom(WindowPtr window, const Rect* fromRect, const Rect* toRect) {
    /* Would animate zoom transition */
}

void WM_InterpolateRect(const Rect* from, const Rect* to, Rect* result, short fraction) {
    if (!from || !to || !result) return;

    /* Linear interpolation between rectangles */
    result->left = from->left + ((to->left - from->left) * fraction) / 100;
    result->top = from->top + ((to->top - from->top) * fraction) / 100;
    result->right = from->right + ((to->right - from->right) * fraction) / 100;
    result->bottom = from->bottom + ((to->bottom - from->bottom) * fraction) / 100;
}

/* Snap features */
void WM_InitializeSnapSizes(void) {
    /* Initialize snap size list */
}

void WM_AddSnapSize(short width, short height) {
    /* Add a snap size */
}

void WM_ApplySnapToEdges(Rect* rect) {
    /* Snap window to screen edges */
}

void WM_ApplySnapToSize(Rect* rect) {
    /* Snap window to predefined size */
}

Rect WM_CalculateNewSize(WindowPtr window, Point currentPt, const Rect* limits) {
    Rect newBounds = window->port.portRect;

    /* Calculate new size based on mouse position */
    newBounds.right = currentPt.h;
    newBounds.bottom = currentPt.v;

    /* Apply limits */
    if (limits) {
        if (WM_RECT_WIDTH(&newBounds) < WM_RECT_WIDTH(limits)) {
            newBounds.right = newBounds.left + WM_RECT_WIDTH(limits);
        }
        if (WM_RECT_HEIGHT(&newBounds) < WM_RECT_HEIGHT(limits)) {
            newBounds.bottom = newBounds.top + WM_RECT_HEIGHT(limits);
        }
    }

    return newBounds;
}

/* Window position calculation */
Point WM_CalculateConstrainedWindowPosition(WindowPtr window, Point proposedPos) {
    Point result = proposedPos;

    /* Constrain to screen */
    Rect screenBounds = qd.screenBits.bounds;
    screenBounds.top += 20;  /* Menu bar */

    if (result.h < screenBounds.left) result.h = screenBounds.left;
    if (result.v < screenBounds.top) result.v = screenBounds.top;

    return result;
}

Point WM_CalculateFinalWindowPosition(WindowPtr window, Point startPt, Point currentPt) {
    WindowManagerState* wmState = GetWindowManagerState();
    Point result;

    result.h = currentPt.h - wmState->dragOffset.h;
    result.v = currentPt.v - wmState->dragOffset.v;

    return WM_CalculateConstrainedWindowPosition(window, result);
}

/* Region area calculation */
long WM_CalculateRegionArea(RgnHandle rgn) {
    if (!rgn || !*rgn) return 0;

    /* Simple approximation using bounding box */
    Rect bounds = (*rgn)->rgnBBox;
    return (long)WM_RECT_WIDTH(&bounds) * (long)WM_RECT_HEIGHT(&bounds);
}

/* Window visibility statistics */
void WM_UpdateWindowVisibilityStats(WindowPtr window) {
    /* Track visibility statistics if needed */
}

/* Platform window order update */
void WM_UpdatePlatformWindowOrder(void) {
    Platform_UpdateNativeWindowOrder();
}

/* Modal window management */
void WM_DisableWindowsBehindModal(WindowPtr modalWindow) {
    if (!modalWindow) return;

    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr window = wmState->windowList;

    while (window) {
        if (window != modalWindow) {
            Platform_DisableWindow(window);
        }
        window = window->nextWindow;
    }
}

/* [WM-050] WM_EnableAllWindows moved to WindowLayering.c to avoid duplication */
/* [WM-050] WDEF procedures moved to WindowParts.c with proper signatures */
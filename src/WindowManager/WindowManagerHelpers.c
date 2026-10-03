/*
 * WindowManagerHelpers.c - Window Manager Helper Functions
 *
 * This file implements helper utilities used throughout the Window Manager
 * including geometry operations, state management, and internal utilities.
 */

#include "SystemTypes.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WindowRegions.h"
#include "WindowManager/WMLogging.h"
#include "QuickDraw/QuickDraw.h"

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

Boolean WM_EmptyRect(const Rect* rect) {
    if (!rect) return true;
    return (rect->left >= rect->right || rect->top >= rect->bottom);
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

/* Window invalidation */
void WM_InvalidateScreenRegion(RgnHandle rgn) {
    if (!rgn) return;

    /* Mark region as needing redraw */
    WindowManagerState* wmState = GetWindowManagerState();
    WindowPtr window = wmState->windowList;

    while (window) {
        if (window->visible && window->strucRgn) {
            /* Check if window intersects region */
            AutoRgnHandle tempRgn = WM_NewAutoRgn();
            if (!tempRgn.rgn) {
                /* Out of memory - skip this window */
                window = window->nextWindow;
                continue;
            }

            Platform_IntersectRgn(window->strucRgn, rgn, tempRgn.rgn);

            if (!Platform_EmptyRgn(tempRgn.rgn)) {
                /* Add to window's update region */
                if (!window->updateRgn) {
                    window->updateRgn = NewRgn();
                    if (!window->updateRgn) {
                        /* Out of memory - dispose temp and skip */
                        WM_DisposeAutoRgn(&tempRgn);
                        window = window->nextWindow;
                        continue;
                    }
                }
                Platform_UnionRgn(window->updateRgn, tempRgn.rgn, window->updateRgn);
            }

            WM_DisposeAutoRgn(&tempRgn);
        }
        window = window->nextWindow;
    }
}

/*
 * Invert a one-pixel outline of r, in global coordinates: the feedback
 * DragWindow and GrowWindow draw while tracking. Drawing it twice erases
 * it. Inverting the whole rectangle, as both used to, flashed the area
 * under the window instead of showing its outline.
 */
void WM_XorFrame(const Rect* r) {
    if (!r || r->right - r->left < 2 || r->bottom - r->top < 2) return;
    GrafPtr save;
    GetPort(&save);
    GrafPtr wm = NULL;
    GetWMgrPort(&wm);
    if (wm) SetPort(wm);
    Rect e;
    SetRect(&e, r->left, r->top, r->right, r->top + 1);            InvertRect(&e);
    SetRect(&e, r->left, r->bottom - 1, r->right, r->bottom);      InvertRect(&e);
    SetRect(&e, r->left, r->top + 1, r->left + 1, r->bottom - 1);  InvertRect(&e);
    SetRect(&e, r->right - 1, r->top + 1, r->right, r->bottom - 1); InvertRect(&e);
    SetPort(save);
}

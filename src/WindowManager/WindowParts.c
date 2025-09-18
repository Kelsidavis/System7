/*
 * WindowParts.c - Window Parts and Controls Implementation
 *
 * This file implements window parts management including title bars, close boxes,
 * zoom boxes, grow boxes, and other window decorations. These are the interactive
 * elements that allow users to manipulate windows.
 *
 * Key functions implemented:
 * - Window part hit testing and identification
 * - Close box (go-away box) handling
 * - Zoom box handling for window zooming
 * - Grow box handling for window resizing
 * - Title bar hit testing and dragging
 * - Window frame calculation and drawing
 * - Window definition procedure support
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#include "WindowManagerInternal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Window Part Geometry Constants
 * ============================================================================ */

/* Standard Mac OS window part dimensions */
#define TITLE_BAR_HEIGHT        20
#define CLOSE_BOX_SIZE          12
#define CLOSE_BOX_MARGIN        8
#define ZOOM_BOX_SIZE          12
#define ZOOM_BOX_MARGIN         8
#define GROW_BOX_SIZE          15
#define WINDOW_BORDER_WIDTH     1
#define WINDOW_SHADOW_WIDTH     3

/* Window part visual states */
typedef enum {
    kPartStateNormal = 0,
    kPartStatePressed = 1,
    kPartStateHighlighted = 2,
    kPartStateDisabled = 3
} WindowPartState;

/* ============================================================================
 * Window Part Calculation Functions
 * ============================================================================ */

void Platform_GetWindowFrameRect(WindowPtr window, Rect* frameRect) {
    if (window == NULL || frameRect == NULL) return;

    WM_DEBUG("Platform_GetWindowFrameRect: Calculating frame rectangle");

    /* Start with content rectangle */
    *frameRect = window->port.portRect;

    /* Add title bar height */
    frameRect->top -= TITLE_BAR_HEIGHT;

    /* Add border width */
    frameRect->left -= WINDOW_BORDER_WIDTH;
    frameRect->right += WINDOW_BORDER_WIDTH;
    frameRect->bottom += WINDOW_BORDER_WIDTH;

    WM_DEBUG("Platform_GetWindowFrameRect: Frame = (%d, %d, %d, %d)",
             frameRect->left, frameRect->top, frameRect->right, frameRect->bottom);
}

void Platform_GetWindowContentRect(WindowPtr window, Rect* contentRect) {
    if (window == NULL || contentRect == NULL) return;

    /* Content rectangle is the port rectangle */
    *contentRect = window->port.portRect;

    WM_DEBUG("Platform_GetWindowContentRect: Content = (%d, %d, %d, %d)",
             contentRect->left, contentRect->top, contentRect->right, contentRect->bottom);
}

void Platform_GetWindowTitleBarRect(WindowPtr window, Rect* titleRect) {
    if (window == NULL || titleRect == NULL) return;

    WM_DEBUG("Platform_GetWindowTitleBarRect: Calculating title bar rectangle");

    /* Title bar spans the full width above the content area */
    titleRect->left = window->port.portRect.left - WINDOW_BORDER_WIDTH;
    titleRect->right = window->port.portRect.right + WINDOW_BORDER_WIDTH;
    titleRect->top = window->port.portRect.top - TITLE_BAR_HEIGHT;
    titleRect->bottom = window->port.portRect.top;

    WM_DEBUG("Platform_GetWindowTitleBarRect: Title bar = (%d, %d, %d, %d)",
             titleRect->left, titleRect->top, titleRect->right, titleRect->bottom);
}

void Platform_GetWindowCloseBoxRect(WindowPtr window, Rect* closeRect) {
    if (window == NULL || closeRect == NULL) return;

    WM_DEBUG("Platform_GetWindowCloseBoxRect: Calculating close box rectangle");

    if (!window->goAwayFlag) {
        /* No close box - return empty rectangle */
        WM_SetRect(closeRect, 0, 0, 0, 0);
        return;
    }

    /* Close box is in the top-left of the title bar */
    closeRect->left = window->port.portRect.left - WINDOW_BORDER_WIDTH + CLOSE_BOX_MARGIN;
    closeRect->top = window->port.portRect.top - TITLE_BAR_HEIGHT +
                     (TITLE_BAR_HEIGHT - CLOSE_BOX_SIZE) / 2;
    closeRect->right = closeRect->left + CLOSE_BOX_SIZE;
    closeRect->bottom = closeRect->top + CLOSE_BOX_SIZE;

    WM_DEBUG("Platform_GetWindowCloseBoxRect: Close box = (%d, %d, %d, %d)",
             closeRect->left, closeRect->top, closeRect->right, closeRect->bottom);
}

void Platform_GetWindowZoomBoxRect(WindowPtr window, Rect* zoomRect) {
    if (window == NULL || zoomRect == NULL) return;

    WM_DEBUG("Platform_GetWindowZoomBoxRect: Calculating zoom box rectangle");

    /* Check if window supports zooming based on procID */
    Boolean hasZoomBox = WM_WindowHasZoomBox(window);
    if (!hasZoomBox) {
        /* No zoom box - return empty rectangle */
        WM_SetRect(zoomRect, 0, 0, 0, 0);
        return;
    }

    /* Zoom box is in the top-right of the title bar */
    zoomRect->right = window->port.portRect.right + WINDOW_BORDER_WIDTH - ZOOM_BOX_MARGIN;
    zoomRect->left = zoomRect->right - ZOOM_BOX_SIZE;
    zoomRect->top = window->port.portRect.top - TITLE_BAR_HEIGHT +
                    (TITLE_BAR_HEIGHT - ZOOM_BOX_SIZE) / 2;
    zoomRect->bottom = zoomRect->top + ZOOM_BOX_SIZE;

    WM_DEBUG("Platform_GetWindowZoomBoxRect: Zoom box = (%d, %d, %d, %d)",
             zoomRect->left, zoomRect->top, zoomRect->right, zoomRect->bottom);
}

void Platform_GetWindowGrowBoxRect(WindowPtr window, Rect* growRect) {
    if (window == NULL || growRect == NULL) return;

    WM_DEBUG("Platform_GetWindowGrowBoxRect: Calculating grow box rectangle");

    /* Check if window supports growing based on procID */
    Boolean hasGrowBox = WM_WindowHasGrowBox(window);
    if (!hasGrowBox) {
        /* No grow box - return empty rectangle */
        WM_SetRect(growRect, 0, 0, 0, 0);
        return;
    }

    /* Grow box is in the bottom-right corner of the content area */
    growRect->right = window->port.portRect.right;
    growRect->bottom = window->port.portRect.bottom;
    growRect->left = growRect->right - GROW_BOX_SIZE;
    growRect->top = growRect->bottom - GROW_BOX_SIZE;

    WM_DEBUG("Platform_GetWindowGrowBoxRect: Grow box = (%d, %d, %d, %d)",
             growRect->left, growRect->top, growRect->right, growRect->bottom);
}

/* ============================================================================
 * Window Part Hit Testing
 * ============================================================================ */

short Platform_WindowHitTest(WindowPtr window, Point pt) {
    if (window == NULL) return wNoHit;

    WM_DEBUG("Platform_WindowHitTest: Testing point (%d, %d) in window", pt.h, pt.v);

    /* Test window parts in order of precedence */

    /* Check close box first */
    if (window->goAwayFlag) {
        Rect closeRect;
        Platform_GetWindowCloseBoxRect(window, &closeRect);
        if (WM_PtInRect(pt, &closeRect)) {
            WM_DEBUG("Platform_WindowHitTest: Hit close box");
            return wInGoAway;
        }
    }

    /* Check zoom box */
    if (WM_WindowHasZoomBox(window)) {
        Rect zoomRect;
        Platform_GetWindowZoomBoxRect(window, &zoomRect);
        if (WM_PtInRect(pt, &zoomRect)) {
            /* Determine zoom direction based on current window state */
            if (WM_WindowIsZoomed(window)) {
                WM_DEBUG("Platform_WindowHitTest: Hit zoom box (zoom out)");
                return wInZoomOut;
            } else {
                WM_DEBUG("Platform_WindowHitTest: Hit zoom box (zoom in)");
                return wInZoomIn;
            }
        }
    }

    /* Check grow box */
    if (WM_WindowHasGrowBox(window)) {
        Rect growRect;
        Platform_GetWindowGrowBoxRect(window, &growRect);
        if (WM_PtInRect(pt, &growRect)) {
            WM_DEBUG("Platform_WindowHitTest: Hit grow box");
            return wInGrow;
        }
    }

    /* Check title bar (drag area) */
    Rect titleRect;
    Platform_GetWindowTitleBarRect(window, &titleRect);
    if (WM_PtInRect(pt, &titleRect)) {
        WM_DEBUG("Platform_WindowHitTest: Hit title bar");
        return wInDrag;
    }

    /* Check content area */
    if (window->contRgn && Platform_PtInRgn(pt, window->contRgn)) {
        WM_DEBUG("Platform_WindowHitTest: Hit content area");
        return wInContent;
    }

    /* Point is in structure but not in any specific part */
    WM_DEBUG("Platform_WindowHitTest: Hit window frame");
    return wInDrag; /* Default to drag for unknown areas */
}

Boolean Platform_PointInWindowPart(WindowPtr window, Point pt, short part) {
    if (window == NULL) return false;

    WM_DEBUG("Platform_PointInWindowPart: Testing point (%d, %d) for part %d",
             pt.h, pt.v, part);

    Rect partRect;
    Boolean validPart = true;

    switch (part) {
        case wInGoAway:
            Platform_GetWindowCloseBoxRect(window, &partRect);
            break;
        case wInZoomIn:
        case wInZoomOut:
            Platform_GetWindowZoomBoxRect(window, &partRect);
            break;
        case wInGrow:
            Platform_GetWindowGrowBoxRect(window, &partRect);
            break;
        case wInDrag:
            Platform_GetWindowTitleBarRect(window, &partRect);
            break;
        case wInContent:
            Platform_GetWindowContentRect(window, &partRect);
            break;
        default:
            validPart = false;
            break;
    }

    if (!validPart) {
        WM_DEBUG("Platform_PointInWindowPart: Invalid part %d", part);
        return false;
    }

    Boolean result = WM_PtInRect(pt, &partRect);
    WM_DEBUG("Platform_PointInWindowPart: Result = %s", result ? "true" : "false");
    return result;
}

/* ============================================================================
 * Window Definition Procedure Support
 * ============================================================================ */

Handle Platform_GetWindowDefProc(short procID) {
    WM_DEBUG("Platform_GetWindowDefProc: Getting WDEF for procID %d", procID);

    /* Return appropriate window definition procedure based on procID */
    switch (procID) {
        case documentProc:
        case noGrowDocProc:
        case zoomDocProc:
        case zoomNoGrow:
        case rDocProc:
            return (Handle)WM_StandardWindowDefProc;

        case dBoxProc:
        case plainDBox:
        case altDBoxProc:
        case movableDBoxProc:
            return (Handle)WM_DialogWindowDefProc;

        default:
            WM_DEBUG("Platform_GetWindowDefProc: Unknown procID %d, using standard", procID);
            return (Handle)WM_StandardWindowDefProc;
    }
}

/* ============================================================================
 * Window Definition Procedures
 * ============================================================================ */

long WM_StandardWindowDefProc(short varCode, WindowPtr theWindow, short message, long param) {
    if (theWindow == NULL) return 0;

    WM_DEBUG("WM_StandardWindowDefProc: Message %d, varCode %d", message, varCode);

    switch (message) {
        case wDraw:
            /* Draw window frame */
            WM_DrawStandardWindowFrame(theWindow, varCode);
            break;

        case wHit:
            /* Hit test window parts */
            {
                Point pt;
                pt.h = (short)(param >> 16);
                pt.v = (short)(param & 0xFFFF);
                return Platform_WindowHitTest(theWindow, pt);
            }

        case wCalcRgns:
            /* Calculate window regions */
            WM_CalculateStandardWindowRegions(theWindow, varCode);
            break;

        case wNew:
            /* Initialize window */
            WM_InitializeWindowParts(theWindow, varCode);
            break;

        case wDispose:
            /* Clean up window */
            WM_CleanupWindowParts(theWindow);
            break;

        case wGrow:
            /* Draw grow image */
            WM_DrawGrowImage(theWindow);
            break;

        case wDrawGIcon:
            /* Draw grow icon */
            WM_DrawGrowIcon(theWindow);
            break;

        default:
            WM_DEBUG("WM_StandardWindowDefProc: Unknown message %d", message);
            break;
    }

    return 0;
}

long WM_DialogWindowDefProc(short varCode, WindowPtr theWindow, short message, long param) {
    if (theWindow == NULL) return 0;

    WM_DEBUG("WM_DialogWindowDefProc: Message %d, varCode %d", message, varCode);

    switch (message) {
        case wDraw:
            /* Draw dialog frame (simpler than document window) */
            WM_DrawDialogWindowFrame(theWindow, varCode);
            break;

        case wHit:
            /* Hit test dialog parts */
            {
                Point pt;
                pt.h = (short)(param >> 16);
                pt.v = (short)(param & 0xFFFF);
                return WM_DialogWindowHitTest(theWindow, pt);
            }

        case wCalcRgns:
            /* Calculate dialog regions */
            WM_CalculateDialogWindowRegions(theWindow, varCode);
            break;

        case wNew:
            /* Initialize dialog */
            WM_InitializeDialogParts(theWindow, varCode);
            break;

        case wDispose:
            /* Clean up dialog */
            WM_CleanupWindowParts(theWindow);
            break;

        case wGrow:
        case wDrawGIcon:
            /* Dialogs typically don't have grow boxes */
            break;

        default:
            WM_DEBUG("WM_DialogWindowDefProc: Unknown message %d", message);
            break;
    }

    return 0;
}

/* ============================================================================
 * Window Frame Drawing
 * ============================================================================ */

void WM_DrawStandardWindowFrame(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_DrawStandardWindowFrame: Drawing standard window frame");

    /* Begin drawing session */
    Platform_BeginWindowDraw(window);

    /* Draw window border */
    WM_DrawWindowBorder(window);

    /* Draw title bar */
    WM_DrawWindowTitleBar(window);

    /* Draw close box if present */
    if (window->goAwayFlag) {
        WM_DrawWindowCloseBox(window, kPartStateNormal);
    }

    /* Draw zoom box if present */
    if (WM_WindowHasZoomBox(window)) {
        WM_DrawWindowZoomBox(window, kPartStateNormal);
    }

    /* Draw grow icon if present */
    if (WM_WindowHasGrowBox(window)) {
        WM_DrawGrowIcon(window);
    }

    /* End drawing session */
    Platform_EndWindowDraw(window);

    WM_DEBUG("WM_DrawStandardWindowFrame: Frame drawing complete");
}

void WM_DrawDialogWindowFrame(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_DrawDialogWindowFrame: Drawing dialog window frame");

    /* Begin drawing session */
    Platform_BeginWindowDraw(window);

    /* Draw simple border for dialog */
    WM_DrawDialogBorder(window);

    /* Draw title bar if movable dialog */
    if (varCode == movableDBoxProc) {
        WM_DrawWindowTitleBar(window);

        /* Draw close box if present */
        if (window->goAwayFlag) {
            WM_DrawWindowCloseBox(window, kPartStateNormal);
        }
    }

    /* End drawing session */
    Platform_EndWindowDraw(window);

    WM_DEBUG("WM_DrawDialogWindowFrame: Dialog frame drawing complete");
}

void WM_DrawWindowBorder(WindowPtr window) {
    if (window == NULL) return;

    /* Get frame rectangle */
    Rect frameRect;
    Platform_GetWindowFrameRect(window, &frameRect);

    /* Draw outer border */
    /* TODO: Implement actual border drawing when graphics system is available */
    WM_DEBUG("WM_DrawWindowBorder: Drawing border at (%d, %d, %d, %d)",
             frameRect.left, frameRect.top, frameRect.right, frameRect.bottom);
}

void WM_DrawDialogBorder(WindowPtr window) {
    if (window == NULL) return;

    /* Get content rectangle for dialog border */
    Rect contentRect = window->port.portRect;

    /* Dialogs have a simpler 3D-style border */
    /* TODO: Implement actual dialog border drawing when graphics system is available */
    WM_DEBUG("WM_DrawDialogBorder: Drawing dialog border");
}

void WM_DrawWindowTitleBar(WindowPtr window) {
    if (window == NULL) return;

    WM_DEBUG("WM_DrawWindowTitleBar: Drawing title bar");

    /* Get title bar rectangle */
    Rect titleRect;
    Platform_GetWindowTitleBarRect(window, &titleRect);

    /* Draw title bar background */
    /* TODO: Implement actual title bar drawing when graphics system is available */

    /* Draw window title text */
    if (window->titleHandle && *(window->titleHandle)) {
        WM_DrawWindowTitle(window, &titleRect);
    }

    WM_DEBUG("WM_DrawWindowTitleBar: Title bar drawn");
}

void WM_DrawWindowTitle(WindowPtr window, const Rect* titleRect) {
    if (window == NULL || titleRect == NULL) return;
    if (window->titleHandle == NULL || *(window->titleHandle) == NULL) return;

    WM_DEBUG("WM_DrawWindowTitle: Drawing window title");

    /* Calculate title position (centered in title bar) */
    unsigned char* title = *(window->titleHandle);
    short titleLength = title[0];

    if (titleLength > 0) {
        /* TODO: Implement actual text drawing when text system is available */
        /* For now, just log the title being drawn */
        char titleStr[256];
        memcpy(titleStr, &title[1], titleLength);
        titleStr[titleLength] = '\0';
        WM_DEBUG("WM_DrawWindowTitle: Title = \"%s\"", titleStr);
    }
}

void WM_DrawWindowCloseBox(WindowPtr window, WindowPartState state) {
    if (window == NULL || !window->goAwayFlag) return;

    WM_DEBUG("WM_DrawWindowCloseBox: Drawing close box, state = %d", state);

    /* Get close box rectangle */
    Rect closeRect;
    Platform_GetWindowCloseBoxRect(window, &closeRect);

    /* Draw close box based on state */
    /* TODO: Implement actual close box drawing when graphics system is available */

    WM_DEBUG("WM_DrawWindowCloseBox: Close box drawn");
}

void WM_DrawWindowZoomBox(WindowPtr window, WindowPartState state) {
    if (window == NULL || !WM_WindowHasZoomBox(window)) return;

    WM_DEBUG("WM_DrawWindowZoomBox: Drawing zoom box, state = %d", state);

    /* Get zoom box rectangle */
    Rect zoomRect;
    Platform_GetWindowZoomBoxRect(window, &zoomRect);

    /* Draw zoom box based on state and zoom direction */
    Boolean isZoomed = WM_WindowIsZoomed(window);

    /* TODO: Implement actual zoom box drawing when graphics system is available */

    WM_DEBUG("WM_DrawWindowZoomBox: Zoom box drawn, zoomed = %s",
             isZoomed ? "true" : "false");
}

void WM_DrawGrowIcon(WindowPtr window) {
    if (window == NULL || !WM_WindowHasGrowBox(window)) return;

    WM_DEBUG("WM_DrawGrowIcon: Drawing grow icon");

    /* Get grow box rectangle */
    Rect growRect;
    Platform_GetWindowGrowBoxRect(window, &growRect);

    /* Draw diagonal lines pattern for grow icon */
    /* TODO: Implement actual grow icon drawing when graphics system is available */

    WM_DEBUG("WM_DrawGrowIcon: Grow icon drawn");
}

void WM_DrawGrowImage(WindowPtr window) {
    /* This is called during window resizing to show grow feedback */
    WM_DrawGrowIcon(window);
}

/* ============================================================================
 * Window Region Calculation
 * ============================================================================ */

void WM_CalculateStandardWindowRegions(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_CalculateStandardWindowRegions: Calculating regions for standard window");

    /* Calculate structure region (includes frame) */
    Rect structRect;
    Platform_GetWindowFrameRect(window, &structRect);
    Platform_SetRectRgn(window->strucRgn, &structRect);

    /* Calculate content region (excludes frame) */
    Rect contentRect;
    Platform_GetWindowContentRect(window, &contentRect);
    Platform_SetRectRgn(window->contRgn, &contentRect);

    WM_DEBUG("WM_CalculateStandardWindowRegions: Regions calculated");
}

void WM_CalculateDialogWindowRegions(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_CalculateDialogWindowRegions: Calculating regions for dialog window");

    /* Dialogs have simpler region calculation */
    Rect dialogRect = window->port.portRect;

    /* Add border for structure region */
    Rect structRect = dialogRect;
    WM_InsetRect(&structRect, -WINDOW_BORDER_WIDTH, -WINDOW_BORDER_WIDTH);

    /* Add title bar for movable dialogs */
    if (varCode == movableDBoxProc) {
        structRect.top -= TITLE_BAR_HEIGHT;
    }

    Platform_SetRectRgn(window->strucRgn, &structRect);
    Platform_SetRectRgn(window->contRgn, &dialogRect);

    WM_DEBUG("WM_CalculateDialogWindowRegions: Dialog regions calculated");
}

/* ============================================================================
 * Window Part Initialization and Cleanup
 * ============================================================================ */

void WM_InitializeWindowParts(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_InitializeWindowParts: Initializing window parts");

    /* Set window capabilities based on procID */
    /* This information is used by hit testing and drawing functions */

    /* All these settings are implicitly handled by the procID,
     * but we could store additional state here if needed */

    WM_DEBUG("WM_InitializeWindowParts: Window parts initialized");
}

void WM_InitializeDialogParts(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_InitializeDialogParts: Initializing dialog parts");

    /* Dialogs have fewer parts than standard windows */

    WM_DEBUG("WM_InitializeDialogParts: Dialog parts initialized");
}

void WM_CleanupWindowParts(WindowPtr window) {
    if (window == NULL) return;

    WM_DEBUG("WM_CleanupWindowParts: Cleaning up window parts");

    /* Clean up any part-specific resources */
    /* For now, this is just a placeholder */

    WM_DEBUG("WM_CleanupWindowParts: Cleanup complete");
}

/* ============================================================================
 * Window Capability Queries
 * ============================================================================ */

Boolean WM_WindowHasCloseBox(WindowPtr window) {
    return (window != NULL && window->goAwayFlag);
}

Boolean WM_WindowHasZoomBox(WindowPtr window) {
    if (window == NULL) return false;

    /* Determine zoom box capability from window definition procedure */
    Handle wdef = window->windowDefProc;
    if (wdef == (Handle)WM_StandardWindowDefProc) {
        /* Check against the window's port for procID inference */
        /* In a full implementation, the procID would be stored */
        /* For now, assume document windows have zoom boxes */
        return true;
    }

    return false;
}

Boolean WM_WindowHasGrowBox(WindowPtr window) {
    if (window == NULL) return false;

    /* Determine grow box capability from window definition procedure */
    Handle wdef = window->windowDefProc;
    if (wdef == (Handle)WM_StandardWindowDefProc) {
        /* Check against specific window types */
        /* For now, assume document windows have grow boxes */
        return true;
    }

    return false;
}

Boolean WM_WindowIsZoomed(WindowPtr window) {
    if (window == NULL) return false;

    /* TODO: Implement actual zoom state tracking */
    /* For now, return false (not zoomed) */
    return false;
}

/* ============================================================================
 * Dialog Window Hit Testing
 * ============================================================================ */

short WM_DialogWindowHitTest(WindowPtr window, Point pt) {
    if (window == NULL) return wNoHit;

    WM_DEBUG("WM_DialogWindowHitTest: Testing point in dialog window");

    /* Check close box for movable dialogs */
    if (window->goAwayFlag) {
        Rect closeRect;
        Platform_GetWindowCloseBoxRect(window, &closeRect);
        if (WM_PtInRect(pt, &closeRect)) {
            return wInGoAway;
        }
    }

    /* Check title bar for movable dialogs */
    Handle wdef = window->windowDefProc;
    if (wdef == (Handle)WM_DialogWindowDefProc) {
        /* Assume movable if it has a close box or based on other criteria */
        Rect titleRect;
        Platform_GetWindowTitleBarRect(window, &titleRect);
        if (WM_PtInRect(pt, &titleRect)) {
            return wInDrag;
        }
    }

    /* Check content area */
    if (window->contRgn && Platform_PtInRgn(pt, window->contRgn)) {
        return wInContent;
    }

    return wNoHit;
}
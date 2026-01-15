/* #include "SystemTypes.h" */
#include <string.h>
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
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "WindowManager/WindowManagerInternal.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "FontManager/FontManager.h"
#include "FontManager/FontTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include <math.h>

/* Color constants */
#define blackColor 33
#include "WindowManager/WMLogging.h"

/* [WM-031] File-local helpers; provenance: IM:Windows "Window Definition Procedures" */
static short WM_DialogWindowHitTest(WindowPtr window, Point pt);

/* [WM-038] WDEF Procedures from IM:Windows Vol I pp. 2-88 to 2-95 */
long WM_StandardWindowDefProc(short varCode, WindowPtr theWindow, short message, long param);
long WM_DialogWindowDefProc(short varCode, WindowPtr theWindow, short message, long param);

/* [WM-054] WDEF constants now in canonical header */
#include "WindowManager/WindowWDEF.h"

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

/* Window part visual states - now defined in WindowManagerInternal.h */

/* ============================================================================
 * Window Part Calculation Functions
 * ============================================================================ */

/* ============================================================================
 * Window Part Hit Testing
 * ============================================================================ */

/* ============================================================================
 * Window Definition Procedure Support
 * ============================================================================ */

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

    /* Dialogs have a simpler 3D-style border */
    /* TODO: Implement actual dialog border drawing when graphics system is available */
    WM_DEBUG("WM_DrawDialogBorder: Drawing dialog border");
}

void WM_DrawWindowTitleBar(WindowPtr window) {
    if (window == NULL) return;

    WM_LOG_TRACE("*** WM_DrawWindowTitleBar called in WindowParts.c ***\n");
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
    extern void TextFont(short);
    extern void TextSize(short);
    extern void TextFace(Style);
    extern short StringWidth(ConstStr255Param);
    extern void DrawString(ConstStr255Param);
    extern void MoveTo(short, short);

    if (window == NULL || titleRect == NULL) return;
    if (window->titleHandle == NULL || *(window->titleHandle) == NULL) return;

    WM_LOG_TRACE("*** CODE PATH A: WM_DrawWindowTitle in WindowParts.c ***\n");
    WM_DEBUG("WM_DrawWindowTitle: Drawing window title with Font Manager");

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)window->titleHandle);

    /* Get title string */
    unsigned char* title = *(window->titleHandle);
    short titleLength = title[0];

    if (titleLength > 0) {
        /* Set font for window title (Chicago 12pt) */
        TextFont(chicagoFont);
        TextSize(12);
        TextFace(normal);
        ForeColor(blackColor);  /* Ensure black text */

        /* Calculate title width for centering */
        short titleWidth = StringWidth(title);

        /* Calculate centered position in title bar */
        short centerX = titleRect->left + ((titleRect->right - titleRect->left) - titleWidth) / 2;
        short centerY = titleRect->top + ((titleRect->bottom - titleRect->top) + 9) / 2; /* 9 = Chicago font ascent */

        /* Move to drawing position */
        MoveTo(centerX, centerY);

        /* Draw the title string using Font Manager */
        DrawString(title);

        /* Log the title being drawn */
        char titleStr[256];
        memcpy(titleStr, &title[1], titleLength);
        titleStr[titleLength] = '\0';
        WM_DEBUG("WM_DrawWindowTitle: Drew title \"%s\" at (%d, %d)", titleStr, centerX, centerY);
    }

    /* Unlock handle after use */
    HUnlock((Handle)window->titleHandle);
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

    /* TODO: Implement actual zoom box drawing when graphics system is available */

    WM_DEBUG("WM_DrawWindowZoomBox: Zoom box drawn");
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
    extern void serial_puts(const char *str);
    extern void uart_flush(void);
    serial_puts("[CALCSTD] enter\n");
    uart_flush();

    if (window == NULL) return;

    serial_puts("[CALCSTD] GetFrameRect\n");
    uart_flush();

    extern int sprintf(char* buf, const char* fmt, ...);
    if (window->refCon == 0x4449534b) {  /* DISK window */
        char dbgbuf[256];
        sprintf(dbgbuf, "[CALCRGN-CALLER] Called from caller - about to recalculate regions\n");
        serial_puts(dbgbuf);
        if (window->strucRgn && *(window->strucRgn)) {
            Rect oldStrucRgn = (*(window->strucRgn))->rgnBBox;
            sprintf(dbgbuf, "[CALCRGN] OLD strucRgn rgnBBox=(%d,%d,%d,%d) portRect=(%d,%d,%d,%d)\n",
                    oldStrucRgn.left, oldStrucRgn.top, oldStrucRgn.right, oldStrucRgn.bottom,
                    window->port.portRect.left, window->port.portRect.top,
                    window->port.portRect.right, window->port.portRect.bottom);
            serial_puts(dbgbuf);
        }
    }

    /* Calculate structure region (includes frame) */
    Rect structRect;
    serial_puts("[CALCSTD] GetWindowFrameRect\n");
    uart_flush();
    Platform_GetWindowFrameRect(window, &structRect);
    serial_puts("[CALCSTD] GetWindowFrameRect done\n");
    uart_flush();

    if (window->refCon == 0x4449534b) {  /* DISK window */
        char dbgbuf[256];
        sprintf(dbgbuf, "[CALCRGN] NEW structRect from GetWindowFrameRect=(%d,%d,%d,%d)\n",
                structRect.left, structRect.top, structRect.right, structRect.bottom);
        serial_puts(dbgbuf);
    }

    serial_puts("[CALCSTD] SetRectRgn struc\n");
    uart_flush();
    Platform_SetRectRgn(window->strucRgn, &structRect);
    serial_puts("[CALCSTD] SetRectRgn struc done\n");
    uart_flush();

    /* Calculate content region (excludes frame) */
    Rect contentRect;
    serial_puts("[CALCSTD] GetWindowContentRect\n");
    uart_flush();
    Platform_GetWindowContentRect(window, &contentRect);
    serial_puts("[CALCSTD] GetWindowContentRect done\n");
    uart_flush();

    /* DEBUG: Log detailed state before changing contRgn */
    extern void serial_puts(const char* str);
    extern int sprintf(char* buf, const char* fmt, ...);
    static int calc_log = 0;
    if (calc_log < 30 && window->refCon == 0x4449534b) {
        char dbgbuf[256];
        if (window->contRgn && *(window->contRgn)) {
            Rect oldContent = (*(window->contRgn))->rgnBBox;
            sprintf(dbgbuf, "[CALCSTD-OLD] DISK: oldContRgn=(%d,%d,%d,%d) strucRgn=(%d,%d,%d,%d)\n",
                    oldContent.left, oldContent.top, oldContent.right, oldContent.bottom,
                    (*(window->strucRgn))->rgnBBox.left, (*(window->strucRgn))->rgnBBox.top,
                    (*(window->strucRgn))->rgnBBox.right, (*(window->strucRgn))->rgnBBox.bottom);
            serial_puts(dbgbuf);
        }
        sprintf(dbgbuf, "[CALCSTD-NEW] DISK: newContentRect=(%d,%d,%d,%d) portBits=(%d,%d,%d,%d)\n",
                contentRect.left, contentRect.top, contentRect.right, contentRect.bottom,
                window->port.portBits.bounds.left, window->port.portBits.bounds.top,
                window->port.portBits.bounds.right, window->port.portBits.bounds.bottom);
        serial_puts(dbgbuf);
    }

    serial_puts("[CALCSTD] SetRectRgn cont\n");
    uart_flush();
    Platform_SetRectRgn(window->contRgn, &contentRect);
    serial_puts("[CALCSTD] SetRectRgn cont done\n");
    uart_flush();

    if (calc_log < 30 && window->refCon == 0x4449534b) {
        calc_log++;
    }

    serial_puts("[CALCSTD] done\n");
    uart_flush();
}

void WM_CalculateDialogWindowRegions(WindowPtr window, short varCode) {
    if (window == NULL) return;

    WM_DEBUG("WM_CalculateDialogWindowRegions: Calculating regions for dialog window");

    /* Dialogs have simpler region calculation */
    /* [WM-032] IM:Windows p.2-13: WindowRecord embeds GrafPort; use port.portRect */
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

/* [WM-035] Local zoom query; sources: IM:Windows "ZoomWindow" */
Boolean WM_WindowIsZoomed(WindowPtr window) {
    if (window == NULL) return false;

    /* Get window state data from WindowResizing module */
    extern void* WM_GetWindowStateData(WindowPtr window);
    void* statePtr = WM_GetWindowStateData(window);
    if (statePtr == NULL) {
        return false;
    }

    /* WindowStateData structure: first field is Boolean isZoomed */
    /* Access at offset 0 since isZoomed is the first member */
    Boolean* isZoomed = (Boolean*)statePtr;
    return *isZoomed;
}

/* ============================================================================
 * Dialog Window Hit Testing
 * ============================================================================ */

/* [WM-046] Used by WindowParts/WindowEvents; centralize later if shared */
static short WM_DialogWindowHitTest(WindowPtr window, Point pt) {
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

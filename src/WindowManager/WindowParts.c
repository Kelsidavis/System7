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
#define whiteColor 30
#include "WindowManager/WMLogging.h"

/* QuickDraw drawing primitives used by window chrome */
extern void EraseRect(const Rect* r);
extern void FrameRect(const Rect* r);
extern void PaintRect(const Rect* r);
extern void InsetRect(Rect* r, short dh, short dv);
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void ForeColor(SInt32 color);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void TextFont(short);
extern void TextSize(short);
extern void TextFace(Style);
extern short StringWidth(ConstStr255Param);
extern void DrawString(ConstStr255Param);

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

    Rect frameRect;
    Platform_GetWindowFrameRect(window, &frameRect);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* Draw 1-pixel black border */
    ForeColor(blackColor);
    FrameRect(&frameRect);

    /* Draw inner white highlight on top and left edges for 3D effect */
    ForeColor(whiteColor);
    MoveTo(frameRect.left + 1, frameRect.top + 1);
    LineTo(frameRect.right - 2, frameRect.top + 1);
    MoveTo(frameRect.left + 1, frameRect.top + 1);
    LineTo(frameRect.left + 1, frameRect.bottom - 2);

    /* Shadow on bottom and right edges */
    ForeColor(blackColor);
    MoveTo(frameRect.right - 1, frameRect.top + 1);
    LineTo(frameRect.right - 1, frameRect.bottom - 1);
    MoveTo(frameRect.left + 1, frameRect.bottom - 1);
    LineTo(frameRect.right - 1, frameRect.bottom - 1);

    ForeColor(blackColor);
    SetPort(savePort);

    WM_DEBUG("WM_DrawWindowBorder: Border drawn at (%d, %d, %d, %d)",
             frameRect.left, frameRect.top, frameRect.right, frameRect.bottom);
}

void WM_DrawDialogBorder(WindowPtr window) {
    if (window == NULL) return;

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* Dialog border: outer black frame + inner white frame for 3D look */
    Rect frameRect;
    Platform_GetWindowFrameRect(window, &frameRect);

    ForeColor(blackColor);
    FrameRect(&frameRect);

    /* Inner highlight border */
    Rect innerRect = frameRect;
    InsetRect(&innerRect, 1, 1);
    ForeColor(whiteColor);
    FrameRect(&innerRect);

    /* Second inner black border for double-border dialog style */
    InsetRect(&innerRect, 1, 1);
    ForeColor(blackColor);
    FrameRect(&innerRect);

    ForeColor(blackColor);
    SetPort(savePort);

    WM_DEBUG("WM_DrawDialogBorder: Dialog border drawn");
}

void WM_DrawWindowTitleBar(WindowPtr window) {
    if (window == NULL) return;

    WM_LOG_TRACE("*** WM_DrawWindowTitleBar called in WindowParts.c ***\n");
    WM_DEBUG("WM_DrawWindowTitleBar: Drawing title bar");

    Rect titleRect;
    Platform_GetWindowTitleBarRect(window, &titleRect);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* Fill title bar background with white */
    EraseRect(&titleRect);

    /* Draw horizontal stripes for active window (classic System 7 look) */
    if (window->hilited) {
        ForeColor(blackColor);
        for (short y = titleRect.top + 2; y < titleRect.bottom - 1; y += 2) {
            MoveTo(titleRect.left + 2, y);
            LineTo(titleRect.right - 3, y);
        }
    }

    /* Frame the title bar */
    ForeColor(blackColor);
    FrameRect(&titleRect);

    SetPort(savePort);

    /* Draw window title text (centered, with white gap behind text) */
    if (window->titleHandle && *(window->titleHandle)) {
        WM_DrawWindowTitle(window, &titleRect);
    }

    WM_DEBUG("WM_DrawWindowTitleBar: Title bar drawn");
}

void WM_DrawWindowTitle(WindowPtr window, const Rect* titleRect) {
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

        /* Calculate title width for centering */
        short titleWidth = StringWidth(title);

        /* Calculate centered position in title bar */
        short centerX = titleRect->left + ((titleRect->right - titleRect->left) - titleWidth) / 2;
        short centerY = titleRect->top + ((titleRect->bottom - titleRect->top) + 9) / 2;

        /* Erase a white rectangle behind the title text to clear stripes */
        Rect textBg;
        textBg.left = centerX - 4;
        textBg.top = titleRect->top + 1;
        textBg.right = centerX + titleWidth + 4;
        textBg.bottom = titleRect->bottom - 1;
        EraseRect(&textBg);

        /* Draw title text in black */
        ForeColor(blackColor);
        MoveTo(centerX, centerY);
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

    Rect closeRect;
    Platform_GetWindowCloseBoxRect(window, &closeRect);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* Clear the close box area */
    EraseRect(&closeRect);

    if (state == kPartStateDisabled) {
        /* Inactive: just draw empty square */
        ForeColor(blackColor);
        FrameRect(&closeRect);
    } else if (state == kPartStatePressed) {
        /* Pressed: filled black square */
        ForeColor(blackColor);
        PaintRect(&closeRect);
    } else {
        /* Normal: empty square frame */
        ForeColor(blackColor);
        FrameRect(&closeRect);
    }

    ForeColor(blackColor);
    SetPort(savePort);

    WM_DEBUG("WM_DrawWindowCloseBox: Close box drawn");
}

void WM_DrawWindowZoomBox(WindowPtr window, WindowPartState state) {
    if (window == NULL || !WM_WindowHasZoomBox(window)) return;

    WM_DEBUG("WM_DrawWindowZoomBox: Drawing zoom box, state = %d", state);

    Rect zoomRect;
    Platform_GetWindowZoomBoxRect(window, &zoomRect);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    EraseRect(&zoomRect);

    if (state == kPartStateDisabled) {
        /* Inactive: empty square */
        ForeColor(blackColor);
        FrameRect(&zoomRect);
    } else if (state == kPartStatePressed) {
        /* Pressed: filled square */
        ForeColor(blackColor);
        Rect outerBox = zoomRect;
        FrameRect(&outerBox);
        /* Inner offset box indicating zoom direction */
        Rect innerBox;
        innerBox.left = zoomRect.left + 2;
        innerBox.top = zoomRect.top + 2;
        innerBox.right = zoomRect.right - 2;
        innerBox.bottom = zoomRect.bottom - 2;
        FrameRect(&innerBox);
    } else {
        /* Normal: outer frame with smaller inner frame (System 7 zoom icon) */
        ForeColor(blackColor);
        FrameRect(&zoomRect);
        /* Small inner box offset toward bottom-right */
        Rect innerBox;
        innerBox.left = zoomRect.left + 3;
        innerBox.top = zoomRect.top + 3;
        innerBox.right = zoomRect.right - 1;
        innerBox.bottom = zoomRect.bottom - 1;
        FrameRect(&innerBox);
    }

    ForeColor(blackColor);
    SetPort(savePort);

    WM_DEBUG("WM_DrawWindowZoomBox: Zoom box drawn");
}

void WM_DrawGrowIcon(WindowPtr window) {
    if (window == NULL || !WM_WindowHasGrowBox(window)) return;

    WM_DEBUG("WM_DrawGrowIcon: Drawing grow icon");

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)window);

    /* Grow box sits in the bottom-right 15x15 area of the content region */
    short right = window->port.portRect.right;
    short bottom = window->port.portRect.bottom;
    Rect growBox;
    growBox.left = right - 15;
    growBox.top = bottom - 15;
    growBox.right = right;
    growBox.bottom = bottom;

    /* Clear area */
    EraseRect(&growBox);

    /* Draw the classic System 7 grow box: a small raised square with
     * a second offset square, creating the iconic double-box pattern */
    /* Outer box (larger) */
    Rect outerBox;
    outerBox.left = right - 13;
    outerBox.top = bottom - 13;
    outerBox.right = right - 2;
    outerBox.bottom = bottom - 2;
    FrameRect(&outerBox);

    /* Inner box (smaller, offset to top-left) */
    Rect innerBox;
    innerBox.left = right - 13;
    innerBox.top = bottom - 13;
    innerBox.right = right - 7;
    innerBox.bottom = bottom - 7;
    FrameRect(&innerBox);

    /* Separator lines between the two boxes */
    MoveTo(right - 7, bottom - 13);
    LineTo(right - 7, bottom - 7);
    MoveTo(right - 13, bottom - 7);
    LineTo(right - 7, bottom - 7);

    SetPort(savePort);

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

    extern int snprintf(char* buf, size_t size, const char* fmt, ...);
    if (window->refCon == 0x4449534b) {  /* DISK window */
        char dbgbuf[256];
        snprintf(dbgbuf, sizeof(dbgbuf), "[CALCRGN-CALLER] Called from caller - about to recalculate regions\n");
        serial_puts(dbgbuf);
        if (window->strucRgn && *(window->strucRgn)) {
            Rect oldStrucRgn = (*(window->strucRgn))->rgnBBox;
            snprintf(dbgbuf, sizeof(dbgbuf), "[CALCRGN] OLD strucRgn rgnBBox=(%d,%d,%d,%d) portRect=(%d,%d,%d,%d)\n",
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
        snprintf(dbgbuf, sizeof(dbgbuf), "[CALCRGN] NEW structRect from GetWindowFrameRect=(%d,%d,%d,%d)\n",
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
    extern int snprintf(char* buf, size_t size, const char* fmt, ...);
    static int calc_log = 0;
    if (calc_log < 30 && window->refCon == 0x4449534b) {
        char dbgbuf[256];
        if (window->contRgn && *(window->contRgn)) {
            Rect oldContent = (*(window->contRgn))->rgnBBox;
            snprintf(dbgbuf, sizeof(dbgbuf), "[CALCSTD-OLD] DISK: oldContRgn=(%d,%d,%d,%d) strucRgn=(%d,%d,%d,%d)\n",
                    oldContent.left, oldContent.top, oldContent.right, oldContent.bottom,
                    (*(window->strucRgn))->rgnBBox.left, (*(window->strucRgn))->rgnBBox.top,
                    (*(window->strucRgn))->rgnBBox.right, (*(window->strucRgn))->rgnBBox.bottom);
            serial_puts(dbgbuf);
        }
        snprintf(dbgbuf, sizeof(dbgbuf), "[CALCSTD-NEW] DISK: newContentRect=(%d,%d,%d,%d) portBits=(%d,%d,%d,%d)\n",
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

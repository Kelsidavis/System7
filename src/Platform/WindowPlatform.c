/*
#include "MemoryMgr/MemoryManager.h"
 * WindowPlatform.c - Platform implementation for Window Manager
#include "MemoryMgr/MemoryManager.h"
 * Provides platform-specific windowing functions
#include "MemoryMgr/MemoryManager.h"
 */
#include "MemoryMgr/MemoryManager.h"

#include "MemoryMgr/MemoryManager.h"
#include "MacTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "WindowManager/WindowPlatform.h"
#include "QuickDraw/QuickDraw.h"
#include "System71StdLib.h"
#include "Platform/PlatformLogging.h"

/* External framebuffer and QuickDraw globals */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern GrafPtr g_currentPort;

/* Initialize windowing system */
void Platform_InitWindowing(void) {
    /* Platform windowing is initialized when QuickDraw is initialized */
}

/* Check for Color QuickDraw */
Boolean Platform_HasColorQuickDraw(void) {
    return false;  /* We're using classic QuickDraw for now */
}

/* Initialize a window's graphics port */
Boolean Platform_InitializeWindowPort(WindowPtr window) {
    if (!window) return false;

    /* CRITICAL: DO NOT overwrite portBits.baseAddr or portBits.bounds!
     *
     * InitializeWindowRecord has already set up portBits correctly using the
     * "Direct Framebuffer" approach:
     * - portBits.baseAddr = framebuffer + offset to window's content area
     * - portBits.bounds = (0, 0, width, height) [LOCAL coordinates]
     * - portBits.rowBytes = set correctly
     *
     * If we overwrite baseAddr here, we break the coordinate system and cause
     * window content to render at the wrong screen position!
     *
     * FIX: Don't touch portBits at all - it's already correctly initialized.
     */

    /* Initialize clipping regions */
    if (!window->port.clipRgn) {
        window->port.clipRgn = NewRgn();
    }
    if (!window->port.visRgn) {
        window->port.visRgn = NewRgn();
    }

    RectRgn(window->port.clipRgn, &window->port.portRect);
    RectRgn(window->port.visRgn, &window->port.portRect);

    return true;
}

/* Calculate window regions (structure, content, etc.) */
void Platform_CalculateWindowRegions(WindowPtr window) {
    if (!window) return;

    /* CRITICAL FIX: Use the standard region calculation that properly handles chrome
     *
     * BUG: Previous code set contRgn = strucRgn (full window including frame)
     * This caused contRgn to cover areas outside the actual content, including
     * title bar and borders, leading to (0,20,800,600) full-screen regions.
     *
     * FIX: Use WM_CalculateStandardWindowRegions which correctly calculates:
     * - strucRgn from Platform_GetWindowFrameRect (portBits.bounds + chrome)
     * - contRgn from Platform_GetWindowContentRect (portBits.bounds directly)
     *
     * This ensures contRgn excludes the title bar and borders.
     */
    extern void WM_CalculateStandardWindowRegions(WindowPtr window, short varCode);
    WM_CalculateStandardWindowRegions(window, 0);
}

/* Create native window (no-op for our framebuffer implementation) */
void Platform_CreateNativeWindow(WindowPtr window) {
    /* No native window system - we draw directly to framebuffer */
}

/* Initialize color window port */
Boolean Platform_InitializeColorWindowPort(WindowPtr window) {
    /* Just use regular port initialization */
    return Platform_InitializeWindowPort(window);
}

/* Cleanup window port */
void Platform_CleanupWindowPort(WindowPtr window) {
    if (!window) return;

    /* Regions are already disposed in CloseWindow() */
    /* Disposing them again here causes a freeze, so skip it */
    /* TODO: Investigate proper region lifecycle management */
}

/* Destroy native window */
void Platform_DestroyNativeWindow(WindowPtr window) {
    /* No native window system */
}

/* Dispose region */
void Platform_DisposeRgn(RgnHandle rgn) {
    DisposeRgn(rgn);
}

/* Invalidate window content */
void Platform_InvalidateWindowContent(WindowPtr window) {
    /* Mark window as needing redraw */
    if (window && window->updateRgn) {
        /* Use the content region (global coords) for the update region */
        if (window->contRgn) {
            CopyRgn(window->contRgn, window->updateRgn);
        } else {
            /* Fallback - use local content dimensions */
            Rect localContent = {0, 0, window->port.portRect.bottom, window->port.portRect.right};
            RectRgn(window->updateRgn, &localContent);
        }
    }
}

/* Dispose color table */
void Platform_DisposeCTable(CTabHandle ctab) {
    /* Color tables not implemented yet */
}

/* Update window colors */
void Platform_UpdateWindowColors(WindowPtr window) {
    /* Colors not implemented yet */
}

/* Initialize port */
Boolean Platform_InitializePort(GrafPtr port) {
    if (!port) return false;

    extern uint32_t fb_pitch;

    /* Basic port initialization */
    port->portBits.baseAddr = (Ptr)framebuffer;
    /* Set PixMap flag (bit 15) to indicate 32-bit PixMap, not 1-bit BitMap
     * CRITICAL FIX: Must use fb_pitch, NOT fb_width*4! */
    port->portBits.rowBytes = fb_pitch | 0x8000;
    SetRect(&port->portBits.bounds, 0, 0, fb_width, fb_height);
    port->portRect = port->portBits.bounds;

    /* Initialize regions */
    if (!port->clipRgn) {
        port->clipRgn = NewRgn();
    }
    if (!port->visRgn) {
        port->visRgn = NewRgn();
    }
    RectRgn(port->clipRgn, &port->portRect);
    RectRgn(port->visRgn, &port->portRect);

    /* Initialize patterns */
    extern QDGlobals qd;
    port->bkPat = qd.white;
    port->fillPat = qd.black;
    port->pnLoc.h = 0;
    port->pnLoc.v = 0;
    port->pnSize.h = 1;
    port->pnSize.v = 1;
    port->pnMode = 8; /* patCopy */
    port->pnPat = qd.black;
    port->pnVis = 0;
    return false;
}

/* Get screen bounds */
void Platform_GetScreenBounds(Rect* bounds) {
    if (bounds) {
        SetRect(bounds, 0, 0, fb_width, fb_height);
    }
}

/* Convert local point to global coordinates */
Point Platform_LocalToGlobalPoint(WindowPtr window, Point localPt) {
    Point globalPt = localPt;

    if (window && window->strucRgn && *(window->strucRgn)) {
        /* Get window's global position from strucRgn */
        Rect globalBounds = (*(window->strucRgn))->rgnBBox;

        /* Offset by content area position (skip border and title) */
        const short kBorder = 1, kTitle = 20, kSeparator = 1;
        globalPt.h = localPt.h + globalBounds.left + kBorder;
        globalPt.v = localPt.v + globalBounds.top + kTitle + kSeparator;
    }

    return globalPt;
}

/* Convert global point to local coordinates */
Point Platform_GlobalToLocalPoint(WindowPtr window, Point globalPt) {
    Point localPt = globalPt;

    if (window && window->strucRgn && *(window->strucRgn)) {
        /* Get window's global position from strucRgn */
        Rect globalBounds = (*(window->strucRgn))->rgnBBox;

        /* Offset by content area position (skip border and title) */
        const short kBorder = 1, kTitle = 20, kSeparator = 1;
        localPt.h = globalPt.h - (globalBounds.left + kBorder);
        localPt.v = globalPt.v - (globalBounds.top + kTitle + kSeparator);
    }

    return localPt;
}

/* Initialize color port */
Boolean Platform_InitializeColorPort(CGrafPtr port) {
    /* Just use regular port initialization */
    Platform_InitializePort((GrafPtr)port);
    return false;
}

/* Create standard gray pattern */
PixPatHandle Platform_CreateStandardGrayPixPat(void) {
    return NULL;  /* Pixel patterns not implemented yet */
}

/* Create new region */
RgnHandle Platform_NewRgn(void) {
    return NewRgn();
}

/* Get window definition procedure */
Handle Platform_GetWindowDefProc(short procID) {
    /* [WM-039] WDEF dispatch - IM:Windows Vol I pp. 2-88 to 2-95 */
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
            return (Handle)WM_StandardWindowDefProc;
    }
}

/* WM_ScheduleWindowUpdate is now in WindowDisplay.c */

/* Debug/error logging defined in WindowManagerHelpers.c */

/* CopyPascalString is defined in WindowManagerCore.c */

/* Window drawing functions */
void Platform_SetNativeWindowTitle(WindowPtr window, ConstStr255Param title) {
    /* No native window system - titles drawn manually */
}

void Platform_BeginWindowDraw(WindowPtr window) {
    /* Set port for drawing */
    if (window) {
        SetPort(&window->port);
    }
}

void Platform_EndWindowDraw(WindowPtr window) {
    /* Nothing to do in our implementation */
}

void Platform_PostWindowEvent(WindowPtr window, short eventType, long eventData) {
    /* Event posting not implemented yet */
}

void Platform_InvalidateWindowFrame(WindowPtr window) {
    /* Mark window as needing redraw */
    if (window && window->updateRgn) {
        /* Use the structure region (global coords) for the update region */
        if (window->strucRgn) {
            CopyRgn(window->strucRgn, window->updateRgn);
        }
    }
}

/* More platform stubs */
void Platform_SendNativeWindowBehind(WindowPtr window, WindowPtr behindWindow) {
    /* No native window ordering */
}

void Platform_GetWindowGrowBoxRect(WindowPtr window, Rect* rect) {
    if (!window || !rect) return;
    if (!window->strucRgn || !*window->strucRgn) return;

    /* Standard grow box in bottom right of window structure (global coords) */
    *rect = (*window->strucRgn)->rgnBBox;
    rect->left = rect->right - 15;
    rect->top = rect->bottom - 15;
}

void Platform_IntersectRgn(RgnHandle src1, RgnHandle src2, RgnHandle dst) {
    /* Simple implementation - just copy src1 for now */
    if (dst && src1 && *src1) {
        CopyRgn(src1, dst);
    }
}

Boolean Platform_EmptyRgn(RgnHandle rgn) {
    if (!rgn || !*rgn) return true;
    return ((*rgn)->rgnBBox.left >= (*rgn)->rgnBBox.right ||
            (*rgn)->rgnBBox.top >= (*rgn)->rgnBBox.bottom);
}

Boolean Platform_PtInRgn(Point pt, RgnHandle rgn) {
    if (!rgn || !*rgn) return false;
    return (pt.h >= (*rgn)->rgnBBox.left && pt.h < (*rgn)->rgnBBox.right &&
            pt.v >= (*rgn)->rgnBBox.top && pt.v < (*rgn)->rgnBBox.bottom);
}

void Platform_ShowNativeWindow(WindowPtr window, Boolean show) {
    /* No native window system - visibility handled by Window Manager */
}

/* [WM-051] WM_InvalidateWindowsBelow moved to WindowLayering.c - no WM_ symbols in Platform */

void Platform_BringNativeWindowToFront(WindowPtr window) {
    /* No native window system - ordering handled by Window Manager */
}

/* Mouse and window tracking functions */
Boolean Platform_IsMouseDown(void) {
    extern Boolean Button(void);
    return Button();
}

void Platform_GetMousePosition(Point* pt) {
    extern void GetMouse(Point* mouseLoc);
    if (pt) {
        GetMouse(pt);
    }
}

/* Window part hit testing */
short Platform_WindowHitTest(WindowPtr window, Point pt) {
    if (!window) return wNoHit;

    Rect titleBar, closeBox, zoomBox, growBox, content;

    /* Get window rectangles */
    Platform_GetWindowTitleBarRect(window, &titleBar);
    Platform_GetWindowContentRect(window, &content);

    /* Check close box */
    if (window->goAwayFlag) {
        Platform_GetWindowCloseBoxRect(window, &closeBox);
        if (PtInRect(pt, &closeBox)) return wInGoAway;
    }

    /* Check zoom box */
    Platform_GetWindowZoomBoxRect(window, &zoomBox);
    if (PtInRect(pt, &zoomBox)) return wInZoomIn;

    /* Check grow box */
    Platform_GetWindowGrowBoxRect(window, &growBox);
    if (PtInRect(pt, &growBox)) return wInGrow;

    /* Check title bar */
    if (PtInRect(pt, &titleBar)) return wInDrag;

    /* Check content */
    if (PtInRect(pt, &content)) return wInContent;

    return wNoHit;
}

/* Window rectangle calculations */
void Platform_GetWindowTitleBarRect(WindowPtr window, Rect* rect) {
    if (!window || !rect) return;
    if (!window->strucRgn || !*window->strucRgn) return;

    /* Title bar is at top of window structure (global coords) */
    *rect = (*window->strucRgn)->rgnBBox;
    rect->bottom = rect->top + 20;  /* Standard title bar height */
}

void Platform_GetWindowContentRect(WindowPtr window, Rect* rect) {
    if (!window || !rect) return;

    /* CRITICAL: Calculate content rect from structure region
     *
     * Now that we use Direct Framebuffer approach, portBits.bounds is LOCAL (0,0,w,h).
     * We need to return GLOBAL coordinates for region calculations.
     *
     * strucRgn contains the full window frame in global coords.
     * Content rect = strucRgn minus chrome (title bar + borders).
     */
    if (!window->strucRgn || !*(window->strucRgn)) {
        SetRect(rect, 0, 0, 0, 0);
        return;
    }

    Rect strucRect = (**(window->strucRgn)).rgnBBox;

    /* Chrome dimensions */
    const SInt16 kBorder = 1;
    const SInt16 kTitleBar = 20;
    const SInt16 kSeparator = 1;

    /* Content is inside the frame */
    rect->left = strucRect.left + kBorder;
    rect->top = strucRect.top + kTitleBar + kSeparator;
    rect->right = strucRect.right - (kBorder + 1);  /* Right border is 2px */
    rect->bottom = strucRect.bottom - kBorder;
}

void Platform_GetWindowCloseBoxRect(WindowPtr window, Rect* rect) {
    if (!window || !rect) return;

    /* Close box is in left side of title bar */
    Platform_GetWindowTitleBarRect(window, rect);
    rect->right = rect->left + 20;
    rect->top += 2;
    rect->bottom -= 2;
    rect->left += 2;
}

void Platform_GetWindowZoomBoxRect(WindowPtr window, Rect* rect) {
    if (!window || !rect) return;

    /* Zoom box is in right side of title bar */
    Platform_GetWindowTitleBarRect(window, rect);
    rect->left = rect->right - 20;
    rect->top += 2;
    rect->bottom -= 2;
    rect->right -= 2;
}

void Platform_GetWindowFrameRect(WindowPtr window, Rect* rect) {
    if (!window || !rect) return;

    /* CRITICAL: Return structure region directly
     *
     * Now that we use Direct Framebuffer approach, portBits.bounds is LOCAL (0,0,w,h).
     * strucRgn contains the full window frame in GLOBAL coordinates, which is exactly
     * what we need to return here.
     */
    if (!window->strucRgn || !*(window->strucRgn)) {
        SetRect(rect, 0, 0, 0, 0);
        return;
    }

    *rect = (**(window->strucRgn)).rgnBBox;
}

/* Window highlighting */
void Platform_HighlightWindowPart(WindowPtr window, short partCode, Boolean highlight) {
    /* Draw highlight feedback for window parts */
    if (!window) return;

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort(&window->port);

    Rect partRect;
    Boolean hasRect = false;

    switch (partCode) {
        case inGoAway:
            Platform_GetWindowCloseBoxRect(window, &partRect);
            hasRect = true;
            break;
        case inZoomIn:
        case inZoomOut:
            Platform_GetWindowZoomBoxRect(window, &partRect);
            hasRect = true;
            break;
    }

    if (hasRect) {
        /* InvertRect toggles highlight state (XOR operation) */
        InvertRect(&partRect);
    }

    SetPort(savePort);
}

/* Draw close box directly to framebuffer to clean up InvertRect artifacts */
void Platform_DrawCloseBoxDirect(WindowPtr window) {
    if (!window || !window->goAwayFlag) return;

    /* Get close box rectangle */
    Rect closeRect;
    Platform_GetWindowCloseBoxRect(window, &closeRect);

    /* Get framebuffer directly - we need to draw outside the GWorld */
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;

    /* Fill close box area with gray50 pattern (title bar background) */
    uint32_t* fb = (uint32_t*)framebuffer;
    int pitch_dwords = fb_pitch / 4;

    for (int y = closeRect.top; y < closeRect.bottom && y < fb_height; y++) {
        for (int x = closeRect.left; x < closeRect.right && x < fb_width; x++) {
            /* Use gray50 pattern for title bar - alternating black/white pixels */
            int pattern_bit = ((x + y) & 1);
            fb[y * pitch_dwords + x] = pattern_bit ? 0xFFFFFFFF : 0xFF000000;
        }
    }

    /* Now draw the close box itself (white box with black border) */
    /* Draw black border */
    for (int x = closeRect.left; x < closeRect.right && x < fb_width; x++) {
        if (closeRect.top < fb_height)
            fb[closeRect.top * pitch_dwords + x] = 0xFF000000;
        if (closeRect.bottom - 1 < fb_height)
            fb[(closeRect.bottom - 1) * pitch_dwords + x] = 0xFF000000;
    }
    for (int y = closeRect.top; y < closeRect.bottom && y < fb_height; y++) {
        if (closeRect.left < fb_width)
            fb[y * pitch_dwords + closeRect.left] = 0xFF000000;
        if (closeRect.right - 1 < fb_width)
            fb[y * pitch_dwords + (closeRect.right - 1)] = 0xFF000000;
    }

    /* Fill interior with white */
    for (int y = closeRect.top + 1; y < closeRect.bottom - 1 && y < fb_height; y++) {
        for (int x = closeRect.left + 1; x < closeRect.right - 1 && x < fb_width; x++) {
            fb[y * pitch_dwords + x] = 0xFFFFFFFF;
        }
    }
}

/* Wait functions */
void Platform_WaitTicks(short ticks) {
    extern UInt32 TickCount(void);
    extern void ProcessModernInput(void);  /* Poll PS/2 controller for button updates */
    extern void serial_puts(const char* str);

    UInt32 start = TickCount();
    UInt32 iterations = 0;
    const UInt32 MAX_ITERATIONS = 1000;  /* Safety timeout - 1000 iterations is ~16ms at ~60Hz */
    static int pwt_call_count = 0;

    if (ticks <= 5) {  /* Only log brief waits that are called frequently */
        pwt_call_count++;
        if (pwt_call_count % 50 == 0) {  /* Log every 50 calls */
            char msg[80];
            sprintf(msg, "[PWT] Call #%d: ticks=%d iterations=%d\n", pwt_call_count, ticks, iterations);
            serial_puts(msg);
        }
    }

    while (TickCount() - start < ticks && iterations < MAX_ITERATIONS) {
        ProcessModernInput();  /* Critical: update gCurrentButtons during waits */
        iterations++;
    }

    if (iterations >= MAX_ITERATIONS) {
        serial_puts("[PWT] WARNING: Timeout in Platform_WaitTicks after 1000 iterations\n");
    }
}

/* Port management */
GrafPtr Platform_GetCurrentPort(void) {
    extern GrafPtr g_currentPort;
    return g_currentPort;
}

void Platform_SetCurrentPort(GrafPtr port) {
    extern GrafPtr g_currentPort;
    g_currentPort = port;
}

GrafPtr Platform_GetUpdatePort(WindowPtr window) {
    return window ? &window->port : NULL;
}

void Platform_SetUpdatePort(GrafPtr port) {
    SetPort(port);
}

/* Region operations */
void Platform_CopyRgn(RgnHandle src, RgnHandle dst) {
    if (src && dst && *src && *dst) {
        /* CRITICAL: Lock handles before dereferencing to prevent heap compaction issues */
        HLock((Handle)src);
        HLock((Handle)dst);
        **dst = **src;
        HUnlock((Handle)dst);
        HUnlock((Handle)src);
    }
}

void Platform_SetRectRgn(RgnHandle rgn, const Rect* rect) {
    if (rgn && *rgn && rect) {
        RectRgn(rgn, rect);
    }
}

void Platform_SetEmptyRgn(RgnHandle rgn) {
    if (rgn && *rgn) {
        SetEmptyRgn(rgn);
    }
}

void Platform_UnionRgn(RgnHandle src1, RgnHandle src2, RgnHandle dst) {
    if (!dst || !(*dst)) return;

    Region* dstRgn = *dst;

    if (src1 && *src1 && src2 && *src2) {
        Region* rgn1 = *src1;
        Region* rgn2 = *src2;

        Boolean rgn1Empty = (rgn1->rgnBBox.left >= rgn1->rgnBBox.right || rgn1->rgnBBox.top >= rgn1->rgnBBox.bottom);
        Boolean rgn2Empty = (rgn2->rgnBBox.left >= rgn2->rgnBBox.right || rgn2->rgnBBox.top >= rgn2->rgnBBox.bottom);

        if (rgn1Empty && !rgn2Empty) {
            Platform_CopyRgn(src2, dst);
        } else if (rgn2Empty && !rgn1Empty) {
            Platform_CopyRgn(src1, dst);
        } else if (!rgn1Empty && !rgn2Empty) {
            dstRgn->rgnBBox.left = (rgn1->rgnBBox.left < rgn2->rgnBBox.left) ? rgn1->rgnBBox.left : rgn2->rgnBBox.left;
            dstRgn->rgnBBox.top = (rgn1->rgnBBox.top < rgn2->rgnBBox.top) ? rgn1->rgnBBox.top : rgn2->rgnBBox.top;
            dstRgn->rgnBBox.right = (rgn1->rgnBBox.right > rgn2->rgnBBox.right) ? rgn1->rgnBBox.right : rgn2->rgnBBox.right;
            dstRgn->rgnBBox.bottom = (rgn1->rgnBBox.bottom > rgn2->rgnBBox.bottom) ? rgn1->rgnBBox.bottom : rgn2->rgnBBox.bottom;
        }
    } else if (src1 && *src1) {
        Platform_CopyRgn(src1, dst);
    } else if (src2 && *src2) {
        Platform_CopyRgn(src2, dst);
    }
}

void Platform_DiffRgn(RgnHandle src1, RgnHandle src2, RgnHandle dst) {
    /* Simple difference - just copy src1 for now */
    if (dst && src1) {
        Platform_CopyRgn(src1, dst);
    }
}

void Platform_OffsetRgn(RgnHandle rgn, short dh, short dv) {
    if (rgn && *rgn) {
        (*rgn)->rgnBBox.left += dh;
        (*rgn)->rgnBBox.right += dh;
        (*rgn)->rgnBBox.top += dv;
        (*rgn)->rgnBBox.bottom += dv;
    }
}

void Platform_SetClipRgn(GrafPtr port, RgnHandle rgn) {
    if (port && rgn) {
        Platform_CopyRgn(rgn, port->clipRgn);
    }
}

void Platform_GetRegionBounds(RgnHandle rgn, Rect* bounds) {
    if (rgn && *rgn && bounds) {
        *bounds = (*rgn)->rgnBBox;
    }
}

/* Window movement and sizing */
void Platform_MoveNativeWindow(WindowPtr window, short h, short v) {
    if (window) {
        /* CRITICAL: portRect must ALWAYS stay in LOCAL coordinates (0,0,width,height)!
         * Only update portBits.bounds to map local coords to new global screen position
         *
         * IMPORTANT: h,v are WINDOW FRAME coords, but portBits.bounds must map to CONTENT area!
         * Content area starts 21px down from frame (20px title + 1px separator) */
        const short kBorder = 1, kTitle = 20, kSeparator = 1;

        short width = window->port.portRect.right - window->port.portRect.left;
        short height = window->port.portRect.bottom - window->port.portRect.top;

        /* portRect stays local - DO NOT modify it! */
        /* portBits.bounds maps local (0,0) to global content area position */
        window->port.portBits.bounds.left = h + kBorder;
        window->port.portBits.bounds.top = v + kTitle + kSeparator;
        window->port.portBits.bounds.right = h + kBorder + width;
        window->port.portBits.bounds.bottom = v + kTitle + kSeparator + height;

        Platform_CalculateWindowRegions(window);
    }
}

void Platform_SizeNativeWindow(WindowPtr window, short width, short height) {
    if (window) {
        /* CRITICAL: portRect must ALWAYS stay in LOCAL coordinates!
         * When resizing, keep portRect at (0,0,width,height) and update portBits.bounds */
        window->port.portRect.left = 0;
        window->port.portRect.top = 0;
        window->port.portRect.right = width;
        window->port.portRect.bottom = height;

        /* Update portBits.bounds to match new size at current global position */
        window->port.portBits.bounds.right = window->port.portBits.bounds.left + width;
        window->port.portBits.bounds.bottom = window->port.portBits.bounds.top + height;

        Platform_CalculateWindowRegions(window);
    }
}

/* Drag feedback */
void Platform_ShowDragOutline(Rect* rect) {
    /* Draw drag outline */
    GrafPtr savePort;
    GetPort(&savePort);

    extern QDGlobals qd;
    SetPort(qd.thePort);

    /* Use XOR mode for drag outline */
    /* patXor = 10, patCopy = 8 in QuickDraw */
    PenMode(10);  /* patXor */
    FrameRect(rect);
    PenMode(8);   /* patCopy */

    SetPort(savePort);
}

void Platform_HideDragOutline(Rect* rect) {
    /* Erase drag outline by drawing again in XOR mode */
    Platform_ShowDragOutline(rect);
}

void Platform_UpdateDragOutline(Rect* oldRect, Rect* newRect) {
    if (oldRect) Platform_HideDragOutline(oldRect);
    if (newRect) Platform_ShowDragOutline(newRect);
}

void Platform_ShowDragRect(Rect* rect) {
    Platform_ShowDragOutline(rect);
}

void Platform_HideDragRect(Rect* rect) {
    Platform_HideDragOutline(rect);
}

void Platform_UpdateDragRect(Rect* oldRect, Rect* newRect) {
    Platform_UpdateDragOutline(oldRect, newRect);
}

/* Size feedback */
void Platform_ShowSizeFeedback(Rect* rect) {
    Platform_ShowDragOutline(rect);
}

void Platform_HideSizeFeedback(Rect* rect) {
    Platform_HideDragOutline(rect);
}

void Platform_UpdateSizeFeedback(Rect* oldRect, Rect* newRect) {
    Platform_UpdateDragOutline(oldRect, newRect);
}

/* Zoom animation */
void Platform_ShowZoomFrame(Rect* rect) {
    Platform_ShowDragOutline(rect);
}

void Platform_HideZoomFrame(Rect* rect) {
    Platform_HideDragOutline(rect);
}

/* Window state */
void Platform_DisableWindow(WindowPtr window) {
    if (window) {
        window->hilited = false;
    }
}

void Platform_EnableWindow(WindowPtr window) {
    if (window) {
        window->hilited = true;
    }
}

/* Preferences */
Boolean Platform_GetPreferredDragFeedback(void) {
    return true;  /* Use outline feedback */
}

Boolean Platform_IsResizeFeedbackEnabled(void) {
    return true;
}

Boolean Platform_IsSnapToEdgesEnabled(void) {
    return false;
}

Boolean Platform_IsSnapToSizeEnabled(void) {
    return false;
}

Boolean Platform_IsZoomAnimationEnabled(void) {
    return false;
}

/* Window ordering */
void Platform_UpdateNativeWindowOrder(void) {
    /* No native window system */
}

/* Window invalidation */
void Platform_InvalidateWindowRect(WindowPtr window, const Rect* rect) {
    if (window && rect) {
        /* Add rect to update region */
        if (!window->updateRgn) {
            window->updateRgn = NewRgn();
        }
        RgnHandle tempRgn = NewRgn();
        RectRgn(tempRgn, rect);
        Platform_UnionRgn(window->updateRgn, tempRgn, window->updateRgn);
        DisposeRgn(tempRgn);
    }
}

/* Pattern creation - removed duplicate, see line 162 */

/* Window definition procedure - removed duplicate, see line 172 */

/* Point testing */
Boolean Platform_PointInWindowPart(WindowPtr window, Point pt, short partCode) {
    if (!window) return false;

    Rect partRect;
    switch (partCode) {
        case inGoAway:
            Platform_GetWindowCloseBoxRect(window, &partRect);
            return PtInRect(pt, &partRect);
        case inZoomIn:
        case inZoomOut:
            Platform_GetWindowZoomBoxRect(window, &partRect);
            return PtInRect(pt, &partRect);
        case inGrow:
            Platform_GetWindowGrowBoxRect(window, &partRect);
            return PtInRect(pt, &partRect);
        case inDrag:
            Platform_GetWindowTitleBarRect(window, &partRect);
            return PtInRect(pt, &partRect);
        case inContent:
            Platform_GetWindowContentRect(window, &partRect);
            return PtInRect(pt, &partRect);
    }
    return false;
}
/*
 * Draw RGBA bitmap directly to framebuffer
 * Used by StartupScreen to display the Mac Picasso logo
 */
void PlatformDrawRGBABitmap(const UInt8* rgba_data, int width, int height, int dest_x, int dest_y) {
    if (!framebuffer || !rgba_data) return;

    uint32_t* fb = (uint32_t*)framebuffer;
    const UInt8* src = rgba_data;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int fb_x = dest_x + x;
            int fb_y = dest_y + y;

            /* Bounds check */
            if (fb_x < 0 || fb_x >= (int)fb_width || fb_y < 0 || fb_y >= (int)fb_height) {
                src += 4;  /* Skip pixel */
                continue;
            }

            /* Read RGBA */
            UInt8 r = *src++;
            UInt8 g = *src++;
            UInt8 b = *src++;
            UInt8 a = *src++;

            /* Alpha blending (if alpha < 255, blend with existing pixel) */
            if (a == 0) {
                /* Fully transparent - skip */
                continue;
            } else if (a == 255) {
                /* Fully opaque - direct write */
                fb[fb_y * fb_width + fb_x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
            } else {
                /* Blend with background */
                uint32_t bg = fb[fb_y * fb_width + fb_x];
                UInt8 bg_r = (bg >> 16) & 0xFF;
                UInt8 bg_g = (bg >> 8) & 0xFF;
                UInt8 bg_b = bg & 0xFF;

                /* Alpha blend formula: result = (fg * alpha + bg * (255 - alpha)) / 255 */
                UInt8 out_r = (r * a + bg_r * (255 - a)) / 255;
                UInt8 out_g = (g * a + bg_g * (255 - a)) / 255;
                UInt8 out_b = (b * a + bg_b * (255 - a)) / 255;

                fb[fb_y * fb_width + fb_x] = (0xFFU << 24) | (out_r << 16) | (out_g << 8) | out_b;
            }
        }
    }
}

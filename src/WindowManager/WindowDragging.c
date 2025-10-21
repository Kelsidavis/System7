/* #include "SystemTypes.h" */
#include <string.h>
/*
 * WindowDragging.c - Window Dragging and Positioning Implementation
 *
 * This file implements window dragging, positioning, and movement functions.
 * These functions handle user interaction for moving windows around the screen,
 * including constraint checking, drag feedback, and platform coordination.
 *
 * Key functions implemented:
 * - Window dragging (DragWindow)
 * - Window positioning and movement (MoveWindow)
 * - Drag constraint enforcement
 * - Drag feedback and visual tracking
 * - Multi-monitor and screen boundary handling
 * - Window snapping and alignment
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "WindowManager/WMLogging.h"
#include "QuickDraw/QuickDraw.h"
#include "MemoryMgr/MemoryManager.h"

#include "WindowManager/WindowManagerInternal.h"
#include <math.h>

/* Forward declarations */
Boolean WM_ValidateWindowPosition(WindowPtr window, const Rect* bounds);
#if 0  /* UNUSED forward declarations */
/* static Point Local_ApplySnapToEdges(Point windowPos); */
#endif
void WM_ConstrainWindowPosition(WindowPtr window, Rect* bounds);
void WM_UpdateWindowVisibility(WindowPtr window);
void WM_OffsetRect(Rect* rect, short deltaH, short deltaV);
#if 0  /* UNUSED forward declarations - orphaned helper functions */
/* static Point Local_ConstrainToRect(Point windowPos, const Rect* constraintRect);
static Point Local_ConstrainToScreen(Point windowPos); */
#endif
#if 0  /* UNUSED forward declarations for commented-out functions */
/* static Point Local_CalculateConstrainedWindowPosition(Point mousePt);
static void Local_EndDragFeedback(void);
static void Local_UpdateDragFeedback(Point currentPt);
static void Local_StartDragFeedback(void);
static void Local_CleanupDragState(void); */
#endif


/* ============================================================================
 * Dragging Constants and Configuration
 * ============================================================================ */

/* Drag behavior constants */
#define DRAG_THRESHOLD              4    /* Minimum pixels to start drag */
#define SNAP_DISTANCE              8    /* Snap-to-edge distance */
#define SCREEN_EDGE_MARGIN         4    /* Minimum margin from screen edge */
#define TITLE_BAR_HEIGHT           20   /* Height of title bar */
#define TITLE_BAR_DRAG_MARGIN      50   /* Minimum title bar visible */
#define DRAG_UPDATE_INTERVAL       16   /* Update interval in milliseconds */
/* Window size constraints are defined in WindowManagerInternal.h */

#if 0  /* UNUSED: Drag state system - preserved for possible future use */
/* Drag feedback modes */
typedef enum {
    kDragFeedbackNone = 0,      /* No visual feedback */
    kDragFeedbackOutline = 1,   /* Gray outline */
    kDragFeedbackWindow = 2,    /* Move actual window */
    kDragFeedbackSolid = 3      /* Solid gray rectangle */
} DragFeedbackMode;

/* Forward declarations for platform functions */
typedef enum {
    kDragFeedbackNone_Local = 0,
    kDragFeedbackOutline_Local = 1,
    kDragFeedbackWindow_Local = 2,
    kDragFeedbackSolid_Local = 3
} DragFeedbackMode_Local;

/* Drag constraint modes */
typedef enum {
    kDragConstraintNone = 0,     /* No constraints */
    kDragConstraintScreen = 1,   /* Constrain to screen */
    kDragConstraintRect = 2,     /* Constrain to rectangle */
    kDragConstraintCustom = 3    /* Custom constraint function */
} DragConstraintMode;

/* Drag state structure */
typedef struct DragState {
    WindowPtr window;           /* Window being dragged */
    Point startPoint;           /* Initial mouse position */
    Point currentPoint;         /* Current mouse position */
    Point windowOffset;         /* Offset from mouse to window origin */
    Rect originalBounds;        /* Original window bounds */
    Rect constraintRect;        /* Constraint rectangle */
    DragFeedbackMode feedback;  /* Visual feedback mode */
    DragConstraintMode constraint; /* Constraint mode */
    Boolean active;             /* True if drag is active */
    Boolean hasMoved;           /* True if window has moved */
    RgnHandle dragRgn;          /* Region for drag feedback */
    unsigned long lastUpdate;   /* Last update time */
} DragState;

/* Global drag state */
static DragState g_dragState = {
    NULL, {0, 0}, {0, 0}, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    kDragFeedbackOutline, kDragConstraintScreen, false, false,
    NULL, 0
};
#endif /* Drag state system */

#if 0  /* UNUSED forward declarations */
static DragFeedbackMode_Local Local_GetPreferredDragFeedback(void);
Boolean Platform_IsSnapToEdgesEnabled(void);
#endif
static void Local_InvalidateScreenRegion(RgnHandle region);
static Boolean Local_RectsIntersect(const Rect* rect1, const Rect* rect2);

/* Finder About box helpers (avoid direct Finder dependencies elsewhere) */
extern Boolean AboutWindow_IsOurs(WindowPtr w);
extern Boolean AboutWindow_HandleUpdate(WindowPtr w);
#if 0  /* UNUSED forward declarations */
static Point Local_CalculateFinalWindowPosition(Point mousePt);
static void Local_InitializeDragState(WindowPtr theWindow, Point startPt, const Rect* bounds);
#endif

#if 0  /* UNUSED: Local_GetPreferredDragFeedback - orphaned by commenting out drag state functions */
/* Implementation of Local_GetPreferredDragFeedback */
static DragFeedbackMode_Local Local_GetPreferredDragFeedback(void) {
    /* Return outline feedback mode as default */
    return kDragFeedbackOutline_Local;
}
#endif /* Local_GetPreferredDragFeedback */

/* ============================================================================
 * Window Movement Functions
 * ============================================================================ */

void MoveWindow(WindowPtr theWindow, short hGlobal, short vGlobal, Boolean front) {
    if (theWindow == NULL) return;

    WM_DEBUG("MoveWindow: Moving window to (%d, %d), front = %s",
             hGlobal, vGlobal, front ? "true" : "false");

    /* CRITICAL: Get current global position from strucRgn, NOT from portRect
     * portRect is ALWAYS in LOCAL coordinates (0,0,width,height)
     * strucRgn contains the GLOBAL screen position */
    Rect currentGlobalBounds;
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        currentGlobalBounds = (*(theWindow->strucRgn))->rgnBBox;
    } else {
        /* Fallback: use portRect but this is wrong if already corrupted */
        currentGlobalBounds = theWindow->port.portRect;
    }

    /* Calculate movement offset */
    short deltaH = hGlobal - currentGlobalBounds.left;
    short deltaV = vGlobal - currentGlobalBounds.top;

    /* Check if window actually needs to move */
    if (deltaH == 0 && deltaV == 0) {
        if (front) {
            SelectWindow(theWindow);
        }
        return;
    }

    /* Validate new position */
    Rect newBounds = currentGlobalBounds;
    WM_OffsetRect(&newBounds, deltaH, deltaV);

    if (!WM_ValidateWindowPosition(theWindow, &newBounds)) {
        WM_DEBUG("MoveWindow: Invalid window position, constraining");
        WM_ConstrainWindowPosition(theWindow, &newBounds);
        deltaH = newBounds.left - currentGlobalBounds.left;
        deltaV = newBounds.top - currentGlobalBounds.top;
    }

    /* Save old structure region for invalidation */
    RgnHandle oldStrucRgn = Platform_NewRgn();
    if (oldStrucRgn && theWindow->strucRgn) {
        Platform_CopyRgn(theWindow->strucRgn, oldStrucRgn);
    }

    /* CRITICAL: Do NOT modify portRect - it must stay in LOCAL coordinates!
     * Only update the window regions which are in GLOBAL coordinates */

    /* Update window regions */
    if (theWindow->strucRgn) {
        Platform_OffsetRgn(theWindow->strucRgn, deltaH, deltaV);
    }
    if (theWindow->contRgn) {
        Platform_OffsetRgn(theWindow->contRgn, deltaH, deltaV);
    }
    if (theWindow->updateRgn) {
        Platform_OffsetRgn(theWindow->updateRgn, deltaH, deltaV);
    }

    /* CRITICAL: Update portBits for Direct Framebuffer approach
     *
     * When window moves, we must update portBits.bounds to reflect the new GLOBAL position.
     * portBits.bounds must be in GLOBAL screen coordinates for BeginUpdate to calculate
     * the correct framebuffer fill region.
     *
     * Architecture: baseAddr = framebuffer (full buffer), bounds = GLOBAL position
     * When filling: fbOffset = bounds.top * pitch + bounds.left * 4
     */
    if (theWindow->contRgn && *(theWindow->contRgn)) {
        Rect newContentRect = (*(theWindow->contRgn))->rgnBBox;

        /* Update portBits.bounds to GLOBAL screen coordinates */
        SetRect(&theWindow->port.portBits.bounds,
                newContentRect.left, newContentRect.top,
                newContentRect.right, newContentRect.bottom);

        /* Keep baseAddr pointing to full framebuffer (not offset) */
        extern void* framebuffer;
        theWindow->port.portBits.baseAddr = (Ptr)framebuffer;
    }

    /* Move native platform window using new global position from strucRgn */
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        Rect newGlobalBounds = (*(theWindow->strucRgn))->rgnBBox;
        Platform_MoveNativeWindow(theWindow, newGlobalBounds.left, newGlobalBounds.top);
    }

    /* Bring to front if requested */
    if (front) {
        SelectWindow(theWindow);
    }

    /* Invalidate old and new positions */
    if (theWindow->visible) {
        if (oldStrucRgn) {
            Local_InvalidateScreenRegion(oldStrucRgn);
        }
        if (theWindow->strucRgn) {
            Local_InvalidateScreenRegion(theWindow->strucRgn);
        }
    }

    /* Clean up */
    if (oldStrucRgn) {
        Platform_DisposeRgn(oldStrucRgn);
    }

    /* Update window layering if needed */
    WM_UpdateWindowVisibility(theWindow);

    /* Debug log removed to avoid unused variable when logging is compiled out */
}

/* ============================================================================
 * Window Dragging Implementation
 * ============================================================================ */

void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect) {
    extern void serial_puts(const char *str);
    serial_puts("[WM_DRAG] DragWindow ENTRY\n");

    if (theWindow == NULL) {
        serial_puts("[WM_DRAG] DragWindow: NULL window, returning\n");
        return;
    }

    /* Get GLOBAL window bounds from strucRgn - System 7 faithful */
    Rect frameG;
    if (theWindow->strucRgn && *(theWindow->strucRgn)) {
        frameG = (*(theWindow->strucRgn))->rgnBBox;  /* GLOBAL coords */
        WM_LOG_TRACE("DragWindow: frameG=(%d,%d,%d,%d)\n",
                     frameG.top, frameG.left, frameG.bottom, frameG.right);
    } else {
        /* Should never happen if Window Manager initialized properly */
        WM_LOG_ERROR("DragWindow: ERROR - no strucRgn!\n");
        return;
    }

    /* Calculate mouse offset from window origin (GLOBAL coords) */
    Point offset;
    offset.h = startPt.h - frameG.left;
    offset.v = startPt.v - frameG.top;

    const short windowWidth = frameG.right - frameG.left;
    const short windowHeight = frameG.bottom - frameG.top;

    /* Use provided bounds or default to screen minus menubar */
    Rect dragBounds;
    if (boundsRect) {
        dragBounds = *boundsRect;
    } else {
        extern QDGlobals qd;
        dragBounds.top = 20;     /* menubar height */
        dragBounds.left = qd.screenBits.bounds.left;
        dragBounds.bottom = qd.screenBits.bounds.bottom;
        dragBounds.right = qd.screenBits.bounds.right;
    }

    /* Ensure callers cannot allow windows to overlap the menu bar */
    if (dragBounds.top < TITLE_BAR_HEIGHT) {
        dragBounds.top = TITLE_BAR_HEIGHT;
    }

    /* Guard against degenerate rectangles that would reject movement */
    if (dragBounds.bottom <= dragBounds.top) {
        dragBounds.bottom = dragBounds.top + windowHeight;
    }
    if (dragBounds.right <= dragBounds.left) {
        dragBounds.right = dragBounds.left + windowWidth;
    }
    if (dragBounds.bottom - dragBounds.top < windowHeight) {
        dragBounds.bottom = dragBounds.top + windowHeight;
    }
    if (dragBounds.right - dragBounds.left < windowWidth) {
        dragBounds.right = dragBounds.left + windowWidth;
    }

    /* System 7 modal drag loop using StillDown/GetMouse */
    extern Boolean StillDown(void);
    extern void GetMouse(Point* mouseLoc);

    Point ptG;
    Point lastPos = startPt;
    Boolean moved = false;
    Rect oldBounds = frameG;

    /* XOR outline state */
    Rect dragOutline = frameG;
    Boolean outlineDrawn = false;

    /* QuickDraw functions for XOR outline */
    extern void QDPlatform_FlushScreen(void);
    extern void GetWMgrPort(GrafPtr* port);
    extern void SetPort(GrafPtr port);

    /* Set graphics port to Window Manager port for XOR drawing */
    GrafPtr wmPort;
    GetWMgrPort(&wmPort);
    if (wmPort) {
        SetPort(wmPort);
    }

    /* CRITICAL: Erase the window from its current position BEFORE starting XOR drag
     * This prevents leaving white artifacts where the title bar was */
    extern void HideWindow(WindowPtr window);
    extern void ShowWindow(WindowPtr window);
    extern void InvalidateCursor(void);  /* Force cursor redraw */
    Boolean wasVisible = theWindow->visible;
    (void)wasVisible; /* Avoid unused warning; keep window visible during drag */
    /* NOTE: Do not HideWindow/PaintBehind here. Using XOR outline without
     * erasing the window avoids a heap overwrite we are chasing in the
     * hide/paint path. This matches classic XOR-drag behavior visually,
     * and prevents the freeze observed at drag start. */

    /* Invalidate cursor state before drag to prevent stale background artifacts */
    InvalidateCursor();

    /* Main modal drag loop - System 7 idiom with XOR outline feedback
     * With safety timeout to prevent infinite loop if StillDown() gets stuck */
    extern void EventPumpYield(void);
    extern void UpdateCursorDisplay(void);
    UInt32 loopCount = 0;
    const UInt32 MAX_DRAG_ITERATIONS = 100000;  /* Safety timeout: ~1666 seconds at 60Hz */

    while (StillDown() && loopCount < MAX_DRAG_ITERATIONS) {
        loopCount++;

        /* Poll hardware for new input events (mouse button state) */
        EventPumpYield();

        /* Update cursor display (drag has its own event loop that bypasses main loop) */
        UpdateCursorDisplay();

        GetMouse(&ptG);  /* Returns GLOBAL coords */

        /* Only process if mouse moved */
        if (ptG.h != lastPos.h || ptG.v != lastPos.v) {
            /* Calculate new window position */
            short newLeft = ptG.h - offset.h;
            short newTop = ptG.v - offset.v;

            /* Constrain to drag bounds */
            if (newLeft < dragBounds.left)
                newLeft = dragBounds.left;
            if (newTop < dragBounds.top)
                newTop = dragBounds.top;
            if (newLeft + windowWidth > dragBounds.right)
                newLeft = dragBounds.right - windowWidth;
            if (newTop + windowHeight > dragBounds.bottom)
                newTop = dragBounds.bottom - windowHeight;

            /* XOR outline feedback (authentic Mac OS behavior) */
            if (newLeft != dragOutline.left || newTop != dragOutline.top) {
                /* Erase old outline if it exists (XOR erases by redrawing) */
                if (outlineDrawn) {
                    extern void InvertRect(const Rect* rect);
                    WM_LOG_TRACE("DragWindow: Erasing old outline at (%d,%d,%d,%d)\n",
                                 dragOutline.left, dragOutline.top, dragOutline.right, dragOutline.bottom);
                    InvertRect(&dragOutline);  /* Use InvertRect for simple XOR */
                    QDPlatform_FlushScreen();  /* Force screen update */
                }

                /* Calculate new outline position */
                dragOutline.left = newLeft;
                dragOutline.top = newTop;
                dragOutline.right = newLeft + windowWidth;
                dragOutline.bottom = newTop + windowHeight;

                /* Draw new outline */
                extern void InvertRect(const Rect* rect);
                WM_LOG_TRACE("DragWindow: Drawing new outline at (%d,%d,%d,%d)\n",
                             dragOutline.left, dragOutline.top, dragOutline.right, dragOutline.bottom);
                InvertRect(&dragOutline);  /* Use InvertRect for simple XOR */
                QDPlatform_FlushScreen();  /* Force screen update */
                outlineDrawn = true;

                moved = true;
            }

            lastPos = ptG;
        }
    }

    /* Check if we hit the safety timeout */
    if (loopCount >= MAX_DRAG_ITERATIONS) {
        WM_LOG_ERROR("DragWindow: TIMEOUT! Loop iterated %u times, StillDown() never returned false!\n", loopCount);
        WM_LOG_ERROR("DragWindow: This indicates mouse button tracking is broken.\n");
    } else {
        WM_LOG_DEBUG("DragWindow: Exited drag loop normally after %u iterations\n", loopCount);
    }

    /* Erase final outline before moving window (XOR erases by redrawing) */
    if (outlineDrawn) {
        extern void InvertRect(const Rect* rect);
        WM_LOG_TRACE("DragWindow: Erasing final outline\n");
        InvertRect(&dragOutline);
        QDPlatform_FlushScreen();  /* Force screen update */
    }

    /* Invalidate cursor state after drag to force fresh redraw */
    InvalidateCursor();

    if (moved) {
        WM_LOG_DEBUG("DragWindow: Final MoveWindow to (%d,%d)\n", dragOutline.left, dragOutline.top);

        /* Create a region for the old window position to invalidate */
        extern RgnHandle NewRgn(void);
        extern void RectRgn(RgnHandle rgn, const Rect* r);
        extern void DisposeRgn(RgnHandle rgn);
        extern void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn);
        extern void PaintOne(WindowPtr window, RgnHandle clobberedRgn);
        extern void CalcVis(WindowPtr window);

        WM_LOG_DEBUG("DragWindow: About to call NewRgn()\n");
        serial_puts("[MEM] DragWindow before NewRgn(oldRgn)\n");
        MemoryManager_CheckSuspectBlock("pre_NewRgn_old");
        RgnHandle oldRgn = NewRgn();
        MemoryManager_CheckSuspectBlock("post_NewRgn_old");
        WM_LOG_DEBUG("DragWindow: NewRgn() returned %p\n", oldRgn);
        RectRgn(oldRgn, &oldBounds);

        WM_LOG_TRACE("DragWindow: Created oldRgn for bounds (%d,%d,%d,%d)\n",
                     oldBounds.left, oldBounds.top, oldBounds.right, oldBounds.bottom);

        /* Move the window to new position */
        MoveWindow(theWindow, dragOutline.left, dragOutline.top, false);

        /* Recalculate window visibility */
        CalcVis(theWindow);

        /* Calculate the uncovered desktop region: old position minus new position */
        extern void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
        RgnHandle uncoveredRgn = NewRgn();
        MemoryManager_CheckSuspectBlock("post_NewRgn_uncovered");
        RgnHandle newRgn = NewRgn();
        MemoryManager_CheckSuspectBlock("post_NewRgn_new");
        if (theWindow->strucRgn && *theWindow->strucRgn) {
            CopyRgn(theWindow->strucRgn, newRgn);
        }

        /* Compute: oldRgn - newRgn = region that was uncovered */
        DiffRgn(oldRgn, newRgn, uncoveredRgn);

        WM_LOG_TRACE("DragWindow: Computed uncovered region\n");

        /* Paint the desktop pattern in the uncovered region FIRST */
        extern void GetWMgrPort(GrafPtr* port);
        extern void SetClip(RgnHandle rgn);
        typedef void (*DeskHookProc)(RgnHandle rgn);
        extern DeskHookProc g_deskHook;  /* From sys71_stubs.c */

        GrafPtr savePort;
        GetPort(&savePort);
        GrafPtr wmgrPort;
        GetWMgrPort(&wmgrPort);
        SetPort(wmgrPort);

        /* Set clip to only the uncovered region */
        SetClip(uncoveredRgn);

        /* Call the desk hook to paint the desktop pattern in the uncovered region */
        if (g_deskHook) {
            WM_LOG_TRACE("DragWindow: Calling DeskHook for uncovered region\n");
            g_deskHook(uncoveredRgn);
        }

        /* Reset clip */
        extern void SetRectRgn(RgnHandle rgn, short left, short top, short right, short bottom);
        RgnHandle fullClip = NewRgn();
        SetRectRgn(fullClip, -32768, -32768, 32767, 32767);
        SetClip(fullClip);
        DisposeRgn(fullClip);

        SetPort(savePort);
        WM_LOG_TRACE("DragWindow: Desktop repainted in uncovered region\n");

        /* Repaint windows behind in the uncovered region */
        PaintBehind(theWindow->nextWindow, uncoveredRgn);
        WM_LOG_TRACE("DragWindow: PaintBehind called for uncovered region\n");

        /* Restore window visibility if it was visible before drag */
        if (wasVisible) {
            theWindow->visible = true;
        }

        /* Repaint the window itself at new position */
        PaintOne(theWindow, NULL);
        WM_LOG_TRACE("DragWindow: PaintOne called for window at new position\n");

        /* Invalidate window content to trigger updateEvt for content redraw */
        if (theWindow->contRgn) {
            extern void InvalRgn(RgnHandle badRgn);
            GrafPtr oldPort;
            GetPort(&oldPort);
            SetPort((GrafPtr)theWindow);
            InvalRgn(theWindow->contRgn);
            SetPort(oldPort);
            WM_LOG_TRACE("DragWindow: Invalidated window content region\n");
        }

        /* WORKAROUND: Directly redraw window content since update events aren't flowing through */
        WM_LOG_TRACE("DragWindow: Checking refCon=0x%08lx (DISK=0x%08x, TRSH=0x%08x)\n",
                     (unsigned long)theWindow->refCon, 'DISK', 'TRSH');
        if (theWindow->refCon == 'DISK' || theWindow->refCon == 'TRSH') {
            extern void FolderWindow_Draw(WindowPtr w);
            WM_LOG_TRACE("DragWindow: Calling FolderWindow_Draw\n");
            FolderWindow_Draw(theWindow);
            WM_LOG_TRACE("DragWindow: Direct content redraw complete\n");
        } else if (AboutWindow_IsOurs(theWindow)) {
            WM_LOG_TRACE("DragWindow: About window detected, forcing redraw\n");
            AboutWindow_HandleUpdate(theWindow);
            WM_LOG_TRACE("DragWindow: About window redraw complete\n");
        } else {
            WM_LOG_TRACE("DragWindow: refCon doesn't match, skipping content redraw\n");
        }

        /* Clean up new regions */
        DisposeRgn(uncoveredRgn);
        DisposeRgn(newRgn);

        /* Clean up */
        WM_LOG_TRACE("DragWindow: About to DisposeRgn\n");
        DisposeRgn(oldRgn);
        WM_LOG_TRACE("DragWindow: DisposeRgn completed\n");

        /* Force screen update */
        extern void QDPlatform_FlushScreen(void);
        QDPlatform_FlushScreen();

        if (wasVisible) {
            theWindow->visible = true;
        }
    }
    else if (wasVisible) {
        /* Restore window visibility even if it never moved */
        theWindow->visible = true;
        PaintOne(theWindow, NULL);
        if (theWindow->contRgn) {
            extern void InvalRgn(RgnHandle badRgn);
            GrafPtr oldPort;
            GetPort(&oldPort);
            SetPort((GrafPtr)theWindow);
            InvalRgn(theWindow->contRgn);
            SetPort(oldPort);
        }
    }

    WM_LOG_TRACE("DragWindow EXIT: moved=%d\n", moved);
}

/* ============================================================================
 * Drag State Management
 * ============================================================================ */

#if 0  /* UNUSED: Local_InitializeDragState - preserved for possible future use */
static void Local_InitializeDragState(WindowPtr window, Point startPt, const Rect* boundsRect) {
    WM_DEBUG("Local_InitializeDragState: Initializing drag state");

    /* Clear previous state */
    Local_CleanupDragState();

    /* Initialize drag state */
    g_dragState.window = window;
    g_dragState.startPoint = startPt;
    g_dragState.currentPoint = startPt;
    g_dragState.originalBounds = window->port.portRect;
    g_dragState.active = true;
    g_dragState.hasMoved = false;

    /* Calculate offset from mouse to window origin */
    g_dragState.windowOffset.h = startPt.h - window->port.portRect.left;
    g_dragState.windowOffset.v = startPt.v - window->port.portRect.top;

    /* Set constraint rectangle */
    if (boundsRect) {
        g_dragState.constraintRect = *boundsRect;
        g_dragState.constraint = kDragConstraintRect;
    } else {
        /* Use screen bounds as default constraint */
        Platform_GetScreenBounds(&g_dragState.constraintRect);
        g_dragState.constraint = kDragConstraintScreen;
    }

    /* Configure drag feedback mode */
    g_dragState.feedback = (DragFeedbackMode)Local_GetPreferredDragFeedback();

    /* Create drag region if needed for outline feedback */
    if (g_dragState.feedback == kDragFeedbackOutline) {
        g_dragState.dragRgn = Platform_NewRgn();
        if (g_dragState.dragRgn && window->strucRgn) {
            Platform_CopyRgn(window->strucRgn, g_dragState.dragRgn);
        }
    }

    WM_DEBUG("Local_InitializeDragState: Drag state initialized");
}
#endif /* Local_InitializeDragState */

#if 0  /* UNUSED: Local_CleanupDragState - orphaned by commenting out Local_InitializeDragState */
static void Local_CleanupDragState(void) {
    if (!g_dragState.active) return;

    WM_DEBUG("Local_CleanupDragState: Cleaning up drag state");

    /* Dispose of drag region */
    if (g_dragState.dragRgn) {
        Platform_DisposeRgn(g_dragState.dragRgn);
        g_dragState.dragRgn = NULL;
    }

    /* Clear state */
    memset(&g_dragState, 0, sizeof(DragState));

    WM_DEBUG("Local_CleanupDragState: Cleanup complete");
}
#endif /* Local_CleanupDragState */

/* ============================================================================
 * Drag Feedback Management
 * ============================================================================ */

#if 0  /* UNUSED: Local_StartDragFeedback - preserved for possible future use */
static void Local_StartDragFeedback(void) {
    WM_DEBUG("Local_StartDragFeedback: Starting drag feedback, mode = %d", g_dragState.feedback);

    switch (g_dragState.feedback) {
        case kDragFeedbackOutline:
            if (g_dragState.dragRgn) {
                Rect rgnBounds;
                Platform_GetRegionBounds(g_dragState.dragRgn, &rgnBounds);
                Platform_ShowDragOutline(&rgnBounds);
            }
            break;

        case kDragFeedbackWindow:
            /* No initial feedback needed - window moves in real time */
            break;

        case kDragFeedbackSolid:
            Platform_ShowDragRect(&g_dragState.originalBounds);
            break;

        case kDragFeedbackNone:
        default:
            /* No feedback */
            break;
    }
}
#endif /* Local_StartDragFeedback */

#if 0  /* UNUSED: Local_UpdateDragFeedback - preserved for possible future use */
static void Local_UpdateDragFeedback(Point currentPt) {
    Point windowPos = Local_CalculateConstrainedWindowPosition(currentPt);

    switch (g_dragState.feedback) {
        case kDragFeedbackOutline:
            if (g_dragState.dragRgn) {
                Rect oldRgnBounds, newRgnBounds;
                Platform_GetRegionBounds(g_dragState.dragRgn, &oldRgnBounds);
                newRgnBounds = oldRgnBounds;
                WM_OffsetRect(&newRgnBounds,
                             windowPos.h - g_dragState.currentPoint.h,
                             windowPos.v - g_dragState.currentPoint.v);
                Platform_UpdateDragOutline(&oldRgnBounds, &newRgnBounds);
            }
            break;

        case kDragFeedbackWindow:
            /* Move actual window in real time */
            MoveWindow(g_dragState.window, windowPos.h, windowPos.v, false);
            break;

        case kDragFeedbackSolid:
            {
                Rect oldDragRect = g_dragState.originalBounds;
                WM_OffsetRect(&oldDragRect,
                             g_dragState.currentPoint.h - g_dragState.originalBounds.left,
                             g_dragState.currentPoint.v - g_dragState.originalBounds.top);
                Rect newDragRect = g_dragState.originalBounds;
                WM_OffsetRect(&newDragRect,
                             windowPos.h - g_dragState.originalBounds.left,
                             windowPos.v - g_dragState.originalBounds.top);
                Platform_UpdateDragRect(&oldDragRect, &newDragRect);
            }
            break;

        case kDragFeedbackNone:
        default:
            /* No feedback updates */
            break;
    }
}
#endif /* Local_UpdateDragFeedback */

#if 0  /* UNUSED: Local_EndDragFeedback - preserved for possible future use */
static void Local_EndDragFeedback(void) {
    WM_DEBUG("Local_EndDragFeedback: Ending drag feedback");

    switch (g_dragState.feedback) {
        case kDragFeedbackOutline:
            if (g_dragState.dragRgn) {
                Rect rgnBounds;
                Platform_GetRegionBounds(g_dragState.dragRgn, &rgnBounds);
                Platform_HideDragOutline(&rgnBounds);
            }
            break;

        case kDragFeedbackWindow:
            /* Window is already in final position */
            break;

        case kDragFeedbackSolid:
            {
                Rect dragRect = g_dragState.originalBounds;
                WM_OffsetRect(&dragRect,
                             g_dragState.currentPoint.h - g_dragState.originalBounds.left,
                             g_dragState.currentPoint.v - g_dragState.originalBounds.top);
                Platform_HideDragRect(&dragRect);
            }
            break;

        case kDragFeedbackNone:
        default:
            /* No feedback to hide */
            break;
    }
}
#endif /* Local_EndDragFeedback */

/* ============================================================================
 * Position Calculation and Constraints
 * ============================================================================ */

#if 0  /* UNUSED: Local_CalculateConstrainedWindowPosition - orphaned by commenting out drag feedback functions */
static Point Local_CalculateConstrainedWindowPosition(Point mousePt) {
    /* Calculate unconstrained window position */
    Point windowPos;
    windowPos.h = mousePt.h - g_dragState.windowOffset.h;
    windowPos.v = mousePt.v - g_dragState.windowOffset.v;

    /* Apply constraints */
    switch (g_dragState.constraint) {
        case kDragConstraintScreen:
            windowPos = Local_ConstrainToScreen(windowPos);
            break;

        case kDragConstraintRect:
            windowPos = Local_ConstrainToRect(windowPos, &g_dragState.constraintRect);
            break;

        case kDragConstraintCustom:
            /* TODO: Implement custom constraint callbacks */
            break;

        case kDragConstraintNone:
        default:
            /* No constraints */
            break;
    }

    return windowPos;
}
#endif /* Local_CalculateConstrainedWindowPosition */

#if 0  /* UNUSED: Local_CalculateFinalWindowPosition - preserved for possible future use */
static Point Local_CalculateFinalWindowPosition(Point mousePt) {
    Point windowPos = Local_CalculateConstrainedWindowPosition(mousePt);

    /* Apply snapping if enabled */
    if (Platform_IsSnapToEdgesEnabled()) {
        windowPos = Local_ApplySnapToEdges(windowPos);
    }

    /* Return offset from original position */
    Point offset;
    offset.h = windowPos.h - g_dragState.originalBounds.left;
    offset.v = windowPos.v - g_dragState.originalBounds.top;

    return offset;
}
#endif /* Local_CalculateFinalWindowPosition */

#if 0  /* UNUSED: Local_ConstrainToScreen - orphaned by commenting out Local_CalculateConstrainedWindowPosition */
static Point Local_ConstrainToScreen(Point windowPos) {
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Calculate window bounds at proposed position */
    Rect windowBounds = g_dragState.originalBounds;
    WM_OffsetRect(&windowBounds,
                 windowPos.h - g_dragState.originalBounds.left,
                 windowPos.v - g_dragState.originalBounds.top);

    /* Ensure title bar remains visible */
    short titleBarBottom = windowBounds.top + TITLE_BAR_HEIGHT;
    if (titleBarBottom < screenBounds.top + SCREEN_EDGE_MARGIN) {
        windowPos.v = screenBounds.top + SCREEN_EDGE_MARGIN - TITLE_BAR_HEIGHT;
    }

    /* Ensure minimum title bar width is visible */
    if (windowBounds.right < screenBounds.left + TITLE_BAR_DRAG_MARGIN) {
        windowPos.h = screenBounds.left + TITLE_BAR_DRAG_MARGIN - WM_RECT_WIDTH(&g_dragState.originalBounds);
    }
    if (windowBounds.left > screenBounds.right - TITLE_BAR_DRAG_MARGIN) {
        windowPos.h = screenBounds.right - TITLE_BAR_DRAG_MARGIN;
    }

    /* Prevent window from going too far off screen vertically */
    if (windowBounds.top > screenBounds.bottom - TITLE_BAR_HEIGHT) {
        windowPos.v = screenBounds.bottom - TITLE_BAR_HEIGHT;
    }

    return windowPos;
}
#endif /* Local_ConstrainToScreen */

#if 0  /* UNUSED: Local_ConstrainToRect - orphaned by commenting out Local_CalculateConstrainedWindowPosition */
static Point Local_ConstrainToRect(Point windowPos, const Rect* constraintRect) {
    if (constraintRect == NULL) return windowPos;

    /* Calculate window bounds at proposed position */
    Rect windowBounds = g_dragState.originalBounds;
    WM_OffsetRect(&windowBounds,
                 windowPos.h - g_dragState.originalBounds.left,
                 windowPos.v - g_dragState.originalBounds.top);

    /* Constrain to rectangle */
    if (windowBounds.left < constraintRect->left) {
        windowPos.h = constraintRect->left;
    }
    if (windowBounds.top < constraintRect->top) {
        windowPos.v = constraintRect->top;
    }
    if (windowBounds.right > constraintRect->right) {
        windowPos.h = constraintRect->right - WM_RECT_WIDTH(&g_dragState.originalBounds);
    }
    if (windowBounds.bottom > constraintRect->bottom) {
        windowPos.v = constraintRect->bottom - WM_RECT_HEIGHT(&g_dragState.originalBounds);
    }

    return windowPos;
}
#endif /* Local_ConstrainToRect */

#if 0  /* UNUSED: Local_ApplySnapToEdges - orphaned by commenting out Local_CalculateFinalWindowPosition */
static Point Local_ApplySnapToEdges(Point windowPos) {
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Calculate window bounds */
    Rect windowBounds = g_dragState.originalBounds;
    WM_OffsetRect(&windowBounds,
                 windowPos.h - g_dragState.originalBounds.left,
                 windowPos.v - g_dragState.originalBounds.top);

    /* Snap to screen edges */
    if (abs(windowBounds.left - screenBounds.left) <= SNAP_DISTANCE) {
        windowPos.h = screenBounds.left;
    }
    if (abs(windowBounds.right - screenBounds.right) <= SNAP_DISTANCE) {
        windowPos.h = screenBounds.right - WM_RECT_WIDTH(&windowBounds);
    }
    if (abs(windowBounds.top - screenBounds.top) <= SNAP_DISTANCE) {
        windowPos.v = screenBounds.top;
    }
    if (abs(windowBounds.bottom - screenBounds.bottom) <= SNAP_DISTANCE) {
        windowPos.v = screenBounds.bottom - WM_RECT_HEIGHT(&windowBounds);
    }

    /* TODO: Snap to other windows if enabled */

    return windowPos;
}
#endif /* Local_ApplySnapToEdges */

/* ============================================================================
 * Window Position Validation
 * ============================================================================ */

Boolean WM_ValidateWindowPosition(WindowPtr window, const Rect* bounds) {
    if (window == NULL || bounds == NULL) return false;

    /* Check for valid rectangle */
    if (!WM_VALID_RECT(bounds)) {
        WM_DEBUG("WM_ValidateWindowPosition: Invalid rectangle");
        return false;
    }

    /* Check minimum window size */
    short width = bounds->right - bounds->left;
    short height = bounds->bottom - bounds->top;

    if (width < MIN_WINDOW_WIDTH || height < MIN_WINDOW_HEIGHT) {
        WM_DEBUG("WM_ValidateWindowPosition: Window too small (%dx%d)", width, height);
        return false;
    }

    /* Check maximum window size */
    if (width > MAX_WINDOW_WIDTH || height > MAX_WINDOW_HEIGHT) {
        WM_DEBUG("WM_ValidateWindowPosition: Window too large (%dx%d)", width, height);
        return false;
    }

    /* Check screen bounds */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Ensure some part of title bar is visible */
    Rect titleBarRect = *bounds;
    titleBarRect.top -= TITLE_BAR_HEIGHT;
    titleBarRect.bottom = bounds->top;

    if (!Local_RectsIntersect(&titleBarRect, &screenBounds)) {
        WM_DEBUG("WM_ValidateWindowPosition: Title bar not visible");
        return false;
    }

    return true;
}

void WM_ConstrainWindowPosition(WindowPtr window, Rect* bounds) {
    if (window == NULL || bounds == NULL) return;

    WM_DEBUG("WM_ConstrainWindowPosition: Constraining window position");

    /* Constrain size first */
    short width = bounds->right - bounds->left;
    short height = bounds->bottom - bounds->top;

    if (width < MIN_WINDOW_WIDTH) {
        bounds->right = bounds->left + MIN_WINDOW_WIDTH;
    } else if (width > MAX_WINDOW_WIDTH) {
        bounds->right = bounds->left + MAX_WINDOW_WIDTH;
    }

    if (height < MIN_WINDOW_HEIGHT) {
        bounds->bottom = bounds->top + MIN_WINDOW_HEIGHT;
    } else if (height > MAX_WINDOW_HEIGHT) {
        bounds->bottom = bounds->top + MAX_WINDOW_HEIGHT;
    }

    /* Constrain position to screen */
    Rect screenBounds;
    Platform_GetScreenBounds(&screenBounds);

    /* Ensure title bar is visible */
    if (bounds->top > screenBounds.bottom - TITLE_BAR_HEIGHT) {
        short offset = (screenBounds.bottom - TITLE_BAR_HEIGHT) - bounds->top;
        bounds->top += offset;
        bounds->bottom += offset;
    }

    if (bounds->top < screenBounds.top - (height - TITLE_BAR_HEIGHT)) {
        short offset = (screenBounds.top - (height - TITLE_BAR_HEIGHT)) - bounds->top;
        bounds->top += offset;
        bounds->bottom += offset;
    }

    /* Ensure some part of window is horizontally visible */
    if (bounds->right < screenBounds.left + TITLE_BAR_DRAG_MARGIN) {
        short offset = (screenBounds.left + TITLE_BAR_DRAG_MARGIN) - bounds->right;
        bounds->left += offset;
        bounds->right += offset;
    }

    if (bounds->left > screenBounds.right - TITLE_BAR_DRAG_MARGIN) {
        short offset = (screenBounds.right - TITLE_BAR_DRAG_MARGIN) - bounds->left;
        bounds->left += offset;
        bounds->right += offset;
    }

    WM_DEBUG("WM_ConstrainWindowPosition: Constrained to (%d, %d, %d, %d)",
             bounds->left, bounds->top, bounds->right, bounds->bottom);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static Boolean Local_RectsIntersect(const Rect* rect1, const Rect* rect2) {
    if (rect1 == NULL || rect2 == NULL) return false;

    return !(rect1->right <= rect2->left ||
             rect1->left >= rect2->right ||
             rect1->bottom <= rect2->top ||
             rect1->top >= rect2->bottom);
}

static void Local_InvalidateScreenRegion(RgnHandle rgn) {
    if (rgn == NULL) return;

    /* Convert region to screen coordinates and invalidate */
    /* TODO: Implement screen invalidation when graphics system is available */
    WM_DEBUG("Local_InvalidateScreenRegion: Invalidating screen region");
}

/* Platform functions are defined in Platform/WindowPlatform.c */

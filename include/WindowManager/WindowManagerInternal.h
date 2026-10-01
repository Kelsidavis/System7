/*
 * WindowManagerInternal.h - Internal Window Manager Definitions
 *
 * This header contains internal definitions, structures, and function
 * declarations used by the Window Manager implementation. These are
 * not part of the public API but are shared between Window Manager
 * source files.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __WINDOW_MANAGER_INTERNAL_H__
#define __WINDOW_MANAGER_INTERNAL_H__

#include "SystemTypes.h"

#include "../../include/WindowManager/WindowManager.h"
#include "WindowManager/WMLogging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Debug Configuration
 * ============================================================================ */

/* Uncomment to enable debug output */
/* #define DEBUG_WINDOW_MANAGER */

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/* Standard window frame dimensions */
#define WINDOW_TITLE_BAR_HEIGHT    20

#include "SystemTypes.h"
#define WINDOW_FRAME_WIDTH         1
#define WINDOW_CLOSE_BOX_SIZE      12
#define WINDOW_ZOOM_BOX_SIZE       12
#define WINDOW_GROW_BOX_SIZE       15

/* Window drag constraints */
#define MIN_WINDOW_WIDTH           80
#define MIN_WINDOW_HEIGHT          60

#include "SystemTypes.h"
#define MAX_WINDOW_WIDTH           2048
#define MAX_WINDOW_HEIGHT          2048

#include "SystemTypes.h"

/* Update timing */
#define UPDATE_THROTTLE_MS         16  /* ~60 FPS */

/* ============================================================================
 * Internal Enumerations
 * ============================================================================ */

/* Window part visual states (used by WindowParts.c) */
typedef enum {
    kPartStateNormal = 0,
    kPartStatePressed = 1,
    kPartStateHighlighted = 2,
    kPartStateDisabled = 3
} WindowPartState;

/* Window state flags for internal tracking */

/* Window update flags */

/* ============================================================================
 * Platform Abstraction Functions
 * ============================================================================ */

/*
 * Platform initialization and cleanup
 */
void Platform_InitWindowing(void);
void Platform_ShutdownWindowing(void);
Boolean Platform_HasColorQuickDraw(void);

/*
 * Mouse and input
 */
Boolean Platform_IsMouseDown(void);
void Platform_GetMousePosition(Point* pt);
void Platform_WaitTicks(short ticks);
Point Platform_LocalToGlobalPoint(WindowPtr window, Point localPt);
Point Platform_GlobalToLocalPoint(WindowPtr window, Point globalPt);

/*
 * Port and region management
 */
Boolean Platform_InitializePort(GrafPtr port);
Boolean Platform_InitializeColorPort(CGrafPtr port);
Boolean Platform_InitializeWindowPort(WindowPtr window);
Boolean Platform_InitializeColorWindowPort(WindowPtr window);
void Platform_CleanupWindowPort(WindowPtr window);
GrafPtr Platform_GetCurrentPort(void);
void Platform_SetCurrentPort(GrafPtr port);
GrafPtr Platform_GetUpdatePort(WindowPtr window);
void Platform_SetUpdatePort(GrafPtr port);

RgnHandle Platform_NewRgn(void);
void Platform_DisposeRgn(RgnHandle rgn);
void Platform_SetRectRgn(RgnHandle rgn, const Rect* rect);
void Platform_UnionRgn(RgnHandle srcA, RgnHandle srcB, RgnHandle dst);
void Platform_IntersectRgn(RgnHandle srcA, RgnHandle srcB, RgnHandle dst);
void Platform_DiffRgn(RgnHandle srcA, RgnHandle srcB, RgnHandle dst);
Boolean Platform_PtInRgn(Point pt, RgnHandle rgn);
void Platform_GetRegionBounds(RgnHandle rgn, Rect* bounds);
Boolean Platform_EmptyRgn(RgnHandle rgn);
void Platform_CopyRgn(RgnHandle src, RgnHandle dst);
void Platform_SetEmptyRgn(RgnHandle rgn);
void Platform_OffsetRgn(RgnHandle rgn, short dh, short dv);
void Platform_SetClipRgn(GrafPtr port, RgnHandle rgn);

/*
 * Native window management
 */
void Platform_CreateNativeWindow(WindowPtr window);
void Platform_DestroyNativeWindow(WindowPtr window);
void Platform_ShowNativeWindow(WindowPtr window, Boolean show);
void Platform_MoveNativeWindow(WindowPtr window, short h, short v);
void Platform_SizeNativeWindow(WindowPtr window, short w, short h);
void Platform_SetNativeWindowTitle(WindowPtr window, ConstStr255Param title);
void Platform_BringNativeWindowToFront(WindowPtr window);
void Platform_SendNativeWindowBehind(WindowPtr window, WindowPtr behind);
void Platform_UpdateNativeWindowOrder(void);
void Platform_DisableWindow(WindowPtr window);

/*
 * Window drawing and updating
 */
void Platform_InvalidateWindowContent(WindowPtr window);
void Platform_InvalidateWindowFrame(WindowPtr window);
void Platform_InvalidateWindowRect(WindowPtr window, const Rect* rect);
void Platform_BeginWindowDraw(WindowPtr window);
void Platform_EndWindowDraw(WindowPtr window);
void Platform_UpdateWindowColors(WindowPtr window);

/*
 * Window region calculation
 */
void Platform_CalculateWindowRegions(WindowPtr window);
void Platform_GetWindowFrameRect(WindowPtr window, Rect* frameRect);
void Platform_GetWindowContentRect(WindowPtr window, Rect* contentRect);
void Platform_GetWindowTitleBarRect(WindowPtr window, Rect* titleRect);
void Platform_GetWindowCloseBoxRect(WindowPtr window, Rect* closeRect);
void Platform_GetWindowZoomBoxRect(WindowPtr window, Rect* zoomRect);
void Platform_GetWindowGrowBoxRect(WindowPtr window, Rect* growRect);

/*
 * Hit testing
 */
short Platform_WindowHitTest(WindowPtr window, Point pt);
Boolean Platform_PointInWindowPart(WindowPtr window, Point pt, short part);
void Platform_HighlightWindowPart(WindowPtr window, short partCode, Boolean highlight);

/*
 * Window definition procedures
 */
Handle Platform_GetWindowDefProc(short procID);

/*
 * Screen and desktop management
 */
void Platform_GetScreenBounds(Rect* bounds);
void Platform_SetDesktopPattern(const Pattern* pattern);
PixPatHandle Platform_CreateStandardGrayPixPat(void);

/*
 * Color management
 */
void Platform_DisposeCTable(CTabHandle ctab);

/*
 * Event handling
 */
void Platform_PostWindowEvent(WindowPtr window, short eventType, long eventData);
Boolean Platform_ProcessPendingEvents(void);

/*
 * Window feedback and visual effects
 */
void Platform_ShowDragOutline(const Rect* rect);
void Platform_HideDragOutline(const Rect* rect);
void Platform_UpdateDragOutline(const Rect* oldRect, const Rect* newRect);
void Platform_ShowDragRect(const Rect* rect);
void Platform_HideDragRect(const Rect* rect);
void Platform_UpdateDragRect(const Rect* oldRect, const Rect* newRect);
void Platform_ShowSizeFeedback(const Rect* rect);
void Platform_HideSizeFeedback(const Rect* rect);
void Platform_UpdateSizeFeedback(const Rect* oldRect, const Rect* newRect);
void Platform_ShowZoomFrame(const Rect* rect);
void Platform_HideZoomFrame(const Rect* rect);
void Platform_EnableWindow(WindowPtr window);
Boolean Platform_GetPreferredDragFeedback(void);
Boolean Platform_IsResizeFeedbackEnabled(void);
Boolean Platform_IsSnapToEdgesEnabled(void);
Boolean Platform_IsSnapToSizeEnabled(void);
Boolean Platform_IsZoomAnimationEnabled(void);

/* ============================================================================
 * Internal Window Manager Functions
 * ============================================================================ */

/*
 * Window list management (WindowLayering.c)
 */
void WM_RecalculateWindowOrder(void);
void WM_RecalculateAllVisibility(void);
void WM_UpdateWindowVisibility(WindowPtr window);
WindowPtr WM_FindWindowAt(Point pt);
WindowPtr WM_GetNextVisibleWindow(WindowPtr window);
WindowPtr WM_GetPreviousWindow(WindowPtr window);
short WM_GetWindowLayer(WindowPtr window);
void WM_SetWindowLayer(WindowPtr window, short layer);
Boolean WM_IsFloatingWindow(WindowPtr window);
Boolean WM_IsAlertDialog(WindowPtr window);
Boolean WM_WindowsOverlap(WindowPtr window1, WindowPtr window2);

/*
 * Modal window management (WindowLayering.c)
 */
void WM_SetModalWindow(WindowPtr window);
void WM_ClearModalWindow(void);
WindowPtr WM_GetModalWindow(void);
void WM_EnableAllWindows(void);

/*
 * Floating window management (WindowLayering.c)
 */
void WM_AddFloatingWindow(WindowPtr window);
void WM_RemoveFloatingWindow(WindowPtr window);

/*
 * Window intersection and overlap detection (WindowLayering.c)
 */
Boolean WM_WindowIntersectsRect(WindowPtr window, const Rect* rect);
void WM_GetWindowsInRect(const Rect* rect, WindowPtr* windows, short maxWindows, short* numWindows);
WindowPtr WM_GetTopmostWindowInRect(const Rect* rect);

/*
 * Layer update and maintenance (WindowLayering.c)
 */
void WM_InvalidateLayerOrder(void);
Boolean WM_LayersNeedUpdate(void);
void WM_UpdateWindowLayers(void);

/*
 * Window state management
 */
UInt32 WM_CalculateStateChecksum(WindowPtr window);
void WM_UpdateStateChecksum(WindowPtr window);
Boolean WM_ValidateStateChecksum(WindowPtr window);

/*
 * Window drawing coordination
 */
void WM_InvalidateWindowsBelow(WindowPtr topWindow, const Rect* rect);
void WM_InvalidateScreenRegion(RgnHandle rgn);
void WM_InvalGlobalRgn(WindowPtr window, RgnHandle globalRgn);   /* add a global region to updateRgn */
void WM_CalculateWindowVisibility(WindowPtr window);
void WM_UpdateWindowVisibilityStats(WindowPtr window);

/*
 * Window tracking and interaction
 */
Boolean WM_TrackWindowPart(WindowPtr window, Point startPt, short part);
void WM_StartDragFeedback(WindowPtr window, Point startPt);
void WM_UpdateDragFeedback(Point currentPt);
void WM_EndDragFeedback(void);
void WM_InitializeDragState(WindowPtr window, Point startPt);
void WM_CleanupDragState(void);
void WM_StartResizeFeedback(WindowPtr window, Point startPt);
void WM_UpdateResizeFeedback(Point currentPt);
void WM_EndResizeFeedback(void);
void WM_InitializeResizeState(WindowPtr window, Point startPt);
void WM_CleanupResizeState(void);
void WM_AnimateZoom(WindowPtr window, const Rect* fromRect, const Rect* toRect);
void WM_GenerateResizeUpdateEvents(WindowPtr window, const Rect* oldBounds, const Rect* newBounds);
void WM_DisableWindowsBehindModal(WindowPtr modalWindow);
void WM_UpdatePlatformWindowOrder(void);

/*
 * Update management
 */
void WM_ScheduleWindowUpdate(WindowPtr window, WindowUpdateFlags flags);

/*
 * Window metrics and layout
 */
Boolean WM_ValidateWindow(WindowPtr window);
Boolean WM_ValidateRect(const Rect* rect);
void WM_InitializeSnapSizes(void);
void WM_AddSnapSize(short width, short height);
void WM_ApplySnapToSize(Rect* rect);
void WM_ApplySnapToEdges(Rect* rect);
Rect WM_CalculateNewSize(WindowPtr window, Point currentPt, const Rect* limits);
Point WM_CalculateConstrainedWindowPosition(WindowPtr window, Point proposedPos);
Point WM_CalculateFinalWindowPosition(WindowPtr window, Point startPt, Point currentPt);
long WM_CalculateRegionArea(RgnHandle rgn);

/*
 * Window parts and capabilities (WindowParts.c)
 */
Boolean WM_WindowHasGrowBox(WindowPtr window);
Boolean WM_WindowHasZoomBox(WindowPtr window);
Boolean WM_WindowIsZoomed(WindowPtr window);

/*
 * Window Definition Procedures (WDEF) (WindowParts.c)
 */
long WM_StandardWindowDefProc(short varCode, WindowPtr theWindow, short message, long param);
long WM_DialogWindowDefProc(short varCode, WindowPtr theWindow, short message, long param);

/*
 * Window frame drawing (WindowParts.c)
 */
void WM_DrawStandardWindowFrame(WindowPtr window, short varCode);
void WM_DrawDialogWindowFrame(WindowPtr window, short varCode);
void WM_DrawWindowBorder(WindowPtr window);
void WM_DrawDialogBorder(WindowPtr window);
void WM_DrawWindowTitleBar(WindowPtr window);
void WM_DrawWindowTitle(WindowPtr window, const Rect* titleRect);
void WM_DrawWindowCloseBox(WindowPtr window, WindowPartState state);
void WM_DrawWindowZoomBox(WindowPtr window, WindowPartState state);
void WM_DrawGrowIcon(WindowPtr window);
void WM_DrawGrowImage(WindowPtr window);

/*
 * Window region calculation (WindowParts.c)
 */
void WM_CalculateStandardWindowRegions(WindowPtr window, short varCode);
void WM_CalculateDialogWindowRegions(WindowPtr window, short varCode);

/*
 * Window parts initialization (WindowParts.c)
 */
void WM_InitializeWindowParts(WindowPtr window, short varCode);
void WM_InitializeDialogParts(WindowPtr window, short varCode);
void WM_CleanupWindowParts(WindowPtr window);


/*
 * String utilities
 */
void WM_CopyPascalString(ConstStr255Param source, Str255 dest);

/*
 * Geometry utilities
 */
void WM_SetRect(Rect* rect, short left, short top, short right, short bottom);
void WM_OffsetRect(Rect* rect, short dh, short dv);
void WM_InsetRect(Rect* rect, short dh, short dv);
void WM_IntersectRect(const Rect* src1, const Rect* src2, Rect* dst);
Boolean WM_EmptyRect(const Rect* rect);
Boolean WM_PtInRect(Point pt, const Rect* rect);
Boolean WM_RectsIntersect(const Rect* rect1, const Rect* rect2);
short WM_GetRectWidth(const Rect* rect);
short WM_GetRectHeight(const Rect* rect);
void WM_ConstrainToScreen(Rect* rect);
void WM_ConstrainToRect(Rect* rect, const Rect* bounds);
void WM_InterpolateRect(const Rect* from, const Rect* to, Rect* result, short fraction);

/*
 * Error handling and debugging
 */
void WM_Assert(Boolean condition, const char* message);

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/* Window Manager State - Full definition for internal use */
struct WindowManagerState {
    WMgrPort*       wMgrPort;          /* Window Manager graphics port (points to embedded port) */
    CGrafPort*      wMgrCPort;         /* Window Manager color port (points to embedded cPort) */
    WindowPtr       windowList;        /* Head of window list */
    WindowPtr       activeWindow;      /* Currently active window */
    AuxWinHandle    auxWinHead;        /* Auxiliary window list */
    Pattern         desktopPattern;    /* Desktop pattern */
    PixPatHandle    desktopPixPat;     /* Desktop pixel pattern (Color QD) */
    short           nextWindowID;      /* Next window ID to assign */
    Boolean         colorQDAvailable;  /* Color QuickDraw available */
    Boolean         initialized;       /* Window Manager initialized */
    void*           platformData;      /* Platform-specific data */
    GrafPort        port;              /* Embedded GrafPort (static storage - avoids heap conflicts) */
    CGrafPort       cPort;             /* Embedded CGrafPort (static storage - avoids heap conflicts) */
    WindowPtr       ghostWindow;       /* Ghost window for dragging */
    short           menuBarHeight;     /* Menu bar height */
    RgnHandle       grayRgn;           /* Desktop gray region */
    Pattern         deskPattern;       /* Alias for desktopPattern */
    Boolean         isDragging;        /* Window drag in progress */
    Point           dragOffset;        /* Drag offset from window origin */
    Boolean         isGrowing;         /* Window resize in progress */
};

/* Access to Window Manager state */
WindowManagerState* GetWindowManagerState(void);

/* Extended window record for internal state tracking */

/* Window update queue entry */

/* Platform-specific data structure */

/* ============================================================================
 * Global State Extensions
 * ============================================================================ */

/* Extended Window Manager state (internal) */

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/* NOTE: Debug logging macros (WM_LOG_*, WM_DEBUG) are now defined in WMLogging.h */

/* Assertion macro */
#ifdef DEBUG_WINDOW_MANAGER
#define WM_ASSERT(cond, msg) WM_Assert(cond, msg)
#else
#define WM_ASSERT(cond, msg)
#endif

/* Safe pointer checks */
#define WM_VALID_WINDOW(w) ((w) != NULL && (w)->windowKind != 0)
#define WM_VALID_RECT(r) ((r) != NULL && (r)->right > (r)->left && (r)->bottom > (r)->top)
#define WM_VALID_POINT(p) (true) /* Points are always valid */

/* Rectangle utilities */
#define WM_RECT_WIDTH(r) ((r)->right - (r)->left)
#define WM_RECT_HEIGHT(r) ((r)->bottom - (r)->top)

#include "SystemTypes.h"
#define WM_RECT_CENTER_H(r) ((r)->left + WM_RECT_WIDTH(r) / 2)

#include "SystemTypes.h"
#define WM_RECT_CENTER_V(r) ((r)->top + WM_RECT_HEIGHT(r) / 2)

#include "SystemTypes.h"

/* Window property checks */
#define WM_WINDOW_IS_VISIBLE(w) (WM_VALID_WINDOW(w) && (w)->visible)
#define WM_WINDOW_IS_ACTIVE(w) (WM_VALID_WINDOW(w) && (w)->hilited)
#define WM_WINDOW_HAS_CLOSE_BOX(w) (WM_VALID_WINDOW(w) && (w)->goAwayFlag)

#include "SystemTypes.h"

#ifdef __cplusplus
}
#endif

/* The desktop's redraw hook, and redrawing what has been invalidated. */
typedef void (*DeskHookProc)(RgnHandle invalidRgn);
void SetDeskHook(DeskHookProc proc);
void WM_Update(void);

#endif /* __WINDOW_MANAGER_INTERNAL_H__ */
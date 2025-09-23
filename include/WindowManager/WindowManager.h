/*
 * WindowManager.h - Complete Portable Window Manager API
 *
 * This is the main header for the Portable Window Manager implementation
 * that provides exact Apple Macintosh System 7.1 Window Manager compatibility
 * on modern platforms. This implementation is CRITICAL for System 7.1
 * application compatibility as ALL Mac applications depend on windows.
 *
 * The Window Manager provides:
 * - Window creation, disposal, and management
 * - Window layering and z-order management
 * - Window event handling and targeting
 * - Window drawing and update management
 * - Window parts (title bar, close box, zoom box, grow box)
 * - Window dragging, resizing, and zooming
 * - Modal window and dialog support
 * - Desktop pattern and background management
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#ifndef __WINDOW_MANAGER_H__
#define __WINDOW_MANAGER_H__

#include "SystemTypes.h"

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"  /* Include before WindowTypes.h to avoid activeFlag conflict */
#include "WindowManager/WindowTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform Compatibility Layer
 * ============================================================================ */

/* Basic types for Mac OS compatibility */
/* Basic types are defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

#ifndef true
#define true 1
#define false 0
#endif

/* ============================================================================
 * Basic Geometry Types
 * ============================================================================ */

/* Point structure */
/* Point type defined in MacTypes.h */

/* Rectangle structure */
/* Rect type defined in MacTypes.h */

/* Region handle (opaque) */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * QuickDraw Types (subset needed for Window Manager)
 * ============================================================================ */

/* Pattern for desktop background */
/* Pattern type defined in MacTypes.h */

/* Graphics port structure */
/* GrafPort defined in WindowTypes.h */

/* Ptr is defined in MacTypes.h */

/* CGrafPort is defined in QuickDraw/QuickDraw.h */
/* CGrafPort is defined in MacTypes.h */

/* Picture handle */
/* Handle is defined in MacTypes.h */

/* Color specification */
/* ColorSpec is defined in QuickDraw/QDTypes.h */

/* Color table */
/* Handle is defined in MacTypes.h */

/* Pixel pattern */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * Control Manager Types (subset needed for Window Manager)
 * ============================================================================ */

/* Control record (forward declaration) */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * Event Manager Types (subset needed for Window Manager)
 * ============================================================================ */

/* Event record structure */
/* EventRecord is defined in WindowTypes.h */

/* ============================================================================
 * Window Manager Constants
 * ============================================================================ */

/* Window Definition IDs */
/* Window definition proc constants are defined in WindowTypes.h */

/* Window kinds are defined in WindowTypes.h */

/* FindWindow result codes are defined in WindowTypes.h */

/* Window messages are defined in WindowTypes.h */

/* Window part codes are defined in WindowTypes.h */

/* Window color table part identifiers are defined in WindowTypes.h */

/* Desktop pattern ID is defined in WindowTypes.h */

/* ============================================================================
 * Window Manager Data Structures
 * ============================================================================ */

/* Forward declarations - WindowPtr and CWindowPtr are in WindowTypes.h */
/* WindowPeek is defined in MacTypes.h */
/* WindowPeek is defined in MacTypes.h */

/* Window structures are defined in WindowTypes.h */

/* Window definition procedure */

/* Drag gray region callback */

/* ============================================================================
 * Window Manager Global State
 * ============================================================================ */

/* Window Manager port structures are defined in WindowTypes.h */

/* Complete Window Manager state is defined in WindowTypes.h */

/* Platform-specific data is part of WindowManagerState in WindowTypes.h */

/* ============================================================================
 * Window Manager API - Initialization
 * ============================================================================ */

/*
 * InitWindows - Initialize the Window Manager
 *
 * This MUST be called before any other Window Manager functions.
 * Sets up the Window Manager port, desktop pattern, and internal structures.
 */
void InitWindows(void);

/*
 * GetWMgrPort - Get the Window Manager port
 *
 * Returns a pointer to the Window Manager's grafport in wPort.
 * This port is used for desktop drawing and window management operations.
 */
void GetWMgrPort(GrafPtr* wPort);

/*
 * GetCWMgrPort - Get the Color Window Manager port
 *
 * Returns a pointer to the Window Manager's color grafport in wMgrCPort.
 * This port is used for color desktop drawing and color window operations.
 */
void GetCWMgrPort(CGrafPtr* wMgrCPort);

/* ============================================================================
 * Window Manager API - Window Creation and Disposal
 * ============================================================================ */

/*
 * NewWindow - Create a new window
 *
 * Parameters:
 *   wStorage    - Storage for window record (NULL to allocate automatically)
 *   boundsRect  - Initial window bounds in global coordinates
 *   title       - Window title (Pascal string)
 *   visible     - TRUE to make window visible immediately
 *   theProc     - Window definition procedure ID
 *   behind      - Window to place new window behind (NULL for front)
 *   goAwayFlag  - TRUE to include close box
 *   refCon      - Application reference constant
 *
 * Returns: Pointer to new window, or NULL if creation failed
 */
WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                   ConstStr255Param title, Boolean visible,
                   short theProc, WindowPtr behind,
                   Boolean goAwayFlag, long refCon);

/*
 * GetNewWindow - Create window from WIND resource
 *
 * Parameters:
 *   windowID - Resource ID of WIND resource
 *   wStorage - Storage for window record (NULL to allocate)
 *   behind   - Window to place new window behind (NULL for front)
 *
 * Returns: Pointer to new window, or NULL if creation failed
 */
WindowPtr GetNewWindow(short windowID, void* wStorage, WindowPtr behind);

/*
 * NewCWindow - Create a new color window
 *
 * Similar to NewWindow but creates a color window with CGrafPort.
 * Automatically creates auxiliary window record for color information.
 */
WindowPtr NewCWindow(void* wStorage, const Rect* boundsRect,
                    ConstStr255Param title, Boolean visible,
                    short procID, WindowPtr behind,
                    Boolean goAwayFlag, long refCon);

/*
 * GetNewCWindow - Create color window from WIND resource
 *
 * Similar to GetNewWindow but creates a color window.
 */
WindowPtr GetNewCWindow(short windowID, void* wStorage, WindowPtr behind);

/*
 * CloseWindow - Close and hide a window
 *
 * Hides the window, removes it from the window list, and disposes of
 * its regions and auxiliary records, but does NOT free the window record.
 */
void CloseWindow(WindowPtr theWindow);

/*
 * DisposeWindow - Completely dispose of a window
 *
 * Calls CloseWindow and then frees the window record memory.
 */
void DisposeWindow(WindowPtr theWindow);

/* ============================================================================
 * Window Manager API - Window Display and Visibility
 * ============================================================================ */

/*
 * SetWTitle - Set window title
 *
 * Changes the window's title and redraws the title bar if visible.
 */
void SetWTitle(WindowPtr theWindow, ConstStr255Param title);

/*
 * GetWTitle - Get window title
 *
 * Copies the window's title into the provided string buffer.
 */
void GetWTitle(WindowPtr theWindow, Str255 title);

/*
 * SelectWindow - Activate a window
 *
 * Makes the specified window active, brings it to front, and highlights it.
 * Deactivates the previously active window.
 */
void SelectWindow(WindowPtr theWindow);

/*
 * HideWindow - Hide a window
 *
 * Makes the window invisible and removes it from the window list visually.
 * If this was the active window, activates the next visible window.
 */
void HideWindow(WindowPtr theWindow);

/*
 * ShowWindow - Show a window
 *
 * Makes the window visible and draws it. If no window is currently active,
 * this window becomes active.
 */
void ShowWindow(WindowPtr theWindow);

/*
 * ShowHide - Show or hide a window based on flag
 *
 * Convenience function that calls ShowWindow or HideWindow.
 */
void ShowHide(WindowPtr theWindow, Boolean showFlag);

/*
 * HiliteWindow - Highlight or unhighlight a window
 *
 * Changes the window's highlight state and redraws the title bar.
 */
void HiliteWindow(WindowPtr theWindow, Boolean fHilite);

/*
 * BringToFront - Bring window to front of window list
 *
 * Moves the window to the front of the window list without activating it.
 */
void BringToFront(WindowPtr theWindow);

/*
 * SendBehind - Send window behind another window
 *
 * Moves the window to behind the specified window in the window list.
 */
void SendBehind(WindowPtr theWindow, WindowPtr behindWindow);

/*
 * FrontWindow - Get frontmost visible window
 *
 * Returns the frontmost visible window, or NULL if no windows are visible.
 */
WindowPtr FrontWindow(void);

/* ============================================================================
 * Window Manager API - Window Drawing and Updates
 * ============================================================================ */

/*
 * DrawGrowIcon - Draw the grow icon in window's grow box
 *
 * Draws the standard grow icon (diagonal lines) in the window's grow box.
 */
void DrawGrowIcon(WindowPtr theWindow);

/*
 * DrawWindow - Draw window chrome (frame, title bar, controls)
 *
 * Draws the window's frame, title bar, and controls.
 * Content drawing is the application's responsibility.
 */
void DrawWindow(WindowPtr theWindow);

/*
 * DrawNew - Draw a newly created window
 *
 * Internal function to draw a window that has just been created or shown.
 */
void DrawNew(WindowPeek window, Boolean update);

/*
 * PaintOne - Paint a single window
 *
 * Redraws one window, intersecting with the clobbered region.
 */
void PaintOne(WindowPeek window, RgnHandle clobberedRgn);

/*
 * PaintBehind - Paint all windows behind a given window
 *
 * Redraws all windows behind the specified window that intersect with
 * the clobbered region.
 */
void PaintBehind(WindowPeek startWindow, RgnHandle clobberedRgn);

/*
 * CalcVis - Calculate visible region for a window
 *
 * Calculates and sets the visible region for a window based on
 * overlapping windows.
 */
void CalcVis(WindowPeek window);

/*
 * CalcVisBehind - Calculate visible regions for windows behind a given window
 *
 * Recalculates visible regions for all windows behind the specified window.
 */
void CalcVisBehind(WindowPeek startWindow, RgnHandle clobberedRgn);

/*
 * ClipAbove - Set clip region to exclude windows above
 *
 * Sets the current port's clip region to exclude all windows above
 * the specified window.
 */
void ClipAbove(WindowPeek window);

/*
 * SaveOld - Save old visible region before window changes
 *
 * Internal function to save the old visible region before making changes.
 */
void SaveOld(WindowPeek window);

/* ============================================================================
 * Window Manager API - Window Positioning and Sizing
 * ============================================================================ */

/*
 * MoveWindow - Move a window to a new position
 *
 * Parameters:
 *   theWindow - Window to move
 *   hGlobal   - New horizontal position (global coordinates)
 *   vGlobal   - New vertical position (global coordinates)
 *   front     - TRUE to bring window to front
 */
void MoveWindow(WindowPtr theWindow, short hGlobal, short vGlobal, Boolean front);

/*
 * SizeWindow - Change window size
 *
 * Parameters:
 *   theWindow - Window to resize
 *   w         - New width
 *   h         - New height
 *   fUpdate   - TRUE to generate update events for new areas
 */
void SizeWindow(WindowPtr theWindow, short w, short h, Boolean fUpdate);

/*
 * ZoomWindow - Zoom window between user and standard states
 *
 * Parameters:
 *   theWindow - Window to zoom
 *   partCode  - inZoomIn or inZoomOut
 *   front     - TRUE to bring window to front
 */
void ZoomWindow(WindowPtr theWindow, short partCode, Boolean front);

/*
 * GrowWindow - Track mouse for window resizing
 *
 * Tracks the mouse while the user resizes a window by dragging the grow box.
 *
 * Parameters:
 *   theWindow - Window being resized
 *   startPt   - Initial mouse position
 *   bBox      - Size constraints (minimum and maximum)
 *
 * Returns: New size as long with width in high word, height in low word
 */
long GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox);

/*
 * DragWindow - Track mouse for window dragging
 *
 * Tracks the mouse while the user drags a window by its title bar.
 *
 * Parameters:
 *   theWindow   - Window being dragged
 *   startPt     - Initial mouse position
 *   boundsRect  - Dragging constraints
 */
void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect);

/* ============================================================================
 * Window Manager API - Update Management
 * ============================================================================ */

/*
 * InvalRect - Invalidate a rectangle in current port
 *
 * Adds the specified rectangle to the current port's update region.
 */
void InvalRect(const Rect* badRect);

/*
 * InvalRgn - Invalidate a region in current port
 *
 * Adds the specified region to the current port's update region.
 */
void InvalRgn(RgnHandle badRgn);

/*
 * ValidRect - Validate a rectangle in current port
 *
 * Removes the specified rectangle from the current port's update region.
 */
void ValidRect(const Rect* goodRect);

/*
 * ValidRgn - Validate a region in current port
 *
 * Removes the specified region from the current port's update region.
 */
void ValidRgn(RgnHandle goodRgn);

/*
 * BeginUpdate - Begin updating a window
 *
 * Sets the current port to the window and clips drawing to the update region.
 * Call this before drawing in response to an update event.
 */
void BeginUpdate(WindowPtr theWindow);

/*
 * EndUpdate - End updating a window
 *
 * Clears the window's update region and restores normal clipping.
 * Call this after drawing in response to an update event.
 */
void EndUpdate(WindowPtr theWindow);

/* ============================================================================
 * Window Manager API - Window Information
 * ============================================================================ */

/*
 * SetWRefCon - Set window reference constant
 *
 * Sets the application-defined reference constant for the window.
 */
void SetWRefCon(WindowPtr theWindow, long data);

/*
 * GetWRefCon - Get window reference constant
 *
 * Returns the application-defined reference constant for the window.
 */
long GetWRefCon(WindowPtr theWindow);

/*
 * SetWindowPic - Set window picture
 *
 * Sets a picture to be automatically drawn in the window's content area.
 */
void SetWindowPic(WindowPtr theWindow, PicHandle pic);

/*
 * GetWindowPic - Get window picture
 *
 * Returns the picture associated with the window, or NULL if none.
 */
PicHandle GetWindowPic(WindowPtr theWindow);

/*
 * CheckUpdate - Check for update events
 *
 * Checks if the specified event is an update event and handles it.
 *
 * Returns: TRUE if the event was an update event
 */
Boolean CheckUpdate(EventRecord* theEvent);

/* ============================================================================
 * Window Manager API - Window Finding and Hit Testing
 * ============================================================================ */

/*
 * FindWindow - Determine which window contains a point
 *
 * Determines which window (if any) contains the specified point and
 * which part of the window was hit.
 *
 * Parameters:
 *   thePoint  - Point to test (global coordinates)
 *   theWindow - Returns pointer to window containing point
 *
 * Returns: Hit test result (inContent, inDrag, inGrow, etc.)
 */
short FindWindow(Point thePoint, WindowPtr* theWindow);

/*
 * TrackBox - Track mouse in a window's zoom or close box
 *
 * Tracks the mouse while it's pressed in a window's zoom or close box,
 * providing visual feedback.
 *
 * Parameters:
 *   theWindow - Window containing the box
 *   thePt     - Initial mouse position
 *   partCode  - Which box (inGoAway, inZoomIn, inZoomOut)
 *
 * Returns: TRUE if mouse was released inside the box
 */
Boolean TrackBox(WindowPtr theWindow, Point thePt, short partCode);

/*
 * TrackGoAway - Track mouse in window's close box
 *
 * Convenience function for tracking the close box.
 *
 * Returns: TRUE if mouse was released inside the close box
 */
Boolean TrackGoAway(WindowPtr theWindow, Point thePt);

/* ============================================================================
 * Window Manager API - Miscellaneous Functions
 * ============================================================================ */

/*
 * PinRect - Constrain a point to within a rectangle
 *
 * Adjusts the point to be within the specified rectangle.
 *
 * Returns: The adjusted point as a long (h in high word, v in low word)
 */
long PinRect(const Rect* theRect, Point thePt);

/*
 * DragGrayRgn - Drag a gray outline of a region
 *
 * Displays and tracks a gray outline of a region as the user drags it.
 * Used internally by DragWindow.
 *
 * Parameters:
 *   theRgn     - Region to drag outline of
 *   startPt    - Initial mouse position
 *   limitRect  - Constraining rectangle
 *   slopRect   - Rectangle outside which dragging snaps back
 *   axis       - Constrain to horizontal/vertical axis
 *   actionProc - Procedure to call during dragging
 *
 * Returns: Final offset as long (h in high word, v in low word)
 */
long DragGrayRgn(RgnHandle theRgn, Point startPt, const Rect* limitRect,
                 const Rect* slopRect, short axis, DragGrayRgnProcPtr actionProc);

/* ============================================================================
 * Window Manager API - Color Window Functions
 * ============================================================================ */

/*
 * GetAuxWin - Get auxiliary window record
 *
 * Retrieves the auxiliary window record for a color window.
 *
 * Parameters:
 *   theWindow - Window to get auxiliary record for
 *   awHndl    - Returns handle to auxiliary record
 *
 * Returns: TRUE if auxiliary record exists
 */
Boolean GetAuxWin(WindowPtr theWindow, AuxWinHandle* awHndl);

/*
 * SetWinColor - Set window color table
 *
 * Sets the color table for a window's various parts (frame, content, etc.)
 */
void SetWinColor(WindowPtr theWindow, WCTabHandle newColorTable);

/* ============================================================================
 * Window Manager API - Desktop Management
 * ============================================================================ */

/*
 * SetDeskCPat - Set desktop color pattern
 *
 * Sets the color pattern used for the desktop background.
 */
void SetDeskCPat(PixPatHandle deskPixPat);

/* ============================================================================
 * Window Manager Internal Functions (for platform implementations)
 * ============================================================================ */

/*
 * GetWindowManagerState - Get global Window Manager state
 *
 * Returns pointer to the global Window Manager state structure.
 * Used by platform implementations and internal functions.
 */
WindowManagerState* GetWindowManagerState(void);

/*
 * Platform-specific functions that must be implemented by each platform:
 *
 * void Platform_InitWindowing(void);
 * void Platform_CreateNativeWindow(WindowPtr window);
 * void Platform_DestroyNativeWindow(WindowPtr window);
 * void Platform_ShowNativeWindow(WindowPtr window, Boolean show);
 * void Platform_MoveNativeWindow(WindowPtr window, short h, short v);
 * void Platform_SizeNativeWindow(WindowPtr window, short w, short h);
 * void Platform_SetNativeWindowTitle(WindowPtr window, ConstStr255Param title);
 * void Platform_InvalidateNativeWindow(WindowPtr window, const Rect* rect);
 * void Platform_BeginNativeWindowDraw(WindowPtr window);
 * void Platform_EndNativeWindowDraw(WindowPtr window);
 */

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/* Get GrafPtr from WindowPtr */
#define GetWindowPort(w) ((GrafPtr)(w))

/* Get WindowPtr from GrafPtr (assuming it's a window) */
#define GetWindowFromPort(p) ((WindowPtr)(p))

/* Get port bounds */
#define GetPortBounds(port, rect) (*(rect) = (port)->portRect)

/* Check if window is visible */
#define IsWindowVisible(w) ((w) && (w)->visible)

/* Check if window is highlighted */
#define IsWindowHilited(w) ((w) && (w)->hilited)

/* Get window's content region */
#define GetWindowContentRgn(w) ((w) ? (w)->contRgn : NULL)

/* Get window's structure region */
#define GetWindowStructRgn(w) ((w) ? (w)->strucRgn : NULL)

/* Get window's update region */
#define GetWindowUpdateRgn(w) ((w) ? (w)->updateRgn : NULL)

#ifdef __cplusplus
}
#endif

#endif /* __WINDOW_MANAGER_H__ */

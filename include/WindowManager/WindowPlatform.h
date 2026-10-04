/*
 * WindowPlatform.h - Window Manager platform hooks
 *
 * The Window Manager draws directly to the kernel framebuffer. Native-window
 * hooks remain for compatibility, but the current implementation does not
 * create host windows or depend on a desktop window system.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __WINDOW_PLATFORM_H__
#define __WINDOW_PLATFORM_H__

#include "SystemTypes.h"
#include "WindowTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform Initialization and Shutdown
 * ============================================================================ */

/*
 * Platform_InitWindowing - Initialize platform windowing system
 *
 * Must be called before any other platform functions.
 */
void Platform_InitWindowing(void);

/*
 * Platform_HasColorQuickDraw - Check for Color QuickDraw support
 *
 * Returns true if platform supports color graphics operations.
 */
Boolean Platform_HasColorQuickDraw(void);

/* ============================================================================
 * Screen and Display Management
 * ============================================================================ */

/*
 * Platform_GetScreenBounds - Get main screen bounds
 *
 * Returns the bounds of the main display in global coordinates.
 */
void Platform_GetScreenBounds(Rect* bounds);

/* ============================================================================
 * Native Window Management
 * ============================================================================ */

/*
 * Platform_CreateNativeWindow - Create platform native window
 *
 * Creates the underlying native window for a Mac OS window.
 */
void Platform_CreateNativeWindow(WindowPtr window);

/*
 * Platform_DestroyNativeWindow - Destroy platform native window
 *
 * Destroys the underlying native window.
 */
void Platform_DestroyNativeWindow(WindowPtr window);

/*
 * Platform_ShowNativeWindow - Show or hide native window
 *
 * Controls the visibility of the native window.
 */
void Platform_ShowNativeWindow(WindowPtr window, Boolean show);

/*
 * Platform_MoveNativeWindow - Move native window
 *
 * Moves the native window to the specified position.
 */
void Platform_MoveNativeWindow(WindowPtr window, short h, short v);

/*
 * Platform_SizeNativeWindow - Resize native window
 *
 * Changes the size of the native window.
 */
void Platform_SizeNativeWindow(WindowPtr window, short w, short h);

/*
 * Platform_SetNativeWindowTitle - Set native window title
 *
 * Sets the title displayed by the native window system.
 */
void Platform_SetNativeWindowTitle(WindowPtr window, ConstStr255Param title);

/*
 * Platform_BringNativeWindowToFront - Bring window to front
 *
 * Brings the native window to the front of the stacking order.
 */
void Platform_BringNativeWindowToFront(WindowPtr window);

/*
 * Platform_SendNativeWindowBehind - Send window behind another
 *
 * Places the native window behind another window in stacking order.
 */
void Platform_SendNativeWindowBehind(WindowPtr window, WindowPtr behind);

/* ============================================================================
 * Graphics Port Management
 * ============================================================================ */

/*
 * Platform_InitializePort - Initialize graphics port
 *
 * Sets up a basic graphics port structure.
 */
Boolean Platform_InitializePort(GrafPtr port);

/*
 * Platform_InitializeColorPort - Initialize color graphics port
 *
 * Sets up a color graphics port structure.
 */
Boolean Platform_InitializeColorPort(CGrafPtr port);

/*
 * Platform_InitializeWindowPort - Initialize window's graphics port
 *
 * Sets up the graphics port for a specific window.
 */
Boolean Platform_InitializeWindowPort(WindowPtr window);

/*
 * Platform_InitializeColorWindowPort - Initialize window's color port
 *
 * Sets up the color graphics port for a specific window.
 */
Boolean Platform_InitializeColorWindowPort(WindowPtr window);

/*
 * Platform_CleanupWindowPort - Clean up window's graphics port
 *
 * Releases resources associated with a window's port.
 */
void Platform_CleanupWindowPort(WindowPtr window);

/* ============================================================================
 * Region Management
 * ============================================================================ */

/*
 * Platform_NewRgn - Create new region
 *
 * Allocates and initializes a new region handle.
 */
RgnHandle Platform_NewRgn(void);

/*
 * Platform_DisposeRgn - Dispose region
 *
 * Releases memory used by a region.
 */
void Platform_DisposeRgn(RgnHandle rgn);

/*
 * Platform_SetRectRgn - Set region to rectangle
 *
 * Sets a region to contain the specified rectangle.
 */
void Platform_SetRectRgn(RgnHandle rgn, const Rect* rect);

/*
 * Platform_SetEmptyRgn - Set region to empty
 *
 * Sets a region to be empty (contain no points).
 */
void Platform_SetEmptyRgn(RgnHandle rgn);

/*
 * Platform_CopyRgn - Copy region
 *
 * Copies the contents of one region to another.
 */
void Platform_CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);

/*
 * Platform_UnionRgn - Union two regions
 *
 * Sets destination region to the union of two source regions.
 */
void Platform_UnionRgn(RgnHandle srcA, RgnHandle srcB, RgnHandle dst);

/*
 * Platform_IntersectRgn - Intersect two regions
 *
 * Sets destination region to the intersection of two source regions.
 */
void Platform_IntersectRgn(RgnHandle srcA, RgnHandle srcB, RgnHandle dst);

/*
 * Platform_DiffRgn - Subtract regions
 *
 * Sets destination region to srcA minus srcB.
 */
void Platform_DiffRgn(RgnHandle srcA, RgnHandle srcB, RgnHandle dst);

/*
 * Platform_OffsetRgn - Offset region
 *
 * Moves a region by the specified offset.
 */
void Platform_OffsetRgn(RgnHandle rgn, short dh, short dv);

/*
 * Platform_PtInRgn - Test if point is in region
 *
 * Returns true if the point is within the region.
 */
Boolean Platform_PtInRgn(Point pt, RgnHandle rgn);

/*
 * Platform_EmptyRgn - Test if region is empty
 *
 * Returns true if the region contains no points.
 */
Boolean Platform_EmptyRgn(RgnHandle rgn);

/*
 * Platform_GetRegionBounds - Get region bounding rectangle
 *
 * Returns the smallest rectangle that contains the region.
 */
void Platform_GetRegionBounds(RgnHandle rgn, Rect* bounds);

/* ============================================================================
 * Window Drawing and Updates
 * ============================================================================ */

/*
 * Platform_BeginWindowDraw - Begin drawing in window
 *
 * Prepares the window for drawing operations.
 */
void Platform_BeginWindowDraw(WindowPtr window);

/*
 * Platform_EndWindowDraw - End drawing in window
 *
 * Finishes drawing operations and updates the display.
 */
void Platform_EndWindowDraw(WindowPtr window);

/*
 * Platform_InvalidateWindowContent - Invalidate window content
 *
 * Marks the window's content area as needing redraw.
 */
void Platform_InvalidateWindowContent(WindowPtr window);

/*
 * Platform_InvalidateWindowFrame - Invalidate window frame
 *
 * Marks the window's frame as needing redraw.
 */
void Platform_InvalidateWindowFrame(WindowPtr window);

/*
 * Platform_InvalidateWindowRect - Invalidate rectangle in window
 *
 * Marks the specified rectangle as needing redraw.
 */
void Platform_InvalidateWindowRect(WindowPtr window, const Rect* rect);

/*
 * Platform_UpdateWindowColors - Update window color scheme
 *
 * Updates the window's appearance based on color table changes.
 */
void Platform_UpdateWindowColors(WindowPtr window);

/* ============================================================================
 * Window Regions and Layout
 * ============================================================================ */

/*
 * Platform_CalculateWindowRegions - Calculate window regions
 *
 * Computes the structure, content, and other regions for a window.
 */
void Platform_CalculateWindowRegions(WindowPtr window);

/*
 * Platform_GetWindowFrameRect - Get window frame rectangle
 *
 * Returns the rectangle that includes the window frame.
 */
void Platform_GetWindowFrameRect(WindowPtr window, Rect* frameRect);

/*
 * Platform_GetWindowContentRect - Get window content rectangle
 *
 * Returns the rectangle of the window's content area.
 */
void Platform_GetWindowContentRect(WindowPtr window, Rect* contentRect);

/*
 * Platform_GetWindowTitleBarRect - Get title bar rectangle
 *
 * Returns the rectangle of the window's title bar.
 */
void Platform_GetWindowTitleBarRect(WindowPtr window, Rect* titleRect);

/*
 * Platform_GetWindowCloseBoxRect - Get close box rectangle
 *
 * Returns the rectangle of the window's close box.
 */
void Platform_GetWindowCloseBoxRect(WindowPtr window, Rect* closeRect);

/*
 * Platform_GetWindowZoomBoxRect - Get zoom box rectangle
 *
 * Returns the rectangle of the window's zoom box.
 */
void Platform_GetWindowZoomBoxRect(WindowPtr window, Rect* zoomRect);

/*
 * Platform_GetWindowGrowBoxRect - Get grow box rectangle
 *
 * Returns the rectangle of the window's grow box.
 */
void Platform_GetWindowGrowBoxRect(WindowPtr window, Rect* growRect);

/* ============================================================================
 * Hit Testing and Mouse Tracking
 * ============================================================================ */

/*
 * Platform_WindowHitTest - Hit test window parts
 *
 * Determines which part of the window contains the specified point.
 */
short Platform_WindowHitTest(WindowPtr window, Point pt);

/*
 * Platform_PointInWindowPart - Test point in window part
 *
 * Returns true if the point is in the specified window part.
 */
Boolean Platform_PointInWindowPart(WindowPtr window, Point pt, short part);

/*
 * Platform_IsMouseDown - Check if mouse button is down
 *
 * Returns true if any mouse button is currently pressed.
 */
Boolean Platform_IsMouseDown(void);

/*
 * Platform_GetMousePosition - Get current mouse position
 *
 * Returns the current mouse position in global coordinates.
 */
void Platform_GetMousePosition(Point* pt);

/*
 * Platform_WaitTicks - Wait for specified time
 *
 * Waits for approximately the specified number of ticks (1/60 second).
 */
void Platform_WaitTicks(short ticks);

/* ============================================================================
 * Visual Feedback and Tracking
 * ============================================================================ */

/*
 * Platform_HighlightWindowPart - Highlight window part
 *
 * Provides visual feedback for window part interaction.
 */
void Platform_HighlightWindowPart(WindowPtr window, short part, Boolean highlight);

/*
 * Platform_ShowDragOutline - Show drag outline
 *
 * Displays a gray outline of the rectangle being dragged.
 */
void Platform_ShowDragOutline(const Rect* rect);

/*
 * Platform_UpdateDragOutline - Update drag outline
 *
 * Updates the position of the drag outline from oldRect to newRect.
 */
void Platform_UpdateDragOutline(const Rect* oldRect, const Rect* newRect);

/*
 * Platform_HideDragOutline - Hide drag outline
 *
 * Removes the drag outline from the display.
 */
void Platform_HideDragOutline(const Rect* rect);

/* ============================================================================
 * Event System Integration
 * ============================================================================ */

/*
 * Platform_PostWindowEvent - Post window event
 *
 * Posts an event related to a specific window.
 */
void Platform_PostWindowEvent(WindowPtr window, short eventType, long eventData);

/* ============================================================================
 * Color and Pattern Management
 * ============================================================================ */

/*
 * Platform_CreateStandardGrayPixPat - Create standard gray pattern
 *
 * Creates a standard 50% gray pixel pattern for the desktop.
 */
PixPatHandle Platform_CreateStandardGrayPixPat(void);

/*
 * Platform_DisposeCTable - Dispose color table
 *
 * Releases memory used by a color table.
 */
void Platform_DisposeCTable(CTabHandle ctab);

void PlatformDrawRGBABitmap(const UInt8* rgba_data, int width, int height, int dest_x, int dest_y);

#ifdef __cplusplus
}
#endif

#endif /* __WINDOW_PLATFORM_H__ */

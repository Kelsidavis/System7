/*
 * WindowFunctions.h - Window Manager Function Declarations
 * This header provides function declarations without redefining types
 */

#ifndef WINDOW_FUNCTIONS_H
#define WINDOW_FUNCTIONS_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"
#include "WindowManager/WindowTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Window creation and disposal */
WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                   const unsigned char* title, Boolean visible,
                   short procID, WindowPtr behind, Boolean goAwayFlag,
                   long refCon);
WindowPtr NewCWindow(void* wStorage, const Rect* boundsRect,
                    const unsigned char* title, Boolean visible,
                    short procID, WindowPtr behind, Boolean goAwayFlag,
                    long refCon);
WindowPtr GetNewWindow(short windowID, void* wStorage, WindowPtr behind);
WindowPtr GetNewCWindow(short windowID, void* wStorage, WindowPtr behind);
void CloseWindow(WindowPtr theWindow);
void DisposeWindow(WindowPtr theWindow);

/* Window visibility */
void ShowWindow(WindowPtr theWindow);
void HideWindow(WindowPtr theWindow);
void ShowHide(WindowPtr theWindow, Boolean showFlag);

/* Window selection and activation */
void SelectWindow(WindowPtr theWindow);
void BringToFront(WindowPtr theWindow);
void SendBehind(WindowPtr theWindow, WindowPtr behindWindow);
void HiliteWindow(WindowPtr theWindow, Boolean fHilite);

/* Window positioning and sizing */
void MoveWindow(WindowPtr theWindow, short hGlobal, short vGlobal,
                Boolean front);
void SizeWindow(WindowPtr theWindow, short w, short h, Boolean fUpdate);
void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect);
void GrowWindow(WindowPtr theWindow, Point startPt, const Rect* bBox);
void ZoomWindow(WindowPtr theWindow, short partCode, Boolean front);

/* Custom window drag constraints */
typedef Point (*DragConstraintProc)(Point proposedPos, WindowPtr window, void* refCon);
void SetDragConstraintProc(DragConstraintProc constraintProc, void* refCon);

/* Window drawing and updating */
void InvalRect(const Rect* badRect);
/* Invalidate a rectangle in a named window rather than in whichever port is
 * current. Prefer this wherever the window is known. */
void InvalWindowRect(WindowPtr window, const Rect* badRect);
void InvalRgn(RgnHandle badRgn);
void ValidRect(const Rect* goodRect);
void ValidRgn(RgnHandle goodRgn);
void BeginUpdate(WindowPtr theWindow);
void EndUpdate(WindowPtr theWindow);
void SetWRefCon(WindowPtr theWindow, long data);
long GetWRefCon(WindowPtr theWindow);

/* Window queries */
void SetWindowKind(WindowPtr theWindow, short theKind);
short GetWindowKind(WindowPtr theWindow);
Boolean IsWindowVisible(WindowPtr theWindow);
Boolean IsWindowHilited(WindowPtr theWindow);
void GetWindowGoAwayFlag(WindowPtr theWindow, Boolean* hasGoAway);
void GetWindowZoomFlag(WindowPtr theWindow, Boolean* hasZoom);
void GetWindowStructureRgn(WindowPtr theWindow, RgnHandle r);
void GetWindowContentRgn(WindowPtr theWindow, RgnHandle r);
void GetWindowUpdateRgn(WindowPtr theWindow, RgnHandle r);
void GetWindowTitle(WindowPtr theWindow, unsigned char* title);
void SetWindowTitle(WindowPtr theWindow, const unsigned char* title);

/* Port management */
void SetPort(GrafPtr port);
void SetPortWindowPort(WindowPtr window);
void GetPort(GrafPtr* port);
GrafPtr GetWindowPort(WindowPtr window);
CGrafPtr GetWindowPortAsCGrafPtr(WindowPtr window);
void GetWMgrPort(GrafPtr* wPort);
void GetCWMgrPort(CGrafPtr* wMgrCPort);

/* Window finding */
WindowPtr FrontWindow(void);
WindowPtr FrontNonFloatingWindow(void);
WindowPtr GetFrontWindowOfClass(short windowClass, Boolean mustBeVisible);
short FindWindow(Point thePoint, WindowPtr* theWindow);
short GetWindowRegionCode(Point thePoint, WindowPtr theWindow);
WindowPtr GetNextWindow(WindowPtr theWindow);
WindowPtr GetPreviousWindow(WindowPtr theWindow);

/* Palettes and colors (Color QuickDraw) */
Boolean IsValidWindowPtr(WindowPtr theWindow);
void CalcVis(WindowPtr theWindow);
void CalcVisBehind(WindowPtr startWindow, RgnHandle clobberedRgn);
void CheckUpdate(EventRecord* theEvent);
void ClipAbove(WindowPtr theWindow);
void PaintOne(WindowPtr theWindow, RgnHandle clobberedRgn);
void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn);
void SaveOld(WindowPtr theWindow);
void DrawNew(WindowPtr theWindow, Boolean update);

/* Window Manager initialization */
void InitWindows(void);
void GetWTitle(WindowPtr theWindow, unsigned char* title);
void SetWTitle(WindowPtr theWindow, const unsigned char* title);

/* Low-memory globals access */
WindowPtr GetWindowList(void);
/* GrafPtr GetWMgrPort(void); - duplicate, using void GetWMgrPort(GrafPtr*) above */
CWindowPtr GetWindowListColorPtr(void);

#ifdef __cplusplus
}
#endif

#endif /* WINDOW_FUNCTIONS_H */

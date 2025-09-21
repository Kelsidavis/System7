/* Window Manager Stubs - Minimal implementation for initial boot */
#include "../../include/SystemTypes.h"

/* Basic window functions */
WindowPtr NewWindow(void* storage, const Rect* boundsRect, const unsigned char* title,
                    Boolean visible, short procID, WindowPtr behind, Boolean goAwayFlag,
                    long refCon) {
    return NULL;
}

void DisposeWindow(WindowPtr window) {}
void ShowWindow(WindowPtr window) {}
void HideWindow(WindowPtr window) {}
void SelectWindow(WindowPtr window) {}
void BringToFront(WindowPtr window) {}
void SendBehind(WindowPtr window, WindowPtr behindWindow) {}
void DrawGrowIcon(WindowPtr window) {}
void MoveWindow(WindowPtr window, short h, short v, Boolean bringToFront) {}
void SizeWindow(WindowPtr window, short w, short h, Boolean fUpdate) {}
void SetWTitle(WindowPtr window, const unsigned char* title) {}
void GetWTitle(WindowPtr window, unsigned char* title) {}
void SetWRefCon(WindowPtr window, long refCon) {}
long GetWRefCon(WindowPtr window) { return 0; }
void HiliteWindow(WindowPtr window, Boolean hilite) {}

/* Window finding and tracking */
short FindWindow(Point pt, WindowPtr* window) {
    *window = NULL;
    return inDesk;
}

Boolean TrackBox(WindowPtr window, Point pt, short partCode) { return false; }
Boolean TrackGoAway(WindowPtr window, Point pt) { return false; }
long GrowWindow(WindowPtr window, Point startPt, const Rect* sizeRect) { return 0; }
void DragWindow(WindowPtr window, Point startPt, const Rect* boundsRect) {}

/* Window updating */
void BeginUpdate(WindowPtr window) {}
void EndUpdate(WindowPtr window) {}
void InvalidRect(const Rect* badRect) {}
void ValidRect(const Rect* goodRect) {}
void InvalidRgn(RgnHandle badRgn) {}
void ValidRgn(RgnHandle goodRgn) {}
void InvalRect(const Rect* badRect) {}
void InvalRgn(RgnHandle badRgn) {}
void ValRect(const Rect* goodRect) {}
void ValRgn(RgnHandle goodRgn) {}

/* Window Manager state */
WindowPtr FrontWindow(void) { return NULL; }
Boolean CheckUpdate(EventRecord* event) { return false; }

/* Platform stubs */
void Platform_SendNativeWindowBehind(WindowPtr window, WindowPtr behind) {}
GrafPtr Platform_GetUpdatePort(WindowPtr window) { return NULL; }
Boolean Platform_EmptyRgn(RgnHandle rgn) { return true; }
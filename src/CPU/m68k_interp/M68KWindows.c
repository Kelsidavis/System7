/*
 * M68KWindows.c - the Window Manager for a 68K application
 *
 * The program's windows are native windows, each with a WindowRecord in the
 * program's memory standing for it. Other applications' windows - the
 * Finder's, which stay on the screen while it runs - are not the program's
 * to see: FindWindow says the desk is there and FrontWindow passes over
 * them. Desk accessories' windows are system windows, as always.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "DeskManager/DeskManager.h"
#include "WindowManager/WindowManager.h"
#include "ResourceManager.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"   /* patXor */
#include "System71StdLib.h"

extern void GetMouseLocal(Point* pt);     /* the current port's coordinates */
extern Boolean StillDown(void);
extern void InvalRect(const Rect* r);

static WindowPtr PopWindow(void) {
    return (WindowPtr)Obj_Port(Pop32());
}

/* The program's record for a native window it may see; 0 otherwise */
static UInt32 VisibleRecord(WindowPtr w) {
    if (!w) return 0;
    if (Obj_IsAppWindow(w) || w->windowKind < 0) return Obj_PortFor((GrafPtr)w);
    return 0;
}

/* Make a window the program asked for, at the content rectangle it gave -
 * which here is the frame, title bar included, until it is moved and sized
 * to match */
static UInt32 MakeWindow(UInt32 storage, const Rect* bounds, ConstStr255Param title,
                         Boolean visible, SInt16 procID, UInt32 behindArg,
                         Boolean goAway, SInt32 refCon) {
    WindowPtr behind = behindArg == 0xFFFFFFFF ? (WindowPtr)-1L
                     : behindArg == 0 ? NULL : (WindowPtr)Obj_Port(behindArg);
    WindowPtr w = NewWindow(NULL, bounds, title, false, procID, behind, goAway, refCon);
    if (!w) return 0;
    SInt16 width = (SInt16)(bounds->right - bounds->left);
    SInt16 height = (SInt16)(bounds->bottom - bounds->top);
    if (w->contRgn && *w->contRgn) {
        Rect c = (*w->contRgn)->rgnBBox;
        if (c.left != bounds->left || c.top != bounds->top) MoveWindow(w, bounds->left, bounds->top, false);
    }
    if (w->port.portRect.right - w->port.portRect.left != width ||
        w->port.portRect.bottom - w->port.portRect.top != height) {
        SizeWindow(w, width, height, false);
    }
    w->windowKind = 8;                          /* userKind */
    UInt32 rec = Obj_NewWindowRecord(w, storage);
    if (!rec) {
        DisposeWindow(w);
        return 0;
    }
    W32(rec + 152, (UInt32)refCon);
    if (visible) ShowWindow(w);
    Obj_SyncWindows();
    return rec;
}

/* FUNCTION NewWindow(wStorage: Ptr; boundsRect: Rect; title: Str255;
 *   visible: BOOLEAN; procID: INTEGER; behind: WindowPtr; goAwayFlag: BOOLEAN;
 *   refCon: LONGINT): WindowPtr */
TRAP(Trap_NewWindow) {
    UNUSED;
    SInt32 refCon = (SInt32)Pop32();
    Boolean goAway = PopBool();
    UInt32 behind = Pop32();
    SInt16 procID = (SInt16)Pop16();
    Boolean visible = PopBool();
    Str255 title;
    ReadPString(Pop32(), title);
    Rect bounds;
    ReadRect(Pop32(), &bounds);
    UInt32 storage = Pop32();
    Result32(MakeWindow(storage, &bounds, title, visible, procID, behind, goAway, refCon));
    return noErr;
}

/* FUNCTION GetNewWindow(windowID: INTEGER; wStorage: Ptr; behind: WindowPtr):
 * WindowPtr - from a WIND resource: bounds, procID, visible, goAway, refCon,
 * title (IM I-299) */
TRAP(Trap_GetNewWindow) {
    UNUSED;
    UInt32 behind = Pop32();
    UInt32 storage = Pop32();
    SInt16 id = (SInt16)Pop16();
    Handle wind = GetResource(FOURCC('W','I','N','D'), id);
    if (!wind || GetHandleSize(wind) < 19) {
        Result32(0);
        return noErr;
    }
    const UInt8* p = (const UInt8*)*wind;
    Rect bounds = { (SInt16)((p[0] << 8) | p[1]), (SInt16)((p[2] << 8) | p[3]),
                    (SInt16)((p[4] << 8) | p[5]), (SInt16)((p[6] << 8) | p[7]) };
    SInt16 procID = (SInt16)((p[8] << 8) | p[9]);
    Boolean visible = p[10] != 0;
    Boolean goAway = p[12] != 0;
    SInt32 refCon = (SInt32)(((UInt32)p[14] << 24) | ((UInt32)p[15] << 16) | ((UInt32)p[16] << 8) | p[17]);
    Str255 title;
    memcpy(title, p + 18, (size_t)p[18] + 1);
    Result32(MakeWindow(storage, &bounds, title, visible, procID, behind, goAway, refCon));
    return noErr;
}

/* DisposeWindow and CloseWindow: the window goes, and the record with it
 * if this side made it */
TRAP(Trap_DisposeWindow) {
    UNUSED;
    WindowPtr w = PopWindow();
    if (w && Obj_IsAppWindow(w)) {
        Obj_LeavePort((GrafPtr)w);
        Obj_ForgetWindow(w);
        DisposeWindow(w);
        Obj_SyncWindows();
    }
    return noErr;
}

#define WINDOW_VERB(name, call) \
    TRAP(name) { UNUSED; WindowPtr w = PopWindow(); if (w) call(w); Obj_SyncWindows(); return noErr; }
WINDOW_VERB(Trap_ShowWindow, ShowWindow)
WINDOW_VERB(Trap_HideWindow, HideWindow)
WINDOW_VERB(Trap_SelectWindow, SelectWindow)
WINDOW_VERB(Trap_BringToFront, BringToFront)
WINDOW_VERB(Trap_DrawGrowIcon, DrawGrowIcon)

/* PROCEDURE SetWindowPic(theWindow: WindowPtr; pic: PicHandle). The picture
 * is kept in the program's record, where GetWindowPic and the program find
 * it; the update that follows draws it rather than reaching the program
 * (M68KEvents.c). */
TRAP(Trap_SetWindowPic) {
    UNUSED;
    UInt32 pic = Pop32();
    UInt32 rec = Pop32();
    WindowPtr w = (WindowPtr)Obj_Port(rec);
    if (!w) return noErr;
    W32(rec + kWindowPicOffset, pic);
    GrafPtr saved;
    GetPort(&saved);
    SetPort((GrafPtr)w);
    InvalRect(&w->port.portRect);
    SetPort(saved);
    return noErr;
}

TRAP(Trap_GetWindowPic) {
    UNUSED;
    UInt32 rec = Pop32();
    Result32(Obj_Port(rec) ? R32(rec + kWindowPicOffset) : 0);
    return noErr;
}

/* FUNCTION DragGrayRgn(theRgn: RgnHandle; startPt: Point; limitRect,
 *   slopRect: Rect; axis: INTEGER; actionProc: ProcPtr): LONGINT
 *
 * The region's outline, in gray, follows the mouse in the current port
 * until the button comes up (IM I-294). The answer is how far it went,
 * vertical in the high word; or $80008000 if the mouse was let go outside
 * slopRect. The mouse is kept within limitRect and, by axis, to one
 * direction. */
TRAP(Trap_DragGrayRgn) {
    UNUSED;
    UInt32 action = Pop32();
    SInt16 axis = (SInt16)Pop16();
    Rect slop, limit;
    ReadRect(Pop32(), &slop);
    ReadRect(Pop32(), &limit);
    Point start = PopPoint();
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (!rgn) {
        Result32(0x80008000);
        return noErr;
    }
    GrafPtr port;
    GetPort(&port);
    Obj_SyncPortIn(port);
    PenState pen;
    GetPenState(&pen);
    PenMode(patXor);
    PenPat(&qd.gray);
    RgnHandle dragged = NewRgn();
    CopyRgn(rgn, dragged);
    Point at = start;                       /* where the dragged is drawn for */
    Boolean shown = false, inside = true;
    for (;;) {
        Point m;
        GetMouseLocal(&m);
        if (axis == 1) m.v = start.v;       /* hAxisOnly */
        if (axis == 2) m.h = start.h;       /* vAxisOnly */
        if (m.h < limit.left) m.h = limit.left;
        if (m.h > limit.right) m.h = limit.right;
        if (m.v < limit.top) m.v = limit.top;
        if (m.v > limit.bottom) m.v = limit.bottom;
        inside = PtInRect(m, &slop);
        Boolean moved = m.h != at.h || m.v != at.v;
        if (shown && (!inside || moved)) {
            Ports_BeforeDraw(port);
            FrameRgn(dragged);              /* xor: off again */
            Ports_AfterDraw(port);
            shown = false;
        }
        if (inside && !shown) {
            OffsetRgn(dragged, (SInt16)(m.h - at.h), (SInt16)(m.v - at.v));
            at = m;
            Ports_BeforeDraw(port);
            FrameRgn(dragged);
            Ports_AfterDraw(port);
            shown = true;
        }
        if (action) CallProgram(action);
        if (!StillDown()) break;
        SystemTask();
    }
    if (shown) {
        Ports_BeforeDraw(port);
        FrameRgn(dragged);
        Ports_AfterDraw(port);
    }
    DisposeRgn(dragged);
    SetPenState(&pen);
    Obj_SyncPortOut(port);
    Result32(inside ? ((UInt32)(UInt16)(at.v - start.v) << 16) | (UInt16)(at.h - start.h)
                    : 0x80008000);
    return noErr;
}

/* PROCEDURE ShowHide(theWindow: WindowPtr; showFlag: BOOLEAN) */
TRAP(Trap_ShowHide) {
    UNUSED;
    Boolean show = PopBool();
    WindowPtr w = PopWindow();
    if (w) ShowHide(w, show);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_HiliteWindow) {
    UNUSED;
    Boolean on = PopBool();
    WindowPtr w = PopWindow();
    if (w) HiliteWindow(w, on);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_SendBehind) {
    UNUSED;
    UInt32 behind = Pop32();
    WindowPtr w = PopWindow();
    if (w) SendBehind(w, behind ? (WindowPtr)Obj_Port(behind) : NULL);
    Obj_SyncWindows();
    return noErr;
}

/* FrontWindow: the frontmost window the program can see */
TRAP(Trap_FrontWindow) {
    UNUSED;
    UInt32 rec = 0;
    for (WindowPtr w = FrontWindow(); w && !rec; w = w->nextWindow) {
        if (w->visible) rec = VisibleRecord(w);
    }
    Result32(rec);
    return noErr;
}

/* FUNCTION FindWindow(thePoint: Point; VAR whichWindow: WindowPtr): INTEGER */
TRAP(Trap_FindWindow) {
    UNUSED;
    UInt32 var = Pop32();
    Point p = PopPoint();
    WindowPtr w = NULL;
    SInt16 part = FindWindow(p, &w);
    UInt32 rec = 0;
    if (w) {
        if (Obj_IsAppWindow(w)) {
            rec = Obj_PortFor((GrafPtr)w);
        } else if (w->windowKind < 0) {
            rec = Obj_PortFor((GrafPtr)w);
            part = inSysWindow;
        } else {
            part = inDesk;                      /* another application's */
        }
    }
    W32(var, rec);
    Result16((UInt16)part);
    return noErr;
}

/* PROCEDURE DragWindow(theWindow: WindowPtr; startPt: Point; boundsRect: Rect) */
TRAP(Trap_DragWindow) {
    UNUSED;
    Rect limit;
    ReadRect(Pop32(), &limit);
    Point p = PopPoint();
    WindowPtr w = PopWindow();
    if (w) DragWindow(w, p, &limit);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_TrackGoAway) {
    UNUSED;
    Point p = PopPoint();
    WindowPtr w = PopWindow();
    ResultBool(w && TrackGoAway(w, p));
    return noErr;
}

TRAP(Trap_TrackBox) {
    UNUSED;
    SInt16 part = (SInt16)Pop16();
    Point p = PopPoint();
    WindowPtr w = PopWindow();
    ResultBool(w && TrackBox(w, p, part));
    return noErr;
}

TRAP(Trap_ZoomWindow) {
    UNUSED;
    Boolean front = PopBool();
    SInt16 part = (SInt16)Pop16();
    WindowPtr w = PopWindow();
    if (w) ZoomWindow(w, part, front);
    Obj_SyncWindows();
    return noErr;
}

/* FUNCTION GrowWindow(theWindow: WindowPtr; startPt: Point; sizeRect: Rect): LONGINT */
TRAP(Trap_GrowWindow) {
    UNUSED;
    Rect sizeRect;
    ReadRect(Pop32(), &sizeRect);
    Point p = PopPoint();
    WindowPtr w = PopWindow();
    Result32(w ? (UInt32)GrowWindow(w, p, &sizeRect) : 0);
    return noErr;
}

TRAP(Trap_SizeWindow) {
    UNUSED;
    Boolean update = PopBool();
    SInt16 h = (SInt16)Pop16(), wd = (SInt16)Pop16();
    WindowPtr w = PopWindow();
    if (w) SizeWindow(w, wd, h, update);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_MoveWindow) {
    UNUSED;
    Boolean front = PopBool();
    SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16();
    WindowPtr w = PopWindow();
    if (w) MoveWindow(w, h, v, front);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_BeginUpdate) {
    UNUSED;
    WindowPtr w = PopWindow();
    if (w) BeginUpdate(w);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_EndUpdate) {
    UNUSED;
    WindowPtr w = PopWindow();
    if (w) EndUpdate(w);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_InvalRect) { UNUSED; Rect r; ReadRect(Pop32(), &r); InvalRect(&r); return noErr; }
TRAP(Trap_ValidRect) { UNUSED; Rect r; ReadRect(Pop32(), &r); ValidRect(&r); return noErr; }

TRAP(Trap_InvalRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) InvalRgn(rgn);
    return noErr;
}

TRAP(Trap_ValidRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) ValidRgn(rgn);
    return noErr;
}

TRAP(Trap_SetWTitle) {
    UNUSED;
    Str255 title;
    ReadPString(Pop32(), title);
    WindowPtr w = PopWindow();
    if (w) SetWTitle(w, title);
    return noErr;
}

TRAP(Trap_GetWTitle) {
    UNUSED;
    UInt32 var = Pop32();
    WindowPtr w = PopWindow();
    Str255 title;
    title[0] = 0;
    if (w) GetWTitle(w, title);
    WritePString(var, title);
    return noErr;
}

/* The reference constant lives in the record: a program may set it there */
TRAP(Trap_SetWRefCon) {
    UNUSED;
    UInt32 value = Pop32();
    UInt32 rec = Pop32();
    WindowPtr w = (WindowPtr)Obj_Port(rec);
    if (w) {
        SetWRefCon(w, (SInt32)value);
        W32(rec + 152, value);
    }
    return noErr;
}

TRAP(Trap_GetWRefCon) {
    UNUSED;
    UInt32 rec = Pop32();
    Result32(Obj_Port(rec) ? R32(rec + 152) : 0);
    return noErr;
}

TRAP(Trap_GetWMgrPort) {
    UNUSED;
    UInt32 var = Pop32();
    GrafPtr port = NULL;
    GetWMgrPort(&port);
    W32(var, Obj_PortFor(port));
    return noErr;
}

const M68KTrapEntry kM68KWindowTraps[] = {
    { 0xA913, Trap_NewWindow },     { 0xA9BD, Trap_GetNewWindow },  { 0xA914, Trap_DisposeWindow },
    { 0xA92D, Trap_DisposeWindow }, /* CloseWindow */
    { 0xA908, Trap_ShowHide },      { 0xA905, Trap_DragGrayRgn },
    { 0xA92E, Trap_SetWindowPic },  { 0xA92F, Trap_GetWindowPic },
    { 0xA915, Trap_ShowWindow },    { 0xA916, Trap_HideWindow },    { 0xA91F, Trap_SelectWindow },
    { 0xA920, Trap_BringToFront },  { 0xA904, Trap_DrawGrowIcon },  { 0xA91C, Trap_HiliteWindow },
    { 0xA921, Trap_SendBehind },    { 0xA924, Trap_FrontWindow },   { 0xA92C, Trap_FindWindow },
    { 0xA925, Trap_DragWindow },    { 0xA91E, Trap_TrackGoAway },   { 0xA83B, Trap_TrackBox },
    { 0xA83A, Trap_ZoomWindow },    { 0xA92B, Trap_GrowWindow },    { 0xA91D, Trap_SizeWindow },
    { 0xA91B, Trap_MoveWindow },    { 0xA922, Trap_BeginUpdate },   { 0xA923, Trap_EndUpdate },
    { 0xA928, Trap_InvalRect },     { 0xA92A, Trap_ValidRect },     { 0xA927, Trap_InvalRgn },
    { 0xA929, Trap_ValidRgn },      { 0xA91A, Trap_SetWTitle },     { 0xA919, Trap_GetWTitle },
    { 0xA918, Trap_SetWRefCon },    { 0xA917, Trap_GetWRefCon },    { 0xA910, Trap_GetWMgrPort },
};
const int kM68KWindowTrapCount = (int)(sizeof(kM68KWindowTraps) / sizeof(kM68KWindowTraps[0]));

/*
 * M68KControls.c - the Control Manager for a 68K application
 *
 * Controls are native; the program holds a handle to a ControlRecord in its
 * own memory for each, kept up to date after every call (M68KObjects.c).
 * TrackControl's action procedure, which is the program's - a scroll bar's
 * arrows scroll by calling it - is called back while the mouse is down.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "ControlManager/ControlManager.h"
#include "WindowManager/WindowManager.h"
#include "System71StdLib.h"

static ControlHandle PopControl(void) {
    return Obj_Control(Pop32());
}

static UInt32 Adopt(ControlHandle c, SInt32 refCon) {
    UInt32 h = Obj_ControlFor(c);
    if (h) {
        UInt32 p = M68KHeap_Deref(h);
        W32(p + 36, (UInt32)refCon);
    }
    Obj_SyncWindows();
    return h;
}

/* FUNCTION NewControl(theWindow: WindowPtr; boundsRect: Rect; title: Str255;
 *   visible: BOOLEAN; value: INTEGER; min, max: INTEGER; procID: INTEGER;
 *   refCon: LONGINT): ControlHandle */
TRAP(Trap_NewControl) {
    UNUSED;
    SInt32 refCon = (SInt32)Pop32();
    SInt16 procID = (SInt16)Pop16();
    SInt16 max = (SInt16)Pop16(), min = (SInt16)Pop16(), value = (SInt16)Pop16();
    Boolean visible = PopBool();
    Str255 title;
    ReadPString(Pop32(), title);
    Rect bounds;
    ReadRect(Pop32(), &bounds);
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    ControlHandle c = w ? NewControl(w, &bounds, (ConstStr255Param)title, visible, value,
                                     min, max, procID, refCon) : NULL;
    Result32(c ? Adopt(c, refCon) : 0);
    return noErr;
}

/* FUNCTION GetNewControl(controlID: INTEGER; theWindow: WindowPtr): ControlHandle */
TRAP(Trap_GetNewControl) {
    UNUSED;
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    SInt16 id = (SInt16)Pop16();
    ControlHandle c = w ? GetNewControl(id, w) : NULL;
    Result32(c ? Adopt(c, (*c)->contrlRfCon) : 0);
    return noErr;
}

TRAP(Trap_DisposeControl) {
    UNUSED;
    ControlHandle c = PopControl();
    if (c) {
        Obj_ForgetControl(c);
        DisposeControl(c);
        Obj_SyncWindows();
    }
    return noErr;
}

TRAP(Trap_KillControls) {
    UNUSED;
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    if (!w) return noErr;
    for (ControlHandle c = w->controlList; c; c = (*c)->nextControl) Obj_ForgetControl(c);
    KillControls(w);
    Obj_SyncWindows();
    return noErr;
}

#define CONTROL_VERB(name, call) \
    TRAP(name) { UNUSED; ControlHandle c = PopControl(); if (c) { call(c); Obj_SyncControl(c); } return noErr; }
CONTROL_VERB(Trap_ShowControl, ShowControl)
CONTROL_VERB(Trap_HideControl, HideControl)
CONTROL_VERB(Trap_Draw1Control, Draw1Control)

TRAP(Trap_DrawControls) {
    UNUSED;
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    if (w) DrawControls(w);
    return noErr;
}

/* PROCEDURE UpdtControl(theWindow: WindowPtr; updateRgn: RgnHandle) */
TRAP(Trap_UpdtControl) {
    UNUSED;
    (void)Pop32();
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    if (w) DrawControls(w);
    return noErr;
}

TRAP(Trap_HiliteControl) {
    UNUSED;
    SInt16 state = (SInt16)Pop16();
    ControlHandle c = PopControl();
    if (c) {
        HiliteControl(c, state);
        Obj_SyncControl(c);
    }
    return noErr;
}

TRAP(Trap_MoveControl) {
    UNUSED;
    SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16();
    ControlHandle c = PopControl();
    if (c) {
        MoveControl(c, h, v);
        Obj_SyncControl(c);
    }
    return noErr;
}

TRAP(Trap_SizeControl) {
    UNUSED;
    SInt16 ht = (SInt16)Pop16(), wd = (SInt16)Pop16();
    ControlHandle c = PopControl();
    if (c) {
        SizeControl(c, wd, ht);
        Obj_SyncControl(c);
    }
    return noErr;
}

#define CONTROL_SET(name, call) \
    TRAP(name) { UNUSED; SInt16 v = (SInt16)Pop16(); ControlHandle c = PopControl(); \
                 if (c) { call(c, v); Obj_SyncControl(c); } return noErr; }
CONTROL_SET(Trap_SetCtlValue, SetControlValue)
CONTROL_SET(Trap_SetMinCtl, SetControlMinimum)
CONTROL_SET(Trap_SetMaxCtl, SetControlMaximum)

#define CONTROL_GET(name, call) \
    TRAP(name) { UNUSED; ControlHandle c = PopControl(); Result16((UInt16)(c ? call(c) : 0)); return noErr; }
CONTROL_GET(Trap_GetCtlValue, GetControlValue)
CONTROL_GET(Trap_GetMinCtl, GetControlMinimum)
CONTROL_GET(Trap_GetMaxCtl, GetControlMaximum)

TRAP(Trap_SetCTitle) {
    UNUSED;
    Str255 title;
    ReadPString(Pop32(), title);
    ControlHandle c = PopControl();
    if (c) {
        SetControlTitle(c, (ConstStr255Param)title);
        Obj_SyncControl(c);
    }
    return noErr;
}

TRAP(Trap_GetCTitle) {
    UNUSED;
    UInt32 var = Pop32();
    ControlHandle c = PopControl();
    Str255 title;
    title[0] = 0;
    if (c) GetControlTitle(c, title);
    WritePString(var, title);
    return noErr;
}

/* The reference constant lives in the program's record, where it may set it */
TRAP(Trap_SetCRefCon) {
    UNUSED;
    UInt32 value = Pop32();
    UInt32 h = Pop32();
    ControlHandle c = Obj_Control(h);
    if (c) {
        SetControlReference(c, (SInt32)value);
        W32(M68KHeap_Deref(h) + 36, value);
    }
    return noErr;
}

TRAP(Trap_GetCRefCon) {
    UNUSED;
    UInt32 h = Pop32();
    Result32(Obj_Control(h) ? R32(M68KHeap_Deref(h) + 36) : 0);
    return noErr;
}

TRAP(Trap_SetCtlAction) {
    UNUSED;
    UInt32 proc = Pop32();
    UInt32 h = Pop32();
    if (Obj_Control(h)) W32(M68KHeap_Deref(h) + 32, proc);
    return noErr;
}

TRAP(Trap_GetCtlAction) {
    UNUSED;
    UInt32 h = Pop32();
    Result32(Obj_Control(h) ? R32(M68KHeap_Deref(h) + 32) : 0);
    return noErr;
}

/* FUNCTION FindControl(thePoint: Point; theWindow: WindowPtr;
 *   VAR whichControl: ControlHandle): INTEGER */
TRAP(Trap_FindControl) {
    UNUSED;
    UInt32 var = Pop32();
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    Point p = PopPoint();
    ControlHandle c = NULL;
    SInt16 part = w ? FindControl(p, w, &c) : 0;
    W32(var, c ? Obj_ControlFor(c) : 0);
    Result16((UInt16)part);
    return noErr;
}

TRAP(Trap_TestControl) {
    UNUSED;
    Point p = PopPoint();
    ControlHandle c = PopControl();
    Result16((UInt16)(c ? TestControl(c, p) : 0));
    return noErr;
}

/* TrackControl's action procedure: PROCEDURE Action(theControl: ControlHandle;
 * partCode: INTEGER), called while the mouse stays in the part */
static UInt32 gAction;
static ControlHandle gTracked;

static void ActionTrampoline(ControlHandle c, SInt16 part) {
    if (!gAction) return;
    Obj_SyncControl(c);
    A(7) -= 4;
    W32(A(7), Obj_ControlFor(c));
    A(7) -= 2;
    W16(A(7), (UInt16)part);
    CallProgram(gAction);
    Obj_SyncControl(c);
}

/* FUNCTION TrackControl(theControl: ControlHandle; startPt: Point;
 *   actionProc: ProcPtr): INTEGER - a procedure of -1 means the control's own */
TRAP(Trap_TrackControl) {
    UNUSED;
    UInt32 action = Pop32();
    Point p = PopPoint();
    UInt32 h = Pop32();
    ControlHandle c = Obj_Control(h);
    if (!c) {
        Result16(0);
        return noErr;
    }
    if (action == 0xFFFFFFFF) action = R32(M68KHeap_Deref(h) + 32);
    UInt32 savedAction = gAction;
    ControlHandle savedTracked = gTracked;
    gAction = action;
    gTracked = c;
    SInt16 part = TrackControl(c, p, action ? (ControlActionProcPtr)ActionTrampoline : NULL);
    gAction = savedAction;
    gTracked = savedTracked;
    Obj_SyncControl(c);
    Result16((UInt16)part);
    return noErr;
}

/* PROCEDURE DragControl(theControl: ControlHandle; startPt: Point;
 *   limitRect, slopRect: Rect; axis: INTEGER) */
TRAP(Trap_DragControl) {
    UNUSED;
    SInt16 axis = (SInt16)Pop16();
    Rect slop, limit;
    ReadRect(Pop32(), &slop);
    ReadRect(Pop32(), &limit);
    Point p = PopPoint();
    ControlHandle c = PopControl();
    if (c) {
        DragControl(c, p, &limit, &slop, axis);
        Obj_SyncControl(c);
    }
    return noErr;
}

const M68KTrapEntry kM68KControlTraps[] = {
    { 0xA954, Trap_NewControl },    { 0xA9BE, Trap_GetNewControl }, { 0xA955, Trap_DisposeControl },
    { 0xA956, Trap_KillControls },  { 0xA957, Trap_ShowControl },   { 0xA958, Trap_HideControl },
    { 0xA96D, Trap_Draw1Control },  { 0xA969, Trap_DrawControls },  { 0xA953, Trap_UpdtControl },
    { 0xA95D, Trap_HiliteControl }, { 0xA959, Trap_MoveControl },   { 0xA95C, Trap_SizeControl },
    { 0xA963, Trap_SetCtlValue },   { 0xA960, Trap_GetCtlValue },   { 0xA964, Trap_SetMinCtl },
    { 0xA961, Trap_GetMinCtl },     { 0xA965, Trap_SetMaxCtl },     { 0xA962, Trap_GetMaxCtl },
    { 0xA95F, Trap_SetCTitle },     { 0xA95E, Trap_GetCTitle },     { 0xA95B, Trap_SetCRefCon },
    { 0xA95A, Trap_GetCRefCon },    { 0xA96B, Trap_SetCtlAction },  { 0xA96A, Trap_GetCtlAction },
    { 0xA96C, Trap_FindControl },   { 0xA966, Trap_TestControl },   { 0xA968, Trap_TrackControl },
    { 0xA967, Trap_DragControl },
};
const int kM68KControlTrapCount = (int)(sizeof(kM68KControlTraps) / sizeof(kM68KControlTraps[0]));

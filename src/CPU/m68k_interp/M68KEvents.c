/*
 * M68KEvents.c - the Event Manager for a 68K application
 *
 * Events come from the native queue and go into the program's EventRecord,
 * its window in the message replaced by the program's record for it. Events
 * that are not the program's are dealt with here on the way: the Finder's
 * windows still need redrawing while the program runs, and a desk
 * accessory's events go to it through SystemEvent, as GetNextEvent always
 * sent them.
 *
 * EventRecord: what, message, when, where, modifiers - 16 bytes (IM I-249).
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "EventManager/EventManager.h"
#include "WindowManager/WindowManager.h"
#include "DeskManager/DeskManager.h"
#include "System71StdLib.h"

extern Boolean HandleUpdate(EventRecord* event);
extern void GetMouseLocal(Point* mouseLoc);

void M68KTB_ReadEvent(UInt32 a, EventRecord* e) {
    e->what = R16(a + 0);
    e->message = R32(a + 2);
    e->when = R32(a + 6);
    ReadPoint(a + 10, &e->where);
    e->modifiers = R16(a + 14);
    if (e->what == updateEvt || e->what == activateEvt) {
        GrafPtr port = Obj_Port(e->message);
        if (port) e->message = (UInt32)(uintptr_t)port;
    }
}

void M68KTB_WriteEvent(UInt32 a, const EventRecord* e) {
    UInt32 message = e->message;
    if (e->what == updateEvt || e->what == activateEvt) {
        message = Obj_PortFor((GrafPtr)(uintptr_t)e->message);
    }
    W16(a + 0, e->what);
    W32(a + 2, message);
    W32(a + 6, e->when);
    WritePoint(a + 10, e->where);
    W16(a + 14, e->modifiers);
}

static void WriteEvent(UInt32 a, const EventRecord* e, UInt32 message) {
    W16(a + 0, e->what);
    W32(a + 2, message);
    W32(a + 6, e->when);
    WritePoint(a + 10, e->where);
    W16(a + 14, e->modifiers);
}


/* Is the event the program's? If not it is handled here. On true, *message
 * is the message as the program should see it. */
static Boolean ForProgram(EventRecord* e, UInt32* message) {
    *message = e->message;
    if (e->what == updateEvt || e->what == activateEvt) {
        WindowPtr w = (WindowPtr)(uintptr_t)e->message;
        if (Obj_IsAppWindow(w)) {
            *message = Obj_PortFor((GrafPtr)w);
            Obj_SyncWindows();
            return true;
        }
        if (w && w->windowKind < 0) {
            SystemEvent(e);                 /* a desk accessory's */
        } else if (e->what == updateEvt) {
            HandleUpdate(e);                /* the Finder's */
        }
        return false;
    }
    if (e->what == keyDown || e->what == autoKey || e->what == keyUp) {
        WindowPtr front = FrontWindow();
        if (front && front->windowKind < 0 && !(e->modifiers & cmdKey)) {
            SystemEvent(e);
            return false;
        }
    }
    return true;
}

/* FUNCTION GetNextEvent(eventMask: INTEGER; VAR theEvent: EventRecord): BOOLEAN */
TRAP(Trap_GetNextEvent) {
    UNUSED;
    UInt32 var = Pop32();
    UInt16 mask = Pop16();
    EventRecord e;
    memset(&e, 0, sizeof(e));
    Boolean got = GetNextEvent(mask, &e);
    UInt32 message = e.message;
    if (got && !ForProgram(&e, &message)) {
        got = false;
        e.what = nullEvent;
        message = 0;
    }
    WriteEvent(var, &e, message);
    ResultBool(got);
    return noErr;
}

/* FUNCTION WaitNextEvent(eventMask: INTEGER; VAR theEvent: EventRecord;
 *   sleep: LONGINT; mouseRgn: RgnHandle): BOOLEAN */
TRAP(Trap_WaitNextEvent) {
    UNUSED;
    UInt32 rgn = Pop32();
    UInt32 sleep = Pop32();
    UInt32 var = Pop32();
    UInt16 mask = Pop16();
    EventRecord e;
    memset(&e, 0, sizeof(e));
    Boolean got = WaitNextEvent(mask, &e, sleep, Obj_Rgn(rgn));
    UInt32 message = e.message;
    if (got && !ForProgram(&e, &message)) {
        got = false;
        e.what = nullEvent;
        message = 0;
    }
    WriteEvent(var, &e, message);
    ResultBool(got);
    return noErr;
}

TRAP(Trap_EventAvail) {
    UNUSED;
    UInt32 var = Pop32();
    UInt16 mask = Pop16();
    EventRecord e;
    memset(&e, 0, sizeof(e));
    Boolean got = EventAvail(mask, &e);
    UInt32 message = e.message;
    if (got && (e.what == updateEvt || e.what == activateEvt)) {
        WindowPtr w = (WindowPtr)(uintptr_t)e.message;
        message = Obj_IsAppWindow(w) ? Obj_PortFor((GrafPtr)w) : 0;
    }
    WriteEvent(var, &e, message);
    ResultBool(got);
    return noErr;
}

/* GetMouse answers in the current port's coordinates */
TRAP(Trap_GetMouse) {
    UNUSED;
    UInt32 var = Pop32();
    Point p;
    GetMouseLocal(&p);
    WritePoint(var, p);
    return noErr;
}

TRAP(Trap_Button)      { UNUSED; ResultBool(Button()); return noErr; }
TRAP(Trap_StillDown)   { UNUSED; ResultBool(StillDown()); return noErr; }
TRAP(Trap_WaitMouseUp) { UNUSED; ResultBool(WaitMouseUp()); return noErr; }

TRAP(Trap_GetKeys) {
    UNUSED;
    UInt32 var = Pop32();
    KeyMap keys;
    GetKeys(keys);
    WriteBytes(var, keys, sizeof(KeyMap));
    return noErr;
}

/* PROCEDURE SystemClick(theEvent: EventRecord; theWindow: WindowPtr) */
TRAP(Trap_SystemClick) {
    UNUSED;
    WindowPtr w = (WindowPtr)Obj_Port(Pop32());
    EventRecord e;
    M68KTB_ReadEvent(Pop32(), &e);
    if (w) SystemClick(&e, (WindowRecord*)w);
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_SystemEvent) {
    UNUSED;
    EventRecord e;
    M68KTB_ReadEvent(Pop32(), &e);
    ResultBool(false);          /* GetNextEvent already gave them theirs */
    return noErr;
}

TRAP(Trap_SystemTask) { UNUSED; SystemTask(); return noErr; }

TRAP(Trap_SystemEdit) {
    UNUSED;
    SInt16 cmd = (SInt16)Pop16();
    ResultBool(SystemEdit(cmd));
    return noErr;
}

/* FUNCTION OpenDeskAcc(theAcc: Str255): INTEGER */
TRAP(Trap_OpenDeskAcc) {
    UNUSED;
    Str255 name;
    ReadPString(Pop32(), name);
    char cname[256];
    memcpy(cname, &name[1], name[0]);
    cname[name[0]] = '\0';
    Result16((UInt16)OpenDeskAcc(cname));
    Obj_SyncWindows();
    return noErr;
}

TRAP(Trap_CloseDeskAcc) {
    UNUSED;
    CloseDeskAcc((SInt16)Pop16());
    Obj_SyncWindows();
    return noErr;
}

/* PostEvent (OS): A0 the event code, D0 the message */
TRAP(Trap_PostEvent) {
    UNUSED;
    D(0) = (UInt32)(SInt32)PostEvent((EventKind)A(0), D(0));
    return noErr;
}

/* Delay (OS): A0 ticks to wait; D0 the tick count after */
TRAP(Trap_Delay) {
    UNUSED;
    extern UInt32 TickCount(void);
    UInt32 until = TickCount() + A(0);
    while (TickCount() < until) SystemTask();
    D(0) = TickCount();
    return noErr;
}

const M68KTrapEntry kM68KEventTraps[] = {
    { 0xA970, Trap_GetNextEvent },  { 0xA860, Trap_WaitNextEvent }, { 0xA971, Trap_EventAvail },
    { 0xA972, Trap_GetMouse },      { 0xA974, Trap_Button },        { 0xA973, Trap_StillDown },
    { 0xA977, Trap_WaitMouseUp },   { 0xA976, Trap_GetKeys },       { 0xA9B3, Trap_SystemClick },
    { 0xA9B2, Trap_SystemEvent },   { 0xA9B4, Trap_SystemTask },    { 0xA9C2, Trap_SystemEdit },
    { 0xA9B6, Trap_OpenDeskAcc },   { 0xA9B7, Trap_CloseDeskAcc },  { 0xA02F, Trap_PostEvent },
    { 0xA03B, Trap_Delay },
};
const int kM68KEventTrapCount = (int)(sizeof(kM68KEventTraps) / sizeof(kM68KEventTraps[0]));

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
#include "QuickDraw/QuickDraw.h"
#include "DeskManager/DeskManager.h"
#include "System71StdLib.h"
#include "TimeManager/TimeBase.h"

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
            /* A window with a picture is drawn from it, and the program
             * gets no update for it (IM I-275) */
            UInt32 pic = R32(*message + kWindowPicOffset);
            if (e->what == updateEvt && pic) {
                GrafPtr saved;
                GetPort(&saved);
                SetPort((GrafPtr)w);
                BeginUpdate(w);
                Rect frame;
                ReadRect(M68KHeap_Deref(pic) + 2, &frame);          /* picFrame */
                M68KQD_DrawPicture(pic, &frame);
                EndUpdate(w);
                SetPort(saved);
                return false;
            }
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
    M68KTimers_Service();
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
    M68KTimers_Service();
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
    M68KTimers_Service();
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
    UInt32 until = TickCount() + A(0);
    while (TickCount() < until) {
        SystemTask();
        M68KTimers_Service();
    }
    D(0) = TickCount();
    return noErr;
}

/* GetOSEvent and OSEventAvail (OS): A0 the event record, D0 the mask; D0
 * comes back 0 for an event, -1 for a null one. The queue holds no update
 * or activate events, so no window in a message needs translating. */
TRAP(Trap_GetOSEvent) {
    UNUSED;
    EventRecord e;
    memset(&e, 0, sizeof(e));
    Boolean got = GetOSEvent((SInt16)D(0), &e);
    WriteEvent(A(0), &e, e.message);
    D(0) = got ? 0 : (UInt32)-1;
    return noErr;
}

TRAP(Trap_OSEventAvail) {
    UNUSED;
    EventRecord e;
    memset(&e, 0, sizeof(e));
    Boolean got = OSEventAvail((SInt16)D(0), &e);
    WriteEvent(A(0), &e, e.message);
    D(0) = got ? 0 : (UInt32)-1;
    return noErr;
}

/* FUNCTION KeyTrans(transData: Ptr; keycode: INTEGER; VAR state: LONGINT):
 * LONGINT - a key through a KCHR resource (Inside Macintosh VI, the Script
 * Manager's keyboard tables): the modifier table
 * picks a character table, the key's virtual code a character in it, and
 * a dead key waits in state for the key that completes it. keycode is the
 * virtual key in bits 0-6, up in bit 7, the modifiers in the high byte. The
 * answer is one character in bits 0-7, or two - the second in bits 16-23 -
 * when a dead key is followed by one it does not combine with. */
TRAP(Trap_KeyTrans) {
    UNUSED;
    UInt32 stateAddr = Pop32();
    UInt16 keycode = Pop16();
    UInt32 kchr = Pop32();
    UInt32 state = R32(stateAddr);
    if (!kchr) {
        Result32((UInt32)KeyTranslate(NULL, keycode, &state));
        W32(stateAddr, state);
        return noErr;
    }
    Boolean up = (keycode & 0x80) != 0;
    UInt8 vk = keycode & 0x7F;
    UInt8 table = R8(kchr + 2 + (keycode >> 8));
    UInt16 tables = R16(kchr + 258);
    if (table >= tables) table = 0;
    UInt8 ch = R8(kchr + 260 + (UInt32)table * 128 + vk);

    /* The dead-key records follow the tables */
    UInt32 dead = kchr + 260 + (UInt32)tables * 128;
    UInt16 deadCount = R16(dead);
    UInt32 rec = dead + 2;
    if (state && !up) {
        /* The key after a dead key: a completor makes one character, any
         * other key gives the dead key's own and then its own */
        UInt32 r = rec;
        for (UInt32 i = 1; i < state && i <= deadCount; i++) r += 4 + 2 * (UInt32)R16(r + 2) + 2;
        UInt16 n = R16(r + 2);
        UInt32 result = ((UInt32)R8(r + 4 + 2 * (UInt32)n + 1) << 16) | ch;
        for (UInt16 k = 0; k < n; k++) {
            if (R8(r + 4 + 2 * (UInt32)k) == ch) { result = R8(r + 4 + 2 * (UInt32)k + 1); break; }
        }
        W32(stateAddr, 0);
        Result32(result);
        return noErr;
    }
    for (UInt16 i = 0; i < deadCount; i++) {
        if (R8(rec) == table && R8(rec + 1) == vk) {
            if (!up) W32(stateAddr, (UInt32)i + 1);
            Result32(0);
            return noErr;
        }
        rec += 4 + 2 * (UInt32)R16(rec + 2) + 2;
    }
    Result32(ch);
    return noErr;
}

const M68KTrapEntry kM68KEventTraps[] = {
    { 0xA970, Trap_GetNextEvent },  { 0xA860, Trap_WaitNextEvent }, { 0xA971, Trap_EventAvail },
    { 0xA972, Trap_GetMouse },      { 0xA974, Trap_Button },        { 0xA973, Trap_StillDown },
    { 0xA977, Trap_WaitMouseUp },   { 0xA976, Trap_GetKeys },       { 0xA9B3, Trap_SystemClick },
    { 0xA9B2, Trap_SystemEvent },   { 0xA9B4, Trap_SystemTask },    { 0xA9C2, Trap_SystemEdit },
    { 0xA9B6, Trap_OpenDeskAcc },   { 0xA9B7, Trap_CloseDeskAcc },  { 0xA02F, Trap_PostEvent },
    { 0xA03B, Trap_Delay },         { 0xA9C3, Trap_KeyTrans },         { 0xA031, Trap_GetOSEvent },    { 0xA030, Trap_OSEventAvail },
};
const int kM68KEventTrapCount = (int)(sizeof(kM68KEventTraps) / sizeof(kM68KEventTraps[0]));

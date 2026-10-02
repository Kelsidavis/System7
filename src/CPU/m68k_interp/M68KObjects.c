/*
 * M68KObjects.c - the native objects a 68K application holds
 *
 * A program is given windows, ports and regions as pointers and handles it
 * can follow: it reads thePort^.portRect and (**rgn).rgnBBox for itself. So
 * each native object it holds has a record in its memory, laid out as Inside
 * Macintosh lays it out, and this table says which native object each record
 * stands for. The records are written from the native objects after calls
 * that change them; the pen and text state, which programs also set
 * directly, is read back before a drawing call.
 *
 * GrafPort, 108 bytes (IM I-163); WindowRecord, 156 (IM I-275).
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "System71StdLib.h"

extern QDGlobals qd;

enum { kPortSize = 108, kWindowSize = 156, kMaxObjects = 512 };
enum { kKindPort, kKindWindow, kKindRegion };

typedef struct {
    UInt32 addr;            /* the record in the program's memory */
    void* native;
    UInt8 kind;
    Boolean ownRecord;      /* allocated here, not storage the program gave */
    Boolean ownNative;      /* the program's: disposed when it quits */
} Object;

static Object gObjects[kMaxObjects];
static int gObjectCount;

static Object* FindNative(void* native, UInt8 kind) {
    for (int i = 0; i < gObjectCount; i++)
        if (gObjects[i].native == native && gObjects[i].kind == kind) return &gObjects[i];
    return NULL;
}

static Object* FindAddr(UInt32 addr) {
    addr &= 0x00FFFFFF;
    for (int i = 0; i < gObjectCount; i++)
        if (gObjects[i].addr == addr) return &gObjects[i];
    return NULL;
}

static Object* Add(UInt32 addr, void* native, UInt8 kind, Boolean ownRecord, Boolean ownNative) {
    if (gObjectCount >= kMaxObjects) return NULL;
    Object* o = &gObjects[gObjectCount++];
    o->addr = addr & 0x00FFFFFF;
    o->native = native;
    o->kind = kind;
    o->ownRecord = ownRecord;
    o->ownNative = ownNative;
    return o;
}

static void Remove(Object* o) {
    *o = gObjects[--gObjectCount];
}

/* ------------------------------------------------------------------------
 * Regions: a 10-byte record, rgnSize and rgnBBox. A region that is not a
 * rectangle says it is bigger than 10; its outline stays native.
 * ------------------------------------------------------------------------ */

void Obj_SyncRgn(RgnHandle rgn) {
    Object* o = rgn ? FindNative(rgn, kKindRegion) : NULL;
    if (!o || !*rgn) return;
    UInt32 p = M68KHeap_Deref(o->addr);
    if (!p) return;
    W16(p, (*rgn)->rgnSize == 10 ? 10 : 12);
    WriteRect(p + 2, &(*rgn)->rgnBBox);
}

UInt32 Obj_NewRgnRecord(RgnHandle rgn) {
    UInt32 h = M68KHeap_NewHandle(10, true);
    if (!h) return 0;
    if (!Add(h, rgn, kKindRegion, true, true)) {
        M68KHeap_DisposeHandle(h);
        return 0;
    }
    Obj_SyncRgn(rgn);
    return h;
}

UInt32 Obj_RgnFor(RgnHandle rgn) {
    if (!rgn) return 0;
    Object* o = FindNative(rgn, kKindRegion);
    if (o) {
        Obj_SyncRgn(rgn);
        return o->addr;
    }
    UInt32 h = Obj_NewRgnRecord(rgn);
    o = h ? FindNative(rgn, kKindRegion) : NULL;
    if (o) o->ownNative = false;            /* a window's: the WM disposes it */
    return h;
}

RgnHandle Obj_Rgn(UInt32 h) {
    Object* o = h ? FindAddr(h) : NULL;
    return (o && o->kind == kKindRegion) ? (RgnHandle)o->native : NULL;
}

void Obj_ForgetRgn(UInt32 h) {
    Object* o = FindAddr(h);
    if (o && o->kind == kKindRegion) {
        M68KHeap_DisposeHandle(o->addr);
        Remove(o);
    }
}

/* ------------------------------------------------------------------------
 * Ports and windows
 * ------------------------------------------------------------------------ */

/* Where the port's local origin is on the screen. For a window that is its
 * content's corner; a native port keeps it in portBits.bounds, which during
 * an update points at the window's off-screen buffer instead. */
static Point GlobalOrigin(GrafPtr port, Boolean isWindow) {
    Point o;
    if (isWindow) {
        WindowPtr w = (WindowPtr)port;
        Rect c = w->contRgn && *w->contRgn ? (*w->contRgn)->rgnBBox : port->portRect;
        o.h = (SInt16)(c.left - port->portRect.left);
        o.v = (SInt16)(c.top - port->portRect.top);
    } else {
        o.h = port->portBits.bounds.left;
        o.v = port->portBits.bounds.top;
    }
    return o;
}

static void WritePort(Object* o) {
    GrafPtr p = (GrafPtr)o->native;
    UInt32 m = o->addr;
    Boolean isWindow = o->kind == kKindWindow;
    Rect screen = qd.screenBits.bounds;
    UInt16 rowBytes = (UInt16)(((screen.right - screen.left) + 15) / 16 * 2);
    Point g = GlobalOrigin(p, isWindow);

    W16(m + 0, p->device);
    /* portBits as the program expects it: the screen, its bounds placing
     * local (0,0) at the window's corner - classic bounds are the negative
     * of where the origin is */
    W32(m + 2, M68KTB_ScreenBase());
    W16(m + 6, rowBytes);
    Rect b = { (SInt16)(screen.top - g.v), (SInt16)(screen.left - g.h),
               (SInt16)(screen.bottom - g.v), (SInt16)(screen.right - g.h) };
    WriteRect(m + 8, &b);
    WriteRect(m + 16, &p->portRect);
    W32(m + 24, Obj_RgnFor(p->visRgn));
    W32(m + 28, Obj_RgnFor(p->clipRgn));
    WritePattern(m + 32, &p->bkPat);
    WritePattern(m + 40, &p->fillPat);
    WritePoint(m + 48, p->pnLoc);
    WritePoint(m + 52, p->pnSize);
    W16(m + 56, p->pnMode);
    WritePattern(m + 58, &p->pnPat);
    W16(m + 66, p->pnVis);
    W16(m + 68, p->txFont);
    W8(m + 70, p->txFace);
    W8(m + 71, 0);
    W16(m + 72, p->txMode);
    W16(m + 74, p->txSize);
    W32(m + 76, p->spExtra);
    W32(m + 80, p->fgColor);
    W32(m + 84, p->bkColor);
    W16(m + 88, p->colrBit);
    W16(m + 90, p->patStretch);
}

static void WriteWindowFields(Object* o) {
    WindowPtr w = (WindowPtr)o->native;
    UInt32 m = o->addr;
    W16(m + 108, w->windowKind);
    W8(m + 110, w->visible ? 0xFF : 0);
    W8(m + 111, w->hilited ? 0xFF : 0);
    W8(m + 112, w->goAwayFlag ? 0xFF : 0);
    W8(m + 113, 0);
    W32(m + 114, Obj_RgnFor(w->strucRgn));
    W32(m + 118, Obj_RgnFor(w->contRgn));
    W32(m + 122, Obj_RgnFor(w->updateRgn));
    W16(m + 138, w->titleWidth);
    /* The next of the program's own windows; it does not see the Finder's */
    UInt32 next = 0;
    for (WindowPtr n = w->nextWindow; n; n = n->nextWindow) {
        Object* no = FindNative(n, kKindWindow);
        if (no) {
            next = no->addr;
            break;
        }
    }
    W32(m + 144, next);
}

void Obj_SyncPortOut(GrafPtr port) {
    Object* o = port ? FindNative(port, kKindWindow) : NULL;
    if (!o && port) o = FindNative(port, kKindPort);
    if (!o) return;
    WritePort(o);
    if (o->kind == kKindWindow) WriteWindowFields(o);
}

/* The pen and text state, which a program may have set in the record */
void Obj_SyncPortIn(GrafPtr port) {
    Object* o = port ? FindNative(port, kKindWindow) : NULL;
    if (!o && port) o = FindNative(port, kKindPort);
    if (!o) return;
    UInt32 m = o->addr;
    ReadPattern(m + 32, &port->bkPat);
    ReadPattern(m + 40, &port->fillPat);
    ReadPoint(m + 48, &port->pnLoc);
    ReadPoint(m + 52, &port->pnSize);
    port->pnMode = (SInt16)R16(m + 56);
    ReadPattern(m + 58, &port->pnPat);
    port->pnVis = (SInt16)R16(m + 66);
    port->txFont = (SInt16)R16(m + 68);
    port->txFace = R8(m + 70);
    port->txMode = (SInt16)R16(m + 72);
    port->txSize = (SInt16)R16(m + 74);
    port->spExtra = (SInt32)R32(m + 76);
    port->fgColor = (SInt32)R32(m + 80);
    port->bkColor = (SInt32)R32(m + 84);
}

void Obj_SyncWindows(void) {
    for (int i = 0; i < gObjectCount; i++) {
        if (gObjects[i].kind != kKindWindow) continue;
        WritePort(&gObjects[i]);
        WriteWindowFields(&gObjects[i]);
    }
}

UInt32 Obj_NewWindowRecord(WindowPtr w, UInt32 storage) {
    Boolean own = storage == 0;
    UInt32 m = own ? M68KHeap_NewPtr(kWindowSize, true) : storage;
    if (!m) return 0;
    if (!own) for (int i = 0; i < kWindowSize; i++) W8(m + i, 0);
    Object* o = Add(m, w, kKindWindow, own, true);
    if (!o) {
        if (own) M68KHeap_DisposePtr(m);
        return 0;
    }
    WritePort(o);
    WriteWindowFields(o);
    return o->addr;
}

Boolean Obj_IsAppWindow(WindowPtr w) {
    Object* o = w ? FindNative(w, kKindWindow) : NULL;
    return o && o->ownNative;
}

void Obj_ForgetWindow(WindowPtr w) {
    Object* o = FindNative(w, kKindWindow);
    if (!o) return;
    if (o->ownRecord) M68KHeap_DisposePtr(o->addr);
    Remove(o);
    /* Its regions go with it */
    for (int i = gObjectCount - 1; i >= 0; i--) {
        Object* r = &gObjects[i];
        if (r->kind != kKindRegion || r->ownNative) continue;
        RgnHandle rgn = (RgnHandle)r->native;
        if (rgn == w->strucRgn || rgn == w->contRgn || rgn == w->updateRgn ||
            rgn == w->port.visRgn || rgn == w->port.clipRgn) {
            M68KHeap_DisposeHandle(r->addr);
            Remove(r);
        }
    }
}

/* The record for a native port. A window the program did not make - a desk
 * accessory's - gets one too, so it can be told apart by its windowKind. */
UInt32 Obj_PortFor(GrafPtr port) {
    if (!port) return 0;
    Object* o = FindNative(port, kKindWindow);
    if (!o) o = FindNative(port, kKindPort);
    if (o) return o->addr;
    UInt32 m = M68KHeap_NewPtr(kWindowSize, true);
    if (!m) return 0;
    o = Add(m, port, kKindWindow, true, false);
    if (!o) {
        M68KHeap_DisposePtr(m);
        return 0;
    }
    WritePort(o);
    WriteWindowFields(o);
    return o->addr;
}

GrafPtr Obj_Port(UInt32 addr) {
    Object* o = addr ? FindAddr(addr) : NULL;
    return (o && (o->kind == kKindWindow || o->kind == kKindPort)) ? (GrafPtr)o->native : NULL;
}

/* SetPort, and QuickDraw's thePort in the program's globals */
void Obj_SetThePort(GrafPtr port) {
    SetPort(port);
    UInt32 g = M68KTB_QDGlobals();
    if (g) W32(g, Obj_PortFor(port));
}

void Obj_Finish(void) {
    /* The program's windows close with it; regions it made are freed */
    for (int i = gObjectCount - 1; i >= 0; i--) {
        Object* o = &gObjects[i];
        if (o->kind == kKindWindow && o->ownNative) {
            DisposeWindow((WindowPtr)o->native);
        } else if (o->kind == kKindRegion && o->ownNative) {
            DisposeRgn((RgnHandle)o->native);
        }
    }
    gObjectCount = 0;
}

/*
 * M68KQuickDraw.c - QuickDraw for a 68K application
 *
 * Drawing happens in the native port; before each call the pen and text
 * state the program may have set in its copy of the port are read in, and
 * after it the copy is brought up to date (M68KObjects.c). Rectangles,
 * points and patterns come from the program's memory by address.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "chicago_font.h"
#include "FontManager/FontManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"

extern void PlotIcon(const Rect* theRect, Handle theIcon);
extern void DrawChar(short ch);

static GrafPtr gQDPort;

static void Begin(void) {
    GetPort(&gQDPort);
    Obj_SyncPortIn(gQDPort);
}
static void End(void) {
    Obj_SyncPortOut(gQDPort);
}

/* A rectangle argument, by address */
static Rect PopRect(void) {
    Rect r;
    ReadRect(Pop32(), &r);
    return r;
}
static Pattern PopPattern(void) {
    Pattern p;
    ReadPattern(Pop32(), &p);
    return p;
}

/* ------------------------------------------------------------------------
 * Ports
 * ------------------------------------------------------------------------ */

TRAP(Trap_SetPort) {
    UNUSED;
    GrafPtr port = Obj_Port(Pop32());
    if (port) Obj_SetThePort(port);
    return noErr;
}

TRAP(Trap_GetPort) {
    UNUSED;
    UInt32 var = Pop32();
    GrafPtr port;
    GetPort(&port);
    W32(var, Obj_PortFor(port));
    return noErr;
}

TRAP(Trap_GlobalToLocal) {
    UNUSED;
    UInt32 var = Pop32();
    Point p;
    ReadPoint(var, &p);
    GlobalToLocal(&p);
    WritePoint(var, p);
    return noErr;
}

TRAP(Trap_LocalToGlobal) {
    UNUSED;
    UInt32 var = Pop32();
    Point p;
    ReadPoint(var, &p);
    LocalToGlobal(&p);
    WritePoint(var, p);
    return noErr;
}

TRAP(Trap_SetOrigin) {
    UNUSED;
    SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16();
    Begin();
    SetOrigin(h, v);
    End();
    return noErr;
}

TRAP(Trap_ClipRect) {
    UNUSED;
    Rect r = PopRect();
    Begin();
    ClipRect(&r);
    End();
    return noErr;
}

TRAP(Trap_SetClip) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    Begin();
    if (rgn) SetClip(rgn);
    End();
    return noErr;
}

TRAP(Trap_GetClip) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        GetClip(rgn);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

/* ------------------------------------------------------------------------
 * The pen
 * ------------------------------------------------------------------------ */

TRAP(Trap_MoveTo) { UNUSED; SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16(); Begin(); MoveTo(h, v); End(); return noErr; }
TRAP(Trap_Move)   { UNUSED; SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16(); Begin(); Move(h, v); End(); return noErr; }
TRAP(Trap_LineTo) { UNUSED; SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16(); Begin(); LineTo(h, v); End(); return noErr; }
TRAP(Trap_Line)   { UNUSED; SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16(); Begin(); Line(h, v); End(); return noErr; }
TRAP(Trap_PenSize){ UNUSED; SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16(); Begin(); PenSize(h, v); End(); return noErr; }
TRAP(Trap_PenMode){ UNUSED; SInt16 m = (SInt16)Pop16(); Begin(); PenMode(m); End(); return noErr; }
TRAP(Trap_PenPat) { UNUSED; Pattern p = PopPattern(); Begin(); PenPat(&p); End(); return noErr; }
TRAP(Trap_BackPat){ UNUSED; Pattern p = PopPattern(); Begin(); BackPat(&p); End(); return noErr; }
TRAP(Trap_PenNormal) { UNUSED; Begin(); PenNormal(); End(); return noErr; }
TRAP(Trap_HidePen)   { UNUSED; Begin(); HidePen(); End(); return noErr; }
TRAP(Trap_ShowPen)   { UNUSED; Begin(); ShowPen(); End(); return noErr; }
TRAP(Trap_ForeColor) { UNUSED; SInt32 c = (SInt32)Pop32(); Begin(); ForeColor(c); End(); return noErr; }
TRAP(Trap_BackColor) { UNUSED; SInt32 c = (SInt32)Pop32(); Begin(); BackColor(c); End(); return noErr; }

TRAP(Trap_GetPen) {
    UNUSED;
    UInt32 var = Pop32();
    Point p;
    Begin();
    GetPen(&p);
    WritePoint(var, p);
    return noErr;
}

/* PenState: pnLoc, pnSize, pnMode, pnPat - 18 bytes */
TRAP(Trap_GetPenState) {
    UNUSED;
    UInt32 var = Pop32();
    PenState ps;
    Begin();
    GetPenState(&ps);
    WritePoint(var, ps.pnLoc);
    WritePoint(var + 4, ps.pnSize);
    W16(var + 8, ps.pnMode);
    WritePattern(var + 10, &ps.pnPat);
    return noErr;
}

TRAP(Trap_SetPenState) {
    UNUSED;
    UInt32 a = Pop32();
    PenState ps;
    ReadPoint(a, &ps.pnLoc);
    ReadPoint(a + 4, &ps.pnSize);
    ps.pnMode = (SInt16)R16(a + 8);
    ReadPattern(a + 10, &ps.pnPat);
    Begin();
    SetPenState(&ps);
    End();
    return noErr;
}

/* ------------------------------------------------------------------------
 * Text
 * ------------------------------------------------------------------------ */

TRAP(Trap_TextFont) { UNUSED; SInt16 f = (SInt16)Pop16(); Begin(); TextFont(f); End(); return noErr; }
TRAP(Trap_TextFace) { UNUSED; UInt8 f = (UInt8)Pop16(); Begin(); TextFace(f); End(); return noErr; }
TRAP(Trap_TextMode) { UNUSED; SInt16 m = (SInt16)Pop16(); Begin(); TextMode(m); End(); return noErr; }
TRAP(Trap_TextSize) { UNUSED; SInt16 s = (SInt16)Pop16(); Begin(); TextSize(s); End(); return noErr; }

TRAP(Trap_SpaceExtra) {
    UNUSED;
    SInt32 extra = (SInt32)Pop32();
    Begin();
    if (gQDPort) gQDPort->spExtra = extra;
    End();
    return noErr;
}

TRAP(Trap_DrawChar) {
    UNUSED;
    UInt8 ch = (UInt8)Pop16();
    Begin();
    DrawChar(ch);
    End();
    return noErr;
}

TRAP(Trap_DrawString) {
    UNUSED;
    Str255 s;
    ReadPString(Pop32(), s);
    Begin();
    DrawString(s);
    End();
    return noErr;
}

TRAP(Trap_DrawText) {
    UNUSED;
    SInt16 count = (SInt16)Pop16(), first = (SInt16)Pop16();
    UInt32 buf = Pop32();
    char text[512];
    if (count < 0) count = 0;
    if (count > (SInt16)sizeof(text)) count = sizeof(text);
    ReadBytes(buf + (UInt32)first, text, (UInt32)count);
    Begin();
    DrawText(text, 0, count);
    End();
    return noErr;
}

TRAP(Trap_StringWidth) {
    UNUSED;
    Str255 s;
    ReadPString(Pop32(), s);
    Begin();
    Result16((UInt16)StringWidth(s));
    return noErr;
}

TRAP(Trap_CharWidth) {
    UNUSED;
    UInt8 ch = (UInt8)Pop16();
    Begin();
    Result16((UInt16)CharWidth(ch));
    return noErr;
}

TRAP(Trap_TextWidth) {
    UNUSED;
    SInt16 count = (SInt16)Pop16(), first = (SInt16)Pop16();
    UInt32 buf = Pop32();
    char text[512];
    if (count < 0) count = 0;
    if (count > (SInt16)sizeof(text)) count = sizeof(text);
    ReadBytes(buf + (UInt32)first, text, (UInt32)count);
    Begin();
    Result16((UInt16)TextWidth(text, 0, count));
    return noErr;
}

/* GetFontInfo(VAR info): ascent, descent, widMax, leading. The system draws
 * every font with the Chicago strike, so these are its measurements. */
TRAP(Trap_GetFontInfo) {
    UNUSED;
    UInt32 var = Pop32();
    W16(var + 0, CHICAGO_ASCENT);
    W16(var + 2, CHICAGO_DESCENT);
    W16(var + 4, 16);
    W16(var + 6, 0);
    return noErr;
}

/* TextBox(text, length, box, just): the text wrapped at word breaks into the
 * box, each line justified left (0), centred (1) or right (-1) */
TRAP(Trap_TextBox) {
    UNUSED;
    SInt16 just = (SInt16)Pop16();
    Rect box = PopRect();
    SInt32 length = (SInt32)Pop32();
    UInt32 text = Pop32();
    char buf[1024];
    if (length < 0) length = 0;
    if (length > (SInt32)sizeof(buf)) length = sizeof(buf);
    ReadBytes(text, buf, (UInt32)length);
    Begin();
    EraseRect(&box);
    SInt16 width = (SInt16)(box.right - box.left);
    SInt16 y = (SInt16)(box.top + CHICAGO_ASCENT);
    SInt32 at = 0;
    while (at < length && y <= box.bottom) {
        SInt32 end = at, lastBreak = -1;
        while (end < length && buf[end] != '\r') {
            if (TextWidth(buf, (SInt16)at, (SInt16)(end - at + 1)) > width) break;
            if (buf[end] == ' ') lastBreak = end;
            end++;
        }
        if (end < length && buf[end] != '\r' && lastBreak > at) end = lastBreak;
        SInt16 w = TextWidth(buf, (SInt16)at, (SInt16)(end - at));
        SInt16 x = box.left;
        if (just == 1) x = (SInt16)(box.left + (width - w) / 2);
        else if (just == -1) x = (SInt16)(box.right - w);
        MoveTo(x, y);
        DrawText(buf, (SInt16)at, (SInt16)(end - at));
        at = end;
        if (at < length && (buf[at] == ' ' || buf[at] == '\r')) at++;
        y = (SInt16)(y + CHICAGO_HEIGHT + 1);
    }
    End();
    return noErr;
}

/* ------------------------------------------------------------------------
 * Shapes
 * ------------------------------------------------------------------------ */

#define RECT_VERB(name, call) \
    TRAP(name) { UNUSED; Rect r = PopRect(); Begin(); call(&r); End(); return noErr; }
RECT_VERB(Trap_FrameRect, FrameRect)
RECT_VERB(Trap_PaintRect, PaintRect)
RECT_VERB(Trap_EraseRect, EraseRect)
RECT_VERB(Trap_InvertRect, InvertRect)
RECT_VERB(Trap_FrameOval, FrameOval)
RECT_VERB(Trap_PaintOval, PaintOval)
RECT_VERB(Trap_EraseOval, EraseOval)
RECT_VERB(Trap_InvertOval, InvertOval)

TRAP(Trap_FillRect) { UNUSED; Pattern p = PopPattern(); Rect r = PopRect(); Begin(); FillRect(&r, &p); End(); return noErr; }
TRAP(Trap_FillOval) { UNUSED; Pattern p = PopPattern(); Rect r = PopRect(); Begin(); FillOval(&r, &p); End(); return noErr; }

#define RRECT_VERB(name, call) \
    TRAP(name) { UNUSED; SInt16 oh = (SInt16)Pop16(), ow = (SInt16)Pop16(); Rect r = PopRect(); \
                 Begin(); call(&r, ow, oh); End(); return noErr; }
RRECT_VERB(Trap_FrameRoundRect, FrameRoundRect)
RRECT_VERB(Trap_PaintRoundRect, PaintRoundRect)
RRECT_VERB(Trap_EraseRoundRect, EraseRoundRect)
RRECT_VERB(Trap_InvertRoundRect, InvertRoundRect)

TRAP(Trap_FillRoundRect) {
    UNUSED;
    Pattern p = PopPattern();
    SInt16 oh = (SInt16)Pop16(), ow = (SInt16)Pop16();
    Rect r = PopRect();
    Begin();
    FillRoundRect(&r, ow, oh, &p);
    End();
    return noErr;
}

#define ARC_VERB(name, call) \
    TRAP(name) { UNUSED; SInt16 arc = (SInt16)Pop16(), start = (SInt16)Pop16(); Rect r = PopRect(); \
                 Begin(); call(&r, start, arc); End(); return noErr; }
ARC_VERB(Trap_FrameArc, FrameArc)
ARC_VERB(Trap_PaintArc, PaintArc)
ARC_VERB(Trap_EraseArc, EraseArc)
ARC_VERB(Trap_InvertArc, InvertArc)

TRAP(Trap_FillArc) {
    UNUSED;
    Pattern p = PopPattern();
    SInt16 arc = (SInt16)Pop16(), start = (SInt16)Pop16();
    Rect r = PopRect();
    Begin();
    FillArc(&r, start, arc, &p);
    End();
    return noErr;
}

TRAP(Trap_ScrollRect) {
    UNUSED;
    RgnHandle update = Obj_Rgn(Pop32());
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    Rect r = PopRect();
    Begin();
    RgnHandle tmp = update ? update : NewRgn();
    ScrollRect(&r, dh, dv, tmp);
    if (update) Obj_SyncRgn(update);
    else DisposeRgn(tmp);
    End();
    return noErr;
}

/* ------------------------------------------------------------------------
 * Rectangles and points: arithmetic, done here on the program's own records
 * ------------------------------------------------------------------------ */

TRAP(Trap_SetRect) {
    UNUSED;
    SInt16 bottom = (SInt16)Pop16(), right = (SInt16)Pop16();
    SInt16 top = (SInt16)Pop16(), left = (SInt16)Pop16();
    UInt32 var = Pop32();
    Rect r = { top, left, bottom, right };
    WriteRect(var, &r);
    return noErr;
}

TRAP(Trap_OffsetRect) {
    UNUSED;
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    UInt32 var = Pop32();
    Rect r;
    ReadRect(var, &r);
    OffsetRect(&r, dh, dv);
    WriteRect(var, &r);
    return noErr;
}

TRAP(Trap_InsetRect) {
    UNUSED;
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    UInt32 var = Pop32();
    Rect r;
    ReadRect(var, &r);
    InsetRect(&r, dh, dv);
    WriteRect(var, &r);
    return noErr;
}

TRAP(Trap_SectRect) {
    UNUSED;
    UInt32 dst = Pop32();
    Rect b = PopRect(), a = PopRect(), r;
    Boolean any = SectRect(&a, &b, &r);
    WriteRect(dst, &r);
    ResultBool(any);
    return noErr;
}

TRAP(Trap_UnionRect) {
    UNUSED;
    UInt32 dst = Pop32();
    Rect b = PopRect(), a = PopRect(), r;
    UnionRect(&a, &b, &r);
    WriteRect(dst, &r);
    return noErr;
}

TRAP(Trap_PtInRect) {
    UNUSED;
    Rect r = PopRect();
    Point p = PopPoint();
    ResultBool(PtInRect(p, &r));
    return noErr;
}

TRAP(Trap_Pt2Rect) {
    UNUSED;
    UInt32 dst = Pop32();
    Point b = PopPoint(), a = PopPoint();
    Rect r;
    Pt2Rect(a, b, &r);
    WriteRect(dst, &r);
    return noErr;
}

TRAP(Trap_EqualRect) {
    UNUSED;
    Rect b = PopRect(), a = PopRect();
    ResultBool(EqualRect(&a, &b));
    return noErr;
}

TRAP(Trap_EmptyRect) {
    UNUSED;
    Rect r = PopRect();
    ResultBool(EmptyRect(&r));
    return noErr;
}

TRAP(Trap_AddPt) {
    UNUSED;
    UInt32 var = Pop32();
    Point src = PopPoint(), dst;
    ReadPoint(var, &dst);
    dst.h = (SInt16)(dst.h + src.h);
    dst.v = (SInt16)(dst.v + src.v);
    WritePoint(var, dst);
    return noErr;
}

TRAP(Trap_SubPt) {
    UNUSED;
    UInt32 var = Pop32();
    Point src = PopPoint(), dst;
    ReadPoint(var, &dst);
    dst.h = (SInt16)(dst.h - src.h);
    dst.v = (SInt16)(dst.v - src.v);
    WritePoint(var, dst);
    return noErr;
}

TRAP(Trap_SetPt) {
    UNUSED;
    SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16();
    UInt32 var = Pop32();
    Point p = { v, h };
    WritePoint(var, p);
    return noErr;
}

TRAP(Trap_EqualPt) {
    UNUSED;
    Point b = PopPoint(), a = PopPoint();
    ResultBool(a.h == b.h && a.v == b.v);
    return noErr;
}

/* ------------------------------------------------------------------------
 * Regions
 * ------------------------------------------------------------------------ */

TRAP(Trap_NewRgn) {
    UNUSED;
    RgnHandle rgn = NewRgn();
    Result32(rgn ? Obj_NewRgnRecord(rgn) : 0);
    return noErr;
}

TRAP(Trap_DisposeRgn) {
    UNUSED;
    UInt32 h = Pop32();
    RgnHandle rgn = Obj_Rgn(h);
    if (rgn) {
        Obj_ForgetRgn(h);
        DisposeRgn(rgn);
    }
    return noErr;
}

TRAP(Trap_OpenRgn) { UNUSED; Begin(); OpenRgn(); return noErr; }

TRAP(Trap_CloseRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    Begin();
    if (rgn) {
        CloseRgn(rgn);
        Obj_SyncRgn(rgn);
    }
    End();
    return noErr;
}

TRAP(Trap_CopyRgn) {
    UNUSED;
    RgnHandle dst = Obj_Rgn(Pop32()), src = Obj_Rgn(Pop32());
    if (src && dst) {
        CopyRgn(src, dst);
        Obj_SyncRgn(dst);
    }
    return noErr;
}

TRAP(Trap_SetEmptyRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        SetEmptyRgn(rgn);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

TRAP(Trap_SetRectRgn) {
    UNUSED;
    SInt16 bottom = (SInt16)Pop16(), right = (SInt16)Pop16();
    SInt16 top = (SInt16)Pop16(), left = (SInt16)Pop16();
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        SetRectRgn(rgn, left, top, right, bottom);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

TRAP(Trap_RectRgn) {
    UNUSED;
    Rect r = PopRect();
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        RectRgn(rgn, &r);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

TRAP(Trap_OffsetRgn) {
    UNUSED;
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        OffsetRgn(rgn, dh, dv);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

TRAP(Trap_InsetRgn) {
    UNUSED;
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        InsetRgn(rgn, dh, dv);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

#define RGN_OP(name, call) \
    TRAP(name) { UNUSED; RgnHandle d = Obj_Rgn(Pop32()), b = Obj_Rgn(Pop32()), a = Obj_Rgn(Pop32()); \
                 if (a && b && d) { call(a, b, d); Obj_SyncRgn(d); } return noErr; }
RGN_OP(Trap_SectRgn, SectRgn)
RGN_OP(Trap_UnionRgn, UnionRgn)
RGN_OP(Trap_DiffRgn, DiffRgn)
RGN_OP(Trap_XorRgn, XorRgn)

TRAP(Trap_PtInRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    Point p = PopPoint();
    ResultBool(rgn && PtInRgn(p, rgn));
    return noErr;
}

TRAP(Trap_RectInRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    Rect r = PopRect();
    ResultBool(rgn && RectInRgn(&r, rgn));
    return noErr;
}

TRAP(Trap_EqualRgn) {
    UNUSED;
    RgnHandle b = Obj_Rgn(Pop32()), a = Obj_Rgn(Pop32());
    ResultBool(a && b && EqualRgn(a, b));
    return noErr;
}

TRAP(Trap_EmptyRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    ResultBool(!rgn || EmptyRgn(rgn));
    return noErr;
}

#define RGN_VERB(name, call) \
    TRAP(name) { UNUSED; RgnHandle rgn = Obj_Rgn(Pop32()); Begin(); if (rgn) call(rgn); End(); return noErr; }
RGN_VERB(Trap_FrameRgn, FrameRgn)
RGN_VERB(Trap_PaintRgn, PaintRgn)
RGN_VERB(Trap_EraseRgn, EraseRgn)
RGN_VERB(Trap_InvertRgn, InvertRgn)

TRAP(Trap_FillRgn) {
    UNUSED;
    Pattern p = PopPattern();
    RgnHandle rgn = Obj_Rgn(Pop32());
    Begin();
    if (rgn) FillRgn(rgn, &p);
    End();
    return noErr;
}

/* ------------------------------------------------------------------------
 * Odds and ends
 * ------------------------------------------------------------------------ */

/* Random: QuickDraw's generator, on the randSeed in the program's globals
 * (IM I-194), so a program that seeds it gets its own sequence */
TRAP(Trap_Random) {
    UNUSED;
    UInt32 g = M68KTB_QDGlobals();
    UInt32 seed = g ? R32(g - 126) : 1;
    UInt64 next = ((UInt64)seed * 16807) % 0x7FFFFFFF;
    if (next == 0) next = 1;
    if (g) W32(g - 126, (UInt32)next);
    SInt16 r = (SInt16)(next & 0xFFFF);
    Result16((UInt16)(r == -32768 ? 0 : r));
    return noErr;
}

TRAP(Trap_GetPixel) {
    UNUSED;
    SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16();
    Begin();
    ResultBool(GetPixel(h, v));
    return noErr;
}

/* SetCursor(crsr): the 68-byte Cursor record, by address */
TRAP(Trap_SetCursor) {
    UNUSED;
    UInt32 a = Pop32();
    Cursor c;
    for (int i = 0; i < 16; i++) {
        c.data[i] = R16(a + 2 * i);
        c.mask[i] = R16(a + 32 + 2 * i);
    }
    c.hotSpot.v = (SInt16)R16(a + 64);
    c.hotSpot.h = (SInt16)R16(a + 66);
    SetCursor(&c);
    return noErr;
}

TRAP(Trap_HideCursor)    { UNUSED; HideCursor(); return noErr; }
TRAP(Trap_ShowCursor)    { UNUSED; ShowCursor(); return noErr; }
TRAP(Trap_ObscureCursor) { UNUSED; ObscureCursor(); return noErr; }

/* The program's handle, copied into a native one for the length of a call */
static Handle NativeCopy(UInt32 h) {
    UInt32 n = M68KHeap_GetHandleSize(h);
    Handle nh = n ? NewHandle((Size)n) : NULL;
    if (nh) ReadBytes(M68KHeap_Deref(h), *nh, n);
    return nh;
}

TRAP(Trap_DrawPicture) {
    UNUSED;
    Rect r = PopRect();
    UInt32 pic = Pop32();
    Handle nh = pic ? NativeCopy(pic) : NULL;
    Begin();
    if (nh) DrawPicture((PicHandle)nh, &r);
    End();
    if (nh) DisposeHandle(nh);
    return noErr;
}

TRAP(Trap_PlotIcon) {
    UNUSED;
    UInt32 icon = Pop32();
    Rect r = PopRect();
    Handle nh = icon ? NativeCopy(icon) : NULL;
    Begin();
    if (nh) PlotIcon(&r, nh);
    End();
    if (nh) DisposeHandle(nh);
    return noErr;
}

const M68KTrapEntry kM68KQuickDrawTraps[] = {
    { 0xA873, Trap_SetPort },       { 0xA874, Trap_GetPort },       { 0xA871, Trap_GlobalToLocal },
    { 0xA870, Trap_LocalToGlobal }, { 0xA878, Trap_SetOrigin },     { 0xA87B, Trap_ClipRect },
    { 0xA879, Trap_SetClip },       { 0xA87A, Trap_GetClip },
    { 0xA893, Trap_MoveTo },        { 0xA894, Trap_Move },          { 0xA891, Trap_LineTo },
    { 0xA892, Trap_Line },          { 0xA89B, Trap_PenSize },       { 0xA89C, Trap_PenMode },
    { 0xA89D, Trap_PenPat },        { 0xA87C, Trap_BackPat },       { 0xA89E, Trap_PenNormal },
    { 0xA896, Trap_HidePen },       { 0xA897, Trap_ShowPen },       { 0xA862, Trap_ForeColor },
    { 0xA863, Trap_BackColor },     { 0xA89A, Trap_GetPen },        { 0xA898, Trap_GetPenState },
    { 0xA899, Trap_SetPenState },
    { 0xA887, Trap_TextFont },      { 0xA888, Trap_TextFace },      { 0xA889, Trap_TextMode },
    { 0xA88A, Trap_TextSize },      { 0xA88E, Trap_SpaceExtra },    { 0xA883, Trap_DrawChar },
    { 0xA884, Trap_DrawString },    { 0xA885, Trap_DrawText },      { 0xA88C, Trap_StringWidth },
    { 0xA88D, Trap_CharWidth },     { 0xA886, Trap_TextWidth },     { 0xA88B, Trap_GetFontInfo },
    { 0xA9CE, Trap_TextBox },
    { 0xA8A1, Trap_FrameRect },     { 0xA8A2, Trap_PaintRect },     { 0xA8A3, Trap_EraseRect },
    { 0xA8A4, Trap_InvertRect },    { 0xA8A5, Trap_FillRect },      { 0xA8B7, Trap_FrameOval },
    { 0xA8B8, Trap_PaintOval },     { 0xA8B9, Trap_EraseOval },     { 0xA8BA, Trap_InvertOval },
    { 0xA8BB, Trap_FillOval },      { 0xA8B0, Trap_FrameRoundRect },{ 0xA8B1, Trap_PaintRoundRect },
    { 0xA8B2, Trap_EraseRoundRect },{ 0xA8B3, Trap_InvertRoundRect },{ 0xA8B4, Trap_FillRoundRect },
    { 0xA8BE, Trap_FrameArc },      { 0xA8BF, Trap_PaintArc },      { 0xA8C0, Trap_EraseArc },
    { 0xA8C1, Trap_InvertArc },     { 0xA8C2, Trap_FillArc },       { 0xA8EF, Trap_ScrollRect },
    { 0xA8A7, Trap_SetRect },       { 0xA8A8, Trap_OffsetRect },    { 0xA8A9, Trap_InsetRect },
    { 0xA8AA, Trap_SectRect },      { 0xA8AB, Trap_UnionRect },     { 0xA8AD, Trap_PtInRect },
    { 0xA8AC, Trap_Pt2Rect },       { 0xA8A6, Trap_EqualRect },     { 0xA8AE, Trap_EmptyRect },
    { 0xA87E, Trap_AddPt },         { 0xA87F, Trap_SubPt },         { 0xA880, Trap_SetPt },
    { 0xA881, Trap_EqualPt },
    { 0xA8D8, Trap_NewRgn },        { 0xA8D9, Trap_DisposeRgn },    { 0xA8DA, Trap_OpenRgn },
    { 0xA8DB, Trap_CloseRgn },      { 0xA8DC, Trap_CopyRgn },       { 0xA8DD, Trap_SetEmptyRgn },
    { 0xA8DE, Trap_SetRectRgn },    { 0xA8DF, Trap_RectRgn },       { 0xA8E0, Trap_OffsetRgn },
    { 0xA8E1, Trap_InsetRgn },      { 0xA8E4, Trap_SectRgn },       { 0xA8E5, Trap_UnionRgn },
    { 0xA8E6, Trap_DiffRgn },       { 0xA8E7, Trap_XorRgn },        { 0xA8E8, Trap_PtInRgn },
    { 0xA8E9, Trap_RectInRgn },     { 0xA8E3, Trap_EqualRgn },      { 0xA8E2, Trap_EmptyRgn },
    { 0xA8D2, Trap_FrameRgn },      { 0xA8D3, Trap_PaintRgn },      { 0xA8D4, Trap_EraseRgn },
    { 0xA8D5, Trap_InvertRgn },     { 0xA8D6, Trap_FillRgn },
    { 0xA861, Trap_Random },        { 0xA865, Trap_GetPixel },      { 0xA851, Trap_SetCursor },
    { 0xA852, Trap_HideCursor },    { 0xA853, Trap_ShowCursor },    { 0xA856, Trap_ObscureCursor },
    { 0xA8F6, Trap_DrawPicture },   { 0xA94B, Trap_PlotIcon },
};
const int kM68KQuickDrawTrapCount = (int)(sizeof(kM68KQuickDrawTraps) / sizeof(kM68KQuickDrawTraps[0]));

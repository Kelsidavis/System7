/*
 * M68KQuickDraw.c - QuickDraw for a 68K application
 *
 * Drawing happens in the native port; before each call the pen and text
 * state the program may have set in its copy of the port are read in, and
 * after it the copy is brought up to date (M68KObjects.c). Rectangles,
 * points and patterns come from the program's memory by address.
 */

#include <string.h>
#include <math.h>

#include "M68KToolboxInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "WindowManager/WindowManager.h"
#include "chicago_font.h"
#include "FontManager/FontManager.h"
#include "FontManager/FontInternal.h"
#include "ResourceManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include "Toolbox/IconUtilities.h"

static GrafPtr gQDPort;

static void Begin(void) {
    GetPort(&gQDPort);
    Obj_SyncPortIn(gQDPort);
    Ports_BeforeDraw(gQDPort);
}
static void End(void) {
    Ports_AfterDraw(gQDPort);
    Obj_SyncPortOut(gQDPort);
}

/* ------------------------------------------------------------------------
 * Drawing, or recording it when a picture is open on the port
 * ------------------------------------------------------------------------ */

enum { kVerbFrame, kVerbPaint, kVerbErase, kVerbInvert, kVerbFill };

static Boolean Recording(void) { return Pict_Recording(gQDPort); }

static void DoRect(int verb, const Rect* r, const Pattern* pat) {
    if (pat) gQDPort->fillPat = *pat;
    if (Recording()) { Pict_Rect(verb, r); return; }
    switch (verb) {
        case kVerbFrame: FrameRect(r); break;
        case kVerbPaint: PaintRect(r); break;
        case kVerbErase: EraseRect(r); break;
        case kVerbInvert: InvertRect(r); break;
        default: FillRect(r, &gQDPort->fillPat); break;
    }
}

static void DoOval(int verb, const Rect* r, const Pattern* pat) {
    if (pat) gQDPort->fillPat = *pat;
    if (Recording()) { Pict_Oval(verb, r); return; }
    switch (verb) {
        case kVerbFrame: FrameOval(r); break;
        case kVerbPaint: PaintOval(r); break;
        case kVerbErase: EraseOval(r); break;
        case kVerbInvert: InvertOval(r); break;
        default: FillOval(r, &gQDPort->fillPat); break;
    }
}

static void DoRRect(int verb, const Rect* r, SInt16 ow, SInt16 oh, const Pattern* pat) {
    if (pat) gQDPort->fillPat = *pat;
    if (Recording()) { Pict_RRect(verb, r, ow, oh); return; }
    switch (verb) {
        case kVerbFrame: FrameRoundRect(r, ow, oh); break;
        case kVerbPaint: PaintRoundRect(r, ow, oh); break;
        case kVerbErase: EraseRoundRect(r, ow, oh); break;
        case kVerbInvert: InvertRoundRect(r, ow, oh); break;
        default: FillRoundRect(r, ow, oh, &gQDPort->fillPat); break;
    }
}

static void DoArc(int verb, const Rect* r, SInt16 start, SInt16 arc, const Pattern* pat) {
    if (pat) gQDPort->fillPat = *pat;
    if (Recording()) { Pict_Arc(verb, r, start, arc); return; }
    switch (verb) {
        case kVerbFrame: FrameArc(r, start, arc); break;
        case kVerbPaint: PaintArc(r, start, arc); break;
        case kVerbErase: EraseArc(r, start, arc); break;
        case kVerbInvert: InvertArc(r, start, arc); break;
        default: FillArc(r, start, arc, &gQDPort->fillPat); break;
    }
}

static void DoRgn(int verb, RgnHandle rgn, const Pattern* pat) {
    if (!rgn) return;
    if (pat) gQDPort->fillPat = *pat;
    if (Recording()) { Pict_Rgn(verb, rgn); return; }
    switch (verb) {
        case kVerbFrame: FrameRgn(rgn); break;
        case kVerbPaint: PaintRgn(rgn); break;
        case kVerbErase: EraseRgn(rgn); break;
        case kVerbInvert: InvertRgn(rgn); break;
        default: FillRgn(rgn, &gQDPort->fillPat); break;
    }
}

static void DoLineTo(SInt16 h, SInt16 v) {
    if (Recording()) {
        Point to = { v, h };
        Pict_Line(gQDPort->pnLoc, to);
        MoveTo(h, v);
        return;
    }
    LineTo(h, v);
}

static void DoText(const char* text, SInt16 n) {
    if (Recording()) {
        Pict_Text(gQDPort->pnLoc, text, n);
        Move(TextWidth(text, 0, n), 0);
        return;
    }
    DrawText(text, 0, n);
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
    Ports_Offset(gQDPort, (SInt16)(h - gQDPort->portRect.left), (SInt16)(v - gQDPort->portRect.top));
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
TRAP(Trap_LineTo) { UNUSED; SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16(); Begin(); DoLineTo(h, v); End(); return noErr; }
TRAP(Trap_Line) {
    UNUSED;
    SInt16 v = (SInt16)Pop16(), h = (SInt16)Pop16();
    Begin();
    DoLineTo((SInt16)(gQDPort->pnLoc.h + h), (SInt16)(gQDPort->pnLoc.v + v));
    End();
    return noErr;
}
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
    char ch = (char)Pop16();
    Begin();
    DoText(&ch, 1);
    End();
    return noErr;
}

TRAP(Trap_DrawString) {
    UNUSED;
    Str255 s;
    ReadPString(Pop32(), s);
    Begin();
    DoText((const char*)s + 1, s[0]);
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
    DoText(text, count);
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
    DoRect(kVerbErase, &box, NULL);
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
        DoText(buf + at, (SInt16)(end - at));
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

#define RECT_VERB(name, call, verb) \
    TRAP(name) { UNUSED; Rect r = PopRect(); Begin(); call(verb, &r, NULL); End(); return noErr; }
RECT_VERB(Trap_FrameRect, DoRect, kVerbFrame)
RECT_VERB(Trap_PaintRect, DoRect, kVerbPaint)
RECT_VERB(Trap_EraseRect, DoRect, kVerbErase)
RECT_VERB(Trap_InvertRect, DoRect, kVerbInvert)
RECT_VERB(Trap_FrameOval, DoOval, kVerbFrame)
RECT_VERB(Trap_PaintOval, DoOval, kVerbPaint)
RECT_VERB(Trap_EraseOval, DoOval, kVerbErase)
RECT_VERB(Trap_InvertOval, DoOval, kVerbInvert)

TRAP(Trap_FillRect) { UNUSED; Pattern p = PopPattern(); Rect r = PopRect(); Begin(); DoRect(kVerbFill, &r, &p); End(); return noErr; }
TRAP(Trap_FillOval) { UNUSED; Pattern p = PopPattern(); Rect r = PopRect(); Begin(); DoOval(kVerbFill, &r, &p); End(); return noErr; }

#define RRECT_VERB(name, verb) \
    TRAP(name) { UNUSED; SInt16 oh = (SInt16)Pop16(), ow = (SInt16)Pop16(); Rect r = PopRect(); \
                 Begin(); DoRRect(verb, &r, ow, oh, NULL); End(); return noErr; }
RRECT_VERB(Trap_FrameRoundRect, kVerbFrame)
RRECT_VERB(Trap_PaintRoundRect, kVerbPaint)
RRECT_VERB(Trap_EraseRoundRect, kVerbErase)
RRECT_VERB(Trap_InvertRoundRect, kVerbInvert)

TRAP(Trap_FillRoundRect) {
    UNUSED;
    Pattern p = PopPattern();
    SInt16 oh = (SInt16)Pop16(), ow = (SInt16)Pop16();
    Rect r = PopRect();
    Begin();
    DoRRect(kVerbFill, &r, ow, oh, &p);
    End();
    return noErr;
}

#define ARC_VERB(name, verb) \
    TRAP(name) { UNUSED; SInt16 arc = (SInt16)Pop16(), start = (SInt16)Pop16(); Rect r = PopRect(); \
                 Begin(); DoArc(verb, &r, start, arc, NULL); End(); return noErr; }
ARC_VERB(Trap_FrameArc, kVerbFrame)
ARC_VERB(Trap_PaintArc, kVerbPaint)
ARC_VERB(Trap_EraseArc, kVerbErase)
ARC_VERB(Trap_InvertArc, kVerbInvert)

TRAP(Trap_FillArc) {
    UNUSED;
    Pattern p = PopPattern();
    SInt16 arc = (SInt16)Pop16(), start = (SInt16)Pop16();
    Rect r = PopRect();
    Begin();
    DoArc(kVerbFill, &r, start, arc, &p);
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

#define RGN_VERB(name, verb) \
    TRAP(name) { UNUSED; RgnHandle rgn = Obj_Rgn(Pop32()); Begin(); DoRgn(verb, rgn, NULL); End(); return noErr; }
RGN_VERB(Trap_FrameRgn, kVerbFrame)
RGN_VERB(Trap_PaintRgn, kVerbPaint)
RGN_VERB(Trap_EraseRgn, kVerbErase)
RGN_VERB(Trap_InvertRgn, kVerbInvert)

TRAP(Trap_FillRgn) {
    UNUSED;
    Pattern p = PopPattern();
    RgnHandle rgn = Obj_Rgn(Pop32());
    Begin();
    DoRgn(kVerbFill, rgn, &p);
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

/* A program's picture drawn in the current port */
void M68KQD_DrawPicture(UInt32 pic, const Rect* r) {
    Handle nh = pic ? NativeCopy(pic) : NULL;
    Begin();
    if (nh) DrawPicture((PicHandle)nh, r);
    End();
    if (nh) DisposeHandle(nh);
}

TRAP(Trap_DrawPicture) {
    UNUSED;
    Rect r = PopRect();
    UInt32 pic = Pop32();
    M68KQD_DrawPicture(pic, &r);
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


/* ------------------------------------------------------------------------
 * Polygons: the program's own handles - polySize, polyBBox, polyPoints
 * (IM I-190) - which it may read; a native polygon is made from one for
 * the length of a drawing call
 * ------------------------------------------------------------------------ */

static UInt32 gOpenPoly;            /* the program's handle being recorded */
static PolyHandle gOpenNativePoly;

static PolyHandle NativePoly(UInt32 h) {
    UInt32 p = h ? M68KHeap_Deref(h) : 0;
    if (!p) return NULL;
    SInt16 size = (SInt16)R16(p);
    SInt16 n = size >= 10 ? (SInt16)((size - 10) / 4) : 0;
    PolyHandle np = (PolyHandle)NewHandle((Size)(sizeof(SInt16) + sizeof(Rect) + n * sizeof(Point)));
    if (!np) return NULL;
    (*np)->polySize = (SInt16)(sizeof(SInt16) + sizeof(Rect) + n * sizeof(Point));
    ReadRect(p + 2, &(*np)->polyBBox);
    for (SInt16 i = 0; i < n; i++) ReadPoint(p + 10 + 4u * (UInt32)i, &(*np)->polyPoints[i]);
    return np;
}

static void WritePoly(UInt32 h, PolyHandle np) {
    SInt16 n = (SInt16)(((*np)->polySize - (SInt16)(sizeof(SInt16) + sizeof(Rect))) / (SInt16)sizeof(Point));
    if (n < 0) n = 0;
    if (M68KHeap_SetHandleSize(h, 10u + 4u * (UInt32)n) != noErr) return;
    UInt32 p = M68KHeap_Deref(h);
    W16(p, 10 + 4 * n);
    WriteRect(p + 2, &(*np)->polyBBox);
    for (SInt16 i = 0; i < n; i++) WritePoint(p + 10 + 4u * (UInt32)i, (*np)->polyPoints[i]);
}

TRAP(Trap_OpenPoly) {
    UNUSED;
    Begin();
    if (gOpenNativePoly) KillPoly(ClosePoly());
    gOpenNativePoly = OpenPoly();
    gOpenPoly = gOpenNativePoly ? M68KHeap_NewHandle(10, true) : 0;
    if (gOpenPoly) W16(M68KHeap_Deref(gOpenPoly), 10);
    End();
    Result32(gOpenPoly);
    return noErr;
}

TRAP(Trap_ClosePoly) {
    UNUSED;
    Begin();
    PolyHandle np = gOpenNativePoly ? ClosePoly() : NULL;
    if (np && gOpenPoly) WritePoly(gOpenPoly, np);
    if (np) KillPoly(np);
    gOpenNativePoly = NULL;
    gOpenPoly = 0;
    End();
    return noErr;
}

TRAP(Trap_KillPoly) { UNUSED; UInt32 h = Pop32(); if (h) M68KHeap_DisposeHandle(h); return noErr; }

TRAP(Trap_OffsetPoly) {
    UNUSED;
    SInt16 dv = (SInt16)Pop16(), dh = (SInt16)Pop16();
    UInt32 h = Pop32();
    PolyHandle np = NativePoly(h);
    if (!np) return noErr;
    OffsetPoly(np, dh, dv);
    WritePoly(h, np);
    KillPoly(np);
    return noErr;
}

static void DoPoly(int verb, UInt32 h, const Pattern* pat) {
    if (pat) gQDPort->fillPat = *pat;
    if (Recording()) { Pict_Poly(verb, h); return; }
    PolyHandle np = NativePoly(h);
    if (!np) return;
    switch (verb) {
        case kVerbFrame: FramePoly(np); break;
        case kVerbPaint: PaintPoly(np); break;
        case kVerbErase: ErasePoly(np); break;
        case kVerbInvert: InvertPoly(np); break;
        default: FillPoly(np, &gQDPort->fillPat); break;
    }
    KillPoly(np);
}

#define POLY_VERB(name, verb) \
    TRAP(name) { UNUSED; UInt32 h = Pop32(); Begin(); DoPoly(verb, h, NULL); End(); return noErr; }
POLY_VERB(Trap_FramePoly, kVerbFrame)
POLY_VERB(Trap_PaintPoly, kVerbPaint)
POLY_VERB(Trap_ErasePoly, kVerbErase)
POLY_VERB(Trap_InvertPoly, kVerbInvert)

TRAP(Trap_FillPoly) {
    UNUSED;
    Pattern pat = PopPattern();
    UInt32 h = Pop32();
    Begin();
    DoPoly(kVerbFill, h, &pat);
    End();
    return noErr;
}

/* ------------------------------------------------------------------------
 * Mapping between rectangles, and angles
 * ------------------------------------------------------------------------ */

TRAP(Trap_MapPt) {
    UNUSED;
    Rect dst = PopRect(), src = PopRect();
    UInt32 var = Pop32();
    Point p;
    ReadPoint(var, &p);
    MapPt(&p, &src, &dst);
    WritePoint(var, p);
    return noErr;
}

TRAP(Trap_ScalePt) {
    UNUSED;
    Rect dst = PopRect(), src = PopRect();
    UInt32 var = Pop32();
    Point p;
    ReadPoint(var, &p);
    ScalePt(&p, &src, &dst);
    WritePoint(var, p);
    return noErr;
}

TRAP(Trap_MapRect) {
    UNUSED;
    Rect dst = PopRect(), src = PopRect();
    UInt32 var = Pop32();
    Rect r;
    ReadRect(var, &r);
    MapRect(&r, &src, &dst);
    WriteRect(var, &r);
    return noErr;
}

TRAP(Trap_MapRgn) {
    UNUSED;
    Rect dst = PopRect(), src = PopRect();
    RgnHandle rgn = Obj_Rgn(Pop32());
    if (rgn) {
        MapRgn(rgn, &src, &dst);
        Obj_SyncRgn(rgn);
    }
    return noErr;
}

TRAP(Trap_MapPoly) {
    UNUSED;
    Rect dst = PopRect(), src = PopRect();
    UInt32 h = Pop32();
    PolyHandle np = NativePoly(h);
    if (!np) return noErr;
    MapPoly(np, &src, &dst);
    WritePoly(h, np);
    KillPoly(np);
    return noErr;
}

/* PtToAngle(r, pt, VAR angle) */
TRAP(Trap_PtToAngle) {
    UNUSED;
    UInt32 var = Pop32();
    Point pt = PopPoint();
    Rect r = PopRect();
    SInt16 angle = 0;
    PtToAngle(&r, pt, &angle);
    W16(var, angle);
    return noErr;
}

/* Angles are clockwise from twelve o'clock; the slope is dh/dv of a line
 * at that angle, as Fixed (IM I-475) */
TRAP(Trap_SlopeFromAngle) {
    UNUSED;
    SInt16 angle = (SInt16)Pop16();
    angle = (SInt16)(((angle % 180) + 180) % 180);
    SInt32 slope;
    if (angle == 90) slope = 0x7FFFFFFF;
    else slope = (SInt32)(-tan(angle * 3.14159265358979 / 180.0) * 65536.0);
    Result32((UInt32)slope);
    return noErr;
}

TRAP(Trap_AngleFromSlope) {
    UNUSED;
    SInt32 slope = (SInt32)Pop32();
    double a = atan(-(double)slope / 65536.0) * 180.0 / 3.14159265358979;
    SInt16 angle = (SInt16)(a < 0 ? a + 180.5 : a + 0.5);
    if (angle == 0) angle = 180;
    Result16((UInt16)angle);
    return noErr;
}

/* PinRect(r, pt): the nearest point inside, the right and bottom edges
 * being outside (IM I-293) */
TRAP(Trap_PinRect) {
    UNUSED;
    Point pt = PopPoint();
    Rect r = PopRect();
    if (pt.h < r.left) pt.h = r.left;
    if (pt.h >= r.right) pt.h = (SInt16)(r.right - 1);
    if (pt.v < r.top) pt.v = r.top;
    if (pt.v >= r.bottom) pt.v = (SInt16)(r.bottom - 1);
    Result32(((UInt32)(UInt16)pt.v << 16) | (UInt16)pt.h);
    return noErr;
}

/* DeltaPoint(a, b): a minus b, as a long (IM I-475) */
TRAP(Trap_DeltaPoint) {
    UNUSED;
    Point b = PopPoint(), a = PopPoint();
    Result32(((UInt32)(UInt16)(a.v - b.v) << 16) | (UInt16)(a.h - b.h));
    return noErr;
}

/* ShieldCursor(r, offset): the cursor hidden for drawing there; ShowCursor
 * brings it back */
TRAP(Trap_ShieldCursor) { UNUSED; (void)Pop32(); (void)Pop32(); HideCursor(); return noErr; }

/* ------------------------------------------------------------------------
 * Colour, on a port of one bit: the nearest of QuickDraw's eight
 * ------------------------------------------------------------------------ */

static SInt32 NearestOldColor(UInt32 a) {
    Boolean r = R16(a) >= 0x8000, g = R16(a + 2) >= 0x8000, b = R16(a + 4) >= 0x8000;
    static const SInt32 kColors[8] = { blackColor, blueColor, greenColor, cyanColor,
                                       redColor, magentaColor, yellowColor, whiteColor };
    return kColors[(r ? 4 : 0) | (g ? 2 : 0) | (b ? 1 : 0)];
}

static void WriteOldColor(UInt32 a, SInt32 c) {
    UInt16 r = 0, g = 0, b = 0;
    switch (c) {
        case whiteColor:   r = g = b = 0xFFFF; break;
        case redColor:     r = 0xFFFF; break;
        case greenColor:   g = 0xFFFF; break;
        case blueColor:    b = 0xFFFF; break;
        case cyanColor:    g = b = 0xFFFF; break;
        case magentaColor: r = b = 0xFFFF; break;
        case yellowColor:  r = g = 0xFFFF; break;
        default: break;
    }
    W16(a, r); W16(a + 2, g); W16(a + 4, b);
}

TRAP(Trap_RGBForeColor) { UNUSED; SInt32 c = NearestOldColor(Pop32()); Begin(); ForeColor(c); End(); return noErr; }
TRAP(Trap_RGBBackColor) { UNUSED; SInt32 c = NearestOldColor(Pop32()); Begin(); BackColor(c); End(); return noErr; }
TRAP(Trap_GetForeColor) { UNUSED; UInt32 a = Pop32(); Begin(); WriteOldColor(a, gQDPort->fgColor); return noErr; }
TRAP(Trap_GetBackColor) { UNUSED; UInt32 a = Pop32(); Begin(); WriteOldColor(a, gQDPort->bkColor); return noErr; }

/* ------------------------------------------------------------------------
 * Fonts by name and number
 * ------------------------------------------------------------------------ */

TRAP(Trap_GetFNum) {
    UNUSED;
    UInt32 var = Pop32();
    Str255 name;
    ReadPString(Pop32(), name);
    short num = 0;
    GetFNum(name, &num);
    W16(var, (UInt16)num);
    return noErr;
}

TRAP(Trap_GetFontName) {
    UNUSED;
    UInt32 var = Pop32();
    SInt16 num = (SInt16)Pop16();
    Str255 name;
    name[0] = 0;
    GetFontName(num, name);
    WritePString(var, name);
    return noErr;
}

TRAP(Trap_RealFont) {
    UNUSED;
    SInt16 size = (SInt16)Pop16(), num = (SInt16)Pop16();
    ResultBool(RealFont(num, size));
    return noErr;
}

TRAP(Trap_SetFontLock) { UNUSED; SetFontLock(PopBool()); return noErr; }

/* ------------------------------------------------------------------------
 * The bottleneck procedures (IM I-197): what QuickDraw calls for each kind
 * of drawing, which a program may call itself or install in grafProcs
 * ------------------------------------------------------------------------ */

TRAP(Trap_StdText) {
    UNUSED;
    (void)Pop32();                                  /* denom */
    (void)Pop32();                                  /* numer */
    UInt32 buf = Pop32();
    SInt16 count = (SInt16)Pop16();
    char text[512];
    if (count < 0) count = 0;
    if (count > (SInt16)sizeof(text)) count = sizeof(text);
    ReadBytes(buf, text, (UInt32)count);
    Begin();
    DoText(text, count);
    End();
    return noErr;
}

TRAP(Trap_StdLine) { UNUSED; Point p = PopPoint(); Begin(); DoLineTo(p.h, p.v); End(); return noErr; }

TRAP(Trap_StdRect) {
    UNUSED;
    Rect r = PopRect();
    UInt8 verb = PopByte();
    Begin();
    DoRect(verb, &r, NULL);
    End();
    return noErr;
}

TRAP(Trap_StdRRect) {
    UNUSED;
    SInt16 oh = (SInt16)Pop16(), ow = (SInt16)Pop16();
    Rect r = PopRect();
    UInt8 verb = PopByte();
    Begin();
    DoRRect(verb, &r, ow, oh, NULL);
    End();
    return noErr;
}

TRAP(Trap_StdOval) {
    UNUSED;
    Rect r = PopRect();
    UInt8 verb = PopByte();
    Begin();
    DoOval(verb, &r, NULL);
    End();
    return noErr;
}

TRAP(Trap_StdArc) {
    UNUSED;
    SInt16 arc = (SInt16)Pop16(), start = (SInt16)Pop16();
    Rect r = PopRect();
    UInt8 verb = PopByte();
    Begin();
    DoArc(verb, &r, start, arc, NULL);
    End();
    return noErr;
}

TRAP(Trap_StdPoly) {
    UNUSED;
    UInt32 h = Pop32();
    UInt8 verb = PopByte();
    Begin();
    DoPoly(verb, h, NULL);
    End();
    return noErr;
}

TRAP(Trap_StdRgn) {
    UNUSED;
    RgnHandle rgn = Obj_Rgn(Pop32());
    UInt8 verb = PopByte();
    Begin();
    DoRgn(verb, rgn, NULL);
    End();
    return noErr;
}

/* StdBits(VAR srcBits; VAR srcRect, dstRect; mode; maskRgn): CopyBits into
 * the current port's bitmap */
TRAP(Trap_StdBits) {
    UNUSED;
    UInt32 mask = Pop32();
    SInt16 mode = (SInt16)Pop16();
    UInt32 dstRect = Pop32(), srcRect = Pop32(), srcBits = Pop32();
    GrafPtr port;
    GetPort(&port);
    UInt32 m = Obj_PortFor(port);
    if (m) Ports_CopyBits(srcBits, m + 2, srcRect, dstRect, mode, mask);
    return noErr;
}

TRAP(Trap_StdComment) { UNUSED; (void)Pop32(); (void)Pop16(); (void)Pop16(); return noErr; }
TRAP(Trap_StdGetPic)  { UNUSED; (void)Pop16(); (void)Pop32(); return noErr; }
TRAP(Trap_StdPutPic)  { UNUSED; (void)Pop16(); (void)Pop32(); return noErr; }

/* StdTxMeas(count, text, VAR numer, VAR denom, VAR info): the width, with
 * no scaling */
TRAP(Trap_StdTxMeas) {
    UNUSED;
    UInt32 info = Pop32();
    (void)Pop32();
    (void)Pop32();
    UInt32 buf = Pop32();
    SInt16 count = (SInt16)Pop16();
    char text[512];
    if (count < 0) count = 0;
    if (count > (SInt16)sizeof(text)) count = sizeof(text);
    ReadBytes(buf, text, (UInt32)count);
    Begin();
    W16(info + 0, CHICAGO_ASCENT);
    W16(info + 2, CHICAGO_DESCENT);
    W16(info + 4, 16);
    W16(info + 6, 0);
    Result16((UInt16)TextWidth(text, 0, count));
    return noErr;
}

/* SetStdProcs(VAR procs): the thirteen standard procedures. Each is a trap
 * word with the auto-pop bit, so a program calling one through the record
 * returns straight from the trap. */
static UInt32 gStdProcs;

TRAP(Trap_SetStdProcs) {
    UNUSED;
    UInt32 procs = Pop32();
    static const UInt16 kStd[13] = { 0xA882, 0xA890, 0xA8A0, 0xA8AF, 0xA8B6, 0xA8BD, 0xA8C5,
                                     0xA8D1, 0xA8EB, 0xA8F1, 0xA8ED, 0xA8EE, 0xA8F0 };
    if (!gStdProcs) {
        gStdProcs = M68KHeap_NewPtr(2 * 13, false);
        if (!gStdProcs) return noErr;
        for (int i = 0; i < 13; i++) W16(gStdProcs + 2u * (UInt32)i, kStd[i] | 0x0400);
    }
    for (int i = 0; i < 13; i++) W32(procs + 4u * (UInt32)i, gStdProcs + 2u * (UInt32)i);
    return noErr;
}

void M68KQD_Finish(void) {
    gStdProcs = 0;
    if (gOpenNativePoly) KillPoly(ClosePoly());
    gOpenNativePoly = NULL;
    gOpenPoly = 0;
}

/* ------------------------------------------------------------------------
 * _IconDispatch: System 7's Icon Utilities (IM: More Macintosh Toolbox 5)
 *
 * An icon from its family: 'ICN#' (32 by 32, image then mask) or, for a
 * small space, 'ics#' (16 by 16), or failing both the old 'ICON'. The mask
 * clears the icon's shape and the image is drawn into it, so an icon is its
 * shape and not a white square; selected is the shape filled and the image
 * cut out of it, disabled the image thinned to every other dot.
 * ------------------------------------------------------------------------ */

enum { kIconSuiteSize = 8 };            /* our suites: 'SUIT', the resource ID */

typedef struct {
    UInt8 bits[2][128];                 /* image, then mask: 32 rows of 4 bytes */
    int size;                           /* 32 or 16 */
    Boolean hasMask;
} IconImage;

static Boolean IconFromData(const UInt8* d, UInt32 n, Boolean small, IconImage* ic) {
    memset(ic, 0, sizeof(*ic));
    if (small) {
        if (n < 32) return false;
        ic->size = 16;
        for (int r = 0; r < 16; r++) {
            ic->bits[0][r * 4] = d[r * 2]; ic->bits[0][r * 4 + 1] = d[r * 2 + 1];
            if (n >= 64) { ic->bits[1][r * 4] = d[32 + r * 2]; ic->bits[1][r * 4 + 1] = d[32 + r * 2 + 1]; }
        }
        ic->hasMask = n >= 64;
        return true;
    }
    if (n < 128) return false;
    ic->size = 32;
    memcpy(ic->bits[0], d, 128);
    if (n >= 256) memcpy(ic->bits[1], d + 128, 128);
    ic->hasMask = n >= 256;
    return true;
}

static Boolean IconFromResource(ResType type, SInt16 id, Boolean small, IconImage* ic) {
    Handle h = GetResource(type, id);
    return h && *h && IconFromData((const UInt8*)*h, (UInt32)GetHandleSize(h), small, ic);
}

/* The best of the family for a rectangle this size */
static Boolean IconByID(SInt16 id, const Rect* r, IconImage* ic) {
    Boolean small = r->bottom - r->top < 32 || r->right - r->left < 32;
    if (small && IconFromResource(FOURCC('i','c','s','#'), id, true, ic)) return true;
    if (IconFromResource(FOURCC('I','C','N','#'), id, false, ic)) return true;
    return IconFromResource(FOURCC('I','C','O','N'), id, false, ic);
}

/* Where the icon goes: its own size placed by align, or stretched to the
 * rectangle when there is no alignment */
static Rect IconPlace(const Rect* r, SInt16 align, int size) {
    if (align == 0) return *r;
    Rect d;
    SInt16 w = (SInt16)size, h = (SInt16)size;
    switch (align & 0x0C) {
        case 0x08: d.left = r->left; break;                                 /* left */
        case 0x0C: d.left = (SInt16)(r->right - w); break;                  /* right */
        default:   d.left = (SInt16)((r->left + r->right - w) / 2); break;  /* centred */
    }
    switch (align & 0x03) {
        case 0x02: d.top = r->top; break;                                   /* top */
        case 0x03: d.top = (SInt16)(r->bottom - h); break;                  /* bottom */
        default:   d.top = (SInt16)((r->top + r->bottom - h) / 2); break;   /* centred */
    }
    d.right = (SInt16)(d.left + w);
    d.bottom = (SInt16)(d.top + h);
    return d;
}

static void DrawIcon(const IconImage* ic, const Rect* r, SInt16 align, SInt16 transform) {
    UInt8 image[128], mask[128];
    memcpy(image, ic->bits[0], 128);
    if (ic->hasMask) memcpy(mask, ic->bits[1], 128);
    else memset(mask, 0xFF, 128);                       /* no mask: the whole square */
    if ((transform & 0xFF) == 1) {                      /* ttDisabled */
        for (int row = 0; row < 32; row++)
            for (int b = 0; b < 4; b++) image[row * 4 + b] &= (row & 1) ? 0x55 : 0xAA;
    }
    Rect dst = IconPlace(r, align, ic->size);
    BitMap src = { (Ptr)image, 4, { 0, 0, (SInt16)ic->size, (SInt16)ic->size } };
    BitMap msk = { (Ptr)mask, 4, { 0, 0, (SInt16)ic->size, (SInt16)ic->size } };
    Begin();
    Boolean selected = (transform & 0x4000) != 0;       /* ttSelected */
    CopyBits(&msk, &gQDPort->portBits, &msk.bounds, &dst, selected ? srcOr : srcBic, NULL);
    CopyBits(&src, &gQDPort->portBits, &src.bounds, &dst, selected ? srcBic : srcOr, NULL);
    End();
}

static Boolean DrawIconByID(SInt16 id, const Rect* r, SInt16 align, SInt16 transform) {
    IconImage ic;
    if (!IconByID(id, r, &ic)) return false;
    DrawIcon(&ic, r, align, transform);
    return true;
}

TRAP(Trap_PlotIconID) {
    UNUSED;
    SInt16 id = (SInt16)Pop16();
    SInt16 transform = (SInt16)Pop16();
    SInt16 align = (SInt16)Pop16();
    Rect r = PopRect();
    DrawIconByID(id, &r, align, transform);
    return noErr;
}

enum {
    kSelPlotIconID = 0x0500, kSelGetIconSuite = 0x0501, kSelDisposeIconSuite = 0x0302,
    kSelPlotIconSuite = 0x0603, kSelPlotIconHandle = 0x061D, kSelPlotSICNHandle = 0x061E
};

/* _IconDispatch: D0 the selector - the routine in the low byte, the size of
 * its arguments in words in the high byte. Each answers an OSErr. */
TRAP(Trap_IconDispatch) {
    UNUSED;
    UInt16 selector = (UInt16)D(0);
    IconImage ic;
    OSErr err = noErr;
    switch (selector) {
    case kSelPlotIconID: {
        SInt16 id = (SInt16)Pop16(), transform = (SInt16)Pop16(), align = (SInt16)Pop16();
        Rect r = PopRect();
        if (!DrawIconByID(id, &r, align, transform)) err = resNotFound;
        break;
    }
    case kSelGetIconSuite: {
        /* GetIconSuite(VAR theSuite; theResID: INTEGER; selector: IconSelectorValue) */
        (void)Pop32();
        SInt16 id = (SInt16)Pop16();
        UInt32 var = Pop32();
        Rect big = { 0, 0, 32, 32 };
        UInt32 suite = 0;
        if (IconByID(id, &big, &ic)) {
            suite = M68KHeap_NewHandle(kIconSuiteSize, true);
            if (suite) {
                W32(M68KHeap_Deref(suite), FOURCC('S', 'U', 'I', 'T'));
                W16(M68KHeap_Deref(suite) + 4, (UInt16)id);
            }
        }
        W32(var, suite);
        err = suite ? noErr : resNotFound;
        break;
    }
    case kSelDisposeIconSuite: {
        (void)PopBool();                                /* disposeData */
        UInt32 suite = Pop32();
        if (suite && M68KHeap_IsHandle(suite)) M68KHeap_DisposeHandle(suite);
        break;
    }
    case kSelPlotIconSuite: {
        UInt32 suite = Pop32();
        SInt16 transform = (SInt16)Pop16(), align = (SInt16)Pop16();
        Rect r = PopRect();
        if (suite && M68KHeap_IsHandle(suite) && R32(M68KHeap_Deref(suite)) == FOURCC('S','U','I','T') &&
            IconByID((SInt16)R16(M68KHeap_Deref(suite) + 4), &r, &ic)) {
            DrawIcon(&ic, &r, align, transform);
        } else {
            err = paramErr;
        }
        break;
    }
    case kSelPlotIconHandle: case kSelPlotSICNHandle: {
        UInt32 h = Pop32();
        SInt16 transform = (SInt16)Pop16(), align = (SInt16)Pop16();
        Rect r = PopRect();
        UInt32 n = h ? M68KHeap_GetHandleSize(h) : 0;
        UInt8 data[256];
        if (n > sizeof(data)) n = sizeof(data);
        if (n) ReadBytes(M68KHeap_Deref(h), data, n);
        /* An 'ICON' or 'ICN#' by its size; a SICN's first icon */
        Boolean small = selector == kSelPlotSICNHandle;
        if (small && n > 32) n = 32;
        if (IconFromData(data, n, small, &ic)) DrawIcon(&ic, &r, align, transform);
        else err = paramErr;
        break;
    }
    default: {
        static char why[48];
        snprintf(why, sizeof(why), "Icon Utilities selector $%04X not implemented", selector);
        gM68KApp->halted = true;
        gM68KApp->lastException = M68K_VEC_LINE_A;
        gM68KApp->faultReason = why;
        gM68KApp->faultPC = gM68KApp->instrPC;
        return noErr;
    }
    }
    Result16((UInt16)err);
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
    { 0xA8F6, Trap_DrawPicture },   { 0xA94B, Trap_PlotIcon },      { 0xABC9, Trap_IconDispatch },
    { 0xA8CB, Trap_OpenPoly },      { 0xA8CC, Trap_ClosePoly },     { 0xA8CD, Trap_KillPoly },
    { 0xA8CE, Trap_OffsetPoly },    { 0xA8C6, Trap_FramePoly },     { 0xA8C7, Trap_PaintPoly },
    { 0xA8C8, Trap_ErasePoly },     { 0xA8C9, Trap_InvertPoly },    { 0xA8CA, Trap_FillPoly },
    { 0xA8F9, Trap_MapPt },         { 0xA8F8, Trap_ScalePt },       { 0xA8FA, Trap_MapRect },
    { 0xA8FB, Trap_MapRgn },        { 0xA8FC, Trap_MapPoly },       { 0xA8C3, Trap_PtToAngle },
    { 0xA8BC, Trap_SlopeFromAngle },{ 0xA8C4, Trap_AngleFromSlope },{ 0xA94E, Trap_PinRect },
    { 0xA94F, Trap_DeltaPoint },    { 0xA855, Trap_ShieldCursor },
    { 0xAA14, Trap_RGBForeColor },  { 0xAA15, Trap_RGBBackColor },  { 0xAA19, Trap_GetForeColor },
    { 0xAA1A, Trap_GetBackColor },
    { 0xA900, Trap_GetFNum },       { 0xA8FF, Trap_GetFontName },   { 0xA902, Trap_RealFont },
    { 0xA903, Trap_SetFontLock },
    { 0xA831, Trap_PlotIconID },
    { 0xA882, Trap_StdText },       { 0xA890, Trap_StdLine },       { 0xA8A0, Trap_StdRect },
    { 0xA8AF, Trap_StdRRect },      { 0xA8B6, Trap_StdOval },       { 0xA8BD, Trap_StdArc },
    { 0xA8C5, Trap_StdPoly },       { 0xA8D1, Trap_StdRgn },        { 0xA8EB, Trap_StdBits },
    { 0xA8F1, Trap_StdComment },    { 0xA8ED, Trap_StdTxMeas },     { 0xA8EE, Trap_StdGetPic },
    { 0xA8F0, Trap_StdPutPic },     { 0xA8EA, Trap_SetStdProcs },
};
const int kM68KQuickDrawTrapCount = (int)(sizeof(kM68KQuickDrawTraps) / sizeof(kM68KQuickDrawTraps[0]));

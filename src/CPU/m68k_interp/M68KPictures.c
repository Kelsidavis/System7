/*
 * M68KPictures.c - pictures a 68K program records
 *
 * Between OpenPicture and ClosePicture what the program draws in the port
 * is recorded, not shown (OpenPicture hides the pen, IM I-189): each call
 * becomes a version 1 picture opcode in a handle in the program's memory,
 * preceded by whatever pen, text, colour and clip state has changed since
 * the last one. The result is an ordinary PICT, which DrawPicture plays and
 * which the program may put on the Clipboard or in a file.
 *
 * Regions are recorded as their bounding box.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"

static struct {
    GrafPtr port;               /* the port being recorded, or NULL */
    UInt32 handle;              /* the program's PicHandle */
    UInt8* buf;
    UInt32 len, cap;
    Rect picFrame;
    /* State as last recorded */
    SInt16 txFont, txSize, txMode;
    UInt8 txFace;
    SInt32 spExtra;
    Point pnSize;
    SInt16 pnMode;
    Pattern pnPat, fillPat, bkPat;
    SInt32 fg, bk;
    Point ovSize;
    Rect clip;
    Boolean clipSent;
} gPic;

static void Put8(UInt8 b) {
    if (gPic.len >= gPic.cap) {
        UInt32 cap = gPic.cap ? gPic.cap * 2 : 1024;
        UInt8* nb = (UInt8*)NewPtr((Size)cap);
        if (!nb) return;
        if (gPic.buf) {
            memcpy(nb, gPic.buf, gPic.len);
            DisposePtr((Ptr)gPic.buf);
        }
        gPic.buf = nb;
        gPic.cap = cap;
    }
    gPic.buf[gPic.len++] = b;
}
static void Put16(SInt32 v) { Put8((UInt8)(v >> 8)); Put8((UInt8)v); }
static void Put32(UInt32 v) { Put16((SInt32)(v >> 16)); Put16((SInt32)(v & 0xFFFF)); }
static void PutRect(const Rect* r) { Put16(r->top); Put16(r->left); Put16(r->bottom); Put16(r->right); }
static void PutPoint(Point p) { Put16(p.v); Put16(p.h); }
static void PutPat(const Pattern* p) { for (int i = 0; i < 8; i++) Put8(p->pat[i]); }

Boolean Pict_Recording(GrafPtr port) {
    return gPic.port && port == gPic.port;
}

/* What changed since the last opcode, as opcodes */
static void PutState(void) {
    GrafPtr port = gPic.port;
    if (port->clipRgn && *port->clipRgn) {
        Rect c = (*port->clipRgn)->rgnBBox;
        OffsetRect(&c, (SInt16)-port->portBits.bounds.left, (SInt16)-port->portBits.bounds.top);
        if (!gPic.clipSent || !EqualRect(&c, &gPic.clip)) {
            Put8(0x01);
            Put16(10);
            PutRect(&c);
            gPic.clip = c;
            gPic.clipSent = true;
        }
    }
    if (memcmp(&port->bkPat, &gPic.bkPat, 8)) {
        Put8(0x02); PutPat(&port->bkPat); gPic.bkPat = port->bkPat;
    }
    if (port->txFont != gPic.txFont) { Put8(0x03); Put16(port->txFont); gPic.txFont = port->txFont; }
    if (port->txFace != gPic.txFace) { Put8(0x04); Put8(port->txFace); gPic.txFace = port->txFace; }
    if (port->txMode != gPic.txMode) { Put8(0x05); Put16(port->txMode); gPic.txMode = port->txMode; }
    if (port->spExtra != gPic.spExtra) { Put8(0x06); Put32((UInt32)port->spExtra); gPic.spExtra = port->spExtra; }
    if (port->pnSize.h != gPic.pnSize.h || port->pnSize.v != gPic.pnSize.v) {
        Put8(0x07); PutPoint(port->pnSize); gPic.pnSize = port->pnSize;
    }
    if (port->pnMode != gPic.pnMode) { Put8(0x08); Put16(port->pnMode); gPic.pnMode = port->pnMode; }
    if (memcmp(&port->pnPat, &gPic.pnPat, 8)) { Put8(0x09); PutPat(&port->pnPat); gPic.pnPat = port->pnPat; }
    if (memcmp(&port->fillPat, &gPic.fillPat, 8)) {
        Put8(0x0A); PutPat(&port->fillPat); gPic.fillPat = port->fillPat;
    }
    if (port->txSize != gPic.txSize) { Put8(0x0D); Put16(port->txSize); gPic.txSize = port->txSize; }
    if (port->fgColor != gPic.fg) { Put8(0x0E); Put32((UInt32)port->fgColor); gPic.fg = port->fgColor; }
    if (port->bkColor != gPic.bk) { Put8(0x0F); Put32((UInt32)port->bkColor); gPic.bk = port->bkColor; }
}

/* ------------------------------------------------------------------------
 * The drawing calls, as opcodes
 * ------------------------------------------------------------------------ */

void Pict_Rect(int verb, const Rect* r)  { PutState(); Put8((UInt8)(0x30 + verb)); PutRect(r); }
void Pict_Oval(int verb, const Rect* r)  { PutState(); Put8((UInt8)(0x50 + verb)); PutRect(r); }

void Pict_RRect(int verb, const Rect* r, SInt16 ow, SInt16 oh) {
    PutState();
    if (ow != gPic.ovSize.h || oh != gPic.ovSize.v) {
        gPic.ovSize.h = ow;
        gPic.ovSize.v = oh;
        Put8(0x0B);
        PutPoint(gPic.ovSize);
    }
    Put8((UInt8)(0x40 + verb));
    PutRect(r);
}

void Pict_Arc(int verb, const Rect* r, SInt16 start, SInt16 arc) {
    PutState();
    Put8((UInt8)(0x60 + verb));
    PutRect(r);
    Put16(start);
    Put16(arc);
}

/* A polygon: the program's own handle is already the stored form */
void Pict_Poly(int verb, UInt32 poly) {
    UInt32 p = poly ? M68KHeap_Deref(poly) : 0;
    if (!p) return;
    UInt16 size = R16(p);
    PutState();
    Put8((UInt8)(0x70 + verb));
    for (UInt32 i = 0; i < size; i++) Put8(R8(p + i));
}

void Pict_Rgn(int verb, RgnHandle rgn) {
    if (!rgn || !*rgn) return;
    PutState();
    Put8((UInt8)(0x80 + verb));
    Put16(10);
    PutRect(&(*rgn)->rgnBBox);
}

void Pict_Line(Point from, Point to) {
    PutState();
    Put8(0x20);
    PutPoint(from);
    PutPoint(to);
}

void Pict_Text(Point at, const char* text, SInt16 n) {
    while (n > 0) {
        UInt8 chunk = (UInt8)(n > 255 ? 255 : n);
        PutState();
        Put8(0x28);
        PutPoint(at);
        Put8(chunk);
        for (int i = 0; i < chunk; i++) Put8((UInt8)text[i]);
        text += chunk;
        n = (SInt16)(n - chunk);
    }
}

/* A bitmap, one bit a pixel, as BitsRect; bits holds rowBytes-wide rows
 * covering srcRect */
void Pict_Bits(const UInt8* bits, SInt16 rowBytes, const Rect* srcRect, const Rect* dstRect,
               SInt16 mode) {
    PutState();
    Put8(0x90);
    Put16(rowBytes);
    PutRect(srcRect);                   /* bounds */
    PutRect(srcRect);
    PutRect(dstRect);
    Put16(mode);
    SInt32 rows = srcRect->bottom - srcRect->top;
    for (SInt32 i = 0; i < rows * rowBytes; i++) Put8(bits[i]);
}

/* ------------------------------------------------------------------------
 * The traps
 * ------------------------------------------------------------------------ */

static void Reset(void) {
    if (gPic.buf) DisposePtr((Ptr)gPic.buf);
    memset(&gPic, 0, sizeof(gPic));
}

/* OpenPicture(picFrame): PicHandle */
TRAP(Trap_OpenPicture) {
    UNUSED;
    Rect picFrame;
    ReadRect(Pop32(), &picFrame);
    Reset();
    UInt32 h = M68KHeap_NewHandle(10, true);
    if (!h) {
        Result32(0);
        return noErr;
    }
    GetPort(&gPic.port);
    Obj_SyncPortIn(gPic.port);
    gPic.handle = h;
    gPic.picFrame = picFrame;
    /* A new picture's state: what DrawPicture starts from */
    gPic.txMode = srcOr;
    gPic.pnSize.h = gPic.pnSize.v = 1;
    gPic.pnMode = patCopy;
    memset(&gPic.pnPat, 0xFF, 8);
    memset(&gPic.fillPat, 0xFF, 8);
    gPic.fg = blackColor;
    gPic.bk = whiteColor;
    Put16(0);                           /* picSize and picFrame, filled in at the end */
    PutRect(&picFrame);
    Put8(0x11);                         /* version 1 */
    Put8(0x01);
    Result32(h);
    return noErr;
}

TRAP(Trap_ClosePicture) {
    UNUSED;
    if (!gPic.port) return noErr;
    Put8(0xFF);
    if (gPic.buf && gPic.len >= 10) {
        gPic.buf[0] = (UInt8)(gPic.len >> 8);
        gPic.buf[1] = (UInt8)gPic.len;
        if (M68KHeap_SetHandleSize(gPic.handle, gPic.len) == noErr) {
            UInt32 p = M68KHeap_Deref(gPic.handle);
            for (UInt32 i = 0; i < gPic.len; i++) W8(p + i, gPic.buf[i]);
        }
    }
    Reset();
    return noErr;
}

TRAP(Trap_KillPicture) {
    UNUSED;
    UInt32 h = Pop32();
    if (h && h == gPic.handle) Reset();
    if (h) M68KHeap_DisposeHandle(h);
    return noErr;
}

/* PicComment(kind, dataSize, dataHandle) */
TRAP(Trap_PicComment) {
    UNUSED;
    UInt32 data = Pop32();
    SInt16 size = (SInt16)Pop16(), kind = (SInt16)Pop16();
    GrafPtr port;
    GetPort(&port);
    if (!Pict_Recording(port)) return noErr;
    UInt32 p = data ? M68KHeap_Deref(data) : 0;
    if (size <= 0 || !p) {
        Put8(0xA0);
        Put16(kind);
    } else {
        Put8(0xA1);
        Put16(kind);
        Put16(size);
        for (SInt16 i = 0; i < size; i++) Put8(R8(p + (UInt32)i));
    }
    return noErr;
}

void M68KPict_Finish(void) {
    Reset();
}

const M68KTrapEntry kM68KPictureTraps[] = {
    { 0xA8F3, Trap_OpenPicture },   { 0xA8F4, Trap_ClosePicture },  { 0xA8F5, Trap_KillPicture },
    { 0xA8F2, Trap_PicComment },
};
const int kM68KPictureTrapCount = (int)(sizeof(kM68KPictureTraps) / sizeof(kM68KPictureTraps[0]));

/*
 * M68KPorts.c - the ports a 68K program makes for itself, and CopyBits
 *
 * A program opens a GrafPort of its own to draw off the screen: it points
 * portBits at a bitmap in its own memory - one bit a pixel, Inside
 * Macintosh's BitMap - and draws there with QuickDraw, then CopyBits the
 * result into a window. QuickDraw here draws 32 bits a pixel, so such a
 * port draws into a shadow the size of its portRect, and the shadow and the
 * program's bits are brought together around each drawing call: the bits
 * into the shadow where the program changed them, the shadow back into the
 * bits where the drawing did. A port whose portBits are the screen's draws
 * on the screen like a window.
 *
 * CopyBits works on the program's records directly, since one or both of
 * its bitmaps are in the program's memory: the screen's, or a window's,
 * which are the screen seen through that window's bounds, or the program's
 * own.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "QuickDraw/QuickDrawPlatform.h"
#include "WindowManager/WindowManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"


enum { kMaxPorts = 32 };

typedef struct {
    GrafPtr port;           /* native */
    UInt32 base;            /* the program's portBits */
    UInt16 rowBytes;
    Rect bounds;
    Boolean offscreen;      /* base is the program's memory, not the screen */
    UInt32* shadow;         /* offscreen: the portRect, 32 bits a pixel */
    SInt16 shadowW, shadowH;
    UInt8* mirror;          /* the program's bits as last brought together */
    UInt32 mirrorSize;
    Boolean mirrorValid;
} AppPort;

static AppPort gPorts[kMaxPorts];
static int gPortCount;

static AppPort* Find(GrafPtr port) {
    for (int i = 0; i < gPortCount; i++)
        if (gPorts[i].port == port) return &gPorts[i];
    return NULL;
}

static UInt32 Black(void) { return QDPlatform_RGBToPixel(0, 0, 0); }
static UInt32 White(void) { return QDPlatform_RGBToPixel(255, 255, 255); }
static Boolean IsWhite(UInt32 px) { return (px & 0x00FFFFFF) == (White() & 0x00FFFFFF); }

static UInt32 ScreenBase(void) { return M68KTB_ScreenBase() & 0x00FFFFFF; }

static void FreeShadow(AppPort* ap) {
    if (ap->shadow) DisposePtr((Ptr)ap->shadow);
    if (ap->mirror) DisposePtr((Ptr)ap->mirror);
    ap->shadow = NULL;
    ap->mirror = NULL;
    ap->shadowW = ap->shadowH = 0;
    ap->mirrorSize = 0;
    ap->mirrorValid = false;
}

/* The native port drawn into, from the program's portBits and portRect */
static void Configure(AppPort* ap) {
    GrafPtr p = ap->port;
    Rect pr = p->portRect;
    ap->offscreen = (ap->base & 0x00FFFFFF) != ScreenBase();
    if (!ap->offscreen) {
        FreeShadow(ap);
        /* Pixel is local plus native bounds; classic bounds are the negative */
        p->portBits.baseAddr = qd.screenBits.baseAddr;
        p->portBits.rowBytes = qd.screenBits.rowBytes;
        SetRect(&p->portBits.bounds, (SInt16)-ap->bounds.left, (SInt16)-ap->bounds.top,
                (SInt16)(-ap->bounds.left + (ap->bounds.right - ap->bounds.left)),
                (SInt16)(-ap->bounds.top + (ap->bounds.bottom - ap->bounds.top)));
        return;
    }
    SInt16 w = (SInt16)(pr.right - pr.left), h = (SInt16)(pr.bottom - pr.top);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 2048) w = 2048;
    if (h > 2048) h = 2048;
    UInt32 bh = (UInt32)(ap->bounds.bottom > ap->bounds.top ? ap->bounds.bottom - ap->bounds.top : 0);
    UInt32 mirrorSize = (UInt32)ap->rowBytes * bh;
    if (!ap->shadow || ap->shadowW != w || ap->shadowH != h || ap->mirrorSize != mirrorSize) {
        FreeShadow(ap);
        ap->shadow = (UInt32*)NewPtr((Size)w * h * 4);
        ap->mirror = mirrorSize ? (UInt8*)NewPtr((Size)mirrorSize) : NULL;
        if (!ap->shadow) {
            FreeShadow(ap);
            return;
        }
        ap->shadowW = w;
        ap->shadowH = h;
        ap->mirrorSize = ap->mirror ? mirrorSize : 0;
        UInt32 white = White();
        for (Size i = 0; i < (Size)w * h; i++) ap->shadow[i] = white;
    }
    ap->mirrorValid = false;
    /* Native drawing indexes the buffer by local minus the portRect's
     * corner, with bounds as the buffer's own extent */
    p->portBits.baseAddr = (Ptr)ap->shadow;
    p->portBits.rowBytes = (SInt16)(w * 4);
    SetRect(&p->portBits.bounds, 0, 0, w, h);
}

/* ------------------------------------------------------------------------
 * The program's bits and the shadow
 * ------------------------------------------------------------------------ */

/* Bitmap pixel (x, y) in the shadow, or -1 */
static SInt32 ShadowIndex(const AppPort* ap, SInt32 x, SInt32 y) {
    SInt32 sx = ap->bounds.left + x - ap->port->portRect.left;
    SInt32 sy = ap->bounds.top + y - ap->port->portRect.top;
    if (sx < 0 || sy < 0 || sx >= ap->shadowW || sy >= ap->shadowH) return -1;
    return sy * ap->shadowW + sx;
}

/* What the program changed in its bits, into the shadow */
static void Unpack(AppPort* ap) {
    if (!ap->shadow || !ap->mirror) return;
    SInt32 bw = ap->bounds.right - ap->bounds.left;
    SInt32 bh = ap->bounds.bottom - ap->bounds.top;
    UInt32 black = Black(), white = White();
    for (SInt32 y = 0; y < bh; y++) {
        for (SInt32 bx = 0; bx < ap->rowBytes; bx++) {
            UInt32 at = (UInt32)y * ap->rowBytes + (UInt32)bx;
            UInt8 v = R8(ap->base + at);
            if (ap->mirrorValid && ap->mirror[at] == v) continue;
            ap->mirror[at] = v;
            for (int bit = 0; bit < 8; bit++) {
                SInt32 x = bx * 8 + bit;
                if (x >= bw) break;
                SInt32 i = ShadowIndex(ap, x, y);
                if (i >= 0) ap->shadow[i] = (v & (0x80 >> bit)) ? black : white;
            }
        }
    }
    ap->mirrorValid = true;
}

/* What the drawing changed in the shadow, into the program's bits */
static void Pack(AppPort* ap) {
    if (!ap->shadow || !ap->mirror || !ap->mirrorValid) return;
    SInt32 bw = ap->bounds.right - ap->bounds.left;
    SInt32 bh = ap->bounds.bottom - ap->bounds.top;
    for (SInt32 y = 0; y < bh; y++) {
        for (SInt32 bx = 0; bx < ap->rowBytes; bx++) {
            UInt32 at = (UInt32)y * ap->rowBytes + (UInt32)bx;
            UInt8 v = ap->mirror[at];
            for (int bit = 0; bit < 8; bit++) {
                SInt32 x = bx * 8 + bit;
                if (x >= bw) break;
                SInt32 i = ShadowIndex(ap, x, y);
                if (i < 0) continue;
                if (IsWhite(ap->shadow[i])) v &= (UInt8)~(0x80 >> bit);
                else v |= (UInt8)(0x80 >> bit);
            }
            if (v != ap->mirror[at]) {
                ap->mirror[at] = v;
                W8(ap->base + at, v);
            }
        }
    }
}

/* ------------------------------------------------------------------------
 * For M68KObjects.c and the QuickDraw traps
 * ------------------------------------------------------------------------ */

Boolean Ports_Bits(GrafPtr port, UInt32* base, UInt16* rowBytes, Rect* bounds) {
    AppPort* ap = Find(port);
    if (!ap) return false;
    *base = ap->base;
    *rowBytes = ap->rowBytes;
    *bounds = ap->bounds;
    return true;
}

/* portBits and portRect as the program has them in its record: it often
 * sets them there itself rather than through SetPortBits and PortSize */
void Ports_ReadRecord(GrafPtr port, UInt32 m) {
    AppPort* ap = Find(port);
    if (!ap) return;
    UInt32 base = R32(m + 2) & 0x00FFFFFF;
    UInt16 rowBytes = R16(m + 6) & 0x3FFF;
    Rect bounds, pr;
    ReadRect(m + 8, &bounds);
    ReadRect(m + 16, &pr);
    if (base == ap->base && rowBytes == ap->rowBytes && EqualRect(&bounds, &ap->bounds) &&
        EqualRect(&pr, &port->portRect)) return;
    if (ap->offscreen && ap->mirrorValid) Pack(ap);
    ap->base = base;
    ap->rowBytes = rowBytes;
    ap->bounds = bounds;
    port->portRect = pr;
    Configure(ap);
}

void Ports_BeforeDraw(GrafPtr port) {
    AppPort* ap = Find(port);
    if (ap && ap->offscreen) Unpack(ap);
}

void Ports_AfterDraw(GrafPtr port) {
    AppPort* ap = Find(port);
    if (ap && ap->offscreen) Pack(ap);
}

Boolean Ports_IsOffscreen(GrafPtr port) {
    AppPort* ap = Find(port);
    return ap && ap->offscreen;
}

/* SetOrigin moves the bitmap's bounds with the portRect (IM I-166) */
void Ports_Offset(GrafPtr port, SInt16 dh, SInt16 dv) {
    AppPort* ap = Find(port);
    if (ap) OffsetRect(&ap->bounds, dh, dv);
}

void Ports_Finish(void) {
    GrafPtr cur;
    GetPort(&cur);
    if (Find(cur)) {
        GrafPtr wmgr = NULL;
        GetWMgrPort(&wmgr);
        if (wmgr) SetPort(wmgr);
    }
    for (int i = 0; i < gPortCount; i++) {
        FreeShadow(&gPorts[i]);
        if (gPorts[i].port) {
            ClosePort(gPorts[i].port);
            DisposePtr((Ptr)gPorts[i].port);
        }
    }
    gPortCount = 0;
}

/* ------------------------------------------------------------------------
 * The traps
 * ------------------------------------------------------------------------ */

/* OpenPort(port): the program's 108 bytes, a native port behind them, made
 * current; it starts on the whole screen (IM I-163) */
static GrafPtr OpenAppPort(UInt32 addr) {
    GrafPtr existing = Obj_Port(addr);
    AppPort* ap = existing ? Find(existing) : NULL;
    if (!ap) {
        if (gPortCount >= kMaxPorts) return NULL;
        GrafPtr p = (GrafPtr)NewPtrClear(sizeof(GrafPort));
        if (!p) return NULL;
        if (!Obj_AddPort(addr, p)) {
            DisposePtr((Ptr)p);
            return NULL;
        }
        ap = &gPorts[gPortCount++];
        memset(ap, 0, sizeof(*ap));
        ap->port = p;
    } else {
        FreeShadow(ap);
        ClosePort(ap->port);
    }
    InitPort(ap->port);
    Rect screen = qd.screenBits.bounds;
    ap->base = ScreenBase();
    ap->rowBytes = (UInt16)(((screen.right - screen.left) + 15) / 16 * 2);
    ap->bounds = screen;
    Configure(ap);
    Obj_SetThePort(ap->port);
    Obj_SyncPortOut(ap->port);
    return ap->port;
}

TRAP(Trap_OpenPort) { UNUSED; OpenAppPort(Pop32()); return noErr; }
TRAP(Trap_InitPort) { UNUSED; OpenAppPort(Pop32()); return noErr; }

TRAP(Trap_ClosePort) {
    UNUSED;
    UInt32 addr = Pop32();
    GrafPtr p = Obj_Port(addr);
    AppPort* ap = p ? Find(p) : NULL;
    if (!ap) return noErr;
    Obj_LeavePort(p);
    FreeShadow(ap);
    ClosePort(p);
    Obj_ForgetPort(p);
    DisposePtr((Ptr)p);
    *ap = gPorts[--gPortCount];
    return noErr;
}

/* SetPortBits(bm): the current port's bitmap */
TRAP(Trap_SetPortBits) {
    UNUSED;
    UInt32 bm = Pop32();
    GrafPtr p;
    GetPort(&p);
    UInt32 m = Obj_PortFor(p);
    if (!m || !Find(p)) return noErr;
    for (int i = 0; i < 14; i++) W8(m + 2 + i, R8(bm + i));
    Ports_ReadRecord(p, m);
    return noErr;
}

TRAP(Trap_PortSize) {
    UNUSED;
    SInt16 h = (SInt16)Pop16(), w = (SInt16)Pop16();
    GrafPtr p;
    GetPort(&p);
    UInt32 m = Obj_PortFor(p);
    Rect pr = p->portRect;
    pr.right = (SInt16)(pr.left + w);
    pr.bottom = (SInt16)(pr.top + h);
    if (Find(p) && m) {
        WriteRect(m + 16, &pr);
        Ports_ReadRecord(p, m);
    } else {
        PortSize(w, h);
        Obj_SyncPortOut(p);
    }
    return noErr;
}

/* MovePortTo(left, top): the portRect's corner to that point of the screen,
 * by moving the bitmap's bounds (IM I-166) */
TRAP(Trap_MovePortTo) {
    UNUSED;
    SInt16 top = (SInt16)Pop16(), left = (SInt16)Pop16();
    GrafPtr p;
    GetPort(&p);
    UInt32 m = Obj_PortFor(p);
    if (!Find(p) || !m) return noErr;
    Rect b;
    ReadRect(m + 8, &b);
    SInt16 w = (SInt16)(b.right - b.left), h = (SInt16)(b.bottom - b.top);
    b.left = (SInt16)(p->portRect.left - left);
    b.top = (SInt16)(p->portRect.top - top);
    b.right = (SInt16)(b.left + w);
    b.bottom = (SInt16)(b.top + h);
    WriteRect(m + 8, &b);
    Ports_ReadRecord(p, m);
    return noErr;
}

/* ------------------------------------------------------------------------
 * CopyBits(srcBits, dstBits, srcRect, dstRect, mode, maskRgn) - IM I-188
 * ------------------------------------------------------------------------ */

typedef struct {
    Boolean screen;         /* the screen, or a window's view of it */
    UInt32 base;
    UInt16 rowBytes;
    Rect bounds;
    Boolean ok;
} Bits;

static Bits ReadBits(UInt32 a) {
    Bits b;
    memset(&b, 0, sizeof(b));
    if (!a) return b;
    b.base = R32(a) & 0x00FFFFFF;
    UInt16 rb = R16(a + 4);
    b.rowBytes = rb & 0x3FFF;
    ReadRect(a + 6, &b.bounds);
    b.screen = b.base == ScreenBase();
    /* A PixMap deeper than a bit is not drawn here */
    if ((rb & 0x8000) && !b.screen && R16(a + 32) != 1) return b;
    b.ok = b.screen || b.rowBytes > 0;
    return b;
}

static Boolean MemBit(const Bits* b, SInt32 x, SInt32 y) {
    if (x < b->bounds.left || x >= b->bounds.right || y < b->bounds.top || y >= b->bounds.bottom)
        return false;
    UInt32 at = b->base + (UInt32)(y - b->bounds.top) * b->rowBytes + (UInt32)((x - b->bounds.left) >> 3);
    return (R8(at) & (0x80 >> ((x - b->bounds.left) & 7))) != 0;
}

static void SetMemBit(const Bits* b, SInt32 x, SInt32 y, Boolean black) {
    if (x < b->bounds.left || x >= b->bounds.right || y < b->bounds.top || y >= b->bounds.bottom)
        return;
    UInt32 at = b->base + (UInt32)(y - b->bounds.top) * b->rowBytes + (UInt32)((x - b->bounds.left) >> 3);
    UInt8 mask = (UInt8)(0x80 >> ((x - b->bounds.left) & 7));
    UInt8 v = R8(at);
    UInt8 nv = black ? (UInt8)(v | mask) : (UInt8)(v & ~mask);
    if (nv != v) W8(at, nv);
}

void Ports_CopyBits(UInt32 srcBits, UInt32 dstBits, UInt32 srcRect, UInt32 dstRect,
                    SInt16 mode, UInt32 maskRgn) {
    RgnHandle mask = Obj_Rgn(maskRgn);
    Rect dr, sr;
    ReadRect(dstRect, &dr);
    ReadRect(srcRect, &sr);
    Bits dst = ReadBits(dstBits);
    Bits src = ReadBits(srcBits);
    if (!dst.ok || !src.ok || EmptyRect(&sr) || EmptyRect(&dr)) return;

    GrafPtr cur;
    GetPort(&cur);
    AppPort* curApp = Find(cur);
    if (curApp && curApp->offscreen) Unpack(curApp);

    /* The screen is read and written through a port that draws on it: the
     * current one, unless that is off the screen */
    GrafPtr screenPort = cur;
    if (curApp && curApp->offscreen) GetWMgrPort(&screenPort);

    SInt32 dw = dr.right - dr.left, dh = dr.bottom - dr.top;
    SInt32 sw = sr.right - sr.left, sh = sr.bottom - sr.top;
    if (dw > 2048 || dh > 2048) return;
    UInt32* px = (UInt32*)NewPtr((Size)dw * dh * 4);
    if (!px) return;

    UInt32 fg = QDPlatform_MapQDColor(cur ? cur->fgColor : blackColor);
    UInt32 bg = QDPlatform_MapQDColor(cur ? cur->bkColor : whiteColor);

    /* The source first, all of it: a copy within one bitmap may overlap */
    SetPort(screenPort);
    for (SInt32 y = 0; y < dh; y++) {
        SInt32 syy = sr.top + (y * sh) / dh;
        for (SInt32 x = 0; x < dw; x++) {
            SInt32 sxx = sr.left + (x * sw) / dw;
            UInt32 c;
            if (src.screen) c = QDPlatform_GetPixel(sxx - src.bounds.left, syy - src.bounds.top);
            else c = MemBit(&src, sxx, syy) ? fg : bg;
            px[y * dw + x] = c;
        }
    }

    /* Into a port recording a picture: recorded, as a bitmap at the size it
     * lands, not drawn */
    if (Pict_Recording(cur)) {
        UInt32 m = Obj_PortFor(cur);
        Bits mine = ReadBits(m ? m + 2 : 0);
        if (mine.ok && mine.base == dst.base && mine.rowBytes == dst.rowBytes &&
            EqualRect(&mine.bounds, &dst.bounds)) {
            SInt16 rb = (SInt16)(((dw + 15) / 16) * 2);
            UInt8* bits = (UInt8*)NewPtrClear((Size)rb * dh);
            if (bits) {
                for (SInt32 y = 0; y < dh; y++)
                    for (SInt32 x = 0; x < dw; x++)
                        if (!IsWhite(px[y * dw + x])) bits[y * rb + (x >> 3)] |= (UInt8)(0x80 >> (x & 7));
                Rect area = { 0, 0, (SInt16)dh, (SInt16)dw };
                Pict_Bits(bits, rb, &area, &dr, mode);
                DisposePtr((Ptr)bits);
            }
            SetPort(cur);
            DisposePtr((Ptr)px);
            return;
        }
    }

    Boolean notSrc = (mode & 4) != 0;
    int op = mode & 3;                  /* copy, or, xor, bic */
    /* The destination is clipped to the current port's clip region when the
     * port is the one drawing into it */
    RgnHandle clip = NULL;
    Point clipOff = { 0, 0 };
    if (!dst.screen && curApp && curApp->offscreen && curApp->base == dst.base) {
        clip = cur->clipRgn;
        clipOff.h = cur->portBits.bounds.left;
        clipOff.v = cur->portBits.bounds.top;
    }
    if (dst.screen) QD_ClipBegin(screenPort);
    for (SInt32 y = 0; y < dh; y++) {
        for (SInt32 x = 0; x < dw; x++) {
            SInt32 dx = dr.left + x, dy = dr.top + y;
            if (mask) {
                Point pt = { (SInt16)dy, (SInt16)dx };
                if (!PtInRgn(pt, mask)) continue;
            }
            if (clip) {
                Point pt = { (SInt16)(dy - cur->portRect.top + clipOff.v),
                             (SInt16)(dx - cur->portRect.left + clipOff.h) };
                if (!PtInRgn(pt, clip)) continue;
            }
            UInt32 c = px[y * dw + x];
            Boolean s = !IsWhite(c);
            if (notSrc) {
                s = !s;
                c = s ? fg : bg;
            }
            if (dst.screen) {
                SInt32 gx = dx - dst.bounds.left, gy = dy - dst.bounds.top;
                switch (op) {
                    case 0: QDPlatform_SetPixel(gx, gy, c); break;
                    case 1: if (s) QDPlatform_SetPixel(gx, gy, fg); break;
                    case 2: if (s) QDPlatform_SetPixel(gx, gy, QDPlatform_GetPixel(gx, gy) ^ 0x00FFFFFF); break;
                    case 3: if (s) QDPlatform_SetPixel(gx, gy, bg); break;
                }
            } else {
                switch (op) {
                    case 0: SetMemBit(&dst, dx, dy, s); break;
                    case 1: if (s) SetMemBit(&dst, dx, dy, true); break;
                    case 2: if (s) SetMemBit(&dst, dx, dy, !MemBit(&dst, dx, dy)); break;
                    case 3: if (s) SetMemBit(&dst, dx, dy, false); break;
                }
            }
        }
    }
    if (dst.screen) QD_ClipEnd();
    SetPort(cur);
    DisposePtr((Ptr)px);
}

TRAP(Trap_CopyBits) {
    UNUSED;
    UInt32 mask = Pop32();
    SInt16 mode = (SInt16)Pop16();
    UInt32 dr = Pop32(), sr = Pop32(), dst = Pop32(), src = Pop32();
    Ports_CopyBits(src, dst, sr, dr, mode, mask);
    return noErr;
}

/* ------------------------------------------------------------------------
 * PackBits and UnpackBits (IM I-470): run-length coding of bytes
 * ------------------------------------------------------------------------ */

/* PackBits(VAR srcPtr, VAR dstPtr: Ptr; srcBytes) - both pointers advanced */
TRAP(Trap_PackBits) {
    UNUSED;
    SInt16 n = (SInt16)Pop16();
    UInt32 dstVar = Pop32(), srcVar = Pop32();
    UInt32 s = R32(srcVar), d = R32(dstVar);
    SInt32 i = 0;
    while (i < n) {
        /* A run of three or more alike is worth a repeat */
        SInt32 run = 1;
        UInt8 b = R8(s + (UInt32)i);
        while (i + run < n && run < 128 && R8(s + (UInt32)(i + run)) == b) run++;
        if (run >= 3 || (run == 2 && i + run >= n)) {
            W8(d++, (UInt8)(1 - run));
            W8(d++, b);
            i += run;
            continue;
        }
        /* Otherwise literal bytes, up to the next run of three */
        SInt32 start = i, len = 0;
        while (i < n && len < 128) {
            if (i + 2 < n && R8(s + (UInt32)i) == R8(s + (UInt32)(i + 1)) &&
                R8(s + (UInt32)i) == R8(s + (UInt32)(i + 2))) break;
            i++;
            len++;
        }
        W8(d++, (UInt8)(len - 1));
        for (SInt32 k = 0; k < len; k++) W8(d++, R8(s + (UInt32)(start + k)));
    }
    W32(srcVar, s + (UInt32)n);
    W32(dstVar, d);
    return noErr;
}

/* UnpackBits(VAR srcPtr, VAR dstPtr: Ptr; dstBytes) */
TRAP(Trap_UnpackBits) {
    UNUSED;
    SInt16 n = (SInt16)Pop16();
    UInt32 dstVar = Pop32(), srcVar = Pop32();
    UInt32 s = R32(srcVar), d = R32(dstVar);
    SInt32 out = 0;
    while (out < n) {
        SInt8 c = (SInt8)R8(s++);
        if (c >= 0) {
            for (SInt32 k = 0; k <= c && out < n; k++, out++) W8(d++, R8(s++));
        } else if (c != -128) {
            UInt8 b = R8(s++);
            for (SInt32 k = 0; k < 1 - c && out < n; k++, out++) W8(d++, b);
        }
    }
    W32(srcVar, s);
    W32(dstVar, d);
    return noErr;
}

const M68KTrapEntry kM68KPortTraps[] = {
    { 0xA86F, Trap_OpenPort },      { 0xA86D, Trap_InitPort },      { 0xA87D, Trap_ClosePort },
    { 0xA875, Trap_SetPortBits },   { 0xA876, Trap_PortSize },      { 0xA877, Trap_MovePortTo },
    { 0xA8EC, Trap_CopyBits },      { 0xA8CF, Trap_PackBits },      { 0xA8D0, Trap_UnpackBits },
};
const int kM68KPortTrapCount = (int)(sizeof(kM68KPortTraps) / sizeof(kM68KPortTraps[0]));

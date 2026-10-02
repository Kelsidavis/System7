#include "QuickDraw/QuickDrawInternal.h"
/*
 * QuickDraw picture playback - DrawPicture
 *
 * A picture is a header - picSize, picFrame - and a stream of opcodes, each
 * followed by its data (Inside Macintosh: Imaging With QuickDraw, appendix
 * A). Version 1 pictures have one-byte opcodes and unaligned data; version
 * 2 pictures, which begin $0011 $02FF, have two-byte opcodes, each starting
 * on an even byte. Everything is big-endian, as the resource file has it.
 *
 * Each opcode's data has a known size, so an opcode not drawn here is
 * stepped over rather than ending the picture. Coordinates are mapped from
 * picFrame onto the destination rectangle. The port's drawing state is
 * saved before and put back after, so the picture's pen, text and clip do
 * not leak into what the caller draws next.
 */

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/quickdraw_types.h"
#include "QuickDraw/QuickDrawPlatform.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "SystemTypes.h"
#include "QuickDrawConstants.h"
#include "MemoryMgr/MemoryManager.h"
#include "FontManager/FontManager.h"

#include <string.h>

extern GrafPtr g_currentPort;
extern CGrafPtr g_currentCPort;

typedef struct {
    const UInt8* base;
    const UInt8* ptr;
    const UInt8* end;
    Boolean v2;
    Boolean bad;
    Rect picFrame, dst;
    SInt32 fw, fh, dw, dh;
    Point origin;               /* OpOrigin's accumulated offset */
    Point pen;                  /* the pen, in the picture's coordinates */
    Point ovSize;
    Rect lastRect, lastRRect, lastOval, lastArc;
    SInt16 arcStart, arcAngle;
    PolyHandle lastPoly;
    RgnHandle lastRgn;
    RgnHandle savedClip;        /* the caller's clip, which the picture's narrows */
    UInt32 fgPixel, bgPixel;
} Play;

/* ------------------------------------------------------------------------
 * Reading the stream
 * ------------------------------------------------------------------------ */

static Boolean Need(Play* p, SInt32 n) {
    if (n < 0 || p->end - p->ptr < n) {
        p->bad = true;
        return false;
    }
    return true;
}

static UInt8 U8(Play* p) { return Need(p, 1) ? *p->ptr++ : 0; }
static SInt8 S8(Play* p) { return (SInt8)U8(p); }

static UInt16 U16(Play* p) {
    if (!Need(p, 2)) return 0;
    UInt16 v = (UInt16)((p->ptr[0] << 8) | p->ptr[1]);
    p->ptr += 2;
    return v;
}

static SInt16 S16(Play* p) { return (SInt16)U16(p); }

static UInt32 U32(Play* p) {
    UInt32 hi = U16(p);
    return (hi << 16) | U16(p);
}

static void Skip(Play* p, SInt32 n) {
    if (Need(p, n)) p->ptr += n;
}

static Rect ReadRect(Play* p) {
    Rect r;
    r.top = S16(p);
    r.left = S16(p);
    r.bottom = S16(p);
    r.right = S16(p);
    return r;
}

static Point ReadPoint(Play* p) {
    Point pt;
    pt.v = S16(p);
    pt.h = S16(p);
    return pt;
}

static void ReadPat(Play* p, Pattern* pat) {
    for (int i = 0; i < 8; i++) pat->pat[i] = U8(p);
}

/* ------------------------------------------------------------------------
 * From the picture's coordinates to the destination's
 * ------------------------------------------------------------------------ */

static SInt16 MapH(const Play* p, SInt32 h) {
    h -= p->origin.h;
    return (SInt16)(p->dst.left + ((h - p->picFrame.left) * p->dw) / p->fw);
}

static SInt16 MapV(const Play* p, SInt32 v) {
    v -= p->origin.v;
    return (SInt16)(p->dst.top + ((v - p->picFrame.top) * p->dh) / p->fh);
}

static Rect MapRectP(const Play* p, Rect r) {
    Rect m;
    m.left = MapH(p, r.left);
    m.top = MapV(p, r.top);
    m.right = MapH(p, r.right);
    m.bottom = MapV(p, r.bottom);
    return m;
}

static Point MapPointP(const Play* p, Point pt) {
    Point m;
    m.h = MapH(p, pt.h);
    m.v = MapV(p, pt.v);
    return m;
}

/* ------------------------------------------------------------------------
 * Regions and polygons, from their stored form
 * ------------------------------------------------------------------------ */

/* A region: rgnSize, rgnBBox, then for a region that is not a rectangle,
 * scan lines - v, then the h coordinates where inside and outside change
 * from the line above, ended by $7FFF - and a final $7FFF. Made native as
 * the union of the bands' spans, then mapped. */
static RgnHandle ReadRegion(Play* p) {
    const UInt8* start = p->ptr;
    UInt16 size = U16(p);
    Rect bbox = ReadRect(p);
    RgnHandle rgn = NewRgn();
    if (size < 10 || p->bad) {
        p->ptr = start;
        Skip(p, size < 2 ? 2 : size);
        return rgn;
    }
    if (!rgn) {
        p->ptr = start;
        Skip(p, size);
        return NULL;
    }
    if (size == 10) {
        RectRgn(rgn, &bbox);
    } else {
        enum { kMaxX = 128 };
        SInt16 xs[kMaxX] = {0};
        int nx = 0;
        const UInt8* stop = start + size;
        RgnHandle band = NewRgn();
        SInt16 prevV = 0;
        Boolean have = false;
        while (p->ptr + 2 <= stop && band) {
            SInt16 v = S16(p);
            if (v == 0x7FFF) break;
            /* The band above this line ends here */
            if (have) {
                for (int i = 0; i + 1 < nx; i += 2) {
                    SetRectRgn(band, xs[i], prevV, xs[i + 1], v);
                    UnionRgn(rgn, band, rgn);
                }
            }
            for (;;) {
                if (p->ptr + 2 > stop) break;
                SInt16 h = S16(p);
                if (h == 0x7FFF) break;
                /* An inversion: in the list, it goes; not, it joins */
                int at = -1;
                for (int i = 0; i < nx; i++) if (xs[i] == h) { at = i; break; }
                if (at >= 0) {
                    for (int i = at; i + 1 < nx; i++) xs[i] = xs[i + 1];
                    nx--;
                } else if (nx < kMaxX) {
                    int i = nx++;
                    while (i > 0 && xs[i - 1] > h) { xs[i] = xs[i - 1]; i--; }
                    xs[i] = h;
                }
            }
            prevV = v;
            have = true;
        }
        if (band) DisposeRgn(band);
        p->ptr = start;
        Skip(p, size);
    }
    OffsetRgn(rgn, (SInt16)-p->origin.h, (SInt16)-p->origin.v);
    MapRgn(rgn, &p->picFrame, &p->dst);
    return rgn;
}

/* A polygon: polySize, polyBBox, polyPoints; native and mapped */
static PolyHandle ReadPolygon(Play* p) {
    const UInt8* start = p->ptr;
    UInt16 size = U16(p);
    if (size < 10) {
        p->ptr = start;
        Skip(p, 2);
        return NULL;
    }
    Rect bbox = ReadRect(p);
    SInt16 n = (SInt16)((size - 10) / 4);
    PolyHandle poly = (PolyHandle)NewHandle((Size)(sizeof(SInt16) + sizeof(Rect) + n * sizeof(Point)));
    if (!poly) {
        p->ptr = start;
        Skip(p, size);
        return NULL;
    }
    (*poly)->polySize = (SInt16)(sizeof(SInt16) + sizeof(Rect) + n * sizeof(Point));
    (*poly)->polyBBox = MapRectP(p, bbox);
    for (SInt16 i = 0; i < n; i++) (*poly)->polyPoints[i] = MapPointP(p, ReadPoint(p));
    p->ptr = start;
    Skip(p, size);
    return poly;
}

/* ------------------------------------------------------------------------
 * The shape verbs - frame, paint, erase, invert, fill
 * ------------------------------------------------------------------------ */

static void RectVerb(int verb, const Rect* r) {
    switch (verb) {
        case 0: FrameRect(r); break;
        case 1: PaintRect(r); break;
        case 2: EraseRect(r); break;
        case 3: InvertRect(r); break;
        case 4: FillRect(r, &g_currentPort->fillPat); break;
    }
}

static void OvalVerb(int verb, const Rect* r) {
    switch (verb) {
        case 0: FrameOval(r); break;
        case 1: PaintOval(r); break;
        case 2: EraseOval(r); break;
        case 3: InvertOval(r); break;
        case 4: FillOval(r, &g_currentPort->fillPat); break;
    }
}

static void RRectVerb(int verb, const Rect* r, SInt16 ow, SInt16 oh) {
    switch (verb) {
        case 0: FrameRoundRect(r, ow, oh); break;
        case 1: PaintRoundRect(r, ow, oh); break;
        case 2: EraseRoundRect(r, ow, oh); break;
        case 3: InvertRoundRect(r, ow, oh); break;
        case 4: FillRoundRect(r, ow, oh, &g_currentPort->fillPat); break;
    }
}

static void ArcVerb(int verb, const Rect* r, SInt16 start, SInt16 arc) {
    switch (verb) {
        case 0: FrameArc(r, start, arc); break;
        case 1: PaintArc(r, start, arc); break;
        case 2: EraseArc(r, start, arc); break;
        case 3: InvertArc(r, start, arc); break;
        case 4: FillArc(r, start, arc, &g_currentPort->fillPat); break;
    }
}

static void PolyVerb(int verb, PolyHandle poly) {
    if (!poly) return;
    switch (verb) {
        case 0: FramePoly(poly); break;
        case 1: PaintPoly(poly); break;
        case 2: ErasePoly(poly); break;
        case 3: InvertPoly(poly); break;
        case 4: FillPoly(poly, &g_currentPort->fillPat); break;
    }
}

static void RgnVerb(int verb, RgnHandle rgn) {
    if (!rgn) return;
    switch (verb) {
        case 0: FrameRgn(rgn); break;
        case 1: PaintRgn(rgn); break;
        case 2: EraseRgn(rgn); break;
        case 3: InvertRgn(rgn); break;
        case 4: FillRgn(rgn, &g_currentPort->fillPat); break;
    }
}

/* ------------------------------------------------------------------------
 * Pixel images: BitsRect, PackBitsRect, DirectBitsRect and the Rgn forms
 * ------------------------------------------------------------------------ */

/* PackBits: a count byte n, then n+1 literal bytes, or 1-n copies of the
 * next byte (Inside Macintosh I-470) - or, for 16-bit pixels, of words */
static void UnpackRow(const UInt8* src, SInt32 srcLen, UInt8* dst, SInt32 dstLen, int unit) {
    SInt32 in = 0, out = 0;
    while (in < srcLen && out < dstLen) {
        SInt8 c = (SInt8)src[in++];
        if (c >= 0) {
            SInt32 n = ((SInt32)c + 1) * unit;
            for (SInt32 k = 0; k < n && in < srcLen && out < dstLen; k++) dst[out++] = src[in++];
        } else if (c != -128) {
            SInt32 n = 1 - (SInt32)c;
            if (in + unit > srcLen) break;
            for (SInt32 k = 0; k < n; k++)
                for (int u = 0; u < unit && out < dstLen; u++) dst[out++] = src[in + u];
            in += unit;
        }
    }
}

static UInt32 PixelFromRGB(UInt16 r, UInt16 g, UInt16 b) {
    return QDPlatform_RGBToPixel((UInt8)(r >> 8), (UInt8)(g >> 8), (UInt8)(b >> 8));
}

/* The image's pixels, into the port: scaled from srcRect to dstRect, by
 * the transfer mode, within the mask, the clip and what is visible */
static void PutImage(Play* p, const UInt32* image, const Boolean* ink, SInt16 imgW, SInt16 imgH,
                     const Rect* bounds, Rect srcRect, Rect dstRect, SInt16 mode, RgnHandle mask) {
    GrafPtr port = g_currentPort;
    if (!port) return;
    Rect dst = MapRectP(p, dstRect);
    SInt32 dw = dst.right - dst.left, dh = dst.bottom - dst.top;
    SInt32 sw = srcRect.right - srcRect.left, sh = srcRect.bottom - srcRect.top;
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    int op = mode & 3;
    Boolean notSrc = (mode & 4) != 0 && mode < 8;
    if (mode >= 32) op = 0;             /* arithmetic and dither modes: a copy */
    SInt16 bh = port->portBits.bounds.left, bv = port->portBits.bounds.top;
    QD_ClipBegin(port);
    for (SInt32 y = 0; y < dh; y++) {
        SInt32 ly = dst.top + y;
        SInt32 sy = srcRect.top + (y * sh) / dh - bounds->top;
        if (sy < 0 || sy >= imgH) continue;
        for (SInt32 x = 0; x < dw; x++) {
            SInt32 lx = dst.left + x;
            SInt32 sx = srcRect.left + (x * sw) / dw - bounds->left;
            if (sx < 0 || sx >= imgW) continue;
            Point lp = { (SInt16)ly, (SInt16)lx };
            if (lx < port->portRect.left || lx >= port->portRect.right ||
                ly < port->portRect.top || ly >= port->portRect.bottom) continue;
            if (mask && !PtInRgn(lp, mask)) continue;
            if (port->clipRgn) {
                Point gp = { (SInt16)(ly + bv), (SInt16)(lx + bh) };
                if (!PtInRgn(gp, port->clipRgn)) continue;
            }
            SInt32 at = sy * imgW + sx;
            Boolean s = ink[at];
            UInt32 c = image[at];
            if (notSrc) {
                s = !s;
                c = s ? p->fgPixel : p->bgPixel;
            }
            SInt32 gx = lx + bh, gy = ly + bv;
            switch (op) {
                case 0: QDPlatform_SetPixel(gx, gy, c); break;
                case 1: if (s) QDPlatform_SetPixel(gx, gy, p->fgPixel); break;
                case 2: if (s) QDPlatform_SetPixel(gx, gy, QDPlatform_GetPixel(gx, gy) ^ 0x00FFFFFF); break;
                case 3: if (s) QDPlatform_SetPixel(gx, gy, p->bgPixel); break;
            }
        }
    }
    QD_ClipEnd();
}

/* One opcode's worth of pixels. packed: the rows are PackBits-coded when
 * rowBytes is 8 or more; withRgn: a mask region follows the mode; direct:
 * DirectBitsRect, whose PixMap has a baseAddr in front and no colour table. */
static void DoBits(Play* p, Boolean packed, Boolean withRgn, Boolean direct) {
    if (direct) Skip(p, 4);                     /* baseAddr */
    UInt16 rawRowBytes = U16(p);
    Boolean pixMap = (rawRowBytes & 0x8000) != 0 || direct;
    SInt32 rowBytes = rawRowBytes & 0x3FFF;
    Rect bounds = ReadRect(p);
    UInt16 packType = 0, pixelSize = 1, cmpCount = 1;
    UInt32 ctab[256];
    int ctabSize = 0;
    ctab[0] = QDPlatform_RGBToPixel(255, 255, 255);
    ctab[1] = QDPlatform_RGBToPixel(0, 0, 0);
    if (pixMap) {
        U16(p);                                 /* pmVersion */
        packType = U16(p);
        U32(p);                                 /* packSize */
        U32(p); U32(p);                         /* hRes, vRes */
        U16(p);                                 /* pixelType */
        pixelSize = U16(p);
        cmpCount = U16(p);
        U16(p);                                 /* cmpSize */
        U32(p); U32(p); U32(p);                 /* planeBytes, pmTable, pmReserved */
        if (!direct) {
            U32(p);                             /* ctSeed */
            UInt16 flags = U16(p);
            SInt16 n = S16(p);
            for (SInt32 i = 0; i <= n && !p->bad; i++) {
                UInt16 value = U16(p);
                UInt16 r = U16(p), g = U16(p), b = U16(p);
                SInt32 index = (flags & 0x8000) ? i : value;
                if (index >= 0 && index < 256) {
                    ctab[index] = PixelFromRGB(r, g, b);
                    if (index >= ctabSize) ctabSize = (int)index + 1;
                }
            }
        }
    }
    Rect srcRect = ReadRect(p), dstRect = ReadRect(p);
    SInt16 mode = S16(p);
    RgnHandle mask = withRgn ? ReadRegion(p) : NULL;
    if (p->bad) {
        if (mask) DisposeRgn(mask);
        return;
    }

    SInt16 w = (SInt16)(bounds.right - bounds.left), h = (SInt16)(bounds.bottom - bounds.top);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || rowBytes <= 0) {
        if (mask) DisposeRgn(mask);
        p->bad = true;
        return;
    }
    /* A 32-bit row PackBits-coded component by component is cmpCount
     * planes of w bytes; otherwise rowBytes */
    SInt32 rowLen = (direct && pixelSize == 32 && packType == 4) ? (SInt32)cmpCount * w : rowBytes;
    UInt8* row = (UInt8*)NewPtr(rowLen + 16);
    UInt32* image = (UInt32*)NewPtr((Size)w * h * 4);
    Boolean* ink = (Boolean*)NewPtr((Size)w * h);
    if (!row || !image || !ink) {
        if (row) DisposePtr((Ptr)row);
        if (image) DisposePtr((Ptr)image);
        if (ink) DisposePtr((Ptr)ink);
        if (mask) DisposeRgn(mask);
        p->bad = true;
        return;
    }
    Boolean isPacked = packed && rowBytes >= 8 && packType != 1 && packType != 2;
    int unit = (direct && pixelSize == 16 && packType == 3) ? 2 : 1;
    UInt32 white = QDPlatform_RGBToPixel(255, 255, 255);
    for (SInt16 y = 0; y < h && !p->bad; y++) {
        if (isPacked) {
            SInt32 count = rowBytes > 250 ? U16(p) : U8(p);
            if (!Need(p, count)) break;
            memset(row, 0, (size_t)rowLen);
            UnpackRow(p->ptr, count, row, rowLen, unit);
            p->ptr += count;
        } else {
            SInt32 n = (direct && packType == 2) ? (SInt32)w * 3 : rowBytes;
            if (!Need(p, n)) break;
            if (direct && packType == 2) {
                for (SInt16 x = 0; x < w && 4 * x + 3 < rowLen + 16; x++) {
                    row[4 * x] = 0;
                    row[4 * x + 1] = p->ptr[3 * x];
                    row[4 * x + 2] = p->ptr[3 * x + 1];
                    row[4 * x + 3] = p->ptr[3 * x + 2];
                }
            } else {
                memcpy(row, p->ptr, (size_t)(n < rowLen ? n : rowLen));
            }
            p->ptr += n;
        }
        for (SInt16 x = 0; x < w; x++) {
            UInt32 c;
            if (!pixMap) {
                Boolean b = (row[x >> 3] & (0x80 >> (x & 7))) != 0;
                c = b ? p->fgPixel : p->bgPixel;
            } else if (direct && pixelSize == 32) {
                if (packType == 4) {
                    int o = cmpCount == 4 ? 1 : 0;
                    c = QDPlatform_RGBToPixel(row[(o + 0) * w + x], row[(o + 1) * w + x],
                                              row[(o + 2) * w + x]);
                } else {
                    c = QDPlatform_RGBToPixel(row[4 * x + 1], row[4 * x + 2], row[4 * x + 3]);
                }
            } else if (direct && pixelSize == 16) {
                UInt16 v = (UInt16)((row[2 * x] << 8) | row[2 * x + 1]);
                UInt8 r = (UInt8)(((v >> 10) & 31) * 255 / 31);
                UInt8 g = (UInt8)(((v >> 5) & 31) * 255 / 31);
                UInt8 b = (UInt8)((v & 31) * 255 / 31);
                c = QDPlatform_RGBToPixel(r, g, b);
            } else {
                int bits = pixelSize == 2 || pixelSize == 4 || pixelSize == 8 ? pixelSize : 1;
                int perByte = 8 / bits;
                UInt8 byte = row[x / perByte];
                int shift = (perByte - 1 - (x % perByte)) * bits;
                int index = (byte >> shift) & ((1 << bits) - 1);
                if (bits == 1 && ctabSize < 2) c = index ? p->fgPixel : p->bgPixel;
                else c = index < 256 ? ctab[index] : white;
            }
            image[(SInt32)y * w + x] = c;
            ink[(SInt32)y * w + x] = (c & 0x00FFFFFF) != (white & 0x00FFFFFF);
        }
    }
    if (!p->bad) PutImage(p, image, ink, w, h, &bounds, srcRect, dstRect, mode, mask);
    DisposePtr((Ptr)row);
    DisposePtr((Ptr)image);
    DisposePtr((Ptr)ink);
    if (mask) DisposeRgn(mask);
}

/* A PixPat (BkPixPat, PnPixPat, FillPixPat): its old-style pattern is used,
 * the rest stepped over */
static void DoPixPat(Play* p, Pattern* out) {
    UInt16 type = U16(p);
    ReadPat(p, out);
    if (type == 2) {                            /* dither: an RGB */
        Skip(p, 6);
    } else if (type == 1) {                     /* a full pixel pattern */
        UInt16 rowBytes = U16(p) & 0x3FFF;
        Rect b = ReadRect(p);
        Skip(p, 36);                            /* the rest of the PixMap */
        U32(p);
        U16(p);
        SInt16 n = S16(p);
        Skip(p, ((SInt32)n + 1) * 8);
        SInt16 h = (SInt16)(b.bottom - b.top);
        for (SInt16 y = 0; y < h && !p->bad; y++) {
            if (rowBytes < 8) Skip(p, rowBytes);
            else Skip(p, rowBytes > 250 ? U16(p) : U8(p));
        }
    }
}

/* ------------------------------------------------------------------------
 * Text
 * ------------------------------------------------------------------------ */

static void DoText(Play* p) {
    UInt8 n = U8(p);
    if (!Need(p, n)) return;
    Point at = MapPointP(p, p->pen);
    MoveTo(at.h, at.v);
    DrawText((const char*)p->ptr, 0, n);
    p->ptr += n;
}

/* ------------------------------------------------------------------------
 * The opcodes
 * ------------------------------------------------------------------------ */

static void SetPictureClip(Play* p, RgnHandle rgn) {
    GrafPtr port = g_currentPort;
    if (!rgn || !port || !port->clipRgn) return;
    Boolean colour = g_currentCPort && (GrafPtr)g_currentCPort == port;
    /* The clip is kept in global coordinates on a port of one bit */
    if (!colour) OffsetRgn(rgn, port->portBits.bounds.left, port->portBits.bounds.top);
    if (p->savedClip) SectRgn(rgn, p->savedClip, port->clipRgn);
    else CopyRgn(rgn, port->clipRgn);
}

static void RGBOp(Play* p, Boolean fore) {
    RGBColor c;
    c.red = U16(p);
    c.green = U16(p);
    c.blue = U16(p);
    if (g_currentCPort && (GrafPtr)g_currentCPort == g_currentPort) {
        if (fore) RGBForeColor(&c);
        else RGBBackColor(&c);
    } else {
        static const SInt32 kOld[8] = { blackColor, blueColor, greenColor, cyanColor,
                                        redColor, magentaColor, yellowColor, whiteColor };
        SInt32 old = kOld[(c.red >= 0x8000 ? 4 : 0) | (c.green >= 0x8000 ? 2 : 0) |
                          (c.blue >= 0x8000 ? 1 : 0)];
        if (fore) ForeColor(old);
        else BackColor(old);
    }
    UInt32 px = PixelFromRGB(c.red, c.green, c.blue);
    if (fore) p->fgPixel = px;
    else p->bgPixel = px;
}

/* One opcode. False when the picture ends. */
static Boolean DoOpcode(Play* p, UInt16 op) {
    GrafPtr port = g_currentPort;
    if (op >= 0x30 && op <= 0x8F) {
        int verb = op & 7;
        Boolean same = (op & 8) != 0;
        int kind = (op - 0x30) >> 4;            /* rect, rrect, oval, arc, poly, rgn */
        if (verb > 4) {                         /* reserved: data as the kind's */
            if (!same) {
                if (kind <= 2) Skip(p, 8);
                else if (kind == 3) Skip(p, 12);
                else Skip(p, U16(p) - 2);
            } else if (kind == 3) {
                Skip(p, 4);
            }
            return true;
        }
        switch (kind) {
            case 0:
                if (!same) p->lastRect = MapRectP(p, ReadRect(p));
                RectVerb(verb, &p->lastRect);
                break;
            case 1:
                if (!same) p->lastRRect = MapRectP(p, ReadRect(p));
                RRectVerb(verb, &p->lastRRect,
                          (SInt16)((p->ovSize.h * p->dw) / p->fw), (SInt16)((p->ovSize.v * p->dh) / p->fh));
                break;
            case 2:
                if (!same) p->lastOval = MapRectP(p, ReadRect(p));
                OvalVerb(verb, &p->lastOval);
                break;
            case 3:
                if (!same) p->lastArc = MapRectP(p, ReadRect(p));
                p->arcStart = S16(p);
                p->arcAngle = S16(p);
                ArcVerb(verb, &p->lastArc, p->arcStart, p->arcAngle);
                break;
            case 4:
                if (!same) {
                    if (p->lastPoly) KillPoly(p->lastPoly);
                    p->lastPoly = ReadPolygon(p);
                }
                PolyVerb(verb, p->lastPoly);
                break;
            case 5:
                if (!same) {
                    if (p->lastRgn) DisposeRgn(p->lastRgn);
                    p->lastRgn = ReadRegion(p);
                }
                RgnVerb(verb, p->lastRgn);
                break;
        }
        return true;
    }

    switch (op) {
        case 0x00: break;                                       /* NOP */
        case 0x01: {                                            /* Clip */
            RgnHandle rgn = ReadRegion(p);
            SetPictureClip(p, rgn);
            if (rgn) DisposeRgn(rgn);
            break;
        }
        case 0x02: { Pattern pat; ReadPat(p, &pat); BackPat(&pat); break; }
        case 0x03: TextFont(S16(p)); break;
        case 0x04: TextFace((Style)U8(p)); break;
        case 0x05: TextMode(S16(p)); break;
        case 0x06: if (port) port->spExtra = (Fixed)U32(p); else U32(p); break;
        case 0x07: { Point s = ReadPoint(p); PenSize(s.h, s.v); break; }
        case 0x08: PenMode(S16(p)); break;
        case 0x09: { Pattern pat; ReadPat(p, &pat); PenPat(&pat); break; }
        case 0x0A: { Pattern pat; ReadPat(p, &pat); if (port) port->fillPat = pat; break; }
        case 0x0B: p->ovSize = ReadPoint(p); break;
        case 0x0C: {                                            /* Origin: dh, dv */
            SInt16 dh = S16(p), dv = S16(p);
            p->origin.h = (SInt16)(p->origin.h + dh);
            p->origin.v = (SInt16)(p->origin.v + dv);
            break;
        }
        case 0x0D: TextSize(S16(p)); break;
        case 0x0E: { SInt32 c = (SInt32)U32(p); ForeColor(c); p->fgPixel = QDPlatform_MapQDColor(c); break; }
        case 0x0F: { SInt32 c = (SInt32)U32(p); BackColor(c); p->bgPixel = QDPlatform_MapQDColor(c); break; }
        case 0x10: Skip(p, 8); break;                           /* TxRatio */
        case 0x11: Skip(p, p->v2 ? 2 : 1); break;               /* Version */
        case 0x12: { Pattern pat; DoPixPat(p, &pat); BackPat(&pat); break; }
        case 0x13: { Pattern pat; DoPixPat(p, &pat); PenPat(&pat); break; }
        case 0x14: { Pattern pat; DoPixPat(p, &pat); if (port) port->fillPat = pat; break; }
        case 0x15: case 0x16: Skip(p, 2); break;                /* PnLocHFrac, ChExtra */
        case 0x17: case 0x18: case 0x19: break;
        case 0x1A: RGBOp(p, true); break;
        case 0x1B: RGBOp(p, false); break;
        case 0x1C: case 0x1E: break;                            /* HiliteMode, DefHilite */
        case 0x1D: case 0x1F: Skip(p, 6); break;                /* HiliteColor, OpColor */
        case 0x20: {                                            /* Line */
            Point from = ReadPoint(p), to = ReadPoint(p);
            Point a = MapPointP(p, from), b = MapPointP(p, to);
            MoveTo(a.h, a.v);
            LineTo(b.h, b.v);
            p->pen = to;
            break;
        }
        case 0x21: {                                            /* LineFrom */
            Point to = ReadPoint(p);
            Point a = MapPointP(p, p->pen), b = MapPointP(p, to);
            MoveTo(a.h, a.v);
            LineTo(b.h, b.v);
            p->pen = to;
            break;
        }
        case 0x22: {                                            /* ShortLine */
            Point from = ReadPoint(p);
            SInt8 dh = S8(p), dv = S8(p);
            Point to = { (SInt16)(from.v + dv), (SInt16)(from.h + dh) };
            Point a = MapPointP(p, from), b = MapPointP(p, to);
            MoveTo(a.h, a.v);
            LineTo(b.h, b.v);
            p->pen = to;
            break;
        }
        case 0x23: {                                            /* ShortLineFrom */
            SInt8 dh = S8(p), dv = S8(p);
            Point to = { (SInt16)(p->pen.v + dv), (SInt16)(p->pen.h + dh) };
            Point a = MapPointP(p, p->pen), b = MapPointP(p, to);
            MoveTo(a.h, a.v);
            LineTo(b.h, b.v);
            p->pen = to;
            break;
        }
        case 0x28: p->pen = ReadPoint(p); DoText(p); break;     /* LongText */
        case 0x29: p->pen.h = (SInt16)(p->pen.h + U8(p)); DoText(p); break;    /* DHText */
        case 0x2A: p->pen.v = (SInt16)(p->pen.v + U8(p)); DoText(p); break;    /* DVText */
        case 0x2B: {                                            /* DHDVText */
            p->pen.h = (SInt16)(p->pen.h + U8(p));
            p->pen.v = (SInt16)(p->pen.v + U8(p));
            DoText(p);
            break;
        }
        case 0x90: DoBits(p, false, false, false); break;      /* BitsRect */
        case 0x91: DoBits(p, false, true, false); break;       /* BitsRgn */
        case 0x98: DoBits(p, true, false, false); break;       /* PackBitsRect */
        case 0x99: DoBits(p, true, true, false); break;        /* PackBitsRgn */
        case 0x9A: DoBits(p, true, false, true); break;        /* DirectBitsRect */
        case 0x9B: DoBits(p, true, true, true); break;         /* DirectBitsRgn */
        case 0xA0: Skip(p, 2); break;                           /* ShortComment */
        case 0xA1: Skip(p, 2); Skip(p, U16(p)); break;          /* LongComment */
        case 0xFF: return false;                                /* OpEndPic */
        case 0x0C00: Skip(p, 24); break;                        /* HeaderOp */
        default:
            /* Reserved opcodes: the sizes appendix A gives them */
            if ((op >= 0x24 && op <= 0x27) || (op >= 0x2C && op <= 0x2F) ||
                (op >= 0x92 && op <= 0x97) || (op >= 0x9C && op <= 0x9F) ||
                (op >= 0xA2 && op <= 0xAF)) {
                Skip(p, U16(p));
            } else if (op >= 0xB0 && op <= 0xCF) {
                /* no data */
            } else if (op >= 0xD0 && op <= 0xFE) {
                Skip(p, (SInt32)U32(p));
            } else if (op >= 0x0100 && op <= 0x7FFF) {
                Skip(p, (op >> 8) * 2);
            } else if (op >= 0x8000 && op <= 0x80FF) {
                /* no data */
            } else if (op >= 0x8100) {
                Skip(p, (SInt32)U32(p));
            } else {
                p->bad = true;
            }
            break;
    }
    return true;
}

/*
 * DrawPicture - the picture into the current port, picFrame onto dstRect
 */
void DrawPicture(PicHandle myPicture, const Rect* dstRect) {
    GrafPtr port = g_currentPort;
    if (!port || !myPicture || !dstRect || !*myPicture) return;
    Size size = GetHandleSize((Handle)myPicture);
    if (size < 11) return;

    Play p;
    memset(&p, 0, sizeof(p));
    p.base = (const UInt8*)*myPicture;
    p.ptr = p.base;
    /* picSize is the low word only for a picture over 32K; the handle says */
    p.end = p.base + size;
    U16(&p);
    p.picFrame = ReadRect(&p);
    p.dst = *dstRect;
    p.fw = p.picFrame.right - p.picFrame.left;
    p.fh = p.picFrame.bottom - p.picFrame.top;
    p.dw = p.dst.right - p.dst.left;
    p.dh = p.dst.bottom - p.dst.top;
    if (p.fw <= 0 || p.fh <= 0 || p.dw <= 0 || p.dh <= 0) return;
    if (p.end - p.ptr >= 4 && p.ptr[0] == 0x00 && p.ptr[1] == 0x11 &&
        p.ptr[2] == 0x02 && p.ptr[3] == 0xFF) {
        p.v2 = true;
        p.ptr += 4;
    } else if (p.end - p.ptr >= 2 && p.ptr[0] == 0x11 && p.ptr[1] == 0x01) {
        p.ptr += 2;
    }

    /* The caller's drawing state, put back afterwards */
    PenState pen;
    GetPenState(&pen);
    SInt16 txFont = port->txFont, txSize = port->txSize, txMode = port->txMode;
    Style txFace = port->txFace;
    Fixed spExtra = port->spExtra;
    SInt32 fg = port->fgColor, bk = port->bkColor;
    Pattern bkPat = port->bkPat, fillPat = port->fillPat;
    p.savedClip = NewRgn();
    if (p.savedClip && port->clipRgn) CopyRgn(port->clipRgn, p.savedClip);

    /* A picture states what differs from a new port's drawing state, so
     * it is played from that state, not from whatever the caller had */
    PenNormal();
    TextFont(0);
    TextFace(0);
    TextSize(0);
    TextMode(srcOr);
    port->spExtra = 0;
    ForeColor(blackColor);
    BackColor(whiteColor);
    memset(&port->fillPat, 0xFF, sizeof(port->fillPat));
    memset(&port->bkPat, 0x00, sizeof(port->bkPat));
    p.fgPixel = QDPlatform_MapQDColor(blackColor);
    p.bgPixel = QDPlatform_MapQDColor(whiteColor);
    while (!p.bad && p.ptr < p.end) {
        UInt16 op;
        if (p.v2) {
            if ((p.ptr - p.base) & 1) p.ptr++;
            op = U16(&p);
        } else {
            op = U8(&p);
        }
        if (p.bad || !DoOpcode(&p, op)) break;
    }

    if (p.lastPoly) KillPoly(p.lastPoly);
    if (p.lastRgn) DisposeRgn(p.lastRgn);
    SetPenState(&pen);
    TextFont(txFont);
    TextSize(txSize);
    TextMode(txMode);
    TextFace(txFace);
    port->spExtra = spExtra;
    ForeColor(fg);
    BackColor(bk);
    port->bkPat = bkPat;
    port->fillPat = fillPat;
    if (p.savedClip) {
        if (port->clipRgn) CopyRgn(p.savedClip, port->clipRgn);
        DisposeRgn(p.savedClip);
    }
}

/* A picture from the resource file: 'PICT' theID (Inside Macintosh:
 * Imaging With QuickDraw, 7-30) */
PicHandle GetPicture(SInt16 picID) {
    extern Handle GetResource(ResType theType, SInt16 theID);
    return (PicHandle)GetResource('PICT', picID);
}

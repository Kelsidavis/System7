/*
 * MacPaint_Tools.c - the page and everything that changes it
 *
 * The page is one bitmap. A gesture that shows something while the button is
 * held - a shape being pulled out, a selection being dragged, text being
 * typed - takes a snapshot of the page when it starts and, at each step, puts
 * the snapshot back and draws the step on it. The window draws whatever part
 * of the page a change touched (Page_TakeDirty), so a tool never draws on the
 * screen itself.
 *
 * Undo is one level, as in MacPaint: the page before the last change, and
 * Undo swaps the two, so a second Undo puts the change back.
 */

#include <string.h>

#include "MacPaintInternal.h"
#include "EventManager/EventManager.h"
#include "chicago_font.h"

extern UInt32 TickCount(void);

/* MacPaint's patterns, as its palette shows them */
const UInt8 kPatterns[kPatternCount][8] = {
    { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF }, { 0xDD,0xFF,0x77,0xFF,0xDD,0xFF,0x77,0xFF },
    { 0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77 }, { 0xAA,0xFF,0xAA,0xFF,0xAA,0xFF,0xAA,0xFF },
    { 0x55,0xFF,0x55,0xFF,0x55,0xFF,0x55,0xFF }, { 0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA },
    { 0xEE,0xDD,0xBB,0x77,0xEE,0xDD,0xBB,0x77 }, { 0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88 },
    { 0xB1,0x30,0x03,0x1B,0xD8,0xC0,0x0C,0x8D }, { 0x80,0x10,0x02,0x20,0x01,0x08,0x40,0x04 },
    { 0xFF,0x88,0x88,0x88,0xFF,0x88,0x88,0x88 }, { 0xFF,0x80,0x80,0x80,0xFF,0x08,0x08,0x08 },
    { 0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, { 0x80,0x40,0x20,0x00,0x02,0x04,0x08,0x00 },
    { 0x82,0x44,0x39,0x44,0x82,0x01,0x01,0x01 }, { 0xF8,0x74,0x22,0x47,0x8F,0x17,0x22,0x71 },
    { 0x55,0xA0,0x40,0x40,0x55,0x0A,0x04,0x04 }, { 0x20,0x50,0x88,0x88,0x88,0x88,0x05,0x02 },
    { 0xBF,0x00,0xBF,0xBF,0xB0,0xB0,0xB0,0xB0 }, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
    { 0x80,0x00,0x08,0x00,0x80,0x00,0x08,0x00 }, { 0x88,0x00,0x22,0x00,0x88,0x00,0x22,0x00 },
    { 0x88,0x22,0x88,0x22,0x88,0x22,0x88,0x22 }, { 0xAA,0x00,0xAA,0x00,0xAA,0x00,0xAA,0x00 },
    { 0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00 }, { 0x11,0x22,0x44,0x88,0x11,0x22,0x44,0x88 },
    { 0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,0x00 }, { 0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80 },
    { 0xAA,0x00,0x80,0x00,0x88,0x00,0x80,0x00 }, { 0xFF,0x80,0x80,0x80,0x80,0x80,0x80,0x80 },
    { 0x08,0x1C,0x22,0xC1,0x80,0x01,0x02,0x04 }, { 0x88,0x14,0x22,0x41,0x88,0x00,0xAA,0x00 },
    { 0x40,0xA0,0x00,0x00,0x04,0x0A,0x00,0x00 }, { 0x03,0x84,0x48,0x30,0x0C,0x02,0x01,0x01 },
    { 0x80,0x80,0x41,0x3E,0x08,0x08,0x14,0xE3 }, { 0x10,0x20,0x54,0xAA,0xFF,0x02,0x04,0x08 },
    { 0x77,0x89,0x8F,0x8F,0x77,0x98,0xF8,0xF8 }, { 0x00,0x08,0x14,0x2A,0x55,0x2A,0x14,0x08 },
};

const int kLineWidths[kLineWidthCount] = { 1, 2, 3, 5, 8 };

PaintState gPaint = { kToolPencil, 0, 0, false, false, 0, 0 };

UInt8 gPage[kPageBytes];
static UInt8 gSnapshot[kPageBytes];     /* the page when the gesture began */
static UInt8 gUndo[kPageBytes];
static Boolean gUndoValid = false;
static UInt8 gTemp[kPageBytes];         /* scratch: fill region, the page under a move */
static UInt8 gSelMask[kPageBytes];      /* the selection, a bit per page pixel */
static UInt8 gMoveBits[kPageBytes];     /* the page as a move started */
static UInt8 gClipBits[kPageBytes];     /* the clipboard, at the page's origin */
static UInt8 gClipMask[kPageBytes];

/* ------------------------------------------------------------------------
 * Bits
 * ------------------------------------------------------------------------ */

#define BIT(x) ((UInt8)(0x80 >> ((x) & 7)))

static inline Boolean InPage(int x, int y) {
    return (unsigned)x < kPageW && (unsigned)y < kPageH;
}
static inline Boolean GetBit(const UInt8* b, int x, int y) {
    return (b[y * kPageRowBytes + (x >> 3)] & BIT(x)) != 0;
}
static inline void SetBit(UInt8* b, int x, int y, Boolean on) {
    UInt8* p = &b[y * kPageRowBytes + (x >> 3)];
    if (on) *p |= BIT(x); else *p &= (UInt8)~BIT(x);
}

Boolean Page_IsBlack(int x, int y) {
    return InPage(x, y) && GetBit(gPage, x, y);
}

/* ------------------------------------------------------------------------
 * What changed
 * ------------------------------------------------------------------------ */

static Rect gDirty;         /* changed and not yet drawn */
static Rect gSinceSnap;     /* drawn since the snapshot */

static Boolean RectEmpty(const Rect* r) {
    return r->right <= r->left || r->bottom <= r->top;
}
static void Grow(Rect* r, int l, int t, int rr, int b) {
    if (RectEmpty(r)) {
        SetRect(r, l, t, rr, b);
        return;
    }
    if (l < r->left) r->left = l;
    if (t < r->top) r->top = t;
    if (rr > r->right) r->right = rr;
    if (b > r->bottom) r->bottom = b;
}

void Page_MarkDirty(const Rect* r) {
    if (!RectEmpty(r)) Grow(&gDirty, r->left, r->top, r->right, r->bottom);
}

Boolean Page_TakeDirty(Rect* r) {
    if (RectEmpty(&gDirty)) return false;
    *r = gDirty;
    SetRect(&gDirty, 0, 0, 0, 0);
    return true;
}

static void MarkAll(void) {
    Rect all = { 0, 0, kPageH, kPageW };
    Page_MarkDirty(&all);
}

static void Put(int x, int y, Boolean black) {
    if (!InPage(x, y)) return;
    SetBit(gPage, x, y, black);
    Grow(&gDirty, x, y, x + 1, y + 1);
    Grow(&gSinceSnap, x, y, x + 1, y + 1);
}

static Boolean PatBit(int x, int y) {
    return (kPatterns[gPaint.pattern][y & 7] & BIT(x)) != 0;
}
static void PutPat(int x, int y) {
    Put(x, y, PatBit(x, y));
}

/* ------------------------------------------------------------------------
 * Snapshot and undo
 * ------------------------------------------------------------------------ */

static void Snap(void) {
    memcpy(gSnapshot, gPage, kPageBytes);
    SetRect(&gSinceSnap, 0, 0, 0, 0);
}

/* Put back what was drawn since the snapshot */
static void Restore(void) {
    if (RectEmpty(&gSinceSnap)) return;
    int top = gSinceSnap.top < 0 ? 0 : gSinceSnap.top;
    int bottom = gSinceSnap.bottom > kPageH ? kPageH : gSinceSnap.bottom;
    if (bottom > top) {
        memcpy(&gPage[top * kPageRowBytes], &gSnapshot[top * kPageRowBytes],
               (size_t)(bottom - top) * kPageRowBytes);
    }
    Page_MarkDirty(&gSinceSnap);
    SetRect(&gSinceSnap, 0, 0, 0, 0);
}

static void SaveUndo(void) {
    memcpy(gUndo, gPage, kPageBytes);
    gUndoValid = true;
}

Boolean Edit_CanUndo(void) {
    return gUndoValid;
}

/* ------------------------------------------------------------------------
 * Pens
 * ------------------------------------------------------------------------ */

typedef void (*PlotFn)(int x, int y);

static void DrawSeg(int x0, int y0, int x1, int y1, PlotFn plot) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static int PenW(void) {
    return kLineWidths[gPaint.lineWidth];
}

/* Lines and borders: a square pen of the line width, in black */
static void PenAt(int x, int y) {
    int w = PenW();
    for (int dy = 0; dy < w; dy++)
        for (int dx = 0; dx < w; dx++)
            Put(x + dx, y + dy, true);
}

/* The paintbrush: a round brush, painting the pattern */
static const UInt8 kBrush[6] = { 0x78, 0xFC, 0xFC, 0xFC, 0xFC, 0x78 };
static void BrushAt(int x, int y) {
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 6; c++)
            if (kBrush[r] & (0x80 >> c)) PutPat(x - 3 + c, y - 3 + r);
}

/* The eraser: a 16-pixel square on the screen, so smaller on the page in
 * FatBits */
static void EraserAt(int x, int y) {
    int s = 16 / Draw_Zoom();
    if (s < 1) s = 1;
    for (int dy = 0; dy < s; dy++)
        for (int dx = 0; dx < s; dx++)
            Put(x - s / 2 + dx, y - s / 2 + dy, false);
}

/* The pencil draws white where it starts on black, black otherwise */
static Boolean gPencilBlack;
static void PencilAt(int x, int y) {
    Put(x, y, gPencilBlack);
}

/* The airbrush: a few dots of the pattern each tick, within a circle */
static UInt32 gSeed = 12345;
static int Rand(int n) {
    gSeed = gSeed * 1103515245u + 12345u;
    return (int)((gSeed >> 16) % (UInt32)n);
}
static UInt32 gLastSprayTick;
static void SprayAt(int x, int y) {
    UInt32 now = TickCount();
    if (now == gLastSprayTick) return;
    gLastSprayTick = now;
    for (int i = 0; i < 10; i++) {
        int dx = Rand(17) - 8, dy = Rand(17) - 8;
        if (dx * dx + dy * dy <= 64) PutPat(x + dx, y + dy);
    }
}

/* Selection outlines while they are being drawn: the snapshot inverted, so
 * drawing over a point twice does no harm */
static void InvertAt(int x, int y) {
    if (InPage(x, y)) Put(x, y, !GetBit(gSnapshot, x, y));
}
static void DottedAt(int x, int y) {
    if (((x + y) & 3) < 2) InvertAt(x, y);
}

/* ------------------------------------------------------------------------
 * Shapes
 *
 * Each shape is a test of whether a pixel is inside it. Its border is what is
 * inside the shape and not inside the shape inset by the line width; a filled
 * shape paints the inside with the pattern.
 * ------------------------------------------------------------------------ */

enum { kShapeRect, kShapeRRect, kShapeOval };

static Boolean InsideRect(const Rect* r, int x, int y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

static Boolean InsideOval(const Rect* r, int x, int y) {
    if (!InsideRect(r, x, y)) return false;
    long long w = r->right - r->left, h = r->bottom - r->top;
    long long dx = 2LL * x + 1 - (r->left + r->right);
    long long dy = 2LL * y + 1 - (r->top + r->bottom);
    return dx * dx * h * h + dy * dy * w * w <= w * w * h * h;
}

static Boolean InsideRRect(const Rect* r, int radius, int x, int y) {
    if (!InsideRect(r, x, y)) return false;
    int w = r->right - r->left, h = r->bottom - r->top;
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;
    if (radius <= 0) return true;
    if (x >= r->left + radius && x < r->right - radius) return true;
    if (y >= r->top + radius && y < r->bottom - radius) return true;
    Rect corner;
    corner.left = x < r->left + radius ? r->left : r->right - 2 * radius;
    corner.top = y < r->top + radius ? r->top : r->bottom - 2 * radius;
    corner.right = corner.left + 2 * radius;
    corner.bottom = corner.top + 2 * radius;
    return InsideOval(&corner, x, y);
}

static Boolean InsideShape(int shape, const Rect* r, int radius, int x, int y) {
    switch (shape) {
        case kShapeOval:  return InsideOval(r, x, y);
        case kShapeRRect: return InsideRRect(r, radius, x, y);
        default:          return InsideRect(r, x, y);
    }
}

static void PaintShape(int shape, const Rect* r, Boolean filled) {
    if (RectEmpty(r)) return;
    int w = PenW();
    int radius = 8;
    Rect in = *r;
    in.left += w; in.top += w; in.right -= w; in.bottom -= w;
    int inRadius = radius - w > 0 ? radius - w : 0;

    int top = r->top < 0 ? 0 : r->top, bottom = r->bottom > kPageH ? kPageH : r->bottom;
    int left = r->left < 0 ? 0 : r->left, right = r->right > kPageW ? kPageW : r->right;
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            if (!InsideShape(shape, r, radius, x, y)) continue;
            Boolean inner = !RectEmpty(&in) && InsideShape(shape, &in, inRadius, x, y);
            if (!inner) Put(x, y, true);
            else if (filled) PutPat(x, y);
        }
    }
}

/* The rectangle a drag from (x0,y0) to (x1,y1) covers, both ends included */
static void DragRect(int x0, int y0, int x1, int y1, Rect* r) {
    r->left = x0 < x1 ? x0 : x1;
    r->right = (x0 < x1 ? x1 : x0) + 1;
    r->top = y0 < y1 ? y0 : y1;
    r->bottom = (y0 < y1 ? y1 : y0) + 1;
}

/* ------------------------------------------------------------------------
 * Polygons: the freeform and polygon tools, and the lasso
 * ------------------------------------------------------------------------ */

#define kMaxPoints 2048
static short gPtX[kMaxPoints], gPtY[kMaxPoints];
static int gPtCount;

static void AddPoint(int x, int y) {
    if (gPtCount > 0 && gPtX[gPtCount - 1] == x && gPtY[gPtCount - 1] == y) return;
    if (gPtCount < kMaxPoints) {
        gPtX[gPtCount] = (short)x;
        gPtY[gPtCount] = (short)y;
        gPtCount++;
    }
}

typedef void (*SpanFn)(int y, int x0, int x1);   /* x0 <= x < x1 */

/* Even-odd scan of the closed polygon through the points */
static void ScanPolygon(SpanFn span) {
    static int xs[kMaxPoints];
    if (gPtCount < 3) return;
    int top = gPtY[0], bottom = gPtY[0];
    for (int i = 1; i < gPtCount; i++) {
        if (gPtY[i] < top) top = gPtY[i];
        if (gPtY[i] > bottom) bottom = gPtY[i];
    }
    if (top < 0) top = 0;
    if (bottom >= kPageH) bottom = kPageH - 1;

    for (int y = top; y <= bottom; y++) {
        int n = 0;
        int Y = 2 * y + 1;                /* through the middle of the row */
        for (int i = 0; i < gPtCount; i++) {
            int j = (i + 1) % gPtCount;
            int a = 2 * gPtY[i], b = 2 * gPtY[j];
            if ((a <= Y) == (b <= Y)) continue;
            xs[n++] = gPtX[i] + (gPtX[j] - gPtX[i]) * (Y - a) / (b - a);
        }
        for (int i = 1; i < n; i++) {      /* few crossings: insertion sort */
            int v = xs[i], k = i - 1;
            while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; }
            xs[k + 1] = v;
        }
        for (int i = 0; i + 1 < n; i += 2) span(y, xs[i], xs[i + 1] + 1);
    }
}

static void PatternSpan(int y, int x0, int x1) {
    for (int x = x0; x < x1; x++) PutPat(x, y);
}

static void OutlinePoints(Boolean closed) {
    for (int i = 0; i + 1 < gPtCount; i++) DrawSeg(gPtX[i], gPtY[i], gPtX[i + 1], gPtY[i + 1], PenAt);
    if (closed && gPtCount > 1) DrawSeg(gPtX[gPtCount - 1], gPtY[gPtCount - 1], gPtX[0], gPtY[0], PenAt);
    if (gPtCount == 1) PenAt(gPtX[0], gPtY[0]);
}

/* ------------------------------------------------------------------------
 * The selection
 *
 * A mask with a bit for each page pixel. The selection rectangle selects
 * everything in it, white included, and moves it as a block; the lasso
 * selects only the black pixels inside its outline, so what it moves is
 * transparent - as in MacPaint.
 * ------------------------------------------------------------------------ */

static Boolean gSelActive;
static Rect gSelRect;           /* bounds of the mask */
static Boolean gSelOpaque;
static int gSelDX, gSelDY;      /* how far a move under way has taken it */

static void SelMarkDirty(void) {
    Rect r = gSelRect;
    OffsetRect(&r, gSelDX, gSelDY);
    r.left--; r.top--; r.right++; r.bottom++;
    Page_MarkDirty(&r);
}

static void Sel_Drop(void) {
    if (!gSelActive) return;
    SelMarkDirty();
    for (int y = gSelRect.top; y < gSelRect.bottom; y++) {
        memset(&gSelMask[y * kPageRowBytes], 0, kPageRowBytes);
    }
    gSelActive = false;
    gSelDX = gSelDY = 0;
}

static void ClipToPage(Rect* r) {
    if (r->left < 0) r->left = 0;
    if (r->top < 0) r->top = 0;
    if (r->right > kPageW) r->right = kPageW;
    if (r->bottom > kPageH) r->bottom = kPageH;
}

static void Sel_SetRect(Rect r) {
    Sel_Drop();
    ClipToPage(&r);
    if (RectEmpty(&r)) return;
    for (int y = r.top; y < r.bottom; y++)
        for (int x = r.left; x < r.right; x++)
            SetBit(gSelMask, x, y, true);
    gSelRect = r;
    gSelOpaque = true;
    gSelActive = true;
    SelMarkDirty();
}

/* After the mask changed: its bounds, and whether anything is left */
static void Sel_Bound(void) {
    Rect b = { 0, 0, 0, 0 };
    for (int y = 0; y < kPageH; y++) {
        const UInt8* row = &gSelMask[y * kPageRowBytes];
        for (int i = 0; i < kPageRowBytes; i++) {
            if (!row[i]) continue;
            for (int k = 0; k < 8; k++)
                if (row[i] & (0x80 >> k)) Grow(&b, i * 8 + k, y, i * 8 + k + 1, y + 1);
        }
    }
    gSelRect = b;
    gSelActive = !RectEmpty(&b);
    if (gSelActive) SelMarkDirty();
}

static Boolean Sel_Contains(int x, int y) {
    return gSelActive && InPage(x, y) && GetBit(gSelMask, x, y);
}

Boolean Edit_HasSelection(void) {
    return gSelActive;
}

Boolean Sel_IsEdge(int x, int y) {
    if (!gSelActive) return false;
    x -= gSelDX;
    y -= gSelDY;
    if (!InPage(x, y) || !GetBit(gSelMask, x, y)) return false;
    return x == 0 || y == 0 || x == kPageW - 1 || y == kPageH - 1 ||
           !GetBit(gSelMask, x - 1, y) || !GetBit(gSelMask, x + 1, y) ||
           !GetBit(gSelMask, x, y - 1) || !GetBit(gSelMask, x, y + 1);
}

/* A move: the page under the selection is gTemp; each step puts that back
 * and stamps the selection at its new place. Option leaves the original. */
static Boolean gMoving;
static int gMoveX0, gMoveY0;

static void MoveBegin(int x, int y, Boolean copy) {
    SaveUndo();
    memcpy(gMoveBits, gPage, kPageBytes);
    memcpy(gTemp, gPage, kPageBytes);
    if (!copy) {
        for (int py = gSelRect.top; py < gSelRect.bottom; py++)
            for (int px = gSelRect.left; px < gSelRect.right; px++)
                if (GetBit(gSelMask, px, py)) SetBit(gTemp, px, py, false);
    }
    gMoving = true;
    gMoveX0 = x;
    gMoveY0 = y;
    gSelDX = gSelDY = 0;
}

static void MoveTo_(int x, int y) {
    int dx = x - gMoveX0, dy = y - gMoveY0;
    if (dx == gSelDX && dy == gSelDY) return;
    SelMarkDirty();                                 /* where it was */

    /* The page under the selection, then the selection on it */
    Rect span = gSelRect;
    OffsetRect(&span, gSelDX, gSelDY);
    Page_MarkDirty(&span);
    Page_MarkDirty(&gSelRect);
    int top = span.top < gSelRect.top ? span.top : gSelRect.top;
    int bottom = span.bottom > gSelRect.bottom ? span.bottom : gSelRect.bottom;
    if (top < 0) top = 0;
    if (bottom > kPageH) bottom = kPageH;
    if (bottom > top)
        memcpy(&gPage[top * kPageRowBytes], &gTemp[top * kPageRowBytes],
               (size_t)(bottom - top) * kPageRowBytes);

    gSelDX = dx;
    gSelDY = dy;
    for (int py = gSelRect.top; py < gSelRect.bottom; py++) {
        for (int px = gSelRect.left; px < gSelRect.right; px++) {
            if (!GetBit(gSelMask, px, py)) continue;
            Boolean black = GetBit(gMoveBits, px, py);
            if (gSelOpaque || black) Put(px + dx, py + dy, black);
        }
    }
    SelMarkDirty();                                 /* where it is */
}

static void MoveEnd(void) {
    gMoving = false;
    if (gSelDX == 0 && gSelDY == 0) return;
    /* The mask follows the pixels; gSnapshot is free to build it in */
    memset(gSnapshot, 0, kPageBytes);
    for (int py = gSelRect.top; py < gSelRect.bottom; py++)
        for (int px = gSelRect.left; px < gSelRect.right; px++)
            if (GetBit(gSelMask, px, py) && InPage(px + gSelDX, py + gSelDY))
                SetBit(gSnapshot, px + gSelDX, py + gSelDY, true);
    memcpy(gSelMask, gSnapshot, kPageBytes);
    gSelDX = gSelDY = 0;
    Sel_Bound();
    MacPaint_SetDirty();
}

static void LassoSpan(int y, int x0, int x1) {
    if (x0 < 0) x0 = 0;
    if (x1 > kPageW) x1 = kPageW;
    for (int x = x0; x < x1; x++)
        if (GetBit(gPage, x, y)) SetBit(gSelMask, x, y, true);
}

/* Map each selected pixel somewhere else: flips */
typedef void (*MapFn)(int x, int y, int* nx, int* ny);

static void Sel_Transform(MapFn map) {
    if (!gSelActive) return;
    Tool_Finish();
    SaveUndo();
    memcpy(gTemp, gPage, kPageBytes);
    memcpy(gSnapshot, gSelMask, kPageBytes);
    Rect old = gSelRect;
    Sel_Drop();
    for (int y = old.top; y < old.bottom; y++)
        for (int x = old.left; x < old.right; x++)
            if (GetBit(gSnapshot, x, y)) Put(x, y, false);
    for (int y = old.top; y < old.bottom; y++) {
        for (int x = old.left; x < old.right; x++) {
            if (!GetBit(gSnapshot, x, y)) continue;
            int nx, ny;
            map(x, y, &nx, &ny);
            if (!InPage(nx, ny)) continue;
            Boolean black = GetBit(gTemp, x, y);
            if (gSelOpaque || black) Put(nx, ny, black);
            SetBit(gSelMask, nx, ny, true);
        }
    }
    Sel_Bound();
    MacPaint_SetDirty();
}

static Rect gMapRect;
static void MapFlipH(int x, int y, int* nx, int* ny) {
    *nx = gMapRect.left + gMapRect.right - 1 - x;
    *ny = y;
}
static void MapFlipV(int x, int y, int* nx, int* ny) {
    *nx = x;
    *ny = gMapRect.top + gMapRect.bottom - 1 - y;
}

void Edit_FlipHorizontal(void) {
    gMapRect = gSelRect;
    Sel_Transform(MapFlipH);
}

void Edit_FlipVertical(void) {
    gMapRect = gSelRect;
    Sel_Transform(MapFlipV);
}

/* ------------------------------------------------------------------------
 * Text: typed straight onto the page in Chicago, from where it was clicked
 * ------------------------------------------------------------------------ */

static Boolean gTextActive;
static int gTextX, gTextY;      /* top left of the first line */
static char gText[512];
static int gTextLen;

enum { kTextLineH = CHICAGO_HEIGHT + 1 };

static void CaretDirty(void) {
    int x, top, h;
    if (Text_Caret(&x, &top, &h)) {
        Rect r = { (short)top, (short)(x - 1), (short)(top + h), (short)(x + 2) };
        Page_MarkDirty(&r);
    }
}

/* How far a character moves the pen: the font's own advance figures are not
 * to be trusted (an 'i' advanced 8), so this spaces by the ink width, as the
 * Font Manager does */
static int Advance(char ch) {
    if (ch < 32 || ch > 126) return 0;
    int w = chicago_ascii[ch - 32].bit_width + 2;
    return ch == ' ' ? w + 3 : w;
}

static int GlyphAt(char ch, int penX, int top) {
    if (ch < 32 || ch > 126) return penX;
    const ChicagoCharInfo* info = &chicago_ascii[ch - 32];
    for (int row = 0; row < CHICAGO_HEIGHT; row++) {
        const uint8_t* strike = chicago_bitmap + row * CHICAGO_ROW_BYTES;
        for (int col = 0; col < info->bit_width; col++) {
            int bit = info->bit_start + col;
            if ((strike[bit >> 3] >> (7 - (bit & 7))) & 1)
                Put(penX + col, top + row, true);
        }
    }
    return penX + Advance(ch);
}

static void TextRender(void) {
    Restore();
    int x = gTextX, top = gTextY;
    for (int i = 0; i < gTextLen; i++) {
        if (gText[i] == '\r') {
            x = gTextX;
            top += kTextLineH;
        } else {
            x = GlyphAt(gText[i], x, top);
        }
    }
}

Boolean Text_Caret(int* x, int* top, int* height) {
    if (!gTextActive) return false;
    int cx = gTextX, ct = gTextY;
    for (int i = 0; i < gTextLen; i++) {
        if (gText[i] == '\r') {
            cx = gTextX;
            ct += kTextLineH;
        } else if (gText[i] >= 32 && gText[i] <= 126) {
            cx += Advance(gText[i]);
        }
    }
    *x = cx;
    *top = ct;
    *height = CHICAGO_HEIGHT;
    return true;
}

static void TextEnd(void) {
    if (!gTextActive) return;
    CaretDirty();
    gTextActive = false;
}

Boolean Tool_Key(unsigned char ch) {
    if (!gTextActive) return false;
    CaretDirty();
    if (ch == 8 || ch == 127) {                 /* backspace */
        if (gTextLen > 0) gTextLen--;
    } else if (ch == '\r' || ch == 3) {         /* return, enter */
        if (gTextLen < (int)sizeof(gText) - 1) gText[gTextLen++] = '\r';
    } else if (ch >= 32 && ch <= 126) {
        if (gTextLen < (int)sizeof(gText) - 1) gText[gTextLen++] = (char)ch;
    } else {
        return true;
    }
    TextRender();
    CaretDirty();
    MacPaint_SetDirty();
    return true;
}

/* ------------------------------------------------------------------------
 * The paint bucket: the region of like pixels around the click, by rows,
 * gathered in gTemp first so a pattern that contains the region's own
 * colour does not leak the fill into the rest of the page
 * ------------------------------------------------------------------------ */

static void BucketFill(int x, int y) {
    static short stack[2 * 4096];
    if (!InPage(x, y)) return;
    Boolean target = GetBit(gPage, x, y);
    memset(gTemp, 0, kPageBytes);
    Rect box = { 0, 0, 0, 0 };
    int n = 0;
    stack[n++] = (short)x;
    stack[n++] = (short)y;
#define WANT(px, py) (GetBit(gPage, px, py) == target && !GetBit(gTemp, px, py))
    while (n > 0) {
        int sy = stack[--n], sx = stack[--n];
        if (!WANT(sx, sy)) continue;
        int l = sx, r = sx;
        while (l > 0 && WANT(l - 1, sy)) l--;
        while (r < kPageW - 1 && WANT(r + 1, sy)) r++;
        for (int px = l; px <= r; px++) SetBit(gTemp, px, sy, true);
        Grow(&box, l, sy, r + 1, sy + 1);
        for (int ny = sy - 1; ny <= sy + 1; ny += 2) {
            if (ny < 0 || ny >= kPageH) continue;
            Boolean inRun = false;
            for (int px = l; px <= r; px++) {
                if (WANT(px, ny)) {
                    if (!inRun && n < (int)(sizeof(stack) / sizeof(stack[0])) - 2) {
                        stack[n++] = (short)px;
                        stack[n++] = (short)ny;
                    }
                    inRun = true;
                } else {
                    inRun = false;
                }
            }
        }
    }
#undef WANT
    for (int py = box.top; py < box.bottom; py++)
        for (int px = box.left; px < box.right; px++)
            if (GetBit(gTemp, px, py)) PutPat(px, py);
}

/* ------------------------------------------------------------------------
 * Gestures
 * ------------------------------------------------------------------------ */

static int gStartX, gStartY, gLastX, gLastY;
static Boolean gPolyActive;
static Boolean gGesture;        /* the button went down on the page */

static int SnapToGrid(int v) {
    return gPaint.grid ? ((v + 4) & ~7) : v;
}

static Boolean IsShapeTool(int t) {
    return t == kToolLine || (t >= kToolRect && t <= kToolOvalFill);
}

static void ShapeStep(int x, int y) {
    Restore();
    if (gPaint.tool == kToolLine) {
        DrawSeg(gStartX, gStartY, x, y, PenAt);
        return;
    }
    Rect r;
    DragRect(gStartX, gStartY, x, y, &r);
    switch (gPaint.tool) {
        case kToolRect:      PaintShape(kShapeRect, &r, false); break;
        case kToolRectFill:  PaintShape(kShapeRect, &r, true); break;
        case kToolRRect:     PaintShape(kShapeRRect, &r, false); break;
        case kToolRRectFill: PaintShape(kShapeRRect, &r, true); break;
        case kToolOval:      PaintShape(kShapeOval, &r, false); break;
        case kToolOvalFill:  PaintShape(kShapeOval, &r, true); break;
    }
}

static void PolyPreview(int x, int y) {
    Restore();
    OutlinePoints(false);
    if (gPtCount > 0) DrawSeg(gPtX[gPtCount - 1], gPtY[gPtCount - 1], x, y, PenAt);
}

static void PolyFinish(void) {
    if (!gPolyActive) return;
    gPolyActive = false;
    Restore();
    if (gPaint.tool == kToolPolyFill) ScanPolygon(PatternSpan);
    OutlinePoints(true);
    MacPaint_SetDirty();
}

void Tool_Finish(void) {
    PolyFinish();
    TextEnd();
    Sel_Drop();
}

void Tool_Begin(int x, int y, int modifiers) {
    int tool = gPaint.tool;
    gGesture = true;
    gLastX = x;
    gLastY = y;

    if (tool != kToolText) TextEnd();
    if (tool != kToolSelect && tool != kToolLasso) Sel_Drop();

    switch (tool) {
        case kToolSelect:
        case kToolLasso:
            if (Sel_Contains(x, y)) {
                MoveBegin(x, y, (modifiers & optionKey) != 0);
                return;
            }
            Sel_Drop();
            Snap();
            gStartX = SnapToGrid(x);
            gStartY = SnapToGrid(y);
            gPtCount = 0;
            AddPoint(x, y);
            return;

        case kToolText:
            TextEnd();
            SaveUndo();
            Snap();
            gTextActive = true;
            gTextLen = 0;
            gTextX = x;
            gTextY = y - CHICAGO_ASCENT / 2;
            CaretDirty();
            return;

        case kToolBucket:
            SaveUndo();
            BucketFill(x, y);
            MacPaint_SetDirty();
            return;

        case kToolSpray:
            SaveUndo();
            gLastSprayTick = 0;
            SprayAt(x, y);
            return;

        case kToolBrush:
            SaveUndo();
            BrushAt(x, y);
            return;

        case kToolPencil:
            SaveUndo();
            gPencilBlack = !Page_IsBlack(x, y);
            PencilAt(x, y);
            return;

        case kToolEraser:
            SaveUndo();
            EraserAt(x, y);
            return;

        case kToolFree:
        case kToolFreeFill:
            SaveUndo();
            Snap();
            gPtCount = 0;
            AddPoint(x, y);
            PenAt(x, y);
            return;

        case kToolPoly:
        case kToolPolyFill:
            x = SnapToGrid(x);
            y = SnapToGrid(y);
            if (!gPolyActive) {
                SaveUndo();
                Snap();
                gPtCount = 0;
                AddPoint(x, y);
                gPolyActive = true;
                PolyPreview(x, y);
                return;
            }
            /* A click on the first corner closes it; a second click on the
             * last one (a double-click) ends it there */
            {
                int dx0 = x - gPtX[0], dy0 = y - gPtY[0];
                int dxl = x - gPtX[gPtCount - 1], dyl = y - gPtY[gPtCount - 1];
                if ((gPtCount > 2 && dx0 * dx0 + dy0 * dy0 <= 9) ||
                    (dxl * dxl + dyl * dyl <= 4)) {
                    PolyFinish();
                    return;
                }
            }
            AddPoint(x, y);
            PolyPreview(x, y);
            return;

        default:
            if (IsShapeTool(tool)) {
                SaveUndo();
                Snap();
                gStartX = SnapToGrid(x);
                gStartY = SnapToGrid(y);
                ShapeStep(gStartX, gStartY);
            }
            return;
    }
}

void Tool_Move(int x, int y) {
    if (!gGesture) return;
    int tool = gPaint.tool;
    if (tool == kToolSpray) {
        SprayAt(x, y);
        gLastX = x;
        gLastY = y;
        return;
    }
    if (x == gLastX && y == gLastY) return;

    if (gMoving) {
        MoveTo_(x, y);
    } else {
        switch (tool) {
            case kToolSelect: {
                Restore();
                Rect r;
                DragRect(gStartX, gStartY, SnapToGrid(x), SnapToGrid(y), &r);
                for (int px = r.left; px < r.right; px++) {
                    DottedAt(px, r.top);
                    DottedAt(px, r.bottom - 1);
                }
                for (int py = r.top; py < r.bottom; py++) {
                    DottedAt(r.left, py);
                    DottedAt(r.right - 1, py);
                }
                break;
            }
            case kToolLasso:
                AddPoint(x, y);
                DrawSeg(gLastX, gLastY, x, y, DottedAt);
                break;
            case kToolBrush:  DrawSeg(gLastX, gLastY, x, y, BrushAt); break;
            case kToolPencil: DrawSeg(gLastX, gLastY, x, y, PencilAt); break;
            case kToolEraser: DrawSeg(gLastX, gLastY, x, y, EraserAt); break;
            case kToolFree:
            case kToolFreeFill:
                AddPoint(x, y);
                DrawSeg(gLastX, gLastY, x, y, PenAt);
                break;
            case kToolPoly:
            case kToolPolyFill:
                break;
            default:
                if (IsShapeTool(tool)) ShapeStep(SnapToGrid(x), SnapToGrid(y));
                break;
        }
    }
    gLastX = x;
    gLastY = y;
}

void Tool_End(int x, int y) {
    if (!gGesture) return;
    Tool_Move(x, y);
    gGesture = false;

    if (gMoving) {
        MoveEnd();
        return;
    }
    switch (gPaint.tool) {
        case kToolSelect: {
            Restore();
            Rect r;
            DragRect(gStartX, gStartY, SnapToGrid(x), SnapToGrid(y), &r);
            if (r.right - r.left > 1 || r.bottom - r.top > 1) Sel_SetRect(r);
            return;
        }
        case kToolLasso:
            Restore();
            AddPoint(x, y);
            if (gPtCount >= 3) {
                ScanPolygon(LassoSpan);
                gSelOpaque = false;
                gSelDX = gSelDY = 0;
                Sel_Bound();
            }
            return;
        case kToolFree:
            DrawSeg(x, y, gPtX[0], gPtY[0], PenAt);
            break;
        case kToolFreeFill:
            Restore();
            ScanPolygon(PatternSpan);
            OutlinePoints(true);
            break;
        case kToolText:
        case kToolPoly:
        case kToolPolyFill:
            return;
        default:
            break;
    }
    MacPaint_SetDirty();
}

Boolean Tool_WantsHover(void) {
    return gPolyActive;
}

void Tool_Hover(int x, int y) {
    if (!gPolyActive) return;
    x = SnapToGrid(x);
    y = SnapToGrid(y);
    if (x == gLastX && y == gLastY) return;
    gLastX = x;
    gLastY = y;
    PolyPreview(x, y);
}

void Tools_Reset(void) {
    gPolyActive = false;
    gTextActive = false;
    gMoving = false;
    gGesture = false;
    Sel_Drop();
    memset(gSelMask, 0, kPageBytes);
    memset(gPage, 0, kPageBytes);
    gUndoValid = false;
    SetRect(&gSinceSnap, 0, 0, 0, 0);
    MarkAll();
}

/* ------------------------------------------------------------------------
 * The Edit menu
 * ------------------------------------------------------------------------ */

void Edit_Undo(void) {
    if (!gUndoValid) return;
    Tool_Finish();
    memcpy(gTemp, gPage, kPageBytes);
    memcpy(gPage, gUndo, kPageBytes);
    memcpy(gUndo, gTemp, kPageBytes);
    MarkAll();
    MacPaint_SetDirty();
}

static Boolean gClipValid, gClipOpaque;
static int gClipW, gClipH;

Boolean Edit_HasClipboard(void) {
    return gClipValid;
}

void Edit_Copy(void) {
    if (!gSelActive) return;
    memset(gClipBits, 0, kPageBytes);
    memset(gClipMask, 0, kPageBytes);
    for (int y = gSelRect.top; y < gSelRect.bottom; y++) {
        for (int x = gSelRect.left; x < gSelRect.right; x++) {
            if (!GetBit(gSelMask, x, y)) continue;
            int cx = x - gSelRect.left, cy = y - gSelRect.top;
            SetBit(gClipMask, cx, cy, true);
            SetBit(gClipBits, cx, cy, GetBit(gPage, x, y));
        }
    }
    gClipW = gSelRect.right - gSelRect.left;
    gClipH = gSelRect.bottom - gSelRect.top;
    gClipOpaque = gSelOpaque;
    gClipValid = true;
}

void Edit_Clear(void) {
    if (!gSelActive) return;
    SaveUndo();
    for (int y = gSelRect.top; y < gSelRect.bottom; y++)
        for (int x = gSelRect.left; x < gSelRect.right; x++)
            if (GetBit(gSelMask, x, y)) Put(x, y, false);
    Sel_Drop();
    MacPaint_SetDirty();
}

void Edit_Cut(void) {
    if (!gSelActive) return;
    Edit_Copy();
    Edit_Clear();
}

/* Paste puts the clipboard at the top left of the view, selected, ready to
 * be dragged where it belongs */
void Edit_Paste(void) {
    if (!gClipValid) return;
    Tool_Finish();
    SaveUndo();
    int ox = gPaint.viewX + 8, oy = gPaint.viewY + 8;
    if (ox + gClipW > kPageW) ox = kPageW - gClipW;
    if (oy + gClipH > kPageH) oy = kPageH - gClipH;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    for (int cy = 0; cy < gClipH; cy++) {
        for (int cx = 0; cx < gClipW; cx++) {
            if (!GetBit(gClipMask, cx, cy) || !InPage(ox + cx, oy + cy)) continue;
            Boolean black = GetBit(gClipBits, cx, cy);
            if (gClipOpaque || black) Put(ox + cx, oy + cy, black);
            SetBit(gSelMask, ox + cx, oy + cy, true);
        }
    }
    gSelOpaque = gClipOpaque;
    gSelDX = gSelDY = 0;
    Sel_Bound();
    if (gPaint.tool != kToolSelect && gPaint.tool != kToolLasso) gPaint.tool = kToolSelect;
    MacPaint_SetDirty();
}

void Edit_Invert(void) {
    if (!gSelActive) return;
    SaveUndo();
    for (int y = gSelRect.top; y < gSelRect.bottom; y++)
        for (int x = gSelRect.left; x < gSelRect.right; x++)
            if (GetBit(gSelMask, x, y)) Put(x, y, !GetBit(gPage, x, y));
    MacPaint_SetDirty();
}

void Edit_Fill(void) {
    if (!gSelActive) return;
    SaveUndo();
    for (int y = gSelRect.top; y < gSelRect.bottom; y++)
        for (int x = gSelRect.left; x < gSelRect.right; x++)
            if (GetBit(gSelMask, x, y)) PutPat(x, y);
    MacPaint_SetDirty();
}

void Edit_SelectAll(void) {
    Tool_Finish();
    Rect all = { 0, 0, kPageH, kPageW };
    Sel_SetRect(all);
    if (gPaint.tool != kToolSelect && gPaint.tool != kToolLasso) gPaint.tool = kToolSelect;
}

/* What the view shows, in page coordinates */
static void ViewOnPage(Rect* r) {
    int z = Draw_Zoom();
    r->left = (short)gPaint.viewX;
    r->top = (short)gPaint.viewY;
    r->right = (short)(gPaint.viewX + (kViewRight - kViewLeft) / z);
    r->bottom = (short)(gPaint.viewY + (kViewBottom - kViewTop) / z);
    ClipToPage(r);
}

void Edit_EraseView(void) {
    Tool_Finish();
    SaveUndo();
    Rect v;
    ViewOnPage(&v);
    for (int y = v.top; y < v.bottom; y++)
        for (int x = v.left; x < v.right; x++)
            Put(x, y, false);
    MacPaint_SetDirty();
}

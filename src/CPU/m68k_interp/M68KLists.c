/*
 * M68KLists.c - _Pack0, the List Manager, for 68K programs
 *
 * A list is a ListRec in the program's own heap, laid out as Inside
 * Macintosh IV-262 gives it, because programs read it: dataBounds for the
 * number of rows, visible for what shows, selFlags, refCon, userHandle,
 * even cellArray's top bits for which cells are selected. Its cells' data
 * is a handle in the program's heap too, each cell's place in it given by
 * cellArray: one word per cell, the offset of its data with the selected
 * flag in the top bit, and one more word for the end.
 *
 * Drawing is the list definition's: the standard one (LDEF 0) is done here
 * with QuickDraw; any other is the program's own LDEF resource, called in
 * the program as the List Manager calls one. The scroll bars are controls
 * in the list's window.
 *
 * The native List Manager (src/ListManager) has an interface of its own
 * and a private record, so it is not used here.
 */

#include <string.h>
#include "M68KToolboxInternal.h"
#include "DeskManager/DeskManager.h"
#include "QuickDraw/QuickDraw.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "WindowManager/WindowManager.h"
#include "EventManager/EventManager.h"
#include "ResourceManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "chicago_font.h"
#include "System71StdLib.h"
#include "TimeManager/TimeBase.h"

extern UInt32 GetDblTime(void);
extern void GetMouseLocal(Point* pt);     /* the current port's coordinates */

/* ListRec (IM IV-262) */
enum {
    kLRView = 0, kLPort = 8, kLIndent = 12, kLCellSize = 16, kLVisible = 20,
    kLVScroll = 28, kLHScroll = 32, kLSelFlags = 36, kLActive = 37,
    kLReserved = 38, kLListFlags = 39, kLClikTime = 40, kLClikLoc = 44,
    kLMouseLoc = 48, kLClikLoop = 52, kLLastClick = 56, kLRefCon = 60,
    kLListDefProc = 64, kLUserHandle = 68, kLDataBounds = 72, kLCells = 80,
    kLMaxIndex = 84, kLCellArray = 86
};

/* listFlags: autoscrolling, and drawing turned off (LDoDraw) */
enum { kDoHAutoscroll = 0x01, kDoVAutoscroll = 0x02, kDrawingOff = 0x08 };

/* selFlags (IM IV-266) */
enum {
    kOnlyOne = 0x80, kExtendDrag = 0x40, kNoDisjoint = 0x20, kNoExtend = 0x10,
    kNoRect = 0x08, kUseSense = 0x04, kNoNilHilite = 0x02
};

/* LDEF messages */
enum { kInitMsg = 0, kDrawMsg = 1, kHiliteMsg = 2, kCloseMsg = 3 };

/* ------------------------------------------------------------------------
 * The record
 * ------------------------------------------------------------------------ */

static UInt32 LP(UInt32 list) { return M68KHeap_Deref(list); }

static Rect GetRectAt(UInt32 list, UInt32 off) { Rect r; ReadRect(LP(list) + off, &r); return r; }
static void SetRectAt(UInt32 list, UInt32 off, const Rect* r) { WriteRect(LP(list) + off, r); }
static Point GetPointAt(UInt32 list, UInt32 off) { Point p; ReadPoint(LP(list) + off, &p); return p; }

static int Cols(const Rect* db) { return db->right > db->left ? db->right - db->left : 0; }
static int Rows(const Rect* db) { return db->bottom > db->top ? db->bottom - db->top : 0; }

static Boolean ValidList(UInt32 list) { return list != 0 && M68KHeap_IsHandle(list); }

/* A cell's index in cellArray, or -1 if it is not in the list */
static int CellIndex(UInt32 list, Point c) {
    Rect db = GetRectAt(list, kLDataBounds);
    if (c.h < db.left || c.h >= db.right || c.v < db.top || c.v >= db.bottom) return -1;
    return (c.v - db.top) * Cols(&db) + (c.h - db.left);
}

static UInt16 Entry(UInt32 list, int i) { return R16(LP(list) + kLCellArray + 2 * (UInt32)i); }
static void SetEntry(UInt32 list, int i, UInt16 v) { W16(LP(list) + kLCellArray + 2 * (UInt32)i, v); }

static Boolean IsSelected(UInt32 list, int i) { return (Entry(list, i) & 0x8000) != 0; }
static UInt16 CellOffset(UInt32 list, int i) { return Entry(list, i) & 0x7FFF; }
static UInt16 CellLength(UInt32 list, int i) { return (UInt16)((Entry(list, i + 1) & 0x7FFF) - CellOffset(list, i)); }

/* ------------------------------------------------------------------------
 * Rebuilding: every change of shape or contents goes through here. The
 * cells are read out, the change is made to the copy, and the record and
 * the data are written back. Simple, and with at most 32K of data, quick
 * enough.
 * ------------------------------------------------------------------------ */

typedef struct {
    const UInt8* src;           /* the cell's data (native), or NULL */
    UInt16 len;
    UInt8 selected;
} CellInfo;

typedef struct {
    int rows, cols;
    CellInfo* cells;            /* rows * cols */
    UInt8* data;                /* the old data, which cells point into */
} CellCopy;

static Boolean LoadCells(UInt32 list, CellCopy* cc) {
    Rect db = GetRectAt(list, kLDataBounds);
    cc->rows = Rows(&db);
    cc->cols = Cols(&db);
    int n = cc->rows * cc->cols;
    cc->cells = (CellInfo*)NewPtr(sizeof(CellInfo) * (UInt32)(n + 1));
    UInt32 cellsH = R32(LP(list) + kLCells);
    UInt32 total = n ? (Entry(list, n) & 0x7FFF) : 0;
    UInt32 have = M68KHeap_GetHandleSize(cellsH);
    if (total > have) total = have;
    cc->data = (UInt8*)NewPtr(total ? total : 1);
    if (!cc->cells || !cc->data) {
        if (cc->cells) DisposePtr((Ptr)cc->cells);
        if (cc->data) DisposePtr((Ptr)cc->data);
        return false;
    }
    if (total) ReadBytes(M68KHeap_Deref(cellsH), cc->data, total);
    for (int i = 0; i < n; i++) {
        UInt16 off = CellOffset(list, i), len = CellLength(list, i);
        if ((UInt32)off + len > total) len = 0;
        cc->cells[i].src = cc->data + off;
        cc->cells[i].len = len;
        cc->cells[i].selected = IsSelected(list, i);
    }
    return true;
}

static void FreeCells(CellCopy* cc) {
    DisposePtr((Ptr)cc->cells);
    DisposePtr((Ptr)cc->data);
}

/* Write cells (rows x cols) back as the list's contents */
static Boolean StoreCells(UInt32 list, const CellInfo* cells, int rows, int cols) {
    int n = rows * cols;
    UInt32 total = 0;
    for (int i = 0; i < n; i++) total += cells[i].len;
    if (total > 0x7FFF) return false;                  /* offsets are fifteen bits */
    if (M68KHeap_SetHandleSize(list, kLCellArray + 2 * (UInt32)(n + 1)) != noErr) return false;
    UInt32 cellsH = R32(LP(list) + kLCells);
    if (M68KHeap_SetHandleSize(cellsH, total) != noErr) return false;
    UInt32 dataAddr = M68KHeap_Deref(cellsH);
    UInt16 off = 0;
    for (int i = 0; i < n; i++) {
        SetEntry(list, i, (UInt16)(off | (cells[i].selected ? 0x8000 : 0)));
        if (cells[i].len) WriteBytes(dataAddr + off, cells[i].src, cells[i].len);
        off = (UInt16)(off + cells[i].len);
    }
    SetEntry(list, n, off);
    Rect db = GetRectAt(list, kLDataBounds);
    db.bottom = (SInt16)(db.top + rows);
    db.right = (SInt16)(db.left + cols);
    SetRectAt(list, kLDataBounds, &db);
    W16(LP(list) + kLMaxIndex, (UInt16)(2 * n));
    return true;
}

/* ------------------------------------------------------------------------
 * Geometry and scrolling
 * ------------------------------------------------------------------------ */

static ControlHandle VBar(UInt32 list) { UInt32 h = R32(LP(list) + kLVScroll); return h ? Obj_Control(h) : NULL; }
static ControlHandle HBar(UInt32 list) { UInt32 h = R32(LP(list) + kLHScroll); return h ? Obj_Control(h) : NULL; }

/* The range the first visible row (or column) can take */
static void ScrollRange(UInt32 list, Boolean vertical, SInt16* min, SInt16* max) {
    Rect db = GetRectAt(list, kLDataBounds), vis = GetRectAt(list, kLVisible), rv = GetRectAt(list, kLRView);
    Point cs = GetPointAt(list, kLCellSize);
    if (vertical) {
        int shown = vis.bottom - vis.top;
        *min = db.top;
        int m = db.top + Rows(&db) - shown;
        if (cs.v * shown > rv.bottom - rv.top) m++;        /* the last row only partly shows */
        *max = (SInt16)(m < db.top ? db.top : m);
    } else {
        int shown = vis.right - vis.left;
        *min = db.left;
        int m = db.left + Cols(&db) - shown;
        if (cs.h * shown > rv.right - rv.left) m++;
        *max = (SInt16)(m < db.left ? db.left : m);
    }
}

static void SyncBar(ControlHandle c, SInt16 min, SInt16 max, SInt16 value) {
    if (!c) return;
    SetControlMinimum(c, min);
    SetControlMaximum(c, max);
    SetControlValue(c, value);
    Obj_SyncControl(c);
}

/* visible from rView and cellSize, kept within what can be scrolled to,
 * and the scroll bars told */
static void Recompute(UInt32 list) {
    Rect rv = GetRectAt(list, kLRView), vis = GetRectAt(list, kLVisible);
    Point cs = GetPointAt(list, kLCellSize);
    int ch = cs.v > 0 ? cs.v : 1, cw = cs.h > 0 ? cs.h : 1;
    vis.bottom = (SInt16)(vis.top + (rv.bottom - rv.top + ch - 1) / ch);
    vis.right = (SInt16)(vis.left + (rv.right - rv.left + cw - 1) / cw);
    SetRectAt(list, kLVisible, &vis);
    SInt16 vmin, vmax, hmin, hmax;
    ScrollRange(list, true, &vmin, &vmax);
    ScrollRange(list, false, &hmin, &hmax);
    SInt16 top = vis.top < vmin ? vmin : vis.top > vmax ? vmax : vis.top;
    SInt16 left = vis.left < hmin ? hmin : vis.left > hmax ? hmax : vis.left;
    if (top != vis.top || left != vis.left) {
        vis.bottom = (SInt16)(vis.bottom + top - vis.top);
        vis.right = (SInt16)(vis.right + left - vis.left);
        vis.top = top;
        vis.left = left;
        SetRectAt(list, kLVisible, &vis);
    }
    SyncBar(VBar(list), vmin, vmax, vis.top);
    SyncBar(HBar(list), hmin, hmax, vis.left);
}

/* A cell's rectangle in the window, whether or not it shows */
static Rect CellRect(UInt32 list, Point c) {
    Rect rv = GetRectAt(list, kLRView), vis = GetRectAt(list, kLVisible);
    Point cs = GetPointAt(list, kLCellSize);
    Rect r;
    r.left = (SInt16)(rv.left + (c.h - vis.left) * cs.h);
    r.top = (SInt16)(rv.top + (c.v - vis.top) * cs.v);
    r.right = (SInt16)(r.left + cs.h);
    r.bottom = (SInt16)(r.top + cs.v);
    return r;
}

/* ------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------ */

typedef struct {
    GrafPtr saved, port;
    RgnHandle clip;             /* the port's own clip, to put back */
} DrawState;

static Boolean Drawing(UInt32 list) { return !(R8(LP(list) + kLListFlags) & kDrawingOff); }

static Boolean BeginDraw(UInt32 list, DrawState* d) {
    d->port = Obj_Port(R32(LP(list) + kLPort));
    if (!d->port) return false;
    GetPort(&d->saved);
    SetPort(d->port);
    Obj_SyncPortIn(d->port);
    d->clip = NewRgn();
    GetClip(d->clip);
    return true;
}

static void EndDraw(DrawState* d) {
    SetClip(d->clip);
    DisposeRgn(d->clip);
    Obj_SyncPortOut(d->port);
    SetPort(d->saved);
}

/* Drawing limited to r, within the list and the port's own clip */
static void ClipTo(UInt32 list, DrawState* d, const Rect* r) {
    Rect rv = GetRectAt(list, kLRView), both;
    SectRect(r, &rv, &both);
    RgnHandle rgn = NewRgn();
    RectRgn(rgn, &both);
    SectRgn(rgn, d->clip, rgn);
    SetClip(rgn);
    DisposeRgn(rgn);
}

/* The message to the list's definition: the standard one here, a
 * program's LDEF called in the program */
static void CallLDEF(UInt32 list, DrawState* d, SInt16 msg, Boolean sel, const Rect* r, Point cell,
                     UInt16 off, UInt16 len) {
    UInt32 proc = R32(LP(list) + kLListDefProc);
    if (proc) {
        UInt32 sp = A(7);
        A(7) -= 8;
        UInt32 rectAddr = A(7);
        if (r) WriteRect(rectAddr, r);
        A(7) -= 2; W16(A(7), (UInt16)msg);
        A(7) -= 2; W16(A(7), sel ? 0x0100 : 0);
        A(7) -= 4; W32(A(7), rectAddr);
        A(7) -= 4; W16(A(7), (UInt16)cell.v); W16(A(7) + 2, (UInt16)cell.h);
        A(7) -= 2; W16(A(7), off);
        A(7) -= 2; W16(A(7), len);
        A(7) -= 4; W32(A(7), list);
        if (d) { Obj_SyncPortOut(d->port); Obj_SetThePort(d->port); }
        CallProgram(M68KHeap_Deref(proc));
        if (d) { SetPort(d->port); Obj_SyncPortIn(d->port); }
        A(7) = sp;
        return;
    }
    /* LDEF 0: the text, indented, inverted when selected */
    switch (msg) {
    case kInitMsg:
        W16(LP(list) + kLIndent, CHICAGO_ASCENT);           /* indent.v */
        W16(LP(list) + kLIndent + 2, 5);                    /* indent.h */
        break;
    case kDrawMsg: {
        Point indent = GetPointAt(list, kLIndent);
        char text[256];
        UInt16 n = len > sizeof(text) ? (UInt16)sizeof(text) : len;
        if (n) ReadBytes(M68KHeap_Deref(R32(LP(list) + kLCells)) + off, text, n);
        Ports_BeforeDraw(d->port);
        EraseRect(r);
        if (n) {
            MoveTo((SInt16)(r->left + indent.h), (SInt16)(r->top + indent.v));
            DrawText(text, 0, (SInt16)n);
        }
        if (sel) InvertRect(r);
        Ports_AfterDraw(d->port);
        break;
    }
    case kHiliteMsg:
        Ports_BeforeDraw(d->port);
        InvertRect(r);
        Ports_AfterDraw(d->port);
        break;
    default:
        break;
    }
}

static void DrawCell(UInt32 list, DrawState* d, Point c) {
    Rect r = CellRect(list, c);
    ClipTo(list, d, &r);
    int i = CellIndex(list, c);
    if (i < 0) {
        Ports_BeforeDraw(d->port);
        EraseRect(&r);                                     /* past the last row or column */
        Ports_AfterDraw(d->port);
        return;
    }
    Boolean sel = IsSelected(list, i) && R8(LP(list) + kLActive);
    CallLDEF(list, d, kDrawMsg, sel, &r, c, CellOffset(list, i), CellLength(list, i));
}

/* Every visible cell that meets rgn (or all, for NULL), and the scroll bars */
static void DrawList(UInt32 list, RgnHandle rgn) {
    DrawState d;
    if (!BeginDraw(list, &d)) return;
    Rect vis = GetRectAt(list, kLVisible), rv = GetRectAt(list, kLRView);
    for (SInt16 v = vis.top; v < vis.bottom; v++) {
        for (SInt16 h = vis.left; h < vis.right; h++) {
            Point c = { v, h };
            Rect r = CellRect(list, c), both;
            SectRect(&r, &rv, &both);
            if (!rgn || RectInRgn(&both, rgn)) DrawCell(list, &d, c);
        }
    }
    SetClip(d.clip);
    ControlHandle bars[2] = { VBar(list), HBar(list) };
    for (int k = 0; k < 2; k++) {
        if (bars[k] && (*bars[k])->contrlVis && (!rgn || RectInRgn(&(*bars[k])->contrlRect, rgn))) {
            Draw1Control(bars[k]);
        }
    }
    EndDraw(&d);
}

static void DrawOne(UInt32 list, Point c) {
    if (!Drawing(list)) return;
    Rect vis = GetRectAt(list, kLVisible);
    if (c.v < vis.top || c.v >= vis.bottom || c.h < vis.left || c.h >= vis.right) return;
    DrawState d;
    if (!BeginDraw(list, &d)) return;
    DrawCell(list, &d, c);
    EndDraw(&d);
}

/* A cell selected or not, hilited by the definition if that changed it */
static void SetSelected(UInt32 list, Point c, Boolean on) {
    int i = CellIndex(list, c);
    if (i < 0 || IsSelected(list, i) == on) return;
    SetEntry(list, i, (UInt16)((Entry(list, i) & 0x7FFF) | (on ? 0x8000 : 0)));
    if (!Drawing(list) || !R8(LP(list) + kLActive)) return;
    if ((R8(LP(list) + kLSelFlags) & kNoNilHilite) && CellLength(list, i) == 0) return;
    Rect vis = GetRectAt(list, kLVisible);
    if (c.v < vis.top || c.v >= vis.bottom || c.h < vis.left || c.h >= vis.right) return;
    DrawState d;
    if (!BeginDraw(list, &d)) return;
    Rect r = CellRect(list, c);
    ClipTo(list, &d, &r);
    CallLDEF(list, &d, kHiliteMsg, on, &r, c, CellOffset(list, i), CellLength(list, i));
    EndDraw(&d);
}

/* Every selected cell that shows, hilited by the definition: for LActivate */
static void HiliteSelected(UInt32 list) {
    if (!Drawing(list)) return;
    DrawState d;
    if (!BeginDraw(list, &d)) return;
    Rect vis = GetRectAt(list, kLVisible);
    Boolean noNil = (R8(LP(list) + kLSelFlags) & kNoNilHilite) != 0;
    for (SInt16 v = vis.top; v < vis.bottom; v++)
        for (SInt16 h = vis.left; h < vis.right; h++) {
            Point c = { v, h };
            int i = CellIndex(list, c);
            if (i < 0 || !IsSelected(list, i) || (noNil && CellLength(list, i) == 0)) continue;
            Rect r = CellRect(list, c);
            ClipTo(list, &d, &r);
            CallLDEF(list, &d, kHiliteMsg, R8(LP(list) + kLActive) != 0, &r, c,
                     CellOffset(list, i), CellLength(list, i));
        }
    EndDraw(&d);
}

static void ScrollTo(UInt32 list, SInt16 top, SInt16 left) {
    SInt16 vmin, vmax, hmin, hmax;
    ScrollRange(list, true, &vmin, &vmax);
    ScrollRange(list, false, &hmin, &hmax);
    if (top < vmin) top = vmin;
    if (top > vmax) top = vmax;
    if (left < hmin) left = hmin;
    if (left > hmax) left = hmax;
    Rect vis = GetRectAt(list, kLVisible);
    if (top == vis.top && left == vis.left) return;
    vis.bottom = (SInt16)(vis.bottom + top - vis.top);
    vis.right = (SInt16)(vis.right + left - vis.left);
    vis.top = top;
    vis.left = left;
    SetRectAt(list, kLVisible, &vis);
    SyncBar(VBar(list), vmin, vmax, top);
    SyncBar(HBar(list), hmin, hmax, left);
    if (Drawing(list)) DrawList(list, NULL);
}

/* ------------------------------------------------------------------------
 * Changing the cells
 * ------------------------------------------------------------------------ */

static void AfterReshape(UInt32 list) {
    Recompute(list);
    if (Drawing(list)) DrawList(list, NULL);
}

/* Rows (or columns) in, at index 'at', count of them */
static SInt16 AddLines(UInt32 list, SInt16 count, SInt16 at, Boolean rows) {
    Rect db = GetRectAt(list, kLDataBounds);
    SInt16 lo = rows ? db.top : db.left, hi = rows ? db.bottom : db.right;
    if (at > hi) at = hi;
    if (at < lo) at = lo;
    if (count <= 0) return at;
    CellCopy cc;
    if (!LoadCells(list, &cc)) return at;
    int nr = cc.rows + (rows ? count : 0), nc = cc.cols + (rows ? 0 : count);
    CellInfo* out = (CellInfo*)NewPtr(sizeof(CellInfo) * (UInt32)(nr * nc + 1));
    if (out) {
        int pos = at - lo;
        for (int r = 0; r < nr; r++) {
            for (int c = 0; c < nc; c++) {
                int sr = r, sc = c;
                Boolean fresh = false;
                if (rows) { if (r >= pos && r < pos + count) fresh = true; else if (r >= pos + count) sr = r - count; }
                else      { if (c >= pos && c < pos + count) fresh = true; else if (c >= pos + count) sc = c - count; }
                CellInfo* o = &out[r * nc + c];
                if (fresh) { o->src = NULL; o->len = 0; o->selected = 0; }
                else *o = cc.cells[sr * cc.cols + sc];
            }
        }
        StoreCells(list, out, nr, nc);
        DisposePtr((Ptr)out);
    }
    FreeCells(&cc);
    AfterReshape(list);
    return at;
}

/* Rows (or columns) out: count of them from 'from'; a count of 0, all */
static void DelLines(UInt32 list, SInt16 count, SInt16 from, Boolean rows) {
    Rect db = GetRectAt(list, kLDataBounds);
    SInt16 lo = rows ? db.top : db.left, hi = rows ? db.bottom : db.right;
    if (count == 0) { from = lo; count = (SInt16)(hi - lo); }
    if (from < lo) from = lo;
    if (from >= hi || count <= 0) return;
    if (from + count > hi) count = (SInt16)(hi - from);
    CellCopy cc;
    if (!LoadCells(list, &cc)) return;
    int nr = cc.rows - (rows ? count : 0), nc = cc.cols - (rows ? 0 : count);
    CellInfo* out = (CellInfo*)NewPtr(sizeof(CellInfo) * (UInt32)(nr * nc + 1));
    if (out) {
        int pos = from - lo;
        for (int r = 0; r < nr; r++) {
            for (int c = 0; c < nc; c++) {
                int sr = rows && r >= pos ? r + count : r;
                int sc = !rows && c >= pos ? c + count : c;
                out[r * nc + c] = cc.cells[sr * cc.cols + sc];
            }
        }
        StoreCells(list, out, nr, nc);
        DisposePtr((Ptr)out);
    }
    FreeCells(&cc);
    AfterReshape(list);
}

/* A cell's data replaced, or added to */
static void PutCell(UInt32 list, Point c, UInt32 dataPtr, SInt16 len, Boolean append) {
    int i = CellIndex(list, c);
    if (i < 0) return;
    if (len < 0) len = 0;
    CellCopy cc;
    if (!LoadCells(list, &cc)) return;
    UInt16 keep = append ? cc.cells[i].len : 0;
    UInt8* bytes = (UInt8*)NewPtr((UInt32)keep + (UInt32)len + 1);
    if (bytes) {
        if (keep) memcpy(bytes, cc.cells[i].src, keep);
        if (len) ReadBytes(dataPtr, bytes + keep, (UInt32)len);
        cc.cells[i].src = bytes;
        cc.cells[i].len = (UInt16)(keep + len);
        StoreCells(list, cc.cells, cc.rows, cc.cols);
        DisposePtr((Ptr)bytes);
    }
    FreeCells(&cc);
    DrawOne(list, c);
}

/* ------------------------------------------------------------------------
 * Finding cells
 * ------------------------------------------------------------------------ */

/* The next cell, row by row; false at the end */
static Boolean NextCell(UInt32 list, Boolean hNext, Boolean vNext, Point* c) {
    Rect db = GetRectAt(list, kLDataBounds);
    Point n = *c;
    if (hNext && vNext) {
        n.h++;
        if (n.h >= db.right) { n.h = db.left; n.v++; }
        if (n.v >= db.bottom) return false;
    } else if (hNext) {
        if (++n.h >= db.right) return false;
    } else if (vNext) {
        if (++n.v >= db.bottom) return false;
    } else {
        return false;
    }
    *c = n;
    return true;
}

/* The first selected cell at or after *c */
static Boolean NextSelected(UInt32 list, Point* c) {
    Rect db = GetRectAt(list, kLDataBounds);
    if (Cols(&db) == 0 || Rows(&db) == 0) return false;
    Point p = *c;
    if (p.v < db.top) { p.v = db.top; p.h = db.left; }
    if (p.h < db.left) p.h = db.left;
    if (p.h >= db.right) { p.h = db.left; p.v++; }
    for (; p.v < db.bottom; p.v++, p.h = db.left) {
        for (; p.h < db.right; p.h++) {
            if (IsSelected(list, CellIndex(list, p))) { *c = p; return true; }
        }
    }
    return false;
}

static void SelectOnly(UInt32 list, const Rect* keep) {
    Rect db = GetRectAt(list, kLDataBounds);
    for (SInt16 v = db.top; v < db.bottom; v++)
        for (SInt16 h = db.left; h < db.right; h++) {
            Point c = { v, h };
            Boolean in = keep && h >= keep->left && h < keep->right && v >= keep->top && v < keep->bottom;
            SetSelected(list, c, in);
        }
}

static Boolean SameText(const UInt8* a, UInt16 alen, const UInt8* b, UInt16 blen) {
    if (alen != blen) return false;
    for (UInt16 i = 0; i < alen; i++) {
        UInt8 x = a[i], y = b[i];
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y) return false;
    }
    return true;
}

/* ------------------------------------------------------------------------
 * Clicking
 * ------------------------------------------------------------------------ */

static UInt32 gScrolling;       /* the list whose scroll bar is being tracked */

static void ScrollAction(ControlHandle c, SInt16 part) {
    UInt32 list = gScrolling;
    if (!list) return;
    Boolean vertical = c == VBar(list);
    Rect vis = GetRectAt(list, kLVisible);
    SInt16 page = (SInt16)((vertical ? vis.bottom - vis.top : vis.right - vis.left) - 1);
    if (page < 1) page = 1;
    SInt16 delta = part == inUpButton ? -1 : part == inDownButton ? 1 :
                   part == inPageUp ? (SInt16)-page : part == inPageDown ? page : 0;
    if (vertical) ScrollTo(list, (SInt16)(vis.top + delta), vis.left);
    else ScrollTo(list, vis.top, (SInt16)(vis.left + delta));
}

/* Which cell a point is over, kept within what shows */
static Point CellAt(UInt32 list, Point pt) {
    Rect rv = GetRectAt(list, kLRView), vis = GetRectAt(list, kLVisible);
    Point cs = GetPointAt(list, kLCellSize);
    if (pt.h < rv.left) pt.h = rv.left;
    if (pt.h >= rv.right) pt.h = (SInt16)(rv.right - 1);
    if (pt.v < rv.top) pt.v = rv.top;
    if (pt.v >= rv.bottom) pt.v = (SInt16)(rv.bottom - 1);
    Point c;
    c.h = (SInt16)(vis.left + (pt.h - rv.left) / (cs.h > 0 ? cs.h : 1));
    c.v = (SInt16)(vis.top + (pt.v - rv.top) / (cs.v > 0 ? cs.v : 1));
    return c;
}

/* The selection a drag from anchor to c makes: a rectangle of cells */
static Rect Span(Point a, Point c) {
    Rect r;
    r.left = a.h < c.h ? a.h : c.h;
    r.right = (SInt16)((a.h > c.h ? a.h : c.h) + 1);
    r.top = a.v < c.v ? a.v : c.v;
    r.bottom = (SInt16)((a.v > c.v ? a.v : c.v) + 1);
    return r;
}

static Boolean ClikLoop(UInt32 list) {
    UInt32 proc = R32(LP(list) + kLClikLoop);
    if (!proc) return true;
    UInt32 sp = A(7);
    A(7) -= 2;
    W16(A(7), 0x0100);                                     /* result: carry on */
    Boolean go = CallProgram(proc) != noErr || (R16(A(7)) & 0xFF00) != 0;
    A(7) = sp;
    return go;
}

static Boolean Click(UInt32 list, Point pt, UInt16 mods) {
    WindowPtr w = (WindowPtr)Obj_Port(R32(LP(list) + kLPort));
    if (!w) return false;

    /* The scroll bars track themselves */
    ControlHandle bar = NULL;
    SInt16 part = FindControl(pt, w, &bar);
    if (part && bar && (bar == VBar(list) || bar == HBar(list))) {
        UInt32 saved = gScrolling;
        gScrolling = list;
        part = TrackControl(bar, pt, part == inThumb ? NULL : (ControlActionProcPtr)ScrollAction);
        gScrolling = saved;
        if (part == inThumb) {
            Rect vis = GetRectAt(list, kLVisible);
            SInt16 v = GetControlValue(bar);
            if (bar == VBar(list)) ScrollTo(list, v, vis.left);
            else ScrollTo(list, vis.top, v);
        }
        Obj_SyncControl(bar);
        return false;
    }
    Rect rv = GetRectAt(list, kLRView);
    if (!PtInRect(pt, &rv)) return false;

    Point cell = CellAt(list, pt);
    Point last = GetPointAt(list, kLLastClick);
    UInt32 now = TickCount();
    Boolean dbl = cell.h == last.h && cell.v == last.v &&
                  now - R32(LP(list) + kLClikTime) <= GetDblTime();
    W32(LP(list) + kLClikTime, now);
    WritePoint(LP(list) + kLClikLoc, pt);
    WritePoint(LP(list) + kLLastClick, cell);

    UInt8 flags = R8(LP(list) + kLSelFlags);
    Boolean extending = ((mods & shiftKey) || (flags & kExtendDrag)) && !(flags & kOnlyOne);
    Boolean toggle = !extending && (mods & cmdKey) && !(flags & kOnlyOne);
    Rect db = GetRectAt(list, kLDataBounds);

    /* Where an extended selection is anchored: the corner of what is
     * selected away from the click */
    Point anchor = cell;
    if (extending && !(flags & kNoExtend)) {
        Point first = { db.top, db.left };
        if (NextSelected(list, &first)) {
            Point lastSel = first, p = first;
            while (NextCell(list, true, true, &p)) if (IsSelected(list, CellIndex(list, p))) lastSel = p;
            anchor = (cell.v > first.v || (cell.v == first.v && cell.h >= first.h)) ? first : lastSel;
        }
    }
    Boolean sense = toggle ? !IsSelected(list, CellIndex(list, cell)) : true;

    Point prev = { -32768, -32768 };
    for (;;) {
        if (CellIndex(list, cell) >= 0 && (cell.h != prev.h || cell.v != prev.v)) {
            if (toggle || (flags & kUseSense)) {
                SetSelected(list, cell, sense);
            } else if (extending) {
                Rect span = Span(anchor, cell);
                if (flags & kNoDisjoint) SelectOnly(list, &span);
                else for (SInt16 v = span.top; v < span.bottom; v++)
                    for (SInt16 h = span.left; h < span.right; h++) { Point c = { v, h }; SetSelected(list, c, true); }
            } else {
                Rect one = { cell.v, cell.h, (SInt16)(cell.v + 1), (SInt16)(cell.h + 1) };
                SelectOnly(list, &one);
            }
            prev = cell;
        }
        if (!StillDown() || !ClikLoop(list)) break;
        Point m;
        GetMouseLocal(&m);
        WritePoint(LP(list) + kLMouseLoc, m);
        /* Past the edge, the list scrolls toward the mouse */
        UInt8 lf = R8(LP(list) + kLListFlags);
        Rect vis = GetRectAt(list, kLVisible);
        if ((lf & kDoVAutoscroll) && m.v < rv.top) ScrollTo(list, (SInt16)(vis.top - 1), vis.left);
        else if ((lf & kDoVAutoscroll) && m.v >= rv.bottom) ScrollTo(list, (SInt16)(vis.top + 1), vis.left);
        if ((lf & kDoHAutoscroll) && m.h < rv.left) ScrollTo(list, vis.top, (SInt16)(vis.left - 1));
        else if ((lf & kDoHAutoscroll) && m.h >= rv.right) ScrollTo(list, vis.top, (SInt16)(vis.left + 1));
        cell = CellAt(list, m);
        WritePoint(LP(list) + kLLastClick, cell);
        SystemTask();
    }
    return dbl;
}

/* ------------------------------------------------------------------------
 * LNew and LDispose
 * ------------------------------------------------------------------------ */

static void CellSize(UInt32 list, Point cs) {
    if (cs.h == 0 && cs.v == 0) {
        Rect rv = GetRectAt(list, kLRView), db = GetRectAt(list, kLDataBounds);
        int cols = Cols(&db);
        cs.h = (SInt16)((rv.right - rv.left) / (cols ? cols : 1));
        cs.v = CHICAGO_ASCENT + CHICAGO_DESCENT;
    }
    WritePoint(LP(list) + kLCellSize, cs);
    Recompute(list);
}

static UInt32 NewList(const Rect* rView, const Rect* bounds, Point cs, SInt16 proc, UInt32 window,
                      Boolean drawIt, Boolean hasGrow, Boolean scrollH, Boolean scrollV) {
    (void)hasGrow;
    WindowPtr w = (WindowPtr)Obj_Port(window);
    if (!w) return 0;
    int n = Cols(bounds) * Rows(bounds);
    UInt32 list = M68KHeap_NewHandle(kLCellArray + 2 * (UInt32)(n + 1), true);
    UInt32 cells = M68KHeap_NewHandle(0, true);
    UInt32 def = 0;
    if (proc != 0) {
        /* A program's own definition, from its resources */
        Handle native = GetResource(FOURCC('L','D','E','F'), proc);
        def = native ? M68KTB_ResHandleFor(native) : 0;
        if (!def) {
            if (list) M68KHeap_DisposeHandle(list);
            if (cells) M68KHeap_DisposeHandle(cells);
            return 0;
        }
        M68KHeap_SetState(def, (UInt8)(M68KHeap_GetState(def) | 0x80));   /* it runs in place */
    }
    if (!list || !cells) {
        if (list) M68KHeap_DisposeHandle(list);
        if (cells) M68KHeap_DisposeHandle(cells);
        return 0;
    }
    UInt32 p = LP(list);
    WriteRect(p + kLRView, rView);
    W32(p + kLPort, window);
    WriteRect(p + kLDataBounds, bounds);
    Rect vis = { bounds->top, bounds->left, bounds->top, bounds->left };
    WriteRect(p + kLVisible, &vis);
    W8(p + kLActive, 1);
    W8(p + kLListFlags, (UInt8)((drawIt ? 0 : kDrawingOff) | (scrollV ? kDoVAutoscroll : 0) |
                                (scrollH ? kDoHAutoscroll : 0)));
    Point none = { -1, -1 };
    WritePoint(p + kLLastClick, none);
    W32(p + kLListDefProc, def);
    W32(p + kLCells, cells);
    W16(p + kLMaxIndex, (UInt16)(2 * n));
    /* cellArray is all zero: every cell empty and unselected */

    static const UInt8 kNoTitle[] = { 0 };
    if (scrollV) {
        Rect r = { (SInt16)(rView->top - 1), rView->right, (SInt16)(rView->bottom + 1), (SInt16)(rView->right + 16) };
        ControlHandle c = NewControl(w, &r, kNoTitle, drawIt, 0, 0, 0, scrollBarProc, 0);
        W32(LP(list) + kLVScroll, c ? Obj_ControlFor(c) : 0);
    }
    if (scrollH) {
        Rect r = { rView->bottom, (SInt16)(rView->left - 1), (SInt16)(rView->bottom + 16), (SInt16)(rView->right + 1) };
        ControlHandle c = NewControl(w, &r, kNoTitle, drawIt, 0, 0, 0, scrollBarProc, 0);
        W32(LP(list) + kLHScroll, c ? Obj_ControlFor(c) : 0);
    }
    CellSize(list, cs);
    Point origin = { 0, 0 };
    DrawState d;
    Boolean drawing = BeginDraw(list, &d);
    CallLDEF(list, drawing ? &d : NULL, kInitMsg, false, NULL, origin, 0, 0);
    if (drawing) EndDraw(&d);
    return list;
}

static void DisposeList(UInt32 list) {
    Point origin = { 0, 0 };
    if (R32(LP(list) + kLListDefProc)) {
        DrawState d;
        Boolean drawing = BeginDraw(list, &d);
        CallLDEF(list, drawing ? &d : NULL, kCloseMsg, false, NULL, origin, 0, 0);
        if (drawing) EndDraw(&d);
    }
    ControlHandle bars[2] = { VBar(list), HBar(list) };
    for (int k = 0; k < 2; k++) {
        if (bars[k]) {
            Obj_ForgetControl(bars[k]);
            DisposeControl(bars[k]);
        }
    }
    UInt32 cells = R32(LP(list) + kLCells);
    if (cells) M68KHeap_DisposeHandle(cells);
    M68KHeap_DisposeHandle(list);
}

/* ------------------------------------------------------------------------
 * _Pack0
 * ------------------------------------------------------------------------ */

enum {
    kSelActivate = 0, kSelAddColumn = 4, kSelAddRow = 8, kSelAddToCell = 12, kSelAutoScroll = 16,
    kSelCellSizeSel = 20, kSelClick = 24, kSelClrCell = 28, kSelDelColumn = 32, kSelDelRow = 36,
    kSelDispose = 40, kSelDoDraw = 44, kSelDraw = 48, kSelFind = 52, kSelGetCell = 56,
    kSelGetSelect = 60, kSelLastClick = 64, kSelNew = 68, kSelNextCell = 72, kSelRect = 76,
    kSelScroll = 80, kSelSearch = 84, kSelSetCell = 88, kSelSetSelect = 92, kSelSize = 96,
    kSelUpdate = 100
};

TRAP(Trap_Pack0) {
    UNUSED;
    UInt16 selector = Pop16();
    if (selector == kSelNew) {
        Boolean scrollV = PopBool(), scrollH = PopBool(), grow = PopBool(), drawIt = PopBool();
        UInt32 window = Pop32();
        SInt16 proc = (SInt16)Pop16();
        Point cs = PopPoint();
        Rect bounds, view;
        ReadRect(Pop32(), &bounds);
        ReadRect(Pop32(), &view);
        Result32(NewList(&view, &bounds, cs, proc, window, drawIt, grow, scrollH, scrollV));
        return noErr;
    }

    UInt32 list = Pop32();
    Boolean ok = ValidList(list);
    switch (selector) {
    case kSelActivate: {
        /* The selection shown or not: hilited as the list becomes active,
         * unhilited before it stops being */
        Boolean act = PopBool();
        if (!ok || (R8(LP(list) + kLActive) != 0) == act) break;
        if (act) W8(LP(list) + kLActive, 1);
        HiliteSelected(list);
        if (!act) W8(LP(list) + kLActive, 0);
        ControlHandle bars[2] = { VBar(list), HBar(list) };
        for (int k = 0; k < 2; k++) if (bars[k]) { HiliteControl(bars[k], act ? 0 : 255); Obj_SyncControl(bars[k]); }
        break;
    }
    case kSelAddColumn: case kSelAddRow: {
        SInt16 at = (SInt16)Pop16(), count = (SInt16)Pop16();
        Result16(ok ? (UInt16)AddLines(list, count, at, selector == kSelAddRow) : 0);
        break;
    }
    case kSelDelColumn: case kSelDelRow: {
        SInt16 from = (SInt16)Pop16(), count = (SInt16)Pop16();
        if (ok) DelLines(list, count, from, selector == kSelDelRow);
        break;
    }
    case kSelAddToCell: case kSelSetCell: {
        Point c = PopPoint();
        SInt16 len = (SInt16)Pop16();
        UInt32 ptr = Pop32();
        if (ok) PutCell(list, c, ptr, len, selector == kSelAddToCell);
        break;
    }
    case kSelClrCell: {
        Point c = PopPoint();
        if (ok) PutCell(list, c, 0, 0, false);
        break;
    }
    case kSelAutoScroll: {
        if (!ok) break;
        Rect db = GetRectAt(list, kLDataBounds), vis = GetRectAt(list, kLVisible);
        Point c = { db.top, db.left };
        if (NextSelected(list, &c) &&
            (c.v < vis.top || c.v >= vis.bottom || c.h < vis.left || c.h >= vis.right)) {
            ScrollTo(list, c.v, c.h);
        }
        break;
    }
    case kSelCellSizeSel: {
        Point cs = PopPoint();
        if (ok) CellSize(list, cs);
        break;
    }
    case kSelClick: {
        UInt16 mods = Pop16();
        Point pt = PopPoint();
        ResultBool(ok && Click(list, pt, mods));
        break;
    }
    case kSelDispose:
        if (ok) DisposeList(list);
        break;
    case kSelDoDraw: {
        Boolean on = PopBool();
        if (!ok) break;
        UInt8 f = R8(LP(list) + kLListFlags);
        W8(LP(list) + kLListFlags, on ? (f & ~kDrawingOff) : (f | kDrawingOff));
        break;
    }
    case kSelDraw: {
        Point c = PopPoint();
        if (ok) DrawOne(list, c);
        break;
    }
    case kSelFind: {
        Point c = PopPoint();
        UInt32 lenAddr = Pop32(), offAddr = Pop32();
        int i = ok ? CellIndex(list, c) : -1;
        W16(offAddr, i < 0 ? 0xFFFF : CellOffset(list, i));
        W16(lenAddr, i < 0 ? 0xFFFF : CellLength(list, i));
        break;
    }
    case kSelGetCell: {
        Point c = PopPoint();
        UInt32 lenAddr = Pop32(), ptr = Pop32();
        int i = ok ? CellIndex(list, c) : -1;
        SInt16 room = (SInt16)R16(lenAddr);
        UInt16 n = 0;
        if (i >= 0 && room > 0) {
            n = CellLength(list, i);
            if (n > (UInt16)room) n = (UInt16)room;
            UInt32 src = M68KHeap_Deref(R32(LP(list) + kLCells)) + CellOffset(list, i);
            for (UInt16 k = 0; k < n; k++) W8(ptr + k, R8(src + k));
        }
        W16(lenAddr, n);
        break;
    }
    case kSelGetSelect: {
        UInt32 cellAddr = Pop32();
        Boolean next = PopBool();
        Point c;
        ReadPoint(cellAddr, &c);
        Boolean found = false;
        if (ok) {
            if (next) {
                found = NextSelected(list, &c);
                if (found) WritePoint(cellAddr, c);
            } else {
                int i = CellIndex(list, c);
                found = i >= 0 && IsSelected(list, i);
            }
        }
        ResultBool(found);
        break;
    }
    case kSelLastClick: {
        Point c = ok ? GetPointAt(list, kLLastClick) : (Point){ -1, -1 };
        W16(A(7), (UInt16)c.v);
        W16(A(7) + 2, (UInt16)c.h);
        break;
    }
    case kSelNextCell: {
        UInt32 cellAddr = Pop32();
        Boolean vNext = PopBool(), hNext = PopBool();
        Point c;
        ReadPoint(cellAddr, &c);
        Boolean more = ok && NextCell(list, hNext, vNext, &c);
        if (more) WritePoint(cellAddr, c);
        ResultBool(more);
        break;
    }
    case kSelRect: {
        Point c = PopPoint();
        UInt32 rectAddr = Pop32();
        Rect r = { 0, 0, 0, 0 };
        if (ok && CellIndex(list, c) >= 0) r = CellRect(list, c);
        WriteRect(rectAddr, &r);
        break;
    }
    case kSelScroll: {
        SInt16 dRows = (SInt16)Pop16(), dCols = (SInt16)Pop16();
        if (!ok) break;
        Rect vis = GetRectAt(list, kLVisible);
        ScrollTo(list, (SInt16)(vis.top + dRows), (SInt16)(vis.left + dCols));
        break;
    }
    case kSelSearch: {
        UInt32 cellAddr = Pop32(), proc = Pop32();
        SInt16 len = (SInt16)Pop16();
        UInt32 ptr = Pop32();
        Point c;
        ReadPoint(cellAddr, &c);
        Boolean found = false;
        UInt8 want[256];
        UInt16 wantLen = len < 0 ? 0 : len > 255 ? 255 : (UInt16)len;
        if (wantLen) ReadBytes(ptr, want, wantLen);
        Rect db = ok ? GetRectAt(list, kLDataBounds) : (Rect){ 0, 0, 0, 0 };
        if (ok && c.v < db.top) { c.v = db.top; c.h = db.left; }
        while (ok && CellIndex(list, c) >= 0) {
            int i = CellIndex(list, c);
            UInt16 clen = CellLength(list, i);
            UInt32 cdata = M68KHeap_Deref(R32(LP(list) + kLCells)) + CellOffset(list, i);
            if (proc) {
                /* FUNCTION Search(aPtr, bPtr: Ptr; aLen, bLen: INTEGER): INTEGER; 0 = a match */
                UInt32 sp = A(7);
                A(7) -= 2; W16(A(7), 1);
                A(7) -= 4; W32(A(7), ptr);
                A(7) -= 4; W32(A(7), cdata);
                A(7) -= 2; W16(A(7), (UInt16)len);
                A(7) -= 2; W16(A(7), clen);
                found = CallProgram(proc) == noErr && R16(A(7)) == 0;
                A(7) = sp;
            } else {
                UInt8 have[256];
                UInt16 n = clen > 255 ? 255 : clen;
                ReadBytes(cdata, have, n);
                found = clen <= 255 && SameText(want, wantLen, have, n);
            }
            if (found) { WritePoint(cellAddr, c); break; }
            if (!NextCell(list, true, true, &c)) break;
        }
        ResultBool(found);
        break;
    }
    case kSelSetSelect: {
        Point c = PopPoint();
        Boolean on = PopBool();
        if (ok) SetSelected(list, c, on);
        break;
    }
    case kSelSize: {
        SInt16 height = (SInt16)Pop16(), width = (SInt16)Pop16();
        if (!ok) break;
        Rect rv = GetRectAt(list, kLRView);
        rv.right = (SInt16)(rv.left + width);
        rv.bottom = (SInt16)(rv.top + height);
        SetRectAt(list, kLRView, &rv);
        ControlHandle v = VBar(list), h = HBar(list);
        if (v) { MoveControl(v, rv.right, (SInt16)(rv.top - 1)); SizeControl(v, 16, (SInt16)(height + 2)); Obj_SyncControl(v); }
        if (h) { MoveControl(h, (SInt16)(rv.left - 1), rv.bottom); SizeControl(h, (SInt16)(width + 2), 16); Obj_SyncControl(h); }
        Recompute(list);
        break;
    }
    case kSelUpdate: {
        RgnHandle rgn = Obj_Rgn(Pop32());
        if (ok && Drawing(list)) DrawList(list, rgn);
        break;
    }
    default: {
        static char why[48];
        snprintf(why, sizeof(why), "List Manager selector %u not implemented", selector);
        gM68KApp->halted = true;
        gM68KApp->lastException = M68K_VEC_LINE_A;
        gM68KApp->faultReason = why;
        gM68KApp->faultPC = gM68KApp->instrPC;
        break;
    }
    }
    Obj_SyncWindows();
    return noErr;
}

const M68KTrapEntry kM68KListTraps[] = {
    { 0xA9E7, Trap_Pack0 },
};
const int kM68KListTrapCount = (int)(sizeof(kM68KListTraps) / sizeof(kM68KListTraps[0]));

/*
 * MacPaintInternal.h - what the parts of MacPaint share
 *
 * MacPaint.c       the application: window, menus, event loop, files
 * MacPaint_Tools.c the document and everything that changes it
 * MacPaint_Draw.c  everything that puts it on the screen
 */

#ifndef MACPAINT_INTERNAL_H
#define MACPAINT_INTERNAL_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"

/* The document: MacPaint's page, 8 by 10 inches at 72 dpi, one bit a pixel,
 * a set bit black. */
enum {
    kPageW = 576,
    kPageH = 720,
    kPageRowBytes = kPageW / 8,
    kPageBytes = kPageRowBytes * kPageH
};

/* The tools, in palette order: two columns, read across then down. The
 * shapes come in pairs, hollow on the left and filled on the right. */
enum {
    kToolLasso, kToolSelect,
    kToolGrabber, kToolText,
    kToolBucket, kToolSpray,
    kToolBrush, kToolPencil,
    kToolLine, kToolEraser,
    kToolRect, kToolRectFill,
    kToolRRect, kToolRRectFill,
    kToolOval, kToolOvalFill,
    kToolFree, kToolFreeFill,
    kToolPoly, kToolPolyFill,
    kToolCount
};

enum { kPatternCount = 38, kLineWidthCount = 5 };

/* The window's content, in its local coordinates */
enum {
    kToolsW = 74,                         /* tool palette column */
    kToolCellW = 36, kToolCellH = 24,
    kLinesTop = 10 * kToolCellH + 12,     /* line widths, below the tools */
    kLineCellH = 14,
    kPatH = 50,                           /* pattern palette, along the bottom */
    kPatCellW = 30, kPatCellH = 22, kPatCols = 19,
    kWinW = kToolsW + kPageW,
    kWinH = 500,
    kViewTop = 0, kViewLeft = kToolsW,
    kViewRight = kWinW, kViewBottom = kWinH - kPatH
};

typedef struct {
    int tool;
    int pattern;                /* index into kPatterns */
    int lineWidth;              /* index into kLineWidths */
    Boolean grid;               /* shapes and selections snap to 8 pixels */
    Boolean fatBits;            /* the view at 8x */
    int viewX, viewY;           /* page point at the view's top left */
} PaintState;

extern PaintState gPaint;
extern UInt8 gPage[kPageBytes];
extern const UInt8 kPatterns[kPatternCount][8];
extern const int kLineWidths[kLineWidthCount];

/* ---- MacPaint_Tools.c ---- */

void Tools_Reset(void);                         /* a blank page, nothing pending */
void Tool_Begin(int x, int y, int modifiers);   /* page coordinates */
void Tool_Move(int x, int y);                   /* button still down */
void Tool_End(int x, int y);
Boolean Tool_WantsHover(void);                  /* polygon under way */
void Tool_Hover(int x, int y);
Boolean Tool_Key(unsigned char ch);             /* text typed on the page */
void Tool_Finish(void);                         /* tool change: settle what is pending */

void Edit_Undo(void);
Boolean Edit_CanUndo(void);
Boolean Edit_HasSelection(void);
Boolean Edit_HasClipboard(void);
void Edit_Cut(void);
void Edit_Copy(void);
void Edit_Paste(void);
void Edit_Clear(void);
void Edit_Invert(void);
void Edit_Fill(void);
void Edit_FlipHorizontal(void);
void Edit_FlipVertical(void);
void Edit_SelectAll(void);
void Edit_EraseView(void);                      /* double-click on the eraser */

Boolean Page_IsBlack(int x, int y);
Boolean Sel_IsEdge(int x, int y);               /* marching-ants outline */
Boolean Text_Caret(int* x, int* top, int* height);

/* The part of the page changed since the last call */
Boolean Page_TakeDirty(Rect* r);
void Page_MarkDirty(const Rect* r);

/* ---- MacPaint_Draw.c ---- */

void Draw_Window(void);                         /* everything; an update */
void Draw_PageArea(const Rect* pageRect);       /* part of the view, now */
void Draw_Palettes(void);                       /* tools, lines, patterns */
void Draw_ViewRect(Rect* r);                    /* the view, local */
void Draw_PageToLocal(int px, int py, int* lx, int* ly);
void Draw_LocalToPage(int lx, int ly, int* px, int* py);
int  Draw_Zoom(void);
void Draw_ScrollTo(int viewX, int viewY);       /* clamped to the page */
int  Draw_ToolAt(int lx, int ly);               /* -1 if none */
int  Draw_LineWidthAt(int lx, int ly);
int  Draw_PatternAt(int lx, int ly);

/* ---- MacPaint.c ---- */

WindowPtr MacPaint_Window(void);
void MacPaint_SetDirty(void);

#endif /* MACPAINT_INTERNAL_H */

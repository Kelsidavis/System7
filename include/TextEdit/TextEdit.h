/*
 * TextEdit.h - System 7.1 TextEdit Manager
 *
 * Text editing APIs with single-style editing and partial styled-text support
 * Based on Inside Macintosh: Text (1993)
 */

#ifndef __TEXTEDIT_H__
#define __TEXTEDIT_H__

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontTypes.h"

/* ============================================================================
 * TextEdit Constants
 * ============================================================================ */

/* Text justification */
#define teJustLeft      0       /* Left justified */
#define teJustCenter    1       /* Center justified */
#define teJustRight     -1      /* Right justified */
#define teForceLeft     -2      /* Force left */

/* Style modes for TESetStyle */
#define doFont          1       /* Set font */
#define doFace          2       /* Set face */
#define doSize          4       /* Set size */
#define doColor         8       /* Set color */
#define doAll           15      /* Set all attributes */
#define addSize         16      /* Add to size */
#define doToggle        32      /* Toggle face */

/* Feature flags */
#define teFAutoScroll   0       /* Auto-scroll during selection */
#define teFTextBuffering 1      /* Text buffering */
#define teFOutlineHilite 2      /* Outline highlighting */
#define teFInlineInput  3       /* Inline input support */
#define teFUseWhiteBackground 4 /* Use white background */

/* Caret/Selection */
#define teCaretWidth    1       /* Caret width in pixels */
#define teDefaultTab    8       /* Default tab width in chars */

/* Limits */
#define teMaxLength     32767   /* Maximum text length */

/* ============================================================================
 * TextEdit Types
 * ============================================================================ */

/* Forward declarations */
typedef struct TERec **TEHandle;
typedef struct TERec *TEPtr;

/* Style run - associates style with text range */
typedef struct StyleRun {
    SInt16      startChar;      /* Starting character position */
    SInt16      styleIndex;     /* Index into style table */
} StyleRun;

typedef struct NullStRec {
    SInt32       teReserved;
    StScrpHandle nullScrap;
} NullStRec;

typedef NullStRec *NullStPtr;
typedef NullStPtr *NullStHandle;

typedef struct STRec {
    SInt16      nRuns;          /* Number of style runs */
    SInt16      nStyles;        /* Number of unique styles */
    Handle      styleTab;       /* Handle to style table */
    Handle      runArray;       /* Handle to run array */
    Handle      lineHeights;    /* Handle to line height array */
    NullStHandle nullStyle;     /* Handle to the null-style record */
} STRec;

/* Line height record */
typedef struct LHElement {
    SInt32      lhHeight;       /* Line height */
    SInt32      lhAscent;       /* Line ascent */
} LHElement;

typedef LHElement *LHPtr, **LHHandle;

/* Click loop callback */
typedef pascal Boolean (*TEClickLoopProcPtr)(TEHandle hTE);

/* High-level hook callback */
typedef pascal void (*TEDoTextProcPtr)(TEHandle hTE, SInt16 firstByte,
                                       SInt16 byteCount, SInt16 selector);

/* Note: Our implementation adds extra fields not in the standard TERec */

/* ============================================================================
 * TextEdit API
 * ============================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization */
extern void TEInit(void);

/* Creation and disposal */
extern TEHandle TENew(const Rect *destRect, const Rect *viewRect);
extern TEHandle TEStyleNew(const Rect *destRect, const Rect *viewRect);
extern void TEDispose(TEHandle hTE);

/* Text manipulation */
extern void TESetText(const void *text, SInt32 length, TEHandle hTE);
extern Handle TEGetText(TEHandle hTE);
extern void TEInsert(const void *text, SInt32 length, TEHandle hTE);
extern void TEDelete(TEHandle hTE);
extern void TEKey(CharParameter key, TEHandle hTE);
extern void TEReplaceSel(const void *text, SInt32 length, TEHandle hTE);

/* Selection */
extern void TESetSelect(SInt32 selStart, SInt32 selEnd, TEHandle hTE);
extern void TEGetSelection(SInt32 *selStart, SInt32 *selEnd, TEHandle hTE);

/* Mouse handling */
extern void TEClick(Point pt, Boolean extendSelection, TEHandle hTE);

/* Clipboard operations */
extern void TECut(TEHandle hTE);
extern void TECopy(TEHandle hTE);
extern void TEPaste(TEHandle hTE);
extern OSErr TEFromScrap(void);
extern OSErr TEToScrap(void);
extern Handle TEScrapHandle(void);

/* Display and update */
extern void TEUpdate(const Rect *updateRect, TEHandle hTE);
extern void TETextBox(const void *text, SInt32 length, const Rect *box, SInt16 just);
extern void TECalText(TEHandle hTE);

/* Scrolling */
extern void TEScroll(SInt16 dh, SInt16 dv, TEHandle hTE);
extern void TESelView(TEHandle hTE);
extern void TEPinScroll(SInt16 dh, SInt16 dv, TEHandle hTE);
extern void TEAutoView(Boolean autoView, TEHandle hTE);

/* Activation */
extern void TEActivate(TEHandle hTE);
extern void TEDeactivate(TEHandle hTE);
extern void TEIdle(TEHandle hTE);

/* Information */
extern SInt16 TEGetHeight(SInt32 endLine, SInt32 startLine, TEHandle hTE);
extern Point TEGetPoint(SInt16 offset, TEHandle hTE);
extern SInt16 TEGetOffset(Point pt, TEHandle hTE);
extern SInt16 TEGetLine(SInt16 offset, TEHandle hTE);

/* Style support (styled TextEdit only) */
extern void TEGetStyle(SInt32 offset, TextStyle *theStyle,
                      SInt16 *lineHeight, SInt16 *fontAscent, TEHandle hTE);
extern void TESetStyle(SInt16 mode, const TextStyle *newStyle,
                      Boolean redraw, TEHandle hTE);
extern void TEUseStyleScrap(SInt32 rangeStart, SInt32 rangeEnd,
                          StScrpHandle newStyles, Boolean redraw, TEHandle hTE);
extern void TEStyleInsert(const void *text, SInt32 length,
                         StScrpHandle hST, TEHandle hTE);
extern void TEStylePaste(TEHandle hTE);


/* Utilities */
extern void TESetJust(SInt16 just, TEHandle hTE);
extern void TESetWordWrap(Boolean wrap, TEHandle hTE);
extern Boolean TEIsActive(TEHandle hTE);

/* Internal helpers (exposed for other TE modules) */
extern void TE_RecalcLines(TEHandle hTE);
extern SInt32 TE_OffsetToLine(TEHandle hTE, SInt32 offset);
extern SInt32 TE_LineToOffset(TEHandle hTE, SInt32 line);
extern void TE_DrawLine(TEHandle hTE, SInt32 lineNum, SInt16 y);
extern void TE_InvalidateSelection(TEHandle hTE);
extern void TE_UpdateCaret(TEHandle hTE, Boolean show);
SInt16 TE_LineInfo(TEHandle hTE, const SInt32** starts);
void TE_GetScroll(TEHandle hTE, SInt16* dh, SInt16* dv);
void TE_SetScroll(TEHandle hTE, SInt16 dh, SInt16 dv);
extern SInt32 TE_FindWordBoundary(TEHandle hTE, SInt32 offset, Boolean forward);
extern SInt32 TE_FindLineStart(TEHandle hTE, SInt32 offset);
extern SInt32 TE_FindLineEnd(TEHandle hTE, SInt32 offset);

/* Application helper entry points (SimpleText shell) */
void TextEdit_InitApp(void);
Boolean TextEdit_IsRunning(void);
void TextEdit_HandleEvent(EventRecord* event);
void TextEdit_LoadFile(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* __TEXTEDIT_H__ */

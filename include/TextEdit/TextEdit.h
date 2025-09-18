/************************************************************
 *
 * TextEdit.h
 * System 7.1 Portable - TextEdit Manager
 *
 * Main TextEdit API - Comprehensive text editing functionality
 * for System 7.1 applications. Provides complete compatibility
 * with Mac OS TextEdit Toolbox Manager.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#ifndef __TEXTEDIT_H__
#define __TEXTEDIT_H__

#ifndef __QUICKDRAW_H__
#include <Quickdraw.h>
#endif

#ifndef __MEMORY_H__
#include <Memory.h>
#endif

#ifndef __TYPES_H__
#include <Types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************
 * TextEdit Constants
 ************************************************************/

/* Justification (word alignment) styles */
enum {
    teJustLeft = 0,
    teJustCenter = 1,
    teJustRight = -1,
    teForceLeft = -2,

    /* New names for justification styles */
    teFlushDefault = 0,     /* flush according to line direction */
    teCenter = 1,           /* center justify (word alignment) */
    teFlushRight = -1,      /* flush right for all scripts */
    teFlushLeft = -2        /* flush left for all scripts */
};

/* Set/Replace style modes */
enum {
    fontBit = 0,           /* set font */
    faceBit = 1,           /* set face */
    sizeBit = 2,           /* set size */
    clrBit = 3,            /* set color */
    addSizeBit = 4,        /* add size mode */
    toggleBit = 5,         /* set faces in toggle mode */
    toglBit = 5            /* obsolete, use toggleBit */
};

/* TESetStyle/TEContinuousStyle modes */
enum {
    doFont = 1,            /* set font (family) number */
    doFace = 2,            /* set character style */
    doSize = 4,            /* set type size */
    doColor = 8,           /* set color */
    doAll = 15,            /* set all attributes */
    addSize = 16,          /* adjust type size */
    doToggle = 32          /* toggle mode for TESetStyle */
};

/* Offsets into TEDispatchRec */
enum {
    EOLHook = 0,           /* [ProcPtr] TEEOLHook */
    DRAWHook = 4,          /* [ProcPtr] TEWidthHook */
    WIDTHHook = 8,         /* [ProcPtr] TEDrawHook */
    HITTESTHook = 12,      /* [ProcPtr] TEHitTestHook */
    nWIDTHHook = 24,       /* [ProcPtr] nTEWidthHook */
    TextWidthHook = 28     /* [ProcPtr] TETextWidthHook */
};

/* Selectors for TECustomHook */
enum {
    intEOLHook = 0,        /* TEIntHook value */
    intDrawHook = 1,       /* TEIntHook value */
    intWidthHook = 2,      /* TEIntHook value */
    intHitTestHook = 3,    /* TEIntHook value */
    intNWidthHook = 6,     /* TEIntHook value for new WidthHook */
    intTextWidthHook = 7   /* TEIntHook value for new TextWidthHook */
};

/* Feature or bit definitions for TEFeatureFlag */
enum {
    teFAutoScroll = 0,     /* 00000001b */
    teFAutoScr = 0,        /* 00000001b obsolete, use teFAutoScroll */
    teFTextBuffering = 1,  /* 00000010b */
    teFOutlineHilite = 2,  /* 00000100b */
    teFInlineInput = 3,    /* 00001000b */
    teFUseTextServices = 4 /* 00010000b */
};

/* Action for TEFeatureFlag interface */
enum {
    TEBitClear = 0,        /* clear the selector bit */
    TEBitSet = 1,          /* set the selector bit */
    TEBitTest = -1,        /* no change; return current setting */

    teBitClear = 0,        /* clear the selector bit */
    teBitSet = 1,          /* set the selector bit */
    teBitTest = -1         /* no change; return current setting */
};

/* Constants for identifying FindWord callers */
enum {
    teWordSelect = 4,      /* clickExpand to select word */
    teWordDrag = 8,        /* clickExpand to drag new word */
    teFromFind = 12,       /* FindLine called it ($0C) */
    teFromRecal = 16       /* RecalLines called it ($10) obsolete */
};

/* Constants for identifying DoText selectors */
enum {
    teFind = 0,            /* DoText called for searching */
    teHighlight = 1,       /* DoText called for highlighting */
    teDraw = -1,           /* DoText called for drawing text */
    teCaret = -2           /* DoText called for drawing caret */
};

/************************************************************
 * TextEdit Data Types
 ************************************************************/

/* Forward declarations */
typedef struct TERec TERec;
typedef TERec *TEPtr, **TEHandle;

/* Hook procedure types */
typedef pascal Boolean (*WordBreakProcPtr)(Ptr text, short charPos);
typedef pascal Boolean (*ClikLoopProcPtr)(void);
typedef pascal void (*TEHookProcPtr)(void);

/* Text data types */
typedef char Chars[32001];
typedef char *CharsPtr, **CharsHandle;
typedef short TEIntHook;

/* Main TextEdit Record */
struct TERec {
    Rect destRect;          /* destination rectangle */
    Rect viewRect;          /* view rectangle */
    Rect selRect;           /* selection rectangle */
    short lineHeight;       /* line height */
    short fontAscent;       /* font ascent */
    Point selPoint;         /* selection point */
    short selStart;         /* selection start */
    short selEnd;           /* selection end */
    short active;           /* active flag */
    WordBreakProcPtr wordBreak;     /* word break procedure */
    ClikLoopProcPtr clikLoop;       /* click loop procedure */
    long clickTime;         /* click time */
    short clickLoc;         /* click location */
    long caretTime;         /* caret time */
    short caretState;       /* caret state */
    short just;             /* justification */
    short teLength;         /* text length */
    Handle hText;           /* text handle */
    long hDispatchRec;      /* dispatch record handle */
    short clikStuff;        /* click stuff */
    short crOnly;           /* carriage return only */
    short txFont;           /* text font */
    Style txFace;           /* text face (unpacked byte) */
    char filler;            /* filler byte */
    short txMode;           /* text mode */
    short txSize;           /* text size */
    GrafPtr inPort;         /* input port */
    ProcPtr highHook;       /* highlight hook */
    ProcPtr caretHook;      /* caret hook */
    short nLines;           /* number of lines */
    short lineStarts[16001]; /* line start array */
};

/* Style run structure */
struct StyleRun {
    short startChar;        /* starting character position */
    short styleIndex;       /* index in style table */
};
typedef struct StyleRun StyleRun;

/* Style table element */
struct STElement {
    short stCount;          /* number of runs in this style */
    short stHeight;         /* line height */
    short stAscent;         /* font ascent */
    short stFont;           /* font (family) number */
    Style stFace;           /* character style */
    char filler;            /* stFace is unpacked byte */
    short stSize;           /* size in points */
    RGBColor stColor;       /* absolute (RGB) color */
};
typedef struct STElement STElement;
typedef STElement TEStyleTable[1777], *STPtr, **STHandle;

/* Line height element */
struct LHElement {
    short lhHeight;         /* maximum height in line */
    short lhAscent;         /* maximum ascent in line */
};
typedef struct LHElement LHElement;
typedef LHElement LHTable[8001], *LHPtr, **LHHandle;

/* Scrap style element */
struct ScrpSTElement {
    long scrpStartChar;     /* starting character position */
    short scrpHeight;       /* height */
    short scrpAscent;       /* ascent */
    short scrpFont;         /* font */
    Style scrpFace;         /* face (unpacked byte) */
    char filler;            /* scrpFace is unpacked byte */
    short scrpSize;         /* size */
    RGBColor scrpColor;     /* color */
};
typedef struct ScrpSTElement ScrpSTElement;
typedef ScrpSTElement ScrpSTTable[1601];

/* Scrap style record */
struct StScrpRec {
    short scrpNStyles;      /* number of styles in scrap */
    ScrpSTTable scrpStyleTab; /* table of styles for scrap */
};
typedef struct StScrpRec StScrpRec;
typedef StScrpRec *StScrpPtr, **StScrpHandle;

/* Null style record */
struct NullStRec {
    long teReserved;        /* reserved for future expansion */
    StScrpHandle nullScrap; /* handle to scrap style table */
};
typedef struct NullStRec NullStRec;
typedef NullStRec *NullStPtr, **NullStHandle;

/* TextEdit style record */
struct TEStyleRec {
    short nRuns;            /* number of style runs */
    short nStyles;          /* size of style table */
    STHandle styleTab;      /* handle to style table */
    LHHandle lhTab;         /* handle to line-height table */
    long teRefCon;          /* reserved for application use */
    NullStHandle nullStyle; /* handle to style set at null selection */
    StyleRun runs[8001];    /* array of style runs */
};
typedef struct TEStyleRec TEStyleRec;
typedef TEStyleRec *TEStylePtr, **TEStyleHandle;

/* Text style */
struct TextStyle {
    short tsFont;           /* font (family) number */
    Style tsFace;           /* character style */
    char filler;            /* tsFace is unpacked byte */
    short tsSize;           /* size in points */
    RGBColor tsColor;       /* absolute (RGB) color */
};
typedef struct TextStyle TextStyle;
typedef TextStyle *TextStylePtr, **TextStyleHandle;

/************************************************************
 * Core TextEdit Functions
 ************************************************************/

/* Initialization */
pascal void TEInit(void);

/* TextEdit Record Management */
pascal TEHandle TENew(const Rect *destRect, const Rect *viewRect);
pascal TEHandle TEStyleNew(const Rect *destRect, const Rect *viewRect);
pascal TEHandle TEStylNew(const Rect *destRect, const Rect *viewRect); /* synonym */
pascal void TEDispose(TEHandle hTE);

/* Text Access */
pascal void TESetText(const void *text, long length, TEHandle hTE);
pascal CharsHandle TEGetText(TEHandle hTE);

/* Selection Management */
pascal void TESetSelect(long selStart, long selEnd, TEHandle hTE);
pascal void TEClick(Point pt, Boolean fExtend, TEHandle h);
pascal short TEGetOffset(Point pt, TEHandle hTE);
pascal Point TEGetPoint(short offset, TEHandle hTE);

/* Activation */
pascal void TEActivate(TEHandle hTE);
pascal void TEDeactivate(TEHandle hTE);
pascal void TEIdle(TEHandle hTE);

/* Text Input */
pascal void TEKey(short key, TEHandle hTE);
pascal void TEInsert(const void *text, long length, TEHandle hTE);
pascal void TEDelete(TEHandle hTE);

/* Clipboard Operations */
pascal void TECut(TEHandle hTE);
pascal void TECopy(TEHandle hTE);
pascal void TEPaste(TEHandle hTE);

/* Display and Scrolling */
pascal void TEUpdate(const Rect *rUpdate, TEHandle hTE);
pascal void TEScroll(short dh, short dv, TEHandle hTE);
pascal void TESelView(TEHandle hTE);
pascal void TEPinScroll(short dh, short dv, TEHandle hTE);
pascal void TEAutoView(Boolean fAuto, TEHandle hTE);

/* Text Formatting */
pascal void TESetAlignment(short just, TEHandle hTE);
pascal void TESetJust(short just, TEHandle hTE);
pascal void TECalText(TEHandle hTE);
pascal void TETextBox(const void *text, long length, const Rect *box, short just);
pascal void TextBox(const void *text, long length, const Rect *box, short just);

/* Style Management */
pascal void TESetStyleHandle(TEStyleHandle theHandle, TEHandle hTE);
pascal void SetStyleHandle(TEStyleHandle theHandle, TEHandle hTE); /* synonym */
pascal void SetStylHandle(TEStyleHandle theHandle, TEHandle hTE);  /* synonym */
pascal TEStyleHandle TEGetStyleHandle(TEHandle hTE);
pascal TEStyleHandle GetStyleHandle(TEHandle hTE); /* synonym */
pascal TEStyleHandle GetStylHandle(TEHandle hTE);  /* synonym */

pascal void TEGetStyle(short offset, TextStyle *theStyle, short *lineHeight,
                      short *fontAscent, TEHandle hTE);
pascal void TESetStyle(short mode, const TextStyle *newStyle, Boolean fRedraw,
                      TEHandle hTE);
pascal void TEReplaceStyle(short mode, const TextStyle *oldStyle,
                          const TextStyle *newStyle, Boolean fRedraw, TEHandle hTE);
pascal Boolean TEContinuousStyle(short *mode, TextStyle *aStyle, TEHandle hTE);

/* Style Scrap Operations */
pascal StScrpHandle TEGetStyleScrapHandle(TEHandle hTE);
pascal StScrpHandle GetStyleScrap(TEHandle hTE); /* synonym */
pascal StScrpHandle GetStylScrap(TEHandle hTE);  /* synonym */
pascal void TEStyleInsert(const void *text, long length, StScrpHandle hST,
                         TEHandle hTE);
pascal void TEStylInsert(const void *text, long length, StScrpHandle hST,
                        TEHandle hTE); /* synonym */
pascal void TEStylePaste(TEHandle hTE);
pascal void TEStylPaste(TEHandle hTE); /* synonym */
pascal void TEUseStyleScrap(long rangeStart, long rangeEnd, StScrpHandle newStyles,
                           Boolean fRedraw, TEHandle hTE);
pascal void SetStyleScrap(long rangeStart, long rangeEnd, StScrpHandle newStyles,
                         Boolean redraw, TEHandle hTE); /* synonym */
pascal void SetStylScrap(long rangeStart, long rangeEnd, StScrpHandle newStyles,
                        Boolean redraw, TEHandle hTE);  /* synonym */

/* Advanced Functions */
pascal long TEGetHeight(long endLine, long startLine, TEHandle hTE);
pascal long TENumStyles(long rangeStart, long rangeEnd, TEHandle hTE);
pascal void TECustomHook(TEIntHook which, ProcPtr *addr, TEHandle hTE);
pascal short TEFeatureFlag(short feature, short action, TEHandle hTE);

/* Hook Functions */
pascal void TESetClickLoop(ClikLoopProcPtr clikProc, TEHandle hTE);
pascal void SetClikLoop(ClikLoopProcPtr clikProc, TEHandle hTE); /* synonym */
pascal void TESetWordBreak(WordBreakProcPtr wBrkProc, TEHandle hTE);
pascal void SetWordBreak(WordBreakProcPtr wBrkProc, TEHandle hTE); /* synonym */

/* Scrap Functions */
#define TEScrapHandle() (* (Handle*) 0xAB4)
#define TEGetScrapLength() ((long) * (unsigned short *) 0x0AB0)
#define TEGetScrapLen() ((long) * (unsigned short *) 0x0AB0)
pascal void TESetScrapLength(long length);
pascal void TESetScrapLen(long length);
pascal OSErr TEFromScrap(void);
pascal OSErr TEToScrap(void);

/* C interface helper */
void teclick(Point *pt, Boolean fExtend, TEHandle h);

/************************************************************
 * Internal/Platform Abstraction Functions
 ************************************************************/

/* Modern platform integration */
OSErr TEInitPlatform(void);
void TECleanupPlatform(void);

/* Unicode/multi-byte character support */
OSErr TESetTextEncoding(TEHandle hTE, TextEncoding encoding);
TextEncoding TEGetTextEncoding(TEHandle hTE);

/* Modern input method support */
OSErr TESetInputMethod(TEHandle hTE, Boolean useModernIM);
Boolean TEGetInputMethod(TEHandle hTE);

/* Accessibility support */
OSErr TESetAccessibilityEnabled(TEHandle hTE, Boolean enabled);
Boolean TEGetAccessibilityEnabled(TEHandle hTE);

#ifdef __cplusplus
}
#endif

#endif /* __TEXTEDIT_H__ */
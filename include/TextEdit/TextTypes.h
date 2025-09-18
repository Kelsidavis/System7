/************************************************************
 *
 * TextTypes.h
 * System 7.1 Portable - TextEdit Types and Data Structures
 *
 * Internal data structures and type definitions for TextEdit
 * implementation. Contains private structures and constants
 * used internally by the TextEdit Manager.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#ifndef __TEXTTYPES_H__
#define __TEXTTYPES_H__

#ifndef __TYPES_H__
#include <Types.h>
#endif

#ifndef __QUICKDRAW_H__
#include <Quickdraw.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************
 * Internal TextEdit Constants
 ************************************************************/

/* Private offsets into TEDispatchRec */
#define newTEFlags          16    /* teFAutoScr, teFTextBuffering, teFOutlineHilite */
#define TwoByteCharBuffer   20    /* storage for buffered double-byte character */
#define lastScript          22    /* keep last script for split/single caret display */

/* Text measure overflow flag */
#define measOverFlow        0     /* text measure overflow (00000001b) */

/* Size of TEDispatchRec */
#define intDispSize         32

/* Style constants */
#define teStylSize          28    /* initial size of teStylesRec with 2 entries */
#define stBaseSize          20    /* added to fix MakeRoom bug */

/* Extended style modes */
enum {
    doAll2 = 0x1F,              /* all of the above + add size */
    doAll3 = 0x3F               /* all of the above + toggle faces */
};

/* Text buffer size */
#define BufferSize          50    /* max num of bytes for text buffer */

/* Defined key characters */
#define downArrowChar       0x1F
#define upArrowChar         0x1E
#define rightArrowChar      0x1D
#define leftArrowChar       0x1C
#define backspaceChar       0x08
#define returnChar          0x0D
#define tabChar             0x09

/* Internal constants */
#define lnStStartSize       2     /* stStartSize = 4; bit shift left of 2 */
#define UnboundLeft         0x8002 /* unbound left edge for rectangle */
#define UnboundRight        0x7FFE /* unbound right edge for rectangle */
#define numStyleRuns        10    /* max # for local frame; if more, allocate handle */
#define defBufSize          256   /* initial size of temp line start buffer */

/************************************************************
 * Internal Data Structures
 ************************************************************/

/* TEDispatchRec structure - internal dispatch table */
typedef struct TEDispatchRec {
    ProcPtr EOLHook;            /* End of line hook */
    ProcPtr DRAWHook;           /* Drawing hook */
    ProcPtr WIDTHHook;          /* Width calculation hook */
    ProcPtr HITTESTHook;        /* Hit testing hook */
    short newTEFlags;           /* Feature flags */
    short TwoByteCharBuffer;    /* Double-byte character buffer */
    char lastScript;            /* Last script for caret display */
    char padding1;              /* Padding */
    ProcPtr nWIDTHHook;         /* New width hook */
    ProcPtr TextWidthHook;      /* Text width hook */
} TEDispatchRec, *TEDispatchPtr, **TEDispatchHandle;

/* Format ordering frame for styled text */
typedef struct FmtOrderFrame {
    long firstStyleRun;         /* First style run */
    long secondStyleRun;        /* Second style run */
    long startStyleRun;         /* Starting style run */
    TEPtr teRecPtr;             /* Pointer to TE record */
    short LRTextFlag;           /* Left-to-right text flag */
    short numberOfRuns;         /* Number of style runs on current line */
    short styleRunIndex;        /* Style run index */
    short fmtOrdering[numStyleRuns]; /* Format ordering array */
    long fmtOrderingPtr;        /* Format ordering pointer */
    long a2StyleRun;            /* Style run at a2 */
    short fmtOrderingIndex;     /* Format ordering index */
} FmtOrderFrame;

/* Standard frame for text processing */
typedef struct TextFrame {
    long a6Link;                /* Stack frame link */
    short saveLeft;             /* True left */
    short saveJust;             /* Left setting after justification */
    short useOldMethod;         /* Use old method flag */
    FmtOrderFrame TEFormatOrder; /* Format ordering frame */
} TextFrame;

/* Shared frame for text operations */
typedef struct SharedFrame {
    long a6Link;                /* Stack frame link */
    long dummy;                 /* Space filler */
    char dummyByte;             /* Named space filler */
    char doneFlag;              /* Done flag */
    FmtOrderFrame TEFormatOrder; /* Format ordering frame */
    /* Selection rectangle */
    short selRectR;             /* Right */
    short selRectB;             /* Bottom */
    short selRectL;             /* Left */
    short selRectT;             /* Top */
    /* Caret display storage */
    char cursorScriptDir;       /* Cursor script direction */
    char previousScriptDir;     /* Previous script direction */
    char scriptDirection;       /* Script direction */
    char prevStyleFlag;         /* Previous style flag */
    char onBoundaryFlag;        /* On boundary flag */
    char gotOneRun;             /* Got one run flag */
    char lineEndFlag;           /* Line end flag */
    char highCaret;             /* High caret flag */
    /* OnSameLine storage */
    long d3StyleRun;            /* Style run at d3 */
    long d4StyleRun;            /* Style run at d4 */
    short totalWidth;           /* Total width */
    char foundD3;               /* Found d3 flag */
    char foundD4;               /* Found d4 flag */
    short d3FmtIndex;           /* d3 format index */
    short d4FmtIndex;           /* d4 format index */
    char sameStyle;             /* Same style flag */
    char hiliteInbetween;       /* Highlight in between flag */
    short saveD3;               /* Saved d3 */
    short saveD4;               /* Saved d4 */
    short anchorL;              /* Anchor left */
    short anchorR;              /* Anchor right */
    long offPairs[3];           /* Offset pairs */
    char hiliteLEndFlag;        /* Highlight left end flag */
    char hiliteREndFlag;        /* Highlight right end flag */
} SharedFrame;

/* Frame for recalculate lines */
typedef struct RecalFrame {
    long a6Link;                /* Stack frame link */
    short savedD4;              /* Saved D4 register */
    short savedD2;              /* Saved D2 register */
    Handle startHndl;           /* Start handle */
    Ptr savePtr;                /* Save pointer */
    short oldNLines;            /* Old number of lines */
} RecalFrame;

/* Frame for GetCurScript */
typedef struct GetCurScriptFrame {
    long a6Link;                /* Stack frame link */
    long gcsTemp;               /* Temporary storage */
    short gcsIndex;             /* Index */
} GetCurScriptFrame;

/* Frame for FindWord */
typedef struct FindWordFrame {
    long a6Link;                /* Stack frame link */
    long fwOffTab[3];           /* Offset table */
    long fwSelWrap;             /* Selection wrap */
    Ptr fwSRTxtPtr;             /* Style run text pointer */
    long fwSavedD2;             /* Saved D2 register */
    short fwLeadingEdge;        /* Leading edge */
    short fwSROffset;           /* Style run offset */
    short fwSRLength;           /* Style run length */
    short fwSRFont;             /* Style run font */
    short fwSRIndx;             /* Style run index */
    short fwSRStrtCh;           /* Style run start character */
} FindWordFrame;

/* Frame for outline highlighting */
typedef struct OutlineFrame {
    long a6Link;                /* Stack frame link */
    char oldPenState[18];       /* Old pen state (18 bytes) */
    ProcPtr oldHighHook;        /* Old highlight hook */
    ProcPtr oldCaretHook;       /* Old caret hook */
    char paintFlag;             /* Paint flag */
    char dummy;                 /* Padding */
} OutlineFrame;

/* Frame for cursor operations */
typedef struct CursorFrame {
    long a6Link;                /* Stack frame link */
    short dummies[3];           /* Space fillers */
    FmtOrderFrame TEFormatOrder; /* Format ordering frame */
    char FormatEndFlag;         /* Format end flag */
    short RunDirection;         /* Run direction */
} CursorFrame;

/* Frame for line breaking */
typedef struct LineBreakFrame {
    long oldA6;                 /* Old A6 link */
    long length;                /* Length parameter */
    long width;                 /* Width parameter */
    long offset;                /* Offset parameter */
    long start;                 /* Start parameter */
    long end;                   /* End parameter */
    long currStyle;             /* Current style */
    long nextStyle;             /* Next style */
    Ptr textPtr;                /* Text pointer */
    Ptr initlTextPtr;           /* Initial text pointer */
    short textPtrOffset;        /* Text pointer offset */
    short lineStart;            /* Line start */
    short EOLCharPosn;          /* End of line character position */
    short styleIndex;           /* Style index */
} LineBreakFrame;

/* Frame for pixel to character conversion */
typedef struct P2CFrame {
    long returnAddr;            /* Return address */
    long a6Link;                /* Stack frame link */
    short leadingEdge;          /* Leading edge (was leftSide) */
    short textLen;              /* Text length */
    Ptr textPtr;                /* Text pointer */
    short pixelWidth;           /* Pixel width */
    short slop;                 /* Slop */
    short txtWidth;             /* Text width */
    long widthRemaining;        /* Width remaining */
} P2CFrame;

/* Frame for highlighting */
typedef struct HiliteFrame {
    long a6Link;                /* Stack frame link */
    short savD4;                /* Saved D4 register */
    Ptr d3Line;                 /* Pointer to line start for d3 */
    Ptr d4Line;                 /* Pointer to line start for d4 */
} HiliteFrame;

/************************************************************
 * Internal State Management
 ************************************************************/

/* TextEdit global state */
typedef struct TEGlobals {
    Handle TEScrapHandle;       /* Scrap handle */
    long TEScrapLength;         /* Scrap length */
    short TELastScript;         /* Last script */
    Boolean TEInited;           /* Initialization flag */
    Boolean TEPlatformInited;   /* Platform initialization flag */

    /* Modern extensions */
    Boolean TEUnicodeSupport;   /* Unicode support enabled */
    Boolean TEAccessibilityMode; /* Accessibility mode */
    TextEncoding TEDefaultEncoding; /* Default text encoding */
} TEGlobals;

/* Platform-specific text input state */
typedef struct TEInputState {
    Boolean modernInput;        /* Use modern input methods */
    Boolean compositionActive;  /* Text composition in progress */
    long compositionStart;      /* Start of composition range */
    long compositionEnd;        /* End of composition range */
    Handle compositionText;     /* Composition text buffer */
} TEInputState;

/* Undo information */
typedef struct TEUndoInfo {
    short undoType;             /* Type of undo operation */
    long undoStart;             /* Start of undo range */
    long undoEnd;               /* End of undo range */
    Handle undoText;            /* Undo text */
    StScrpHandle undoStyles;    /* Undo styles */
} TEUndoInfo;

/************************************************************
 * Internal Function Prototypes
 ************************************************************/

/* Internal utility functions */
TEDispatchHandle TECreateDispatchRec(void);
void TEDisposeDispatchRec(TEDispatchHandle hDispatch);
OSErr TEValidateRecord(TEHandle hTE);
OSErr TERecalLines(TEHandle hTE, short startLine);
void TEUpdateLineStarts(TEHandle hTE);

/* Style management internals */
OSErr TEInitStyles(TEHandle hTE);
OSErr TEDisposeStyles(TEHandle hTE);
OSErr TEStyleRunFromOffset(TEHandle hTE, long offset, StyleRun **styleRun);
short TEStyleIndexFromOffset(TEHandle hTE, long offset);

/* Text measurement and layout */
short TECharWidth(char ch, short font, short size, Style face);
short TETextWidth(Ptr textPtr, short textLen, short font, short size, Style face);
Point TEOffsetToPoint(TEHandle hTE, long offset);
long TEPointToOffset(TEHandle hTE, Point pt);

/* Selection and highlighting */
void TEHiliteRange(TEHandle hTE, long startSel, long endSel, Boolean hilite);
void TEDrawCaret(TEHandle hTE);
void TEHideCaret(TEHandle hTE);
void TEShowCaret(TEHandle hTE);

/* Platform abstraction internals */
OSErr TEInitPlatformInput(void);
void TECleanupPlatformInput(void);
OSErr TEPlatformHandleKeyEvent(TEHandle hTE, short key, long modifiers);
OSErr TEPlatformUpdateDisplay(TEHandle hTE, const Rect *updateRect);

#ifdef __cplusplus
}
#endif

#endif /* __TEXTTYPES_H__ */
/*
 * WindowTypes.h - Window Manager Type Definitions
 *
 * This header defines all data structures, constants, and type definitions
 * used by the Window Manager. It provides complete type compatibility with
 * the original Apple Macintosh System 7.1 Window Manager.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __WINDOW_TYPES_H__
#define __WINDOW_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct WindowRecord* WindowPtr;
typedef struct WindowRecord* WindowPeek;
typedef struct CWindowRecord* CWindowPtr;
typedef struct CWindowRecord* CWindowPeek;
typedef struct AuxWinRec** AuxWinHandle;
typedef struct ColorTable** CTabHandle;
typedef struct PixPat** PixPatHandle;

/* ============================================================================
 * Basic Mac OS Types
 * ============================================================================ */

typedef unsigned char Boolean;
typedef signed char SignedByte;
typedef unsigned char Byte;
typedef signed short SInt16;
typedef unsigned short UInt16;
typedef signed long SInt32;
typedef unsigned long UInt32;
typedef void* Ptr;
typedef Ptr* Handle;
typedef unsigned char Str255[256];
typedef const unsigned char* ConstStr255Param;
typedef Handle StringHandle;

#ifndef true
#define true 1
#define false 0
#endif

/* ============================================================================
 * Geometry Types
 * ============================================================================ */

typedef struct Point {
    short v;    /* vertical coordinate */
    short h;    /* horizontal coordinate */
} Point;

typedef struct Rect {
    short top;
    short left;
    short bottom;
    short right;
} Rect;

typedef struct Region** RgnHandle;

/* ============================================================================
 * Graphics Types
 * ============================================================================ */

typedef struct Pattern {
    unsigned char pat[8];
} Pattern;

typedef struct GrafPort {
    short device;
    Rect portRect;
    RgnHandle visRgn;
    RgnHandle clipRgn;
    Pattern bkPat;
    Pattern fillPat;
    Point pnLoc;
    Point pnSize;
    short pnMode;
    Pattern pnPat;
    short pnVis;
    short txFont;
    unsigned char txFace;
    unsigned char txMode;
    short txSize;
    long spExtra;
    long fgColor;
    long bkColor;
    short colrBit;
    short patStretch;
    Handle picSave;
    Handle rgnSave;
    Handle polySave;
    Ptr grafProcs;
} GrafPort;

typedef GrafPort* GrafPtr;

typedef struct CGrafPort {
    short device;
    Rect portRect;
    RgnHandle visRgn;
    RgnHandle clipRgn;
    Pattern bkPat;
    long rgbFgColor;
    long rgbBkColor;
    Point pnLoc;
    Point pnSize;
    short pnMode;
    Pattern pnPat;
    short pnVis;
    short txFont;
    unsigned char txFace;
    short txMode;
    short txSize;
    long spExtra;
    Ptr grafProcs;
    long portVersion;
    Handle grafVars;
    short chExtra;
    short pnLocHFrac;
    Rect portRect2;
    RgnHandle visRgn2;
    RgnHandle clipRgn2;
    Handle pixMap;
    Handle pmFgColor;
    Handle pmBkColor;
    Handle pmFgIndex;
    Handle pmBkIndex;
} CGrafPort;

typedef CGrafPort* CGrafPtr;

typedef struct Picture** PicHandle;

typedef struct ColorSpec {
    short value;
    short rgb[3];
} ColorSpec;

/* ============================================================================
 * Control and Event Types
 * ============================================================================ */

typedef struct ControlRecord** ControlHandle;

typedef struct EventRecord {
    short what;
    long message;
    long when;
    Point where;
    short modifiers;
} EventRecord;

/* ============================================================================
 * Window Definition Constants
 * ============================================================================ */

/* Window definition procedure IDs */
enum {
    documentProc = 0,       /* Standard document window */
    dBoxProc = 1,          /* Modal dialog box */
    plainDBox = 2,         /* Plain box */
    altDBoxProc = 3,       /* Alert box */
    noGrowDocProc = 4,     /* Document without grow box */
    movableDBoxProc = 5,   /* Movable modal dialog */
    zoomDocProc = 8,       /* Document with zoom box */
    zoomNoGrow = 12,       /* Zoomable without grow */
    rDocProc = 16          /* Rounded-corner document */
};

/* Window kinds */
enum {
    dialogKind = 2,        /* Dialog window */
    userKind = 8,          /* User application window */
    systemKind = -1,       /* System window */
    deskKind = -2          /* Desktop */
};

/* FindWindow result codes */
enum {
    inDesk = 0,            /* Desktop */
    inMenuBar = 1,         /* Menu bar */
    inSysWindow = 2,       /* System window */
    inContent = 3,         /* Window content area */
    inDrag = 4,            /* Window title bar (drag area) */
    inGrow = 5,            /* Window grow box */
    inGoAway = 6,          /* Window close box */
    inZoomIn = 7,          /* Window zoom box (zoom in) */
    inZoomOut = 8          /* Window zoom box (zoom out) */
};

/* Window messages for WDEF */
enum {
    wDraw = 0,             /* Draw window frame */
    wHit = 1,              /* Hit test */
    wCalcRgns = 2,         /* Calculate regions */
    wNew = 3,              /* Initialize window */
    wDispose = 4,          /* Dispose window */
    wGrow = 5,             /* Draw grow image */
    wDrawGIcon = 6         /* Draw grow icon */
};

/* Window part codes for WDEF hit testing */
enum {
    wNoHit = 0,            /* Not in window */
    wInContent = 1,        /* In content region */
    wInDrag = 2,           /* In drag region */
    wInGrow = 3,           /* In grow region */
    wInGoAway = 4,         /* In close box */
    wInZoomIn = 5,         /* In zoom in box */
    wInZoomOut = 6         /* In zoom out box */
};

/* Window color table part identifiers */
enum {
    wContentColor = 0,     /* Content area color */
    wFrameColor = 1,       /* Window frame color */
    wTextColor = 2,        /* Text color */
    wHiliteColor = 3,      /* Highlight color */
    wTitleBarColor = 4     /* Title bar color */
};

/* Desktop pattern ID */
enum {
    deskPatID = 16         /* Standard desktop pattern resource ID */
};

/* Floating window kinds (System 7.1 extension) */
enum {
    applicationFloatKind = 1024,  /* Application floating window */
    systemFloatKind = 1025        /* System floating window */
};

/* ============================================================================
 * Window Data Structures
 * ============================================================================ */

/* Window state data for zooming */
typedef struct WStateData {
    Rect userState;        /* User-defined position/size */
    Rect stdState;         /* Standard (zoomed) position/size */
} WStateData;

typedef WStateData* WStateDataPtr;
typedef WStateData** WStateDataHandle;

/* Window record - black & white windows */
struct WindowRecord {
    GrafPort port;                      /* Window's grafport */
    short windowKind;                   /* Window type (userKind, dialogKind, etc.) */
    Boolean visible;                    /* TRUE if window is visible */
    Boolean hilited;                    /* TRUE if window is highlighted/active */
    Boolean goAwayFlag;                 /* TRUE if window has close box */
    Boolean spareFlag;                  /* Reserved for future use */
    RgnHandle strucRgn;                 /* Structure region (includes frame) */
    RgnHandle contRgn;                  /* Content region (excludes frame) */
    RgnHandle updateRgn;                /* Update region (needs redrawing) */
    Handle windowDefProc;               /* Window definition procedure handle */
    Handle dataHandle;                  /* Window definition data */
    StringHandle titleHandle;           /* Window title string */
    short titleWidth;                   /* Title width in pixels */
    ControlHandle controlList;          /* First control in window */
    struct WindowRecord* nextWindow;    /* Next window in chain */
    PicHandle windowPic;                /* Window picture for auto-drawing */
    long refCon;                        /* Application reference constant */
};

/* Color window record - extends WindowRecord for color */
struct CWindowRecord {
    CGrafPort port;                     /* Window's color grafport */
    short windowKind;                   /* Window type */
    Boolean visible;                    /* TRUE if window is visible */
    Boolean hilited;                    /* TRUE if window is highlighted/active */
    Boolean goAwayFlag;                 /* TRUE if window has close box */
    Boolean spareFlag;                  /* Reserved for future use */
    RgnHandle strucRgn;                 /* Structure region (includes frame) */
    RgnHandle contRgn;                  /* Content region (excludes frame) */
    RgnHandle updateRgn;                /* Update region (needs redrawing) */
    Handle windowDefProc;               /* Window definition procedure handle */
    Handle dataHandle;                  /* Window definition data */
    StringHandle titleHandle;           /* Window title string */
    short titleWidth;                   /* Title width in pixels */
    ControlHandle controlList;          /* First control in window */
    struct CWindowRecord* nextWindow;   /* Next window in chain */
    PicHandle windowPic;                /* Window picture for auto-drawing */
    long refCon;                        /* Application reference constant */
};

/* Auxiliary window record for color information */
struct AuxWinRec {
    struct AuxWinRec** awNext;          /* Handle to next auxiliary record */
    WindowPtr awOwner;                  /* Pointer to owning window */
    CTabHandle awCTable;                /* Color table for window parts */
    Handle dialogCItem;                 /* Dialog Manager structures */
    long awFlags;                       /* Flags for window attributes */
    CTabHandle awReserved;              /* Reserved for future expansion */
    long awRefCon;                      /* Auxiliary reference constant */
};

typedef struct AuxWinRec AuxWinRec;
typedef AuxWinRec* AuxWinPtr;

/* Window color table */
typedef struct WinCTab {
    long wCSeed;                        /* Color table seed */
    short wCReserved;                   /* Reserved field */
    short ctSize;                       /* Number of entries (usually 4) */
    ColorSpec ctTable[5];               /* Color specifications */
} WinCTab;

typedef WinCTab* WCTabPtr;
typedef WinCTab** WCTabHandle;

/* ============================================================================
 * Function Pointer Types
 * ============================================================================ */

/* Window definition procedure */
typedef long (*WindowDefProcPtr)(short varCode, WindowPtr theWindow,
                                short message, long param);

/* Drag gray region callback */
typedef void (*DragGrayRgnProcPtr)(void);

/* ============================================================================
 * Window Manager State Types
 * ============================================================================ */

/* Window Manager port structure */
typedef struct WMgrPort {
    GrafPort port;                      /* Base graphics port */
    WindowPtr windowList;               /* Head of window list */
    WindowPtr activeWindow;             /* Currently active window */
    WindowPtr ghostWindow;              /* Window being dragged */
    Pattern deskPattern;                /* Desktop background pattern */
    short menuBarHeight;                /* Height of menu bar */
    Rect grayRgn;                       /* Gray region (screen bounds) */
    Boolean initialized;                /* TRUE when Window Manager is ready */
} WMgrPort;

/* Window list entry for internal management */
typedef struct WindowListEntry {
    WindowPtr window;
    struct WindowListEntry* next;
    struct WindowListEntry* prev;
} WindowListEntry;

/* Complete Window Manager state */
typedef struct WindowManagerState {
    WMgrPort* wMgrPort;                 /* Window Manager port */
    CGrafPtr wMgrCPort;                 /* Color Window Manager port */
    WindowPtr windowList;               /* Head of window list */
    WindowPtr activeWindow;             /* Currently active window */
    AuxWinHandle auxWinHead;            /* Head of auxiliary window list */
    Pattern desktopPattern;             /* Desktop pattern */
    PixPatHandle desktopPixPat;         /* Color desktop pattern */
    short nextWindowID;                 /* Next window resource ID */
    Boolean colorQDAvailable;           /* TRUE if Color QuickDraw available */
    Boolean initialized;                /* TRUE when initialized */

    /* Platform-specific data */
    void* platformData;                 /* Platform window system data */
} WindowManagerState;

/* ============================================================================
 * Extended Window Attributes (System 7.1)
 * ============================================================================ */

/* Window attributes for modern features */
typedef UInt32 WindowAttributes;

enum {
    kWindowCloseBoxAttribute = (1 << 0),
    kWindowHorizontalZoomAttribute = (1 << 1),
    kWindowVerticalZoomAttribute = (1 << 2),
    kWindowFullZoomAttribute = (1 << 3),
    kWindowCollapseBoxAttribute = (1 << 4),
    kWindowResizableAttribute = (1 << 5),
    kWindowSideTitlebarAttribute = (1 << 6),
    kWindowToolbarButtonAttribute = (1 << 7),
    kWindowMetalAttribute = (1 << 8),
    kWindowNoUpdatesAttribute = (1 << 16),
    kWindowNoActivateAttribute = (1 << 17),
    kWindowOpaqueForEventsAttribute = (1 << 18),
    kWindowCompositingAttribute = (1 << 19),
    kWindowNoShadowAttribute = (1 << 20),
    kWindowHideOnSuspendAttribute = (1 << 21),
    kWindowStandardHandlerAttribute = (1 << 22),
    kWindowHideOnFullScreenAttribute = (1 << 23),
    kWindowInWindowMenuAttribute = (1 << 24),
    kWindowLiveResizeAttribute = (1 << 25),
    kWindowIgnoreClicksAttribute = (1 << 26),
    kWindowNoConstrainAttribute = (1 << 27),
    kWindowStandardDocumentAttributes = (kWindowCloseBoxAttribute |
                                       kWindowFullZoomAttribute |
                                       kWindowCollapseBoxAttribute |
                                       kWindowResizableAttribute),
    kWindowStandardFloatingAttributes = (kWindowCloseBoxAttribute |
                                       kWindowCollapseBoxAttribute)
};

/* Window classes for layering */
typedef UInt32 WindowClass;

enum {
    kAlertWindowClass = 1,
    kMovableAlertWindowClass = 2,
    kModalWindowClass = 3,
    kMovableModalWindowClass = 4,
    kFloatingWindowClass = 5,
    kDocumentWindowClass = 6,
    kUtilityWindowClass = 8,
    kHelpWindowClass = 10,
    kSheetWindowClass = 11,
    kToolbarWindowClass = 12,
    kPlainWindowClass = 13,
    kOverlayWindowClass = 14,
    kSheetAlertWindowClass = 15,
    kAltPlainWindowClass = 16,
    kDrawerWindowClass = 20,
    kAllWindowClasses = 0xFFFFFFFF
};

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/* Rectangle utilities */
#define EmptyRect(r) ((r)->left >= (r)->right || (r)->top >= (r)->bottom)
#define EqualRect(r1, r2) ((r1)->left == (r2)->left && (r1)->top == (r2)->top && \
                          (r1)->right == (r2)->right && (r1)->bottom == (r2)->bottom)

/* Point utilities */
#define EqualPt(p1, p2) ((p1).h == (p2).h && (p1).v == (p2).v)

/* Window utilities */
#define GetWindowPort(w) ((GrafPtr)(w))
#define GetWindowFromPort(p) ((WindowPtr)(p))
#define IsWindowVisible(w) ((w) && (w)->visible)
#define IsWindowHilited(w) ((w) && (w)->hilited)

/* Type checking macros */
#define IsWindowPtr(w) ((w) != NULL)
#define IsColorWindow(w) (sizeof(*(w)) == sizeof(CWindowRecord))

#ifdef __cplusplus
}
#endif

#endif /* __WINDOW_TYPES_H__ */
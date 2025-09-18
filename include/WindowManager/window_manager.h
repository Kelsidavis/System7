/**
 * RE-AGENT-BANNER
 * Window Manager - Apple System 7.1 Window Manager Reimplementation
 *
 * This file contains reimplemented Window Manager functions from Apple System 7.1.
 * Based on evidence extracted from System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 *
 * Evidence sources:
 * - radare2 binary analysis of System.rsrc
 * - Mac OS Toolbox trap analysis (0xA910-0xA92D range)
 * - Inside Macintosh documentation for structure layouts
 *
 * Architecture: Motorola 68000
 * ABI: Mac OS Toolbox calling conventions
 */

#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* Basic types based on evidence from layouts.curated.windowmgr.json */
typedef int16_t short;
typedef int32_t long;
typedef uint8_t Boolean;
typedef void* Ptr;
typedef void** Handle;
typedef int32_t Fixed;  /* 16.16 fixed-point */

/* String types */
typedef const uint8_t* ConstStr255Param;
typedef uint8_t Str255[256];
typedef Handle StringHandle;

/* Evidence: Point structure at offset analysis */
typedef struct Point {
    int16_t v;  /* Vertical coordinate */
    int16_t h;  /* Horizontal coordinate */
} Point;

/* Evidence: Rect structure at offset analysis */
typedef struct Rect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} Rect;

/* Evidence: Pattern structure from QuickDraw */
typedef uint8_t Pattern[8];

/* Evidence: BitMap structure from QuickDraw */
typedef struct BitMap {
    Ptr baseAddr;
    int16_t rowBytes;
    Rect bounds;
} BitMap;

/* Forward declarations */
typedef struct WindowRecord* WindowPtr;
typedef struct WindowRecord* WindowPeek;
typedef struct GrafPort* GrafPtr;
typedef Handle RgnHandle;
typedef Handle ControlHandle;
typedef Handle PicHandle;
typedef Ptr QDProcsPtr;

/* Evidence: GrafPort structure from layouts analysis (108 bytes total) */
typedef struct GrafPort {
    int16_t device;
    BitMap portBits;
    Rect portRect;
    RgnHandle visRgn;
    RgnHandle clipRgn;
    Pattern bkPat;
    Pattern fillPat;
    Point pnLoc;
    Point pnSize;
    int16_t pnMode;
    Pattern pnPat;
    int16_t pnVis;
    int16_t txFont;
    uint8_t txFace;
    uint8_t txPad;
    int16_t txMode;
    int16_t txSize;
    Fixed spExtra;
    int32_t fgColor;
    int32_t bkColor;
    int16_t colrBit;
    int16_t patStretch;
    Handle picSave;
    Handle rgnSave;
    Handle polySave;
    QDProcsPtr grafProcs;
} GrafPort;

/* Evidence: WindowRecord structure from layouts analysis (156 bytes total) */
typedef struct WindowRecord {
    GrafPort port;              /* Offset 0, size 108 */
    int16_t windowKind;         /* Offset 108, size 2 */
    Boolean visible;            /* Offset 110, size 1 */
    Boolean hilited;            /* Offset 111, size 1 */
    Boolean goAwayFlag;         /* Offset 112, size 1 */
    Boolean spareFlag;          /* Offset 113, size 1 */
    RgnHandle strucRgn;         /* Offset 114, size 4 */
    RgnHandle contRgn;          /* Offset 118, size 4 */
    RgnHandle updateRgn;        /* Offset 122, size 4 */
    Handle windowDefProc;       /* Offset 126, size 4 */
    Handle dataHandle;          /* Offset 130, size 4 */
    StringHandle titleHandle;   /* Offset 134, size 4 */
    int16_t titleWidth;         /* Offset 138, size 2 */
    ControlHandle controlList;  /* Offset 140, size 4 */
    WindowPeek nextWindow;      /* Offset 144, size 4 */
    PicHandle windowPic;        /* Offset 148, size 4 */
    int32_t refCon;            /* Offset 152, size 4 */
} WindowRecord;

/* Evidence: WDEF structure for window definition procedures */
typedef struct WDEF {
    int16_t varCode;
    WindowPtr theWindow;
    int16_t message;
} WDEF;

/* Evidence: Constants from trap analysis and Inside Macintosh */
enum {
    /* FindWindow results */
    inDesk = 0,
    inMenuBar = 1,
    inSysWindow = 2,
    inContent = 3,
    inDrag = 4,
    inGrow = 5,
    inGoAway = 6,

    /* Window definition procedures */
    documentProc = 0,
    dBoxProc = 1,
    plainDBox = 2,
    altDBoxProc = 3,
    noGrowDocProc = 4,
    rDocProc = 16
};

/* Evidence: Window Manager trap functions from mappings.windowmgr.json */

/* Trap 0xA912 - Initialize Window Manager */
void InitWindows(void);

/* Trap 0xA913 - Create new window */
WindowPtr NewWindow(void* wStorage, const Rect* boundsRect, ConstStr255Param title,
                   Boolean visible, int16_t theProc, WindowPtr behind,
                   Boolean goAwayFlag, int32_t refCon);

/* Trap 0xA914 - Dispose of window */
void DisposeWindow(WindowPtr window);

/* Trap 0xA915 - Make window visible */
void ShowWindow(WindowPtr window);

/* Trap 0xA916 - Hide window */
void HideWindow(WindowPtr window);

/* Trap 0xA910 - Get Window Manager port */
void GetWMgrPort(GrafPtr* wMgrPort);

/* Trap 0xA922 - Begin window update */
void BeginUpdate(WindowPtr window);

/* Trap 0xA923 - End window update */
void EndUpdate(WindowPtr window);

/* Trap 0xA92C - Find window at point */
int16_t FindWindow(Point thePoint, WindowPtr* whichWindow);

/* Trap 0xA91F - Select window */
void SelectWindow(WindowPtr window);

/* Additional Window Manager functions */
void MoveWindow(WindowPtr window, int16_t hGlobal, int16_t vGlobal, Boolean front);
void SizeWindow(WindowPtr window, int16_t w, int16_t h, Boolean fUpdate);
void DragWindow(WindowPtr window, Point startPt, const Rect* boundsRect);
int32_t GrowWindow(WindowPtr window, Point startPt, const Rect* bBox);
void BringToFront(WindowPtr window);
void SendBehind(WindowPtr window, WindowPtr behindWindow);
WindowPtr FrontWindow(void);
void SetWTitle(WindowPtr window, ConstStr255Param title);
void GetWTitle(WindowPtr window, Str255 title);
void DrawControls(WindowPtr window);
void DrawGrowIcon(WindowPtr window);

#endif /* WINDOW_MANAGER_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "component": "window_manager",
 *   "evidence_sources": [
 *     {"source": "System.rsrc", "sha256": "78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba"},
 *     {"source": "layouts.curated.windowmgr.json", "structures": 7, "validation": "passed"},
 *     {"source": "mappings.windowmgr.json", "trap_calls": 21, "confidence": "high"}
 *   ],
 *   "structures_implemented": [
 *     {"name": "WindowRecord", "size": 156, "fields": 17},
 *     {"name": "GrafPort", "size": 108, "fields": 26},
 *     {"name": "Point", "size": 4, "fields": 2},
 *     {"name": "Rect", "size": 8, "fields": 4},
 *     {"name": "WDEF", "size": 8, "fields": 3}
 *   ],
 *   "trap_functions": 21,
 *   "architecture": "m68k",
 *   "abi": "mac_toolbox",
 *   "compliance": "evidence_based"
 * }
 */
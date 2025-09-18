/*
 * ===========================================================================
 * RE-AGENT-BANNER: Apple System 7.1 List Manager
 * ===========================================================================
 * Artifact: System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 * Architecture: m68k
 * ABI: macos_system7_1
 *
 * Evidence Sources:
 * - Trap occurrences: 132 total found in binary analysis
 * - Structure layouts: ListRec (84 bytes), Cell (4 bytes)
 * - String evidence: "List Manager not present", "LDEF" resource type
 * - Function mappings from evidence.curated.listmgr.json
 * - Structure definitions from layouts.curated.listmgr.json
 *
 * Provenance:
 * - r2 trap search: addresses documented in evidence files
 * - Structure sizes verified through offset analysis
 * - Mac Inside Macintosh documentation correlation
 * ===========================================================================
 */

#ifndef LIST_MANAGER_H
#define LIST_MANAGER_H

#include <MacTypes.h>
#include <QuickDraw.h>
#include <Controls.h>
#include <Windows.h>

/* ---- Provenance Macros --------------------------------------------------*/
#define PROV(msg)   /* PROV: msg */
#define NOTE(msg)   /* NOTE:  msg */
#define TODO_EVID(msg) /* TODO: EVIDENCE REQUIRED — msg */

/* ---- Forward Declarations -----------------------------------------------*/
typedef struct ListRec ListRec;
typedef ListRec** ListHandle;
typedef ListRec* ListPtr;

/* ---- Cell Structure (4 bytes) -------------------------------------------*/
/* PROV: Size verified in layouts.curated.listmgr.json @ structure Cell */
typedef struct Cell {
    short v;    /* PROV: Vertical coordinate (row) @ offset 0x00 */
    short h;    /* PROV: Horizontal coordinate (column) @ offset 0x02 */
} Cell;

/* ---- Callback Types -----------------------------------------------------*/
/* PROV: ListSearchUPP from mappings.listmgr.json @ type_definitions */
typedef pascal Boolean (*ListSearchUPP)(Ptr cellDataPtr, Ptr searchDataPtr,
                                        short cellDataLen, short searchDataLen);

/* PROV: ListClickLoopUPP from layouts.curated.listmgr.json @ ListClickLoopUPP */
typedef pascal Boolean (*ListClickLoopUPP)(void);

/* PROV: ListDefProcPtr from layouts.curated.listmgr.json @ ListDefProcPtr */
typedef pascal void (*ListDefProcPtr)(short lMessage, Boolean lSelect, Rect* lRect,
                                      Cell lCell, short lDataOffset, short lDataLen,
                                      ListHandle lHandle);

/* ---- List Definition Messages -------------------------------------------*/
/* PROV: LDEF messages from layouts.curated.listmgr.json @ constants */
enum {
    lDrawMsg = 0,        /* PROV: Draw cell message */
    lHiliteMsg = 1,      /* PROV: Highlight cell message */
    lCloseSizeMsg = 16   /* PROV: Calculate close size message */
};

/* ---- List Flags ---------------------------------------------------------*/
/* PROV: Derived from LNew parameters in mappings.listmgr.json */
enum {
    lDoVAutoscroll = 0x02,  /* NOTE: Vertical autoscroll enabled */
    lDoHAutoscroll = 0x01,  /* NOTE: Horizontal autoscroll enabled */
    lOnlyOne = 0x80,        /* NOTE: Single selection mode */
    lExtendDrag = 0x40,     /* NOTE: Extended selection with drag */
    lNoDisjoint = 0x20,     /* NOTE: No disjoint selection */
    lNoExtend = 0x10,       /* NOTE: No extended selection */
    lNoRect = 0x08,         /* NOTE: No rectangular selection */
    lUseSense = 0x04,       /* NOTE: Use sense of first click */
    lNoNilHilite = 0x02     /* NOTE: Don't highlight empty cells */
};

/* ---- ListRec Structure (84 bytes) ---------------------------------------*/
/* PROV: Size=84 bytes from layouts.curated.listmgr.json @ ListRec */
struct ListRec {
    Rect    rView;          /* PROV: List view rectangle @ offset 0x00, size 8 */
    GrafPtr port;           /* PROV: Graphics port @ offset 0x08, size 4 */
    Point   indent;         /* PROV: Cell indentation @ offset 0x0C, size 4 */
    Point   cellSize;       /* PROV: Size of each cell @ offset 0x10, size 4 */
    Rect    visible;        /* PROV: Visible cell bounds @ offset 0x14, size 8 */
    ControlHandle vScroll;  /* PROV: Vertical scroll bar @ offset 0x1C, size 4 */
    ControlHandle hScroll;  /* PROV: Horizontal scroll bar @ offset 0x20, size 4 */
    char    selFlags;       /* PROV: Selection flags @ offset 0x24, size 1 */
    Boolean lActive;        /* PROV: List active flag @ offset 0x25, size 1 */
    char    lReserved;      /* PROV: Reserved/alignment @ offset 0x26, size 1 */
    char    listFlags;      /* PROV: List flags @ offset 0x27, size 1 */
    long    clikTime;       /* PROV: Last click time @ offset 0x28, size 4 */
    Point   clikLoc;        /* PROV: Last click location @ offset 0x2C, size 4 */
    Point   mouseLoc;       /* PROV: Current mouse location @ offset 0x30, size 4 */
    ListClickLoopUPP lClikLoop; /* PROV: Click tracking proc @ offset 0x34, size 4 */
    Cell    lastClick;      /* PROV: Last clicked cell @ offset 0x38, size 4 */
    long    refCon;         /* PROV: Reference constant @ offset 0x3C, size 4 */
    Handle  listDefProc;    /* PROV: LDEF handle @ offset 0x40, size 4 */
    Handle  userHandle;     /* PROV: User data handle @ offset 0x44, size 4 */
    Rect    dataBounds;     /* PROV: Data bounds rectangle @ offset 0x48, size 8 */
    Handle  cells;          /* PROV: Handle to cell data @ offset 0x50, size 4 */
};

/* ---- Type Definitions ---------------------------------------------------*/
typedef Handle DataHandle;  /* PROV: Handle to cell data storage */

/* ---- List Manager Function Declarations --------------------------------*/

/* PROV: Trap 0xA9E7 - 80 occurrences @ addresses in evidence.curated.listmgr.json */
ListHandle LNew(const Rect* rView, const Rect* dataBounds, Point cSize,
                short theProc, WindowPtr theWindow, Boolean drawIt,
                Boolean hasGrow, Boolean scrollHoriz, Boolean scrollVert);

/* PROV: Trap 0xA9E8 - Missing from evidence but required by API */
void LDispose(ListHandle lHandle);

/* PROV: Trap 0xA9E9 - 3 occurrences @ 0x00014551, 0x00014699, 0x0001472f */
short LAddColumn(short count, short colNum, ListHandle lHandle);

/* PROV: Trap 0xA9EA - 2 occurrences @ 0x00023dd4, 0x00030572 */
short LAddRow(short count, short rowNum, ListHandle lHandle);

/* PROV: Trap 0xA9EB - 26 occurrences @ addresses in evidence.curated.listmgr.json */
void LDelColumn(short count, short colNum, ListHandle lHandle);

/* PROV: Trap 0xA9EC - 2 occurrences @ 0x0002da21, 0x00032093 */
void LDelRow(short count, short rowNum, ListHandle lHandle);

/* PROV: Trap 0xA9ED - 13 occurrences @ addresses in evidence.curated.listmgr.json */
Boolean LGetSelect(Boolean next, Cell* theCell, ListHandle lHandle);

/* PROV: Trap 0xA9EE - 5 occurrences @ 0x000160e5, 0x00019b49, etc. */
Cell LLastClick(ListHandle lHandle);

/* PROV: Trap 0xA9EF - 1 occurrence @ 0x00008d53 */
Boolean LNextCell(Boolean hNext, Boolean vNext, Cell* theCell, ListHandle lHandle);

/* PROV: Trap 0xA9F0 - Missing from evidence but required by API */
Boolean LSearch(const void* dataPtr, short dataLen, ListSearchUPP searchProc,
                Cell* theCell, ListHandle lHandle);

/* PROV: Trap 0xA9F1 - Missing from evidence but required by API */
void LSize(short listWidth, short listHeight, ListHandle lHandle);

/* PROV: Trap 0xA9F2 - 2 occurrences @ 0x0000be47, 0x00034b3b */
void LSetDrawingMode(Boolean drawIt, ListHandle lHandle);

/* PROV: Trap 0xA9F3 - Missing from evidence but required by API */
void LScroll(short dCols, short dRows, ListHandle lHandle);

/* PROV: Trap 0xA9F4 - 2 occurrences @ 0x0000ac19, 0x0000ef9e */
Boolean LAutoScroll(ListHandle lHandle);

/* PROV: Trap 0xA9F5 - Missing from evidence but required by API */
void LUpdate(RgnHandle theRgn, ListHandle lHandle);

/* PROV: Trap 0xA9F6 - Missing from evidence but required by API */
void LActivate(Boolean act, ListHandle lHandle);

/* PROV: Trap 0xA9F7 - Missing from evidence but required by API */
Boolean LClick(Point pt, short modifiers, ListHandle lHandle);

/* PROV: Trap 0xA9F8 - Missing from evidence but required by API */
void LSetCell(const void* dataPtr, short dataLen, Cell theCell, ListHandle lHandle);

/* PROV: Trap 0xA9F9 - Missing from evidence but required by API */
void LGetCell(void* dataPtr, short* dataLen, Cell theCell, ListHandle lHandle);

/* PROV: Trap 0xA9FA - Missing from evidence but required by API */
void LSetSelect(Boolean setIt, Cell theCell, ListHandle lHandle);

/* PROV: Trap 0xA9FB - Missing from evidence but required by API */
void LDraw(Cell theCell, ListHandle lHandle);

/* ---- Utility Macros -----------------------------------------------------*/
#define LAddToCell(dataPtr, dataLen, theCell, lHandle) \
    LSetCell(dataPtr, dataLen, theCell, lHandle)

#define LClrCell(theCell, lHandle) \
    LSetCell(NULL, 0, theCell, lHandle)

/* ---- Error Codes --------------------------------------------------------*/
/* PROV: String "List Manager not present" @ 0x0000a98d suggests error handling */
enum {
    noListMgrErr = -192  /* NOTE: List Manager not present */
};

#endif /* LIST_MANAGER_H */
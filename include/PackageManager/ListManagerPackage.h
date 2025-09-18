/*
 * ListManagerPackage.h
 * System 7.1 Portable List Manager Package Implementation
 *
 * Implements Mac OS List Manager Package (Pack 0) for scrollable list display and management.
 * Essential for applications with list controls, file browsers, and selection interfaces.
 */

#ifndef __LIST_MANAGER_PACKAGE_H__
#define __LIST_MANAGER_PACKAGE_H__

#include "PackageTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* List Manager selector constants */
#define lSelAddColumn       0x0004
#define lSelAddRow          0x0008
#define lSelAddToCell       0x000C
#define lSelAutoScroll      0x0010
#define lSelCellSize        0x0014
#define lSelClick           0x0018
#define lSelClrCell         0x001C
#define lSelDelColumn       0x0020
#define lSelDelRow          0x0024
#define lSelDispose         0x0028
#define lSelDoDraw          0x002C
#define lSelDraw            0x0030
#define lSelFind            0x0034
#define lSelGetCell         0x0038
#define lSelGetSelect       0x003C
#define lSelLastClick       0x0040
#define lSelNew             0x0044
#define lSelNextCell        0x0048
#define lSelRect            0x004C
#define lSelScroll          0x0050
#define lSelSearch          0x0054
#define lSelSetCell         0x0058
#define lSelSetSelect       0x005C
#define lSelSize            0x0060
#define lSelUpdate          0x0064
#define lSelActivate        0x0267  /* Special selector */

/* List behavior flags */
#define lDoVAutoscroll      2
#define lDoHAutoscroll      1
#define lOnlyOne            -128
#define lExtendDrag         64
#define lNoDisjoint         32
#define lNoExtend           16
#define lNoRect             8
#define lUseSense           4
#define lNoNilHilite        2

/* List definition messages */
#define lInitMsg            0
#define lDrawMsg            1
#define lHiliteMsg          2
#define lCloseMsg           3

/* Cell definition */
typedef Point Cell;

/* Data structures */
typedef char DataArray[32001];
typedef char *DataPtr;
typedef char **DataHandle;

/* Search procedure type */
typedef int16_t (*SearchProcPtr)(Ptr aPtr, Ptr bPtr, int16_t aLen, int16_t bLen);

/* List record structure */
typedef struct {
    Rect            rView;          /* Visible rectangle */
    GrafPtr         port;           /* Graphics port */
    Point           indent;         /* Cell indentation */
    Point           cellSize;       /* Size of each cell */
    Rect            visible;        /* Visible cell range */
    ControlHandle   vScroll;        /* Vertical scroll bar */
    ControlHandle   hScroll;        /* Horizontal scroll bar */
    char            selFlags;       /* Selection flags */
    Boolean         lActive;        /* List is active */
    char            lReserved;      /* Reserved */
    char            listFlags;      /* List behavior flags */
    int32_t         clikTime;       /* Last click time */
    Point           clikLoc;        /* Last click location */
    Point           mouseLoc;       /* Current mouse location */
    ProcPtr         lClikLoop;      /* Click loop procedure */
    Cell            lastClick;      /* Last clicked cell */
    int32_t         refCon;         /* Reference constant */
    Handle          listDefProc;    /* List definition procedure */
    Handle          userHandle;     /* User handle */
    Rect            dataBounds;     /* Data bounds */
    DataHandle      cells;          /* Cell data */
    int16_t         maxIndex;       /* Maximum cell index */
    int16_t         cellArray[1];   /* Cell offset array */
} ListRec;

typedef ListRec *ListPtr;
typedef ListPtr *ListHandle;

/* Cell data structure */
typedef struct {
    int16_t     dataOffset;     /* Offset to cell data */
    int16_t     dataLen;        /* Length of cell data */
    Boolean     selected;       /* Cell is selected */
    Boolean     visible;        /* Cell is visible */
    Rect        cellRect;       /* Cell rectangle */
    void       *userData;       /* User data pointer */
} CellInfo;

/* List drawing context */
typedef struct {
    ListHandle  list;           /* List handle */
    GrafPtr     port;           /* Graphics port */
    Rect        drawRect;       /* Drawing rectangle */
    Cell        cell;           /* Current cell */
    Boolean     selected;       /* Cell is selected */
    Boolean     active;         /* List is active */
    void       *drawData;       /* Cell data */
    int16_t     dataLen;        /* Data length */
} ListDrawContext;

/* List Manager Package API Functions */

/* Package initialization and management */
int32_t InitListManagerPackage(void);
void CleanupListManagerPackage(void);
int32_t ListManagerDispatch(int16_t selector, void *params);

/* List creation and destruction */
ListHandle LNew(const Rect *rView, const Rect *dataBounds, Point cSize,
                int16_t theProc, WindowPtr theWindow, Boolean drawIt,
                Boolean hasGrow, Boolean scrollHoriz, Boolean scrollVert);

ListHandle lnew(Rect *rView, Rect *dataBounds, Point *cSize, int16_t theProc,
                WindowPtr theWindow, Boolean drawIt, Boolean hasGrow,
                Boolean scrollHoriz, Boolean scrollVert);

void LDispose(ListHandle lHandle);

/* List structure modification */
int16_t LAddColumn(int16_t count, int16_t colNum, ListHandle lHandle);
int16_t LAddRow(int16_t count, int16_t rowNum, ListHandle lHandle);
void LDelColumn(int16_t count, int16_t colNum, ListHandle lHandle);
void LDelRow(int16_t count, int16_t rowNum, ListHandle lHandle);

/* Cell data management */
void LAddToCell(const void *dataPtr, int16_t dataLen, Cell theCell, ListHandle lHandle);
void LClrCell(Cell theCell, ListHandle lHandle);
void LGetCell(void *dataPtr, int16_t *dataLen, Cell theCell, ListHandle lHandle);
void LSetCell(const void *dataPtr, int16_t dataLen, Cell theCell, ListHandle lHandle);
void LFind(int16_t *offset, int16_t *len, Cell theCell, ListHandle lHandle);

/* Cell selection and navigation */
Boolean LGetSelect(Boolean next, Cell *theCell, ListHandle lHandle);
void LSetSelect(Boolean setIt, Cell theCell, ListHandle lHandle);
Cell LLastClick(ListHandle lHandle);
Boolean LNextCell(Boolean hNext, Boolean vNext, Cell *theCell, ListHandle lHandle);
Boolean LSearch(const void *dataPtr, int16_t dataLen, SearchProcPtr searchProc,
                Cell *theCell, ListHandle lHandle);

/* List display and interaction */
void LDoDraw(Boolean drawIt, ListHandle lHandle);
void LDraw(Cell theCell, ListHandle lHandle);
void LUpdate(RgnHandle theRgn, ListHandle lHandle);
void LActivate(Boolean act, ListHandle lHandle);
Boolean LClick(Point pt, int16_t modifiers, ListHandle lHandle);

/* List geometry and scrolling */
void LCellSize(Point cSize, ListHandle lHandle);
void LSize(int16_t listWidth, int16_t listHeight, ListHandle lHandle);
void LRect(Rect *cellRect, Cell theCell, ListHandle lHandle);
void LScroll(int16_t dCols, int16_t dRows, ListHandle lHandle);
void LAutoScroll(ListHandle lHandle);

/* C-style interface functions */
void ldraw(Cell *theCell, ListHandle lHandle);
Boolean lclick(Point *pt, int16_t modifiers, ListHandle lHandle);
void lcellsize(Point *cSize, ListHandle lHandle);

/* Extended List Manager functions */

/* List data source interface */
typedef struct {
    int32_t (*GetItemCount)(ListHandle list);
    void (*GetItemData)(ListHandle list, Cell cell, void *data, int16_t *dataLen);
    void (*SetItemData)(ListHandle list, Cell cell, const void *data, int16_t dataLen);
    Boolean (*IsItemSelected)(ListHandle list, Cell cell);
    void (*SetItemSelected)(ListHandle list, Cell cell, Boolean selected);
    void (*DrawItem)(ListHandle list, Cell cell, const Rect *cellRect, Boolean selected, Boolean active);
    int16_t (*CompareItems)(ListHandle list, Cell cell1, Cell cell2);
    Boolean (*CanSelectItem)(ListHandle list, Cell cell);
} ListDataSource;

/* List configuration */
typedef struct {
    Point           cellSize;           /* Size of each cell */
    Boolean         hasVerticalScroll;  /* Has vertical scroll bar */
    Boolean         hasHorizontalScroll; /* Has horizontal scroll bar */
    Boolean         allowMultipleSelection; /* Allow multiple selection */
    Boolean         allowEmptySelection; /* Allow no selection */
    Boolean         extendSelection;     /* Extend selection with shift */
    Boolean         autoScroll;         /* Auto-scroll on drag */
    Boolean         drawFocusRect;      /* Draw focus rectangle */
    int16_t         selectionStyle;     /* Selection visual style */
    ListDataSource *dataSource;        /* Data source callbacks */
    void           *userData;           /* User data pointer */
} ListConfiguration;

/* Enhanced list creation */
ListHandle LCreateWithConfig(const Rect *rView, const ListConfiguration *config,
                            WindowPtr theWindow);

/* List configuration functions */
void LSetConfiguration(ListHandle lHandle, const ListConfiguration *config);
void LGetConfiguration(ListHandle lHandle, ListConfiguration *config);
void LSetDataSource(ListHandle lHandle, const ListDataSource *dataSource);
ListDataSource *LGetDataSource(ListHandle lHandle);

/* Selection management */
int16_t LCountSelectedItems(ListHandle lHandle);
void LGetSelectedItems(ListHandle lHandle, Cell *cells, int16_t maxCells, int16_t *actualCount);
void LSelectAll(ListHandle lHandle);
void LSelectNone(ListHandle lHandle);
void LSelectRange(ListHandle lHandle, Cell startCell, Cell endCell, Boolean extend);
void LToggleSelection(ListHandle lHandle, Cell theCell);
Boolean LIsItemSelected(ListHandle lHandle, Cell theCell);

/* List sorting */
void LSortItems(ListHandle lHandle, SearchProcPtr compareProc);
void LSortItemsWithData(ListHandle lHandle, void *userData,
                       int16_t (*compareProc)(Cell cell1, Cell cell2, void *userData));

/* Keyboard navigation */
void LHandleKeyDown(ListHandle lHandle, char keyCode, int16_t modifiers);
void LSetKeyboardNavigation(ListHandle lHandle, Boolean enabled);
Boolean LGetKeyboardNavigation(ListHandle lHandle);

/* List metrics and geometry */
int16_t LGetVisibleCells(ListHandle lHandle, Cell *topLeft, Cell *bottomRight);
Boolean LIsCellVisible(ListHandle lHandle, Cell theCell);
void LScrollToCell(ListHandle lHandle, Cell theCell, Boolean centerInView);
void LGetCellBounds(ListHandle lHandle, Cell theCell, Rect *bounds);
Cell LPointToCell(ListHandle lHandle, Point pt);
Boolean LIsPointInList(ListHandle lHandle, Point pt);

/* List content management */
void LRefreshList(ListHandle lHandle);
void LInvalidateCell(ListHandle lHandle, Cell theCell);
void LInvalidateRange(ListHandle lHandle, Cell startCell, Cell endCell);
void LBeginUpdate(ListHandle lHandle);
void LEndUpdate(ListHandle lHandle);

/* List state management */
void LSaveState(ListHandle lHandle, void **stateData, int32_t *stateSize);
void LRestoreState(ListHandle lHandle, const void *stateData, int32_t stateSize);
void LResetToDefaults(ListHandle lHandle);

/* List validation and debugging */
Boolean LValidateList(ListHandle lHandle);
void LDumpListInfo(ListHandle lHandle);
int32_t LGetMemoryUsage(ListHandle lHandle);

/* Platform integration */
typedef struct {
    void (*InvalidateRect)(const Rect *rect);
    void (*ScrollRect)(const Rect *rect, int16_t dh, int16_t dv);
    void (*SetCursor)(int16_t cursorID);
    Boolean (*TrackMouse)(Point *mouseLoc, int16_t *modifiers);
    void (*PlaySound)(int16_t soundID);
} ListPlatformCallbacks;

void LSetPlatformCallbacks(const ListPlatformCallbacks *callbacks);
void LEnablePlatformDrawing(Boolean enabled);

/* Thread safety */
void LLockList(ListHandle lHandle);
void LUnlockList(ListHandle lHandle);
void LSetThreadSafe(Boolean threadSafe);

/* Memory management */
void LCompactMemory(ListHandle lHandle);
void LSetMemoryGrowthIncrement(ListHandle lHandle, int32_t increment);
int32_t LGetMemoryGrowthIncrement(ListHandle lHandle);

/* Performance optimization */
void LSetLazyDrawing(ListHandle lHandle, Boolean enabled);
Boolean LGetLazyDrawing(ListHandle lHandle);
void LSetUpdateMode(ListHandle lHandle, int16_t mode);
int16_t LGetUpdateMode(ListHandle lHandle);

/* List Manager utilities */
void LCopyCell(ListHandle srcList, Cell srcCell, ListHandle dstList, Cell dstCell);
void LMoveCell(ListHandle srcList, Cell srcCell, ListHandle dstList, Cell dstCell);
void LExchangeCells(ListHandle lHandle, Cell cell1, Cell cell2);
void LFillCells(ListHandle lHandle, Cell startCell, Cell endCell, const void *data, int16_t dataLen);

/* Constants for configuration */
#define kListSelectionStyleHighlight    0
#define kListSelectionStyleInvert       1
#define kListSelectionStyleFrame        2

#define kListUpdateModeImmediate        0
#define kListUpdateModeDeferred         1
#define kListUpdateModeLazy             2

#define kListKeyNavEnabled              1
#define kListKeyNavDisabled             0

#ifdef __cplusplus
}
#endif

#endif /* __LIST_MANAGER_PACKAGE_H__ */
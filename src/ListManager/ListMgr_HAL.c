/**
 * List Manager Hardware Abstraction Layer
 * Provides platform-independent implementation of list display and management
 *
 * This HAL layer bridges the System 7.1 List Manager with modern platforms,
 * providing complete list functionality including scrolling lists, selection,
 * custom list definition procedures (LDEFs), and integration with Control Manager.
 */

#include "../../include/ListManager/ListManager.h"
#include "../../include/ListManager/list_manager.h"
#include "../../include/WindowManager/WindowManager.h"
#include "../../include/ControlManager/ControlManager.h"
#include "../../include/EventManager/EventManager.h"
#include "../../include/QuickDraw/QuickDraw.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* List Manager constants */
#define DEFAULT_CELL_HEIGHT 16
#define DEFAULT_CELL_WIDTH 90
#define LDEF_DRAW_MSG 0
#define LDEF_HILIGHT_MSG 1
#define LDEF_CLOSE_MSG 2

/* List Manager Global State */
typedef struct ListMgrState {
    bool initialized;

    /* LDEF (List Definition) cache */
    struct {
        int16_t ldefID;
        Handle ldefHandle;
        ListDefProcPtr defProc;
    } ldefCache[16];
    int ldefCacheCount;

    /* Active lists tracking */
    ListHandle activeLists[256];
    int activeListCount;

    /* Default text LDEF */
    ListDefProcPtr defaultTextLDEF;

} ListMgrState;

static ListMgrState gListMgr = {0};

/* Forward declarations */
static void ListMgr_HAL_DrawCell(ListHandle list, Cell cell, Rect* cellRect);
static void ListMgr_HAL_HiliteCell(ListHandle list, Cell cell, bool hilite);
static void ListMgr_HAL_ScrollList(ListHandle list, int16_t dh, int16_t dv);
static void ListMgr_HAL_UpdateScrollBars(ListHandle list);
static bool ListMgr_HAL_CellInView(ListHandle list, Cell cell);
static void ListMgr_HAL_RecalcVisibility(ListHandle list);
static ListDefProcPtr ListMgr_HAL_GetLDEF(int16_t ldefID);
static pascal void ListMgr_HAL_DefaultTextLDEF(short message, Boolean selected,
                                               Rect* cellRect, Cell cell,
                                               short dataOffset, short dataLen,
                                               ListHandle list);

/**
 * Initialize List Manager HAL
 */
void ListMgr_HAL_Init(void)
{
    if (gListMgr.initialized) {
        return;
    }

    printf("List Manager HAL: Initializing list system...\n");

    /* Set default text LDEF */
    gListMgr.defaultTextLDEF = (ListDefProcPtr)ListMgr_HAL_DefaultTextLDEF;

    /* Clear active lists */
    gListMgr.activeListCount = 0;

    gListMgr.initialized = true;

    printf("List Manager HAL: List system initialized\n");
}

/**
 * Create a new list
 */
ListHandle ListMgr_HAL_LNew(const Rect* rView, const ListBounds* dataBounds,
                           Point cSize, int16_t theProc, WindowPtr theWindow,
                           Boolean drawIt, Boolean hasGrow, Boolean scrollHoriz,
                           Boolean scrollVert)
{
    if (!gListMgr.initialized) {
        ListMgr_HAL_Init();
    }

    /* Allocate list record */
    ListHandle list = (ListHandle)NewHandleClear(sizeof(ListRec));
    if (!list) {
        return NULL;
    }

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Initialize list fields */
    listPtr->rView = *rView;
    listPtr->port = theWindow;
    listPtr->indent.h = 0;
    listPtr->indent.v = 0;
    listPtr->cellSize = cSize;

    /* Set data bounds */
    if (dataBounds) {
        listPtr->dataBounds = *dataBounds;
    } else {
        /* Default to empty list */
        SetRect(&listPtr->dataBounds, 0, 0, 1, 0);
    }

    /* Calculate visible cells */
    listPtr->visible.top = 0;
    listPtr->visible.left = 0;
    listPtr->visible.bottom = (rView->bottom - rView->top) / cSize.v;
    listPtr->visible.right = (rView->right - rView->left) / cSize.h;

    /* Allocate cell data array */
    int16_t rows = listPtr->dataBounds.bottom - listPtr->dataBounds.top;
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;
    int32_t cellCount = rows * cols;

    if (cellCount > 0) {
        listPtr->cells = NewHandle(cellCount * sizeof(Handle));
        if (!listPtr->cells) {
            HUnlock((Handle)list);
            DisposeHandle((Handle)list);
            return NULL;
        }

        /* Initialize cells to NULL */
        HLock(listPtr->cells);
        Handle* cellArray = (Handle*)*listPtr->cells;
        for (int32_t i = 0; i < cellCount; i++) {
            cellArray[i] = NULL;
        }
        HUnlock(listPtr->cells);
    }

    /* Create selection handle */
    listPtr->selFlags = NewHandle(cellCount);
    if (cellCount > 0 && !listPtr->selFlags) {
        if (listPtr->cells) DisposeHandle(listPtr->cells);
        HUnlock((Handle)list);
        DisposeHandle((Handle)list);
        return NULL;
    }

    /* Clear selection flags */
    if (listPtr->selFlags) {
        HLock(listPtr->selFlags);
        memset(*listPtr->selFlags, 0, GetHandleSize(listPtr->selFlags));
        HUnlock(listPtr->selFlags);
    }

    /* Set list flags */
    listPtr->listFlags = 0;
    if (!drawIt) listPtr->listFlags |= lDoVAutoscroll;
    if (hasGrow) listPtr->listFlags |= lDoHAutoscroll;

    /* Set selection flags */
    listPtr->selFlags = 0;  /* lOnlyOne by default */

    /* Get LDEF */
    listPtr->listDefProc = ListMgr_HAL_GetLDEF(theProc);
    listPtr->userHandle = NULL;
    listPtr->clikLoc.h = -1;
    listPtr->clikLoc.v = -1;
    listPtr->mouseLoc.h = 0;
    listPtr->mouseLoc.v = 0;
    listPtr->lClikLoop = NULL;
    listPtr->lastClick = 0;
    listPtr->refCon = 0;

    /* Create scroll bars if requested */
    if (scrollVert) {
        Rect vScrollRect = *rView;
        vScrollRect.left = vScrollRect.right - 16;
        if (hasGrow) vScrollRect.bottom -= 15;

        listPtr->vScroll = NewControl(theWindow, &vScrollRect, "\p",
                                     drawIt, 0, 0, rows - listPtr->visible.bottom,
                                     scrollBarProc, 0);
    }

    if (scrollHoriz) {
        Rect hScrollRect = *rView;
        hScrollRect.top = hScrollRect.bottom - 16;
        if (hasGrow) hScrollRect.right -= 15;

        listPtr->hScroll = NewControl(theWindow, &hScrollRect, "\p",
                                     drawIt, 0, 0, cols - listPtr->visible.right,
                                     scrollBarProc, 0);
    }

    HUnlock((Handle)list);

    /* Add to active lists */
    if (gListMgr.activeListCount < 256) {
        gListMgr.activeLists[gListMgr.activeListCount++] = list;
    }

    /* Draw if requested */
    if (drawIt) {
        LUpdate(theWindow->visRgn, list);
    }

    return list;
}

/**
 * Dispose of a list
 */
void ListMgr_HAL_LDispose(ListHandle list)
{
    if (!list) return;

    ListPtr listPtr = *list;

    /* Call LDEF close message */
    if (listPtr->listDefProc) {
        /* CallListDefProc(LDEF_CLOSE_MSG, ...) */
    }

    /* Dispose cell data */
    if (listPtr->cells) {
        HLock(listPtr->cells);
        Handle* cellArray = (Handle*)*listPtr->cells;
        int32_t cellCount = (listPtr->dataBounds.bottom - listPtr->dataBounds.top) *
                           (listPtr->dataBounds.right - listPtr->dataBounds.left);

        for (int32_t i = 0; i < cellCount; i++) {
            if (cellArray[i]) {
                DisposeHandle(cellArray[i]);
            }
        }
        HUnlock(listPtr->cells);
        DisposeHandle(listPtr->cells);
    }

    /* Dispose selection flags */
    if (listPtr->selFlags) {
        DisposeHandle(listPtr->selFlags);
    }

    /* Dispose scroll bars */
    if (listPtr->vScroll) {
        DisposeControl(listPtr->vScroll);
    }
    if (listPtr->hScroll) {
        DisposeControl(listPtr->hScroll);
    }

    /* Remove from active lists */
    for (int i = 0; i < gListMgr.activeListCount; i++) {
        if (gListMgr.activeLists[i] == list) {
            gListMgr.activeLists[i] = gListMgr.activeLists[--gListMgr.activeListCount];
            break;
        }
    }

    /* Dispose list handle */
    DisposeHandle((Handle)list);
}

/**
 * Add rows to list
 */
int16_t ListMgr_HAL_LAddRow(int16_t count, int16_t rowNum, ListHandle list)
{
    if (!list || count <= 0) return -1;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    int16_t oldRows = listPtr->dataBounds.bottom - listPtr->dataBounds.top;
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;

    /* Adjust row number */
    if (rowNum < 0 || rowNum > oldRows) {
        rowNum = oldRows;  /* Add at end */
    }

    /* Expand data bounds */
    listPtr->dataBounds.bottom += count;

    /* Reallocate cell array */
    int32_t oldCellCount = oldRows * cols;
    int32_t newCellCount = (oldRows + count) * cols;

    SetHandleSize(listPtr->cells, newCellCount * sizeof(Handle));
    if (MemError() != noErr) {
        listPtr->dataBounds.bottom -= count;  /* Restore */
        HUnlock((Handle)list);
        return -1;
    }

    /* Move existing cells if inserting */
    HLock(listPtr->cells);
    Handle* cellArray = (Handle*)*listPtr->cells;

    if (rowNum < oldRows) {
        /* Move cells down */
        for (int32_t i = newCellCount - 1; i >= (rowNum + count) * cols; i--) {
            cellArray[i] = cellArray[i - count * cols];
        }
    }

    /* Initialize new cells */
    for (int16_t r = 0; r < count; r++) {
        for (int16_t c = 0; c < cols; c++) {
            cellArray[(rowNum + r) * cols + c] = NULL;
        }
    }

    HUnlock(listPtr->cells);

    /* Expand selection flags */
    SetHandleSize(listPtr->selFlags, newCellCount);
    if (listPtr->selFlags) {
        HLock(listPtr->selFlags);
        char* selArray = *listPtr->selFlags;

        /* Move selection flags if inserting */
        if (rowNum < oldRows) {
            for (int32_t i = newCellCount - 1; i >= (rowNum + count) * cols; i--) {
                selArray[i] = selArray[i - count * cols];
            }
        }

        /* Clear new selection flags */
        for (int32_t i = rowNum * cols; i < (rowNum + count) * cols; i++) {
            selArray[i] = 0;
        }

        HUnlock(listPtr->selFlags);
    }

    /* Update scroll bars */
    ListMgr_HAL_UpdateScrollBars(list);

    HUnlock((Handle)list);

    /* Return first new row */
    return rowNum;
}

/**
 * Delete rows from list
 */
void ListMgr_HAL_LDelRow(int16_t count, int16_t rowNum, ListHandle list)
{
    if (!list || count <= 0) return;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    int16_t rows = listPtr->dataBounds.bottom - listPtr->dataBounds.top;
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;

    /* Validate parameters */
    if (rowNum < 0 || rowNum >= rows) {
        HUnlock((Handle)list);
        return;
    }

    if (rowNum + count > rows) {
        count = rows - rowNum;
    }

    /* Delete cell data */
    HLock(listPtr->cells);
    Handle* cellArray = (Handle*)*listPtr->cells;

    /* Dispose cells being deleted */
    for (int16_t r = 0; r < count; r++) {
        for (int16_t c = 0; c < cols; c++) {
            int32_t index = (rowNum + r) * cols + c;
            if (cellArray[index]) {
                DisposeHandle(cellArray[index]);
                cellArray[index] = NULL;
            }
        }
    }

    /* Move remaining cells up */
    int32_t cellsToMove = (rows - rowNum - count) * cols;
    if (cellsToMove > 0) {
        for (int32_t i = rowNum * cols; i < rowNum * cols + cellsToMove; i++) {
            cellArray[i] = cellArray[i + count * cols];
        }
    }

    HUnlock(listPtr->cells);

    /* Update data bounds */
    listPtr->dataBounds.bottom -= count;

    /* Shrink arrays */
    int32_t newCellCount = (rows - count) * cols;
    SetHandleSize(listPtr->cells, newCellCount * sizeof(Handle));
    SetHandleSize(listPtr->selFlags, newCellCount);

    /* Update scroll bars */
    ListMgr_HAL_UpdateScrollBars(list);

    HUnlock((Handle)list);
}

/**
 * Get cell data
 */
void ListMgr_HAL_LGetCell(void* dataPtr, int16_t* dataLen, Cell cell, ListHandle list)
{
    if (!list || !dataLen) return;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Validate cell */
    if (cell.v < listPtr->dataBounds.top || cell.v >= listPtr->dataBounds.bottom ||
        cell.h < listPtr->dataBounds.left || cell.h >= listPtr->dataBounds.right) {
        *dataLen = 0;
        HUnlock((Handle)list);
        return;
    }

    /* Get cell index */
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;
    int32_t index = (cell.v - listPtr->dataBounds.top) * cols +
                    (cell.h - listPtr->dataBounds.left);

    /* Get cell data */
    HLock(listPtr->cells);
    Handle* cellArray = (Handle*)*listPtr->cells;
    Handle cellData = cellArray[index];

    if (!cellData) {
        *dataLen = 0;
    } else {
        int32_t cellSize = GetHandleSize(cellData);
        if (dataPtr && *dataLen > 0) {
            int16_t copyLen = (*dataLen < cellSize) ? *dataLen : cellSize;
            HLock(cellData);
            BlockMove(*cellData, dataPtr, copyLen);
            HUnlock(cellData);
            *dataLen = copyLen;
        } else {
            *dataLen = cellSize;  /* Just return size */
        }
    }

    HUnlock(listPtr->cells);
    HUnlock((Handle)list);
}

/**
 * Set cell data
 */
void ListMgr_HAL_LSetCell(const void* dataPtr, int16_t dataLen, Cell cell, ListHandle list)
{
    if (!list) return;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Validate cell */
    if (cell.v < listPtr->dataBounds.top || cell.v >= listPtr->dataBounds.bottom ||
        cell.h < listPtr->dataBounds.left || cell.h >= listPtr->dataBounds.right) {
        HUnlock((Handle)list);
        return;
    }

    /* Get cell index */
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;
    int32_t index = (cell.v - listPtr->dataBounds.top) * cols +
                    (cell.h - listPtr->dataBounds.left);

    /* Set cell data */
    HLock(listPtr->cells);
    Handle* cellArray = (Handle*)*listPtr->cells;

    /* Dispose old data */
    if (cellArray[index]) {
        DisposeHandle(cellArray[index]);
        cellArray[index] = NULL;
    }

    /* Set new data */
    if (dataPtr && dataLen > 0) {
        cellArray[index] = NewHandle(dataLen);
        if (cellArray[index]) {
            HLock(cellArray[index]);
            BlockMove(dataPtr, *cellArray[index], dataLen);
            HUnlock(cellArray[index]);
        }
    }

    HUnlock(listPtr->cells);

    /* Redraw cell if visible */
    if (ListMgr_HAL_CellInView(list, cell)) {
        Rect cellRect;
        LRect(&cellRect, cell, list);
        ListMgr_HAL_DrawCell(list, cell, &cellRect);
    }

    HUnlock((Handle)list);
}

/**
 * Get cell rectangle
 */
void ListMgr_HAL_LRect(Rect* cellRect, Cell cell, ListHandle list)
{
    if (!list || !cellRect) return;

    ListPtr listPtr = *list;

    /* Calculate cell rectangle */
    cellRect->left = listPtr->rView.left + listPtr->indent.h +
                     (cell.h - listPtr->visible.left) * listPtr->cellSize.h;
    cellRect->top = listPtr->rView.top + listPtr->indent.v +
                    (cell.v - listPtr->visible.top) * listPtr->cellSize.v;
    cellRect->right = cellRect->left + listPtr->cellSize.h;
    cellRect->bottom = cellRect->top + listPtr->cellSize.v;
}

/**
 * Find cell at point
 */
Boolean ListMgr_HAL_LGetSelect(Boolean next, Cell* cell, ListHandle list)
{
    if (!list || !cell) return false;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    int16_t rows = listPtr->dataBounds.bottom - listPtr->dataBounds.top;
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;

    /* Get starting position */
    int16_t startRow = next ? cell->v : listPtr->dataBounds.top;
    int16_t startCol = next ? cell->h : listPtr->dataBounds.left;

    /* Find next selected cell */
    HLock(listPtr->selFlags);
    char* selArray = *listPtr->selFlags;

    for (int16_t r = startRow; r < listPtr->dataBounds.bottom; r++) {
        for (int16_t c = (r == startRow) ? startCol : listPtr->dataBounds.left;
             c < listPtr->dataBounds.right; c++) {

            int32_t index = (r - listPtr->dataBounds.top) * cols +
                           (c - listPtr->dataBounds.left);

            if (selArray[index]) {
                cell->v = r;
                cell->h = c;
                HUnlock(listPtr->selFlags);
                HUnlock((Handle)list);
                return true;
            }
        }
    }

    HUnlock(listPtr->selFlags);
    HUnlock((Handle)list);
    return false;
}

/**
 * Set cell selection
 */
void ListMgr_HAL_LSetSelect(Boolean setIt, Cell cell, ListHandle list)
{
    if (!list) return;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Validate cell */
    if (cell.v < listPtr->dataBounds.top || cell.v >= listPtr->dataBounds.bottom ||
        cell.h < listPtr->dataBounds.left || cell.h >= listPtr->dataBounds.right) {
        HUnlock((Handle)list);
        return;
    }

    /* Get cell index */
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;
    int32_t index = (cell.v - listPtr->dataBounds.top) * cols +
                    (cell.h - listPtr->dataBounds.left);

    /* Set selection */
    HLock(listPtr->selFlags);
    char* selArray = *listPtr->selFlags;

    if (selArray[index] != setIt) {
        selArray[index] = setIt;

        /* Redraw cell if visible */
        if (ListMgr_HAL_CellInView(list, cell)) {
            Rect cellRect;
            LRect(&cellRect, cell, list);
            ListMgr_HAL_HiliteCell(list, cell, setIt);
        }
    }

    HUnlock(listPtr->selFlags);
    HUnlock((Handle)list);
}

/**
 * Click in list
 */
Boolean ListMgr_HAL_LClick(Point pt, int16_t modifiers, ListHandle list)
{
    if (!list) return false;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Check if click is in list */
    if (!PtInRect(pt, &listPtr->rView)) {
        HUnlock((Handle)list);
        return false;
    }

    /* Find cell at point */
    Cell cell;
    cell.h = listPtr->visible.left + (pt.h - listPtr->rView.left) / listPtr->cellSize.h;
    cell.v = listPtr->visible.top + (pt.v - listPtr->rView.top) / listPtr->cellSize.v;

    /* Validate cell */
    if (cell.v >= listPtr->dataBounds.top && cell.v < listPtr->dataBounds.bottom &&
        cell.h >= listPtr->dataBounds.left && cell.h < listPtr->dataBounds.right) {

        /* Handle selection */
        int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;
        int32_t index = (cell.v - listPtr->dataBounds.top) * cols +
                       (cell.h - listPtr->dataBounds.left);

        HLock(listPtr->selFlags);
        char* selArray = *listPtr->selFlags;

        if (modifiers & shiftKey) {
            /* Extend selection */
            selArray[index] = !selArray[index];
        } else if (modifiers & cmdKey) {
            /* Toggle selection */
            selArray[index] = !selArray[index];
        } else {
            /* Single selection - clear others first */
            if (!(listPtr->selFlags & lNoNilHilite)) {
                int32_t cellCount = (listPtr->dataBounds.bottom - listPtr->dataBounds.top) * cols;
                for (int32_t i = 0; i < cellCount; i++) {
                    if (i != index && selArray[i]) {
                        selArray[i] = 0;
                        /* Redraw deselected cells */
                        Cell oldCell;
                        oldCell.v = listPtr->dataBounds.top + i / cols;
                        oldCell.h = listPtr->dataBounds.left + i % cols;
                        if (ListMgr_HAL_CellInView(list, oldCell)) {
                            Rect cellRect;
                            LRect(&cellRect, oldCell, list);
                            ListMgr_HAL_HiliteCell(list, oldCell, false);
                        }
                    }
                }
            }
            selArray[index] = 1;
        }

        /* Redraw cell */
        Rect cellRect;
        LRect(&cellRect, cell, list);
        ListMgr_HAL_HiliteCell(list, cell, selArray[index]);

        HUnlock(listPtr->selFlags);

        /* Check for double-click */
        int32_t currentTime = TickCount();
        if (cell.v == listPtr->clikLoc.v && cell.h == listPtr->clikLoc.h &&
            currentTime - listPtr->lastClick < GetDblTime()) {
            /* Double-click detected */
            HUnlock((Handle)list);
            return true;
        }

        /* Update click info */
        listPtr->clikLoc = cell;
        listPtr->lastClick = currentTime;
    }

    HUnlock((Handle)list);
    return false;
}

/**
 * Update list display
 */
void ListMgr_HAL_LUpdate(RgnHandle updateRgn, ListHandle list)
{
    if (!list) return;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Save graphics state */
    PenState savedPen;
    GetPenState(&savedPen);

    /* Set up clipping */
    RgnHandle clipRgn = NewRgn();
    GetClip(clipRgn);
    ClipRect(&listPtr->rView);

    /* Clear background */
    EraseRect(&listPtr->rView);

    /* Draw visible cells */
    for (int16_t r = listPtr->visible.top; r < listPtr->visible.bottom &&
         r < listPtr->dataBounds.bottom; r++) {

        for (int16_t c = listPtr->visible.left; c < listPtr->visible.right &&
             c < listPtr->dataBounds.right; c++) {

            Cell cell;
            cell.v = r;
            cell.h = c;

            Rect cellRect;
            LRect(&cellRect, cell, list);

            /* Check if cell needs updating */
            if (!updateRgn || RectInRgn(&cellRect, updateRgn)) {
                ListMgr_HAL_DrawCell(list, cell, &cellRect);
            }
        }
    }

    /* Draw scroll bars */
    if (listPtr->vScroll) {
        Draw1Control(listPtr->vScroll);
    }
    if (listPtr->hScroll) {
        Draw1Control(listPtr->hScroll);
    }

    /* Restore clipping */
    SetClip(clipRgn);
    DisposeRgn(clipRgn);

    /* Restore graphics state */
    SetPenState(&savedPen);

    HUnlock((Handle)list);
}

/**
 * Activate/deactivate list
 */
void ListMgr_HAL_LActivate(Boolean act, ListHandle list)
{
    if (!list) return;

    ListPtr listPtr = *list;

    /* Activate/deactivate scroll bars */
    if (listPtr->vScroll) {
        if (act) {
            ShowControl(listPtr->vScroll);
        } else {
            HideControl(listPtr->vScroll);
        }
    }

    if (listPtr->hScroll) {
        if (act) {
            ShowControl(listPtr->hScroll);
        } else {
            HideControl(listPtr->hScroll);
        }
    }

    /* Set active flag */
    if (act) {
        listPtr->listFlags |= lActive;
    } else {
        listPtr->listFlags &= ~lActive;
    }

    /* Redraw list */
    LUpdate(NULL, list);
}

/**
 * Scroll list
 */
void ListMgr_HAL_LScroll(int16_t dCols, int16_t dRows, ListHandle list)
{
    if (!list || (dCols == 0 && dRows == 0)) return;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Update visible range */
    listPtr->visible.left += dCols;
    listPtr->visible.right += dCols;
    listPtr->visible.top += dRows;
    listPtr->visible.bottom += dRows;

    /* Constrain to data bounds */
    if (listPtr->visible.left < listPtr->dataBounds.left) {
        int16_t diff = listPtr->dataBounds.left - listPtr->visible.left;
        listPtr->visible.left += diff;
        listPtr->visible.right += diff;
    }

    if (listPtr->visible.top < listPtr->dataBounds.top) {
        int16_t diff = listPtr->dataBounds.top - listPtr->visible.top;
        listPtr->visible.top += diff;
        listPtr->visible.bottom += diff;
    }

    /* Update scroll bars */
    if (listPtr->vScroll) {
        SetCtlValue(listPtr->vScroll, listPtr->visible.top);
    }
    if (listPtr->hScroll) {
        SetCtlValue(listPtr->hScroll, listPtr->visible.left);
    }

    /* Scroll and update display */
    Rect scrollRect = listPtr->rView;
    ScrollRect(&scrollRect, -dCols * listPtr->cellSize.h,
               -dRows * listPtr->cellSize.v, NULL);

    /* Redraw exposed area */
    LUpdate(NULL, list);

    HUnlock((Handle)list);
}

/* ===== Internal Helper Functions ===== */

/**
 * Draw a single cell
 */
static void ListMgr_HAL_DrawCell(ListHandle list, Cell cell, Rect* cellRect)
{
    ListPtr listPtr = *list;

    /* Get cell data */
    int16_t cols = listPtr->dataBounds.right - listPtr->dataBounds.left;
    int32_t index = (cell.v - listPtr->dataBounds.top) * cols +
                    (cell.h - listPtr->dataBounds.left);

    HLock(listPtr->cells);
    Handle* cellArray = (Handle*)*listPtr->cells;
    Handle cellData = cellArray[index];

    /* Get selection state */
    Boolean selected = false;
    if (listPtr->selFlags) {
        HLock(listPtr->selFlags);
        char* selArray = *listPtr->selFlags;
        selected = selArray[index];
        HUnlock(listPtr->selFlags);
    }

    /* Call LDEF or default drawing */
    if (listPtr->listDefProc) {
        /* Would call LDEF here */
        /* CallListDefProc(LDEF_DRAW_MSG, selected, cellRect, cell, ...) */
    } else {
        /* Default text drawing */
        if (cellData) {
            int32_t dataLen = GetHandleSize(cellData);
            HLock(cellData);

            /* Draw selection background */
            if (selected) {
                PenMode(patXor);
                PaintRect(cellRect);
                PenMode(patCopy);
            }

            /* Draw text */
            MoveTo(cellRect->left + 2, cellRect->bottom - 4);
            DrawText(*cellData, 0, dataLen);

            HUnlock(cellData);
        } else if (selected) {
            /* Empty cell but selected */
            PenMode(patXor);
            PaintRect(cellRect);
            PenMode(patCopy);
        }
    }

    HUnlock(listPtr->cells);
}

/**
 * Highlight/unhighlight a cell
 */
static void ListMgr_HAL_HiliteCell(ListHandle list, Cell cell, bool hilite)
{
    Rect cellRect;
    LRect(&cellRect, cell, list);

    /* Simple XOR highlighting */
    PenMode(patXor);
    PaintRect(&cellRect);
    PenMode(patCopy);
}

/**
 * Check if cell is visible
 */
static bool ListMgr_HAL_CellInView(ListHandle list, Cell cell)
{
    ListPtr listPtr = *list;

    return (cell.v >= listPtr->visible.top && cell.v < listPtr->visible.bottom &&
            cell.h >= listPtr->visible.left && cell.h < listPtr->visible.right);
}

/**
 * Update scroll bar settings
 */
static void ListMgr_HAL_UpdateScrollBars(ListHandle list)
{
    ListPtr listPtr = *list;

    if (listPtr->vScroll) {
        int16_t max = listPtr->dataBounds.bottom - listPtr->visible.bottom +
                     listPtr->visible.top;
        if (max < 0) max = 0;
        SetCtlMax(listPtr->vScroll, max);
    }

    if (listPtr->hScroll) {
        int16_t max = listPtr->dataBounds.right - listPtr->visible.right +
                     listPtr->visible.left;
        if (max < 0) max = 0;
        SetCtlMax(listPtr->hScroll, max);
    }
}

/**
 * Get LDEF procedure
 */
static ListDefProcPtr ListMgr_HAL_GetLDEF(int16_t ldefID)
{
    /* Check cache first */
    for (int i = 0; i < gListMgr.ldefCacheCount; i++) {
        if (gListMgr.ldefCache[i].ldefID == ldefID) {
            return gListMgr.ldefCache[i].defProc;
        }
    }

    /* For now, return default text LDEF */
    return gListMgr.defaultTextLDEF;
}

/**
 * Default text LDEF
 */
static pascal void ListMgr_HAL_DefaultTextLDEF(short message, Boolean selected,
                                               Rect* cellRect, Cell cell,
                                               short dataOffset, short dataLen,
                                               ListHandle list)
{
    switch (message) {
        case LDEF_DRAW_MSG:
            /* Draw cell is handled inline */
            break;

        case LDEF_HILIGHT_MSG:
            /* Highlight cell */
            if (selected) {
                PenMode(patXor);
                PaintRect(cellRect);
                PenMode(patCopy);
            }
            break;

        case LDEF_CLOSE_MSG:
            /* Cleanup */
            break;
    }
}

/**
 * Auto-scroll support
 */
Boolean ListMgr_HAL_LAutoScroll(ListHandle list)
{
    if (!list) return false;

    ListPtr listPtr = *list;
    Point mouse;
    GetMouse(&mouse);

    /* Check if mouse is outside list bounds */
    if (!PtInRect(mouse, &listPtr->rView)) {
        int16_t dRows = 0, dCols = 0;

        if (mouse.v < listPtr->rView.top) dRows = -1;
        if (mouse.v > listPtr->rView.bottom) dRows = 1;
        if (mouse.h < listPtr->rView.left) dCols = -1;
        if (mouse.h > listPtr->rView.right) dCols = 1;

        if (dRows || dCols) {
            LScroll(dCols, dRows, list);
            return true;
        }
    }

    return false;
}

/**
 * Search for data in list
 */
Boolean ListMgr_HAL_LSearch(const void* dataPtr, int16_t dataLen,
                           ListSearchProcPtr searchProc, Cell* cell, ListHandle list)
{
    if (!list || !cell || !dataPtr) return false;

    HLock((Handle)list);
    ListPtr listPtr = *list;

    /* Start from specified cell */
    for (int16_t r = cell->v; r < listPtr->dataBounds.bottom; r++) {
        for (int16_t c = (r == cell->v) ? cell->h : listPtr->dataBounds.left;
             c < listPtr->dataBounds.right; c++) {

            Cell testCell;
            testCell.v = r;
            testCell.h = c;

            /* Get cell data */
            char buffer[256];
            int16_t cellDataLen = sizeof(buffer);
            LGetCell(buffer, &cellDataLen, testCell, list);

            /* Compare */
            bool match = false;
            if (searchProc) {
                /* Use custom search proc */
                match = (*searchProc)(buffer, cellDataLen, dataPtr, dataLen);
            } else {
                /* Default comparison */
                if (cellDataLen >= dataLen) {
                    match = (memcmp(buffer, dataPtr, dataLen) == 0);
                }
            }

            if (match) {
                *cell = testCell;
                HUnlock((Handle)list);
                return true;
            }
        }
    }

    HUnlock((Handle)list);
    return false;
}

/**
 * Get/Set list reference constant
 */
int32_t ListMgr_HAL_GetListRefCon(ListHandle list)
{
    return list ? (**list).refCon : 0;
}

void ListMgr_HAL_SetListRefCon(ListHandle list, int32_t refCon)
{
    if (list) {
        (**list).refCon = refCon;
    }
}

/**
 * Cleanup List Manager HAL
 */
void ListMgr_HAL_Cleanup(void)
{
    if (!gListMgr.initialized) {
        return;
    }

    /* Dispose all active lists */
    for (int i = 0; i < gListMgr.activeListCount; i++) {
        if (gListMgr.activeLists[i]) {
            LDispose(gListMgr.activeLists[i]);
        }
    }

    /* Clear LDEF cache */
    for (int i = 0; i < gListMgr.ldefCacheCount; i++) {
        if (gListMgr.ldefCache[i].ldefHandle) {
            DisposeHandle(gListMgr.ldefCache[i].ldefHandle);
        }
    }

    /* Reset state */
    memset(&gListMgr, 0, sizeof(gListMgr));

    printf("List Manager HAL: Cleaned up\n");
}
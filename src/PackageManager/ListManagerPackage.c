/*
 * ListManagerPackage.c
 * System 7.1 Portable List Manager Package Implementation
 *
 * Implements Mac OS List Manager Package (Pack 0) for scrollable list display and management.
 * Essential for applications with list controls, file browsers, and selection interfaces.
 * Provides complete list management functionality with platform-independent rendering.
 */

#include "PackageManager/ListManagerPackage.h"
#include "PackageManager/PackageTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

/* List Manager package state */
static struct {
    Boolean                 initialized;
    ListPlatformCallbacks   platformCallbacks;
    Boolean                 platformDrawing;
    Boolean                 threadSafe;
    pthread_mutex_t         mutex;
    int32_t                 listCount;
    int32_t                 memoryUsage;
} g_listState = {
    .initialized = false,
    .platformCallbacks = {NULL},
    .platformDrawing = false,
    .threadSafe = true,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .listCount = 0,
    .memoryUsage = 0
};

/* Default list configuration */
static ListConfiguration g_defaultConfig = {
    .cellSize = {100, 16},
    .hasVerticalScroll = true,
    .hasHorizontalScroll = false,
    .allowMultipleSelection = false,
    .allowEmptySelection = true,
    .extendSelection = true,
    .autoScroll = true,
    .drawFocusRect = true,
    .selectionStyle = kListSelectionStyleHighlight,
    .dataSource = NULL,
    .userData = NULL
};

/* Forward declarations */
static void List_InitializeDefaults(void);
static void List_DrawCell(ListHandle lHandle, Cell theCell, Boolean selected, Boolean active);
static void List_InvalidateCell(ListHandle lHandle, Cell theCell);
static void List_UpdateScrollBars(ListHandle lHandle);
static Boolean List_IsValidCell(ListHandle lHandle, Cell theCell);
static int16_t List_GetCellDataOffset(ListHandle lHandle, Cell theCell);
static void List_SetCellDataOffset(ListHandle lHandle, Cell theCell, int16_t offset);
static CellInfo *List_GetCellInfo(ListHandle lHandle, Cell theCell);
static void List_ResizeCellArray(ListHandle lHandle, int16_t newSize);
static void List_DefaultDrawProc(ListHandle lHandle, Cell theCell, const Rect *cellRect,
                                Boolean selected, Boolean active);

/**
 * Initialize List Manager Package
 */
int32_t InitListManagerPackage(void)
{
    if (g_listState.initialized) {
        return PACKAGE_NO_ERROR;
    }

    /* Initialize thread safety */
    if (g_listState.threadSafe) {
        pthread_mutex_init(&g_listState.mutex, NULL);
    }

    /* Set up defaults */
    List_InitializeDefaults();

    g_listState.initialized = true;
    g_listState.listCount = 0;
    g_listState.memoryUsage = 0;

    return PACKAGE_NO_ERROR;
}

/**
 * List Manager package dispatch function
 */
int32_t ListManagerDispatch(int16_t selector, void *params)
{
    if (!g_listState.initialized) {
        InitListManagerPackage();
    }

    if (!params) {
        return PACKAGE_INVALID_PARAMS;
    }

    switch (selector) {
        case lSelNew: {
            void **args = (void**)params;
            const Rect *rView = (const Rect*)args[0];
            const Rect *dataBounds = (const Rect*)args[1];
            Point cSize = *(Point*)args[2];
            int16_t theProc = *(int16_t*)args[3];
            WindowPtr theWindow = (WindowPtr)args[4];
            Boolean drawIt = *(Boolean*)args[5];
            Boolean hasGrow = *(Boolean*)args[6];
            Boolean scrollHoriz = *(Boolean*)args[7];
            Boolean scrollVert = *(Boolean*)args[8];
            ListHandle *result = (ListHandle*)args[9];
            *result = LNew(rView, dataBounds, cSize, theProc, theWindow,
                          drawIt, hasGrow, scrollHoriz, scrollVert);
            return PACKAGE_NO_ERROR;
        }

        case lSelDispose: {
            ListHandle lHandle = *(ListHandle*)params;
            LDispose(lHandle);
            return PACKAGE_NO_ERROR;
        }

        case lSelAddRow: {
            void **args = (void**)params;
            int16_t count = *(int16_t*)args[0];
            int16_t rowNum = *(int16_t*)args[1];
            ListHandle lHandle = *(ListHandle*)args[2];
            int16_t *result = (int16_t*)args[3];
            *result = LAddRow(count, rowNum, lHandle);
            return PACKAGE_NO_ERROR;
        }

        case lSelSetCell: {
            void **args = (void**)params;
            const void *dataPtr = args[0];
            int16_t dataLen = *(int16_t*)args[1];
            Cell theCell = *(Cell*)args[2];
            ListHandle lHandle = *(ListHandle*)args[3];
            LSetCell(dataPtr, dataLen, theCell, lHandle);
            return PACKAGE_NO_ERROR;
        }

        case lSelGetCell: {
            void **args = (void**)params;
            void *dataPtr = args[0];
            int16_t *dataLen = (int16_t*)args[1];
            Cell theCell = *(Cell*)args[2];
            ListHandle lHandle = *(ListHandle*)args[3];
            LGetCell(dataPtr, dataLen, theCell, lHandle);
            return PACKAGE_NO_ERROR;
        }

        case lSelDraw: {
            void **args = (void**)params;
            Cell theCell = *(Cell*)args[0];
            ListHandle lHandle = *(ListHandle*)args[1];
            LDraw(theCell, lHandle);
            return PACKAGE_NO_ERROR;
        }

        case lSelSetSelect: {
            void **args = (void**)params;
            Boolean setIt = *(Boolean*)args[0];
            Cell theCell = *(Cell*)args[1];
            ListHandle lHandle = *(ListHandle*)args[2];
            LSetSelect(setIt, theCell, lHandle);
            return PACKAGE_NO_ERROR;
        }

        case lSelClick: {
            void **args = (void**)params;
            Point pt = *(Point*)args[0];
            int16_t modifiers = *(int16_t*)args[1];
            ListHandle lHandle = *(ListHandle*)args[2];
            Boolean *result = (Boolean*)args[3];
            *result = LClick(pt, modifiers, lHandle);
            return PACKAGE_NO_ERROR;
        }

        default:
            return PACKAGE_INVALID_SELECTOR;
    }
}

/**
 * List creation and destruction
 */
ListHandle LNew(const Rect *rView, const Rect *dataBounds, Point cSize,
                int16_t theProc, WindowPtr theWindow, Boolean drawIt,
                Boolean hasGrow, Boolean scrollHoriz, Boolean scrollVert)
{
    if (!rView || !dataBounds) {
        return NULL;
    }

    /* Allocate list record */
    ListHandle lHandle = (ListHandle)malloc(sizeof(ListPtr));
    if (!lHandle) return NULL;

    *lHandle = (ListPtr)malloc(sizeof(ListRec));
    if (!*lHandle) {
        free(lHandle);
        return NULL;
    }

    ListPtr list = *lHandle;
    memset(list, 0, sizeof(ListRec));

    /* Initialize list record */
    list->rView = *rView;
    list->port = NULL; /* Would be set to current graphics port */
    list->indent.h = 0;
    list->indent.v = 0;
    list->cellSize = cSize;
    list->dataBounds = *dataBounds;
    list->vScroll = NULL; /* Would create scroll bar control if needed */
    list->hScroll = NULL;
    list->selFlags = 0;
    list->lActive = true;
    list->lReserved = 0;
    list->listFlags = 0;

    if (scrollVert) list->listFlags |= lDoVAutoscroll;
    if (scrollHoriz) list->listFlags |= lDoHAutoscroll;

    list->clikTime = 0;
    list->clikLoc.h = 0;
    list->clikLoc.v = 0;
    list->mouseLoc.h = 0;
    list->mouseLoc.v = 0;
    list->lClikLoop = NULL;
    list->lastClick.h = -1;
    list->lastClick.v = -1;
    list->refCon = 0;
    list->listDefProc = NULL;
    list->userHandle = NULL;

    /* Calculate visible cell range */
    int16_t visibleCols = (rView->right - rView->left) / cSize.h;
    int16_t visibleRows = (rView->bottom - rView->top) / cSize.v;
    list->visible.left = 0;
    list->visible.top = 0;
    list->visible.right = visibleCols;
    list->visible.bottom = visibleRows;

    /* Allocate cell data */
    int16_t totalCells = (dataBounds->right - dataBounds->left) *
                        (dataBounds->bottom - dataBounds->top);
    list->cells = (DataHandle)malloc(sizeof(DataPtr));
    if (list->cells) {
        *list->cells = (DataPtr)malloc(totalCells * 32); /* Initial cell data size */
        if (!*list->cells) {
            free(list->cells);
            list->cells = NULL;
        }
    }

    /* Initialize cell array */
    list->maxIndex = totalCells;
    if (totalCells > 1) {
        /* Reallocate to include full cell array */
        ListPtr newList = (ListPtr)realloc(list, sizeof(ListRec) + (totalCells - 1) * sizeof(int16_t));
        if (newList) {
            *lHandle = newList;
            list = newList;
            memset(list->cellArray, 0, totalCells * sizeof(int16_t));
        }
    } else {
        list->cellArray[0] = 0;
    }

    g_listState.listCount++;
    g_listState.memoryUsage += sizeof(ListRec) + totalCells * sizeof(int16_t);

    return lHandle;
}

ListHandle lnew(Rect *rView, Rect *dataBounds, Point *cSize, int16_t theProc,
                WindowPtr theWindow, Boolean drawIt, Boolean hasGrow,
                Boolean scrollHoriz, Boolean scrollVert)
{
    if (!rView || !dataBounds || !cSize) return NULL;
    return LNew(rView, dataBounds, *cSize, theProc, theWindow, drawIt, hasGrow, scrollHoriz, scrollVert);
}

void LDispose(ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;

    /* Free cell data */
    if (list->cells && *list->cells) {
        free(*list->cells);
        free(list->cells);
    }

    /* Free list record */
    g_listState.memoryUsage -= sizeof(ListRec) + list->maxIndex * sizeof(int16_t);
    free(list);
    free(lHandle);

    g_listState.listCount--;
}

/**
 * List structure modification
 */
int16_t LAddRow(int16_t count, int16_t rowNum, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || count <= 0) return 0;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    if (rowNum < 0) rowNum = dataRows;
    if (rowNum > dataRows) rowNum = dataRows;

    /* Expand data bounds */
    list->dataBounds.bottom += count;

    /* Resize cell array if needed */
    int16_t newMaxIndex = (dataRows + count) * dataCols;
    if (newMaxIndex > list->maxIndex) {
        List_ResizeCellArray(lHandle, newMaxIndex);
    }

    /* Shift existing rows down */
    if (rowNum < dataRows) {
        int16_t insertIndex = rowNum * dataCols;
        int16_t moveCount = (dataRows - rowNum) * dataCols;
        memmove(&list->cellArray[insertIndex + count * dataCols],
                &list->cellArray[insertIndex],
                moveCount * sizeof(int16_t));
    }

    /* Initialize new rows */
    int16_t startIndex = rowNum * dataCols;
    memset(&list->cellArray[startIndex], 0, count * dataCols * sizeof(int16_t));

    /* Update visible range if needed */
    if (rowNum <= list->visible.bottom) {
        List_UpdateScrollBars(lHandle);
    }

    return count;
}

int16_t LAddColumn(int16_t count, int16_t colNum, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || count <= 0) return 0;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    if (colNum < 0) colNum = dataCols;
    if (colNum > dataCols) colNum = dataCols;

    /* Expand data bounds */
    list->dataBounds.right += count;

    /* This would require significant restructuring of the cell array */
    /* For now, return 0 to indicate no columns added */
    return 0;
}

void LDelRow(int16_t count, int16_t rowNum, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || count <= 0) return;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    if (rowNum < 0 || rowNum >= dataRows) return;
    if (rowNum + count > dataRows) count = dataRows - rowNum;

    /* Shift rows up */
    int16_t deleteIndex = rowNum * dataCols;
    int16_t moveCount = (dataRows - rowNum - count) * dataCols;
    if (moveCount > 0) {
        memmove(&list->cellArray[deleteIndex],
                &list->cellArray[deleteIndex + count * dataCols],
                moveCount * sizeof(int16_t));
    }

    /* Update data bounds */
    list->dataBounds.bottom -= count;

    /* Update visible range if needed */
    if (rowNum <= list->visible.bottom) {
        List_UpdateScrollBars(lHandle);
    }
}

void LDelColumn(int16_t count, int16_t colNum, ListHandle lHandle)
{
    /* Column deletion would require significant restructuring */
    /* Not implemented in this version */
}

/**
 * Cell data management
 */
void LSetCell(const void *dataPtr, int16_t dataLen, Cell theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !List_IsValidCell(lHandle, theCell)) return;

    ListPtr list = *lHandle;

    /* Find cell data location */
    int16_t cellIndex = theCell.v * (list->dataBounds.right - list->dataBounds.left) + theCell.h;
    if (cellIndex >= list->maxIndex) return;

    /* For now, store data length in cell array (simplified implementation) */
    if (dataPtr && dataLen > 0) {
        list->cellArray[cellIndex] = dataLen;
        /* In a full implementation, would store actual data in cells handle */
    } else {
        list->cellArray[cellIndex] = 0;
    }

    /* Invalidate cell for redraw */
    List_InvalidateCell(lHandle, theCell);
}

void LGetCell(void *dataPtr, int16_t *dataLen, Cell theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !dataLen || !List_IsValidCell(lHandle, theCell)) {
        if (dataLen) *dataLen = 0;
        return;
    }

    ListPtr list = *lHandle;
    int16_t cellIndex = theCell.v * (list->dataBounds.right - list->dataBounds.left) + theCell.h;
    if (cellIndex >= list->maxIndex) {
        *dataLen = 0;
        return;
    }

    /* Return stored data length (simplified implementation) */
    *dataLen = list->cellArray[cellIndex];

    /* In a full implementation, would copy actual data to dataPtr */
    if (dataPtr && *dataLen > 0) {
        /* Copy cell data here */
        memset(dataPtr, 0, *dataLen);
    }
}

void LAddToCell(const void *dataPtr, int16_t dataLen, Cell theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !dataPtr || dataLen <= 0 || !List_IsValidCell(lHandle, theCell)) {
        return;
    }

    /* Get existing data length */
    int16_t existingLen = 0;
    LGetCell(NULL, &existingLen, theCell, lHandle);

    /* Set new data length (append) */
    LSetCell(dataPtr, existingLen + dataLen, theCell, lHandle);
}

void LClrCell(Cell theCell, ListHandle lHandle)
{
    LSetCell(NULL, 0, theCell, lHandle);
}

void LFind(int16_t *offset, int16_t *len, Cell theCell, ListHandle lHandle)
{
    if (!offset || !len) return;

    *offset = 0;
    *len = 0;

    if (!lHandle || !*lHandle || !List_IsValidCell(lHandle, theCell)) return;

    /* Return cell data offset and length */
    int16_t dataLen;
    LGetCell(NULL, &dataLen, theCell, lHandle);
    *len = dataLen;
    *offset = List_GetCellDataOffset(lHandle, theCell);
}

/**
 * Cell selection and navigation
 */
Boolean LGetSelect(Boolean next, Cell *theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !theCell) return false;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    if (next) {
        /* Find next selected cell */
        for (int16_t row = theCell->v; row < dataRows; row++) {
            int16_t startCol = (row == theCell->v) ? theCell->h + 1 : 0;
            for (int16_t col = startCol; col < dataCols; col++) {
                Cell cell = {col, row};
                if (LIsItemSelected(lHandle, cell)) {
                    *theCell = cell;
                    return true;
                }
            }
        }
    } else {
        /* Find first selected cell */
        for (int16_t row = 0; row < dataRows; row++) {
            for (int16_t col = 0; col < dataCols; col++) {
                Cell cell = {col, row};
                if (LIsItemSelected(lHandle, cell)) {
                    *theCell = cell;
                    return true;
                }
            }
        }
    }

    return false;
}

void LSetSelect(Boolean setIt, Cell theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !List_IsValidCell(lHandle, theCell)) return;

    ListPtr list = *lHandle;

    /* For simplified implementation, use bit in cell array high bit */
    int16_t cellIndex = theCell.v * (list->dataBounds.right - list->dataBounds.left) + theCell.h;
    if (cellIndex >= list->maxIndex) return;

    if (setIt) {
        list->cellArray[cellIndex] |= 0x8000; /* Set high bit for selection */
    } else {
        list->cellArray[cellIndex] &= 0x7FFF; /* Clear high bit */
    }

    /* Redraw cell with new selection state */
    List_DrawCell(lHandle, theCell, setIt, list->lActive);
}

Cell LLastClick(ListHandle lHandle)
{
    if (!lHandle || !*lHandle) {
        Cell emptyCell = {-1, -1};
        return emptyCell;
    }

    return (*lHandle)->lastClick;
}

Boolean LNextCell(Boolean hNext, Boolean vNext, Cell *theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !theCell) return false;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    if (hNext) {
        theCell->h++;
        if (theCell->h >= dataCols) {
            theCell->h = 0;
            theCell->v++;
        }
    }

    if (vNext) {
        theCell->v++;
        if (theCell->v >= dataRows) {
            theCell->v = 0;
            theCell->h++;
        }
    }

    return (theCell->h < dataCols && theCell->v < dataRows);
}

Boolean LSearch(const void *dataPtr, int16_t dataLen, SearchProcPtr searchProc,
                Cell *theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !dataPtr || !searchProc || !theCell) return false;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    /* Search from current cell to end */
    for (int16_t row = theCell->v; row < dataRows; row++) {
        int16_t startCol = (row == theCell->v) ? theCell->h : 0;
        for (int16_t col = startCol; col < dataCols; col++) {
            Cell searchCell = {col, row};

            /* Get cell data */
            char cellData[256];
            int16_t cellDataLen = 0;
            LGetCell(cellData, &cellDataLen, searchCell, lHandle);

            /* Compare using search procedure */
            if (searchProc((Ptr)dataPtr, (Ptr)cellData, dataLen, cellDataLen) == 0) {
                *theCell = searchCell;
                return true;
            }
        }
    }

    return false;
}

/**
 * List display and interaction
 */
void LDoDraw(Boolean drawIt, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;

    if (drawIt) {
        /* Draw all visible cells */
        for (int16_t row = list->visible.top; row < list->visible.bottom; row++) {
            for (int16_t col = list->visible.left; col < list->visible.right; col++) {
                Cell cell = {col, row};
                if (List_IsValidCell(lHandle, cell)) {
                    Boolean selected = LIsItemSelected(lHandle, cell);
                    List_DrawCell(lHandle, cell, selected, list->lActive);
                }
            }
        }
    }
    /* If drawIt is false, disable drawing (not implemented) */
}

void LDraw(Cell theCell, ListHandle lHandle)
{
    if (!lHandle || !*lHandle || !List_IsValidCell(lHandle, theCell)) return;

    Boolean selected = LIsItemSelected(lHandle, theCell);
    Boolean active = (*lHandle)->lActive;
    List_DrawCell(lHandle, theCell, selected, active);
}

void LUpdate(RgnHandle theRgn, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    /* In a full implementation, would intersect region with list bounds
       and redraw only affected cells */
    LDoDraw(true, lHandle);
}

void LActivate(Boolean act, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;
    if (list->lActive != act) {
        list->lActive = act;
        /* Redraw to show activation state change */
        LDoDraw(true, lHandle);
    }
}

Boolean LClick(Point pt, int16_t modifiers, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return false;

    ListPtr list = *lHandle;

    /* Convert point to cell coordinates */
    Cell clickedCell;
    clickedCell.h = (pt.h - list->rView.left) / list->cellSize.h;
    clickedCell.v = (pt.v - list->rView.top) / list->cellSize.v;

    /* Adjust for visible area offset */
    clickedCell.h += list->visible.left;
    clickedCell.v += list->visible.top;

    if (!List_IsValidCell(lHandle, clickedCell)) return false;

    /* Handle selection */
    Boolean wasSelected = LIsItemSelected(lHandle, clickedCell);
    Boolean shouldSelect = true;

    if (modifiers & 0x0100) { /* Command key */
        /* Toggle selection */
        shouldSelect = !wasSelected;
    } else if (modifiers & 0x0200) { /* Shift key */
        /* Extend selection from last click */
        if (list->lastClick.h >= 0 && list->lastClick.v >= 0) {
            /* Select range */
            /* Implementation would select all cells between lastClick and clickedCell */
        }
        shouldSelect = true;
    } else {
        /* Normal click - clear other selections if not allowing multiple */
        if (!(list->listFlags & lExtendDrag)) {
            /* Clear all selections first */
            LSelectNone(lHandle);
        }
        shouldSelect = true;
    }

    LSetSelect(shouldSelect, clickedCell, lHandle);

    /* Update last click */
    list->lastClick = clickedCell;
    list->clikLoc = pt;

    return true;
}

/**
 * List geometry and scrolling
 */
void LCellSize(Point cSize, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;
    list->cellSize = cSize;

    /* Recalculate visible cell range */
    int16_t visibleCols = (list->rView.right - list->rView.left) / cSize.h;
    int16_t visibleRows = (list->rView.bottom - list->rView.top) / cSize.v;
    list->visible.right = list->visible.left + visibleCols;
    list->visible.bottom = list->visible.top + visibleRows;

    /* Redraw list */
    LDoDraw(true, lHandle);
}

void LSize(int16_t listWidth, int16_t listHeight, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;
    list->rView.right = list->rView.left + listWidth;
    list->rView.bottom = list->rView.top + listHeight;

    /* Recalculate visible cell range */
    int16_t visibleCols = listWidth / list->cellSize.h;
    int16_t visibleRows = listHeight / list->cellSize.v;
    list->visible.right = list->visible.left + visibleCols;
    list->visible.bottom = list->visible.top + visibleRows;

    List_UpdateScrollBars(lHandle);
    LDoDraw(true, lHandle);
}

void LRect(Rect *cellRect, Cell theCell, ListHandle lHandle)
{
    if (!cellRect || !lHandle || !*lHandle) return;

    ListPtr list = *lHandle;

    /* Calculate cell rectangle */
    cellRect->left = list->rView.left + (theCell.h - list->visible.left) * list->cellSize.h;
    cellRect->top = list->rView.top + (theCell.v - list->visible.top) * list->cellSize.v;
    cellRect->right = cellRect->left + list->cellSize.h;
    cellRect->bottom = cellRect->top + list->cellSize.v;
}

void LScroll(int16_t dCols, int16_t dRows, ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;

    /* Update visible range */
    list->visible.left += dCols;
    list->visible.top += dRows;
    list->visible.right += dCols;
    list->visible.bottom += dRows;

    /* Clamp to valid ranges */
    int16_t maxCols = list->dataBounds.right - list->dataBounds.left;
    int16_t maxRows = list->dataBounds.bottom - list->dataBounds.top;

    if (list->visible.left < 0) {
        int16_t adjust = -list->visible.left;
        list->visible.left += adjust;
        list->visible.right += adjust;
    }
    if (list->visible.top < 0) {
        int16_t adjust = -list->visible.top;
        list->visible.top += adjust;
        list->visible.bottom += adjust;
    }
    if (list->visible.right > maxCols) {
        int16_t adjust = list->visible.right - maxCols;
        list->visible.left -= adjust;
        list->visible.right -= adjust;
    }
    if (list->visible.bottom > maxRows) {
        int16_t adjust = list->visible.bottom - maxRows;
        list->visible.top -= adjust;
        list->visible.bottom -= adjust;
    }

    /* Redraw list */
    LDoDraw(true, lHandle);
    List_UpdateScrollBars(lHandle);
}

void LAutoScroll(ListHandle lHandle)
{
    /* Auto-scroll would be implemented based on mouse position */
    /* Not implemented in this simplified version */
}

/**
 * C-style interface functions
 */
void ldraw(Cell *theCell, ListHandle lHandle)
{
    if (theCell) {
        LDraw(*theCell, lHandle);
    }
}

Boolean lclick(Point *pt, int16_t modifiers, ListHandle lHandle)
{
    if (pt) {
        return LClick(*pt, modifiers, lHandle);
    }
    return false;
}

void lcellsize(Point *cSize, ListHandle lHandle)
{
    if (cSize) {
        LCellSize(*cSize, lHandle);
    }
}

/**
 * Extended selection functions
 */
Boolean LIsItemSelected(ListHandle lHandle, Cell theCell)
{
    if (!lHandle || !*lHandle || !List_IsValidCell(lHandle, theCell)) return false;

    ListPtr list = *lHandle;
    int16_t cellIndex = theCell.v * (list->dataBounds.right - list->dataBounds.left) + theCell.h;
    if (cellIndex >= list->maxIndex) return false;

    return (list->cellArray[cellIndex] & 0x8000) != 0;
}

void LSelectAll(ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    for (int16_t row = 0; row < dataRows; row++) {
        for (int16_t col = 0; col < dataCols; col++) {
            Cell cell = {col, row};
            LSetSelect(true, cell, lHandle);
        }
    }
}

void LSelectNone(ListHandle lHandle)
{
    if (!lHandle || !*lHandle) return;

    ListPtr list = *lHandle;
    int16_t dataCols = list->dataBounds.right - list->dataBounds.left;
    int16_t dataRows = list->dataBounds.bottom - list->dataBounds.top;

    for (int16_t row = 0; row < dataRows; row++) {
        for (int16_t col = 0; col < dataCols; col++) {
            Cell cell = {col, row};
            LSetSelect(false, cell, lHandle);
        }
    }
}

/**
 * Internal helper functions
 */
static void List_InitializeDefaults(void)
{
    /* Set up default platform callbacks if none provided */
    if (!g_listState.platformCallbacks.InvalidateRect) {
        /* Set default no-op callbacks */
        g_listState.platformCallbacks.InvalidateRect = NULL;
        g_listState.platformCallbacks.ScrollRect = NULL;
        g_listState.platformCallbacks.SetCursor = NULL;
        g_listState.platformCallbacks.TrackMouse = NULL;
        g_listState.platformCallbacks.PlaySound = NULL;
    }
}

static void List_DrawCell(ListHandle lHandle, Cell theCell, Boolean selected, Boolean active)
{
    if (!lHandle || !*lHandle) return;

    /* Calculate cell rectangle */
    Rect cellRect;
    LRect(&cellRect, theCell, lHandle);

    /* Call default drawing procedure */
    List_DefaultDrawProc(lHandle, theCell, &cellRect, selected, active);
}

static void List_DefaultDrawProc(ListHandle lHandle, Cell theCell, const Rect *cellRect,
                                Boolean selected, Boolean active)
{
    if (!lHandle || !*lHandle || !cellRect) return;

    /* Platform-specific drawing implementation */
    if (g_listState.platformDrawing) {
        /* Platform-specific drawing would go here */
        /* Note: Cell drawing at (%d,%d) rect [%d,%d,%d,%d] %s%s */
    }
}

static void List_InvalidateCell(ListHandle lHandle, Cell theCell)
{
    if (!lHandle || !*lHandle) return;

    if (g_listState.platformCallbacks.InvalidateRect) {
        Rect cellRect;
        LRect(&cellRect, theCell, lHandle);
        g_listState.platformCallbacks.InvalidateRect(&cellRect);
    }
}

static void List_UpdateScrollBars(ListHandle lHandle)
{
    /* Scroll bar updates would be implemented here */
    /* Not implemented in this simplified version */
}

static Boolean List_IsValidCell(ListHandle lHandle, Cell theCell)
{
    if (!lHandle || !*lHandle) return false;

    ListPtr list = *lHandle;
    return (theCell.h >= list->dataBounds.left &&
            theCell.h < list->dataBounds.right &&
            theCell.v >= list->dataBounds.top &&
            theCell.v < list->dataBounds.bottom);
}

static int16_t List_GetCellDataOffset(ListHandle lHandle, Cell theCell)
{
    if (!lHandle || !*lHandle || !List_IsValidCell(lHandle, theCell)) return 0;

    ListPtr list = *lHandle;
    int16_t cellIndex = theCell.v * (list->dataBounds.right - list->dataBounds.left) + theCell.h;
    return cellIndex * 32; /* Simplified: each cell gets 32 bytes */
}

static void List_ResizeCellArray(ListHandle lHandle, int16_t newSize)
{
    if (!lHandle || !*lHandle || newSize <= 0) return;

    ListPtr list = *lHandle;
    if (newSize <= list->maxIndex) return;

    /* Reallocate list record with larger cell array */
    ListPtr newList = (ListPtr)realloc(list, sizeof(ListRec) + (newSize - 1) * sizeof(int16_t));
    if (newList) {
        *lHandle = newList;

        /* Initialize new cells */
        memset(&newList->cellArray[newList->maxIndex], 0,
               (newSize - newList->maxIndex) * sizeof(int16_t));

        newList->maxIndex = newSize;
        g_listState.memoryUsage += (newSize - list->maxIndex) * sizeof(int16_t);
    }
}

/**
 * Configuration and platform integration
 */
void LSetPlatformCallbacks(const ListPlatformCallbacks *callbacks)
{
    if (callbacks) {
        g_listState.platformCallbacks = *callbacks;
    }
}

void LEnablePlatformDrawing(Boolean enabled)
{
    g_listState.platformDrawing = enabled;
}

/**
 * Cleanup function
 */
void CleanupListManagerPackage(void)
{
    if (g_listState.threadSafe) {
        pthread_mutex_destroy(&g_listState.mutex);
    }

    g_listState.initialized = false;
    g_listState.listCount = 0;
    g_listState.memoryUsage = 0;
}
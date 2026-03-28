/*
 * ListManager.c - Classic Mac OS List Manager Implementation
 *
 * System 7.1-compatible List Manager providing scrollable lists with
 * selection support for file pickers, option lists, and dialog controls.
 *
 * PUBLIC API:
 *   Lifecycle:   LNew, LDispose, LSize, LSetRefCon, LGetRefCon
 *   Model:       LAddRow, LDelRow, LAddColumn, LDelColumn, LSetCell, LGetCell
 *   Drawing:     LUpdate, LDraw, LGetCellRect, LScroll
 *   Selection:   LClick, LGetSelect, LSetSelect, LSelectAll, LClearSelect, LLastClick
 *   Keyboard:    LKey (Up/Down/PageUp/PageDown/Home/End/Space)
 *   Integration: LAttachScrollbars, ListFromDialogItem, AttachListToDialogItem
 *   Optional:    LSearch (stub)
 *
 * USAGE:
 *   1. Call LNew() with ListParams to create a list
 *   2. Add rows via LAddRow(), populate with LSetCell()
 *   3. Handle updates via LUpdate() in window update events
 *   4. Handle clicks via LClick() in mouse down events
 *   5. Handle keyboard via LKey() for arrow keys
 *   6. Dispose with LDispose() when done
 *
 * INTEGRATION:
 *   - Window Manager: list renders in window's local coordinates
 *   - Dialog Manager: attach lists to dialog items via AttachListToDialogItem()
 *   - Control Manager: attach scrollbars via LAttachScrollbars()
 *
 * NOTES:
 *   - Uses ListMgrRec internally (avoids conflict with SystemTypes.h ListRec)
 *   - Cell.h = column (horizontal), Cell.v = row (vertical)
 *   - Selection: single-sel or multi-sel with Shift/Cmd modifiers
 *   - All drawing clipped to viewRect; saves/restores port state
 *   - Error codes: memFullErr (-108), paramErr (-50)
 */

#include <string.h>
#include "SystemTypes.h"
#include "ListManager/ListManager.h"
#include "ListManager/ListManagerInternal.h"
#include "MemoryMgr/MemoryManager.h"
#include "EventManager/EventManager.h"
#include "DeskManager/DeskManager.h"
#include "System71StdLib.h"
#include "ListManager/ListLogging.h"

/* External functions */
extern OSErr MemError(void);
extern void InvalRect(const Rect* r);

/* Debug logging control */
#ifndef LIST_DEBUG
#define LIST_DEBUG 1
#endif

#if LIST_DEBUG
#define LIST_LOG(...) LIST_LOG_DEBUG(__VA_ARGS__)
#else
#define LIST_LOG(...)
#endif

/* Error codes */
#ifndef memFullErr
#define memFullErr (-108)
#endif
#ifndef paramErr
#define paramErr (-50)
#endif

/* Dialog-list registry */
DialogListAssoc gDialogListRegistry[MAX_DIALOG_LISTS];
static Boolean gRegistryInited = false;

/* ================================================================
 * INTERNAL HELPERS
 * ================================================================ */

static void InitRegistryIfNeeded(void)
{
    int i;
    if (gRegistryInited) return;
    
    for (i = 0; i < MAX_DIALOG_LISTS; i++) {
        gDialogListRegistry[i].dialog = NULL;
        gDialogListRegistry[i].itemNo = 0;
        gDialogListRegistry[i].list = NULL;
        gDialogListRegistry[i].active = false;
    }
    gRegistryInited = true;
}

DialogListAssoc* FindDialogListSlot(DialogPtr dlg, short itemNo, Boolean allocate)
{
    int i;
    DialogListAssoc* freeSlot = NULL;
    
    InitRegistryIfNeeded();
    
    /* First pass: find existing or free slot */
    for (i = 0; i < MAX_DIALOG_LISTS; i++) {
        if (gDialogListRegistry[i].active &&
            gDialogListRegistry[i].dialog == dlg &&
            gDialogListRegistry[i].itemNo == itemNo) {
            return &gDialogListRegistry[i];
        }
        if (!gDialogListRegistry[i].active && freeSlot == NULL) {
            freeSlot = &gDialogListRegistry[i];
        }
    }
    
    /* Allocate if requested */
    if (allocate && freeSlot) {
        freeSlot->dialog = dlg;
        freeSlot->itemNo = itemNo;
        freeSlot->active = true;
        return freeSlot;
    }
    
    return NULL;
}

/* ================================================================
 * LIST MANAGER LIFECYCLE
 * ================================================================ */

/* Convert ListHandle (system typedef) to our internal ListMgrHandle */
#define LIST_MGR_HANDLE(lh) ((ListMgrHandle)(lh))
#define LIST_MGR_PTR(lh) (*LIST_MGR_HANDLE(lh))

ListHandle LNew(const ListParams* params)
{
    ListMgrHandle lh;
    ListMgrRec* list;
    short cellW, cellH;

    if (!params || !params->window) {
        LIST_LOG_ERROR("LNew: invalid parameters\n");
        return NULL;
    }

    /* Allocate list handle */
    lh = (ListMgrHandle)NewHandleClear(sizeof(ListMgrRec));
    if (!lh) {
        LIST_LOG_ERROR("LNew: failed to allocate list handle\n");
        return NULL;
    }
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    /* Initialize geometry */
    list->viewRect = params->viewRect;
    list->contentRect = params->viewRect;  /* Initially same */
    
    /* Cell size from .right/.bottom of cellSizeRect */
    cellW = params->cellSizeRect.right;
    cellH = params->cellSizeRect.bottom;
    if (cellW <= 0) cellW = 200;  /* Default width */
    if (cellH <= 0) cellH = 16;   /* Default height */
    
    list->cellWidth = cellW;
    list->cellHeight = cellH;
    
    /* Initialize model */
    list->rowCount = 0;
    list->colCount = 1;  /* Default to single column */
    list->rows = NULL;
    
    /* Initialize selection */
    list->selMode = params->selMode;
    list->selRange.active = false;
    list->selectIterRow = -1;
    list->anchorCell.h = 0;
    list->anchorCell.v = 0;
    
    /* Initialize scroll */
    list->topRow = 0;
    list->leftCol = 0;
    list->vScroll = NULL;
    list->hScroll = NULL;
    
    /* Owner */
    list->window = params->window;
    
    /* Event state */
    list->lastClick.valid = false;
    
    /* Client data */
    list->refCon = params->refCon;
    
    /* Flags */
    list->hasVScroll = params->hasVScroll;
    list->hasHScroll = params->hasHScroll;
    list->active = true;
    
    /* Compute visible cells */
    List_ComputeVisibleCells(list);
    
    HUnlock((Handle)lh);
    
    LIST_LOG("LNew: view=(%d,%d,%d,%d) cell=(%dx%d) mode=%d\n",
             params->viewRect.left, params->viewRect.top,
             params->viewRect.right, params->viewRect.bottom,
             cellW, cellH, params->selMode);

    return (ListHandle)lh;  /* Cast to system ListHandle type */
}

void LDispose(ListHandle lh)
{
    ListMgrRec* list;
    short i;
    
    if (!lh) return;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    /* Free row data */
    if (list->rows) {
        RowData* rowArray = *(list->rows);
        for (i = 0; i < list->rowCount; i++) {
            if (rowArray[i].cells) {
                DisposePtr((Ptr)rowArray[i].cells);
            }
        }
        DisposeHandle((Handle)list->rows);
    }
    
    HUnlock((Handle)lh);
    DisposeHandle((Handle)lh);
    
    LIST_LOG("LDispose: list disposed\n");
}

void LSize(ListHandle lh, short newWidth, short newHeight)
{
    ListMgrRec* list;

    if (!lh) return;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    /* Update view rect size */
    list->viewRect.right = list->viewRect.left + newWidth;
    list->viewRect.bottom = list->viewRect.top + newHeight;
    list->contentRect = list->viewRect;

    /* Recompute visible cells */
    List_ComputeVisibleCells(list);

    /* Clamp scroll position */
    List_ClampScroll(list);

    /* Update scrollbars */
    List_UpdateScrollbars(list);

    /* Invalidate */
    List_InvalidateAll(list);

    HUnlock((Handle)lh);

    LIST_LOG("LSize: new size=(%dx%d) visRows=%d\n",
             newWidth, newHeight, LIST_MGR_PTR(lh)->visibleRows);
}

/* ================================================================
 * LIST MODEL OPERATIONS
 * ================================================================ */

OSErr LAddRow(ListHandle lh, short count, short afterRow)
{
    ListMgrRec* list;
    RowData* rowArray;
    RowData* newRowArray;
    Handle newRows;
    short newRowCount;
    short insertPos;
    short i, j;
    
    if (!lh || count <= 0) return paramErr;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    newRowCount = list->rowCount + count;
    insertPos = afterRow + 1;  /* Insert after specified row */
    if (insertPos < 0) insertPos = 0;
    if (insertPos > list->rowCount) insertPos = list->rowCount;
    
    /* Allocate new row array */
    newRows = NewHandleClear((Size)(newRowCount * sizeof(RowData)));
    if (!newRows) {
        HUnlock((Handle)lh);
        LIST_LOG_ERROR("LAddRow: failed to allocate row array\n");
        return memFullErr;
    }
    
    HLock(newRows);
    newRowArray = (RowData*)(*newRows);
    
    /* Copy existing rows */
    if (list->rows) {
        HLock((Handle)list->rows);  /* Lock handle before dereferencing */
        rowArray = *(list->rows);

        /* Copy rows before insertion point */
        for (i = 0; i < insertPos; i++) {
            newRowArray[i] = rowArray[i];
        }

        /* Copy rows after insertion point (shifted) */
        for (i = insertPos; i < list->rowCount; i++) {
            newRowArray[i + count] = rowArray[i];
        }

        HUnlock((Handle)list->rows);  /* Unlock after done */
    }
    
    /* Initialize new rows */
    for (i = insertPos; i < insertPos + count; i++) {
        newRowArray[i].colCount = list->colCount;
        newRowArray[i].cells = (CellData*)NewPtrClear((Size)(list->colCount * sizeof(CellData)));
        newRowArray[i].selected = false;
        
        if (!newRowArray[i].cells) {
            /* Cleanup on failure */
            for (j = insertPos; j < i; j++) {
                if (newRowArray[j].cells) {
                    DisposePtr((Ptr)newRowArray[j].cells);
                }
            }
            HUnlock(newRows);
            DisposeHandle(newRows);
            HUnlock((Handle)lh);
            return memFullErr;
        }
    }
    
    HUnlock(newRows);
    
    /* Replace row array */
    if (list->rows) {
        DisposeHandle((Handle)list->rows);
    }
    list->rows = (RowData**)newRows;
    list->rowCount = newRowCount;
    
    /* Update scrollbars */
    List_UpdateScrollbars(list);
    
    /* Invalidate */
    List_InvalidateAll(list);
    
    HUnlock((Handle)lh);
    
    LIST_LOG("LAddRow: count=%d after=%d -> rows=%d\n",
             count, afterRow, newRowCount);
    
    return noErr;
}

OSErr LDelRow(ListHandle lh, short count, short fromRow)
{
    ListMgrRec* list;
    RowData* rowArray;
    short i;
    
    if (!lh || count <= 0) return paramErr;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    if (fromRow < 0 || fromRow >= list->rowCount) {
        HUnlock((Handle)lh);
        return paramErr;
    }
    
    if (fromRow + count > list->rowCount) {
        count = list->rowCount - fromRow;
    }
    
    if (!list->rows) {
        HUnlock((Handle)lh);
        return paramErr;
    }
    
    HLock((Handle)list->rows);
    rowArray = *(list->rows);
    
    /* Free deleted rows */
    for (i = fromRow; i < fromRow + count; i++) {
        if (rowArray[i].cells) {
            DisposePtr((Ptr)rowArray[i].cells);
            rowArray[i].cells = NULL;
        }
    }
    
    /* Shift remaining rows */
    for (i = fromRow + count; i < list->rowCount; i++) {
        rowArray[i - count] = rowArray[i];
    }
    
    /* Clear vacated tail */
    for (i = list->rowCount - count; i < list->rowCount; i++) {
        rowArray[i].colCount = 0;
        rowArray[i].cells = NULL;
        rowArray[i].selected = false;
    }
    
    HUnlock((Handle)list->rows);
    
    list->rowCount -= count;
    
    /* Clamp scroll */
    List_ClampScroll(list);
    
    /* Update scrollbars */
    List_UpdateScrollbars(list);
    
    /* Invalidate */
    List_InvalidateAll(list);
    
    HUnlock((Handle)lh);
    
    LIST_LOG("LDelRow: count=%d from=%d -> rows=%d\n",
             count, fromRow, list->rowCount);
    
    return noErr;
}

OSErr LAddColumn(ListHandle lh, short count, short afterCol)
{
    ListMgrRec* list;
    RowData* rowArray;
    short newColCount;
    short insertPos;
    short i, col;

    if (!lh || count <= 0) return paramErr;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    newColCount = list->colCount + count;
    insertPos = afterCol + 1;  /* Insert after specified column */
    if (insertPos < 0) insertPos = 0;
    if (insertPos > list->colCount) insertPos = list->colCount;

    LIST_LOG("LAddColumn: adding %d columns after col %d (insertPos=%d, old colCount=%d, new colCount=%d)\n",
             count, afterCol, insertPos, list->colCount, newColCount);

    /* Update each row to accommodate new columns */
    if (list->rows) {
        HLock((Handle)list->rows);  /* Lock handle before dereferencing */
        rowArray = *(list->rows);

        for (i = 0; i < list->rowCount; i++) {
            CellData* oldCells = rowArray[i].cells;
            CellData* newCells;

            /* Allocate new cells array */
            newCells = (CellData*)NewPtrClear((Size)(newColCount * sizeof(CellData)));
            if (!newCells) {
                /* Clean up already-processed rows on allocation failure */
                HUnlock((Handle)list->rows);
                HUnlock((Handle)lh);
                LIST_LOG_ERROR("LAddColumn: failed to allocate cells for row %d\n", i);
                return memFullErr;
            }

            /* Copy cells before insertion point */
            for (col = 0; col < insertPos; col++) {
                if (col < rowArray[i].colCount) {
                    newCells[col] = oldCells[col];
                }
            }

            /* New cells at insertPos..insertPos+count-1 are already zero-initialized */

            /* Copy cells after insertion point (shifted by count) */
            for (col = insertPos; col < rowArray[i].colCount; col++) {
                newCells[col + count] = oldCells[col];
            }

            /* Replace old cells array */
            if (oldCells) {
                DisposePtr((Ptr)oldCells);
            }
            rowArray[i].cells = newCells;
            rowArray[i].colCount = newColCount;
        }
        HUnlock((Handle)list->rows);
    }

    /* Update list column count */
    list->colCount = newColCount;

    /* Recompute visible cells */
    List_ComputeVisibleCells(list);

    /* Update scrollbars */
    List_UpdateScrollbars(list);

    /* Invalidate for redraw */
    List_InvalidateAll(list);

    HUnlock((Handle)lh);
    LIST_LOG("LAddColumn: completed successfully\n");
    return noErr;
}

OSErr LDelColumn(ListHandle lh, short count, short fromCol)
{
    ListMgrRec* list;
    RowData* rowArray;
    short newColCount;
    short i, col;

    if (!lh || count <= 0) return paramErr;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    /* Validate deletion range */
    if (fromCol < 0 || fromCol >= list->colCount) {
        HUnlock((Handle)lh);
        LIST_LOG_ERROR("LDelColumn: invalid fromCol %d (colCount=%d)\n", fromCol, list->colCount);
        return paramErr;
    }

    /* Clamp count to available columns */
    if (fromCol + count > list->colCount) {
        count = list->colCount - fromCol;
    }

    newColCount = list->colCount - count;

    /* Don't allow deleting all columns */
    if (newColCount < 1) {
        HUnlock((Handle)lh);
        LIST_LOG_ERROR("LDelColumn: cannot delete all columns\n");
        return paramErr;
    }

    LIST_LOG("LDelColumn: deleting %d columns from col %d (old colCount=%d, new colCount=%d)\n",
             count, fromCol, list->colCount, newColCount);

    /* Update each row to remove columns */
    if (list->rows) {
        HLock((Handle)list->rows);
        rowArray = *(list->rows);

        for (i = 0; i < list->rowCount; i++) {
            CellData* oldCells = rowArray[i].cells;
            CellData* newCells;

            /* Allocate new cells array */
            newCells = (CellData*)NewPtrClear((Size)(newColCount * sizeof(CellData)));
            if (!newCells) {
                HUnlock((Handle)lh);
                LIST_LOG_ERROR("LDelColumn: failed to allocate cells for row %d\n", i);
                return memFullErr;
            }

            /* Copy cells before deletion point */
            for (col = 0; col < fromCol && col < rowArray[i].colCount; col++) {
                newCells[col] = oldCells[col];
            }

            /* Copy cells after deletion point (skip deleted cells) */
            for (col = fromCol + count; col < rowArray[i].colCount; col++) {
                newCells[col - count] = oldCells[col];
            }

            /* Replace old cells array */
            if (oldCells) {
                DisposePtr((Ptr)oldCells);
            }
            rowArray[i].cells = newCells;
            rowArray[i].colCount = newColCount;
        }
        HUnlock((Handle)list->rows);
    }

    /* Update list column count */
    list->colCount = newColCount;

    /* Adjust leftCol if needed */
    if (list->leftCol >= newColCount) {
        list->leftCol = newColCount - 1;
        if (list->leftCol < 0) list->leftCol = 0;
    }

    /* Recompute visible cells */
    List_ComputeVisibleCells(list);

    /* Update scrollbars */
    List_UpdateScrollbars(list);

    /* Invalidate for redraw */
    List_InvalidateAll(list);

    HUnlock((Handle)lh);
    LIST_LOG("LDelColumn: completed successfully\n");
    return noErr;
}

OSErr LSetCell(ListHandle lh, const void* data, short dataLen, Cell cell)
{
    ListMgrRec* list;
    CellData* cellData;
    short copyLen;
    
    if (!lh || !data) return paramErr;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    cellData = List_GetCellData(list, cell);
    if (!cellData) {
        HUnlock((Handle)lh);
        return paramErr;
    }
    
    /* Cap length */
    copyLen = dataLen;
    if (copyLen > MAX_CELL_DATA) {
        copyLen = MAX_CELL_DATA;
    }
    
    cellData->len = (unsigned char)copyLen;
    if (copyLen > 0) {
        memcpy(cellData->data, data, copyLen);
    }
    
    HUnlock((Handle)lh);
    return noErr;
}

short LGetCell(ListHandle lh, void* out, short outMax, Cell cell)
{
    ListMgrRec* list;
    CellData* cellData;
    short copyLen;
    
    if (!lh || !out) return 0;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    cellData = List_GetCellData(list, cell);
    if (!cellData) {
        HUnlock((Handle)lh);
        return 0;
    }
    
    copyLen = cellData->len;
    if (copyLen > outMax) {
        copyLen = outMax;
    }
    
    if (copyLen > 0) {
        memcpy(out, cellData->data, copyLen);
    }
    
    HUnlock((Handle)lh);
    return copyLen;
}

void LSetRefCon(ListHandle lh, long refCon)
{
    if (!lh) return;
    LIST_MGR_PTR(lh)->refCon = refCon;
}

long LGetRefCon(ListHandle lh)
{
    if (!lh) return 0;
    return LIST_MGR_PTR(lh)->refCon;
}

/* Forward declarations for internal functions */
extern void List_DrawCell(ListMgrRec* list, const Rect* cellRect, short row, short col, Boolean selected);
extern void List_EraseBackground(ListMgrRec* list, const Rect* updateRect);
extern void List_InvalidateBand(ListMgrRec* list, short dRows);

/* ================================================================
 * DRAWING AND UPDATE
 * ================================================================ */

void LUpdate(ListHandle lh, RgnHandle updateRgn)
{
    ListMgrRec* list;
    short row, col;
    short startRow, endRow;
    Rect cellRect;
    Boolean selected;

    if (!lh) return;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    /* Note: Port/pen/clip save/restore handled in individual drawing functions */

    /* Draw visible cells */
    startRow = list->topRow;
    endRow = list->topRow + list->visibleRows;
    if (endRow > list->rowCount) {
        endRow = list->rowCount;
    }

    /* Narrow erase: only erase area containing actual content rows */
    {
        Rect eraseRect = list->contentRect;
        short contentRows = endRow - startRow;
        if (contentRows > 0) {
            eraseRect.bottom = eraseRect.top + (contentRows * list->cellHeight);
        } else {
            eraseRect.bottom = eraseRect.top;  /* No rows to display */
        }
        List_EraseBackground(list, &eraseRect);
    }

    for (row = startRow; row < endRow; row++) {
        for (col = 0; col < list->colCount; col++) {
            Cell c;
            c.h = col;  /* horizontal = column */
            c.v = row;  /* vertical = row */

            LGetCellRect(lh, c, &cellRect);
            selected = List_IsCellSelected(list, c);
            List_DrawCell(list, &cellRect, row, col, selected);
        }
    }

    HUnlock((Handle)lh);
}

void LDraw(ListHandle lh)
{
    /* Full redraw - pass NULL updateRgn (handled in LUpdate) */
    LUpdate(lh, NULL);
}

void LGetCellRect(ListHandle lh, Cell cell, Rect* outCellRect)
{
    ListMgrRec* list;
    short rowOffset, colOffset;

    if (!lh || !outCellRect) return;

    list = *LIST_MGR_HANDLE(lh);

    /* Validate cell indices */
    if (cell.v < 0 || cell.v >= list->rowCount ||
        cell.h < 0 || cell.h >= list->colCount) {
        outCellRect->top = 0;
        outCellRect->left = 0;
        outCellRect->bottom = 0;
        outCellRect->right = 0;
        return;
    }

    rowOffset = (cell.v - list->topRow) * list->cellHeight;
    colOffset = (cell.h - list->leftCol) * list->cellWidth;
    
    outCellRect->left = list->contentRect.left + colOffset;
    outCellRect->top = list->contentRect.top + rowOffset;
    outCellRect->right = outCellRect->left + list->cellWidth;
    outCellRect->bottom = outCellRect->top + list->cellHeight;
}

void LScroll(ListHandle lh, short dRows, short dCols)
{
    ListMgrRec* list;
    short rowsPerStep, colsPerStep;
    short numSteps;
    short step;

    if (!lh) return;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    /* Use smooth scrolling animation with 5 steps */
    #define SCROLL_STEPS 5
    numSteps = SCROLL_STEPS;
    rowsPerStep = (dRows > 0) ? (dRows + numSteps - 1) / numSteps : (dRows - numSteps + 1) / numSteps;
    colsPerStep = (dCols > 0) ? (dCols + numSteps - 1) / numSteps : (dCols - numSteps + 1) / numSteps;

    /* Scroll in incremental steps */
    for (step = 0; step < numSteps; step++) {
        short stepRows = rowsPerStep;
        short stepCols = colsPerStep;

        /* Adjust last step to reach exact destination */
        if (step == numSteps - 1) {
            stepRows = dRows - (rowsPerStep * (numSteps - 1));
            stepCols = dCols - (colsPerStep * (numSteps - 1));
        }

        list->topRow += stepRows;
        list->leftCol += stepCols;

        List_ClampScroll(list);
        List_UpdateScrollbars(list);

        /* Invalidate only the exposed band for efficient redraw */
        if (stepRows != 0) {
            List_InvalidateBand(list, stepRows);
        }
        if (stepCols != 0) {
            List_InvalidateAll(list);  /* Horizontal scroll still needs full invalidate */
        }

        /* Allow some time for drawing on the last step only (for visual effect) */
        if (step < numSteps - 1) {
            /* Small delay to make scrolling visible */
            UInt32 startTime = TickCount();
            while (TickCount() - startTime < 2) {
                /* Yield to allow drawing */
                SystemTask();
            }
        }
    }

    HUnlock((Handle)lh);

    LIST_LOG("LScroll: dRows=%d dCols=%d (smooth, %d steps) -> topRow=%d\n",
             dRows, dCols, numSteps, LIST_MGR_PTR(lh)->topRow);
}

/* ================================================================
 * SELECTION
 * ================================================================ */

Boolean LClick(ListHandle lh, Point localWhere, unsigned short mods, short* outItem)
{
    extern UInt32 GetDblTime(void);
    ListMgrRec* list;
    Cell hitCell;
    Boolean selChanged = false;
    Boolean wasSelected;
    Boolean isDoubleClick = false;

    if (!lh) return false;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    /* Hit test */
    if (!List_HitTest(list, localWhere, &hitCell)) {
        HUnlock((Handle)lh);
        return false;
    }

    /* Detect double-click: same cell within GetDblTime threshold */
    UInt32 now = TickCount();
    if (list->lastClick.valid &&
        hitCell.v == list->lastClick.cell.v &&
        hitCell.h == list->lastClick.cell.h &&
        (now - list->lastClick.when) < GetDblTime()) {
        isDoubleClick = true;
    }

    /* Record this click */
    list->lastClick.cell = hitCell;
    list->lastClick.when = now;
    list->lastClick.mods = mods;
    list->lastClick.valid = true;
    
    wasSelected = List_IsCellSelected(list, hitCell);

    if (list->selMode == lsSingleSel) {
        /* Single selection: clear others, select this */
        List_ClearAllSelection(list);
        List_SetCellSelection(list, hitCell, true);
        list->anchorCell = hitCell;
        selChanged = true;
    } else {
        /* Multi-selection: check modifiers */
        if (mods & 0x0100) {
            /* Cmd key: toggle clicked cell */
            List_SetCellSelection(list, hitCell, !wasSelected);
            list->anchorCell = hitCell;
            selChanged = true;
        } else if (mods & 0x0200) {
            /* Shift key: extend from anchor to clicked cell */
            short startRow, endRow, row;
            Cell c;

            /* Clear existing selection */
            List_ClearAllSelection(list);

            /* Select range from anchor to clicked (single column) */
            startRow = list->anchorCell.v;
            endRow = hitCell.v;
            if (startRow > endRow) {
                short temp = startRow;
                startRow = endRow;
                endRow = temp;
            }

            for (row = startRow; row <= endRow; row++) {
                c.h = 0;
                c.v = row;
                List_SetCellSelection(list, c, true);
            }
            /* Don't update anchor on Shift-extend */
            selChanged = true;
        } else {
            /* No modifiers: clear others, select this */
            List_ClearAllSelection(list);
            List_SetCellSelection(list, hitCell, true);
            list->anchorCell = hitCell;
            selChanged = true;
        }
    }
    
    if (outItem) {
        *outItem = hitCell.v;  /* Return row index */
    }
    
    /* Invalidate to show selection change */
    if (selChanged) {
        List_InvalidateAll(list);
    }
    
    HUnlock((Handle)lh);

    LIST_LOG("LClick: cell(%d,%d) mods=0x%x sel=%d dblClick=%d\n",
             hitCell.v, hitCell.h, mods, selChanged, isDoubleClick);

    /* Mac API: LClick returns true on double-click (item activation) */
    return isDoubleClick;
}

Boolean LGetSelect(ListHandle lh, Cell* outCell)
{
    ListMgrRec* list;
    short row;
    
    if (!lh || !outCell) return false;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    /* Iterate from selectIterRow */
    if (list->selectIterRow < 0) {
        list->selectIterRow = 0;
    }
    
    for (row = list->selectIterRow; row < list->rowCount; row++) {
        Cell c;
        c.v = row;
        c.h = 0;
        
        if (List_IsCellSelected(list, c)) {
            outCell->v = row;
            outCell->h = 0;
            list->selectIterRow = row + 1;
            HUnlock((Handle)lh);
            return true;
        }
    }
    
    /* Reset iterator */
    list->selectIterRow = -1;
    HUnlock((Handle)lh);
    return false;
}

void LSetSelect(ListHandle lh, Boolean sel, Cell cell)
{
    ListMgrRec* list;

    if (!lh) return;

    HLock((Handle)lh);
    list = LIST_MGR_PTR(lh);
    List_SetCellSelection(list, cell, sel);
    List_InvalidateAll(list);
    HUnlock((Handle)lh);
}

void LSelectAll(ListHandle lh)
{
    ListMgrRec* list;
    short row;
    Cell c;
    
    if (!lh) return;
    
    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);
    
    for (row = 0; row < list->rowCount; row++) {
        c.v = row;
        c.h = 0;
        List_SetCellSelection(list, c, true);
    }
    
    List_InvalidateAll(list);
    HUnlock((Handle)lh);
}

void LClearSelect(ListHandle lh)
{
    ListMgrRec* list;

    if (!lh) return;

    HLock((Handle)lh);
    list = LIST_MGR_PTR(lh);
    List_ClearAllSelection(list);
    List_InvalidateAll(list);
    HUnlock((Handle)lh);
}

Boolean LLastClick(ListHandle lh, Cell* outCell, UInt32* outWhen, unsigned short* outMods)
{
    ListMgrRec* list;
    
    if (!lh) return false;
    
    list = *LIST_MGR_HANDLE(lh);
    if (!list->lastClick.valid) return false;
    
    if (outCell) *outCell = list->lastClick.cell;
    if (outWhen) *outWhen = list->lastClick.when;
    if (outMods) *outMods = list->lastClick.mods;
    
    return true;
}

/* ================================================================
 * SEARCH (Stubbed)
 * ================================================================ */

Boolean LSearch(ListHandle lh, const unsigned char* pStr, Boolean caseSensitive, Cell* outFound)
{
    ListMgrRec* list;
    SInt16 row, col;
    Cell searchCell;
    unsigned char cellStr[256];
    SInt16 len;
    SInt16 searchLen;
    Boolean found = false;

    if (!lh || !pStr || !outFound) {
        return false;
    }

    HLock((Handle)lh);
    list = (ListMgrRec*)*lh;

    searchLen = pStr[0];  /* Pascal string length */
    if (searchLen == 0) {
        HUnlock((Handle)lh);
        return false;
    }

    LIST_LOG("LSearch: searching for '%.*s' (len=%d, case=%s)\n",
             searchLen, &pStr[1], searchLen, caseSensitive ? "sensitive" : "insensitive");

    /* Search through all cells */
    for (row = 0; row < list->rowCount && !found; row++) {
        for (col = 0; col < list->colCount && !found; col++) {
            searchCell.h = col;
            searchCell.v = row;

            /* Get cell data */
            len = LGetCell(lh, cellStr, sizeof(cellStr), searchCell);
            if (len > 0) {
                /* Compare strings */
                if (caseSensitive) {
                    /* Case-sensitive comparison */
                    if (len >= searchLen &&
                        memcmp(&cellStr[1], &pStr[1], searchLen) == 0) {
                        found = true;
                        *outFound = searchCell;
                        LIST_LOG("LSearch: found at cell (%d,%d)\n", col, row);
                    }
                } else {
                    /* Case-insensitive comparison */
                    SInt16 i;
                    Boolean match = (len >= searchLen);

                    for (i = 0; i < searchLen && match; i++) {
                        unsigned char c1 = cellStr[1 + i];
                        unsigned char c2 = pStr[1 + i];

                        /* Convert to uppercase for comparison */
                        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
                        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;

                        if (c1 != c2) {
                            match = false;
                        }
                    }

                    if (match) {
                        found = true;
                        *outFound = searchCell;
                        LIST_LOG("LSearch: found (case-insensitive) at cell (%d,%d)\n", col, row);
                    }
                }
            }
        }
    }

    HUnlock((Handle)lh);

    if (!found) {
        LIST_LOG("LSearch: not found\n");
    }

    return found;
}

/* ================================================================
 * KEYBOARD HANDLING
 * ================================================================ */

/*
 * Key codes (ASCII character codes, not raw scan codes)
 * Note: LKey expects event.message & charCodeMask from EventRecord,
 * which has already been mapped from scan code to character code.
 */
#define kUpArrow    0x1E
#define kDownArrow  0x1F
#define kLeftArrow  0x1C
#define kRightArrow 0x1D
#define kPageUp     0x0B  /* Page Up */
#define kPageDown   0x0C  /* Page Down */
#define kHome       0x01
#define kEnd        0x04
#define kSpace      ' '

/*
 * LKey - Handle keyboard navigation in list
 * ch: character code from (event.message & charCodeMask), NOT raw scan code
 * Returns: true if key was handled
 */
Boolean LKey(ListHandle lh, char ch)
{
    ListMgrRec* list;
    Cell currentCell;
    short newRow;
    Boolean handled = false;

    if (!lh) return false;

    HLock((Handle)lh);
    list = *LIST_MGR_HANDLE(lh);

    /* Find current selection (use anchor as current) */
    currentCell = list->anchorCell;
    newRow = currentCell.v;

    switch (ch) {
        case kUpArrow:
            if (newRow > 0) {
                newRow--;
                handled = true;
            }
            break;

        case kDownArrow:
            if (newRow < list->rowCount - 1) {
                newRow++;
                handled = true;
            }
            break;

        case kPageUp:
            newRow -= list->visibleRows;
            if (newRow < 0) newRow = 0;
            handled = true;
            break;

        case kPageDown:
            newRow += list->visibleRows;
            if (newRow >= list->rowCount) newRow = list->rowCount - 1;
            handled = true;
            break;

        case kHome:
            newRow = 0;
            handled = true;
            break;

        case kEnd:
            newRow = list->rowCount - 1;
            handled = true;
            break;

        case kSpace:
            /* Toggle selection in multi-select mode */
            if (list->selMode == lsMultiSel) {
                Boolean wasSelected = List_IsCellSelected(list, currentCell);
                List_SetCellSelection(list, currentCell, !wasSelected);
                List_InvalidateAll(list);
                handled = true;
            }
            break;

        default:
            break;
    }

    if (handled && newRow != currentCell.v) {
        /* Move selection to new row */
        Cell newCell;
        newCell.h = 0;
        newCell.v = newRow;

        List_ClearAllSelection(list);
        List_SetCellSelection(list, newCell, true);
        list->anchorCell = newCell;

        /* Auto-scroll if needed */
        if (newRow < list->topRow) {
            list->topRow = newRow;
            List_ClampScroll(list);
            List_UpdateScrollbars(list);
        } else if (newRow >= list->topRow + list->visibleRows) {
            list->topRow = newRow - list->visibleRows + 1;
            List_ClampScroll(list);
            List_UpdateScrollbars(list);
        }

        List_InvalidateAll(list);
    }

    HUnlock((Handle)lh);

    if (handled) {
        LIST_LOG("LKey: ch=0x%02x handled\n", (unsigned char)ch);
    }
    return handled;
}

/* ================================================================
 * SCROLLBAR INTEGRATION
 * ================================================================ */

void LAttachScrollbars(ListHandle lh, ControlHandle vScroll, ControlHandle hScroll)
{
    if (!lh) return;
    
    LIST_MGR_PTR(lh)->vScroll = vScroll;
    LIST_MGR_PTR(lh)->hScroll = hScroll;

    List_UpdateScrollbars(LIST_MGR_PTR(lh));
    
    LIST_LOG("LAttachScrollbars: v=%p h=%p\n", vScroll, hScroll);
}

/* ================================================================
 * DIALOG INTEGRATION
 * ================================================================ */

ListHandle ListFromDialogItem(DialogPtr dlg, short itemNo)
{
    DialogListAssoc* assoc;
    
    assoc = FindDialogListSlot(dlg, itemNo, false);
    if (assoc && assoc->active) {
        return assoc->list;
    }
    return NULL;
}

void AttachListToDialogItem(DialogPtr dlg, short itemNo, ListHandle lh)
{
    DialogListAssoc* assoc;
    
    assoc = FindDialogListSlot(dlg, itemNo, true);
    if (assoc) {
        assoc->list = lh;
        LIST_LOG("AttachListToDialogItem: dlg=%p item=%d list=%p\n",
                 dlg, itemNo, lh);
    }
}

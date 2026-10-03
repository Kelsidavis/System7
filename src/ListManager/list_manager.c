/*
 * list_manager.c - List Manager Internal Implementation
 *
 * Internal drawing, hit testing, scrolling, and utility functions
 * for the List Manager. Not for direct client use.
 */

#include <string.h>
#include "SystemTypes.h"
#include "ListManager/ListManager.h"
#include "ListManager/ListManagerInternal.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "ControlManager/ControlManager.h"
#include "System71StdLib.h"
#include "ListManager/ListLogging.h"

/* ================================================================
 * GEOMETRY CALCULATIONS
 * ================================================================ */

void List_ComputeVisibleCells(ListMgrRec* list)
{
    short viewWidth, viewHeight;

    if (!list) return;

    viewWidth = list->viewRect.right - list->viewRect.left;
    viewHeight = list->viewRect.bottom - list->viewRect.top;

    list->visibleRows = 0;
    list->visibleCols = 0;

    if (list->cellHeight > 0) {
        /* Integer division gives fully visible cells - safe, no overdraw */
        list->visibleRows = viewHeight / list->cellHeight;
    }

    if (list->cellWidth > 0) {
        list->visibleCols = viewWidth / list->cellWidth;
    }

    /* Update content rect (could reserve space for scrollbars) */
    list->contentRect = list->viewRect;
}

void List_ClampScroll(ListMgrRec* list)
{
    short maxTopRow;

    if (!list) return;

    /* Clamp topRow */
    if (list->topRow < 0) {
        list->topRow = 0;
    }

    maxTopRow = list->rowCount - list->visibleRows + 1;
    if (maxTopRow < 0) maxTopRow = 0;

    if (list->topRow > maxTopRow) {
        list->topRow = maxTopRow;
    }

    /* Clamp leftCol */
    if (list->leftCol < 0) {
        list->leftCol = 0;
    }
}

/* ================================================================
 * DRAWING FUNCTIONS
 * ================================================================ */

void List_EraseBackground(ListMgrRec* list, const Rect* updateRect)
{
    GrafPtr savePort;
    RgnHandle saveClip;

    if (!list || !updateRect) return;

    GetPort(&savePort);
    SetPort((GrafPtr)list->window);

    /* Save and set clip to viewRect */
    saveClip = NewRgn();
    if (saveClip) {
        GetClip(saveClip);
        ClipRect(&list->viewRect);
    }

    /* Erase to white */
    EraseRect(updateRect);

    /* Restore clip and port */
    if (saveClip) {
        SetClip(saveClip);
        DisposeRgn(saveClip);
    }
    SetPort(savePort);
}

void List_DrawCell(ListMgrRec* list, const Rect* cellRect, short row, short col, Boolean selected)
{
    GrafPtr savePort;
    PenState savePen;
    RgnHandle saveClip;
    Cell c;
    CellData* cellData;
    unsigned char textBuf[256];
    short textLen;
    short textH, textV;

    if (!list || !cellRect) return;

    c.v = row;
    c.h = col;

    cellData = List_GetCellData(list, c);
    if (!cellData || cellData->len == 0) {
        /* Empty cell - no content to draw */
        return;
    }

    GetPort(&savePort);
    SetPort((GrafPtr)list->window);

    /* Save pen state and clip */
    GetPenState(&savePen);
    saveClip = NewRgn();
    if (saveClip) {
        GetClip(saveClip);
        ClipRect(&list->viewRect);
    }

    /* Draw selection background */
    if (selected) {
        if (list->active) {
            /* Active window: fill with gray */
            FillRect(cellRect, &qd.gray);
        } else {
            /* Inactive window: lighter gray */
            FillRect(cellRect, &qd.ltGray);
        }
    }

    /* Draw cell text */
    textLen = cellData->len;
    if (textLen > 255) textLen = 255;

    /* Prepare Pascal string */
    textBuf[0] = (unsigned char)textLen;
    if (textLen > 0) {
        memcpy(&textBuf[1], cellData->data, textLen);
    }

    /* Text position: left margin + baseline */
    textH = cellRect->left + 2;
    textV = cellRect->top + 12;  /* Baseline for 16px cell height */

    MoveTo(textH, textV);
    DrawString(textBuf);

    SetPenState(&savePen);
    if (saveClip) {
        SetClip(saveClip);
        DisposeRgn(saveClip);
    }
    SetPort(savePort);
}

void List_InvalidateAll(ListMgrRec* list)
{
    if (!list) return;
    InvalRect(&list->viewRect);
}

/*
 * List_InvalidateBand - Invalidate only the exposed band when scrolling
 * dRows: number of rows scrolled (positive=forward, negative=backward)
 */
void List_InvalidateBand(ListMgrRec* list, short dRows)
{
    Rect bandRect;
    short bandHeight;

    if (!list || dRows == 0) return;

    bandRect = list->contentRect;
    bandHeight = (dRows > 0 ? dRows : -dRows) * list->cellHeight;

    /* Clamp band height to content area */
    if (bandHeight > (bandRect.bottom - bandRect.top)) {
        bandHeight = bandRect.bottom - bandRect.top;
    }

    if (dRows > 0) {
        /* Scrolled forward: invalidate band at bottom */
        bandRect.top = bandRect.bottom - bandHeight;
    } else {
        /* Scrolled backward: invalidate band at top */
        bandRect.bottom = bandRect.top + bandHeight;
    }

    InvalRect(&bandRect);
}

/* ================================================================
 * HIT TESTING
 * ================================================================ */

Boolean List_HitTest(ListMgrRec* list, Point localPt, Cell* outCell)
{
    short relH, relV;
    short hitRow, hitCol;

    if (!list || !outCell) return false;

    /* Check if point is in view rect */
    if (localPt.h < list->contentRect.left || localPt.h >= list->contentRect.right ||
        localPt.v < list->contentRect.top || localPt.v >= list->contentRect.bottom) {
        return false;
    }

    /* Compute relative position */
    relH = localPt.h - list->contentRect.left;
    relV = localPt.v - list->contentRect.top;

    /* Compute cell indices */
    if (list->cellHeight > 0) {
        hitRow = list->topRow + (relV / list->cellHeight);
    } else {
        hitRow = 0;
    }

    if (list->cellWidth > 0) {
        hitCol = list->leftCol + (relH / list->cellWidth);
    } else {
        hitCol = 0;
    }

    /* Validate */
    if (hitRow < 0 || hitRow >= list->rowCount) return false;
    if (hitCol < 0 || hitCol >= list->colCount) return false;

    outCell->v = hitRow;
    outCell->h = hitCol;
    return true;
}

/* ================================================================
 * SELECTION MANAGEMENT
 * ================================================================ */

Boolean List_IsCellSelected(ListMgrRec* list, Cell cell)
{
    RowData* rowArray;

    if (!list || !List_ValidateCell(list, cell)) return false;
    if (!list->rows) return false;

    rowArray = *(list->rows);
    return rowArray[cell.v].selected;
}

void List_SetCellSelection(ListMgrRec* list, Cell cell, Boolean selected)
{
    RowData* rowArray;

    if (!list || !List_ValidateCell(list, cell)) return;
    if (!list->rows) return;

    rowArray = *(list->rows);
    rowArray[cell.v].selected = selected;
}

void List_ClearAllSelection(ListMgrRec* list)
{
    RowData* rowArray;
    short i;

    if (!list || !list->rows) return;

    rowArray = *(list->rows);
    for (i = 0; i < list->rowCount; i++) {
        rowArray[i].selected = false;
    }
}

/* ================================================================
 * CELL DATA ACCESS
 * ================================================================ */

CellData* List_GetCellData(ListMgrRec* list, Cell cell)
{
    RowData* rowArray;

    if (!list || !List_ValidateCell(list, cell)) return NULL;
    if (!list->rows) return NULL;

    rowArray = *(list->rows);
    if (!rowArray[cell.v].cells) return NULL;

    if (cell.h >= rowArray[cell.v].colCount) return NULL;

    return &rowArray[cell.v].cells[cell.h];
}

Boolean List_ValidateCell(ListMgrRec* list, Cell cell)
{
    if (!list) return false;
    if (cell.v < 0 || cell.v >= list->rowCount) return false;
    if (cell.h < 0 || cell.h >= list->colCount) return false;
    return true;
}

/* ================================================================
 * SCROLLBAR INTEGRATION
 * ================================================================ */

void List_UpdateScrollbars(ListMgrRec* list)
{
    short maxVal;

    if (!list) return;

    /* Update vertical scrollbar with proportional thumb sizing */
    if (list->vScroll) {
        maxVal = list->rowCount - list->visibleRows;
        if (maxVal < 0) maxVal = 0;

        /* Use UpdateScrollThumb to set value, range, AND visible span for proportional sizing */
        UpdateScrollThumb(list->vScroll, list->topRow, 0, maxVal, list->visibleRows);
    }

    /* Update horizontal scrollbar with proportional thumb sizing */
    if (list->hScroll) {
        maxVal = list->colCount - list->visibleCols;
        if (maxVal < 0) maxVal = 0;

        UpdateScrollThumb(list->hScroll, list->leftCol, 0, maxVal, list->visibleCols);
    }
}

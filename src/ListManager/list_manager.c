/*
 * ===========================================================================
 * RE-AGENT-BANNER: Apple System 7.1 List Manager Implementation
 * ===========================================================================
 * Artifact: System.rsrc
 * SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba
 * Architecture: m68k
 * ABI: macos_system7_1
 *
 * Implementation based on reverse engineering evidence:
 * - 132 trap occurrences analyzed
 * - ListRec structure: 84 bytes
 * - Cell structure: 4 bytes
 * - LDEF resource type support
 *
 * Provenance: evidence.curated.listmgr.json, mappings.listmgr.json
 * ===========================================================================
 */

#include "list_manager.h"
#include <Memory.h>
#include <Resources.h>
#include <Events.h>
#include <ToolUtils.h>

/* ---- Internal Constants -------------------------------------------------*/
#define kDefaultCellHeight 16   /* NOTE: Standard cell height in pixels */
#define kDefaultCellWidth  80   /* NOTE: Standard cell width in pixels */
#define kScrollBarWidth    15   /* NOTE: Standard scroll bar width */
#define kCellDataChunk    256   /* NOTE: Cell data allocation chunk size */

/* ---- Internal Utility Functions -----------------------------------------*/

/* PROV: Calculate visible cells from view rect and cell size */
static void CalcVisibleCells(ListHandle lHandle) {
    ListPtr listP = *lHandle;
    short visWidth, visHeight;

    visWidth = (listP->rView.right - listP->rView.left) / listP->cellSize.h;
    visHeight = (listP->rView.bottom - listP->rView.top) / listP->cellSize.v;

    listP->visible.left = 0;
    listP->visible.top = 0;
    listP->visible.right = visWidth;
    listP->visible.bottom = visHeight;
}

/* PROV: Initialize cell data storage */
static Handle AllocateCellData(const Rect* dataBounds) {
    long dataSize;
    Handle cells;

    /* Calculate initial data size based on bounds */
    dataSize = (dataBounds->right - dataBounds->left) *
               (dataBounds->bottom - dataBounds->top) * sizeof(Handle);

    if (dataSize < kCellDataChunk) {
        dataSize = kCellDataChunk;
    }

    cells = NewHandleClear(dataSize);
    return cells;
}

/* PROV: Call list definition procedure */
static void CallLDEF(ListHandle lHandle, short message, Boolean select,
                    Rect* cellRect, Cell cell, short offset, short len) {
    ListPtr listP = *lHandle;
    ListDefProcPtr ldefProc;

    if (listP->listDefProc) {
        HLock(listP->listDefProc);
        ldefProc = (ListDefProcPtr)*listP->listDefProc;
        (*ldefProc)(message, select, cellRect, cell, offset, len, lHandle);
        HUnlock(listP->listDefProc);
    }
}

/* ===========================================================================
 * Trap 0xA9E7: LNew
 * PROV: 80 occurrences found @ addresses documented in evidence file
 * Creates a new list with specified parameters
 * ===========================================================================
 */
ListHandle LNew(const Rect* rView, const Rect* dataBounds, Point cSize,
                short theProc, WindowPtr theWindow, Boolean drawIt,
                Boolean hasGrow, Boolean scrollHoriz, Boolean scrollVert) {
    ListHandle lHandle;
    ListPtr listP;
    Handle ldefHandle;
    Rect scrollRect;

    /* PROV: Allocate ListRec structure (84 bytes) */
    lHandle = (ListHandle)NewHandleClear(sizeof(ListRec));
    if (!lHandle) {
        return NULL;
    }

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Initialize rView @ offset 0x00 */
    listP->rView = *rView;

    /* PROV: Initialize port @ offset 0x08 */
    listP->port = theWindow ? GetWindowPort(theWindow) : qd.thePort;

    /* PROV: Initialize cellSize @ offset 0x10 */
    listP->cellSize = cSize;
    if (listP->cellSize.h == 0) listP->cellSize.h = kDefaultCellWidth;
    if (listP->cellSize.v == 0) listP->cellSize.v = kDefaultCellHeight;

    /* PROV: Initialize dataBounds @ offset 0x48 */
    listP->dataBounds = *dataBounds;

    /* PROV: Calculate visible bounds @ offset 0x14 */
    CalcVisibleCells(lHandle);

    /* PROV: Create vertical scroll bar @ offset 0x1C */
    if (scrollVert) {
        scrollRect.left = rView->right - kScrollBarWidth;
        scrollRect.top = rView->top;
        scrollRect.right = rView->right;
        scrollRect.bottom = rView->bottom - (hasGrow ? kScrollBarWidth : 0);

        listP->vScroll = NewControl(theWindow, &scrollRect, "\p", drawIt,
                                    0, 0, 0, scrollBarProc, (long)lHandle);
    }

    /* PROV: Create horizontal scroll bar @ offset 0x20 */
    if (scrollHoriz) {
        scrollRect.left = rView->left;
        scrollRect.top = rView->bottom - kScrollBarWidth;
        scrollRect.right = rView->right - (scrollVert ? kScrollBarWidth : 0);
        scrollRect.bottom = rView->bottom;

        listP->hScroll = NewControl(theWindow, &scrollRect, "\p", drawIt,
                                    0, 0, 0, scrollBarProc, (long)lHandle);
    }

    /* PROV: Load LDEF resource @ offset 0x40 */
    if (theProc != 0) {
        ldefHandle = GetResource('LDEF', theProc);
        if (ldefHandle) {
            LoadResource(ldefHandle);
            listP->listDefProc = ldefHandle;
        }
    }

    /* PROV: Initialize cells data @ offset 0x50 */
    listP->cells = AllocateCellData(dataBounds);

    /* PROV: Set list flags @ offset 0x27 */
    listP->listFlags = 0;
    if (drawIt) listP->listFlags |= 0x01;
    if (hasGrow) listP->listFlags |= 0x02;
    if (scrollHoriz) listP->listFlags |= 0x04;
    if (scrollVert) listP->listFlags |= 0x08;

    /* PROV: Set active flag @ offset 0x25 */
    listP->lActive = true;

    /* PROV: Initialize click tracking @ offsets 0x28, 0x2C */
    listP->clikTime = 0;
    listP->clikLoc.h = listP->clikLoc.v = -1;

    HUnlock((Handle)lHandle);

    return lHandle;
}

/* ===========================================================================
 * Trap 0xA9E8: LDispose
 * PROV: Required by API though not found in evidence
 * Disposes of a list and its associated resources
 * ===========================================================================
 */
void LDispose(ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Dispose scroll bars */
    if (listP->vScroll) {
        DisposeControl(listP->vScroll);
    }
    if (listP->hScroll) {
        DisposeControl(listP->hScroll);
    }

    /* PROV: Dispose cell data */
    if (listP->cells) {
        DisposHandle(listP->cells);
    }

    /* PROV: Release LDEF if loaded */
    if (listP->listDefProc) {
        ReleaseResource(listP->listDefProc);
    }

    /* PROV: Dispose user handle if present */
    if (listP->userHandle) {
        DisposHandle(listP->userHandle);
    }

    HUnlock((Handle)lHandle);
    DisposHandle((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9E9: LAddColumn
 * PROV: 3 occurrences @ 0x00014551, 0x00014699, 0x0001472f
 * Adds columns to the list
 * ===========================================================================
 */
short LAddColumn(short count, short colNum, ListHandle lHandle) {
    ListPtr listP;
    short newCol;

    if (!lHandle || count <= 0) return -1;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Adjust data bounds for new columns */
    if (colNum < 0 || colNum > listP->dataBounds.right) {
        colNum = listP->dataBounds.right;
    }

    newCol = colNum;
    listP->dataBounds.right += count;

    /* TODO_EVID: Reallocate cell data for new columns */
    /* NOTE: Need evidence for exact cell data reallocation strategy */

    /* PROV: Update scroll bar ranges */
    if (listP->hScroll) {
        SetControlMaximum(listP->hScroll,
                         listP->dataBounds.right - listP->visible.right);
    }

    HUnlock((Handle)lHandle);
    return newCol;
}

/* ===========================================================================
 * Trap 0xA9EA: LAddRow
 * PROV: 2 occurrences @ 0x00023dd4, 0x00030572
 * Adds rows to the list
 * ===========================================================================
 */
short LAddRow(short count, short rowNum, ListHandle lHandle) {
    ListPtr listP;
    short newRow;

    if (!lHandle || count <= 0) return -1;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Adjust data bounds for new rows */
    if (rowNum < 0 || rowNum > listP->dataBounds.bottom) {
        rowNum = listP->dataBounds.bottom;
    }

    newRow = rowNum;
    listP->dataBounds.bottom += count;

    /* TODO_EVID: Reallocate cell data for new rows */
    /* NOTE: Need evidence for exact cell data reallocation strategy */

    /* PROV: Update scroll bar ranges */
    if (listP->vScroll) {
        SetControlMaximum(listP->vScroll,
                         listP->dataBounds.bottom - listP->visible.bottom);
    }

    HUnlock((Handle)lHandle);
    return newRow;
}

/* ===========================================================================
 * Trap 0xA9EB: LDelColumn
 * PROV: 26 occurrences @ addresses documented in evidence file
 * Deletes columns from the list
 * ===========================================================================
 */
void LDelColumn(short count, short colNum, ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle || count <= 0) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Validate column range */
    if (colNum < 0 || colNum >= listP->dataBounds.right) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* PROV: Adjust count if needed */
    if (colNum + count > listP->dataBounds.right) {
        count = listP->dataBounds.right - colNum;
    }

    /* TODO_EVID: Shift cell data to remove columns */
    /* NOTE: Need evidence for exact cell data shifting algorithm */

    /* PROV: Update data bounds */
    listP->dataBounds.right -= count;

    /* PROV: Update scroll bar ranges */
    if (listP->hScroll) {
        SetControlMaximum(listP->hScroll,
                         listP->dataBounds.right - listP->visible.right);
    }

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9EC: LDelRow
 * PROV: 2 occurrences @ 0x0002da21, 0x00032093
 * Deletes rows from the list
 * ===========================================================================
 */
void LDelRow(short count, short rowNum, ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle || count <= 0) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Validate row range */
    if (rowNum < 0 || rowNum >= listP->dataBounds.bottom) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* PROV: Adjust count if needed */
    if (rowNum + count > listP->dataBounds.bottom) {
        count = listP->dataBounds.bottom - rowNum;
    }

    /* TODO_EVID: Shift cell data to remove rows */
    /* NOTE: Need evidence for exact cell data shifting algorithm */

    /* PROV: Update data bounds */
    listP->dataBounds.bottom -= count;

    /* PROV: Update scroll bar ranges */
    if (listP->vScroll) {
        SetControlMaximum(listP->vScroll,
                         listP->dataBounds.bottom - listP->visible.bottom);
    }

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9ED: LGetSelect
 * PROV: 13 occurrences @ addresses documented in evidence file
 * Gets the next selected cell
 * ===========================================================================
 */
Boolean LGetSelect(Boolean next, Cell* theCell, ListHandle lHandle) {
    ListPtr listP;
    short row, col;
    Boolean found = false;

    if (!lHandle || !theCell) return false;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Start search from current cell or beginning */
    if (next) {
        row = theCell->v;
        col = theCell->h + 1;
        if (col >= listP->dataBounds.right) {
            col = 0;
            row++;
        }
    } else {
        row = 0;
        col = 0;
    }

    /* TODO_EVID: Search for selected cells in cell data */
    /* NOTE: Need evidence for selection bit storage in cell data */

    /* PROV: Scan through cells looking for selection */
    while (row < listP->dataBounds.bottom && !found) {
        while (col < listP->dataBounds.right && !found) {
            /* NOTE: Check selection status of cell[row][col] */
            /* Stub implementation - need selection storage evidence */
            col++;
        }
        if (!found) {
            col = 0;
            row++;
        }
    }

    if (found) {
        theCell->v = row;
        theCell->h = col;
    }

    HUnlock((Handle)lHandle);
    return found;
}

/* ===========================================================================
 * Trap 0xA9EE: LLastClick
 * PROV: 5 occurrences @ 0x000160e5, 0x00019b49, 0x00019b5f, etc.
 * Returns the last clicked cell
 * ===========================================================================
 */
Cell LLastClick(ListHandle lHandle) {
    ListPtr listP;
    Cell result = {-1, -1};

    if (!lHandle) return result;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Return lastClick field @ offset 0x38 */
    result = listP->lastClick;

    HUnlock((Handle)lHandle);
    return result;
}

/* ===========================================================================
 * Trap 0xA9EF: LNextCell
 * PROV: 1 occurrence @ 0x00008d53
 * Finds the next cell in specified direction
 * ===========================================================================
 */
Boolean LNextCell(Boolean hNext, Boolean vNext, Cell* theCell, ListHandle lHandle) {
    ListPtr listP;
    Boolean found = false;

    if (!lHandle || !theCell) return false;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Move horizontally if requested */
    if (hNext) {
        if (theCell->h + 1 < listP->dataBounds.right) {
            theCell->h++;
            found = true;
        }
    }

    /* PROV: Move vertically if requested */
    if (vNext && !found) {
        if (theCell->v + 1 < listP->dataBounds.bottom) {
            theCell->v++;
            found = true;
        }
    }

    HUnlock((Handle)lHandle);
    return found;
}

/* ===========================================================================
 * Trap 0xA9F0: LSearch
 * PROV: Required by API though not found in evidence
 * Searches for data in the list
 * ===========================================================================
 */
Boolean LSearch(const void* dataPtr, short dataLen, ListSearchUPP searchProc,
                Cell* theCell, ListHandle lHandle) {
    /* TODO_EVID: Implement search through cell data */
    /* NOTE: Need evidence for cell data storage format */
    return false;
}

/* ===========================================================================
 * Trap 0xA9F1: LSize
 * PROV: Required by API though not found in evidence
 * Resizes the list view
 * ===========================================================================
 */
void LSize(short listWidth, short listHeight, ListHandle lHandle) {
    ListPtr listP;
    Rect newBounds;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Update view rectangle */
    listP->rView.right = listP->rView.left + listWidth;
    listP->rView.bottom = listP->rView.top + listHeight;

    /* PROV: Recalculate visible cells */
    CalcVisibleCells(lHandle);

    /* PROV: Resize scroll bars if present */
    if (listP->vScroll) {
        MoveControl(listP->vScroll,
                   listP->rView.right - kScrollBarWidth,
                   listP->rView.top);
        SizeControl(listP->vScroll,
                   kScrollBarWidth,
                   listHeight - (listP->hScroll ? kScrollBarWidth : 0));
    }

    if (listP->hScroll) {
        MoveControl(listP->hScroll,
                   listP->rView.left,
                   listP->rView.bottom - kScrollBarWidth);
        SizeControl(listP->hScroll,
                   listWidth - (listP->vScroll ? kScrollBarWidth : 0),
                   kScrollBarWidth);
    }

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9F2: LSetDrawingMode
 * PROV: 2 occurrences @ 0x0000be47, 0x00034b3b
 * Sets whether list drawing is enabled
 * ===========================================================================
 */
void LSetDrawingMode(Boolean drawIt, ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Update drawing flag in listFlags @ offset 0x27 */
    if (drawIt) {
        listP->listFlags |= 0x01;
    } else {
        listP->listFlags &= ~0x01;
    }

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9F3: LScroll
 * PROV: Required by API though not found in evidence
 * Scrolls the list view
 * ===========================================================================
 */
void LScroll(short dCols, short dRows, ListHandle lHandle) {
    ListPtr listP;
    RgnHandle updateRgn;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Scroll the visible rectangle */
    listP->visible.left += dCols;
    listP->visible.right += dCols;
    listP->visible.top += dRows;
    listP->visible.bottom += dRows;

    /* PROV: Update scroll bar positions */
    if (listP->hScroll && dCols != 0) {
        SetControlValue(listP->hScroll, listP->visible.left);
    }

    if (listP->vScroll && dRows != 0) {
        SetControlValue(listP->vScroll, listP->visible.top);
    }

    /* PROV: Scroll pixels in the view */
    updateRgn = NewRgn();
    ScrollRect(&listP->rView, -dCols * listP->cellSize.h,
               -dRows * listP->cellSize.v, updateRgn);

    /* TODO_EVID: Redraw exposed cells */
    /* NOTE: Need evidence for LDEF drawing calls */

    DisposeRgn(updateRgn);
    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9F4: LAutoScroll
 * PROV: 2 occurrences @ 0x0000ac19, 0x0000ef9e
 * Auto-scrolls to make selection visible
 * ===========================================================================
 */
Boolean LAutoScroll(ListHandle lHandle) {
    ListPtr listP;
    Cell firstSelected;
    short dCols = 0, dRows = 0;
    Boolean didScroll = false;

    if (!lHandle) return false;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Find first selected cell */
    firstSelected.v = firstSelected.h = 0;
    if (LGetSelect(false, &firstSelected, lHandle)) {
        /* PROV: Check if cell is visible */
        if (firstSelected.h < listP->visible.left) {
            dCols = firstSelected.h - listP->visible.left;
        } else if (firstSelected.h >= listP->visible.right) {
            dCols = firstSelected.h - listP->visible.right + 1;
        }

        if (firstSelected.v < listP->visible.top) {
            dRows = firstSelected.v - listP->visible.top;
        } else if (firstSelected.v >= listP->visible.bottom) {
            dRows = firstSelected.v - listP->visible.bottom + 1;
        }

        /* PROV: Scroll if needed */
        if (dCols != 0 || dRows != 0) {
            LScroll(dCols, dRows, lHandle);
            didScroll = true;
        }
    }

    HUnlock((Handle)lHandle);
    return didScroll;
}

/* ===========================================================================
 * Trap 0xA9F5: LUpdate
 * PROV: Required by API though not found in evidence
 * Updates the list display
 * ===========================================================================
 */
void LUpdate(RgnHandle theRgn, ListHandle lHandle) {
    ListPtr listP;
    Cell cell;
    Rect cellRect;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Only draw if drawing is enabled */
    if (!(listP->listFlags & 0x01)) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* PROV: Draw each visible cell */
    for (cell.v = listP->visible.top; cell.v < listP->visible.bottom; cell.v++) {
        for (cell.h = listP->visible.left; cell.h < listP->visible.right; cell.h++) {
            /* PROV: Calculate cell rectangle */
            cellRect.left = listP->rView.left +
                          (cell.h - listP->visible.left) * listP->cellSize.h;
            cellRect.top = listP->rView.top +
                         (cell.v - listP->visible.top) * listP->cellSize.v;
            cellRect.right = cellRect.left + listP->cellSize.h;
            cellRect.bottom = cellRect.top + listP->cellSize.v;

            /* PROV: Call LDEF to draw cell */
            CallLDEF(lHandle, lDrawMsg, false, &cellRect, cell, 0, 0);
        }
    }

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9F6: LActivate
 * PROV: Required by API though not found in evidence
 * Activates or deactivates the list
 * ===========================================================================
 */
void LActivate(Boolean act, ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Set active flag @ offset 0x25 */
    listP->lActive = act;

    /* PROV: Activate/deactivate scroll bars */
    if (listP->vScroll) {
        if (act) {
            ShowControl(listP->vScroll);
        } else {
            HideControl(listP->vScroll);
        }
    }

    if (listP->hScroll) {
        if (act) {
            ShowControl(listP->hScroll);
        } else {
            HideControl(listP->hScroll);
        }
    }

    /* TODO_EVID: Redraw list with active/inactive appearance */
    /* NOTE: Need evidence for active/inactive drawing differences */

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9F7: LClick
 * PROV: Required by API though not found in evidence
 * Handles mouse clicks in the list
 * ===========================================================================
 */
Boolean LClick(Point pt, short modifiers, ListHandle lHandle) {
    ListPtr listP;
    Cell clickCell;
    Boolean doubleClick = false;
    long currentTime;

    if (!lHandle) return false;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Check if click is in list bounds */
    if (!PtInRect(pt, &listP->rView)) {
        HUnlock((Handle)lHandle);
        return false;
    }

    /* PROV: Calculate which cell was clicked */
    clickCell.h = listP->visible.left +
                  (pt.h - listP->rView.left) / listP->cellSize.h;
    clickCell.v = listP->visible.top +
                  (pt.v - listP->rView.top) / listP->cellSize.v;

    /* PROV: Check for valid cell */
    if (clickCell.h >= listP->dataBounds.right ||
        clickCell.v >= listP->dataBounds.bottom) {
        HUnlock((Handle)lHandle);
        return false;
    }

    /* PROV: Check for double-click @ offsets 0x28, 0x2C */
    currentTime = TickCount();
    if ((currentTime - listP->clikTime) < GetDblTime() &&
        abs(pt.h - listP->clikLoc.h) < 3 &&
        abs(pt.v - listP->clikLoc.v) < 3) {
        doubleClick = true;
    }

    /* PROV: Update click tracking @ offsets 0x28, 0x2C, 0x38 */
    listP->clikTime = currentTime;
    listP->clikLoc = pt;
    listP->lastClick = clickCell;

    /* TODO_EVID: Handle selection based on modifiers */
    /* NOTE: Need evidence for selection modification with shift/cmd keys */

    HUnlock((Handle)lHandle);
    return doubleClick;
}

/* ===========================================================================
 * Trap 0xA9F8: LSetCell
 * PROV: Required by API though not found in evidence
 * Sets data for a cell
 * ===========================================================================
 */
void LSetCell(const void* dataPtr, short dataLen, Cell theCell, ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Validate cell coordinates */
    if (theCell.h < 0 || theCell.h >= listP->dataBounds.right ||
        theCell.v < 0 || theCell.v >= listP->dataBounds.bottom) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* TODO_EVID: Store cell data in cells handle @ offset 0x50 */
    /* NOTE: Need evidence for cell data storage format and indexing */

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9F9: LGetCell
 * PROV: Required by API though not found in evidence
 * Gets data from a cell
 * ===========================================================================
 */
void LGetCell(void* dataPtr, short* dataLen, Cell theCell, ListHandle lHandle) {
    ListPtr listP;

    if (!lHandle || !dataLen) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Validate cell coordinates */
    if (theCell.h < 0 || theCell.h >= listP->dataBounds.right ||
        theCell.v < 0 || theCell.v >= listP->dataBounds.bottom) {
        *dataLen = 0;
        HUnlock((Handle)lHandle);
        return;
    }

    /* TODO_EVID: Retrieve cell data from cells handle @ offset 0x50 */
    /* NOTE: Need evidence for cell data storage format and indexing */

    *dataLen = 0; /* Stub implementation */

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9FA: LSetSelect
 * PROV: Required by API though not found in evidence
 * Sets selection state of a cell
 * ===========================================================================
 */
void LSetSelect(Boolean setIt, Cell theCell, ListHandle lHandle) {
    ListPtr listP;
    Rect cellRect;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Validate cell coordinates */
    if (theCell.h < 0 || theCell.h >= listP->dataBounds.right ||
        theCell.v < 0 || theCell.v >= listP->dataBounds.bottom) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* TODO_EVID: Update selection state in cell data */
    /* NOTE: Need evidence for selection bit storage */

    /* PROV: Redraw cell if visible */
    if (theCell.h >= listP->visible.left && theCell.h < listP->visible.right &&
        theCell.v >= listP->visible.top && theCell.v < listP->visible.bottom) {

        /* PROV: Calculate cell rectangle */
        cellRect.left = listP->rView.left +
                      (theCell.h - listP->visible.left) * listP->cellSize.h;
        cellRect.top = listP->rView.top +
                     (theCell.v - listP->visible.top) * listP->cellSize.v;
        cellRect.right = cellRect.left + listP->cellSize.h;
        cellRect.bottom = cellRect.top + listP->cellSize.v;

        /* PROV: Call LDEF to highlight/unhighlight */
        CallLDEF(lHandle, lHiliteMsg, setIt, &cellRect, theCell, 0, 0);
    }

    HUnlock((Handle)lHandle);
}

/* ===========================================================================
 * Trap 0xA9FB: LDraw
 * PROV: Required by API though not found in evidence
 * Draws a specific cell
 * ===========================================================================
 */
void LDraw(Cell theCell, ListHandle lHandle) {
    ListPtr listP;
    Rect cellRect;
    Boolean isSelected = false;

    if (!lHandle) return;

    HLock((Handle)lHandle);
    listP = *lHandle;

    /* PROV: Check if drawing is enabled */
    if (!(listP->listFlags & 0x01)) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* PROV: Check if cell is visible */
    if (theCell.h < listP->visible.left || theCell.h >= listP->visible.right ||
        theCell.v < listP->visible.top || theCell.v >= listP->visible.bottom) {
        HUnlock((Handle)lHandle);
        return;
    }

    /* PROV: Calculate cell rectangle */
    cellRect.left = listP->rView.left +
                  (theCell.h - listP->visible.left) * listP->cellSize.h;
    cellRect.top = listP->rView.top +
                 (theCell.v - listP->visible.top) * listP->cellSize.v;
    cellRect.right = cellRect.left + listP->cellSize.h;
    cellRect.bottom = cellRect.top + listP->cellSize.v;

    /* TODO_EVID: Check if cell is selected */
    /* NOTE: Need evidence for selection state retrieval */

    /* PROV: Call LDEF to draw cell */
    CallLDEF(lHandle, lDrawMsg, isSelected, &cellRect, theCell, 0, 0);

    HUnlock((Handle)lHandle);
}
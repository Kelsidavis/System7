/*
 * ListManagerInternal.h - Internal List Manager structures and functions
 *
 * Private header for List Manager implementation details.
 * Not for public consumption - use ListManager.h for public API.
 */

#ifndef LIST_MANAGER_INTERNAL_H
#define LIST_MANAGER_INTERNAL_H

#include "SystemTypes.h"
#include "ListManager/ListManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum cell data size (bytes) */
#define MAX_CELL_DATA 255

/* Cell data storage */
typedef struct CellData {
    unsigned char len;              /* Data length (0-255) */
    unsigned char data[MAX_CELL_DATA]; /* Cell content */
} CellData;

/* Row storage - array of columns */
typedef struct RowData {
    short colCount;           /* Number of columns in this row */
    CellData* cells;          /* Array of cell data [colCount] */
    Boolean selected;         /* Selection state (for single-column lists) */
} RowData;

/* Selection range for multi-select */
typedef struct SelectionRange {
    short startRow;
    short endRow;
    Boolean active;
} SelectionRange;

/* Last click info for double-click detection */
typedef struct LastClickInfo {
    Cell cell;
    UInt32 when;             /* Tick count */
    unsigned short mods;     /* Event modifiers */
    Boolean valid;
} LastClickInfo;

/*
 * ListMgrRec - Our internal list structure
 * Note: SystemTypes.h defines ListRec (classic Mac structure)
 * We use ListMgrRec internally to avoid conflicts
 */
typedef struct ListMgrRec ListMgrRec;
typedef ListMgrRec** ListMgrHandle;

struct ListMgrRec {
    /* Geometry */
    Rect viewRect;           /* Full view rectangle in window local coords */
    Rect contentRect;        /* Cell grid area (inside any border) */
    short cellWidth;         /* Individual cell width */
    short cellHeight;        /* Individual cell height */
    short visibleRows;       /* Number of visible rows */
    short visibleCols;       /* Number of visible columns */

    /* Model */
    short rowCount;          /* Total number of rows */
    short colCount;          /* Total number of columns */
    RowData** rows;          /* Handle to array of RowData */

    /* Selection */
    short selMode;           /* lsSingleSel or lsMultiSel */
    SelectionRange selRange; /* For range selection */
    short selectIterRow;     /* Iterator for LGetSelect */
    Cell anchorCell;         /* Anchor for Shift-extend */

    /* Scrolling */
    short topRow;            /* First visible row */
    short leftCol;           /* First visible column */
    ControlHandle vScroll;   /* Vertical scrollbar (optional) */
    ControlHandle hScroll;   /* Horizontal scrollbar (optional) */

    /* Owner */
    WindowPtr window;        /* Owning window */

    /* Event state */
    LastClickInfo lastClick;

    /* Client data */
    long refCon;

    /* Flags */
    Boolean hasVScroll;      /* Has vertical scrollbar */
    Boolean hasHScroll;      /* Has horizontal scrollbar */
    Boolean active;          /* Window is active */
};

/* ================================================================
 * INTERNAL FUNCTIONS (implemented in list_manager.c)
 * ================================================================ */

/*
 * List_ComputeVisibleCells - Recompute visible row/column counts
 */
void List_ComputeVisibleCells(ListMgrRec* list);

/*
 * List_DrawCell - Draw a single cell
 * cellRect: cell rectangle in local coordinates
 * row, col: cell indices
 * selected: draw with selection highlight
 */
void List_DrawCell(ListMgrRec* list, const Rect* cellRect, short row, short col, Boolean selected);

/*
 * List_EraseBackground - Erase list background
 * updateRect: area to erase in local coordinates
 */
void List_EraseBackground(ListMgrRec* list, const Rect* updateRect);

/*
 * List_HitTest - Test if point hits a cell
 * localPt: point in window local coordinates
 * outCell: receives hit cell
 * Returns: true if point is within a valid cell
 */
Boolean List_HitTest(ListMgrRec* list, Point localPt, Cell* outCell);

/*
 * List_InvalidateAll - Invalidate entire list view
 */
void List_InvalidateAll(ListMgrRec* list);

/*
 * List_InvalidateBand - Invalidate only exposed band when scrolling
 * dRows: number of rows scrolled (positive=forward, negative=backward)
 */
void List_InvalidateBand(ListMgrRec* list, short dRows);

/*
 * List_UpdateScrollbars - Update scrollbar values and ranges
 */
void List_UpdateScrollbars(ListMgrRec* list);

/*
 * List_ClampScroll - Clamp topRow/leftCol to valid range
 */
void List_ClampScroll(ListMgrRec* list);

/*
 * List_IsCellSelected - Check if cell is selected
 */
Boolean List_IsCellSelected(ListMgrRec* list, Cell cell);

/*
 * List_SetCellSelection - Set selection state for cell
 */
void List_SetCellSelection(ListMgrRec* list, Cell cell, Boolean selected);

/*
 * List_ClearAllSelection - Clear all selections
 */
void List_ClearAllSelection(ListMgrRec* list);

/*
 * List_GetCellData - Get pointer to cell data structure
 * Returns NULL if cell is invalid
 */
CellData* List_GetCellData(ListMgrRec* list, Cell cell);

/*
 * List_ValidateCell - Check if cell coordinates are valid
 */
Boolean List_ValidateCell(ListMgrRec* list, Cell cell);

/* ================================================================
 * DIALOG INTEGRATION REGISTRY
 * ================================================================ */

/* Maximum dialog-list associations */
#define MAX_DIALOG_LISTS 32

/* Dialog-list association */
typedef struct DialogListAssoc {
    DialogPtr dialog;
    short itemNo;
    ListHandle list;
    Boolean active;
} DialogListAssoc;

/* Registry for dialog-list associations */
extern DialogListAssoc gDialogListRegistry[MAX_DIALOG_LISTS];

/* Find or allocate registry slot */
DialogListAssoc* FindDialogListSlot(DialogPtr dlg, short itemNo, Boolean allocate);

#ifdef __cplusplus
}
#endif

#endif /* LIST_MANAGER_INTERNAL_H */

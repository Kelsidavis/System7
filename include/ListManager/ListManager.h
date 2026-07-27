/*
 * ListManager.h - Classic Mac OS List Manager API
 *
 * System 7.1-compatible List Manager for displaying scrollable lists
 * of rows and columns with selection support. Used extensively in
 * file pickers, option lists, and dialog controls.
 *
 * This is a faithful minimal-but-correct implementation providing:
 * - Single and multiple selection modes
 * - Mouse and keyboard interaction
 * - Scrolling with optional scrollbar integration
 * - Cell-based data storage
 * - Integration with Dialog and Window Managers
 *
 * API Surface: Compatible with classic LNew, LDispose, LAddRow, LDelRow,
 * LSetCell, LGetCell, LClick, LUpdate, LDraw, LGetSelect, LSetSelect, etc.
 */

#ifndef LIST_MANAGER_H
#define LIST_MANAGER_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * LIST MANAGER TYPES
 * ================================================================ */

/*
 * ListHandle and Cell are defined in SystemTypes.h
 * ListHandle = struct ListRec **
 * Cell = struct { short h; short v; }
 */

/*
 * Selection modes for list behavior
 */
enum {
    lsSingleSel = 0,     /* Single selection only */
    lsMultiSel  = 1,     /* Multiple selection with cmd/shift */
    lsNoDraw    = 0x8000 /* Internal flag: suppress draw during batch ops */
};

/*
 * ListParams - Parameters for LNew
 * Defines geometry, scrolling, and selection mode
 */
typedef struct ListParams {
    Rect    viewRect;       /* Full list rect in window local coords */
    Rect    cellSizeRect;   /* Cell dimensions in .right/.bottom (e.g., {0,0,16,200}) */
    WindowPtr window;       /* Owning window */
    Boolean hasVScroll;     /* Show vertical scrollbar */
    Boolean hasHScroll;     /* Show horizontal scrollbar */
    short   selMode;        /* lsSingleSel or lsMultiSel */
    long    refCon;         /* Client reference data */
} ListParams;

/* ================================================================
 * LIST MANAGER LIFECYCLE
 * ================================================================ */

/*
 * LNew - Create new list
 * Returns handle to new list or NULL on failure
 */
ListHandle LNew(const ListParams* params);

/*
 * LDispose - Dispose of list and free all resources
 */
void LDispose(ListHandle lh);

/*
 * LSize - Resize list view rectangle
 * Updates visible row/column calculations and invalidates
 */
void LSize(ListHandle lh, short newWidth, short newHeight);

/* ================================================================
 * LIST MODEL OPERATIONS
 * ================================================================ */

/*
 * LAddRow - Add rows to list
 * count: number of rows to add
 * afterRow: insert after this row (-1 = insert before row 0)
 * Returns: error code (noErr on success)
 */
OSErr LAddRow(ListHandle lh, short count, short afterRow);

/*
 * LDelRow - Delete rows from list
 * count: number of rows to delete
 * fromRow: starting row index
 * Returns: error code (noErr on success)
 */
OSErr LDelRow(ListHandle lh, short count, short fromRow);

/*
 * LAddColumn - Add columns to list
 * count: number of columns to add
 * afterCol: insert after this column (-1 = insert before col 0)
 * Returns: error code (noErr on success)
 */
OSErr LAddColumn(ListHandle lh, short count, short afterCol);

/*
 * LDelColumn - Delete columns from list
 * count: number of columns to delete
 * fromCol: starting column index
 * Returns: error code (noErr on success)
 */
OSErr LDelColumn(ListHandle lh, short count, short fromCol);

/*
 * LSetCell - Set cell data
 * Copies dataLen bytes from data into the specified cell
 */
OSErr LSetCell(ListHandle lh, const void* data, short dataLen, Cell cell);

/*
 * LGetCell - Get cell data
 * Copies up to outMax bytes into out buffer
 * Returns: actual bytes copied
 */
short LGetCell(ListHandle lh, void* out, short outMax, Cell cell);

/*
 * LSetRefCon - Set list reference value
 */
void LSetRefCon(ListHandle lh, long refCon);

/*
 * LGetRefCon - Get list reference value
 */
long LGetRefCon(ListHandle lh);

/* ================================================================
 * DRAWING AND UPDATE
 * ================================================================ */

/*
 * LUpdate - Update list within region
 * Caller handles BeginUpdate/EndUpdate
 * LUpdate clips to viewRect ∩ updateRgn and draws visible cells
 */
void LUpdate(ListHandle lh, RgnHandle updateRgn);

/*
 * LDraw - Full redraw of list
 * Convenience wrapper that calls LUpdate with entire view
 */
void LDraw(ListHandle lh);

/*
 * LGetCellRect - Get cell rectangle in local coordinates
 * Returns cell content rect for specified cell
 * (Note: Classic Mac used LRect, but that conflicts with typedef)
 */
void LGetCellRect(ListHandle lh, Cell cell, Rect* outCellRect);

/*
 * LScroll - Scroll list by delta rows/columns
 * Adjusts topRow/leftCol with clamping and invalidates
 */
void LScroll(ListHandle lh, short dRows, short dCols);

/* ================================================================
 * SELECTION
 * ================================================================ */

/*
 * LClick - Handle mouse click in list
 * localWhere: click point in window local coordinates
 * mods: event modifiers (shift, cmd keys)
 * outItem: receives row index (or encoded cell for multi-column)
 * Returns: true if selection changed
 */
Boolean LClick(ListHandle lh, Point localWhere, unsigned short mods, short* outItem);

/*
 * LGetSelect - Get first selected cell
 * Returns true if a selection exists, fills outCell with first selected cell
 * For iteration, call repeatedly (uses internal iterator)
 */
Boolean LGetSelect(ListHandle lh, Cell* outCell);
/* Restart LGetSelect's walk at the first row. */
void LResetSelect(ListHandle lh);

/*
 * LSetSelect - Set selection state for a cell
 * sel: true to select, false to deselect
 */
void LSetSelect(ListHandle lh, Boolean sel, Cell cell);

/*
 * LSelectAll - Select all cells
 */
void LSelectAll(ListHandle lh);

/*
 * LClearSelect - Clear all selections
 */
void LClearSelect(ListHandle lh);

/*
 * LLastClick - Get info about last click for double-click detection
 * outCell: receives cell that was clicked
 * outWhen: receives tick count when clicked
 * outMods: receives event modifiers
 * Returns: true if valid last click exists
 */
Boolean LLastClick(ListHandle lh, Cell* outCell, UInt32* outWhen, unsigned short* outMods);

/* ================================================================
 * SEARCH (Optional - may be stubbed)
 * ================================================================ */

/*
 * LSearch - Search for text in list
 * pStr: Pascal string to search for
 * caseSensitive: perform case-sensitive match
 * outFound: receives cell where found
 * Returns: true if found
 */
Boolean LSearch(ListHandle lh, const unsigned char* pStr, Boolean caseSensitive, Cell* outFound);

/* ================================================================
 * KEYBOARD HANDLING
 * ================================================================ */

/*
 * LKey - Handle keyboard input
 * ch: character code
 * Returns: true if key was handled
 */
Boolean LKey(ListHandle lh, char ch);

/* ================================================================
 * SCROLLBAR INTEGRATION
 * ================================================================ */

/*
 * LAttachScrollbars - Attach scrollbars to list
 * Control values updated automatically on scroll/resize
 */
void LAttachScrollbars(ListHandle lh, ControlHandle vScroll, ControlHandle hScroll);

/* ================================================================
 * DIALOG INTEGRATION
 * ================================================================ */

/*
 * ListFromDialogItem - Get list associated with dialog item
 * Returns NULL if no list is associated
 */
ListHandle ListFromDialogItem(DialogPtr dlg, short itemNo);

/*
 * AttachListToDialogItem - Associate list with dialog item
 * Used for userItem rendering integration
 */
void AttachListToDialogItem(DialogPtr dlg, short itemNo, ListHandle lh);

/* ================================================================
 * TESTING FUNCTIONS
 * ================================================================ */

/*
 * RunListSmokeTest - Run smoke test suite for List Manager
 */
void RunListSmokeTest(void);

#ifdef __cplusplus
}
#endif

#endif /* LIST_MANAGER_H */

/*
 * Pack0_ListManager.c - List Manager Package (Pack0)
 *
 * Implements Pack0, the List Manager Package for Mac OS System 7.
 * This package provides list box controls for displaying and managing
 * scrollable lists of items in dialogs and windows.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 * and Inside Macintosh: Toolbox Essentials, Chapter 7
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ListManager/ListManager.h"

/* Forward declaration for Pack0 dispatcher */
OSErr Pack0_Dispatch(short selector, void* params);

/* Debug logging */
#define PACK0_DEBUG 0

#if PACK0_DEBUG
#define PACK0_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Pack0] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define PACK0_LOG(...)
#endif

/* Pack0 selectors - based on Inside Macintosh */
#define kPack0LNew            0   /* Create new list */
#define kPack0LDispose        1   /* Dispose list */
#define kPack0LAddRow         2   /* Add rows */
#define kPack0LDelRow         3   /* Delete rows */
#define kPack0LAddColumn      4   /* Add columns */
#define kPack0LDelColumn      5   /* Delete columns */
#define kPack0LSetCell        6   /* Set cell data */
#define kPack0LDraw           7   /* Draw list */
#define kPack0LClick          8   /* Handle click */
#define kPack0LUpdate         9   /* Update for redraw */
#define kPack0LScroll        10   /* Scroll list */
#define kPack0LSize          11   /* Resize list */
#define kPack0LSetSelect     12   /* Set selection */
#define kPack0LGetSelect     13   /* Get selection */
#define kPack0LClearSelect   14   /* Clear all selections */
#define kPack0LSelectAll     15   /* Select all items */
#define kPack0LGetCellRect   16   /* Get cell rectangle */
#define kPack0LSetRefCon     17   /* Set reference constant */
#define kPack0LLastClick     18   /* Get last click info */
#define kPack0LSearch        19   /* Search for text */
#define kPack0LKey           20   /* Handle keyboard */
#define kPack0LAttachScrollbars 21 /* Attach scrollbars */

/* Parameter blocks for each function */

typedef struct {
    const ListParams* params;
    ListHandle        result;
} LNewParamsBlock;

typedef struct {
    ListHandle lh;
} LDisposeParamsBlock;

typedef struct {
    ListHandle lh;
    short      newWidth;
    short      newHeight;
} LSizeParamsBlock;

typedef struct {
    ListHandle lh;
    short      count;
    short      afterRow;
    OSErr      result;
} LAddRowParamsBlock;

typedef struct {
    ListHandle lh;
    short      count;
    short      fromRow;
    OSErr      result;
} LDelRowParamsBlock;

typedef struct {
    ListHandle lh;
    short      count;
    short      afterCol;
    OSErr      result;
} LAddColumnParamsBlock;

typedef struct {
    ListHandle lh;
    short      count;
    short      fromCol;
    OSErr      result;
} LDelColumnParamsBlock;

typedef struct {
    ListHandle  lh;
    const void* data;
    short       dataLen;
    Cell        cell;
    OSErr       result;
} LSetCellParamsBlock;

typedef struct {
    ListHandle lh;
    long       refCon;
} LSetRefConParamsBlock;

typedef struct {
    ListHandle lh;
    RgnHandle  updateRgn;
} LUpdateParamsBlock;

typedef struct {
    ListHandle lh;
} LDrawParamsBlock;

typedef struct {
    ListHandle lh;
    Cell       cell;
    Rect*      outCellRect;
} LGetCellRectParamsBlock;

typedef struct {
    ListHandle lh;
    short      dRows;
    short      dCols;
} LScrollParamsBlock;

typedef struct {
    ListHandle     lh;
    Point          localWhere;
    unsigned short mods;
    short*         outItem;
    Boolean        result;
} LClickParamsBlock;

typedef struct {
    ListHandle lh;
    Cell*      outCell;
    Boolean    result;
} LGetSelectParamsBlock;

typedef struct {
    ListHandle lh;
    Boolean    sel;
    Cell       cell;
} LSetSelectParamsBlock;

typedef struct {
    ListHandle lh;
} LSelectAllParamsBlock;

typedef struct {
    ListHandle lh;
} LClearSelectParamsBlock;

typedef struct {
    ListHandle     lh;
    Cell*          outCell;
    UInt32*        outWhen;
    unsigned short* outMods;
    Boolean        result;
} LLastClickParamsBlock;

typedef struct {
    ListHandle            lh;
    const unsigned char*  pStr;
    Boolean               caseSensitive;
    Cell*                 outFound;
    Boolean               result;
} LSearchParamsBlock;

typedef struct {
    ListHandle lh;
    char       ch;
    Boolean    result;
} LKeyParamsBlock;

typedef struct {
    ListHandle    lh;
    ControlHandle vScroll;
    ControlHandle hScroll;
} LAttachScrollbarsParamsBlock;

/*
 * Pack0_Dispatch - Pack0 package dispatcher
 *
 * Main dispatcher for the List Manager Package (Pack0).
 * Routes selector calls to the appropriate list management function.
 *
 * Parameters:
 *   selector - Function selector (0-21, see kPack0* constants)
 *   params - Pointer to selector-specific parameter block
 *
 * Returns:
 *   noErr if successful
 *   paramErr if selector is invalid or params are NULL
 *
 * Example usage through Package Manager:
 *   LNewParamsBlock params;
 *   ListParams listParams;
 *   listParams.rView = bounds;
 *   listParams.cellSize.h = 100;
 *   listParams.cellSize.v = 20;
 *   // ... set other fields
 *   params.params = &listParams;
 *   CallPackage(0, 0, &params);  // Call Pack0, LNew selector
 *   if (params.result != NULL) {
 *       // Use the list handle
 *   }
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 * and Inside Macintosh: Toolbox Essentials, Chapter 7
 */
OSErr Pack0_Dispatch(short selector, void* params) {
    PACK0_LOG("Dispatch: selector=%d, params=%p\n", selector, params);

    if (!params) {
        PACK0_LOG("Dispatch: NULL params\n");
        return paramErr;
    }

    switch (selector) {
        case kPack0LNew: {
            LNewParamsBlock* p = (LNewParamsBlock*)params;
            PACK0_LOG("Dispatch: LNew\n");
            if (!p->params) return paramErr;
            p->result = LNew(p->params);
            return noErr;
        }

        case kPack0LDispose: {
            LDisposeParamsBlock* p = (LDisposeParamsBlock*)params;
            PACK0_LOG("Dispatch: LDispose\n");
            if (!p->lh) return paramErr;
            LDispose(p->lh);
            return noErr;
        }

        case kPack0LAddRow: {
            LAddRowParamsBlock* p = (LAddRowParamsBlock*)params;
            PACK0_LOG("Dispatch: LAddRow\n");
            if (!p->lh) return paramErr;
            p->result = LAddRow(p->lh, p->count, p->afterRow);
            return noErr;
        }

        case kPack0LDelRow: {
            LDelRowParamsBlock* p = (LDelRowParamsBlock*)params;
            PACK0_LOG("Dispatch: LDelRow\n");
            if (!p->lh) return paramErr;
            p->result = LDelRow(p->lh, p->count, p->fromRow);
            return noErr;
        }

        case kPack0LAddColumn: {
            LAddColumnParamsBlock* p = (LAddColumnParamsBlock*)params;
            PACK0_LOG("Dispatch: LAddColumn\n");
            if (!p->lh) return paramErr;
            p->result = LAddColumn(p->lh, p->count, p->afterCol);
            return noErr;
        }

        case kPack0LDelColumn: {
            LDelColumnParamsBlock* p = (LDelColumnParamsBlock*)params;
            PACK0_LOG("Dispatch: LDelColumn\n");
            if (!p->lh) return paramErr;
            p->result = LDelColumn(p->lh, p->count, p->fromCol);
            return noErr;
        }

        case kPack0LSetCell: {
            LSetCellParamsBlock* p = (LSetCellParamsBlock*)params;
            PACK0_LOG("Dispatch: LSetCell\n");
            if (!p->lh) return paramErr;
            p->result = LSetCell(p->lh, p->data, p->dataLen, p->cell);
            return noErr;
        }

        case kPack0LDraw: {
            LDrawParamsBlock* p = (LDrawParamsBlock*)params;
            PACK0_LOG("Dispatch: LDraw\n");
            if (!p->lh) return paramErr;
            LDraw(p->lh);
            return noErr;
        }

        case kPack0LClick: {
            LClickParamsBlock* p = (LClickParamsBlock*)params;
            PACK0_LOG("Dispatch: LClick\n");
            if (!p->lh) return paramErr;
            p->result = LClick(p->lh, p->localWhere, p->mods, p->outItem);
            return noErr;
        }

        case kPack0LUpdate: {
            LUpdateParamsBlock* p = (LUpdateParamsBlock*)params;
            PACK0_LOG("Dispatch: LUpdate\n");
            if (!p->lh) return paramErr;
            LUpdate(p->lh, p->updateRgn);
            return noErr;
        }

        case kPack0LScroll: {
            LScrollParamsBlock* p = (LScrollParamsBlock*)params;
            PACK0_LOG("Dispatch: LScroll\n");
            if (!p->lh) return paramErr;
            LScroll(p->lh, p->dRows, p->dCols);
            return noErr;
        }

        case kPack0LSize: {
            LSizeParamsBlock* p = (LSizeParamsBlock*)params;
            PACK0_LOG("Dispatch: LSize\n");
            if (!p->lh) return paramErr;
            LSize(p->lh, p->newWidth, p->newHeight);
            return noErr;
        }

        case kPack0LSetSelect: {
            LSetSelectParamsBlock* p = (LSetSelectParamsBlock*)params;
            PACK0_LOG("Dispatch: LSetSelect\n");
            if (!p->lh) return paramErr;
            LSetSelect(p->lh, p->sel, p->cell);
            return noErr;
        }

        case kPack0LGetSelect: {
            LGetSelectParamsBlock* p = (LGetSelectParamsBlock*)params;
            PACK0_LOG("Dispatch: LGetSelect\n");
            if (!p->lh || !p->outCell) return paramErr;
            p->result = LGetSelect(p->lh, p->outCell);
            return noErr;
        }

        case kPack0LClearSelect: {
            LClearSelectParamsBlock* p = (LClearSelectParamsBlock*)params;
            PACK0_LOG("Dispatch: LClearSelect\n");
            if (!p->lh) return paramErr;
            LClearSelect(p->lh);
            return noErr;
        }

        case kPack0LSelectAll: {
            LSelectAllParamsBlock* p = (LSelectAllParamsBlock*)params;
            PACK0_LOG("Dispatch: LSelectAll\n");
            if (!p->lh) return paramErr;
            LSelectAll(p->lh);
            return noErr;
        }

        case kPack0LGetCellRect: {
            LGetCellRectParamsBlock* p = (LGetCellRectParamsBlock*)params;
            PACK0_LOG("Dispatch: LGetCellRect\n");
            if (!p->lh || !p->outCellRect) return paramErr;
            LGetCellRect(p->lh, p->cell, p->outCellRect);
            return noErr;
        }

        case kPack0LSetRefCon: {
            LSetRefConParamsBlock* p = (LSetRefConParamsBlock*)params;
            PACK0_LOG("Dispatch: LSetRefCon\n");
            if (!p->lh) return paramErr;
            LSetRefCon(p->lh, p->refCon);
            return noErr;
        }

        case kPack0LLastClick: {
            LLastClickParamsBlock* p = (LLastClickParamsBlock*)params;
            PACK0_LOG("Dispatch: LLastClick\n");
            if (!p->lh) return paramErr;
            p->result = LLastClick(p->lh, p->outCell, p->outWhen, p->outMods);
            return noErr;
        }

        case kPack0LSearch: {
            LSearchParamsBlock* p = (LSearchParamsBlock*)params;
            PACK0_LOG("Dispatch: LSearch\n");
            if (!p->lh || !p->pStr) return paramErr;
            p->result = LSearch(p->lh, p->pStr, p->caseSensitive, p->outFound);
            return noErr;
        }

        case kPack0LKey: {
            LKeyParamsBlock* p = (LKeyParamsBlock*)params;
            PACK0_LOG("Dispatch: LKey\n");
            if (!p->lh) return paramErr;
            p->result = LKey(p->lh, p->ch);
            return noErr;
        }

        case kPack0LAttachScrollbars: {
            LAttachScrollbarsParamsBlock* p = (LAttachScrollbarsParamsBlock*)params;
            PACK0_LOG("Dispatch: LAttachScrollbars\n");
            if (!p->lh) return paramErr;
            LAttachScrollbars(p->lh, p->vScroll, p->hScroll);
            return noErr;
        }

        default:
            PACK0_LOG("Dispatch: Invalid selector %d\n", selector);
            return paramErr;
    }
}

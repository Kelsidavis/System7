#include "MemoryMgr/MemoryManager.h"
#include "DialogManager/DialogInternal.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
/*
 * DialogItems.c - Dialog Item Management Implementation
 *
 * This module provides the dialog item management functionality,
 * maintaining exact Mac System 7.1 behavioral compatibility.
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogManagerInternal.h"
#include "DialogManager/DialogDrawing.h"
#include "DialogManager/DialogResourceParser.h"
#include "WindowManager/WindowManager.h"
#include <assert.h>
#include "DialogManager/DialogLogging.h"
#include "TimeManager/TimeBase.h"

#define DIALOG_ITEM_CACHE_CAPACITY 32

/* Private structures for item management */
typedef struct DialogItemCache {
    DialogPtr       dialog;
    SInt16         itemCount;
    DialogItemEx*   items;
    Boolean            needsUpdate;
    UInt32        lastUpdateTime;
} DialogItemCache;

/* Global state for dialog items */
static struct {
    Boolean            initialized;
    DialogItemCache cache[DIALOG_ITEM_CACHE_CAPACITY];
    SInt16         cacheCount;
    SInt16         defaultFont;
    SInt16         defaultSize;
} gDialogItemState = {0};

/* Private function prototypes */
static DialogItemCache* GetDialogItemCache(DialogPtr theDialog);
static DialogItemCache* CreateDialogItemCache(DialogPtr theDialog);
static void DisposeDialogItemCache(DialogItemCache* cache);
static OSErr ParseDialogItemList(Handle itemList, DialogItemEx** items, SInt16* itemCount);
static DialogItemEx* GetDialogItemEx(DialogPtr theDialog, SInt16 itemNo);
static Boolean ValidateItemNumber(DialogPtr theDialog, SInt16 itemNo);
static void InvalidateItemRect(DialogPtr theDialog, const Rect* rect);

/*
 * InitDialogItems - Initialize dialog item subsystem
 */
void InitDialogItems(void)
{
    if (gDialogItemState.initialized) {
        return;
    }

    memset(&gDialogItemState, 0, sizeof(gDialogItemState));
    gDialogItemState.initialized = true;
    gDialogItemState.cacheCount = 0;
    gDialogItemState.defaultFont = 0; /* System font */
    gDialogItemState.defaultSize = 12;

    /* Initialize cache entries */
    for (int i = 0; i < DIALOG_ITEM_CACHE_CAPACITY; i++) {
        gDialogItemState.cache[i].dialog = NULL;
        gDialogItemState.cache[i].itemCount = 0;
        gDialogItemState.cache[i].items = NULL;
        gDialogItemState.cache[i].needsUpdate = false;
        gDialogItemState.cache[i].lastUpdateTime = 0;
    }

}

/*
 * GetDialogItem - Get information about a dialog item
 */
void GetDialogItem(DialogPtr theDialog, SInt16 itemNo, SInt16* itemType,
                   Handle* item, Rect* box)
{
    DialogItemEx* itemEx;

    /* Initialize return values */
    if (itemType) *itemType = 0;
    if (item) *item = NULL;
    if (box) memset(box, 0, sizeof(Rect));

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    /* Return item information */
    if (itemType) {
        *itemType = (itemEx)->type;
    }
    if (item) {
        *item = (itemEx)->handle;
    }
    if (box) {
        *box = (itemEx)->bounds;
    }

}

/*
 * DialogItem_SyncText - refresh an item's cached text pointer
 *
 * item->data is a cached pointer into item->handle so that drawing does not
 * have to lock a handle on every redraw. Anything that writes the handle -
 * SetDialogItemText, the TextEdit glue - has to call this afterwards, because
 * a write that grew the block may have moved it.
 */
void DialogItem_SyncText(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx || !itemEx->handle) {
        return;
    }

    HLock(itemEx->handle);
    itemEx->data = (void*)*itemEx->handle;
}

/*
 * SetDialogItem - Set information about a dialog item
 */
void SetDialogItem(DialogPtr theDialog, SInt16 itemNo, SInt16 itemType,
                   Handle item, const Rect* box)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo) || !box) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    /* Update item information */
    (itemEx)->type = itemType;
    (itemEx)->handle = item;
    (itemEx)->bounds = *box;

    /* Mark for redraw */
    InvalDialogItem(theDialog, itemNo);

}

/*
 * HideDialogItem - Hide a dialog item
 */
void HideDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    if (itemEx->visible) {
        itemEx->visible = false;
        /* Invalidate the item's area for redraw */
        InvalidateItemRect(theDialog, &(itemEx)->bounds);
    }

}

/*
 * ShowDialogItem - Show a hidden dialog item
 */
void ShowDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    if (!itemEx->visible) {
        itemEx->visible = true;
        /* Redraw the item */
        DrawDialogItem(theDialog, itemNo);
    }

}

/*
 * FindDialogItem - Find dialog item at a point
 */
SInt16 FindDialogItem(DialogPtr theDialog, Point thePt)
{
    DialogItemCache* cache;
    SInt16 foundItem = 0;

    if (!theDialog) {
        return 0;
    }

    cache = GetDialogItemCache(theDialog);
    if (!cache || !cache->items) {
        return 0;
    }

    /* Check items from front to back (highest number first) */
    for (SInt16 i = cache->itemCount; i >= 1; i--) {
        DialogItemEx* item = &cache->items[i - 1];
        if (item->visible && item->enabled) {
            Rect bounds = (item)->bounds;
            if (thePt.h >= bounds.left && thePt.h < bounds.right &&
                thePt.v >= bounds.top && thePt.v < bounds.bottom) {
                foundItem = i;
                break;
            }
        }
    }

    return foundItem;
}

/*
 * GetDialogItemText - Get text from a dialog item
 */
void GetDialogItemText(Handle item, unsigned char* text)
{
    if (!item || !text) {
        if (text) text[0] = 0;
        return;
    }

    if (!*item) {
        text[0] = 0;
        return;
    }

    /* Item handle contains a Pascal string - copy it out */
    HLock(item);
    unsigned char *src = (unsigned char *)*item;
    unsigned char len = src[0];
    text[0] = len;
    if (len > 0) {
        memcpy(&text[1], &src[1], len);
    }
    HUnlock(item);
}

/*
 * SetDialogItemText - Set text for a dialog item
 */
void SetDialogItemText(Handle item, const unsigned char* text)
{
    if (!item || !text) {
        return;
    }

    /* Resize handle to fit the Pascal string */
    unsigned char len = text[0];
    SetHandleSize(item, (Size)(len + 1));
    if (MemError() != noErr) {
        return;
    }

    HLock(item);
    memcpy(*item, text, len + 1);
    HUnlock(item);
}

/*
 * SelectDialogItemText - Select text in a dialog item
 */
void SelectDialogItemText(DialogPtr theDialog, SInt16 itemNo, SInt16 strtSel, SInt16 endSel)
{
    (void)strtSel;
    (void)endSel;
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    /* Check if this is a text item */
    SInt16 itemType = (itemEx)->type & itemTypeMask;
    if (itemType != editText) {
        return;
    }

    /* In a full implementation, this would work with TextEdit */
}

/*
 * EnableDialogItem - Enable a dialog item
 */
void EnableDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    if (!itemEx->enabled) {
        itemEx->enabled = true;
        /* Remove disabled flag from type */
        (itemEx)->type &= ~itemDisable;
        /* Redraw the item */
        InvalDialogItem(theDialog, itemNo);
    }

}

/*
 * DisableDialogItem - Disable a dialog item
 */
void DisableDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    if (itemEx->enabled) {
        itemEx->enabled = false;
        /* Add disabled flag to type */
        (itemEx)->type |= itemDisable;
        /* Redraw the item */
        InvalDialogItem(theDialog, itemNo);
    }

}

/*
 * IsDialogItemEnabled - Check if dialog item is enabled
 */
Boolean IsDialogItemEnabled(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return false;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return false;
    }

    return itemEx->enabled;
}

/*
 * IsDialogItemVisible - Check if dialog item is visible
 */
Boolean IsDialogItemVisible(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return false;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return false;
    }

    return itemEx->visible;
}

/*
 * AppendDITL - Append items to dialog
 */
void AppendDITL(DialogPtr theDialog, Handle theHandle, DITLMethod method)
{
    DialogItemCache* cache;
    DialogItemEx* newItems = NULL;
    SInt16 newItemCount = 0;
    OSErr err;

    if (!theDialog || !theHandle) {
        return;
    }

    cache = GetDialogItemCache(theDialog);
    if (!cache) {
        return;
    }

    /* Parse the new item list */
    err = ParseDialogItemList(theHandle, &newItems, &newItemCount);
    if (err != 0 || !newItems || newItemCount == 0) {
        if (newItems) DisposePtr((Ptr)newItems);
        return;
    }

    /* Position new items based on method */
    Rect dialogBounds = {0, 0, 400, 600}; /* Default dialog size */
    SInt16 offsetH = 0, offsetV = 0;

    switch (method) {
        case appendDITLRight:
            offsetH = dialogBounds.right;
            break;
        case appendDITLBottom:
            offsetV = dialogBounds.bottom;
            break;
        case overlayDITL:
        default:
            /* No offset for overlay */
            break;
    }

    /* Adjust positions of new items with overflow protection */
    for (SInt16 i = 0; i < newItemCount; i++) {
        /* Check for overflow/underflow before adding offsets */
        if (offsetH > 0) {
            /* Adding positive offset - check for overflow */
            if (newItems[i].bounds.left > SHRT_MAX - offsetH) {
                newItems[i].bounds.left = SHRT_MAX;
            } else {
                newItems[i].bounds.left += offsetH;
            }
            if (newItems[i].bounds.right > SHRT_MAX - offsetH) {
                newItems[i].bounds.right = SHRT_MAX;
            } else {
                newItems[i].bounds.right += offsetH;
            }
        } else if (offsetH < 0) {
            /* Adding negative offset - check for underflow */
            if (newItems[i].bounds.left < SHRT_MIN - offsetH) {
                newItems[i].bounds.left = SHRT_MIN;
            } else {
                newItems[i].bounds.left += offsetH;
            }
            if (newItems[i].bounds.right < SHRT_MIN - offsetH) {
                newItems[i].bounds.right = SHRT_MIN;
            } else {
                newItems[i].bounds.right += offsetH;
            }
        } else {
            /* offsetH is 0, no change needed */
        }

        if (offsetV > 0) {
            /* Adding positive offset - check for overflow */
            if (newItems[i].bounds.top > SHRT_MAX - offsetV) {
                newItems[i].bounds.top = SHRT_MAX;
            } else {
                newItems[i].bounds.top += offsetV;
            }
            if (newItems[i].bounds.bottom > SHRT_MAX - offsetV) {
                newItems[i].bounds.bottom = SHRT_MAX;
            } else {
                newItems[i].bounds.bottom += offsetV;
            }
        } else if (offsetV < 0) {
            /* Adding negative offset - check for underflow */
            if (newItems[i].bounds.top < SHRT_MIN - offsetV) {
                newItems[i].bounds.top = SHRT_MIN;
            } else {
                newItems[i].bounds.top += offsetV;
            }
            if (newItems[i].bounds.bottom < SHRT_MIN - offsetV) {
                newItems[i].bounds.bottom = SHRT_MIN;
            } else {
                newItems[i].bounds.bottom += offsetV;
            }
        } else {
            /* offsetV is 0, no change needed */
        }
    }

    /* Resize the cache to accommodate new items */
    SInt16 totalItems = cache->itemCount + newItemCount;

    /* Check for integer overflow in addition */
    if (totalItems < cache->itemCount || totalItems < newItemCount) {
        DisposePtr((Ptr)newItems);
        return;
    }

    /* Check for integer overflow in multiplication */
    if (totalItems > 0 && SIZE_MAX / totalItems < sizeof(DialogItemEx)) {
        DisposePtr((Ptr)newItems);
        return;
    }

    Size oldSize = cache->itemCount * sizeof(DialogItemEx);
    DialogItemEx* expandedItems = (DialogItemEx*)NewPtr(totalItems * sizeof(DialogItemEx));
    if (!expandedItems) {
        DisposePtr((Ptr)newItems);
        return;
    }

    if (cache->items) {
        BlockMoveData(cache->items, expandedItems, oldSize);
        DisposePtr((Ptr)cache->items);
    }

    cache->items = expandedItems;

    /* Copy new items to cache */
    memcpy(&cache->items[cache->itemCount], newItems, newItemCount * sizeof(DialogItemEx));
    cache->itemCount = totalItems;
    cache->needsUpdate = true;

    DisposePtr((Ptr)newItems);

}

/*
 * CountDITL - Count items in dialog
 */
SInt16 CountDITL(DialogPtr theDialog)
{
    DialogItemCache* cache;

    if (!theDialog) {
        return 0;
    }

    cache = GetDialogItemCache(theDialog);
    if (!cache) {
        return 0;
    }

    return cache->itemCount;
}

/*
 * ShortenDITL - Remove items from end of dialog
 */
void ShortenDITL(DialogPtr theDialog, SInt16 numberItems)
{
    DialogItemCache* cache;

    if (!theDialog || numberItems <= 0) {
        return;
    }

    cache = GetDialogItemCache(theDialog);
    if (!cache) {
        return;
    }

    if (numberItems >= cache->itemCount) {
        /* Removing all items */
        cache->itemCount = 0;
        if (cache->items) {
            DisposePtr((Ptr)cache->items);
            cache->items = NULL;
        }
    } else {
        /* Remove items from the end */
        SInt16 newCount = cache->itemCount - numberItems;

        /* Invalidate the items being removed */
        for (SInt16 i = newCount; i < cache->itemCount; i++) {
            InvalidateItemRect(theDialog, &cache->items[i].bounds);
        }

        cache->itemCount = newCount;
        /* Shrink the array */
        if (newCount > 0) {
            DialogItemEx* shrunkItems = (DialogItemEx*)NewPtr(newCount * sizeof(DialogItemEx));
            if (shrunkItems) {
                BlockMoveData(cache->items, shrunkItems, newCount * sizeof(DialogItemEx));
                DisposePtr((Ptr)cache->items);
                cache->items = shrunkItems;
            }
            /* If allocation fails, keep the old (larger) buffer */
        } else {
            DisposePtr((Ptr)cache->items);
            cache->items = NULL;
        }
    }

    cache->needsUpdate = true;

}

/*
 * SetUserItemProc - Set procedure for user item
 */
void SetUserItemProc(DialogPtr theDialog, SInt16 itemNo, UserItemProcPtr procPtr)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    /* Check if this is a user item */
    SInt16 itemType = (itemEx)->type & itemTypeMask;
    if (itemType != userItem) {
        return;
    }

    /* Set the procedure as the item handle */
    (itemEx)->handle = (Handle)procPtr;

}

/*
 * GetUserItemProc - Get procedure for user item
 */
UserItemProcPtr GetUserItemProc(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return NULL;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return NULL;
    }

    /* Check if this is a user item */
    SInt16 itemType = (itemEx)->type & itemTypeMask;
    if (itemType != userItem) {
        return NULL;
    }

    return (UserItemProcPtr)(itemEx)->handle;
}

/*
 * DrawDialogItem - Draw a specific dialog item
 */
void DrawDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx || !itemEx->visible) {
        return;
    }

    /* In the dialog's own port: items were drawn in whatever port was
     * current, so focusing the Finder's rename field drew it a second time
     * in the Finder window underneath. */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)theDialog);
    DrawDialogItemByType(theDialog, itemNo, itemEx);
    SetPort(savePort);

}

/*
 * InvalDialogItem - Invalidate dialog item for redrawing
 */
void InvalDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    InvalidateItemRect(theDialog, &(itemEx)->bounds);
}

/*
 * FrameDialogItem - Draw frame around dialog item
 */
void FrameDialogItem(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx || !itemEx->visible) {
        return;
    }

    /* Draw a frame around the item bounds */
    Rect frameRect = (itemEx)->bounds;
    frameRect.left -= 4;
    frameRect.top -= 4;
    frameRect.right += 4;
    frameRect.bottom += 4;

    /* In a full implementation, this would draw an actual frame */
}

/*
 * Private implementation functions
 */

static DialogItemCache* GetDialogItemCache(DialogPtr theDialog)
{
    if (!theDialog || !gDialogItemState.initialized) {
        return NULL;
    }

    /* Look for existing cache */
    for (int i = 0; i < gDialogItemState.cacheCount; i++) {
        if (gDialogItemState.cache[i].dialog == theDialog) {
            return &gDialogItemState.cache[i];
        }
    }

    /* Create new cache */
    return CreateDialogItemCache(theDialog);
}

static DialogItemCache* CreateDialogItemCache(DialogPtr theDialog)
{
    if (!theDialog ||
        gDialogItemState.cacheCount >= DIALOG_ITEM_CACHE_CAPACITY) {
        return NULL;
    }

    DialogItemCache* cache = &gDialogItemState.cache[gDialogItemState.cacheCount];
    cache->dialog = theDialog;
    cache->itemCount = 0;
    cache->items = NULL;
    cache->needsUpdate = true;
    cache->lastUpdateTime = TickCount();

    /* Parse dialog's item list */
    Handle itemList = GetDialogItemList(theDialog);
    if (itemList) {
        OSErr err = ParseDialogItemList(itemList, &cache->items, &cache->itemCount);
        if (err != 0) {
            return NULL;
        }
    }

    gDialogItemState.cacheCount++;

    return cache;
}

static OSErr ParseDialogItemList(Handle itemList, DialogItemEx** items, SInt16* itemCount)
{
    /* Use the new resource parser */
    return ParseDITL(itemList, items, itemCount);
}

static DialogItemEx* GetDialogItemEx(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemCache* cache = GetDialogItemCache(theDialog);
    if (!cache || !cache->items || itemNo < 1 || itemNo > cache->itemCount) {
        return NULL;
    }

    return &cache->items[itemNo - 1]; /* Convert to 0-based index */
}

static Boolean ValidateItemNumber(DialogPtr theDialog, SInt16 itemNo)
{
    if (!theDialog || itemNo < 1) {
        return false;
    }

    DialogItemCache* cache = GetDialogItemCache(theDialog);
    if (!cache) {
        return false;
    }

    return (itemNo <= cache->itemCount);
}

static void InvalidateItemRect(DialogPtr theDialog, const Rect* rect)
{
    if (!theDialog || !rect) {
        return;
    }

    /* In the dialog's port; the current one was often another window */
    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)theDialog);
    InvalRect(rect);
    SetPort(savePort);
}

void CleanupDialogItems(void)
{
    if (!gDialogItemState.initialized) {
        return;
    }

    /* Clean up all cached dialog items */
    for (int i = 0; i < gDialogItemState.cacheCount; i++) {
        DisposeDialogItemCache(&gDialogItemState.cache[i]);
    }

    gDialogItemState.cacheCount = 0;
    gDialogItemState.initialized = false;

}

static void DisposeDialogItemCache(DialogItemCache* cache)
{
    if (!cache) {
        return;
    }

    if (cache->items) {
        DisposePtr((Ptr)cache->items);
        cache->items = NULL;
    }

    cache->dialog = NULL;
    cache->itemCount = 0;
    cache->needsUpdate = false;
}

/*
 * RemoveDialogItemCache - Remove and dispose cache for a dialog
 *
 * This function is called when a dialog is disposed to prevent
 * memory leaks and stale cache entries that could be matched if
 * a new dialog is created at the same memory address.
 *
 * Parameters:
 *   theDialog - The dialog whose cache should be removed
 */
void RemoveDialogItemCache(DialogPtr theDialog)
{
    if (!theDialog || !gDialogItemState.initialized) {
        return;
    }

    /* Search for cache entry matching this dialog */
    for (int i = 0; i < gDialogItemState.cacheCount; i++) {
        if (gDialogItemState.cache[i].dialog == theDialog) {
            /* Dispose of the cache data */
            DisposeDialogItemCache(&gDialogItemState.cache[i]);

            /* Shift remaining caches down to fill the gap */
            for (int j = i; j < gDialogItemState.cacheCount - 1; j++) {
                gDialogItemState.cache[j] = gDialogItemState.cache[j + 1];
            }

            /* Clear the last entry */
            gDialogItemState.cache[gDialogItemState.cacheCount - 1].dialog = NULL;
            gDialogItemState.cache[gDialogItemState.cacheCount - 1].itemCount = 0;
            gDialogItemState.cache[gDialogItemState.cacheCount - 1].items = NULL;
            gDialogItemState.cache[gDialogItemState.cacheCount - 1].needsUpdate = false;

            /* Decrement count */
            gDialogItemState.cacheCount--;

            return;
        }
    }
}

/*
 * The on/off state of a dialog's checkbox or radio button item. It is kept
 * in the item's refCon, which drawing reads; GetDialogItem hands out the
 * title's handle rather than a control, so the Dialog Manager keeps it.
 */
SInt32 DM_GetItemState(DialogPtr theDialog, SInt16 itemNo)
{
    DialogItemEx* itemEx = (theDialog && ValidateItemNumber(theDialog, itemNo))
                           ? GetDialogItemEx(theDialog, itemNo) : NULL;
    return itemEx ? itemEx->refCon : 0;
}

void DM_SetItemState(DialogPtr theDialog, SInt16 itemNo, SInt32 value)
{
    DialogItemEx* itemEx = (theDialog && ValidateItemNumber(theDialog, itemNo))
                           ? GetDialogItemEx(theDialog, itemNo) : NULL;
    if (itemEx && itemEx->refCon != value) {
        itemEx->refCon = value;
        InvalDialogItem(theDialog, itemNo);
    }
}

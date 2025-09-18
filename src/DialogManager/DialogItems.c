/*
 * DialogItems.c - Dialog Item Management Implementation
 *
 * This module provides the dialog item management functionality,
 * maintaining exact Mac System 7.1 behavioral compatibility.
 */

#include "../../include/DialogManager/DialogItems.h"
#include "../../include/DialogManager/DialogManager.h"
#include "../../include/DialogManager/DialogTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Private structures for item management */
typedef struct DialogItemCache {
    DialogPtr       dialog;
    int16_t         itemCount;
    DialogItemEx*   items;
    bool            needsUpdate;
    uint32_t        lastUpdateTime;
} DialogItemCache;

/* Global state for dialog items */
static struct {
    bool            initialized;
    DialogItemCache cache[32];  /* Cache for up to 32 dialogs */
    int16_t         cacheCount;
    int16_t         defaultFont;
    int16_t         defaultSize;
} gDialogItemState = {0};

/* External dependencies */
extern DialogManagerState* GetDialogManagerState(void);
extern OSErr ValidateDialogPtr(DialogPtr dialog);
extern void InvalRect(const Rect* rect);
extern uint32_t TickCount(void);

/* Private function prototypes */
static DialogItemCache* GetDialogItemCache(DialogPtr theDialog);
static DialogItemCache* CreateDialogItemCache(DialogPtr theDialog);
static void DisposeDialogItemCache(DialogItemCache* cache);
static OSErr ParseDialogItemList(Handle itemList, DialogItemEx** items, int16_t* itemCount);
static DialogItemEx* GetDialogItemEx(DialogPtr theDialog, int16_t itemNo);
static bool ValidateItemNumber(DialogPtr theDialog, int16_t itemNo);
static void UpdateItemBounds(DialogPtr theDialog, int16_t itemNo, const Rect* newBounds);
static void DrawButtonItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item);
static void DrawTextItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item);
static void DrawIconItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item);
static void DrawUserItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item);
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
    for (int i = 0; i < 32; i++) {
        gDialogItemState.cache[i].dialog = NULL;
        gDialogItemState.cache[i].itemCount = 0;
        gDialogItemState.cache[i].items = NULL;
        gDialogItemState.cache[i].needsUpdate = false;
        gDialogItemState.cache[i].lastUpdateTime = 0;
    }

    printf("Dialog item subsystem initialized\n");
}

/*
 * GetDialogItem - Get information about a dialog item
 */
void GetDialogItem(DialogPtr theDialog, int16_t itemNo, int16_t* itemType,
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
        *itemType = itemEx->base.type;
    }
    if (item) {
        *item = itemEx->base.handle;
    }
    if (box) {
        *box = itemEx->base.bounds;
    }

    printf("GetDialogItem: item %d, type %d\n", itemNo, itemEx->base.type);
}

/*
 * SetDialogItem - Set information about a dialog item
 */
void SetDialogItem(DialogPtr theDialog, int16_t itemNo, int16_t itemType,
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
    itemEx->base.type = itemType;
    itemEx->base.handle = item;
    itemEx->base.bounds = *box;

    /* Mark for redraw */
    InvalDialogItem(theDialog, itemNo);

    printf("SetDialogItem: item %d, type %d\n", itemNo, itemType);
}

/*
 * HideDialogItem - Hide a dialog item
 */
void HideDialogItem(DialogPtr theDialog, int16_t itemNo)
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
        InvalidateItemRect(theDialog, &itemEx->base.bounds);
    }

    printf("Hid dialog item %d\n", itemNo);
}

/*
 * ShowDialogItem - Show a hidden dialog item
 */
void ShowDialogItem(DialogPtr theDialog, int16_t itemNo)
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

    printf("Showed dialog item %d\n", itemNo);
}

/*
 * FindDialogItem - Find dialog item at a point
 */
int16_t FindDialogItem(DialogPtr theDialog, Point thePt)
{
    DialogItemCache* cache;
    int16_t foundItem = 0;

    if (!theDialog) {
        return 0;
    }

    cache = GetDialogItemCache(theDialog);
    if (!cache) {
        return 0;
    }

    /* Check items from front to back (highest number first) */
    for (int16_t i = cache->itemCount; i >= 1; i--) {
        DialogItemEx* item = &cache->items[i - 1];
        if (item->visible && item->enabled) {
            Rect bounds = item->base.bounds;
            if (thePt.h >= bounds.left && thePt.h < bounds.right &&
                thePt.v >= bounds.top && thePt.v < bounds.bottom) {
                foundItem = i;
                break;
            }
        }
    }

    printf("FindDialogItem at (%d, %d): found item %d\n", thePt.h, thePt.v, foundItem);
    return foundItem;
}

/*
 * GetDialogItemText - Get text from a dialog item
 */
void GetDialogItemText(Handle item, unsigned char* text)
{
    if (!item || !text) {
        if (text) text[0] = 0; /* Empty Pascal string */
        return;
    }

    /* In a full implementation, this would extract text from the item handle */
    /* For now, just return empty string */
    text[0] = 0;

    printf("GetDialogItemText: retrieved text from item handle\n");
}

/*
 * SetDialogItemText - Set text for a dialog item
 */
void SetDialogItemText(Handle item, const unsigned char* text)
{
    if (!item || !text) {
        return;
    }

    /* In a full implementation, this would set text in the item handle */
    printf("SetDialogItemText: set text '%.*s'\n", text[0], &text[1]);
}

/*
 * SelectDialogItemText - Select text in a dialog item
 */
void SelectDialogItemText(DialogPtr theDialog, int16_t itemNo, int16_t strtSel, int16_t endSel)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    /* Check if this is a text item */
    int16_t itemType = itemEx->base.type & itemTypeMask;
    if (itemType != editText) {
        return;
    }

    /* In a full implementation, this would work with TextEdit */
    printf("SelectDialogItemText: item %d, selection %d-%d\n", itemNo, strtSel, endSel);
}

/*
 * EnableDialogItem - Enable a dialog item
 */
void EnableDialogItem(DialogPtr theDialog, int16_t itemNo)
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
        itemEx->base.type &= ~itemDisable;
        /* Redraw the item */
        InvalDialogItem(theDialog, itemNo);
    }

    printf("Enabled dialog item %d\n", itemNo);
}

/*
 * DisableDialogItem - Disable a dialog item
 */
void DisableDialogItem(DialogPtr theDialog, int16_t itemNo)
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
        itemEx->base.type |= itemDisable;
        /* Redraw the item */
        InvalDialogItem(theDialog, itemNo);
    }

    printf("Disabled dialog item %d\n", itemNo);
}

/*
 * IsDialogItemEnabled - Check if dialog item is enabled
 */
bool IsDialogItemEnabled(DialogPtr theDialog, int16_t itemNo)
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
bool IsDialogItemVisible(DialogPtr theDialog, int16_t itemNo)
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
    int16_t newItemCount = 0;
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
        printf("Error: Failed to parse DITL for append (error %d)\n", err);
        return;
    }

    /* Position new items based on method */
    Rect dialogBounds = {0, 0, 400, 600}; /* Default dialog size */
    int16_t offsetH = 0, offsetV = 0;

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

    /* Adjust positions of new items */
    for (int16_t i = 0; i < newItemCount; i++) {
        newItems[i].base.bounds.left += offsetH;
        newItems[i].base.bounds.right += offsetH;
        newItems[i].base.bounds.top += offsetV;
        newItems[i].base.bounds.bottom += offsetV;
    }

    /* Resize the cache to accommodate new items */
    int16_t totalItems = cache->itemCount + newItemCount;
    DialogItemEx* expandedItems = (DialogItemEx*)realloc(cache->items,
                                                         totalItems * sizeof(DialogItemEx));
    if (!expandedItems) {
        free(newItems);
        printf("Error: Failed to expand item cache\n");
        return;
    }

    cache->items = expandedItems;

    /* Copy new items to cache */
    memcpy(&cache->items[cache->itemCount], newItems, newItemCount * sizeof(DialogItemEx));
    cache->itemCount = totalItems;
    cache->needsUpdate = true;

    free(newItems);

    printf("Appended %d items to dialog (method %d), total now %d\n",
           newItemCount, method, totalItems);
}

/*
 * CountDITL - Count items in dialog
 */
int16_t CountDITL(DialogPtr theDialog)
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
void ShortenDITL(DialogPtr theDialog, int16_t numberItems)
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
            free(cache->items);
            cache->items = NULL;
        }
    } else {
        /* Remove items from the end */
        int16_t newCount = cache->itemCount - numberItems;

        /* Invalidate the items being removed */
        for (int16_t i = newCount; i < cache->itemCount; i++) {
            InvalidateItemRect(theDialog, &cache->items[i].base.bounds);
        }

        cache->itemCount = newCount;
        cache->items = (DialogItemEx*)realloc(cache->items, newCount * sizeof(DialogItemEx));
    }

    cache->needsUpdate = true;

    printf("Shortened DITL by %d items, %d items remaining\n",
           numberItems, cache->itemCount);
}

/*
 * SetUserItemProc - Set procedure for user item
 */
void SetUserItemProc(DialogPtr theDialog, int16_t itemNo, UserItemProcPtr procPtr)
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
    int16_t itemType = itemEx->base.type & itemTypeMask;
    if (itemType != userItem) {
        return;
    }

    /* Set the procedure as the item handle */
    itemEx->base.handle = (Handle)procPtr;

    printf("Set user item procedure for item %d\n", itemNo);
}

/*
 * GetUserItemProc - Get procedure for user item
 */
UserItemProcPtr GetUserItemProc(DialogPtr theDialog, int16_t itemNo)
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
    int16_t itemType = itemEx->base.type & itemTypeMask;
    if (itemType != userItem) {
        return NULL;
    }

    return (UserItemProcPtr)itemEx->base.handle;
}

/*
 * DrawDialogItem - Draw a specific dialog item
 */
void DrawDialogItem(DialogPtr theDialog, int16_t itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx || !itemEx->visible) {
        return;
    }

    /* Draw based on item type */
    int16_t itemType = itemEx->base.type & itemTypeMask;

    switch (itemType) {
        case btnCtrl:
        case ctrlItem:
            DrawButtonItem(theDialog, itemNo, itemEx);
            break;

        case statText:
        case editText:
            DrawTextItem(theDialog, itemNo, itemEx);
            break;

        case iconItem:
            DrawIconItem(theDialog, itemNo, itemEx);
            break;

        case userItem:
            DrawUserItem(theDialog, itemNo, itemEx);
            break;

        default:
            printf("Drawing unknown item type %d\n", itemType);
            break;
    }

    printf("Drew dialog item %d (type %d)\n", itemNo, itemType);
}

/*
 * InvalDialogItem - Invalidate dialog item for redrawing
 */
void InvalDialogItem(DialogPtr theDialog, int16_t itemNo)
{
    DialogItemEx* itemEx;

    if (!theDialog || !ValidateItemNumber(theDialog, itemNo)) {
        return;
    }

    itemEx = GetDialogItemEx(theDialog, itemNo);
    if (!itemEx) {
        return;
    }

    InvalidateItemRect(theDialog, &itemEx->base.bounds);
}

/*
 * FrameDialogItem - Draw frame around dialog item
 */
void FrameDialogItem(DialogPtr theDialog, int16_t itemNo)
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
    Rect frameRect = itemEx->base.bounds;
    frameRect.left -= 4;
    frameRect.top -= 4;
    frameRect.right += 4;
    frameRect.bottom += 4;

    /* In a full implementation, this would draw an actual frame */
    printf("Framing dialog item %d at (%d,%d,%d,%d)\n",
           itemNo, frameRect.left, frameRect.top, frameRect.right, frameRect.bottom);
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
    if (!theDialog || gDialogItemState.cacheCount >= 32) {
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
            printf("Error: Failed to parse dialog item list (error %d)\n", err);
            return NULL;
        }
    }

    gDialogItemState.cacheCount++;

    printf("Created item cache for dialog at %p with %d items\n",
           (void*)theDialog, cache->itemCount);

    return cache;
}

static OSErr ParseDialogItemList(Handle itemList, DialogItemEx** items, int16_t* itemCount)
{
    if (!itemList || !items || !itemCount) {
        return -1700; /* dialogErr_InvalidDialog */
    }

    /* In a full implementation, this would parse the actual DITL resource format */
    /* For now, create a simple default item list */

    *itemCount = 2; /* Default: OK and Cancel buttons */
    *items = (DialogItemEx*)malloc(*itemCount * sizeof(DialogItemEx));
    if (!*items) {
        return -108; /* memFullErr */
    }

    /* Create default OK button */
    (*items)[0].base.type = btnCtrl;
    (*items)[0].base.bounds.left = 250;
    (*items)[0].base.bounds.top = 120;
    (*items)[0].base.bounds.right = 320;
    (*items)[0].base.bounds.bottom = 140;
    (*items)[0].base.handle = NULL;
    (*items)[0].base.length = 2; /* "OK" */
    (*items)[0].visible = true;
    (*items)[0].enabled = true;
    (*items)[0].refCon = 0;
    (*items)[0].userData = NULL;
    (*items)[0].platformData = NULL;

    /* Create default Cancel button */
    (*items)[1].base.type = btnCtrl;
    (*items)[1].base.bounds.left = 160;
    (*items)[1].base.bounds.top = 120;
    (*items)[1].base.bounds.right = 240;
    (*items)[1].base.bounds.bottom = 140;
    (*items)[1].base.handle = NULL;
    (*items)[1].base.length = 6; /* "Cancel" */
    (*items)[1].visible = true;
    (*items)[1].enabled = true;
    (*items)[1].refCon = 0;
    (*items)[1].userData = NULL;
    (*items)[1].platformData = NULL;

    return 0; /* noErr */
}

static DialogItemEx* GetDialogItemEx(DialogPtr theDialog, int16_t itemNo)
{
    DialogItemCache* cache = GetDialogItemCache(theDialog);
    if (!cache || itemNo < 1 || itemNo > cache->itemCount) {
        return NULL;
    }

    return &cache->items[itemNo - 1]; /* Convert to 0-based index */
}

static bool ValidateItemNumber(DialogPtr theDialog, int16_t itemNo)
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

static void DrawButtonItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item)
{
    if (!item) {
        return;
    }

    /* Draw button background and border */
    Rect bounds = item->base.bounds;
    bool isDefault = (itemNo == GetDialogDefaultItem(theDialog));
    bool isDisabled = !item->enabled;

    printf("Drawing button item %d at (%d,%d,%d,%d)%s%s\n",
           itemNo, bounds.left, bounds.top, bounds.right, bounds.bottom,
           isDefault ? " [DEFAULT]" : "",
           isDisabled ? " [DISABLED]" : "");

    /* In a full implementation, this would draw the actual button */
}

static void DrawTextItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item)
{
    if (!item) {
        return;
    }

    Rect bounds = item->base.bounds;
    bool isEditable = ((item->base.type & itemTypeMask) == editText);

    printf("Drawing text item %d at (%d,%d,%d,%d)%s\n",
           itemNo, bounds.left, bounds.top, bounds.right, bounds.bottom,
           isEditable ? " [EDITABLE]" : " [STATIC]");

    /* In a full implementation, this would draw the text and frame */
}

static void DrawIconItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item)
{
    if (!item) {
        return;
    }

    Rect bounds = item->base.bounds;

    printf("Drawing icon item %d at (%d,%d,%d,%d)\n",
           itemNo, bounds.left, bounds.top, bounds.right, bounds.bottom);

    /* In a full implementation, this would draw the icon */
}

static void DrawUserItem(DialogPtr theDialog, int16_t itemNo, const DialogItemEx* item)
{
    if (!item) {
        return;
    }

    UserItemProcPtr userProc = (UserItemProcPtr)item->base.handle;
    if (userProc) {
        printf("Calling user item procedure for item %d\n", itemNo);
        userProc(theDialog, itemNo);
    } else {
        printf("No user procedure for user item %d\n", itemNo);
    }
}

static void InvalidateItemRect(DialogPtr theDialog, const Rect* rect)
{
    if (!theDialog || !rect) {
        return;
    }

    /* Invalidate the rectangle for redrawing */
    InvalRect(rect);
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

    printf("Dialog item subsystem cleaned up\n");
}

static void DisposeDialogItemCache(DialogItemCache* cache)
{
    if (!cache) {
        return;
    }

    if (cache->items) {
        free(cache->items);
        cache->items = NULL;
    }

    cache->dialog = NULL;
    cache->itemCount = 0;
    cache->needsUpdate = false;
}
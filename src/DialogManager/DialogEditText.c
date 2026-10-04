/*
 * DialogEditText.c - Dialog Edit Text Focus and Caret Management
 *
 * Implements edit-text focus tracking and caret blinking for System 7.1 dialogs.
 * Addresses the compatibility gap noted in System7_Compatibility_Gaps.md:
 * - Edit-text items ignore focus rings
 * - System 7 drew a focus frame and moved the caret when the control is active
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogEditText.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/DialogInternal.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogManagerInternal.h"
#include "DialogManager/DialogManagerStateExt.h"
#include "DialogManager/DialogLogging.h"
#include "MemoryMgr/MemoryManager.h"
#include "TextEdit/TextEdit.h"
#include "TimeManager/TimeBase.h"

/* Caret blink rate in ticks (System 7 standard was ~30 ticks = 0.5 seconds) */
#define kCaretBlinkRate 30

static DialogEditTextState *FindDialogState(DialogPtr owner, Boolean create)
{
    DialogManagerState* state = GetDialogManagerState();
    DialogManagerState_Extended* extended;
    DialogEditTextState* entry;
    if (!state || !owner) return NULL;
    extended = GET_EXTENDED_DLG_STATE(state);
    for (entry = extended->dialogStates; entry; entry = entry->next)
        if (entry->owner == owner) return entry;
    if (!create) return NULL;
    entry = (DialogEditTextState*)NewPtrClear(sizeof(*entry));
    if (entry) {
        entry->owner = owner;
        entry->caretVisible = true;
        entry->next = extended->dialogStates;
        extended->dialogStates = entry;
    }
    return entry;
}

TEHandle DialogEditText_GetHandle(DialogPtr dialog, SInt16 itemNo)
{
    DialogEditTextState* entry;
    if (itemNo < 1 || itemNo >= 256) return NULL;
    entry = FindDialogState(dialog, false);
    return entry ? (TEHandle)entry->teHandles[itemNo] : NULL;
}

Boolean DialogEditText_CaretVisible(DialogPtr dialog)
{
    DialogEditTextState* entry = FindDialogState(dialog, false);
    return entry ? entry->caretVisible : false;
}

void DialogEditText_SetCaretActive(DialogPtr dialog, Boolean active)
{
    DialogEditTextState* entry = FindDialogState(dialog, active);
    if (!entry) return;
    entry->caretVisible = active;
    entry->caretBlinkTime = TickCount();
}

/*
 * SetDialogEditTextFocus - Set keyboard focus to an edit-text item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The edit-text item number to focus (0 to clear focus)
 */
void SetDialogEditTextFocus(DialogPtr theDialog, SInt16 itemNo) {
    DialogEditTextState* dialogState;
    SInt16 oldFocusItem;

    dialogState = FindDialogState(theDialog, itemNo > 0);
    if (!dialogState) return;
    oldFocusItem = dialogState->focusedItem;

    /* Clear old focus */
    if (oldFocusItem > 0 && oldFocusItem != itemNo) {
        dialogState->focusedItem = 0;
        dialogState->caretVisible = false;
        InvalDialogItem(theDialog, oldFocusItem);
        DrawDialogItem(theDialog, oldFocusItem);
    }

    /* Set new focus */
    if (itemNo > 0) {
        dialogState->focusedItem = itemNo;
        dialogState->caretBlinkTime = TickCount();
        dialogState->caretVisible = true;

        /* Materialise the TextEdit record and select the whole field before
         * redrawing. Focusing a field in System 7 selects its contents, so the
         * name in a rename box is ready to be typed over; leaving the record
         * to be created lazily on the first click meant the field opened with
         * nothing selected and the draw had no selection to show. */
        if (itemNo != oldFocusItem) {
            TEHandle hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
            if (hTE && *hTE) {
                TESetSelect(0, (SInt32)(**hTE).teLength, hTE);
            }
        }

        InvalDialogItem(theDialog, itemNo);
        DrawDialogItem(theDialog, itemNo);

    } else {
        dialogState->focusedItem = 0;
        dialogState->caretVisible = false;
    }
}

/*
 * GetDialogEditTextFocus - Get the currently focused edit-text item
 *
 * Parameters:
 *   theDialog - The dialog to query
 *
 * Returns:
 *   The item number of the focused edit-text item, or 0 if none
 */
SInt16 GetDialogEditTextFocus(DialogPtr theDialog) {
    DialogEditTextState* dialogState = FindDialogState(theDialog, false);
    return dialogState ? dialogState->focusedItem : 0;
}

/*
 * UpdateDialogCaret - Update caret blink state
 *
 * Should be called from the dialog event loop to maintain caret blinking.
 * This implements the classic Mac OS caret blinking behavior.
 *
 * Parameters:
 *   theDialog - The dialog to update
 */
void UpdateDialogCaret(DialogPtr theDialog) {
    DialogEditTextState* dialogState = FindDialogState(theDialog, false);
    UInt32 currentTicks;
    UInt32 elapsed;

    if (!dialogState || !theDialog || dialogState->focusedItem == 0) {
        return;
    }

    currentTicks = TickCount();

    /* Handle tick counter wrap-around */
    if (currentTicks < dialogState->caretBlinkTime) {
        dialogState->caretBlinkTime = currentTicks;
        return;
    }

    elapsed = currentTicks - dialogState->caretBlinkTime;

    /* Toggle caret visibility every kCaretBlinkRate ticks */
    if (elapsed >= kCaretBlinkRate) {
        dialogState->caretVisible = !dialogState->caretVisible;
        dialogState->caretBlinkTime = currentTicks;

        /* Redraw the focused edit-text item */
        InvalDialogItem(theDialog, dialogState->focusedItem);
        DrawDialogItem(theDialog, dialogState->focusedItem);

    }
}

/*
 * AdvanceDialogEditTextFocus - Move focus to next/previous edit-text item
 *
 * Implements Tab/Shift-Tab navigation between edit-text fields.
 *
 * Parameters:
 *   theDialog - The dialog containing the items
 *   backward  - true to move backward (Shift-Tab), false for forward (Tab)
 */
void AdvanceDialogEditTextFocus(DialogPtr theDialog, Boolean backward) {
    DialogEditTextState* dialogState = FindDialogState(theDialog, false);
    SInt16 itemCount;
    SInt16 currentFocus;
    SInt16 nextFocus;
    SInt16 i;
    SInt16 itemType;
    Handle itemHandle;
    Rect itemBox;

    if (!dialogState || !theDialog) {
        return;
    }

    itemCount = CountDITL(theDialog);
    currentFocus = dialogState->focusedItem;
    nextFocus = 0;

    /* Find next focusable edit-text item */
    if (backward) {
        /* Search backward from current focus */
        for (i = currentFocus - 1; i >= 1; i--) {
            GetDialogItem(theDialog, i, &itemType, &itemHandle, &itemBox);
            if ((itemType & 0x7F) == editText) {
                nextFocus = i;
                break;
            }
        }
        /* Wrap around if no item found */
        if (nextFocus == 0) {
            for (i = itemCount; i > currentFocus; i--) {
                GetDialogItem(theDialog, i, &itemType, &itemHandle, &itemBox);
                if ((itemType & 0x7F) == editText) {
                    nextFocus = i;
                    break;
                }
            }
        }
    } else {
        /* Search forward from current focus */
        for (i = currentFocus + 1; i <= itemCount; i++) {
            GetDialogItem(theDialog, i, &itemType, &itemHandle, &itemBox);
            if ((itemType & 0x7F) == editText) {
                nextFocus = i;
                break;
            }
        }
        /* Wrap around if no item found */
        if (nextFocus == 0) {
            for (i = 1; i < currentFocus; i++) {
                GetDialogItem(theDialog, i, &itemType, &itemHandle, &itemBox);
                if ((itemType & 0x7F) == editText) {
                    nextFocus = i;
                    break;
                }
            }
        }
    }

    /* Set focus to next item if found */
    if (nextFocus > 0) {
        SetDialogEditTextFocus(theDialog, nextFocus);
    }
}

/*
 * ClearDialogEditTextFocus - Clear edit-text focus for a dialog
 *
 * Called when a dialog loses focus or is disposed.
 *
 * Parameters:
 *   theDialog - The dialog to clear focus from
 */
void ClearDialogEditTextFocus(DialogPtr theDialog) {
    SetDialogEditTextFocus(theDialog, 0);
}

/*
 * InitDialogEditTextFocus - Initialize edit-text focus for a dialog
 *
 * Sets focus to the first edit-text item in the dialog.
 *
 * Parameters:
 *   theDialog - The dialog to initialize
 */
void InitDialogEditTextFocus(DialogPtr theDialog) {
    SInt16 itemCount;
    SInt16 i;
    SInt16 itemType;
    Handle itemHandle;
    Rect itemBox;

    if (!theDialog) {
        return;
    }

    /* Find first edit-text item */
    itemCount = CountDITL(theDialog);
    for (i = 1; i <= itemCount; i++) {
        GetDialogItem(theDialog, i, &itemType, &itemHandle, &itemBox);
        if ((itemType & 0x7F) == editText) {
            SetDialogEditTextFocus(theDialog, i);
            return;
        }
    }
}

/* ============================================================================
 * TextEdit Integration
 * ============================================================================ */

/*
 * DialogEditText_ReleaseAll - free the edit fields belonging to one dialog
 *
 * Each dialog owns its TextEdit records, so release only that dialog's slots
 * when its window is disposed.
 */
void DialogEditText_ReleaseAll(DialogPtr owner)
{
    DialogManagerState* state = GetDialogManagerState();
    DialogManagerState_Extended* extended;
    DialogEditTextState** link;
    DialogEditTextState* entry;
    int i;
    if (!state || !owner) return;
    extended = GET_EXTENDED_DLG_STATE(state);
    link = &extended->dialogStates;
    while (*link && (*link)->owner != owner) link = &(*link)->next;
    entry = *link;
    if (!entry) return;
    for (i = 0; i < 256; i++)
        if (entry->teHandles[i]) TEDispose((TEHandle)entry->teHandles[i]);
    *link = entry->next;
    DisposePtr(entry);
}

/*
 * GetOrCreateDialogTEHandle - Get or create TextEdit handle for dialog item
 *
 * Returns: TEHandle for the specified dialog item, or NULL on error
 */
TEHandle GetOrCreateDialogTEHandle(DialogPtr theDialog, SInt16 itemNo) {
    DialogManagerState* state;
    DialogEditTextState* dialogState;
    SInt16 itemType;
    Handle itemHandle;
    Rect itemBox;
    unsigned char* itemText;
    TEHandle hTE;

    state = GetDialogManagerState();
    if (!state || !theDialog || itemNo < 1 || itemNo >= 256) {
        return NULL;
    }
    dialogState = FindDialogState(theDialog, true);
    if (!dialogState) return NULL;

    /* Check if TEHandle already exists for this item */
    if (dialogState->teHandles[itemNo] != NULL) {
        return (TEHandle)dialogState->teHandles[itemNo];
    }

    /* Get dialog item information */
    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &itemBox);

    /* Only create TEHandle for edit-text items */
    if ((itemType & 0x7F) != editText) {
        return NULL;
    }

    /* Create new TextEdit record, in the dialog's port: a record draws in
     * the port current when it is made, and this was whichever window was
     * current - the Finder's rename box drew its field a second time in
     * the Finder window underneath. */
    {
        GrafPtr savePort;
        GetPort(&savePort);
        SetPort((GrafPtr)theDialog);
        hTE = TENew(&itemBox, &itemBox);
        SetPort(savePort);
    }
    if (!hTE) {
        return NULL;
    }

    /* Set initial text from dialog item data */
    if (itemHandle) {
        HLock(itemHandle);
        itemText = (unsigned char*)*itemHandle;
        if (itemText && itemText[0] > 0) {
            /* Pascal string to C text */
            TESetText(&itemText[1], itemText[0], hTE);
            /* System 7 opens a field with its existing text selected, so the
             * first thing you type replaces the old name rather than being
             * inserted in front of it. */
            TESetSelect(0, itemText[0], hTE);
        }
        HUnlock(itemHandle);
    }

    /* Store TEHandle for future use */
    dialogState->teHandles[itemNo] = (void*)hTE;

    return hTE;
}

/*
 * HandleDialogEditTextClick - Handle mouse clicks in edit-text items
 *
 * Returns: true if click was handled, false otherwise
 */
Boolean HandleDialogEditTextClick(DialogPtr theDialog, SInt16 itemNo, Point mousePt) {
    TEHandle hTE;

    if (!theDialog || itemNo < 1) {
        return false;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (!hTE) {
        return false;
    }

    /* Make sure item has focus */
    SetDialogEditTextFocus(theDialog, itemNo);

    /* Pass click to TextEdit */
    TEClick(mousePt, false, hTE);

    return true;
}

static void StoreDialogTEText(DialogPtr theDialog, SInt16 itemNo, TEHandle hTE)
{
    Handle text = TEGetText(hTE);
    Handle itemHandle;
    SInt16 itemType;
    Rect itemBox;
    SInt32 textLength = (**hTE).teLength;
    Size itemHandleSize;

    if (!text) return;
    if (textLength < 0) textLength = 0;
    if (textLength > 255) textLength = 255;

    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &itemBox);
    if (!itemHandle) return;

    HLock(text);
    itemHandleSize = GetHandleSize(itemHandle);
    if (itemHandleSize < 0 || (UInt32)itemHandleSize < (UInt32)textLength + 1u) {
        SetHandleSize(itemHandle, textLength + 1);
    }
    itemHandleSize = GetHandleSize(itemHandle);
    if (itemHandleSize >= 0 && (UInt32)itemHandleSize >= (UInt32)textLength + 1u) {
        HLock(itemHandle);
        unsigned char* itemText = (unsigned char*)*itemHandle;
        itemText[0] = (unsigned char)textLength;
        if (textLength > 0) memcpy(&itemText[1], *text, (size_t)textLength);
        HUnlock(itemHandle);
        DialogItem_SyncText(theDialog, itemNo);
    }
    HUnlock(text);

    InvalDialogItem(theDialog, itemNo);
    DrawDialogItem(theDialog, itemNo);
}

/*
 * HandleDialogEditTextKey - Handle keyboard events in edit-text items
 *
 * Returns: true if key was handled, false otherwise
 */
Boolean HandleDialogEditTextKey(DialogPtr theDialog, SInt16 itemNo, CharParameter key) {
    TEHandle hTE;

    if (!theDialog || itemNo < 1) {
        return false;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (!hTE) {
        return false;
    }

    /* Tab, Return and Escape belong to the dialog, not the field. Backspace
     * does not - TEKey deletes the selection or the character before the
     * insertion point, and refusing it here made the field uneditable once
     * you had typed something wrong. */
    switch (key) {
        case '\t':          /* Tab - moves focus between fields */
        case 0x0D:          /* Return - default button */
        case 0x03:          /* Enter - default button */
        case 0x1B:          /* Escape - cancel */
            return false;
        default:
            break;
    }

    /* Pass key to TextEdit */
    TEKey(key, hTE);

    StoreDialogTEText(theDialog, itemNo, hTE);

    return true;
}

/*
 * UpdateDialogTEDisplay - Update TextEdit display for dialog item
 *
 * Called when dialog item needs to be redrawn
 */
void UpdateDialogTEDisplay(DialogPtr theDialog, SInt16 itemNo) {
    TEHandle hTE;

    if (!theDialog || itemNo < 1) {
        return;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (!hTE) {
        return;
    }

    /* Activate and update TextEdit */
    TEActivate(hTE);
    TEUpdate(NULL, hTE);
    TEDeactivate(hTE);
}

/*
 * HandleDialogCut - Handle cut operation in focused edit-text item
 */
void HandleDialogCut(DialogPtr theDialog) {
    TEHandle hTE;
    SInt16 itemNo;
    if (!theDialog) return;
    itemNo = GetDialogEditTextFocus(theDialog);
    if (itemNo < 1) {
        return;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (hTE) {
        TECut(hTE);
        StoreDialogTEText(theDialog, itemNo, hTE);
    }
}

/*
 * HandleDialogCopy - Handle copy operation in focused edit-text item
 */
void HandleDialogCopy(DialogPtr theDialog) {
    TEHandle hTE;
    SInt16 itemNo;
    if (!theDialog) return;
    itemNo = GetDialogEditTextFocus(theDialog);
    if (itemNo < 1) {
        return;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (hTE) {
        TECopy(hTE);
    }
}

/*
 * HandleDialogPaste - Handle paste operation in focused edit-text item
 */
void HandleDialogPaste(DialogPtr theDialog) {
    TEHandle hTE;
    SInt16 itemNo;
    if (!theDialog) return;
    itemNo = GetDialogEditTextFocus(theDialog);
    if (itemNo < 1) {
        return;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (hTE) {
        TEPaste(hTE);
        StoreDialogTEText(theDialog, itemNo, hTE);
    }
}

void DialogCut(DialogPtr theDialog) { HandleDialogCut(theDialog); }
void DialogCopy(DialogPtr theDialog) { HandleDialogCopy(theDialog); }
void DialogPaste(DialogPtr theDialog) { HandleDialogPaste(theDialog); }

void DialogDelete(DialogPtr theDialog)
{
    if (!theDialog) return;
    SInt16 itemNo = GetDialogEditTextFocus(theDialog);
    if (itemNo < 1) return;
    TEHandle hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (!hTE) return;

    TEDelete(hTE);
    StoreDialogTEText(theDialog, itemNo, hTE);
}

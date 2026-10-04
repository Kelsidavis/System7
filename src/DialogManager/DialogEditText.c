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

/*
 * SetDialogEditTextFocus - Set keyboard focus to an edit-text item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The edit-text item number to focus (0 to clear focus)
 */
void SetDialogEditTextFocus(DialogPtr theDialog, SInt16 itemNo) {
    DialogManagerState* state;
    SInt16 oldFocusItem;

    state = GetDialogManagerState();

    if (!state || !theDialog) {
        return;
    }

    oldFocusItem = state->focusedEditTextItem;

    /* Clear old focus */
    if (oldFocusItem > 0 && oldFocusItem != itemNo) {
        state->focusedEditTextItem = 0;
        state->caretVisible = false;
        InvalDialogItem(theDialog, oldFocusItem);
        DrawDialogItem(theDialog, oldFocusItem);
    }

    /* Set new focus */
    if (itemNo > 0) {
        state->focusedEditTextItem = itemNo;
        state->caretBlinkTime = TickCount();
        state->caretVisible = true;

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
        state->focusedEditTextItem = 0;
        state->caretVisible = false;
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
    DialogManagerState* state;

    state = GetDialogManagerState();

    if (!state || !theDialog) {
        return 0;
    }

    return state->focusedEditTextItem;
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
    DialogManagerState* state;

    state = GetDialogManagerState();
    UInt32 currentTicks;
    UInt32 elapsed;

    if (!state || !theDialog || state->focusedEditTextItem == 0) {
        return;
    }

    currentTicks = TickCount();

    /* Handle tick counter wrap-around */
    if (currentTicks < state->caretBlinkTime) {
        state->caretBlinkTime = currentTicks;
        return;
    }

    elapsed = currentTicks - state->caretBlinkTime;

    /* Toggle caret visibility every kCaretBlinkRate ticks */
    if (elapsed >= kCaretBlinkRate) {
        state->caretVisible = !state->caretVisible;
        state->caretBlinkTime = currentTicks;

        /* Redraw the focused edit-text item */
        InvalDialogItem(theDialog, state->focusedEditTextItem);
        DrawDialogItem(theDialog, state->focusedEditTextItem);

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
    DialogManagerState* state;

    state = GetDialogManagerState();
    SInt16 itemCount;
    SInt16 currentFocus;
    SInt16 nextFocus;
    SInt16 i;
    SInt16 itemType;
    Handle itemHandle;
    Rect itemBox;

    if (!state || !theDialog) {
        return;
    }

    itemCount = CountDITL(theDialog);
    currentFocus = state->focusedEditTextItem;
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
 * The handles hang off a single global array, so they have to be let go when
 * their dialog does; otherwise the next window to redraw finds a live-looking
 * TextEdit record for an item number it shares and draws it.
 */
void DialogEditText_ReleaseAll(DialogPtr owner)
{
    DialogManagerState* state = GetDialogManagerState();
    DialogManagerState_Extended* extState = GET_EXTENDED_DLG_STATE(state);
    int i;

    if (!extState) return;
    /* Only the owner may clear them; a stale pointer must not free another
     * dialog's fields. */
    if (owner && extState->teOwner && extState->teOwner != owner) return;

    for (i = 0; i < 256; i++) {
        if (extState->teHandles[i]) {
            TEDispose((TEHandle)extState->teHandles[i]);
            extState->teHandles[i] = NULL;
        }
    }
    extState->teOwner = NULL;
    state->focusedEditTextItem = 0;
}

/*
 * GetOrCreateDialogTEHandle - Get or create TextEdit handle for dialog item
 *
 * Returns: TEHandle for the specified dialog item, or NULL on error
 */
TEHandle GetOrCreateDialogTEHandle(DialogPtr theDialog, SInt16 itemNo) {
    DialogManagerState* state;
    DialogManagerState_Extended* extState;
    SInt16 itemType;
    Handle itemHandle;
    Rect itemBox;
    unsigned char* itemText;
    TEHandle hTE;

    state = GetDialogManagerState();
    extState = GET_EXTENDED_DLG_STATE(state);

    if (!state || !theDialog || itemNo < 1 || itemNo >= 256) {
        return NULL;
    }

    /* A different dialog's fields are still here; they are not ours to reuse
     * and nobody else will free them. */
    if (extState->teOwner && extState->teOwner != theDialog) {
        DialogEditText_ReleaseAll(extState->teOwner);
    }

    /* Check if TEHandle already exists for this item */
    if (extState->teHandles[itemNo] != NULL) {
        return (TEHandle)extState->teHandles[itemNo];
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
    extState->teHandles[itemNo] = (void*)hTE;
    extState->teOwner = theDialog;

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

    /* Store updated text back to dialog item */
    {
        Handle hText;
        Handle itemHandle;
        SInt16 itemType;
        Rect itemBox;
        SInt32 textLen;
        Size itemHandleSize;
        unsigned char* pText;

        hText = TEGetText(hTE);
        if (hText) {
            GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &itemBox);
            HLock(hText);
            /* teLength, not the handle size: TENew allocates a fixed buffer
             * and TextEdit tracks how much of it is in use, so GetHandleSize
             * reports the capacity. Using it here copied the whole buffer -
             * the typed characters plus a kilobyte of uninitialised memory -
             * and set the item's Pascal length to 255. */
            textLen = (SInt32)(**hTE).teLength;
            if (textLen < 0) textLen = 0;
            if (textLen > 255) textLen = 255;

            /* Convert to Pascal string. The item handle was sized for the
             * text the list was built with, so it has to grow before longer
             * text is copied in - otherwise typing past the initial length
             * writes off the end of the block. */
            if (itemHandle) {
                itemHandleSize = GetHandleSize(itemHandle);
                if (itemHandleSize < 0 ||
                    (UInt32)itemHandleSize < (UInt32)textLen + 1u) {
                    SetHandleSize(itemHandle, textLen + 1);
                }
                itemHandleSize = GetHandleSize(itemHandle);
                if (itemHandleSize >= 0 &&
                    (UInt32)itemHandleSize >= (UInt32)textLen + 1u) {
                    HLock(itemHandle);
                    pText = (unsigned char*)*itemHandle;
                    pText[0] = (unsigned char)textLen;
                    if (textLen > 0) {
                        memcpy(&pText[1], *hText, textLen);
                    }
                    HUnlock(itemHandle);
                }
            }
            HUnlock(hText);

            /* The handle may have moved when it grew, so refresh the cached
             * pointer the drawing code reads before asking for a redraw. */
            DialogItem_SyncText(theDialog, itemNo);

            /* Redraw item */
            InvalDialogItem(theDialog, itemNo);
            DrawDialogItem(theDialog, itemNo);
        }
    }

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
    DialogManagerState* state;
    TEHandle hTE;
    SInt16 itemNo;

    state = GetDialogManagerState();

    if (!state || !theDialog) {
        return;
    }

    itemNo = state->focusedEditTextItem;
    if (itemNo < 1) {
        return;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (hTE) {
        TECut(hTE);
    }
}

/*
 * HandleDialogCopy - Handle copy operation in focused edit-text item
 */
void HandleDialogCopy(DialogPtr theDialog) {
    DialogManagerState* state;
    TEHandle hTE;
    SInt16 itemNo;

    state = GetDialogManagerState();

    if (!state || !theDialog) {
        return;
    }

    itemNo = state->focusedEditTextItem;
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
    DialogManagerState* state;
    TEHandle hTE;
    SInt16 itemNo;

    state = GetDialogManagerState();

    if (!state || !theDialog) {
        return;
    }

    itemNo = state->focusedEditTextItem;
    if (itemNo < 1) {
        return;
    }

    hTE = GetOrCreateDialogTEHandle(theDialog, itemNo);
    if (hTE) {
        TEPaste(hTE);
        /* Update dialog item after paste */
        InvalDialogItem(theDialog, itemNo);
        DrawDialogItem(theDialog, itemNo);
    }
}

/*
 * DialogHelpers.c - Dialog Manager Helper Functions
 *
 * Provides utility functions for hit testing, item tracking, coordinate conversion,
 * and item type checking used by both modal and modeless dialog handling.
 */

#include "DialogManager/DialogInternal.h"
#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogManagerInternal.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/DialogLogging.h"

/* DialogGlobals and DialogManagerState are now defined in DialogManagerInternal.h */

/* External dependencies */
extern void InvertRect(const Rect* r);
extern void FrameRect(const Rect* r);
extern void PenSize(SInt16 width, SInt16 height);
extern void PenNormal(void);
extern Boolean StillDown(void);
extern void GetMouse(Point* mouseLoc);
extern UInt32 TickCount(void);

/* Global to track which edit field has focus */
static DialogPtr gFocusDialog = NULL;
static SInt16 gFocusItemNo = 0;

/* Helper: Check if point is in rect (renamed to avoid QuickDraw conflict) */
static Boolean DlgPtInRect(Point pt, const Rect* r) {
    return (pt.h >= r->left && pt.h < r->right &&
            pt.v >= r->top && pt.v < r->bottom);
}

/* Convert global point to dialog-local coordinates */
void GlobalToLocalDialog(DialogPtr theDialog, Point* pt) {
    if (!theDialog || !pt) return;

    /* Dialog coordinates are same as window content coordinates */
    /* In our simplified model, global = local for now */
    /* In full implementation, would subtract window origin */

    /* For now, just log the conversion */
    // DIALOG_LOG_DEBUG("Dialog: GlobalToLocal (%d,%d)\n", pt->h, pt->v);
}

/* Hit test dialog - returns item number or 0 */
SInt16 DialogHitTest(DialogPtr theDialog, Point localPt) {
    SInt16 itemNo;

    if (!theDialog) {
        return 0;
    }

    /* Use FindDialogItem which already does hit testing */
    itemNo = FindDialogItem(theDialog, localPt);

    // DIALOG_LOG_DEBUG("Dialog: HitTest at (%d,%d) -> item %d\n", localPt.h, localPt.v, itemNo);

    return itemNo;
}

/* Check if item is a push button */
Boolean DialogItemIsPushButton(DialogPtr theDialog, SInt16 itemNo) {
    SInt16 itemType;
    Handle itemHandle;
    Rect box;

    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &box);

    itemType &= itemTypeMask;
    return (itemType == (ctrlItem + btnCtrl));
}

/* Check if item is a checkbox */
Boolean DialogItemIsCheckbox(DialogPtr theDialog, SInt16 itemNo) {
    SInt16 itemType;
    Handle itemHandle;
    Rect box;

    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &box);

    itemType &= itemTypeMask;
    return (itemType == (ctrlItem + chkCtrl));
}

/* Check if item is a radio button */
Boolean DialogItemIsRadio(DialogPtr theDialog, SInt16 itemNo) {
    SInt16 itemType;
    Handle itemHandle;
    Rect box;

    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &box);

    itemType &= itemTypeMask;
    return (itemType == (ctrlItem + radCtrl));
}

/* Check if item is edit text */
Boolean DialogItemIsEditText(DialogPtr theDialog, SInt16 itemNo) {
    SInt16 itemType;
    Handle itemHandle;
    Rect box;

    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &box);

    itemType &= itemTypeMask;
    return (itemType == editText);
}

/* Track button press with visual feedback */
void DialogTrackButton(DialogPtr theDialog, SInt16 itemNo, Point startPt,
                      Boolean autoHilite) {
    Rect itemBounds;
    SInt16 itemType;
    Handle itemHandle;
    Boolean inside = true;
    Boolean wasInside = true;
    Point pt;

    if (!theDialog) return;

    GetDialogItem(theDialog, itemNo, &itemType, &itemHandle, &itemBounds);

    // DIALOG_LOG_DEBUG("Dialog: Tracking button item %d\n", itemNo);

    /* Highlight button on press */
    if (autoHilite) {
        InvertRect(&itemBounds);
    }

    /* Track mouse until release */
    while (StillDown()) {
        GetMouse(&pt);
        GlobalToLocalDialog(theDialog, &pt);

        inside = PtInRect(pt, &itemBounds);

        /* Toggle highlight when mouse moves in/out */
        if (inside != wasInside && autoHilite) {
            InvertRect(&itemBounds);
            wasInside = inside;
        }
    }

    /* If mouse released outside, unhighlight */
    if (!inside && autoHilite) {
        InvertRect(&itemBounds);
    }

}

/* Toggle checkbox state */
void ToggleDialogCheckbox(DialogPtr theDialog, SInt16 itemNo) {
    /* Get current item to access refCon */
    extern DialogItemEx* GetDialogItemEx(DialogPtr theDialog, SInt16 itemNo);
    /* For now, use a simple approach via the cache */

    // DIALOG_LOG_DEBUG("Dialog: Toggle checkbox %d\n", itemNo);

    /* The actual state is stored in the item's refCon */
    /* We'll need to access the DialogItemEx to toggle it */
    /* For now, just redraw */
}

/* Select radio button and deselect others in group */
void SelectRadioInGroup(DialogPtr theDialog, SInt16 itemNo) {
    SInt16 itemCount, i;
    SInt16 thisType, otherType;
    Handle h1, h2;
    Rect r1, r2;

    if (!theDialog) return;

    GetDialogItem(theDialog, itemNo, &thisType, &h1, &r1);
    thisType &= itemTypeMask;

    if (thisType != (ctrlItem + radCtrl)) {
        return;
    }

    // DIALOG_LOG_DEBUG("Dialog: Select radio %d in group\n", itemNo);

    /* Find all radio buttons and deselect them */
    /* In full implementation, would check group ID */
    itemCount = CountDITL(theDialog);

    for (i = 1; i <= itemCount; i++) {
        GetDialogItem(theDialog, i, &otherType, &h2, &r2);
        otherType &= itemTypeMask;

        if (otherType == (ctrlItem + radCtrl)) {
            /* Deselect this radio (set refCon = 0) */
            /* Then select the clicked one (set refCon = 1) */
            /* For now, just invalidate all radios to redraw */
            InvalDialogItem(theDialog, i);
        }
    }
}

/* Set edit field focus */
void SetDialogEditFocus(DialogPtr theDialog, SInt16 itemNo) {
    /* Clear previous focus */
    if (gFocusDialog && gFocusItemNo) {
        InvalDialogItem(gFocusDialog, gFocusItemNo);
    }

    gFocusDialog = theDialog;
    gFocusItemNo = itemNo;

    // DIALOG_LOG_DEBUG("Dialog: Set edit focus to item %d\n", itemNo);
}

/* Check if dialog has edit focus */
Boolean HasEditFocus(DialogPtr theDialog) {
    return (gFocusDialog == theDialog && gFocusItemNo != 0);
}

/* Get current focused edit item */
SInt16 GetEditFocusItem(DialogPtr theDialog) {
    if (gFocusDialog == theDialog) {
        return gFocusItemNo;
    }
    return 0;
}

/* Handle key in edit field (simple implementation) */
Boolean DialogEditKey(DialogPtr theDialog, char ch) {
    if (!HasEditFocus(theDialog)) {
        return false;
    }

    /* For now, just log key presses */
    /* Full implementation would modify the text data */
    // DIALOG_LOG_DEBUG("Dialog: Edit key '%c' in item %d\n", ch, gFocusItemNo);

    return true;
}

/* Get front dialog (placeholder - should check window list) */
DialogPtr FrontDialog(void) {
    /* In full implementation, walk window list and find first dialog */
    /* For now, return the front modal dialog from state */
    extern DialogManagerState* GetDialogManagerState(void);
    DialogManagerState* state = GetDialogManagerState();

    if (state) {
        /* Check front modal dialog first (set by BeginModalDialog) */
        if (state->globals.frontModal) {
            return state->globals.frontModal;
        }
        /* Fall back to currentDialog for modeless */
        if (state->currentDialog) {
            return state->currentDialog;
        }
    }

    return NULL;
}

/* Check if front window is a dialog */
Boolean FrontWindowIsDialog(void) {
    return (FrontDialog() != NULL);
}

/* Center dialog on screen */
void CenterDialogOnScreen(DialogPtr theDialog) {
    Rect dialogBounds, screenBounds;
    SInt16 dialogWidth, dialogHeight;
    SInt16 newLeft, newTop;

    if (!theDialog) return;

    /* Get screen bounds */
    extern void Platform_GetScreenBounds(Rect* bounds);
    Platform_GetScreenBounds(&screenBounds);

    /* Account for menu bar at top (20 pixels) */
    screenBounds.top += 20;

    /* Get dialog bounds from window */
    WindowPtr window = (WindowPtr)theDialog;
    dialogBounds = window->port.portRect;

    dialogWidth = dialogBounds.right - dialogBounds.left;
    dialogHeight = dialogBounds.bottom - dialogBounds.top;

    /* Calculate centered position - use 1/3 from top for better visual balance */
    SInt16 screenWidth = screenBounds.right - screenBounds.left;
    SInt16 screenHeight = screenBounds.bottom - screenBounds.top;

    newLeft = screenBounds.left + (screenWidth - dialogWidth) / 2;
    newTop = screenBounds.top + (screenHeight - dialogHeight) / 3;

    /* Ensure dialog stays on screen */
    if (newLeft < screenBounds.left) newLeft = screenBounds.left;
    if (newTop < screenBounds.top) newTop = screenBounds.top;
    if (newLeft + dialogWidth > screenBounds.right) {
        newLeft = screenBounds.right - dialogWidth;
    }
    if (newTop + dialogHeight > screenBounds.bottom) {
        newTop = screenBounds.bottom - dialogHeight;
    }

    /* Move the dialog window */
    extern void MoveWindow(WindowPtr window, short hGlobal, short vGlobal, Boolean front);
    MoveWindow(window, newLeft, newTop, false);
}

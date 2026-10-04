/*
 * DialogEditText.h - Dialog Edit Text Focus and Caret Management API
 *
 * This header defines the edit-text focus tracking and caret blinking
 * functionality for System 7.1 dialogs.
 */

#ifndef DIALOG_EDITTEXT_H
#define DIALOG_EDITTEXT_H

#include "SystemTypes.h"
#include "DialogManager.h"
#include "TextEdit/TextEdit.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SetDialogEditTextFocus - Set keyboard focus to an edit-text item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The edit-text item number to focus (0 to clear focus)
 */
void SetDialogEditTextFocus(DialogPtr theDialog, SInt16 itemNo);

/*
 * GetDialogEditTextFocus - Get the currently focused edit-text item
 *
 * Parameters:
 *   theDialog - The dialog to query
 *
 * Returns:
 *   The item number of the focused edit-text item, or 0 if none
 */
SInt16 GetDialogEditTextFocus(DialogPtr theDialog);

/*
 * UpdateDialogCaret - Update caret blink state
 *
 * Should be called from the dialog event loop to maintain caret blinking.
 *
 * Parameters:
 *   theDialog - The dialog to update
 */
void UpdateDialogCaret(DialogPtr theDialog);

/*
 * AdvanceDialogEditTextFocus - Move focus to next/previous edit-text item
 *
 * Implements Tab/Shift-Tab navigation between edit-text fields.
 *
 * Parameters:
 *   theDialog - The dialog containing the items
 *   backward  - true to move backward (Shift-Tab), false for forward (Tab)
 */
void AdvanceDialogEditTextFocus(DialogPtr theDialog, Boolean backward);

/*
 * ClearDialogEditTextFocus - Clear edit-text focus for a dialog
 *
 * Called when a dialog loses focus or is disposed.
 *
 * Parameters:
 *   theDialog - The dialog to clear focus from
 */
void ClearDialogEditTextFocus(DialogPtr theDialog);

/*
 * InitDialogEditTextFocus - Initialize edit-text focus for a dialog
 *
 * Sets focus to the first edit-text item in the dialog.
 *
 * Parameters:
 *   theDialog - The dialog to initialize
 */
void InitDialogEditTextFocus(DialogPtr theDialog);

/* ============================================================================
 * TextEdit Integration Functions
 * ============================================================================ */

/*
 * GetOrCreateDialogTEHandle - Get or create TextEdit handle for dialog item
 *
 * Returns: TEHandle for the specified dialog item, or NULL on error
 */
extern TEHandle GetOrCreateDialogTEHandle(DialogPtr theDialog, SInt16 itemNo);
extern TEHandle DialogEditText_GetHandle(DialogPtr theDialog, SInt16 itemNo);
extern Boolean DialogEditText_CaretVisible(DialogPtr theDialog);
extern void DialogEditText_SetCaretActive(DialogPtr theDialog, Boolean active);

/*
 * HandleDialogEditTextClick - Handle mouse clicks in edit-text items
 *
 * Returns: true if click was handled, false otherwise
 */
extern Boolean HandleDialogEditTextClick(DialogPtr theDialog, SInt16 itemNo, Point mousePt);

/*
 * HandleDialogEditTextKey - Handle keyboard events in edit-text items
 *
 * Returns: true if key was handled, false otherwise
 */
extern Boolean HandleDialogEditTextKey(DialogPtr theDialog, SInt16 itemNo, CharParameter key);

/*
 * UpdateDialogTEDisplay - Update TextEdit display for dialog item
 *
 * Called when dialog item needs to be redrawn
 */
extern void UpdateDialogTEDisplay(DialogPtr theDialog, SInt16 itemNo);

/*
 * HandleDialogCut - Handle cut operation in focused edit-text item
 */
extern void HandleDialogCut(DialogPtr theDialog);

/*
 * HandleDialogCopy - Handle copy operation in focused edit-text item
 */
extern void HandleDialogCopy(DialogPtr theDialog);

/*
 * HandleDialogPaste - Handle paste operation in focused edit-text item
 */
extern void HandleDialogPaste(DialogPtr theDialog);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_EDITTEXT_H */

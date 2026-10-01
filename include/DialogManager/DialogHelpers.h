/*
 * DialogHelpers.h - Dialog Manager Helper Functions
 */

#ifndef DIALOG_HELPERS_H
#define DIALOG_HELPERS_H

#include "SystemTypes.h"
#include "DialogManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Coordinate conversion */
void GlobalToLocalDialog(DialogPtr theDialog, Point* pt);

/* Hit testing */
SInt16 DialogHitTest(DialogPtr theDialog, Point localPt);

/* Item type checking */
Boolean DialogItemIsPushButton(DialogPtr theDialog, SInt16 itemNo);
Boolean DialogItemIsCheckbox(DialogPtr theDialog, SInt16 itemNo);
Boolean DialogItemIsRadio(DialogPtr theDialog, SInt16 itemNo);
Boolean DialogItemIsEditText(DialogPtr theDialog, SInt16 itemNo);

/* Item interaction */
Boolean DialogTrackButton(DialogPtr theDialog, SInt16 itemNo, Point startPt,
                          Boolean autoHilite);   /* true if released inside */
/* A checkbox or radio button item's on/off state (kept in its refCon) */
SInt32 DM_GetItemState(DialogPtr theDialog, SInt16 itemNo);
void DM_SetItemState(DialogPtr theDialog, SInt16 itemNo, SInt32 value);
void ToggleDialogCheckbox(DialogPtr theDialog, SInt16 itemNo);
void SelectRadioInGroup(DialogPtr theDialog, SInt16 itemNo);

/* Edit field focus management */
void SetDialogEditFocus(DialogPtr theDialog, SInt16 itemNo);
Boolean HasEditFocus(DialogPtr theDialog);
SInt16 GetEditFocusItem(DialogPtr theDialog);
Boolean DialogEditKey(DialogPtr theDialog, char ch);

/* Dialog window management */
DialogPtr FrontDialog(void);
Boolean FrontWindowIsDialog(void);
void CenterDialogOnScreen(DialogPtr theDialog);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_HELPERS_H */

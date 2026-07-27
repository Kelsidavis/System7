/*
 * DialogDrawing.h - Dialog Item Drawing Interface
 */

#ifndef DIALOG_DRAWING_H
#define DIALOG_DRAWING_H

#include "SystemTypes.h"
#include "DialogManager.h"
#include "DialogManagerInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Individual item type drawing functions */
void DrawDialogButton(DialogPtr theDialog, const Rect* bounds, const unsigned char* title,
                     Boolean isDefault, Boolean isEnabled, Boolean isPressed);

void DrawDialogCheckBox(const Rect* bounds, const unsigned char* title,
                       Boolean isChecked, Boolean isEnabled);

void DrawDialogRadioButton(const Rect* bounds, const unsigned char* title,
                          Boolean isSelected, Boolean isEnabled);

void DrawDialogStaticText(DialogPtr theDialog, const Rect* bounds, const unsigned char* text,
                         Boolean isEnabled);

void DrawDialogEditText(const Rect* bounds, const unsigned char* text,
                        Boolean isEnabled, Boolean hasFocus, SInt16 itemNo);

void DrawDialogIcon(const Rect* bounds, SInt16 iconID, Boolean isEnabled);

void DrawDialogUserItem(DialogPtr theDialog, SInt16 itemNo, const Rect* bounds,
                       UserItemProcPtr userProc);

/* Main drawing dispatcher */
void DrawDialogItemByType(DialogPtr theDialog, SInt16 itemNo,
                         const DialogItemEx* item);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_DRAWING_H */

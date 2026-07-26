/*
 * DialogItems.h - Dialog Item Management API
 *
 * This header defines the dialog item management functionality,
 * maintaining exact Mac System 7.1 behavioral compatibility.
 */

#ifndef DIALOG_ITEMS_H
#define DIALOG_ITEMS_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

#include "DialogTypes.h"
#include "DialogManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dialog item management functions */

/*
 * GetDialogItem - Get information about a dialog item
 *
 * This function retrieves information about a specific dialog item,
 * including its type, handle, and bounds rectangle.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number (1-based)
 *   itemType  - Returns the item type
 *   item      - Returns the item handle
 *   box       - Returns the item bounds rectangle
 */
void GetDialogItem(DialogPtr theDialog, SInt16 itemNo, SInt16* itemType,
                   Handle* item, Rect* box);

/*
 * SetDialogItem - Set information about a dialog item
 *
 * This function modifies a dialog item's type, handle, and bounds.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number (1-based)
 *   itemType  - The new item type
 *   item      - The new item handle
 *   box       - The new item bounds rectangle
 */
void SetDialogItem(DialogPtr theDialog, SInt16 itemNo, SInt16 itemType,
                   Handle item, const Rect* box);

/*
 * HideDialogItem - Hide a dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to hide
 */
void HideDialogItem(DialogPtr theDialog, SInt16 itemNo);

/*
 * ShowDialogItem - Show a hidden dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to show
 */
void ShowDialogItem(DialogPtr theDialog, SInt16 itemNo);

/*
 * FindDialogItem - Find dialog item at a point
 *
 * This function determines which dialog item contains a given point.
 *
 * Parameters:
 *   theDialog - The dialog to search
 *   thePt     - The point to test (in local coordinates)
 *
 * Returns:
 *   The item number (1-based) or 0 if no item found
 */
SInt16 FindDialogItem(DialogPtr theDialog, Point thePt);

/* Dialog item text management */

/*
 * GetDialogItemText - Get text from a dialog item
 *
 * This function retrieves the text content of a text item.
 *
 * Parameters:
 *   item - Handle to the item (usually a text edit record)
 *   text - Buffer to receive the text (Pascal string)
 */
void GetDialogItemText(Handle item, unsigned char* text);

/*
 * SetDialogItemText - Set text for a dialog item
 *
 * This function sets the text content of a text item.
 *
 * Parameters:
 *   item - Handle to the item
 *   text - The text to set (Pascal string)
 */
void SetDialogItemText(Handle item, const unsigned char* text);

/*
 * SelectDialogItemText - Select text in a dialog item
 *
 * This function selects a range of text in an editable text item.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *   strtSel   - Start of selection
 *   endSel    - End of selection
 */
void SelectDialogItemText(DialogPtr theDialog, SInt16 itemNo, SInt16 strtSel, SInt16 endSel);

/* Dialog item state management */

/*
 * EnableDialogItem - Enable a dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to enable
 */
void EnableDialogItem(DialogPtr theDialog, SInt16 itemNo);

/*
 * DisableDialogItem - Disable a dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to disable
 */
void DisableDialogItem(DialogPtr theDialog, SInt16 itemNo);

/*
 * IsDialogItemEnabled - Check if dialog item is enabled
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to check
 *
 * Returns:
 *   true if the item is enabled
 */
Boolean IsDialogItemEnabled(DialogPtr theDialog, SInt16 itemNo);

/*
 * IsDialogItemVisible - Check if dialog item is visible
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to check
 *
 * Returns:
 *   true if the item is visible
 */
Boolean IsDialogItemVisible(DialogPtr theDialog, SInt16 itemNo);

/* Dialog item list (DITL) manipulation */

/*
 * AppendDITL - Append items to dialog
 *
 * This function appends additional dialog items to an existing dialog.
 *
 * Parameters:
 *   theDialog - The dialog to modify
 *   theHandle - Handle to the DITL resource to append
 *   method    - How to position the new items
 */
void AppendDITL(DialogPtr theDialog, Handle theHandle, DITLMethod method);

/*
 * CountDITL - Count items in dialog
 *
 * Parameters:
 *   theDialog - The dialog to count items in
 *
 * Returns:
 *   The number of items in the dialog
 */
SInt16 CountDITL(DialogPtr theDialog);

/*
 * ShortenDITL - Remove items from end of dialog
 *
 * This function removes items from the end of the dialog's item list.
 *
 * Parameters:
 *   theDialog     - The dialog to modify
 *   numberItems   - Number of items to remove
 */
void ShortenDITL(DialogPtr theDialog, SInt16 numberItems);

/* User item procedures */

/*
 * SetUserItemProc - Set procedure for user item
 *
 * This function sets a procedure to handle drawing and interaction
 * for a user-defined dialog item.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The user item number
 *   procPtr   - Procedure to handle the item
 */
void SetUserItemProc(DialogPtr theDialog, SInt16 itemNo, UserItemProcPtr procPtr);

/*
 * GetUserItemProc - Get procedure for user item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The user item number
 *
 * Returns:
 *   The procedure handling the item, or NULL
 */
UserItemProcPtr GetUserItemProc(DialogPtr theDialog, SInt16 itemNo);

/* Control item management */

/*
 * GetDialogItemControl - Get control handle from dialog item
 *
 * This function returns the control handle for a control item.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The control item number
 *
 * Returns:
 *   Handle to the control, or NULL if not a control item
 */
Handle GetDialogItemControl(DialogPtr theDialog, SInt16 itemNo);

/*
 * SetDialogItemControl - Set control for dialog item
 *
 * Parameters:
 *   theDialog   - The dialog containing the item
 *   itemNo      - The item number
 *   controlHdl  - Handle to the control
 */
void SetDialogItemControl(DialogPtr theDialog, SInt16 itemNo, Handle controlHdl);

/*
 * GetDialogItemValue - Get value of control item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The control item number
 *
 * Returns:
 *   The control's current value
 */
SInt16 GetDialogItemValue(DialogPtr theDialog, SInt16 itemNo);

/*
 * SetDialogItemValue - Set value of control item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The control item number
 *   value     - The new value to set
 */
void SetDialogItemValue(DialogPtr theDialog, SInt16 itemNo, SInt16 value);

/* Dialog item drawing */

/*
 * DrawDialogItem - Draw a specific dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to draw
 */
void DrawDialogItem(DialogPtr theDialog, SInt16 itemNo);

/*
 * InvalDialogItem - Invalidate dialog item for redrawing
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to invalidate
 */
void InvalDialogItem(DialogPtr theDialog, SInt16 itemNo);

/*
 * FrameDialogItem - Draw frame around dialog item
 *
 * This function draws a frame around a dialog item, typically
 * used to highlight the default button.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to frame
 */
void FrameDialogItem(DialogPtr theDialog, SInt16 itemNo);

/* Advanced dialog item features */

/*
 * SetDialogItemRefCon - Set reference constant for item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *   refCon    - The reference constant to set
 */
void SetDialogItemRefCon(DialogPtr theDialog, SInt16 itemNo, SInt32 refCon);

/*
 * GetDialogItemRefCon - Get reference constant for item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *
 * Returns:
 *   The item's reference constant
 */
SInt32 GetDialogItemRefCon(DialogPtr theDialog, SInt16 itemNo);

/*
 * SetDialogItemUserData - Set user data for item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *   userData  - Pointer to user data
 */
void SetDialogItemUserData(DialogPtr theDialog, SInt16 itemNo, void* userData);

/*
 * GetDialogItemUserData - Get user data for item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *
 * Returns:
 *   Pointer to user data, or NULL
 */
void* GetDialogItemUserData(DialogPtr theDialog, SInt16 itemNo);

/* Dialog item accessibility */

/*
 * SetDialogItemAccessibilityText - Set accessibility text
 *
 * Parameters:
 *   theDialog       - The dialog containing the item
 *   itemNo          - The item number
 *   accessibilityText - Text for screen readers
 */
void SetDialogItemAccessibilityText(DialogPtr theDialog, SInt16 itemNo,
                                   const char* accessibilityText);

/*
 * GetDialogItemAccessibilityText - Get accessibility text
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *
 * Returns:
 *   Accessibility text or NULL
 */
const char* GetDialogItemAccessibilityText(DialogPtr theDialog, SInt16 itemNo);

/* Dialog item platform integration */

/*
 * CreatePlatformDialogItem - Create platform-native item
 *
 * This function creates a platform-native widget for a dialog item
 * when supported by the current platform.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *   itemType  - The type of item to create
 *
 * Returns:
 *   true if platform item was created successfully
 */
Boolean CreatePlatformDialogItem(DialogPtr theDialog, SInt16 itemNo, SInt16 itemType);

/*
 * DestroyPlatformDialogItem - Destroy platform-native item
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 */
void DestroyPlatformDialogItem(DialogPtr theDialog, SInt16 itemNo);

/* Internal dialog item functions */
void InitDialogItems(void);
void CleanupDialogItems(void);
OSErr ValidateDialogItemNumber(DialogPtr theDialog, SInt16 itemNo);
/* Forward declaration - actual type defined in DialogManagerStateExt.h */
struct DialogItemInternal;
typedef struct DialogItemInternal DialogItemInternal;
DialogItemInternal* GetDialogItemPtr(DialogPtr theDialog, SInt16 itemNo);
void UpdateDialogItemLayout(DialogPtr theDialog);
void RemoveDialogItemCache(DialogPtr theDialog);

/* Backwards compatibility aliases */
#define GetDItem        GetDialogItem
#define SetDItem        SetDialogItem
#define HideDItem       HideDialogItem
#define ShowDItem       ShowDialogItem
#define SelIText        SelectDialogItemText
#define GetIText        GetDialogItemText
#define SetIText        SetDialogItemText
#define FindDItem       FindDialogItem

#ifdef __cplusplus
}
#endif

/* Refresh an item's cached text pointer after its handle has been written. */
void DialogItem_SyncText(DialogPtr theDialog, SInt16 itemNo);

#endif /* DIALOG_ITEMS_H */

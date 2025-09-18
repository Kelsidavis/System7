/*
 * ModalDialogs.h - Modal Dialog Management API
 *
 * This header defines the modal dialog processing functionality,
 * maintaining exact Mac System 7.1 behavioral compatibility.
 */

#ifndef MODAL_DIALOGS_H
#define MODAL_DIALOGS_H

#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modal dialog processing functions */

/*
 * ModalDialog - Run modal dialog event loop
 *
 * This is the core modal dialog function that runs the event loop until
 * the user clicks a button or performs an action that ends the dialog.
 *
 * Parameters:
 *   filterProc - Optional filter procedure to handle special events
 *   itemHit    - Returns the item number that was clicked
 */
void ModalDialog(ModalFilterProcPtr filterProc, int16_t* itemHit);

/*
 * IsDialogEvent - Check if an event belongs to a dialog
 *
 * Parameters:
 *   theEvent - The event to check
 *
 * Returns:
 *   true if the event belongs to a dialog window
 */
bool IsDialogEvent(const EventRecord* theEvent);

/*
 * DialogSelect - Handle dialog event
 *
 * This function processes an event for dialog windows, handling
 * button clicks, text editing, and other dialog interactions.
 *
 * Parameters:
 *   theEvent   - The event to process
 *   theDialog  - Returns the dialog that handled the event
 *   itemHit    - Returns the item number that was hit
 *
 * Returns:
 *   true if the event was handled by a dialog
 */
bool DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog, int16_t* itemHit);

/* Modal dialog state management */

/*
 * BeginModalDialog - Begin modal dialog processing
 *
 * This function sets up the modal state and prepares for modal
 * dialog processing. Called automatically by ModalDialog.
 *
 * Parameters:
 *   theDialog - The dialog to make modal
 *
 * Returns:
 *   Error code or noErr
 */
OSErr BeginModalDialog(DialogPtr theDialog);

/*
 * EndModalDialog - End modal dialog processing
 *
 * This function cleans up the modal state and restores the
 * previous state. Called automatically when modal dialog ends.
 *
 * Parameters:
 *   theDialog - The dialog to end modal processing for
 */
void EndModalDialog(DialogPtr theDialog);

/*
 * IsModalDialog - Check if a dialog is modal
 *
 * Parameters:
 *   theDialog - The dialog to check
 *
 * Returns:
 *   true if the dialog is currently modal
 */
bool IsModalDialog(DialogPtr theDialog);

/*
 * GetFrontModalDialog - Get the front-most modal dialog
 *
 * Returns:
 *   Pointer to the front modal dialog, or NULL if none
 */
DialogPtr GetFrontModalDialog(void);

/* Modal dialog event filtering */

/*
 * StandardModalFilter - Standard modal dialog filter
 *
 * This implements the standard modal dialog behavior including:
 * - Return/Enter activates default button
 * - Escape/Cmd-. activates cancel button
 * - Tab navigation between items
 * - Command key handling
 *
 * Parameters:
 *   theDialog - The dialog receiving the event
 *   theEvent  - The event to filter
 *   itemHit   - Returns the item hit by filtering
 *
 * Returns:
 *   true if the event was handled and should not be processed further
 */
bool StandardModalFilter(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);

/*
 * InstallModalFilter - Install a modal filter procedure
 *
 * Parameters:
 *   theDialog  - The dialog to install filter for
 *   filterProc - The filter procedure to install
 *
 * Returns:
 *   The previous filter procedure
 */
ModalFilterProcPtr InstallModalFilter(DialogPtr theDialog, ModalFilterProcPtr filterProc);

/* Modal dialog utilities */

/*
 * FlashButton - Flash a button briefly
 *
 * This function briefly highlights a button to provide visual
 * feedback when it's activated.
 *
 * Parameters:
 *   theDialog - The dialog containing the button
 *   itemNo    - The button item number
 */
void FlashButton(DialogPtr theDialog, int16_t itemNo);

/*
 * HandleModalKeyboard - Handle keyboard input in modal dialog
 *
 * This function processes keyboard events for modal dialogs,
 * including default button activation and text editing.
 *
 * Parameters:
 *   theDialog - The dialog receiving the keyboard event
 *   theEvent  - The keyboard event
 *   itemHit   - Returns the item affected by the keyboard event
 *
 * Returns:
 *   true if the keyboard event was handled
 */
bool HandleModalKeyboard(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);

/*
 * HandleModalMouse - Handle mouse input in modal dialog
 *
 * This function processes mouse events for modal dialogs,
 * including button clicks and text field selection.
 *
 * Parameters:
 *   theDialog - The dialog receiving the mouse event
 *   theEvent  - The mouse event
 *   itemHit   - Returns the item hit by the mouse
 *
 * Returns:
 *   true if the mouse event was handled
 */
bool HandleModalMouse(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);

/* Modal dialog window management */

/*
 * BringModalToFront - Bring modal dialog to front
 *
 * This function brings a modal dialog to the front of all windows
 * and ensures it remains there during modal processing.
 *
 * Parameters:
 *   theDialog - The modal dialog to bring to front
 */
void BringModalToFront(DialogPtr theDialog);

/*
 * DisableNonModalWindows - Disable non-modal windows
 *
 * This function disables all non-modal windows while a modal
 * dialog is active, preventing user interaction with them.
 */
void DisableNonModalWindows(void);

/*
 * EnableNonModalWindows - Re-enable non-modal windows
 *
 * This function re-enables all windows that were disabled
 * when modal dialog processing began.
 */
void EnableNonModalWindows(void);

/* Modal dialog accessibility */

/*
 * AnnounceModalDialog - Announce modal dialog to screen reader
 *
 * This function announces the modal dialog to accessibility
 * technologies when it becomes active.
 *
 * Parameters:
 *   theDialog - The modal dialog to announce
 */
void AnnounceModalDialog(DialogPtr theDialog);

/*
 * SetModalDialogHelp - Set help text for modal dialog
 *
 * Parameters:
 *   theDialog - The dialog to set help for
 *   helpText  - The help text to display
 */
void SetModalDialogHelp(DialogPtr theDialog, const char* helpText);

/* Advanced modal dialog features */

/*
 * SetModalDialogTimeout - Set timeout for modal dialog
 *
 * This function sets a timeout for the modal dialog, after which
 * it will automatically close with a specific result.
 *
 * Parameters:
 *   theDialog    - The dialog to set timeout for
 *   timeoutTicks - Timeout in ticks (1/60 second)
 *   defaultItem  - Item to activate on timeout
 */
void SetModalDialogTimeout(DialogPtr theDialog, uint32_t timeoutTicks, int16_t defaultItem);

/*
 * ClearModalDialogTimeout - Clear modal dialog timeout
 *
 * Parameters:
 *   theDialog - The dialog to clear timeout for
 */
void ClearModalDialogTimeout(DialogPtr theDialog);

/*
 * SetModalDialogDismissButton - Set button that dismisses dialog
 *
 * This function designates a button that will dismiss the modal
 * dialog when clicked, ending the modal loop.
 *
 * Parameters:
 *   theDialog - The dialog
 *   itemNo    - The button item number (0 to clear)
 */
void SetModalDialogDismissButton(DialogPtr theDialog, int16_t itemNo);

/* Modal dialog platform integration */

/*
 * ShowNativeModal - Show platform-native modal dialog
 *
 * This function displays a native modal dialog using the platform's
 * standard dialog system, when available and appropriate.
 *
 * Parameters:
 *   message   - The message to display
 *   title     - The dialog title
 *   buttons   - Button configuration
 *   iconType  - Icon type (stop, note, caution)
 *
 * Returns:
 *   The button that was clicked
 */
int16_t ShowNativeModal(const char* message, const char* title,
                       const char* buttons, int16_t iconType);

/* Internal modal dialog functions */
void InitModalDialogs(void);
void CleanupModalDialogs(void);
OSErr PushModalDialog(DialogPtr theDialog);
OSErr PopModalDialog(DialogPtr theDialog);
bool ProcessModalEvent(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);
void UpdateModalDialogState(DialogPtr theDialog);

#ifdef __cplusplus
}
#endif

#endif /* MODAL_DIALOGS_H */
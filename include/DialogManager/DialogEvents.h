/*
 * DialogEvents.h - Dialog event API declarations
 *
 * This header declares the dialog event routines and helper interfaces.
 * Implementation coverage varies by routine.
 */

#ifndef DIALOG_EVENTS_H
#define DIALOG_EVENTS_H

#include "SystemTypes.h"

/* Forward declarations */


#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event types for dialog processing */

/* Special key codes for dialog navigation */

/* Modifier key flags */

/* Event handling result codes */

/* Core dialog event processing */

/*
 * ProcessDialogEvent - Process event for dialog
 *
 * This is the main event processing function for dialogs. It handles
 * mouse clicks, keyboard input, and other events according to Mac
 * System 7.1 dialog behavior.
 *
 * Parameters:
 *   theDialog - The dialog to process the event for
 *   theEvent  - The event to process
 *   itemHit   - Returns the item number if an item was hit
 *
 * Returns:
 *   Event processing result code
 */
SInt16 ProcessDialogEvent(DialogPtr theDialog, const EventRecord* theEvent,
                          SInt16* itemHit);

/*
 * HandleDialogMouseDown - Handle mouse down event in dialog
 *
 * This function processes mouse down events for dialogs, including
 * button clicks, text field selection, and control interaction.
 *
 * Parameters:
 *   theDialog - The dialog receiving the event
 *   theEvent  - The mouse down event
 *   itemHit   - Returns the item hit by the mouse
 *
 * Returns:
 *   true if the event was handled
 */
Boolean HandleDialogMouseDown(DialogPtr theDialog, const EventRecord* theEvent,
                          SInt16* itemHit);

/*
 * HandleDialogKeyDown - Handle key down event in dialog
 *
 * This function processes keyboard events for dialogs, including
 * default button activation, cancel handling, and text editing.
 *
 * Parameters:
 *   theDialog - The dialog receiving the event
 *   theEvent  - The key down event
 *   itemHit   - Returns the item affected by the key
 *
 * Returns:
 *   true if the event was handled
 */
Boolean HandleDialogKeyDown(DialogPtr theDialog, const EventRecord* theEvent,
                        SInt16* itemHit);

/*
 * HandleDialogUpdate - Handle update event for dialog
 *
 * This function processes update events for dialogs, redrawing
 * the dialog and its items as needed.
 *
 * Parameters:
 *   theDialog   - The dialog to update
 *   updateEvent - The update event (may be NULL for full update)
 */
void HandleDialogUpdate(DialogPtr theDialog, const EventRecord* updateEvent);

/*
 * HandleDialogActivate - Handle activate/deactivate event
 *
 * This function processes activate and deactivate events for dialogs,
 * updating the visual state and focus as appropriate.
 *
 * Parameters:
 *   theDialog - The dialog receiving the event
 *   theEvent  - The activate event
 *   activate  - true for activate, false for deactivate
 */
void HandleDialogActivate(DialogPtr theDialog, const EventRecord* theEvent,
                         Boolean activate);

/* Dialog navigation and focus */

/*
 * SetDialogFocus - Set focus to specific dialog item
 *
 * This function sets the keyboard focus to a specific dialog item,
 * updating the visual state and enabling keyboard input.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to focus (0 to remove focus)
 *
 * Returns:
 *   The previously focused item number
 */
SInt16 SetDialogFocus(DialogPtr theDialog, SInt16 itemNo);

/*
 * GetDialogFocus - Get currently focused dialog item
 *
 * Parameters:
 *   theDialog - The dialog to query
 *
 * Returns:
 *   The item number with focus, or 0 if no item has focus
 */
SInt16 GetDialogFocus(DialogPtr theDialog);

/*
 * AdvanceDialogFocus - Advance focus to next item
 *
 * This function advances the focus to the next focusable item in
 * the dialog, typically in response to Tab key presses.
 *
 * Parameters:
 *   theDialog - The dialog
 *   backward  - true to go backward (Shift+Tab), false for forward
 *
 * Returns:
 *   The new focused item number
 */
SInt16 AdvanceDialogFocus(DialogPtr theDialog, Boolean backward);

/*
 * IsDialogItemFocusable - Check if item can receive focus
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number to check
 *
 * Returns:
 *   true if the item can receive keyboard focus
 */
Boolean IsDialogItemFocusable(DialogPtr theDialog, SInt16 itemNo);

/* Event filtering and customization */

/*
 * InstallDialogEventFilter - Install custom event filter
 *
 * This function installs a custom event filter for a dialog,
 * allowing applications to customize event handling behavior.
 *
 * Parameters:
 *   theDialog   - The dialog to install filter for
 *   filterProc  - The filter procedure
 *   userData    - User data passed to filter procedure
 *
 * Returns:
 *   The previously installed filter procedure
 */
ModalFilterProcPtr InstallDialogEventFilter(DialogPtr theDialog,
                                           ModalFilterProcPtr filterProc,
                                           void* userData);

/*
 * RemoveDialogEventFilter - Remove custom event filter
 *
 * Parameters:
 *   theDialog - The dialog to remove filter from
 */
void RemoveDialogEventFilter(DialogPtr theDialog);

/*
 * CallDialogEventFilter - Call dialog's event filter
 *
 * This function calls the dialog's event filter procedure if one
 * is installed, allowing for custom event processing.
 *
 * Parameters:
 *   theDialog - The dialog
 *   theEvent  - The event to filter
 *   itemHit   - Returns the item hit by filtering
 *
 * Returns:
 *   true if the event was handled by the filter
 */
Boolean CallDialogEventFilter(DialogPtr theDialog, EventRecord* theEvent,
                          SInt16* itemHit);

/* Keyboard shortcuts and accelerators */

/*
 * SetDialogKeyboardShortcut - Set keyboard shortcut for item
 *
 * This function sets a keyboard shortcut that will activate
 * a specific dialog item when pressed.
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 *   keyCode   - The key code for the shortcut
 *   modifiers - Modifier keys required
 */
void SetDialogKeyboardShortcut(DialogPtr theDialog, SInt16 itemNo,
                              SInt16 keyCode, SInt16 modifiers);

/*
 * RemoveDialogKeyboardShortcut - Remove keyboard shortcut
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 */
void RemoveDialogKeyboardShortcut(DialogPtr theDialog, SInt16 itemNo);

/*
 * ProcessDialogKeyboardShortcut - Process keyboard shortcut
 *
 * This function checks if a keyboard event matches any registered
 * shortcuts for the dialog and activates the appropriate item.
 *
 * Parameters:
 *   theDialog - The dialog
 *   theEvent  - The keyboard event
 *   itemHit   - Returns the item activated by shortcut
 *
 * Returns:
 *   true if a shortcut was activated
 */
Boolean ProcessDialogKeyboardShortcut(DialogPtr theDialog, const EventRecord* theEvent,
                                  SInt16* itemHit);

/* Text editing support */

/*
 * HandleDialogTextEdit - Handle text editing in dialog
 *
 * This function handles text editing operations in dialog text fields,
 * including insertion, deletion, and selection changes.
 *
 * Parameters:
 *   theDialog - The dialog containing the text field
 *   itemNo    - The text field item number
 *   theEvent  - The event that triggered editing
 *
 * Returns:
 *   true if the text was modified
 */
Boolean HandleDialogTextEdit(DialogPtr theDialog, SInt16 itemNo,
                         const EventRecord* theEvent);

/*
 * GetDialogTextSelection - Get text selection in dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the text field
 *   itemNo    - The text field item number
 *   selStart  - Returns selection start
 *   selEnd    - Returns selection end
 *
 * Returns:
 *   true if there is a text selection
 */
Boolean GetDialogTextSelection(DialogPtr theDialog, SInt16 itemNo,
                           SInt16* selStart, SInt16* selEnd);

/*
 * SetDialogTextSelection - Set text selection in dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the text field
 *   itemNo    - The text field item number
 *   selStart  - Selection start position
 *   selEnd    - Selection end position
 */
void SetDialogTextSelection(DialogPtr theDialog, SInt16 itemNo,
                           SInt16 selStart, SInt16 selEnd);

/* Event timing and idle processing */

/*
 * SetDialogIdleProc - Set idle procedure for dialog
 *
 * This function sets a procedure that will be called during
 * idle time while the dialog is active.
 *
 * Parameters:
 *   theDialog - The dialog
 *   idleProc  - The idle procedure (or NULL to remove)
 */
void SetDialogIdleProc(DialogPtr theDialog, void (*idleProc)(DialogPtr));

/*
 * ProcessDialogIdle - Process idle time for dialog
 *
 * This function should be called regularly during modal dialog
 * processing to handle idle tasks and animations.
 *
 * Parameters:
 *   theDialog - The dialog to process idle time for
 */
void ProcessDialogIdle(DialogPtr theDialog);

/* Event validation and error handling */

/*
 * ValidateDialogEvent - Validate event for dialog processing
 *
 * This function checks if an event is valid and safe to process
 * for the specified dialog.
 *
 * Parameters:
 *   theDialog - The dialog
 *   theEvent  - The event to validate
 *
 * Returns:
 *   true if the event is valid for processing
 */
Boolean ValidateDialogEvent(DialogPtr theDialog, const EventRecord* theEvent);

/*
 * GetDialogEventError - Get last event processing error
 *
 * Returns:
 *   Error code from last event processing operation
 */
OSErr GetDialogEventError(void);

/* Platform event integration */

/*
 * ConvertPlatformEvent - Convert platform event to EventRecord
 *
 * This function converts a platform-specific event to a Mac
 * EventRecord for processing by the dialog system.
 *
 * Parameters:
 *   platformEvent - Platform-specific event data
 *   macEvent      - Returns converted Mac event
 *
 * Returns:
 *   true if conversion was successful
 */
Boolean ConvertPlatformEvent(const void* platformEvent, EventRecord* macEvent);

/*
 * HandlePlatformDialogEvent - Handle platform-specific dialog event
 *
 * This function handles events that are specific to the current
 * platform and cannot be converted to standard Mac events.
 *
 * Parameters:
 *   theDialog     - The dialog receiving the event
 *   platformEvent - Platform-specific event data
 *   itemHit       - Returns item hit by the event
 *
 * Returns:
 *   true if the event was handled
 */
Boolean HandlePlatformDialogEvent(DialogPtr theDialog, const void* platformEvent,
                              SInt16* itemHit);

/* Event debugging and logging */

/*
 * SetDialogEventLogging - Enable/disable event logging
 *
 * Parameters:
 *   enabled - true to enable logging of dialog events
 */
void SetDialogEventLogging(Boolean enabled);

/*
 * LogDialogEvent - Log a dialog event
 *
 * This function logs a dialog event for debugging purposes.
 *
 * Parameters:
 *   theDialog - The dialog
 *   theEvent  - The event to log
 *   itemHit   - The item hit by the event
 *   result    - The event processing result
 */
void LogDialogEvent(DialogPtr theDialog, const EventRecord* theEvent,
                   SInt16 itemHit, SInt16 result);

/* Internal event functions */
void InitDialogEvents(void);
void CleanupDialogEvents(void);
void NotifyDialogEventHandlers(DialogPtr theDialog, const EventRecord* theEvent,
                              SInt16 itemHit);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_EVENTS_H */

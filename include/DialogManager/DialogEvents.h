/*
 * DialogEvents.h - Dialog Event Management API
 *
 * This header defines the dialog event handling functionality,
 * maintaining exact Mac System 7.1 behavioral compatibility
 * while providing modern event processing capabilities.
 */

#ifndef DIALOG_EVENTS_H
#define DIALOG_EVENTS_H

#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event types for dialog processing */
enum {
    kDialogEvent_MouseDown      = 1,    /* Mouse button pressed */
    kDialogEvent_MouseUp        = 2,    /* Mouse button released */
    kDialogEvent_KeyDown        = 3,    /* Key pressed */
    kDialogEvent_KeyUp          = 4,    /* Key released */
    kDialogEvent_AutoKey        = 5,    /* Key repeat */
    kDialogEvent_Update         = 6,    /* Window needs update */
    kDialogEvent_Activate       = 8,    /* Window activate/deactivate */
    kDialogEvent_Null           = 0     /* Null event */
};

/* Special key codes for dialog navigation */
enum {
    kDialogKey_Return           = 0x0D, /* Return key */
    kDialogKey_Enter            = 0x03, /* Enter key */
    kDialogKey_Escape           = 0x1B, /* Escape key */
    kDialogKey_Tab              = 0x09, /* Tab key */
    kDialogKey_Backspace        = 0x08, /* Backspace key */
    kDialogKey_Delete           = 0x7F, /* Delete key */
    kDialogKey_LeftArrow        = 0x1C, /* Left arrow */
    kDialogKey_RightArrow       = 0x1D, /* Right arrow */
    kDialogKey_UpArrow          = 0x1E, /* Up arrow */
    kDialogKey_DownArrow        = 0x1F  /* Down arrow */
};

/* Modifier key flags */
enum {
    kDialogModifier_Shift       = 0x0001, /* Shift key */
    kDialogModifier_Control     = 0x0002, /* Control key */
    kDialogModifier_Option      = 0x0004, /* Option/Alt key */
    kDialogModifier_Command     = 0x0008, /* Command/Meta key */
    kDialogModifier_CapsLock    = 0x0010, /* Caps Lock */
    kDialogModifier_NumLock     = 0x0020  /* Num Lock */
};

/* Event handling result codes */
enum {
    kDialogEventResult_NotHandled   = 0,    /* Event not handled */
    kDialogEventResult_Handled      = 1,    /* Event handled normally */
    kDialogEventResult_ItemHit      = 2,    /* Event caused item hit */
    kDialogEventResult_Dismiss      = 3,    /* Event should dismiss dialog */
    kDialogEventResult_PassThrough  = 4     /* Pass event to system */
};

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
int16_t ProcessDialogEvent(DialogPtr theDialog, const EventRecord* theEvent,
                          int16_t* itemHit);

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
bool HandleDialogMouseDown(DialogPtr theDialog, const EventRecord* theEvent,
                          int16_t* itemHit);

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
bool HandleDialogKeyDown(DialogPtr theDialog, const EventRecord* theEvent,
                        int16_t* itemHit);

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
                         bool activate);

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
int16_t SetDialogFocus(DialogPtr theDialog, int16_t itemNo);

/*
 * GetDialogFocus - Get currently focused dialog item
 *
 * Parameters:
 *   theDialog - The dialog to query
 *
 * Returns:
 *   The item number with focus, or 0 if no item has focus
 */
int16_t GetDialogFocus(DialogPtr theDialog);

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
int16_t AdvanceDialogFocus(DialogPtr theDialog, bool backward);

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
bool IsDialogItemFocusable(DialogPtr theDialog, int16_t itemNo);

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
bool CallDialogEventFilter(DialogPtr theDialog, EventRecord* theEvent,
                          int16_t* itemHit);

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
void SetDialogKeyboardShortcut(DialogPtr theDialog, int16_t itemNo,
                              int16_t keyCode, int16_t modifiers);

/*
 * RemoveDialogKeyboardShortcut - Remove keyboard shortcut
 *
 * Parameters:
 *   theDialog - The dialog containing the item
 *   itemNo    - The item number
 */
void RemoveDialogKeyboardShortcut(DialogPtr theDialog, int16_t itemNo);

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
bool ProcessDialogKeyboardShortcut(DialogPtr theDialog, const EventRecord* theEvent,
                                  int16_t* itemHit);

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
bool HandleDialogTextEdit(DialogPtr theDialog, int16_t itemNo,
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
bool GetDialogTextSelection(DialogPtr theDialog, int16_t itemNo,
                           int16_t* selStart, int16_t* selEnd);

/*
 * SetDialogTextSelection - Set text selection in dialog item
 *
 * Parameters:
 *   theDialog - The dialog containing the text field
 *   itemNo    - The text field item number
 *   selStart  - Selection start position
 *   selEnd    - Selection end position
 */
void SetDialogTextSelection(DialogPtr theDialog, int16_t itemNo,
                           int16_t selStart, int16_t selEnd);

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
bool ValidateDialogEvent(DialogPtr theDialog, const EventRecord* theEvent);

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
bool ConvertPlatformEvent(const void* platformEvent, EventRecord* macEvent);

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
bool HandlePlatformDialogEvent(DialogPtr theDialog, const void* platformEvent,
                              int16_t* itemHit);

/* Event debugging and logging */

/*
 * SetDialogEventLogging - Enable/disable event logging
 *
 * Parameters:
 *   enabled - true to enable logging of dialog events
 */
void SetDialogEventLogging(bool enabled);

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
                   int16_t itemHit, int16_t result);

/* Internal event functions */
void InitDialogEvents(void);
void CleanupDialogEvents(void);
void UpdateDialogEventState(DialogPtr theDialog, const EventRecord* theEvent);
bool IsDialogEventRelevant(DialogPtr theDialog, const EventRecord* theEvent);
void NotifyDialogEventHandlers(DialogPtr theDialog, const EventRecord* theEvent,
                              int16_t itemHit);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_EVENTS_H */
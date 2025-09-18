/*
 * ModalDialogs.c - Modal Dialog Management Implementation
 *
 * This module provides the modal dialog processing functionality,
 * maintaining exact Mac System 7.1 behavioral compatibility.
 */

#include "../../include/DialogManager/ModalDialogs.h"
#include "../../include/DialogManager/DialogManager.h"
#include "../../include/DialogManager/DialogTypes.h"
#include "../../include/DialogManager/DialogEvents.h"
#include "../../include/DialogManager/DialogItems.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* External functions that need to be linked */
extern bool GetNextEvent(int16_t eventMask, EventRecord* theEvent);
extern void SystemTask(void);
extern uint32_t TickCount(void);
extern void Delay(uint32_t ticks, uint32_t* finalTicks);

/* Private state for modal dialog processing */
static struct {
    bool            initialized;
    DialogPtr       currentModal;
    bool            inModalLoop;
    int16_t         modalLevel;
    WindowPtr       modalStack[16];
    uint32_t        lastEventTime;
    bool            modalFiltersActive;
    ModalFilterProcPtr installedFilters[16];
    void*           filterUserData[16];
} gModalState = {0};

/* Private function prototypes */
static bool ProcessModalEvent(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);
static bool CallModalFilter(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);
static void UpdateModalState(DialogPtr theDialog);
static bool IsEventForDialog(DialogPtr theDialog, const EventRecord* theEvent);
static void HandleModalTimeout(DialogPtr theDialog, int16_t* itemHit);
static bool ProcessStandardModalKeys(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);
static void FlashButtonInternal(DialogPtr theDialog, int16_t itemNo);

/* Global Dialog Manager state access */
extern DialogManagerState* GetDialogManagerState(void);

/*
 * InitModalDialogs - Initialize modal dialog subsystem
 */
void InitModalDialogs(void)
{
    if (gModalState.initialized) {
        return;
    }

    memset(&gModalState, 0, sizeof(gModalState));
    gModalState.initialized = true;
    gModalState.currentModal = NULL;
    gModalState.inModalLoop = false;
    gModalState.modalLevel = 0;
    gModalState.lastEventTime = 0;
    gModalState.modalFiltersActive = false;

    /* Clear modal stack and filters */
    for (int i = 0; i < 16; i++) {
        gModalState.modalStack[i] = NULL;
        gModalState.installedFilters[i] = NULL;
        gModalState.filterUserData[i] = NULL;
    }

    printf("Modal dialog subsystem initialized\n");
}

/*
 * ModalDialog - Run modal dialog event loop
 *
 * This is the core modal dialog function that runs the event loop until
 * the user clicks a button or performs an action that ends the dialog.
 */
void ModalDialog(ModalFilterProcPtr filterProc, int16_t* itemHit)
{
    DialogPtr theDialog;
    EventRecord theEvent;
    bool done = false;
    int16_t eventMask = 0xFFFF; /* All events */
    uint32_t timeoutTime = 0;

    if (!gModalState.initialized) {
        printf("Error: Modal dialog system not initialized\n");
        if (itemHit) *itemHit = 0;
        return;
    }

    /* Get the current front modal dialog */
    theDialog = GetFrontModalDialog();
    if (!theDialog) {
        printf("Error: No modal dialog to process\n");
        if (itemHit) *itemHit = 0;
        return;
    }

    /* Set up modal state */
    gModalState.inModalLoop = true;
    gModalState.currentModal = theDialog;
    gModalState.lastEventTime = TickCount();

    if (itemHit) *itemHit = 0;

    printf("Starting modal dialog loop for dialog at %p\n", (void*)theDialog);

    /* Main modal event loop */
    while (!done) {
        /* Check for timeout if configured */
        if (timeoutTime > 0 && TickCount() > timeoutTime) {
            HandleModalTimeout(theDialog, itemHit);
            break;
        }

        /* Get next event */
        if (GetNextEvent(eventMask, &theEvent)) {
            gModalState.lastEventTime = theEvent.when;

            /* Call custom filter first if provided */
            if (filterProc) {
                if (filterProc(theDialog, &theEvent, itemHit)) {
                    /* Filter handled the event */
                    if (itemHit && *itemHit != 0) {
                        done = true;
                    }
                    continue;
                }
            }

            /* Call installed filter if any */
            if (CallModalFilter(theDialog, &theEvent, itemHit)) {
                if (itemHit && *itemHit != 0) {
                    done = true;
                }
                continue;
            }

            /* Process the event for the dialog */
            if (ProcessModalEvent(theDialog, &theEvent, itemHit)) {
                if (itemHit && *itemHit != 0) {
                    done = true;
                }
            }
        } else {
            /* No event - give time to system and handle idle processing */
            SystemTask();
            ProcessDialogIdle(theDialog);

            /* Small delay to prevent spinning */
            uint32_t finalTicks;
            Delay(1, &finalTicks);
        }

        /* Update modal state */
        UpdateModalState(theDialog);
    }

    /* Clean up modal state */
    gModalState.inModalLoop = false;
    gModalState.currentModal = NULL;

    printf("Modal dialog loop ended, item hit: %d\n", itemHit ? *itemHit : 0);
}

/*
 * IsDialogEvent - Check if an event belongs to a dialog
 */
bool IsDialogEvent(const EventRecord* theEvent)
{
    if (!theEvent || !gModalState.initialized) {
        return false;
    }

    /* Check if event is for the current modal dialog */
    if (gModalState.currentModal) {
        return IsEventForDialog(gModalState.currentModal, theEvent);
    }

    /* Check all dialogs in modal stack */
    for (int i = 0; i < gModalState.modalLevel; i++) {
        if (gModalState.modalStack[i]) {
            DialogPtr dialog = (DialogPtr)gModalState.modalStack[i];
            if (IsEventForDialog(dialog, theEvent)) {
                return true;
            }
        }
    }

    return false;
}

/*
 * DialogSelect - Handle dialog event
 */
bool DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog, int16_t* itemHit)
{
    if (!theEvent || !gModalState.initialized) {
        if (theDialog) *theDialog = NULL;
        if (itemHit) *itemHit = 0;
        return false;
    }

    /* Initialize return values */
    if (theDialog) *theDialog = NULL;
    if (itemHit) *itemHit = 0;

    /* Check if this is a dialog event */
    if (!IsDialogEvent(theEvent)) {
        return false;
    }

    /* Find which dialog should handle this event */
    DialogPtr targetDialog = NULL;

    if (gModalState.currentModal && IsEventForDialog(gModalState.currentModal, theEvent)) {
        targetDialog = gModalState.currentModal;
    } else {
        /* Check modal stack */
        for (int i = gModalState.modalLevel - 1; i >= 0; i--) {
            if (gModalState.modalStack[i]) {
                DialogPtr dialog = (DialogPtr)gModalState.modalStack[i];
                if (IsEventForDialog(dialog, theEvent)) {
                    targetDialog = dialog;
                    break;
                }
            }
        }
    }

    if (!targetDialog) {
        return false;
    }

    /* Process the event for the target dialog */
    EventRecord eventCopy = *theEvent;
    int16_t hit = 0;
    bool handled = ProcessModalEvent(targetDialog, &eventCopy, &hit);

    if (handled) {
        if (theDialog) *theDialog = targetDialog;
        if (itemHit) *itemHit = hit;
    }

    return handled;
}

/*
 * BeginModalDialog - Begin modal dialog processing
 */
OSErr BeginModalDialog(DialogPtr theDialog)
{
    if (!theDialog || !gModalState.initialized) {
        return -1700; /* dialogErr_InvalidDialog */
    }

    if (gModalState.modalLevel >= 16) {
        return -1706; /* dialogErr_AlreadyModal */
    }

    /* Add dialog to modal stack */
    gModalState.modalStack[gModalState.modalLevel] = (WindowPtr)theDialog;
    gModalState.modalLevel++;

    /* Set as front modal dialog */
    DialogManagerState* state = GetDialogManagerState();
    if (state) {
        state->globals.frontModal = theDialog;
    }

    /* Bring dialog to front */
    BringModalToFront(theDialog);

    /* Disable non-modal windows */
    DisableNonModalWindows();

    printf("Began modal processing for dialog at %p (level %d)\n",
           (void*)theDialog, gModalState.modalLevel);

    return 0; /* noErr */
}

/*
 * EndModalDialog - End modal dialog processing
 */
void EndModalDialog(DialogPtr theDialog)
{
    if (!theDialog || !gModalState.initialized) {
        return;
    }

    /* Find dialog in modal stack and remove it */
    for (int i = 0; i < gModalState.modalLevel; i++) {
        if (gModalState.modalStack[i] == (WindowPtr)theDialog) {
            /* Shift remaining dialogs down */
            for (int j = i; j < gModalState.modalLevel - 1; j++) {
                gModalState.modalStack[j] = gModalState.modalStack[j + 1];
            }
            gModalState.modalStack[gModalState.modalLevel - 1] = NULL;
            gModalState.modalLevel--;
            break;
        }
    }

    /* Update front modal dialog */
    DialogManagerState* state = GetDialogManagerState();
    if (state) {
        if (gModalState.modalLevel > 0) {
            state->globals.frontModal = (DialogPtr)gModalState.modalStack[gModalState.modalLevel - 1];
        } else {
            state->globals.frontModal = NULL;
            /* Re-enable non-modal windows */
            EnableNonModalWindows();
        }
    }

    printf("Ended modal processing for dialog at %p (level %d)\n",
           (void*)theDialog, gModalState.modalLevel);
}

/*
 * IsModalDialog - Check if a dialog is modal
 */
bool IsModalDialog(DialogPtr theDialog)
{
    if (!theDialog || !gModalState.initialized) {
        return false;
    }

    /* Check if dialog is in modal stack */
    for (int i = 0; i < gModalState.modalLevel; i++) {
        if (gModalState.modalStack[i] == (WindowPtr)theDialog) {
            return true;
        }
    }

    return false;
}

/*
 * GetFrontModalDialog - Get the front-most modal dialog
 */
DialogPtr GetFrontModalDialog(void)
{
    if (!gModalState.initialized || gModalState.modalLevel == 0) {
        return NULL;
    }

    return (DialogPtr)gModalState.modalStack[gModalState.modalLevel - 1];
}

/*
 * StandardModalFilter - Standard modal dialog filter
 */
bool StandardModalFilter(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit)
{
    if (!theDialog || !theEvent || !gModalState.initialized) {
        return false;
    }

    /* Handle standard modal keys */
    if (theEvent->what == kDialogEvent_KeyDown) {
        return ProcessStandardModalKeys(theDialog, theEvent, itemHit);
    }

    return false;
}

/*
 * InstallModalFilter - Install a modal filter procedure
 */
ModalFilterProcPtr InstallModalFilter(DialogPtr theDialog, ModalFilterProcPtr filterProc)
{
    if (!theDialog || !gModalState.initialized) {
        return NULL;
    }

    /* Find slot for this dialog */
    for (int i = 0; i < 16; i++) {
        if (gModalState.modalStack[i] == (WindowPtr)theDialog ||
            gModalState.installedFilters[i] == NULL) {
            ModalFilterProcPtr oldProc = gModalState.installedFilters[i];
            gModalState.installedFilters[i] = filterProc;
            gModalState.modalFiltersActive = (filterProc != NULL);
            return oldProc;
        }
    }

    return NULL;
}

/*
 * FlashButton - Flash a button briefly
 */
void FlashButton(DialogPtr theDialog, int16_t itemNo)
{
    if (!theDialog || itemNo <= 0) {
        return;
    }

    FlashButtonInternal(theDialog, itemNo);
}

/*
 * HandleModalKeyboard - Handle keyboard input in modal dialog
 */
bool HandleModalKeyboard(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit)
{
    if (!theDialog || !theEvent || theEvent->what != kDialogEvent_KeyDown) {
        return false;
    }

    return ProcessStandardModalKeys(theDialog, theEvent, itemHit);
}

/*
 * HandleModalMouse - Handle mouse input in modal dialog
 */
bool HandleModalMouse(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit)
{
    if (!theDialog || !theEvent || theEvent->what != kDialogEvent_MouseDown) {
        return false;
    }

    /* Find which item was hit */
    int16_t item = FindDialogItem(theDialog, theEvent->where);
    if (item > 0) {
        /* Flash the button if it's a button */
        int16_t itemType;
        Handle itemHandle;
        Rect itemBox;
        GetDialogItem(theDialog, item, &itemType, &itemHandle, &itemBox);

        if ((itemType & itemTypeMask) == ctrlItem || (itemType & itemTypeMask) == btnCtrl) {
            FlashButtonInternal(theDialog, item);
        }

        if (itemHit) *itemHit = item;
        return true;
    }

    return false;
}

/*
 * BringModalToFront - Bring modal dialog to front
 */
void BringModalToFront(DialogPtr theDialog)
{
    if (!theDialog) {
        return;
    }

    /* In a full implementation, this would call SelectWindow or similar */
    printf("Bringing modal dialog at %p to front\n", (void*)theDialog);
}

/*
 * DisableNonModalWindows - Disable non-modal windows
 */
void DisableNonModalWindows(void)
{
    /* In a full implementation, this would iterate through all windows
       and disable those that are not modal dialogs */
    printf("Disabling non-modal windows\n");
}

/*
 * EnableNonModalWindows - Re-enable non-modal windows
 */
void EnableNonModalWindows(void)
{
    /* In a full implementation, this would re-enable previously disabled windows */
    printf("Re-enabling non-modal windows\n");
}

/*
 * Private implementation functions
 */

static bool ProcessModalEvent(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit)
{
    if (!theDialog || !theEvent) {
        return false;
    }

    bool handled = false;
    int16_t hit = 0;

    switch (theEvent->what) {
        case kDialogEvent_MouseDown:
            handled = HandleModalMouse(theDialog, theEvent, &hit);
            break;

        case kDialogEvent_KeyDown:
        case kDialogEvent_AutoKey:
            handled = HandleModalKeyboard(theDialog, theEvent, &hit);
            break;

        case kDialogEvent_Update:
            HandleDialogUpdate(theDialog, theEvent);
            handled = true;
            break;

        case kDialogEvent_Activate:
            HandleDialogActivate(theDialog, theEvent, true);
            handled = true;
            break;

        default:
            /* Let dialog events module handle other events */
            int16_t result = ProcessDialogEvent(theDialog, theEvent, &hit);
            handled = (result != kDialogEventResult_NotHandled);
            break;
    }

    if (itemHit) *itemHit = hit;
    return handled;
}

static bool CallModalFilter(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit)
{
    if (!gModalState.modalFiltersActive || !theDialog || !theEvent) {
        return false;
    }

    /* Find filter for this dialog */
    for (int i = 0; i < 16; i++) {
        if (gModalState.modalStack[i] == (WindowPtr)theDialog &&
            gModalState.installedFilters[i] != NULL) {
            return gModalState.installedFilters[i](theDialog, theEvent, itemHit);
        }
    }

    return false;
}

static void UpdateModalState(DialogPtr theDialog)
{
    if (!theDialog) {
        return;
    }

    /* Update dialog state as needed */
    /* This could include cursor tracking, idle processing, etc. */
}

static bool IsEventForDialog(DialogPtr theDialog, const EventRecord* theEvent)
{
    if (!theDialog || !theEvent) {
        return false;
    }

    /* In a full implementation, this would check if the event's window
       matches the dialog's window */
    /* For now, assume all events are for the current dialog */
    return true;
}

static void HandleModalTimeout(DialogPtr theDialog, int16_t* itemHit)
{
    if (!theDialog) {
        return;
    }

    /* Handle dialog timeout - typically activate default button */
    int16_t defaultItem = GetDialogDefaultItem(theDialog);
    if (defaultItem > 0) {
        if (itemHit) *itemHit = defaultItem;
        FlashButtonInternal(theDialog, defaultItem);
    }

    printf("Modal dialog timeout - activated default item %d\n", defaultItem);
}

static bool ProcessStandardModalKeys(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit)
{
    if (!theDialog || !theEvent) {
        return false;
    }

    uint8_t keyCode = (theEvent->message & 0xFF);
    int16_t modifiers = theEvent->modifiers;

    switch (keyCode) {
        case kDialogKey_Return:
        case kDialogKey_Enter:
            /* Activate default button */
            {
                int16_t defaultItem = GetDialogDefaultItem(theDialog);
                if (defaultItem > 0) {
                    FlashButtonInternal(theDialog, defaultItem);
                    if (itemHit) *itemHit = defaultItem;
                    return true;
                }
            }
            break;

        case kDialogKey_Escape:
            /* Activate cancel button */
            {
                int16_t cancelItem = GetDialogCancelItem(theDialog);
                if (cancelItem > 0) {
                    FlashButtonInternal(theDialog, cancelItem);
                    if (itemHit) *itemHit = cancelItem;
                    return true;
                }
            }
            break;

        case kDialogKey_Tab:
            /* Tab navigation */
            {
                bool backward = (modifiers & kDialogModifier_Shift) != 0;
                AdvanceDialogFocus(theDialog, backward);
                return true;
            }
            break;

        case '.':
            /* Command-. for cancel */
            if (modifiers & kDialogModifier_Command) {
                int16_t cancelItem = GetDialogCancelItem(theDialog);
                if (cancelItem > 0) {
                    FlashButtonInternal(theDialog, cancelItem);
                    if (itemHit) *itemHit = cancelItem;
                    return true;
                }
            }
            break;
    }

    return false;
}

static void FlashButtonInternal(DialogPtr theDialog, int16_t itemNo)
{
    if (!theDialog || itemNo <= 0) {
        return;
    }

    /* Flash the button by briefly highlighting it */
    printf("Flashing button %d in dialog at %p\n", itemNo, (void*)theDialog);

    /* In a full implementation, this would:
       1. Invert the button
       2. Delay briefly
       3. Restore the button
    */
    InvalDialogItem(theDialog, itemNo);
    DrawDialogItem(theDialog, itemNo);

    /* Brief delay for visual feedback */
    uint32_t finalTicks;
    Delay(8, &finalTicks); /* About 1/8 second */

    InvalDialogItem(theDialog, itemNo);
    DrawDialogItem(theDialog, itemNo);
}

/*
 * Advanced modal dialog features
 */

void SetModalDialogTimeout(DialogPtr theDialog, uint32_t timeoutTicks, int16_t defaultItem)
{
    /* This would be implemented with a timer system */
    printf("Set modal dialog timeout: %u ticks, default item %d\n",
           timeoutTicks, defaultItem);
}

void ClearModalDialogTimeout(DialogPtr theDialog)
{
    printf("Cleared modal dialog timeout\n");
}

void SetModalDialogDismissButton(DialogPtr theDialog, int16_t itemNo)
{
    /* This would configure which button dismisses the dialog */
    printf("Set dismiss button to item %d\n", itemNo);
}

int16_t ShowNativeModal(const char* message, const char* title,
                       const char* buttons, int16_t iconType)
{
    /* Platform-specific native modal dialog */
    printf("Native modal: %s - %s (buttons: %s, icon: %d)\n",
           title, message, buttons, iconType);
    return 1; /* Default to OK */
}

void CleanupModalDialogs(void)
{
    if (!gModalState.initialized) {
        return;
    }

    /* Clean up any remaining modal state */
    gModalState.modalLevel = 0;
    gModalState.currentModal = NULL;
    gModalState.inModalLoop = false;

    printf("Modal dialog subsystem cleaned up\n");
}
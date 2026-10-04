/* DialogEvents.c - Dialog Event Handling Implementation
 *
 * This module provides event handling for dialogs in Mac System 7.1.
 * Implements IsDialogEvent and DialogSelect for modeless dialog support.
 */

#include <stdlib.h>
#include <string.h>

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogEvents.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogHelpers.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/DialogLogging.h"
#include "DialogManager/DialogEditText.h"
#include "EventManager/EventTypes.h"
#include "WindowManager/WindowManager.h"

/* Global event state */
static struct {
    Boolean initialized;
} gDialogEventState = {0};

/*
 * InitDialogEvents - Initialize dialog event subsystem
 */
void InitDialogEvents(void)
{
    if (gDialogEventState.initialized) {
        return;
    }

    gDialogEventState.initialized = true;
}

/*
 * IsDialogEvent - Determine if event targets a dialog
 *
 * Returns true if the event should be handled by a dialog.
 */
Boolean IsDialogEvent(const EventRecord* evt)
{
    if (!evt) {
        return false;
    }

    switch (evt->what) {
        case mouseDown:
        case mouseUp:
        case keyDown:
        case autoKey:
        case updateEvt:
            /* If front window is a dialog, claim it */
            return FrontWindowIsDialog();

        default:
            return false;
    }
}

/*
 * DialogSelect - Handle event for modeless dialogs
 *
 * Returns true and sets *itemHit when an item is "activated" (clicked button, etc.)
 */
Boolean DialogSelect(const EventRecord* evt, DialogPtr* which, SInt16* itemHit)
{
    DialogPtr dlg;
    Point local;
    SInt16 hit;

    if (!evt || !which || !itemHit) {
        return false;
    }

    dlg = FrontDialog();
    if (!dlg) {
        return false;
    }

    *which = dlg;
    *itemHit = 0;

    /* Handle update events.
     *
     * Only the ones actually addressed to this dialog. This used to repaint
     * the front dialog for any update event at all, whichever window it named
     * - so an update for a window behind redrew the dialog, erasing its
     * content, while whatever the caller does after DialogSelect to put back
     * the parts the Dialog Manager does not own was skipped because the
     * event's window did not match. That is why the Open dialog's file list
     * appeared once and was then wiped: the Standard File loop redraws the
     * list only for its own update events, and correctly so. */
    if (evt->what == updateEvt) {
        if ((WindowPtr)(uintptr_t)evt->message != (WindowPtr)dlg) {
            return false;
        }
        BeginUpdate((WindowPtr)dlg);
        UpdateDialog(dlg, ((WindowPtr)dlg)->updateRgn);
        EndUpdate((WindowPtr)dlg);
        return false;
    }

    /* Handle mouse down */
    if (evt->what == mouseDown) {
        local = evt->where;
        GlobalToLocalDialog(dlg, &local);

        hit = DialogHitTest(dlg, local);
        if (!hit) {
            return false;
        }

        /* Push buttons, checkboxes and radio buttons count only if the
         * button comes up inside them. */
        if (DialogItemIsPushButton(dlg, hit)) {
            if (!DialogTrackButton(dlg, hit, local, true)) return false;
            *itemHit = hit;
            return true;
        }

        /* Toggle checkbox */
        if (DialogItemIsCheckbox(dlg, hit)) {
            if (!DialogTrackButton(dlg, hit, local, false)) return false;
            ToggleDialogCheckbox(dlg, hit);
            *itemHit = hit;
            return true;
        }

        /* Select radio button (exclusive) */
        if (DialogItemIsRadio(dlg, hit)) {
            if (!DialogTrackButton(dlg, hit, local, false)) return false;
            SelectRadioInGroup(dlg, hit);
            *itemHit = hit;
            return true;
        }

        /* Set edit field focus, and let TextEdit place the insertion point */
        if (DialogItemIsEditText(dlg, hit)) {
            if (!HandleDialogEditTextClick(dlg, hit, local)) {
                SetDialogEditTextFocus(dlg, hit);
            }
            InvalDialogItem(dlg, hit);
            DrawDialogItem(dlg, hit);
            return false;
        }
    }

    /* Handle key down / auto key */
    if (evt->what == keyDown || evt->what == autoKey) {
        char ch = (char)(evt->message & 0xFF);

        /* Tab moves between edit-text items, as in System 7. */
        if (ch == '\t') {
            AdvanceDialogEditTextFocus(dlg, (evt->modifiers & shiftKey) != 0);
            return false;
        }

        /* Route key events through TextEdit so the focused field's selection,
         * caret, and text remain consistent with the dialog item. */
        SInt16 focus = GetDialogEditTextFocus(dlg);
        if (focus > 0 && HandleDialogEditTextKey(dlg, focus, ch)) {
            return false;
        }
    }

    return false;
}

/*
 * HandleDialogActivate - Handle activate event for dialog
 */
void HandleDialogActivate(DialogPtr theDialog, const EventRecord* theEvent, Boolean activating)
{
    (void)theEvent;
    (void)activating;
    if (!theDialog) {
        return;
    }

}

/*
 * ProcessDialogIdle - Process idle time for dialog
 */
void ProcessDialogIdle(DialogPtr theDialog)
{
    if (!theDialog) {
        return;
    }

    /* Stub - would handle idle processing like text cursor blinking */
}

/*
 * AdvanceDialogFocus - Advance focus to next/previous item
 */
SInt16 AdvanceDialogFocus(DialogPtr theDialog, Boolean backward)
{
    (void)backward;
    if (!theDialog) {
        return 0;
    }

    return 0; /* Success */
}

/*
 * CleanupDialogEvents - Cleanup dialog event subsystem
 */
void CleanupDialogEvents(void)
{
    if (!gDialogEventState.initialized) {
        return;
    }

    gDialogEventState.initialized = false;
}

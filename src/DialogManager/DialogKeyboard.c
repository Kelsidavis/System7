/**
#include "DialogManager/DialogInternal.h"
 * @file DialogKeyboard.c
 * @brief Dialog Manager keyboard navigation and control activation
 *
 * Implements Return/Esc (default/cancel button activation), Tab/Shift-Tab
 * (focus traversal), and Space (toggle focused control) for classic Mac dialog behavior.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "SystemTypes.h"
#include "DialogManager/DialogManager.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "WindowManager/WindowManager.h"
#include "EventManager/EventManager.h"
#include "QuickDrawConstants.h"
#include "System71StdLib.h"
#include "DialogManager/DialogLogging.h"

/* Logging helpers */
#define DM_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleDialog, kLogLevelDebug, "[DM] " fmt, ##__VA_ARGS__)
#define DM_LOG_TRACE(fmt, ...) serial_logf(kLogModuleDialog, kLogLevelTrace, "[DM] " fmt, ##__VA_ARGS__)
#define DM_LOG_WARN(fmt, ...)  serial_logf(kLogModuleDialog, kLogLevelWarn,  "[DM] " fmt, ##__VA_ARGS__)

/* External functions */
extern void Delay(UInt32 numTicks, UInt32* finalTicks);
extern void PenMode(SInt16 mode);
extern void InvertRect(const Rect* r);
extern void FrameRect(const Rect* r);
extern void InsetRect(Rect* r, SInt16 dh, SInt16 dv);
extern void OffsetRect(Rect* r, SInt16 dh, SInt16 dv);
extern void PenPat(const Pattern* pat);
extern void MoveTo(SInt16 h, SInt16 v);
extern void LineTo(SInt16 h, SInt16 v);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void GetClip(RgnHandle rgn);
extern void SetClip(RgnHandle rgn);
extern void ClipRect(const Rect* r);
extern RgnHandle NewRgn(void);
extern void DisposeRgn(RgnHandle rgn);
extern UInt32 TickCount(void);
extern struct QDGlobals qd;

/* Focus tracking - simple per-window storage */
#define MAX_DIALOGS 16
static struct {
    WindowPtr window;
    ControlHandle focusedControl;
} gFocusTable[MAX_DIALOGS];

/* Double-fire guard - prevent mouse+key overlap */
static UInt32 gLastActionTick = 0;
static SInt16 gLastActionKind = 0; /* 1=key, 2=mouse */

/**
 * Debounce action to prevent double-fire from mouse+key
 * @param kind 1=keyboard, 2=mouse
 * @return true if action should be suppressed, false if allowed
 */
Boolean DM_DebounceAction(SInt16 kind) {
    UInt32 now = TickCount();
    /* Handle TickCount wrap-around (defensive) */
    if (now < gLastActionTick) {
        gLastActionTick = 0;
        gLastActionKind = 0;
    }
    if (gLastActionKind && (now - gLastActionTick) < 6) {
        return true; /* ~100ms window, suppress */
    }
    gLastActionKind = kind;
    gLastActionTick = now;
    return false;
}

/**
 * Get focused control for window
 */
ControlHandle DM_GetKeyboardFocus(WindowPtr window) {
    int i;
    for (i = 0; i < MAX_DIALOGS; i++) {
        if (gFocusTable[i].window == window) {
            return gFocusTable[i].focusedControl;
        }
    }
    return NULL;
}

/**
 * Check if control can receive keyboard focus
 */
static Boolean _control_can_focus(ControlHandle h) {
    if (!h) {
        return false;
    }
    if (!(*h)->contrlVis) {
        return false; /* Skip invisible controls */
    }
    if ((*h)->contrlHilite == inactiveHilite) {
        return false; /* Skip inactive controls */
    }
    /* Skip zero-sized controls (defensive) */
    if ((*h)->contrlRect.right <= (*h)->contrlRect.left) {
        return false;
    }
    if ((*h)->contrlRect.bottom <= (*h)->contrlRect.top) {
        return false;
    }
    /* Only certain control types are focusable */
    return IsButtonControl(h) || IsCheckboxControl(h) || IsRadioControl(h);
}

/**
 * Toggle focus ring (XOR for clean erase on expose/flash)
 */
void ToggleFocusRing(ControlHandle c) {
    GrafPtr savePort;
    RgnHandle saveClip;
    Rect r;

    if (!c) {
        return;
    }

    /* Save graphics state */
    GetPort(&savePort);
    SetPort((GrafPtr)(*c)->contrlOwner);

    saveClip = NewRgn();
    if (saveClip) {
        GetClip(saveClip);
        ClipRect(&(*c)->contrlRect);
    }

    /* Draw XOR focus ring (1px inset) */
    r = (*c)->contrlRect;
    InsetRect(&r, 1, 1);

    PenPat(&qd.black);
    PenMode(patXor);
    FrameRect(&r); /* XOR makes it toggle on/off */
    PenMode(patCopy);

    /* Restore graphics state */
    if (saveClip) {
        SetClip(saveClip);
        DisposeRgn(saveClip);
    }
    SetPort(savePort);
}

/**
 * Clear focus for a window being disposed
 */
void DM_ClearFocusForWindow(WindowPtr w) {
    int i;
    if (!w) {
        return;
    }
    for (i = 0; i < MAX_DIALOGS; i++) {
        if (gFocusTable[i].window == w) {
            if (gFocusTable[i].focusedControl) {
                ToggleFocusRing(gFocusTable[i].focusedControl); /* erase */
            }
            gFocusTable[i].window = NULL;
            gFocusTable[i].focusedControl = NULL;
            return;
        }
    }
}

/**
 * Handle control disposal (clear focus if this control had it)
 */
void DM_OnDisposeControl(ControlHandle c) {
    int i;
    if (!c) {
        return;
    }
    for (i = 0; i < MAX_DIALOGS; i++) {
        if (gFocusTable[i].focusedControl == c) {
            ToggleFocusRing(c); /* erase ring before disposal */
            gFocusTable[i].focusedControl = NULL;
            break;
        }
    }
}

/**
 * Set keyboard focus to a control
 */
void DM_SetKeyboardFocus(WindowPtr window, ControlHandle newFocus) {
    ControlHandle oldFocus;
    int i, emptySlot;

    if (!window) {
        return;
    }

    /* Find window in focus table */
    emptySlot = -1;
    oldFocus = NULL;
    for (i = 0; i < MAX_DIALOGS; i++) {
        if (gFocusTable[i].window == window) {
            oldFocus = gFocusTable[i].focusedControl;
            gFocusTable[i].focusedControl = newFocus;
            break;
        } else if (gFocusTable[i].window == NULL && emptySlot < 0) {
            emptySlot = i;
        }
    }

    /* If window not in table, add it */
    if (i >= MAX_DIALOGS && emptySlot >= 0) {
        gFocusTable[emptySlot].window = window;
        gFocusTable[emptySlot].focusedControl = newFocus;
    }

    /* Log focus change */
    if (oldFocus != newFocus) {
        DM_LOG_DEBUG("Focus 0x%08x -> 0x%08x (win=0x%08x)\n",
                     (unsigned int)P2UL(oldFocus),
                     (unsigned int)P2UL(newFocus),
                     (unsigned int)P2UL(window));
    }

    /* Toggle focus rings (XOR erase old, XOR draw new) */
    if (oldFocus && oldFocus != newFocus) {
        ToggleFocusRing(oldFocus); /* Erase */
    }

    if (newFocus && newFocus != oldFocus) {
        ToggleFocusRing(newFocus); /* Draw */
    }

    DM_LOG_DEBUG("DM_SetKeyboardFocus: window=0x%08x old=0x%08x new=0x%08x\n",
                 (unsigned int)P2UL(window),
                 (unsigned int)P2UL(oldFocus),
                 (unsigned int)P2UL(newFocus));
}

/**
 * Focus next control (with wraparound, skipping invisible/inactive)
 */
void DM_FocusNextControl(WindowPtr window, Boolean backwards) {
    ControlHandle cur, first, it, chosen;

    if (!window) {
        return;
    }

    cur = DM_GetKeyboardFocus(window);
    first = _GetFirstControl(window);

    if (!first) {
        return; /* No controls */
    }

    chosen = NULL;

    /* If no focus, pick first focusable */
    if (!cur) {
        for (it = first; it; it = (*it)->nextControl) {
            if (_control_can_focus(it)) {
                chosen = it;
                break;
            }
        }
        if (chosen) {
            DM_SetKeyboardFocus(window, chosen);
        }
        return;
    }

    /* Forward traversal */
    if (!backwards) {
        /* Find next focusable after current */
        it = (*cur)->nextControl;
        for (; it; it = (*it)->nextControl) {
            if (_control_can_focus(it)) {
                chosen = it;
                break;
            }
        }
        /* If none found, wrap to first focusable */
        if (!chosen) {
            for (it = first; it; it = (*it)->nextControl) {
                if (_control_can_focus(it)) {
                    chosen = it;
                    break;
                }
            }
        }
    } else {
        /* Backward traversal - find previous focusable */
        ControlHandle prev, lastFocusable;
        prev = NULL;
        lastFocusable = NULL;

        for (it = first; it; it = (*it)->nextControl) {
            if (_control_can_focus(it)) {
                lastFocusable = it;
            }
            if (it == cur) {
                break; /* Stop at current */
            }
            if (_control_can_focus(it)) {
                prev = it; /* Track last focusable before current */
            }
        }

        /* Use previous if found, otherwise wrap to last */
        chosen = prev ? prev : lastFocusable;
    }

    if (chosen) {
        DM_SetKeyboardFocus(window, chosen);
    }
}

/**
 * Find button by flag (default or cancel)
 */
static ControlHandle DM_FindButtonByFlag(WindowPtr w, Boolean wantDefault, Boolean wantCancel) {
    ControlHandle c;
    SInt16 item;
    SInt16 itemType;
    Handle itemHandle;
    Rect itemRect;

    if (!w) {
        return NULL;
    }

    /* Pass 1: scan for a button whose CDEF marks it default/cancel */
    c = _GetFirstControl(w);
    while (c) {
        if (IsButtonControl(c)) {
            if ((wantDefault && IsDefaultButton(c)) ||
                (wantCancel && IsCancelButton(c))) {
                return c;
            }
        }
        c = (*c)->nextControl;
    }

    /* Pass 2 (fallback): use dialog record's aDefItem/aCancelItem */
    if (wantDefault || wantCancel) {
        item = wantDefault ? GetDialogDefaultItem((DialogPtr)w)
                           : GetDialogCancelItem((DialogPtr)w);

        if (item > 0) {
            GetDialogItem((DialogPtr)w, item, &itemType, &itemHandle, &itemRect);

            /* If GetDialogItem returned a control handle, use it directly */
            if (itemHandle && IsButtonControl((ControlHandle)itemHandle)) {
                return (ControlHandle)itemHandle;
            }
        }
    }

    return NULL;
}

/**
 * Find default button in dialog
 */
ControlHandle DM_FindDefaultButton(WindowPtr dialog) {
    return DM_FindButtonByFlag(dialog, true, false);
}

/**
 * Find cancel button in dialog
 */
ControlHandle DM_FindCancelButton(WindowPtr dialog) {
    return DM_FindButtonByFlag(dialog, false, true);
}

/**
 * Activate a push button (visual flash + action)
 */
void DM_ActivatePushButton(ControlHandle button) {
    UInt32 finalTicks;
    Rect innerRect;

    if (!button || !IsButtonControl(button)) {
        return;
    }

    DM_LOG_DEBUG("DM_ActivatePushButton: Flashing button (refCon=%d)\n",
                 (int)GetControlReference(button));

    /* Calculate inner rect for XOR flash (inset by 3 pixels) */
    innerRect = (*button)->contrlRect;
    innerRect.left += 3;
    innerRect.top += 3;
    innerRect.right -= 3;
    innerRect.bottom -= 3;

    /* Classic Mac button flash using XOR inversion */
    SInt16 oldMode __attribute__((unused)) = patCopy; /* Save current mode (simplified - assumes patCopy) */
    PenMode(patXor);
    InvertRect(&innerRect);
    Delay(8, &finalTicks); /* ~8 ticks = 133ms flash */
    InvertRect(&innerRect);
    PenMode(patCopy);

    /* Call action procedure if present */
    if ((*button)->contrlAction) {
        ControlActionProcPtr action = (*button)->contrlAction;
        (*action)(button, inButton);
    }
}

/**
 * Map control handle back to dialog item number
 */
static SInt16 DM_ItemFromControl(DialogPtr d, ControlHandle c) {
    SInt16 count, i;
    SInt16 itype;
    Handle ih;
    Rect r;

    if (!d || !c) {
        return 0;
    }

    count = CountDITL(d);
    for (i = 1; i <= count; i++) {
        itype = 0;
        ih = NULL;
        GetDialogItem(d, i, &itype, &ih, &r);
        if (ih && ((itype & 0x7F) == (ctrlItem + btnCtrl)) && (ControlHandle)ih == c) {
            return i;
        }
    }
    return 0;
}

/**
 * Handle Return key (activate default button)
 */
Boolean DM_HandleReturnKey(WindowPtr dialog, SInt16* itemHit) {
    ControlHandle defaultButton;
    SInt16 item;

    DM_LOG_TRACE("DM_HandleReturnKey: ENTRY\n");

    if (!dialog || !itemHit) {
        DM_LOG_WARN("DM_HandleReturnKey: NULL params\n");
        return false;
    }

    /* Debounce - prevent double-fire from mouse+key */
    if (DM_DebounceAction(1)) {
        DM_LOG_TRACE("DM_HandleReturnKey: Debounce suppressed\n");
        return false; /* Suppressed */
    }

    DM_LOG_TRACE("DM_HandleReturnKey: Finding default button\n");
    defaultButton = DM_FindDefaultButton(dialog);
    if (!defaultButton) {
        DM_LOG_TRACE("DM_HandleReturnKey: No default button found\n");
        return false; /* No default button */
    }

    DM_LOG_DEBUG("DM_HandleReturnKey: Activating default button\n");
    DM_ActivatePushButton(defaultButton);

    /* Map control → item number so modal loop exits */
    item = DM_ItemFromControl((DialogPtr)dialog, defaultButton);
    if (item > 0) {
        *itemHit = item;
        DM_LOG_DEBUG("DM_HandleReturnKey: itemHit=%d\n", item);
    }
    return true;
}

/**
 * Handle Escape key (activate cancel button)
 */
Boolean DM_HandleEscapeKey(WindowPtr dialog, SInt16* itemHit) {
    ControlHandle cancelButton;
    SInt16 item;

    if (!dialog || !itemHit) {
        return false;
    }

    /* Debounce - prevent double-fire from mouse+key */
    if (DM_DebounceAction(1)) {
        return false; /* Suppressed */
    }

    cancelButton = DM_FindCancelButton(dialog);
    if (!cancelButton) {
        return false; /* No cancel button */
    }

    DM_LOG_DEBUG("DM_HandleEscapeKey: Activating cancel button\n");
    DM_ActivatePushButton(cancelButton);

    /* Map control → item number so modal loop exits */
    item = DM_ItemFromControl((DialogPtr)dialog, cancelButton);
    if (item > 0) {
        *itemHit = item;
        DM_LOG_DEBUG("DM_HandleEscapeKey: itemHit=%d\n", item);
    }
    return true;
}

/**
 * Handle Space key (toggle focused control)
 */
Boolean DM_HandleSpaceKey(WindowPtr dialog, ControlHandle focusedControl) {
    SInt16 currentValue;
    ControlHandle focused;

    if (!dialog) {
        return false;
    }

    /* Get current focus if not provided */
    if (!focusedControl) {
        focused = DM_GetKeyboardFocus(dialog);
        if (!focused) {
            return false; /* No focused control */
        }
    } else {
        focused = focusedControl;
    }

    /* Check if control is inactive */
    if ((*focused)->contrlHilite == inactiveHilite) {
        return false; /* Inactive controls don't respond */
    }

    /* Debounce - prevent double-fire from mouse+key */
    if (DM_DebounceAction(1)) {
        return false; /* Suppressed */
    }

    /* Handle checkbox toggle */
    if (IsCheckboxControl(focused)) {
        currentValue = GetControlValue(focused);
        SetControlValue(focused, currentValue ? 0 : 1);
        DM_LOG_DEBUG("DM_HandleSpaceKey: Toggled checkbox to %d\n",
                     GetControlValue(focused));

        /* Call action if present */
        if ((*focused)->contrlAction) {
            ControlActionProcPtr action = (*focused)->contrlAction;
            (*action)(focused, inCheckBox);
        }
        return true;
    }

    /* Handle radio button selection */
    if (IsRadioControl(focused)) {
        SetControlValue(focused, 1); /* This triggers HandleRadioGroup */
        DM_LOG_DEBUG("DM_HandleSpaceKey: Selected radio button\n");

        /* Call action if present */
        if ((*focused)->contrlAction) {
            ControlActionProcPtr action = (*focused)->contrlAction;
            (*action)(focused, inCheckBox);
        }
        return true;
    }

    /* Handle button activation */
    if (IsButtonControl(focused)) {
        DM_ActivatePushButton(focused);
        return true;
    }

    return false;
}

/**
 * Handle Tab key (focus next control with wraparound)
 */
Boolean DM_HandleTabKey(WindowPtr dialog, Boolean shiftPressed) {
    if (!dialog) {
        return false;
    }

    DM_LOG_TRACE("DM_HandleTabKey: %s\n",
                 shiftPressed ? "Shift-Tab" : "Tab");

    DM_FocusNextControl(dialog, shiftPressed);
    return true;
}

/**
 * Main keyboard event handler for dialogs
 */
Boolean DM_HandleDialogKey(WindowPtr dialog, EventRecord* evt, SInt16* itemHit) {
    char ch;
    Boolean shiftPressed;

    if (!dialog || !evt || !itemHit) {
        return false;
    }

    if (evt->what != keyDown && evt->what != autoKey) {
        return false;
    }

    ch = (char)(evt->message & 0xFF);
    shiftPressed = (evt->modifiers & shiftKey) != 0;

    DM_LOG_TRACE("DM_HandleDialogKey: ch=0x%02X (%c)\n", (unsigned char)ch,
                 (ch >= 32 && ch < 127) ? ch : '?');

    /* Return key -> activate default button */
    if (ch == '\r' || ch == 0x03) { /* 0x03 = Enter on numeric keypad */
        DM_LOG_TRACE("DM_HandleDialogKey: Calling DM_HandleReturnKey\n");
        return DM_HandleReturnKey(dialog, itemHit);
    }

    /* Escape key -> activate cancel button */
    if ((unsigned char)ch == 0x1B) {
        return DM_HandleEscapeKey(dialog, itemHit);
    }

    /* Tab key -> focus traversal */
    if (ch == '\t') {
        return DM_HandleTabKey(dialog, shiftPressed);
    }

    /* Space key -> toggle focused control */
    if (ch == ' ') {
        return DM_HandleSpaceKey(dialog, NULL);
    }

    return false;
}

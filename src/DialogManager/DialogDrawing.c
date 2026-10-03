/*
 * DialogDrawing.c - Dialog Item Drawing Implementation
 *
 * Implements faithful System 7.1-style drawing for all dialog item types including
 * buttons, checkboxes, radio buttons, static text, edit text, icons, and user items.
 * Uses classic Mac look with proper beveling, focus rings, and state rendering.
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "FontManager/FontManager.h"
#include "WindowManager/WindowManager.h"
#include "ControlManager/StandardControls.h"
#include "MemoryMgr/MemoryManager.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/DialogDrawing.h"
#include "DialogManager/DialogInternal.h"
#include "DialogManager/DialogManagerInternal.h"  /* For DialogItemEx */
#include "DialogManager/DialogManagerStateExt.h"   /* For extended state with focus tracking */
#include "DialogManager/DialogLogging.h"
#include "DialogManager/AlertDialogs.h"  /* For SubstituteAlertParameters */

/* Draw push button or default button */
void DrawDialogButton(DialogPtr theDialog, const Rect* bounds, const unsigned char* title,
                     Boolean isDefault, Boolean isEnabled, Boolean isPressed) {
    Rect btnRect = *bounds;
    GrafPtr savePort;

    GetPort(&savePort);
    if (theDialog) {
        SetPort((GrafPtr)theDialog);
    }


    /* One routine draws a push button, and it belongs with the rest of the
     * control appearance - the Control Manager's button definition draws
     * through the same code, so a dialog's buttons and a NewControl button
     * cannot look different from each other. */
    CTL_DrawPushButton(&btnRect, title, isDefault, isEnabled, isPressed);

    SetPort(savePort);
}

/* Draw checkbox */
void DrawDialogCheckBox(const Rect* bounds, const unsigned char* title,
                       Boolean isChecked, Boolean isEnabled) {
    Rect boxRect;
    Rect textRect;
    SInt16 textV;
    GrafPtr savePort;

    GetPort(&savePort);

    EraseRect(bounds);   /* drawn afresh: an unchecked box kept its old check */

    /* Checkbox is 13x13 square on left */
    boxRect.top = bounds->top + 1;
    boxRect.left = bounds->left;
    boxRect.bottom = boxRect.top + 13;
    boxRect.right = boxRect.left + 13;

    /* Draw box */
    EraseRect(&boxRect);
    FrameRect(&boxRect);

    /* Draw check mark if checked */
    if (isChecked) {
        Rect checkRect = boxRect;
        InsetRect(&checkRect, 2, 2);
        /* Draw X pattern for check */
        MoveTo(checkRect.left, checkRect.top);
        LineTo(checkRect.right-1, checkRect.bottom-1);
        MoveTo(checkRect.right-1, checkRect.top);
        LineTo(checkRect.left, checkRect.bottom-1);
    }

    /* Draw title text */
    if (title && title[0] > 0) {
        TextFont(0);
        TextSize(12);
        TextFace(0);

        textRect.left = boxRect.right + 6;
        textRect.top = bounds->top;
        textRect.right = bounds->right;
        textRect.bottom = bounds->bottom;

        /* Center text vertically with the checkbox box using font metrics */
        SInt16 fontAscent = 9;   /* System font 12pt ascent */
        SInt16 fontDescent = 2;  /* System font 12pt descent */
        SInt16 textHeight = fontAscent + fontDescent;
        SInt16 boxHeight = boxRect.bottom - boxRect.top;
        /* Align text baseline with center of checkbox */
        textV = boxRect.top + ((boxHeight - textHeight) / 2) + fontAscent;
        MoveTo(textRect.left, textV);
        DrawString(title);
    }

    /* Draw disabled stipple if needed */
    if (!isEnabled) {
        /* Dimmed the classic way: grey cleared over it */
        PenPat(&qd.gray);
        PenMode(patBic);
        PaintRect(bounds);
        PenNormal();
    }

    SetPort(savePort);
}

/* Draw radio button */
void DrawDialogRadioButton(const Rect* bounds, const unsigned char* title,
                          Boolean isSelected, Boolean isEnabled) {
    Rect circleRect;
    Rect fillRect;
    SInt16 textV;
    GrafPtr savePort;

    GetPort(&savePort);

    EraseRect(bounds);   /* drawn afresh, as the checkbox is */


    /* Radio button is 13x13 circle on left */
    circleRect.top = bounds->top + 1;
    circleRect.left = bounds->left;
    circleRect.bottom = circleRect.top + 13;
    circleRect.right = circleRect.left + 13;

    /* Draw circle using RoundRect with equal width/height */
    EraseRect(&circleRect);
    FrameRoundRect(&circleRect, 13, 13);

    /* Draw filled center if selected */
    if (isSelected) {
        fillRect = circleRect;
        InsetRect(&fillRect, 3, 3);
        PaintRoundRect(&fillRect, 7, 7);
    }

    /* Draw title text */
    if (title && title[0] > 0) {
        TextFont(0);
        TextSize(12);
        TextFace(0);

        /* Center text vertically with the radio button circle using font metrics */
        SInt16 fontAscent = 9;   /* System font 12pt ascent */
        SInt16 fontDescent = 2;  /* System font 12pt descent */
        SInt16 textHeight = fontAscent + fontDescent;
        SInt16 circleHeight = circleRect.bottom - circleRect.top;
        /* Align text baseline with center of radio button */
        textV = circleRect.top + ((circleHeight - textHeight) / 2) + fontAscent;
        MoveTo(circleRect.right + 6, textV);
        DrawString(title);
    }

    /* Draw disabled stipple if needed */
    if (!isEnabled) {
        /* Dimmed the classic way: grey cleared over it */
        PenPat(&qd.gray);
        PenMode(patBic);
        PaintRect(bounds);
        PenNormal();
    }

    SetPort(savePort);
}

/* Draw static text */
void DrawDialogStaticText(DialogPtr theDialog, const Rect* bounds, const unsigned char* text,
                         Boolean isEnabled) {
    (void)isEnabled;
    SInt16 textV;
    GrafPtr savePort;
    unsigned char substitutedText[256];

    GetPort(&savePort);
    if (theDialog) {
        SetPort((GrafPtr)theDialog);
    }

    if (!text || text[0] == 0) {
        SetPort(savePort);
        return;
    }

    // DIALOG_LOG_DEBUG("Dialog: DrawStaticText '%.*s'\n", text[0], (const char*)&text[1]);

    /* Make a copy and perform parameter substitution (^0, ^1, ^2, ^3) */
    /* Note: text[0] is unsigned char, always < 256 */
    memcpy(substitutedText, text, text[0] + 1);
    SubstituteAlertParameters(substitutedText);
    text = substitutedText;

    /* Erase background */
    EraseRect(bounds);

    /* Draw text */
    TextFont(0);
    TextSize(12);
    TextFace(0);

    /*
     * Wrap the text inside its item rectangle.
     *
     * It used to be drawn as a single DrawString call, so anything wider than
     * the item was simply cut off at the right edge - the Empty Trash prompt is
     * 66 characters in a 266 pixel rect and lost its second half. System 7 wraps
     * static text within the rect the DITL gives it.
     *
     * Breaks at the last space that still fits; a word longer than the whole
     * line is broken mid-word rather than dropped. Stops when the next line
     * would fall outside the item.
     */
    {
        const SInt16 kLineHeight = 13;
        SInt16 maxWidth = bounds->right - bounds->left - 4;
        SInt16 len = text[0];
        SInt16 start = 1;

        textV = bounds->top + 12;  /* first baseline */

        while (start <= len && textV <= bounds->bottom) {
            unsigned char line[256];
            SInt16 fit = 0;
            SInt16 lastSpace = 0;
            SInt16 i;

            /* Longest prefix that fits */
            for (i = start; i <= len; i++) {
                line[0] = (unsigned char)(i - start + 1);
                memcpy(&line[1], &text[start], i - start + 1);
                if (StringWidth(line) > maxWidth) break;
                fit = i;
                if (text[i] == ' ') lastSpace = i;
            }

            if (fit == 0) {           /* not even one character fits */
                break;
            }
            if (i <= len && lastSpace > start) {
                fit = lastSpace;      /* break at the space instead */
            }

            line[0] = (unsigned char)(fit - start + 1);
            memcpy(&line[1], &text[start], fit - start + 1);

            MoveTo(bounds->left + 2, textV);
            DrawString(line);

            textV += kLineHeight;
            start = fit + 1;
            while (start <= len && text[start] == ' ') start++;  /* eat the break */
        }
    }

    SetPort(savePort);
}

/* Draw edit text field */
void DrawDialogEditText(const Rect* bounds, const unsigned char* text,
                       Boolean isEnabled, Boolean hasFocus, SInt16 itemNo) {
    Rect frameRect = *bounds;
    Rect textRect = *bounds;
    Rect caretRect;
    SInt16 textV;
    SInt16 textWidth;
    GrafPtr savePort;
    DialogManagerState* state;
    DialogManagerState_Extended* extState;
    SInt16 selStart = 0, selEnd = 0;

    GetPort(&savePort);
    state = GetDialogManagerState();
    extState = GET_EXTENDED_DLG_STATE(state);

    /* Read the selection straight off the item's TextEdit record. Peeking at
     * the stored handle rather than calling GetOrCreateDialogTEHandle keeps
     * this a pure draw: creating a TE record as a side effect of painting
     * would give a field a selection just by becoming visible. */
    if (extState && itemNo > 0 &&
        itemNo < (SInt16)(sizeof(extState->teHandles) / sizeof(extState->teHandles[0]))) {
        TEHandle hTE = (TEHandle)extState->teHandles[itemNo];
        if (hTE && *hTE) {
            selStart = (**hTE).selStart;
            selEnd = (**hTE).selEnd;
        }
    }


    /* Draw recessed frame */
    EraseRect(&frameRect);
    InsetRect(&frameRect, -1, -1);
    FrameRect(&frameRect);

    /* Draw inner white background */
    EraseRect(bounds);

    /* Draw text if present */
    textWidth = 0;
    if (text && text[0] > 0) {
        TextFont(0);
        TextSize(12);
        TextFace(0);

        InsetRect(&textRect, 3, 2);
        textV = textRect.top + 11;
        MoveTo(textRect.left, textV);
        DrawString(text);
        textWidth = StringWidth(text);
    } else {
        InsetRect(&textRect, 3, 2);
        textV = textRect.top + 11;
    }

    /* Draw focus ring if active */
    if (hasFocus && isEnabled) {
        Rect focusRect = *bounds;
        InsetRect(&focusRect, -2, -2);
        PenSize(2, 2);
        FrameRect(&focusRect);
        PenNormal();

        SInt16 textLen = (text && text[0] > 0) ? (SInt16)text[0] : 0;
        if (selStart < 0) selStart = 0;
        if (selEnd > textLen) selEnd = textLen;
        if (selStart > selEnd) selStart = selEnd;

        if (selStart < selEnd) {
            /* A selected run is shown inverted, which is what tells you that
             * typing replaces it. A dialog opens with its first field fully
             * selected, so without this the pre-selected name looked like an
             * ordinary insertion point right up until the first keystroke
             * wiped it. */
            Rect selRect;
            selRect.left = textRect.left + TextWidth(text + 1, 0, selStart);
            selRect.right = textRect.left + TextWidth(text + 1, 0, selEnd);
            selRect.top = textRect.top;
            selRect.bottom = textRect.bottom;
            InvertRect(&selRect);
        } else if (extState && extState->caretVisible) {
            /* The caret marks the insertion point, which is not necessarily
             * the end of the text - it was drawn at textLeft + full width
             * regardless of where the insertion point actually was. */
            caretRect.left = textRect.left +
                             (textLen > 0 ? TextWidth(text + 1, 0, selStart) : 0);
            caretRect.right = caretRect.left + 1;
            caretRect.top = textRect.top;
            caretRect.bottom = textRect.bottom;
            InvertRect(&caretRect);
        }
        (void)textWidth;
    }

    /* Draw disabled pattern if needed */
    if (!isEnabled) {
        FillRect(bounds, &qd.ltGray);
    }

    SetPort(savePort);
}

/* Draw icon item */
/*
 * An icon item: a colour 'cicn' if there is one, else the 'ICON'; IDs 0 to 2
 * are the system's stop, note and caution icons. This drew a box with an X
 * through it whatever the ID.
 */
void DrawDialogIcon(const Rect* bounds, SInt16 iconID, Boolean isEnabled) {
    (void)isEnabled;   /* an item's itemDisable bit is about clicks */
    EraseRect(bounds);

    extern CIconHandle GetCIcon(SInt16 iconID);
    extern void PlotCIcon(const Rect* theRect, CIconHandle theIcon);
    extern void DisposeCIcon(CIconHandle theIcon);
    CIconHandle cicn = GetCIcon(iconID);
    if (cicn) {
        PlotCIcon(bounds, cicn);
        DisposeCIcon(cicn);
        return;
    }

    extern Handle GetIcon(short iconID);
    extern void PlotIcon(const Rect* theRect, Handle theIcon);
    Handle icon = GetIcon(iconID);
    if (!icon && iconID >= 0 && iconID <= 2) {
        /* The system's alert icons, from the built-in bitmaps */
        extern const unsigned char* Alert_IconBitmap(SInt16 kind);
        const unsigned char* bits = Alert_IconBitmap((SInt16)(iconID + 1));
        if (bits) {
            Handle h = NewHandle(128);
            if (h) {
                memcpy(*h, bits, 128);
                PlotIcon(bounds, h);
                DisposeHandle(h);
            }
        }
        return;
    }
    if (icon) {
        PlotIcon(bounds, icon);
    }
}

/* A picture item: the 'PICT' with the item's resource ID, drawn into the
 * item's rectangle. There was no case for it, so it drew as an empty frame. */
void DrawDialogPicture(const Rect* bounds, SInt16 picID) {
    EraseRect(bounds);
    PicHandle pic = GetPicture(picID);
    if (pic) {
        DrawPicture(pic, bounds);
    }
}

/* Draw user item (calls user proc) */
void DrawDialogUserItem(DialogPtr theDialog, SInt16 itemNo, const Rect* bounds,
                       UserItemProcPtr userProc) {
    GrafPtr savePort;

    GetPort(&savePort);

    // DIALOG_LOG_DEBUG("Dialog: DrawUserItem %d\n", itemNo);

    if (userProc) {
        /* Call user's drawing procedure */
        userProc(theDialog, itemNo);
    } else {
        /* No procedure - draw placeholder */
        FrameRect(bounds);
    }

    SetPort(savePort);
}

/* Main dialog item drawing dispatcher */
void DrawDialogItemByType(DialogPtr theDialog, SInt16 itemNo,
                         const DialogItemEx* item) {
    SInt16 baseType;
    const unsigned char* textData;

    if (!item || !item->visible) return;

    baseType = item->type & itemTypeMask;
    textData = (const unsigned char*)item->data;

    /* Handle control items (buttons, checkboxes, radios) */
    /* Control items are type 4/5/6 = ctrlItem + control_type */
    if (baseType >= ctrlItem && baseType < statText) {
        SInt16 controlType = baseType - ctrlItem;

        if (controlType == btnCtrl) {
            /* Push button */
            Boolean isDefault = (itemNo == GetDialogDefaultItem(theDialog));
            DrawDialogButton(theDialog, &item->bounds, textData, isDefault,
                           item->enabled, false);
        } else if (controlType == chkCtrl) {
            /* Checkbox */
            Boolean isChecked = (item->refCon != 0);
            DrawDialogCheckBox(&item->bounds, textData, isChecked,
                             item->enabled);
        } else if (controlType == radCtrl) {
            /* Radio button */
            Boolean isSelected = (item->refCon != 0);
            DrawDialogRadioButton(&item->bounds, textData, isSelected,
                                item->enabled);
        } else {
            /* Unknown control type */
            // DIALOG_LOG_DEBUG("Dialog: Unknown control type %d\n", controlType);
            FrameRect(&item->bounds);
        }
        return;
    }

    /* Text and icons are never drawn dimmed. A DITL's itemDisable bit means
     * the item reports no clicks (Inside Macintosh: Toolbox Essentials,
     * 6-128) - which is why every alert's message and icon carry it - and
     * drawing it dimmed greyed out the text of every alert. */
    switch (baseType) {
        case statText:  /* Static text */
            DrawDialogStaticText(theDialog, &item->bounds, textData, true);
            break;

        case editText:  /* Edit text */
        {
            DialogManagerState* state = GetDialogManagerState();
            DialogManagerState_Extended* extState = GET_EXTENDED_DLG_STATE(state);
            Boolean hasFocus = (extState && extState->focusedEditTextItem == itemNo);
            DrawDialogEditText(&item->bounds, textData, true, hasFocus, itemNo);
            break;
        }

        case iconItem:  /* Icon */
            DrawDialogIcon(&item->bounds, (SInt16)item->refCon, true);
            break;

        case picItem:   /* Picture */
            DrawDialogPicture(&item->bounds, (SInt16)item->refCon);
            break;

        case userItem:  /* User item (type 0) */
        {
            UserItemProcPtr proc = (UserItemProcPtr)item->handle;
            DrawDialogUserItem(theDialog, itemNo, &item->bounds, proc);
            break;
        }

        default:
            // DIALOG_LOG_DEBUG("Dialog: Unknown item type %d\n", baseType);
            FrameRect(&item->bounds);
            break;
    }
}

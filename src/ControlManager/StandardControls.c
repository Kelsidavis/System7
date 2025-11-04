/* #include "SystemTypes.h" */
/**
 * @file StandardControls.c
 * @brief Standard control type implementations (buttons, checkboxes, radio buttons)
 *
 * This file implements the standard Mac OS control types including buttons,
 * checkboxes, radio buttons, and their visual rendering and interaction behavior.
 * These controls form the foundation of Mac application user interfaces.
 *
 * CONTROL procIDs AND PART CODES:
 * ================================
 * procIDs (registered in ControlTypes.h):
 *   pushButProc   = 0   - Standard push button (varCode bits: 1=default, 2=cancel)
 *   checkBoxProc  = 1   - Standard checkbox
 *   radioButProc  = 2   - Standard radio button
 *   scrollBarProc = 16  - Standard scrollbar (in ScrollbarControls.c)
 *
 * Part codes (for TestControl/TrackControl):
 *   inButton      = 10  - Inside push button
 *   inCheckBox    = 11  - Inside checkbox/radio button
 *
 * Button variant codes (OR these into procID for NewControl):
 *   0 - Normal button
 *   1 - Default button (thick border, Return key activates)
 *   2 - Cancel button (Esc key activates)
 *   3 - Default + Cancel (rare, but supported)
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "ControlManager/StandardControls.h"
#include "ControlManager/ControlManager.h"
/* ControlDrawing.h not needed */
#include "ControlManager/ControlTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "FontManager/FontManager.h"
#include "System71StdLib.h"
#include "SystemTypes.h"

/* External QuickDraw functions */
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void GetClip(RgnHandle rgn);
extern void SetClip(RgnHandle rgn);
extern RgnHandle NewRgn(void);
extern void DisposeRgn(RgnHandle rgn);
extern void ClipRect(const Rect* r);
extern void FrameRect(const Rect* r);
extern void PaintRect(const Rect* r);
extern void EraseRect(const Rect* r);
extern void InvertRect(const Rect* r);
extern void InvertOval(const Rect* r);
extern void FrameOval(const Rect* r);
extern void PaintOval(const Rect* r);
extern void FrameRoundRect(const Rect* r, short ovalWidth, short ovalHeight);
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void PenPat(const Pattern* pat);
extern void PenMode(short mode);
extern void PenSize(short width, short height);
extern Boolean PtInRect(Point pt, const Rect* r);
extern void RectRgn(RgnHandle rgn, const Rect* r);
extern void OffsetRect(Rect* r, short dh, short dv);
extern void InsetRect(Rect* r, short dh, short dv);
extern short StringWidth(ConstStr255Param s);
extern void DrawString(ConstStr255Param s);
extern struct QDGlobals qd;

/* External Control Manager functions */
extern Handle NewHandleClear(Size byteCount);
extern void DisposeHandle(Handle h);
extern void HLock(Handle h);
extern void HUnlock(Handle h);

/* Logging helpers */
#define CTRL_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleControl, kLogLevelDebug, "[CTRL] " fmt, ##__VA_ARGS__)
#define CTRL_LOG_WARN(fmt, ...)  serial_logf(kLogModuleControl, kLogLevelWarn,  "[CTRL] " fmt, ##__VA_ARGS__)

/* GetFontInfo implementation with Font Manager integration
 * Uses FontManager's GetFontMetrics() when available for accurate metrics,
 * falls back to proportional scaling based on font size.
 */
static void GetFontInfo(FontInfo* info) {
    if (!info) return;

    GrafPtr port = NULL;
    GetPort(&port);

    if (port) {
        /* Try to get actual metrics from Font Manager first */
        extern void GetFontMetrics(FMetricRec *theMetrics);
        FMetricRec fmetrics;
        GetFontMetrics(&fmetrics);

        /* Convert FMetricRec (SInt32) to FontInfo (short) */
        info->ascent = (short)fmetrics.ascent;
        info->descent = (short)fmetrics.descent;
        info->widMax = (short)fmetrics.widMax;
        info->leading = (short)fmetrics.leading;
    } else {
        /* Fallback when no port available */
        info->ascent = 9;
        info->descent = 2;
        info->widMax = 12;
        info->leading = 2;
    }
}

/* Standard control dimensions */
#define BUTTON_HEIGHT       20
#define BUTTON_MIN_WIDTH    59
#define BUTTON_MARGIN       8
#define CHECKBOX_SIZE       12
#define RADIO_SIZE          12
#define CONTROL_TEXT_MARGIN 4

/* Button data structure */
typedef struct ButtonData {
    Boolean isDefault;          /* Default button flag */
    Boolean isCancel;           /* Cancel button flag */
    Boolean isPushed;           /* Currently pushed state */
    SInt16 insetLevel;      /* Button inset level */
    Rect contentRect;        /* Content area rectangle */
    RgnHandle buttonRegion;  /* Button region for hit testing */
} ButtonData;

/* Checkbox/Radio data structure */
typedef struct CheckboxData {
    Boolean isRadio;            /* Radio button vs checkbox */
    Boolean isMixed;            /* Mixed state (for checkboxes) */
    SInt16 groupID;         /* Radio button group ID */
    Rect boxRect;            /* Checkbox/radio box rectangle */
    Rect textRect;           /* Text area rectangle */
} CheckboxData;

/* Internal function prototypes */
static void DrawButtonFrame(ControlHandle button, Boolean pushed);
static void DrawButtonText(ControlHandle button, Boolean pushed);
static void DrawDefaultButtonOutline(ControlHandle button);
static void DrawCheckboxMark(ControlHandle checkbox);
static void DrawRadioMark(ControlHandle radio);
static void CalculateButtonRects(ControlHandle button);
static void CalculateCheckboxRects(ControlHandle checkbox);
static void HandleRadioGroup(ControlHandle radio);
static SInt16 TestButtonPart(ControlHandle button, Point pt);
static SInt16 TestCheckboxPart(ControlHandle checkbox, Point pt);
static void DrawTextInRect(ConstStr255Param text, const Rect *rect, SInt16 alignment);

/**
 * Register standard control types
 */
void RegisterStandardControlTypes(void) {
    CTRL_LOG_DEBUG("Registering standard control types\n");
    RegisterControlType(pushButProc, ButtonCDEF);
    CTRL_LOG_DEBUG("Button control type registered (procID=%d)\n", pushButProc);
    RegisterControlType(checkBoxProc, CheckboxCDEF);
    CTRL_LOG_DEBUG("Checkbox control type registered (procID=%d)\n", checkBoxProc);
    RegisterControlType(radioButProc, RadioButtonCDEF);
    CTRL_LOG_DEBUG("Radio button control type registered (procID=%d)\n", radioButProc);
}

/**
 * Button control definition procedure
 */
SInt32 ButtonCDEF(SInt16 varCode, ControlHandle theControl,
                   SInt16 message, SInt32 param) {
    ButtonData *buttonData;
    Rect bounds;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Allocate button data */
        CTRL_LOG_DEBUG("ButtonCDEF initCntl: Allocating button data\n");
        (*theControl)->contrlData = NewHandleClear(sizeof(ButtonData));
        if ((*theControl)->contrlData) {
            CTRL_LOG_DEBUG("ButtonCDEF initCntl: Button data allocated\n");
            buttonData = (ButtonData *)*(*theControl)->contrlData;
            buttonData->isDefault = (varCode & 1) != 0;
            buttonData->isCancel = (varCode & 2) != 0;
            buttonData->insetLevel = 2;
            buttonData->isPushed = false;
            CTRL_LOG_DEBUG("ButtonCDEF initCntl: Flags set\n");

            /* Create button region */
            CTRL_LOG_DEBUG("ButtonCDEF initCntl: Creating button region\n");
            buttonData->buttonRegion = NewRgn();
            if (!buttonData->buttonRegion) {
                CTRL_LOG_WARN("ButtonCDEF initCntl: Failed to allocate button region\n");
            }
            CTRL_LOG_DEBUG("ButtonCDEF initCntl: Button region created\n");

            /* Calculate button rectangles */
            CTRL_LOG_DEBUG("ButtonCDEF initCntl: Calculating button rects\n");
            CalculateButtonRects(theControl);
            CTRL_LOG_DEBUG("ButtonCDEF initCntl: Done\n");
        } else {
            CTRL_LOG_WARN("ButtonCDEF initCntl: NewHandleClear FAILED\n");
        }
        break;

    case dispCntl:
        /* Dispose button data */
        if ((*theControl)->contrlData) {
            buttonData = (ButtonData *)*(*theControl)->contrlData;
            if (buttonData->buttonRegion) {
                DisposeRgn(buttonData->buttonRegion);
            }
            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw button */
        if ((*theControl)->contrlData) {
            buttonData = (ButtonData *)*(*theControl)->contrlData;
            bounds = (*theControl)->contrlRect;

            /* Draw default button outline if needed */
            if (buttonData->isDefault) {
                DrawDefaultButtonOutline(theControl);
            }

            /* Draw button frame */
            DrawButtonFrame(theControl, buttonData->isPushed ||
                           (*theControl)->contrlHilite == inButton);

            /* Draw button text */
            DrawButtonText(theControl, buttonData->isPushed ||
                          (*theControl)->contrlHilite == inButton);

            /* Draw highlight/inactive state */
            if ((*theControl)->contrlHilite == inactiveHilite) {
                /* Gray out button */
                PenPat(&qd.gray);
                PenMode(patBic);
                PaintRect(&bounds);
                PenMode(patCopy);
                PenPat(&qd.black);
            }
        }
        break;

    case testCntl:
        /* Inactive controls don't respond to hits */
        if ((*theControl)->contrlHilite == inactiveHilite) {
            return 0;
        }
        /* Test if point is in button */
        pt = *(Point *)&param;
        return TestButtonPart(theControl, pt);

    case calcCRgns:
    case calcCntlRgn:
        /* Calculate button regions */
        CalculateButtonRects(theControl);
        break;

    case posCntl:
        /* Handle button positioning/value changes */
        if ((*theControl)->contrlData) {
            buttonData = (ButtonData *)*(*theControl)->contrlData;
            buttonData->isPushed = ((*theControl)->contrlValue != 0);
        }
        break;

    case autoTrack:
        /* Handle automatic tracking */
        if ((*theControl)->contrlData) {
            buttonData = (ButtonData *)*(*theControl)->contrlData;
            buttonData->isPushed = true;
            Draw1Control(theControl);
        }
        break;
    }

    return 0;
}

/**
 * Checkbox control definition procedure
 */
SInt32 CheckboxCDEF(SInt16 varCode, ControlHandle theControl,
                     SInt16 message, SInt32 param) {
    CheckboxData *checkData;
    Rect bounds;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Allocate checkbox data */
        (*theControl)->contrlData = NewHandleClear(sizeof(CheckboxData));
        if ((*theControl)->contrlData) {
            checkData = (CheckboxData *)*(*theControl)->contrlData;
            checkData->isRadio = false;
            checkData->isMixed = false;
            checkData->groupID = 0;

            /* Calculate checkbox rectangles */
            CalculateCheckboxRects(theControl);
        }
        break;

    case dispCntl:
        /* Dispose checkbox data */
        if ((*theControl)->contrlData) {
            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw checkbox */
        if ((*theControl)->contrlData) {
            checkData = (CheckboxData *)*(*theControl)->contrlData;

            /* Draw checkbox frame */
            FrameRect(&checkData->boxRect);

            /* Draw checkmark if checked */
            if ((*theControl)->contrlValue) {
                DrawCheckboxMark(theControl);
            }

            /* Draw checkbox text */
            if ((*theControl)->contrlTitle[0] > 0) {
                DrawTextInRect((*theControl)->contrlTitle, &checkData->textRect, 0);
            }

            /* Gray out if inactive */
            if ((*theControl)->contrlHilite == inactiveHilite) {
                bounds = (*theControl)->contrlRect;
                PenPat(&qd.gray);
                PenMode(patBic);
                PaintRect(&bounds);
                PenMode(patCopy);
                PenPat(&qd.black);
            }

            /* Highlight if tracking */
            if ((*theControl)->contrlHilite == inCheckBox) {
                InvertRect(&checkData->boxRect);
            }
        }
        break;

    case testCntl:
        /* Inactive controls don't respond to hits */
        if ((*theControl)->contrlHilite == inactiveHilite) {
            return 0;
        }
        /* Test if point is in checkbox */
        pt = *(Point *)&param;
        return TestCheckboxPart(theControl, pt);

    case calcCRgns:
    case calcCntlRgn:
        /* Recalculate checkbox regions */
        CalculateCheckboxRects(theControl);
        break;

    case posCntl:
        /* Handle checkbox value changes */
        /* Nothing special needed for checkboxes */
        break;
    }

    return 0;
}

/**
 * Radio button control definition procedure
 */
SInt32 RadioButtonCDEF(SInt16 varCode, ControlHandle theControl,
                        SInt16 message, SInt32 param) {
    CheckboxData *radioData;
    Rect bounds;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Allocate radio button data */
        (*theControl)->contrlData = NewHandleClear(sizeof(CheckboxData));
        if ((*theControl)->contrlData) {
            radioData = (CheckboxData *)*(*theControl)->contrlData;
            radioData->isRadio = true;
            radioData->isMixed = false;
            radioData->groupID = varCode; /* Use variant as group ID */

            /* Calculate radio button rectangles */
            CalculateCheckboxRects(theControl);
        }
        break;

    case dispCntl:
        /* Dispose radio button data */
        if ((*theControl)->contrlData) {
            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw radio button */
        if ((*theControl)->contrlData) {
            radioData = (CheckboxData *)*(*theControl)->contrlData;

            /* Draw radio button circle */
            FrameOval(&radioData->boxRect);

            /* Draw radio mark if selected */
            if ((*theControl)->contrlValue) {
                DrawRadioMark(theControl);
            }

            /* Draw radio button text */
            if ((*theControl)->contrlTitle[0] > 0) {
                DrawTextInRect((*theControl)->contrlTitle, &radioData->textRect, 0);
            }

            /* Gray out if inactive */
            if ((*theControl)->contrlHilite == inactiveHilite) {
                bounds = (*theControl)->contrlRect;
                PenPat(&qd.gray);
                PenMode(patBic);
                PaintRect(&bounds);
                PenMode(patCopy);
                PenPat(&qd.black);
            }

            /* Highlight if tracking */
            if ((*theControl)->contrlHilite == inCheckBox) {
                InvertOval(&radioData->boxRect);
            }
        }
        break;

    case testCntl:
        /* Inactive controls don't respond to hits */
        if ((*theControl)->contrlHilite == inactiveHilite) {
            return 0;
        }
        /* Test if point is in radio button */
        pt = *(Point *)&param;
        return TestCheckboxPart(theControl, pt);

    case calcCRgns:
    case calcCntlRgn:
        /* Recalculate radio button regions */
        CalculateCheckboxRects(theControl);
        break;

    case posCntl:
        /* Handle radio button value changes */
        if ((*theControl)->contrlValue) {
            /* When radio button is selected, deselect others in group */
            HandleRadioGroup(theControl);
        }
        break;
    }

    return 0;
}

/* Utility Functions */

/**
 * Draw button frame
 */
static void DrawButtonFrame(ControlHandle button, Boolean pushed) {
    ButtonData *buttonData;
    Rect frameRect;

    if (!button || !(*button)->contrlData) {
        return;
    }

    buttonData = (ButtonData *)*(*button)->contrlData;
    frameRect = (*button)->contrlRect;

    /* Adjust frame for default button */
    if (buttonData->isDefault) {
        InsetRect(&frameRect, 3, 3);
    }

    /* Draw 3D button frame */
    if (pushed) {
        /* Pushed state - dark frame */
        PenPat(&qd.black);
        FrameRect(&frameRect);
        InsetRect(&frameRect, 1, 1);
        PenPat(&qd.dkGray);
        FrameRect(&frameRect);
    } else {
        /* Normal state - raised frame */
        /* Top and left highlight */
        PenPat(&qd.white);
        MoveTo(frameRect.left, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.top);

        /* Bottom and right shadow */
        PenPat(&qd.black);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);

        /* Inner shadow */
        InsetRect(&frameRect, 1, 1);
        PenPat(&qd.dkGray);
        MoveTo(frameRect.right - 1, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);
    }

    /* Fill button interior */
    InsetRect(&frameRect, 1, 1);
    PenPat(&qd.ltGray);
    PaintRect(&frameRect);
    PenPat(&qd.black);
}

/**
 * Draw button text
 */
static void DrawButtonText(ControlHandle button, Boolean pushed) {
    ButtonData *buttonData;
    Rect textRect;
    SInt16 offset = pushed ? 1 : 0;
    FontInfo fontInfo;

    if (!button || (*button)->contrlTitle[0] == 0 || !(*button)->contrlData) {
        return;
    }

    buttonData = (ButtonData *)*(*button)->contrlData;
    textRect = buttonData->contentRect;

    if (offset) {
        OffsetRect(&textRect, offset, offset);
    }

    /* Set font and get metrics */
    GetFontInfo(&fontInfo);

    /* Center text in button */
    SInt16 textWidth = StringWidth((*button)->contrlTitle);
    SInt16 textHeight = fontInfo.ascent + fontInfo.descent;

    MoveTo(textRect.left + (textRect.right - textRect.left - textWidth) / 2,
           textRect.top + (textRect.bottom - textRect.top - textHeight) / 2 + fontInfo.ascent);

    DrawString((*button)->contrlTitle);
}

/**
 * Draw default button outline
 */
static void DrawDefaultButtonOutline(ControlHandle button) {
    Rect outlineRect;

    if (!button) {
        return;
    }

    outlineRect = (*button)->contrlRect;

    /* Draw thick rounded rectangle outline */
    PenSize(3, 3);
    FrameRoundRect(&outlineRect, 16, 16);
    PenSize(1, 1);
}

/**
 * Draw checkbox mark
 */
static void DrawCheckboxMark(ControlHandle checkbox) {
    CheckboxData *checkData;
    Rect markRect;

    if (!checkbox || !(*checkbox)->contrlData) {
        return;
    }

    checkData = (CheckboxData *)*(*checkbox)->contrlData;
    markRect = checkData->boxRect;

    if (checkData->isMixed) {
        /* Draw dash for mixed state */
        MoveTo(markRect.left + 3, (markRect.top + markRect.bottom) / 2);
        LineTo(markRect.right - 3, (markRect.top + markRect.bottom) / 2);
        LineTo(markRect.right - 3, (markRect.top + markRect.bottom) / 2 + 1);
        LineTo(markRect.left + 3, (markRect.top + markRect.bottom) / 2 + 1);
    } else {
        /* Draw checkmark */
        PenSize(2, 2);
        MoveTo(markRect.left + 2, markRect.top + 6);
        LineTo(markRect.left + 5, markRect.bottom - 3);
        LineTo(markRect.right - 2, markRect.top + 3);
        PenSize(1, 1);
    }
}

/**
 * Draw radio button mark
 */
static void DrawRadioMark(ControlHandle radio) {
    CheckboxData *radioData;
    Rect markRect;

    if (!radio || !(*radio)->contrlData) {
        return;
    }

    radioData = (CheckboxData *)*(*radio)->contrlData;
    markRect = radioData->boxRect;

    /* Draw filled circle in center */
    InsetRect(&markRect, 3, 3);
    PenPat(&qd.black);
    PaintOval(&markRect);
}

/**
 * Calculate button rectangles
 */
static void CalculateButtonRects(ControlHandle button) {
    ButtonData *buttonData;
    Rect bounds;

    if (!button || !(*button)->contrlData) {
        return;
    }

    buttonData = (ButtonData *)*(*button)->contrlData;
    bounds = (*button)->contrlRect;

    /* Content rectangle (inside frame) */
    buttonData->contentRect = bounds;
    InsetRect(&buttonData->contentRect, BUTTON_MARGIN, 4);

    if (buttonData->isDefault) {
        InsetRect(&buttonData->contentRect, 3, 3);
    }

    /* Update button region */
    if (buttonData->buttonRegion) {
        RectRgn(buttonData->buttonRegion, &bounds);
    }
}

/**
 * Calculate checkbox/radio rectangles
 */
static void CalculateCheckboxRects(ControlHandle checkbox) {
    CheckboxData *checkData;
    Rect bounds;
    SInt16 boxSize;

    if (!checkbox || !(*checkbox)->contrlData) {
        return;
    }

    checkData = (CheckboxData *)*(*checkbox)->contrlData;
    bounds = (*checkbox)->contrlRect;
    boxSize = CHECKBOX_SIZE;  /* Same size for both (12 pixels) */

    /* Box rectangle (left side) */
    checkData->boxRect = bounds;
    (checkData->boxRect).right = (checkData->boxRect).left + boxSize;
    (checkData->boxRect).bottom = (checkData->boxRect).top + boxSize;

    /* Center vertically if control is taller than box */
    if ((bounds.bottom - bounds.top) > boxSize) {
        SInt16 offset = ((bounds.bottom - bounds.top) - boxSize) / 2;
        OffsetRect(&checkData->boxRect, 0, offset);
    }

    /* Text rectangle (right side) */
    checkData->textRect = bounds;
    (checkData->textRect).left = (checkData->boxRect).right + CONTROL_TEXT_MARGIN;
}

/**
 * Test button part hit
 */
static SInt16 TestButtonPart(ControlHandle button, Point pt) {
    if (!button) {
        return 0;
    }

    if (PtInRect(pt, &(*button)->contrlRect)) {
        return inButton;
    }

    return 0;
}

/**
 * Test checkbox/radio part hit
 */
static SInt16 TestCheckboxPart(ControlHandle checkbox, Point pt) {
    if (!checkbox) {
        return 0;
    }

    if (PtInRect(pt, &(*checkbox)->contrlRect)) {
        return inCheckBox;
    }

    return 0;
}

/**
 * Handle radio button group logic
 */
static void HandleRadioGroup(ControlHandle radio) {
    CheckboxData *radioData;
    WindowPtr window;
    ControlHandle control;
    CheckboxData *otherData;

    if (!radio || !(*radio)->contrlData) {
        return;
    }

    radioData = (CheckboxData *)*(*radio)->contrlData;
    window = (*radio)->contrlOwner;

    if (!window || !radioData->isRadio) {
        return;
    }

    /* Iterate through all controls in the window */
    control = _GetFirstControl(window);
    while (control) {
        /* Skip self */
        if (control != radio && (*control)->contrlData) {
            /* Check if it's a radio button in the same group */
            if (IsRadioControl(control)) {
                otherData = (CheckboxData *)*(*control)->contrlData;
                if (otherData->groupID == radioData->groupID) {
                    /* Deselect other radio button in group */
                    SetControlValue(control, 0);
                }
            }
        }
        control = (*control)->nextControl;
    }
}

/* Control Type Checking Functions */

/**
 * Check if control is a button
 */
Boolean IsButtonControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    SInt16 procID = GetControlVariant(control) & 0xFFF0;
    return (procID == pushButProc);
}

/**
 * Check if control is a checkbox
 */
Boolean IsCheckboxControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    SInt16 procID = GetControlVariant(control) & 0xFFF0;
    return (procID == checkBoxProc);
}

/**
 * Check if control is a radio button
 */
Boolean IsRadioControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    SInt16 procID = GetControlVariant(control) & 0xFFF0;
    return (procID == radioButProc);
}

/**
 * Set checkbox mixed state
 */
void SetCheckboxMixed(ControlHandle checkbox, Boolean mixed) {
    CheckboxData *checkData;

    if (!checkbox || !IsCheckboxControl(checkbox) || !(*checkbox)->contrlData) {
        return;
    }

    checkData = (CheckboxData *)*(*checkbox)->contrlData;
    if (checkData->isMixed != mixed) {
        checkData->isMixed = mixed;

        /* Redraw if visible */
        if ((*checkbox)->contrlVis) {
            Draw1Control(checkbox);
        }
    }
}

/**
 * Get checkbox mixed state
 */
Boolean GetCheckboxMixed(ControlHandle checkbox) {
    CheckboxData *checkData;

    if (!checkbox || !IsCheckboxControl(checkbox) || !(*checkbox)->contrlData) {
        return false;
    }

    checkData = (CheckboxData *)*(*checkbox)->contrlData;
    return checkData->isMixed;
}

/**
 * Set radio button group
 */
void SetRadioGroup(ControlHandle radio, SInt16 groupID) {
    CheckboxData *radioData;

    if (!radio || !IsRadioControl(radio) || !(*radio)->contrlData) {
        return;
    }

    radioData = (CheckboxData *)*(*radio)->contrlData;
    radioData->groupID = groupID;
}

/**
 * Get radio button group
 */
SInt16 GetRadioGroup(ControlHandle radio) {
    CheckboxData *radioData;

    if (!radio || !IsRadioControl(radio) || !(*radio)->contrlData) {
        return 0;
    }

    radioData = (CheckboxData *)*(*radio)->contrlData;
    return radioData->groupID;
}

/**
 * Check if button is default button
 */
Boolean IsDefaultButton(ControlHandle button) {
    ButtonData *buttonData;

    if (!button || !IsButtonControl(button) || !(*button)->contrlData) {
        return false;
    }

    buttonData = (ButtonData *)*(*button)->contrlData;
    return buttonData->isDefault;
}

/**
 * Check if button is cancel button
 */
Boolean IsCancelButton(ControlHandle button) {
    ButtonData *buttonData;

    if (!button || !IsButtonControl(button) || !(*button)->contrlData) {
        return false;
    }

    buttonData = (ButtonData *)*(*button)->contrlData;
    return buttonData->isCancel;
}

/**
 * Draw text in rectangle (simple left-aligned version for checkboxes/radios)
 */
static void DrawTextInRect(ConstStr255Param text, const Rect *rect, SInt16 alignment) {
    FontInfo fontInfo;
    SInt16 textHeight;
    SInt16 v;

    if (!text || text[0] == 0 || !rect) {
        return;
    }

    /* Get font metrics */
    GetFontInfo(&fontInfo);
    textHeight = fontInfo.ascent + fontInfo.descent;

    /* Calculate vertical position (centered) */
    v = rect->top + ((rect->bottom - rect->top) - textHeight) / 2 + fontInfo.ascent;

    /* Draw text left-aligned (alignment parameter unused for now) */
    MoveTo(rect->left, v);
    DrawString(text);
}

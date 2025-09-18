/**
 * @file StandardControls.c
 * @brief Standard control type implementations (buttons, checkboxes, radio buttons)
 *
 * This file implements the standard Mac OS control types including buttons,
 * checkboxes, radio buttons, and their visual rendering and interaction behavior.
 * These controls form the foundation of Mac application user interfaces.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "StandardControls.h"
#include "ControlManager.h"
#include "ControlDrawing.h"
#include "ControlTypes.h"
#include "../QuickDraw/QuickDraw.h"
#include "../FontManager/FontManager.h"
#include "../MacTypes.h"
#include <string.h>
#include <stdlib.h>

/* Standard control dimensions */
#define BUTTON_HEIGHT       20
#define BUTTON_MIN_WIDTH    59
#define BUTTON_MARGIN       8
#define CHECKBOX_SIZE       12
#define RADIO_SIZE          12
#define CONTROL_TEXT_MARGIN 4

/* Button data structure */
typedef struct ButtonData {
    bool isDefault;          /* Default button flag */
    bool isPushed;           /* Currently pushed state */
    int16_t insetLevel;      /* Button inset level */
    Rect contentRect;        /* Content area rectangle */
    RgnHandle buttonRegion;  /* Button region for hit testing */
} ButtonData;

/* Checkbox/Radio data structure */
typedef struct CheckboxData {
    bool isRadio;            /* Radio button vs checkbox */
    bool isMixed;            /* Mixed state (for checkboxes) */
    int16_t groupID;         /* Radio button group ID */
    Rect boxRect;            /* Checkbox/radio box rectangle */
    Rect textRect;           /* Text area rectangle */
} CheckboxData;

/* Internal function prototypes */
static void DrawButtonFrame(ControlHandle button, bool pushed);
static void DrawButtonText(ControlHandle button, bool pushed);
static void DrawDefaultButtonOutline(ControlHandle button);
static void DrawCheckboxMark(ControlHandle checkbox);
static void DrawRadioMark(ControlHandle radio);
static void CalculateButtonRects(ControlHandle button);
static void CalculateCheckboxRects(ControlHandle checkbox);
static void HandleRadioGroup(ControlHandle radio);
static int16_t TestButtonPart(ControlHandle button, Point pt);
static int16_t TestCheckboxPart(ControlHandle checkbox, Point pt);

/**
 * Register standard control types
 */
void RegisterStandardControlTypes(void) {
    RegisterControlType(pushButProc, ButtonCDEF);
    RegisterControlType(checkBoxProc, CheckboxCDEF);
    RegisterControlType(radioButProc, RadioButtonCDEF);
}

/**
 * Button control definition procedure
 */
int32_t ButtonCDEF(int16_t varCode, ControlHandle theControl,
                   int16_t message, int32_t param) {
    ButtonData *buttonData;
    Rect bounds;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Allocate button data */
        (*theControl)->contrlData = NewHandleClear(sizeof(ButtonData));
        if ((*theControl)->contrlData) {
            buttonData = (ButtonData *)*(*theControl)->contrlData;
            buttonData->isDefault = (varCode & 1) != 0;
            buttonData->insetLevel = 2;
            buttonData->isPushed = false;

            /* Create button region */
            buttonData->buttonRegion = NewRgn();

            /* Calculate button rectangles */
            CalculateButtonRects(theControl);
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
                PenPat(&gray);
                PenMode(patBic);
                PaintRect(&bounds);
                PenMode(patCopy);
                PenPat(&black);
            }
        }
        break;

    case testCntl:
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
int32_t CheckboxCDEF(int16_t varCode, ControlHandle theControl,
                     int16_t message, int32_t param) {
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
                PenPat(&gray);
                PenMode(patBic);
                PaintRect(&bounds);
                PenMode(patCopy);
                PenPat(&black);
            }

            /* Highlight if tracking */
            if ((*theControl)->contrlHilite == inCheckBox) {
                InvertRect(&checkData->boxRect);
            }
        }
        break;

    case testCntl:
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
int32_t RadioButtonCDEF(int16_t varCode, ControlHandle theControl,
                        int16_t message, int32_t param) {
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
                PenPat(&gray);
                PenMode(patBic);
                PaintRect(&bounds);
                PenMode(patCopy);
                PenPat(&black);
            }

            /* Highlight if tracking */
            if ((*theControl)->contrlHilite == inCheckBox) {
                InvertOval(&radioData->boxRect);
            }
        }
        break;

    case testCntl:
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
static void DrawButtonFrame(ControlHandle button, bool pushed) {
    ButtonData *buttonData;
    Rect frameRect;
    int16_t inset;

    if (!button || !(*button)->contrlData) {
        return;
    }

    buttonData = (ButtonData *)*(*button)->contrlData;
    frameRect = (*button)->contrlRect;
    inset = pushed ? 1 : 0;

    /* Adjust frame for default button */
    if (buttonData->isDefault) {
        InsetRect(&frameRect, 3, 3);
    }

    /* Draw 3D button frame */
    if (pushed) {
        /* Pushed state - dark frame */
        PenPat(&black);
        FrameRect(&frameRect);
        InsetRect(&frameRect, 1, 1);
        PenPat(&dkGray);
        FrameRect(&frameRect);
    } else {
        /* Normal state - raised frame */
        /* Top and left highlight */
        PenPat(&white);
        MoveTo(frameRect.left, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.top);

        /* Bottom and right shadow */
        PenPat(&black);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);

        /* Inner shadow */
        InsetRect(&frameRect, 1, 1);
        PenPat(&dkGray);
        MoveTo(frameRect.right - 1, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);
    }

    /* Fill button interior */
    InsetRect(&frameRect, 1, 1);
    PenPat(&ltGray);
    PaintRect(&frameRect);
    PenPat(&black);
}

/**
 * Draw button text
 */
static void DrawButtonText(ControlHandle button, bool pushed) {
    ButtonData *buttonData;
    Rect textRect;
    int16_t offset = pushed ? 1 : 0;
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
    int16_t textWidth = StringWidth((*button)->contrlTitle);
    int16_t textHeight = fontInfo.ascent + fontInfo.descent;

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
    PenPat(&black);
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
    int16_t boxSize;

    if (!checkbox || !(*checkbox)->contrlData) {
        return;
    }

    checkData = (CheckboxData *)*(*checkbox)->contrlData;
    bounds = (*checkbox)->contrlRect;
    boxSize = checkData->isRadio ? RADIO_SIZE : CHECKBOX_SIZE;

    /* Box rectangle (left side) */
    checkData->boxRect = bounds;
    checkData->boxRect.right = checkData->boxRect.left + boxSize;
    checkData->boxRect.bottom = checkData->boxRect.top + boxSize;

    /* Center vertically if control is taller than box */
    if ((bounds.bottom - bounds.top) > boxSize) {
        int16_t offset = ((bounds.bottom - bounds.top) - boxSize) / 2;
        OffsetRect(&checkData->boxRect, 0, offset);
    }

    /* Text rectangle (right side) */
    checkData->textRect = bounds;
    checkData->textRect.left = checkData->boxRect.right + CONTROL_TEXT_MARGIN;
}

/**
 * Test button part hit
 */
static int16_t TestButtonPart(ControlHandle button, Point pt) {
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
static int16_t TestCheckboxPart(ControlHandle checkbox, Point pt) {
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
bool IsButtonControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    int16_t procID = GetControlVariant(control) & 0xFFF0;
    return (procID == pushButProc);
}

/**
 * Check if control is a checkbox
 */
bool IsCheckboxControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    int16_t procID = GetControlVariant(control) & 0xFFF0;
    return (procID == checkBoxProc);
}

/**
 * Check if control is a radio button
 */
bool IsRadioControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    int16_t procID = GetControlVariant(control) & 0xFFF0;
    return (procID == radioButProc);
}

/**
 * Set checkbox mixed state
 */
void SetCheckboxMixed(ControlHandle checkbox, bool mixed) {
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
bool GetCheckboxMixed(ControlHandle checkbox) {
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
void SetRadioGroup(ControlHandle radio, int16_t groupID) {
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
int16_t GetRadioGroup(ControlHandle radio) {
    CheckboxData *radioData;

    if (!radio || !IsRadioControl(radio) || !(*radio)->contrlData) {
        return 0;
    }

    radioData = (CheckboxData *)*(*radio)->contrlData;
    return radioData->groupID;
}
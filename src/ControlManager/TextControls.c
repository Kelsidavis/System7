/* #include "SystemTypes.h" */
/**
 * @file TextControls.c
 * @brief Text control implementations (edit text and static text)
 *
 * This file implements text controls including editable text fields and static
 * text displays. These controls provide text input and display capabilities
 * essential for user interfaces, forms, and information display.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
/* TextControls.h local */
#include "ControlManager/ControlManager.h"
/* ControlDrawing.h not needed */
#include "TextEdit/TextEdit.h"
#include "QuickDraw/QuickDraw.h"
#include "FontManager/FontManager.h"
#include "EventManager/EventManager.h"
#include "SystemTypes.h"


/* Text control type constants */
#define editTextProc        64      /* Editable text control */
#define staticTextProc      65      /* Static text control */

/* Text control dimensions */
#define TEXT_MARGIN         4       /* Text margin from border */
#define EDIT_FRAME_WIDTH    1       /* Edit text frame width */
#define CURSOR_WIDTH        1       /* Text cursor width */
#define BLINK_RATE          30      /* Cursor blink rate in ticks */

/* Edit text data structure */
typedef struct EditTextData {
    TEHandle textEdit;              /* TextEdit record */
    Handle textHandle;              /* Text storage */
    Rect textRect;                  /* Text display rectangle */
    Rect frameRect;                 /* Frame rectangle */
    Boolean isActive;                  /* Currently active */
    Boolean isPassword;                /* Password field */
    char passwordChar;              /* Password display character */
    SInt16 maxLength;              /* Maximum text length */

    /* Validation */
    TextValidationProcPtr validator; /* Validation procedure */
    SInt32 validationRefCon;       /* Validation reference */

    /* Visual properties */
    SInt16 textAlign;              /* Text alignment */
    SInt16 textStyle;              /* Text style flags */
    RGBColor textColor;             /* Text color */
    RGBColor backgroundColor;       /* Background color */

    /* Cursor and selection */
    Boolean cursorVisible;             /* Cursor visibility */
    UInt32 lastBlinkTime;         /* Last cursor blink time */
    SInt16 selStart;               /* Selection start */
    SInt16 selEnd;                 /* Selection end */

} EditTextData;

/* Static text data structure */
typedef struct StaticTextData {
    Handle textHandle;              /* Text storage */
    Rect textRect;                  /* Text display rectangle */
    SInt16 textAlign;              /* Text alignment */
    SInt16 textStyle;              /* Text style flags */
    RGBColor textColor;             /* Text color */
    Boolean autoSize;                  /* Auto-size to fit text */
    Boolean wordWrap;                  /* Word wrap enabled */

} StaticTextData;

/* Internal function prototypes */
static void InitializeEditText(ControlHandle control);
static void InitializeStaticText(ControlHandle control);
static void DrawEditTextFrame(ControlHandle control);
static void DrawEditTextContent(ControlHandle control);
static void DrawStaticTextContent(ControlHandle control);
static void UpdateTextCursor(ControlHandle control);
static void HandleEditTextClick(ControlHandle control, Point pt);
static void HandleEditTextKey(ControlHandle control, SInt16 keyCode, SInt16 modifiers);
static Boolean ValidateEditText(ControlHandle control, const char *text);
static void CalculateTextRect(ControlHandle control);
static void AutoSizeStaticText(ControlHandle control);

/**
 * Register text control types
 */
void RegisterTextControlTypes(void) {
    RegisterControlType(editTextProc, EditTextCDEF);
    RegisterControlType(staticTextProc, StaticTextCDEF);
}

/**
 * Edit text control definition procedure
 */
SInt32 EditTextCDEF(SInt16 varCode, ControlHandle theControl,
                     SInt16 message, SInt32 param) {
    EditTextData *editData;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Initialize edit text control */
        InitializeEditText(theControl);
        break;

    case dispCntl:
        /* Dispose edit text control */
        if ((*theControl)->contrlData) {
            editData = (EditTextData *)*(*theControl)->contrlData;

            if (editData->textEdit) {
                TEDispose(editData->textEdit);
            }
            if (editData->textHandle) {
                DisposeHandle(editData->textHandle);
            }

            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw edit text control */
        DrawEditTextFrame(theControl);
        DrawEditTextContent(theControl);
        UpdateTextCursor(theControl);
        break;

    case testCntl:
        /* Test if point is in edit text */
        pt = *(Point *)&param;
        if (PtInRect(pt, &(*theControl)->contrlRect)) {
            return inButton; /* Use button part code for text controls */
        }
        return 0;

    case calcCRgns:
    case calcCntlRgn:
        /* Calculate text regions */
        CalculateTextRect(theControl);
        break;

    case posCntl:
        /* Handle position/value changes */
        CalculateTextRect(theControl);
        break;

    case autoTrack:
        /* Handle mouse tracking */
        if ((*theControl)->contrlData) {
            pt = *(Point *)&param;
            HandleEditTextClick(theControl, pt);
        }
        break;
    }

    return 0;
}

/**
 * Static text control definition procedure
 */
SInt32 StaticTextCDEF(SInt16 varCode, ControlHandle theControl,
                       SInt16 message, SInt32 param) {
    StaticTextData *staticData;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Initialize static text control */
        InitializeStaticText(theControl);
        break;

    case dispCntl:
        /* Dispose static text control */
        if ((*theControl)->contrlData) {
            staticData = (StaticTextData *)*(*theControl)->contrlData;

            if (staticData->textHandle) {
                DisposeHandle(staticData->textHandle);
            }

            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw static text control */
        DrawStaticTextContent(theControl);
        break;

    case testCntl:
        /* Static text is not interactive */
        return 0;

    case calcCRgns:
    case calcCntlRgn:
        /* Calculate text regions */
        CalculateTextRect(theControl);
        if ((*theControl)->contrlData) {
            staticData = (StaticTextData *)*(*theControl)->contrlData;
            if (staticData->autoSize) {
                AutoSizeStaticText(theControl);
            }
        }
        break;

    case posCntl:
        /* Handle position changes */
        CalculateTextRect(theControl);
        break;
    }

    return 0;
}

/**
 * Create new edit text control
 */
ControlHandle NewEditTextControl(WindowPtr window, const Rect *bounds,
                                ConstStr255Param text, Boolean visible,
                                SInt16 maxLength, SInt32 refCon) {
    ControlHandle control;
    EditTextData *editData;

    control = NewControl(window, bounds, text, visible, 0, 0, 1,
                        editTextProc, refCon);
    if (!control) {
        return NULL;
    }

    /* Set maximum length */
    if ((*control)->contrlData) {
        editData = (EditTextData *)*(*control)->contrlData;
        editData->maxLength = maxLength;
    }

    return control;
}

/**
 * Create new static text control
 */
ControlHandle NewStaticTextControl(WindowPtr window, const Rect *bounds,
                                  ConstStr255Param text, Boolean visible,
                                  SInt16 alignment, SInt32 refCon) {
    ControlHandle control;
    StaticTextData *staticData;

    control = NewControl(window, bounds, text, visible, 0, 0, 1,
                        staticTextProc, refCon);
    if (!control) {
        return NULL;
    }

    /* Set text alignment */
    if ((*control)->contrlData) {
        staticData = (StaticTextData *)*(*control)->contrlData;
        staticData->textAlign = alignment;
    }

    return control;
}

/**
 * Set text control text
 */
void SetTextControlText(ControlHandle control, ConstStr255Param text) {
    EditTextData *editData;
    StaticTextData *staticData;
    Handle textHandle;
    SInt16 textLen;

    if (!control || !text) {
        return;
    }

    textLen = text[0];

    if (IsEditTextControl(control)) {
        editData = (EditTextData *)*(*control)->contrlData;
        if (!editData) return;

        /* Update TextEdit record */
        if (editData->textEdit) {
            TESetText((Ptr)(text + 1), textLen, editData->textEdit);
        }

        /* Update text handle */
        if (editData->textHandle) {
            SetHandleSize(editData->textHandle, textLen + 1);
            if (MemError() == noErr) {
                BlockMove(text, *editData->textHandle, textLen + 1);
            }
        }
    } else if (IsStaticTextControl(control)) {
        staticData = (StaticTextData *)*(*control)->contrlData;
        if (!staticData) return;

        /* Update text handle */
        if (staticData->textHandle) {
            SetHandleSize(staticData->textHandle, textLen + 1);
            if (MemError() == noErr) {
                BlockMove(text, *staticData->textHandle, textLen + 1);
            }
        }

        /* Auto-size if enabled */
        if (staticData->autoSize) {
            AutoSizeStaticText(control);
        }
    }

    /* Update control title */
    SetControlTitle(control, text);

    /* Redraw if visible */
    if ((*control)->contrlVis) {
        Draw1Control(control);
    }
}

/**
 * Get text control text
 */
void GetTextControlText(ControlHandle control, Str255 text) {
    EditTextData *editData;
    StaticTextData *staticData;

    if (!control || !text) {
        text[0] = 0;
        return;
    }

    if (IsEditTextControl(control)) {
        editData = (EditTextData *)*(*control)->contrlData;
        if (editData && editData->textHandle) {
            BlockMove(*editData->textHandle, text,
                     (*editData->textHandle)[0] + 1);
        } else {
            GetControlTitle(control, text);
        }
    } else if (IsStaticTextControl(control)) {
        staticData = (StaticTextData *)*(*control)->contrlData;
        if (staticData && staticData->textHandle) {
            BlockMove(*staticData->textHandle, text,
                     (*staticData->textHandle)[0] + 1);
        } else {
            GetControlTitle(control, text);
        }
    } else {
        GetControlTitle(control, text);
    }
}

/**
 * Set edit text password mode
 */
void SetEditTextPassword(ControlHandle control, Boolean isPassword, char passwordChar) {
    EditTextData *editData;

    if (!control || !IsEditTextControl(control)) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;
    if (editData) {
        editData->isPassword = isPassword;
        editData->passwordChar = passwordChar ? passwordChar : '•';

        /* Redraw if visible */
        if ((*control)->contrlVis) {
            Draw1Control(control);
        }
    }
}

/**
 * Set text validation procedure
 */
void SetTextValidation(ControlHandle control, TextValidationProcPtr validator,
                      SInt32 refCon) {
    EditTextData *editData;

    if (!control || !IsEditTextControl(control)) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;
    if (editData) {
        editData->validator = validator;
        editData->validationRefCon = refCon;
    }
}

/**
 * Activate edit text control
 */
void ActivateEditText(ControlHandle control) {
    EditTextData *editData;

    if (!control || !IsEditTextControl(control)) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;
    if (editData && !editData->isActive) {
        editData->isActive = true;
        editData->cursorVisible = true;
        editData->lastBlinkTime = TickCount();

        if (editData->textEdit) {
            TEActivate(editData->textEdit);
        }

        /* Redraw to show cursor */
        if ((*control)->contrlVis) {
            Draw1Control(control);
        }
    }
}

/**
 * Deactivate edit text control
 */
void DeactivateEditText(ControlHandle control) {
    EditTextData *editData;

    if (!control || !IsEditTextControl(control)) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;
    if (editData && editData->isActive) {
        editData->isActive = false;
        editData->cursorVisible = false;

        if (editData->textEdit) {
            TEDeactivate(editData->textEdit);
        }

        /* Redraw to hide cursor */
        if ((*control)->contrlVis) {
            Draw1Control(control);
        }
    }
}

/* Internal Implementation */

/**
 * Initialize edit text control
 */
static void InitializeEditText(ControlHandle control) {
    EditTextData *editData;
    Rect bounds;

    (*control)->contrlData = NewHandleClear(sizeof(EditTextData));
    if (!(*control)->contrlData) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;
    bounds = (*control)->contrlRect;

    /* Initialize text edit */
    CalculateTextRect(control);
    editData->textEdit = TENew(&editData->textRect, &editData->textRect);

    /* Initialize text storage */
    editData->textHandle = NewHandle(256); /* Default size */
    if (!editData->textHandle) {
        CTRL_LOG_WARN("InitializeEditText: Failed to allocate text handle\n");
        return;
    }

    /* Set defaults */
    editData->isActive = false;
    editData->isPassword = false;
    editData->passwordChar = '•';
    editData->maxLength = 255;
    editData->textAlign = teFlushLeft;
    editData->textStyle = normal;
    (editData)->red = 0;
    (editData)->green = 0;
    (editData)->blue = 0;
    (editData)->red = 0xFFFF;
    (editData)->green = 0xFFFF;
    (editData)->blue = 0xFFFF;
    editData->cursorVisible = false;

    /* Set initial text */
    if ((*control)->contrlTitle[0] > 0) {
        SetTextControlText(control, (*control)->contrlTitle);
    }
}

/**
 * Initialize static text control
 */
static void InitializeStaticText(ControlHandle control) {
    StaticTextData *staticData;

    (*control)->contrlData = NewHandleClear(sizeof(StaticTextData));
    if (!(*control)->contrlData) {
        return;
    }

    staticData = (StaticTextData *)*(*control)->contrlData;

    /* Initialize text storage */
    staticData->textHandle = NewHandle(256); /* Default size */
    if (!staticData->textHandle) {
        CTRL_LOG_WARN("InitializeStaticText: Failed to allocate text handle\n");
        return;
    }

    /* Set defaults */
    staticData->textAlign = teFlushLeft;
    staticData->textStyle = normal;
    (staticData)->red = 0;
    (staticData)->green = 0;
    (staticData)->blue = 0;
    staticData->autoSize = false;
    staticData->wordWrap = true;

    /* Calculate text rectangle */
    CalculateTextRect(control);

    /* Set initial text */
    if ((*control)->contrlTitle[0] > 0) {
        SetTextControlText(control, (*control)->contrlTitle);
    }
}

/**
 * Draw edit text frame
 */
static void DrawEditTextFrame(ControlHandle control) {
    EditTextData *editData;
    Rect frameRect;

    if (!control || !(*control)->contrlData) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;
    frameRect = editData->frameRect;

    /* Draw frame */
    if (editData->isActive) {
        /* Active frame - thicker border */
        PenSize(2, 2);
        PenPat(&black);
        FrameRect(&frameRect);
        PenSize(1, 1);
    } else {
        /* Inactive frame */
        PenPat(&black);
        FrameRect(&frameRect);
    }

    /* Fill background */
    InsetRect(&frameRect, EDIT_FRAME_WIDTH, EDIT_FRAME_WIDTH);
    RGBForeColor(&editData->backgroundColor);
    PaintRect(&frameRect);
    ForeColor(blackColor);
}

/**
 * Draw edit text content
 */
static void DrawEditTextContent(ControlHandle control) {
    EditTextData *editData;
    Str255 displayText;
    SInt16 textLen, i;

    if (!control || !(*control)->contrlData) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;

    /* Get text to display */
    GetTextControlText(control, displayText);
    textLen = displayText[0];

    /* Convert to password characters if needed */
    if (editData->isPassword && textLen > 0) {
        for (i = 1; i <= textLen; i++) {
            displayText[i] = editData->passwordChar;
        }
    }

    /* Set text color */
    RGBForeColor(&editData->textColor);

    /* Draw text using TextEdit if available */
    if (editData->textEdit) {
        TEUpdate(&editData->textRect, editData->textEdit);
    } else {
        /* Fallback text drawing */
        MoveTo((editData)->left,
               (editData)->top + 12); /* Approximate baseline */
        DrawString(displayText);
    }

    ForeColor(blackColor);
}

/**
 * Draw static text content
 */
static void DrawStaticTextContent(ControlHandle control) {
    StaticTextData *staticData;
    Str255 text;
    FontInfo fontInfo;
    SInt16 textWidth, textHeight;
    SInt16 h, v;

    if (!control || !(*control)->contrlData) {
        return;
    }

    staticData = (StaticTextData *)*(*control)->contrlData;

    /* Get text to display */
    GetTextControlText(control, text);
    if (text[0] == 0) {
        return;
    }

    /* Set text color */
    RGBForeColor(&staticData->textColor);

    /* Get font metrics */
    GetFontInfo(&fontInfo);
    textWidth = StringWidth(text);
    textHeight = fontInfo.ascent + fontInfo.descent;

    /* Calculate position based on alignment */
    switch (staticData->textAlign) {
    case teFlushLeft:
        h = (staticData)->left;
        break;
    case teCenter:
        h = (staticData)->left +
            ((staticData)->right - (staticData)->left - textWidth) / 2;
        break;
    case teFlushRight:
        h = (staticData)->right - textWidth;
        break;
    default:
        h = (staticData)->left;
        break;
    }

    v = (staticData)->top + fontInfo.ascent;

    /* Draw text */
    MoveTo(h, v);
    DrawString(text);

    ForeColor(blackColor);
}

/**
 * Update text cursor
 */
static void UpdateTextCursor(ControlHandle control) {
    EditTextData *editData;
    UInt32 currentTime;

    if (!control || !(*control)->contrlData) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;

    if (!editData->isActive) {
        return;
    }

    currentTime = TickCount();

    /* Handle cursor blinking */
    if (currentTime - editData->lastBlinkTime >= BLINK_RATE) {
        editData->cursorVisible = !editData->cursorVisible;
        editData->lastBlinkTime = currentTime;

        /* Redraw cursor area */
        if (editData->textEdit) {
            TEIdle(editData->textEdit);
        }
    }
}

/**
 * Handle edit text click
 */
static void HandleEditTextClick(ControlHandle control, Point pt) {
    EditTextData *editData;

    if (!control || !(*control)->contrlData) {
        return;
    }

    editData = (EditTextData *)*(*control)->contrlData;

    /* Activate if not already active */
    if (!editData->isActive) {
        ActivateEditText(control);
    }

    /* Handle click in TextEdit */
    if (editData->textEdit) {
        TEClick(pt, false, editData->textEdit);
    }
}

/**
 * Calculate text rectangle
 */
static void CalculateTextRect(ControlHandle control) {
    EditTextData *editData;
    StaticTextData *staticData;
    Rect bounds;

    if (!control || !(*control)->contrlData) {
        return;
    }

    bounds = (*control)->contrlRect;

    if (IsEditTextControl(control)) {
        editData = (EditTextData *)*(*control)->contrlData;

        /* Frame rectangle */
        editData->frameRect = bounds;

        /* Text rectangle (inside frame) */
        editData->textRect = bounds;
        InsetRect(&editData->textRect, TEXT_MARGIN, TEXT_MARGIN);

    } else if (IsStaticTextControl(control)) {
        staticData = (StaticTextData *)*(*control)->contrlData;

        /* Text rectangle */
        staticData->textRect = bounds;
        InsetRect(&staticData->textRect, TEXT_MARGIN, TEXT_MARGIN);
    }
}

/**
 * Auto-size static text control
 */
static void AutoSizeStaticText(ControlHandle control) {
    StaticTextData *staticData;
    Str255 text;
    FontInfo fontInfo;
    SInt16 textWidth, textHeight;
    Rect newBounds;

    if (!control || !IsStaticTextControl(control) || !(*control)->contrlData) {
        return;
    }

    staticData = (StaticTextData *)*(*control)->contrlData;
    GetTextControlText(control, text);

    if (text[0] == 0) {
        return;
    }

    /* Get text dimensions */
    GetFontInfo(&fontInfo);
    textWidth = StringWidth(text);
    textHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;

    /* Calculate new bounds */
    newBounds = (*control)->contrlRect;
    newBounds.right = newBounds.left + textWidth + (2 * TEXT_MARGIN);
    newBounds.bottom = newBounds.top + textHeight + (2 * TEXT_MARGIN);

    /* Update control bounds */
    (*control)->contrlRect = newBounds;
    CalculateTextRect(control);
}

/**
 * Check if control is edit text
 */
Boolean IsEditTextControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    SInt16 procID = GetControlVariant(control) & 0xFFF0;
    return (procID == editTextProc);
}

/**
 * Check if control is static text
 */
Boolean IsStaticTextControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    SInt16 procID = GetControlVariant(control) & 0xFFF0;
    return (procID == staticTextProc);
}

/*
 * System 7.1 Calculator Desk Accessory - Portable Implementation
 * Based on original System 7 Calculator DRVR resource
 *
 * This provides equivalent functionality to the original desk accessory
 * with cross-platform support through HAL abstraction.
 */

#include "calculator.h"
#include "Calculator_HAL.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Calculator state */
static CalcState gState;
static WindowPtr gWindow;
static Boolean gActive;

/* Button definitions - matching original layout */
static CalcButton gButtons[] = {
    /* Row 1 - Memory and Clear */
    {{8, 32, 38, 56},   "C",  kActionClear,        false},
    {{46, 32, 76, 56},  "MC", kActionMemoryClear,  false},
    {{84, 32, 114, 56}, "MR", kActionMemoryRecall, false},
    {{122, 32, 152, 56},"M+", kActionMemoryAdd,    false},
    {{160, 32, 190, 56},"M-", kActionMemorySubtract,false},

    /* Row 2 - 7,8,9,/ */
    {{8, 64, 38, 88},   "7",  kActionDigit7,       false},
    {{46, 64, 76, 88},  "8",  kActionDigit8,       false},
    {{84, 64, 114, 88}, "9",  kActionDigit9,       false},
    {{122, 64, 152, 88},"/",  kActionDivide,       false},
    {{160, 64, 190, 88},"%",  kActionPercent,      false},

    /* Row 3 - 4,5,6,* */
    {{8, 96, 38, 120},  "4",  kActionDigit4,       false},
    {{46, 96, 76, 120}, "5",  kActionDigit5,       false},
    {{84, 96, 114, 120},"6",  kActionDigit6,       false},
    {{122, 96, 152, 120},"*", kActionMultiply,     false},
    {{160, 96, 190, 120},"±", kActionChangeSign,   false},

    /* Row 4 - 1,2,3,- */
    {{8, 128, 38, 152}, "1",  kActionDigit1,       false},
    {{46, 128, 76, 152},"2",  kActionDigit2,       false},
    {{84, 128, 114, 152},"3", kActionDigit3,       false},
    {{122, 128, 152, 152},"-",kActionSubtract,     false},

    /* Row 5 - 0,.,=,+ */
    {{8, 160, 76, 184}, "0",  kActionDigit0,       false},  /* Double width */
    {{84, 160, 114, 184},".", kActionDecimal,      false},
    {{122, 160, 152, 184},"=",kActionEquals,       false},
    {{160, 160, 190, 184},"+",kActionAdd,          false}
};

#define NUM_BUTTONS (sizeof(gButtons)/sizeof(CalcButton))

/* Initialize calculator */
OSErr InitCalculator(void) {
    Rect windowBounds;

    /* Set up window rectangle */
    SetRect(&windowBounds, 100, 50,
            100 + CALC_WINDOW_WIDTH,
            50 + CALC_WINDOW_HEIGHT);

    /* Create window through HAL */
    gWindow = Calculator_HAL_CreateWindow(&windowBounds, "\pCalculator");

    if (!gWindow) return memFullErr;

    /* Initialize state */
    gState.accumulator = 0.0;
    gState.memory = 0.0;
    gState.operand = 0.0;
    gState.pendingOp = kActionNone;
    gState.clearOnNext = true;
    gState.hasDecimal = false;
    gState.decimalPlaces = 0;
    gState.error = false;
    gActive = true;

    /* Set initial display */
    strcpy(gState.displayStr, "0");

    /* Initialize HAL */
    Calculator_HAL_Init();

    /* Draw calculator */
    DrawCalculator();

    return noErr;
}

/* Handle calculator events */
void HandleCalculatorEvent(EventRecord *event) {
    WindowPtr whichWindow;
    short part;
    Point localPt;

    switch (event->what) {
        case mouseDown:
            part = FindWindow(event->where, &whichWindow);
            if (whichWindow == gWindow) {
                switch (part) {
                    case inContent:
                        if (whichWindow != FrontWindow()) {
                            SelectWindow(whichWindow);
                        } else {
                            localPt = event->where;
                            GlobalToLocal(&localPt);
                            HandleMouseDown(localPt);
                        }
                        break;

                    case inDrag:
                        Calculator_HAL_DragWindow(whichWindow, event->where);
                        break;

                    case inGoAway:
                        if (Calculator_HAL_TrackGoAway(whichWindow, event->where)) {
                            CloseCalculator();
                        }
                        break;
                }
            }
            break;

        case keyDown:
        case autoKey:
            HandleKeyDown(event->message & charCodeMask);
            break;

        case updateEvt:
            if ((WindowPtr)event->message == gWindow) {
                HandleUpdate();
            }
            break;

        case activateEvt:
            gActive = (event->modifiers & activeFlag) != 0;
            break;
    }
}

/* Close calculator */
void CloseCalculator(void) {
    if (gWindow) {
        Calculator_HAL_DisposeWindow(gWindow);
        gWindow = NULL;
    }
    Calculator_HAL_Cleanup();
}

/* Handle mouse clicks in window */
void HandleMouseDown(Point where) {
    short buttonIndex;

    buttonIndex = FindButton(where);
    if (buttonIndex >= 0) {
        /* Hilite button briefly */
        gButtons[buttonIndex].hilited = true;
        DrawButton(&gButtons[buttonIndex]);

        /* Small delay for visual feedback */
        Calculator_HAL_Delay(8);

        gButtons[buttonIndex].hilited = false;
        DrawButton(&gButtons[buttonIndex]);

        /* Process button click */
        HandleButtonClick(buttonIndex);
    }
}

/* Find which button contains point */
short FindButton(Point pt) {
    short i;

    for (i = 0; i < NUM_BUTTONS; i++) {
        if (PtInRect(pt, &gButtons[i].bounds)) {
            return i;
        }
    }
    return -1;
}

/* Handle keyboard input */
void HandleKeyDown(char key) {
    switch (key) {
        case '0': HandleDigit(0); break;
        case '1': HandleDigit(1); break;
        case '2': HandleDigit(2); break;
        case '3': HandleDigit(3); break;
        case '4': HandleDigit(4); break;
        case '5': HandleDigit(5); break;
        case '6': HandleDigit(6); break;
        case '7': HandleDigit(7); break;
        case '8': HandleDigit(8); break;
        case '9': HandleDigit(9); break;
        case '.': HandleButtonClick(21); break; /* Decimal */
        case '+': HandleOperator(kActionAdd); break;
        case '-': HandleOperator(kActionSubtract); break;
        case '*': HandleOperator(kActionMultiply); break;
        case '/': HandleOperator(kActionDivide); break;
        case '=':
        case '\r': HandleOperator(kActionEquals); break;
        case 'c':
        case 'C': ClearCalculator(); break;
        case '%': HandleOperator(kActionPercent); break;
    }
}

/* Handle window update */
void HandleUpdate(void) {
    BeginUpdate(gWindow);
    DrawCalculator();
    EndUpdate(gWindow);
}

/* Draw entire calculator */
void DrawCalculator(void) {
    short i;

    Calculator_HAL_BeginDrawing(gWindow);

    /* Clear window */
    Calculator_HAL_ClearWindow();

    /* Draw display */
    DrawDisplay();

    /* Draw all buttons */
    for (i = 0; i < NUM_BUTTONS; i++) {
        DrawButton(&gButtons[i]);
    }

    Calculator_HAL_EndDrawing();
}

/* Draw calculator display */
void DrawDisplay(void) {
    Rect displayRect;

    SetRect(&displayRect, 8, 8, CALC_WINDOW_WIDTH - 8, 8 + DISPLAY_HEIGHT);

    /* Draw display background */
    Calculator_HAL_DrawDisplayBackground(&displayRect);

    /* Draw text right-aligned */
    Calculator_HAL_DrawDisplayText(gState.displayStr, &displayRect);
}

/* Draw a button */
void DrawButton(CalcButton *button) {
    Calculator_HAL_DrawButton(button);
}

/* Handle button click by index */
void HandleButtonClick(short buttonIndex) {
    CalcButton *button = &gButtons[buttonIndex];

    switch (button->action) {
        case kActionDigit0:
        case kActionDigit1:
        case kActionDigit2:
        case kActionDigit3:
        case kActionDigit4:
        case kActionDigit5:
        case kActionDigit6:
        case kActionDigit7:
        case kActionDigit8:
        case kActionDigit9:
            HandleDigit(button->action - kActionDigit0);
            break;

        case kActionClear:
            ClearCalculator();
            break;

        case kActionDecimal:
            if (!gState.hasDecimal && !gState.error) {
                gState.hasDecimal = true;
                if (gState.clearOnNext) {
                    strcpy(gState.displayStr, "0.");
                    gState.clearOnNext = false;
                } else {
                    strcat(gState.displayStr, ".");
                }
                UpdateDisplay();
            }
            break;

        case kActionAdd:
        case kActionSubtract:
        case kActionMultiply:
        case kActionDivide:
        case kActionEquals:
        case kActionPercent:
            HandleOperator(button->action);
            break;

        case kActionMemoryClear:
            gState.memory = 0.0;
            break;

        case kActionMemoryRecall:
            if (!gState.error) {
                gState.operand = gState.memory;
                DoubleToString(gState.operand, gState.displayStr);
                gState.clearOnNext = true;
                UpdateDisplay();
            }
            break;

        case kActionMemoryAdd:
            if (!gState.error) {
                gState.memory += gState.operand;
            }
            break;

        case kActionMemorySubtract:
            if (!gState.error) {
                gState.memory -= gState.operand;
            }
            break;

        case kActionChangeSign:
            if (!gState.error) {
                gState.operand = -gState.operand;
                DoubleToString(gState.operand, gState.displayStr);
                UpdateDisplay();
            }
            break;
    }
}

/* Handle digit input */
void HandleDigit(short digit) {
    char digitChar;

    if (gState.error) {
        ClearCalculator();
    }

    if (gState.clearOnNext) {
        strcpy(gState.displayStr, "0");
        gState.clearOnNext = false;
        gState.hasDecimal = false;
        gState.decimalPlaces = 0;
    }

    /* Don't add more digits if display is full */
    if (strlen(gState.displayStr) >= MAX_DIGITS) {
        return;
    }

    /* Don't add leading zeros */
    if (strcmp(gState.displayStr, "0") == 0 && !gState.hasDecimal) {
        gState.displayStr[0] = '0' + digit;
    } else {
        digitChar = '0' + digit;
        strncat(gState.displayStr, &digitChar, 1);
    }

    if (gState.hasDecimal) {
        gState.decimalPlaces++;
    }

    /* Update operand value */
    gState.operand = StringToDouble(gState.displayStr);

    UpdateDisplay();
}

/* Handle operator */
void HandleOperator(short op) {
    if (gState.error && op != kActionClear) {
        return;
    }

    /* If there's a pending operation, perform it first */
    if (gState.pendingOp != kActionNone && gState.pendingOp != kActionEquals) {
        PerformCalculation();
    }

    if (op == kActionPercent) {
        gState.operand = gState.operand / 100.0;
        DoubleToString(gState.operand, gState.displayStr);
        UpdateDisplay();
        gState.clearOnNext = true;
    } else if (op != kActionEquals) {
        gState.accumulator = gState.operand;
        gState.pendingOp = op;
        gState.clearOnNext = true;
    } else {
        if (gState.pendingOp != kActionNone && gState.pendingOp != kActionEquals) {
            PerformCalculation();
        }
        gState.pendingOp = kActionNone;
        gState.clearOnNext = true;
    }
}

/* Perform pending calculation */
void PerformCalculation(void) {
    double result = 0.0;

    switch (gState.pendingOp) {
        case kActionAdd:
            result = gState.accumulator + gState.operand;
            break;

        case kActionSubtract:
            result = gState.accumulator - gState.operand;
            break;

        case kActionMultiply:
            result = gState.accumulator * gState.operand;
            break;

        case kActionDivide:
            if (gState.operand == 0.0) {
                SetError("Error");
                return;
            }
            result = gState.accumulator / gState.operand;
            break;

        default:
            result = gState.operand;
            break;
    }

    gState.operand = result;
    DoubleToString(result, gState.displayStr);
    UpdateDisplay();
}

/* Clear calculator state */
void ClearCalculator(void) {
    gState.accumulator = 0.0;
    gState.operand = 0.0;
    gState.pendingOp = kActionNone;
    gState.clearOnNext = true;
    gState.hasDecimal = false;
    gState.decimalPlaces = 0;
    gState.error = false;

    strcpy(gState.displayStr, "0");
    UpdateDisplay();
}

/* Update display */
void UpdateDisplay(void) {
    DrawDisplay();
}

/* Set error state */
void SetError(const char *errorMsg) {
    gState.error = true;
    strcpy(gState.displayStr, errorMsg);
    UpdateDisplay();
}

/* Convert string to double */
double StringToDouble(const char *str) {
    return atof(str);
}

/* Convert double to string */
void DoubleToString(double value, char *str) {
    /* Format with appropriate precision */
    if (value == (long)value && value >= -999999999 && value <= 999999999) {
        sprintf(str, "%ld", (long)value);
    } else {
        sprintf(str, "%.6g", value);
    }

    /* Ensure it fits in display */
    if (strlen(str) > MAX_DIGITS) {
        if (value >= 1e10 || value <= -1e10) {
            sprintf(str, "%.3e", value);
        } else {
            str[MAX_DIGITS] = '\0';
        }
    }
}

/* Get calculator window */
WindowPtr GetCalculatorWindow(void) {
    return gWindow;
}

/* Check if calculator is active */
Boolean IsCalculatorActive(void) {
    return gActive && gWindow != NULL;
}
/*
 * Calculator Header - System 7.1 Calculator Desk Accessory
 * Portable implementation with cross-platform support
 */

#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <Types.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <Events.h>

/* Constants */
#define CALC_WINDOW_WIDTH    198
#define CALC_WINDOW_HEIGHT   192
#define DISPLAY_HEIGHT       24
#define BUTTON_WIDTH         30
#define BUTTON_HEIGHT        24
#define BUTTON_SPACING       8
#define MAX_DIGITS           12

/* Button action codes */
enum {
    kActionNone = 0,
    kActionDigit0,
    kActionDigit1,
    kActionDigit2,
    kActionDigit3,
    kActionDigit4,
    kActionDigit5,
    kActionDigit6,
    kActionDigit7,
    kActionDigit8,
    kActionDigit9,
    kActionClear,
    kActionEquals,
    kActionAdd,
    kActionSubtract,
    kActionMultiply,
    kActionDivide,
    kActionDecimal,
    kActionMemoryClear,
    kActionMemoryRecall,
    kActionMemoryAdd,
    kActionMemorySubtract,
    kActionChangeSign,
    kActionPercent
};

/* Button structure */
typedef struct {
    Rect    bounds;
    char    label[4];
    short   action;
    Boolean hilited;
} CalcButton;

/* Calculator state */
typedef struct {
    double      accumulator;        /* Previous value */
    double      memory;            /* Memory register */
    double      operand;           /* Current value */
    short       pendingOp;         /* Pending operation */
    Boolean     clearOnNext;       /* Clear on next digit */
    Boolean     hasDecimal;        /* Decimal entered */
    short       decimalPlaces;     /* Decimal position */
    Boolean     error;             /* Error state */
    char        displayStr[32];    /* Display string */
} CalcState;

/* Function prototypes */
OSErr InitCalculator(void);
void HandleCalculatorEvent(EventRecord *event);
void CloseCalculator(void);
void HandleMouseDown(Point where);
short FindButton(Point pt);
void HandleKeyDown(char key);
void HandleUpdate(void);
void DrawCalculator(void);
void DrawDisplay(void);
void DrawButton(CalcButton *button);
void HandleButtonClick(short buttonIndex);
void HandleDigit(short digit);
void HandleOperator(short op);
void PerformCalculation(void);
void ClearCalculator(void);
void UpdateDisplay(void);
void SetError(const char *errorMsg);
double StringToDouble(const char *str);
void DoubleToString(double value, char *str);
WindowPtr GetCalculatorWindow(void);
Boolean IsCalculatorActive(void);

#endif /* CALCULATOR_H */
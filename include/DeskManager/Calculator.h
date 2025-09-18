#ifndef CALCULATOR_H
#define CALCULATOR_H

/*
 * Calculator.h - Calculator Desk Accessory
 *
 * Provides a complete calculator implementation with basic arithmetic,
 * scientific functions, and programmer operations. This matches the
 * functionality of the Mac OS Calculator desk accessory.
 *
 * Based on Apple's Calculator DA from System 7.1
 */

#include "DeskAccessory.h"
#include <stdint.h>
#include <stdbool.h>

/* Calculator Constants */
#define CALC_VERSION            0x0200      /* Calculator version 2.0 */
#define CALC_DISPLAY_DIGITS     32          /* Maximum display digits */
#define CALC_MEMORY_SLOTS       10          /* Number of memory slots */
#define CALC_HISTORY_SIZE       20          /* Calculation history size */

/* Calculator Modes */
typedef enum {
    CALC_MODE_BASIC     = 0,    /* Basic arithmetic */
    CALC_MODE_SCIENTIFIC = 1,   /* Scientific functions */
    CALC_MODE_PROGRAMMER = 2    /* Programmer functions */
} CalcMode;

/* Calculator Number Bases */
typedef enum {
    CALC_BASE_DECIMAL   = 10,   /* Decimal (base 10) */
    CALC_BASE_HEX       = 16,   /* Hexadecimal (base 16) */
    CALC_BASE_OCTAL     = 8,    /* Octal (base 8) */
    CALC_BASE_BINARY    = 2     /* Binary (base 2) */
} CalcBase;

/* Calculator States */
typedef enum {
    CALC_STATE_ENTRY    = 0,    /* Entering a number */
    CALC_STATE_OPERATION = 1,   /* Operation selected */
    CALC_STATE_RESULT   = 2,    /* Showing result */
    CALC_STATE_ERROR    = 3     /* Error state */
} CalcState;

/* Calculator Operations */
typedef enum {
    /* Basic arithmetic */
    CALC_OP_NONE        = 0,
    CALC_OP_ADD         = 1,    /* Addition */
    CALC_OP_SUBTRACT    = 2,    /* Subtraction */
    CALC_OP_MULTIPLY    = 3,    /* Multiplication */
    CALC_OP_DIVIDE      = 4,    /* Division */
    CALC_OP_EQUALS      = 5,    /* Equals */

    /* Scientific functions */
    CALC_OP_SIN         = 10,   /* Sine */
    CALC_OP_COS         = 11,   /* Cosine */
    CALC_OP_TAN         = 12,   /* Tangent */
    CALC_OP_ASIN        = 13,   /* Arc sine */
    CALC_OP_ACOS        = 14,   /* Arc cosine */
    CALC_OP_ATAN        = 15,   /* Arc tangent */
    CALC_OP_LOG         = 16,   /* Log base 10 */
    CALC_OP_LN          = 17,   /* Natural log */
    CALC_OP_EXP         = 18,   /* e^x */
    CALC_OP_POWER       = 19,   /* x^y */
    CALC_OP_SQRT        = 20,   /* Square root */
    CALC_OP_SQUARE      = 21,   /* x^2 */
    CALC_OP_RECIPROCAL  = 22,   /* 1/x */
    CALC_OP_FACTORIAL   = 23,   /* x! */

    /* Programmer functions */
    CALC_OP_AND         = 30,   /* Bitwise AND */
    CALC_OP_OR          = 31,   /* Bitwise OR */
    CALC_OP_XOR         = 32,   /* Bitwise XOR */
    CALC_OP_NOT         = 33,   /* Bitwise NOT */
    CALC_OP_SHIFT_LEFT  = 34,   /* Left shift */
    CALC_OP_SHIFT_RIGHT = 35,   /* Right shift */
    CALC_OP_MOD         = 36,   /* Modulo */

    /* Special operations */
    CALC_OP_CLEAR       = 50,   /* Clear */
    CALC_OP_CLEAR_ALL   = 51,   /* Clear all */
    CALC_OP_BACKSPACE   = 52,   /* Backspace */
    CALC_OP_CHANGE_SIGN = 53,   /* +/- */
    CALC_OP_PERCENT     = 54,   /* Percent */

    /* Memory operations */
    CALC_OP_MEM_CLEAR   = 60,   /* Memory clear */
    CALC_OP_MEM_RECALL  = 61,   /* Memory recall */
    CALC_OP_MEM_STORE   = 62,   /* Memory store */
    CALC_OP_MEM_ADD     = 63,   /* Memory add */
    CALC_OP_MEM_SUB     = 64    /* Memory subtract */
} CalcOperation;

/* Calculator Button IDs (matching Mac OS Calculator) */
typedef enum {
    CALC_BTN_0          = 1,    /* Digit 0 */
    CALC_BTN_1          = 2,    /* Digit 1 */
    CALC_BTN_2          = 3,    /* Digit 2 */
    CALC_BTN_3          = 4,    /* Digit 3 */
    CALC_BTN_4          = 5,    /* Digit 4 */
    CALC_BTN_5          = 6,    /* Digit 5 */
    CALC_BTN_6          = 7,    /* Digit 6 */
    CALC_BTN_7          = 8,    /* Digit 7 */
    CALC_BTN_8          = 9,    /* Digit 8 */
    CALC_BTN_9          = 10,   /* Digit 9 */
    CALC_BTN_A          = 11,   /* Hex digit A */
    CALC_BTN_B          = 12,   /* Hex digit B */
    CALC_BTN_C          = 13,   /* Hex digit C */
    CALC_BTN_D          = 14,   /* Hex digit D */
    CALC_BTN_E          = 15,   /* Hex digit E */
    CALC_BTN_F          = 16,   /* Hex digit F */

    CALC_BTN_DECIMAL    = 20,   /* Decimal point */
    CALC_BTN_EQUALS     = 21,   /* Equals */
    CALC_BTN_ADD        = 22,   /* Plus */
    CALC_BTN_SUBTRACT   = 23,   /* Minus */
    CALC_BTN_MULTIPLY   = 24,   /* Multiply */
    CALC_BTN_DIVIDE     = 25,   /* Divide */
    CALC_BTN_CLEAR      = 26,   /* Clear */
    CALC_BTN_CLEAR_ALL  = 27,   /* Clear all */

    CALC_BTN_MEM_CLEAR  = 30,   /* MC - Memory clear */
    CALC_BTN_MEM_RECALL = 31,   /* MR - Memory recall */
    CALC_BTN_MEM_STORE  = 32,   /* MS - Memory store */
    CALC_BTN_MEM_ADD    = 33,   /* M+ - Memory add */

    CALC_BTN_MODE_BASIC = 40,   /* Basic mode */
    CALC_BTN_MODE_SCI   = 41,   /* Scientific mode */
    CALC_BTN_MODE_PROG  = 42    /* Programmer mode */
} CalcButtonID;

/* Calculator Number */
typedef struct CalcNumber {
    double      value;          /* Numeric value */
    char        display[CALC_DISPLAY_DIGITS + 1];  /* Display string */
    CalcBase    base;           /* Number base */
    bool        isInteger;      /* Integer flag for programmer mode */
    int64_t     intValue;       /* Integer value for bitwise ops */
} CalcNumber;

/* Calculator History Entry */
typedef struct CalcHistoryEntry {
    CalcNumber      operand1;   /* First operand */
    CalcNumber      operand2;   /* Second operand */
    CalcOperation   operation;  /* Operation performed */
    CalcNumber      result;     /* Result */
    char            expression[256];  /* Human-readable expression */
} CalcHistoryEntry;

/* Calculator State */
typedef struct Calculator {
    /* Core state */
    CalcMode        mode;               /* Current mode */
    CalcState       state;              /* Current state */
    CalcBase        base;               /* Current number base */

    /* Display and input */
    CalcNumber      display;            /* Current display value */
    CalcNumber      accumulator;        /* Accumulator register */
    CalcNumber      operand;            /* Current operand */
    CalcOperation   pendingOp;          /* Pending operation */
    bool            newNumber;          /* Starting new number */
    bool            decimalEntered;     /* Decimal point entered */

    /* Memory */
    CalcNumber      memory[CALC_MEMORY_SLOTS];  /* Memory slots */
    bool            memoryUsed[CALC_MEMORY_SLOTS];  /* Memory slot used flags */

    /* History */
    CalcHistoryEntry history[CALC_HISTORY_SIZE];  /* Calculation history */
    int             historyCount;       /* Number of history entries */
    int             historyIndex;       /* Current history index */

    /* Settings */
    bool            angleRadians;       /* Angle mode (true=radians, false=degrees) */
    int             precision;          /* Decimal precision */
    bool            scientific;         /* Scientific notation */

    /* UI state */
    Rect            windowBounds;       /* Window bounds */
    bool            windowVisible;      /* Window visibility */
    void            *buttonRects;       /* Button rectangles */
    int             pressedButton;      /* Currently pressed button */

    /* Error handling */
    int             errorCode;          /* Last error code */
    char            errorMessage[256];  /* Error message */
} Calculator;

/* Calculator Functions */

/**
 * Initialize calculator
 * @param calc Pointer to calculator structure
 * @return 0 on success, negative on error
 */
int Calculator_Initialize(Calculator *calc);

/**
 * Shutdown calculator
 * @param calc Pointer to calculator structure
 */
void Calculator_Shutdown(Calculator *calc);

/**
 * Reset calculator to initial state
 * @param calc Pointer to calculator structure
 */
void Calculator_Reset(Calculator *calc);

/**
 * Process button press
 * @param calc Pointer to calculator structure
 * @param buttonID Button ID
 * @return 0 on success, negative on error
 */
int Calculator_PressButton(Calculator *calc, CalcButtonID buttonID);

/**
 * Process keyboard input
 * @param calc Pointer to calculator structure
 * @param key Key character
 * @return 0 on success, negative on error
 */
int Calculator_KeyPress(Calculator *calc, char key);

/**
 * Perform calculation operation
 * @param calc Pointer to calculator structure
 * @param operation Operation to perform
 * @return 0 on success, negative on error
 */
int Calculator_PerformOperation(Calculator *calc, CalcOperation operation);

/**
 * Enter digit
 * @param calc Pointer to calculator structure
 * @param digit Digit to enter (0-15 for hex)
 * @return 0 on success, negative on error
 */
int Calculator_EnterDigit(Calculator *calc, int digit);

/**
 * Enter decimal point
 * @param calc Pointer to calculator structure
 * @return 0 on success, negative on error
 */
int Calculator_EnterDecimal(Calculator *calc);

/**
 * Clear current entry
 * @param calc Pointer to calculator structure
 */
void Calculator_Clear(Calculator *calc);

/**
 * Clear all (reset)
 * @param calc Pointer to calculator structure
 */
void Calculator_ClearAll(Calculator *calc);

/**
 * Backspace (remove last digit)
 * @param calc Pointer to calculator structure
 */
void Calculator_Backspace(Calculator *calc);

/* Mode and Base Functions */

/**
 * Set calculator mode
 * @param calc Pointer to calculator structure
 * @param mode New mode
 * @return 0 on success, negative on error
 */
int Calculator_SetMode(Calculator *calc, CalcMode mode);

/**
 * Set number base (programmer mode)
 * @param calc Pointer to calculator structure
 * @param base New base
 * @return 0 on success, negative on error
 */
int Calculator_SetBase(Calculator *calc, CalcBase base);

/**
 * Toggle angle mode (radians/degrees)
 * @param calc Pointer to calculator structure
 */
void Calculator_ToggleAngleMode(Calculator *calc);

/* Memory Functions */

/**
 * Clear memory slot
 * @param calc Pointer to calculator structure
 * @param slot Memory slot (0-9)
 */
void Calculator_MemoryClear(Calculator *calc, int slot);

/**
 * Store value in memory slot
 * @param calc Pointer to calculator structure
 * @param slot Memory slot (0-9)
 */
void Calculator_MemoryStore(Calculator *calc, int slot);

/**
 * Recall value from memory slot
 * @param calc Pointer to calculator structure
 * @param slot Memory slot (0-9)
 * @return 0 on success, negative on error
 */
int Calculator_MemoryRecall(Calculator *calc, int slot);

/**
 * Add to memory slot
 * @param calc Pointer to calculator structure
 * @param slot Memory slot (0-9)
 */
void Calculator_MemoryAdd(Calculator *calc, int slot);

/**
 * Subtract from memory slot
 * @param calc Pointer to calculator structure
 * @param slot Memory slot (0-9)
 */
void Calculator_MemorySubtract(Calculator *calc, int slot);

/* History Functions */

/**
 * Add calculation to history
 * @param calc Pointer to calculator structure
 * @param op1 First operand
 * @param op2 Second operand
 * @param operation Operation
 * @param result Result
 */
void Calculator_AddToHistory(Calculator *calc, const CalcNumber *op1,
                           const CalcNumber *op2, CalcOperation operation,
                           const CalcNumber *result);

/**
 * Get history entry
 * @param calc Pointer to calculator structure
 * @param index History index
 * @return Pointer to history entry or NULL
 */
const CalcHistoryEntry *Calculator_GetHistoryEntry(Calculator *calc, int index);

/**
 * Clear calculation history
 * @param calc Pointer to calculator structure
 */
void Calculator_ClearHistory(Calculator *calc);

/* Display Functions */

/**
 * Get display string
 * @param calc Pointer to calculator structure
 * @return Display string
 */
const char *Calculator_GetDisplay(Calculator *calc);

/**
 * Update display for current state
 * @param calc Pointer to calculator structure
 */
void Calculator_UpdateDisplay(Calculator *calc);

/**
 * Format number for display
 * @param number Pointer to number
 * @param buffer Buffer for formatted string
 * @param bufferSize Size of buffer
 */
void Calculator_FormatNumber(const CalcNumber *number, char *buffer, int bufferSize);

/* Desk Accessory Integration */

/**
 * Register Calculator as a desk accessory
 * @return 0 on success, negative on error
 */
int Calculator_RegisterDA(void);

/**
 * Create Calculator DA instance
 * @return Pointer to DA instance or NULL on error
 */
DeskAccessory *Calculator_CreateDA(void);

/* Calculator Error Codes */
#define CALC_ERR_NONE           0       /* No error */
#define CALC_ERR_DIVIDE_BY_ZERO -1      /* Division by zero */
#define CALC_ERR_OVERFLOW       -2      /* Numeric overflow */
#define CALC_ERR_UNDERFLOW      -3      /* Numeric underflow */
#define CALC_ERR_DOMAIN         -4      /* Domain error (e.g., sqrt(-1)) */
#define CALC_ERR_INVALID_OP     -5      /* Invalid operation */
#define CALC_ERR_INVALID_BASE   -6      /* Invalid number base */
#define CALC_ERR_MEMORY_EMPTY   -7      /* Memory slot empty */

#endif /* CALCULATOR_H */
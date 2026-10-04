#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "SystemTypes.h"

/*
 * Calculator.h - Calculator Desk Accessory
 *
 * Provides a complete calculator implementation with basic arithmetic,
 * scientific functions, and programmer operations. This matches the
 * functionality of the Mac OS Calculator desk accessory.
 *
 * Derived from ROM analysis (System 7)
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Calculator Constants */
#define CALC_VERSION            0x0200      /* Calculator version 2.0 */
#define CALC_DISPLAY_DIGITS     32          /* Maximum display digits */
#define CALC_MEMORY_SLOTS       10          /* Number of memory slots */
#define CALC_HISTORY_SIZE       20          /* Calculation history size */


/* Calculator Modes */
typedef enum {
    CALC_MODE_BASIC = 0,
    CALC_MODE_SCIENTIFIC = 1,
    CALC_MODE_PROGRAMMER = 2
} CalcMode;

/* Calculator Number Bases */
typedef enum {
    CALC_BASE_DECIMAL = 10,
    CALC_BASE_BINARY = 2,
    CALC_BASE_OCTAL = 8,
    CALC_BASE_HEXADECIMAL = 16
} CalcBase;

/* Calculator States */
typedef enum {
    CALC_STATE_ENTRY = 0,
    CALC_STATE_OPERATION = 1,
    CALC_STATE_RESULT = 2,
    CALC_STATE_ERROR = 3
} CalcState;

/* Calculator Operations */
typedef enum {
    CALC_OP_NONE = 0,
    CALC_OP_ADD = 1,
    CALC_OP_SUBTRACT = 2,
    CALC_OP_MULTIPLY = 3,
    CALC_OP_DIVIDE = 4,
    CALC_OP_EQUALS = 5,
    CALC_OP_PERCENT = 6,
    CALC_OP_NEGATE = 7,
    CALC_OP_RECIPROCAL = 8,
    CALC_OP_SQUARE = 9,
    CALC_OP_SQRT = 10,
    CALC_OP_SIN = 11,
    CALC_OP_COS = 12,
    CALC_OP_TAN = 13,
    CALC_OP_LOG = 14,
    CALC_OP_LN = 15,
    CALC_OP_EXP = 16,
    CALC_OP_XOR = 17,
    CALC_OP_AND = 18,
    CALC_OP_OR = 19,
    CALC_OP_NOT = 20,
    CALC_OP_ASIN = 21,
    CALC_OP_ACOS = 22,
    CALC_OP_ATAN = 23,
    CALC_OP_FACTORIAL = 24,
    CALC_OP_CHANGE_SIGN = 25,
    CALC_OP_SHIFT_LEFT = 26,
    CALC_OP_SHIFT_RIGHT = 27,
    CALC_OP_MOD = 28
} CalcOperation;

/* Aliases for backward compatibility */
#define CALC_BASE_HEX CALC_BASE_HEXADECIMAL

/* Calculator Button IDs (matching Mac OS Calculator) */
typedef enum {
    CALC_BTN_0 = 0,
    CALC_BTN_1 = 1,
    CALC_BTN_2 = 2,
    CALC_BTN_3 = 3,
    CALC_BTN_4 = 4,
    CALC_BTN_5 = 5,
    CALC_BTN_6 = 6,
    CALC_BTN_7 = 7,
    CALC_BTN_8 = 8,
    CALC_BTN_9 = 9,
    CALC_BTN_A = 10,
    CALC_BTN_B = 11,
    CALC_BTN_C = 12,
    CALC_BTN_D = 13,
    CALC_BTN_E = 14,
    CALC_BTN_F = 15,
    CALC_BTN_POINT = 16,
    CALC_BTN_PLUS = 17,
    CALC_BTN_MINUS = 18,
    CALC_BTN_MULTIPLY = 19,
    CALC_BTN_DIVIDE = 20,
    CALC_BTN_EQUALS = 21,
    CALC_BTN_PERCENT = 22,
    CALC_BTN_NEGATE = 23,
    CALC_BTN_CLEAR = 24,
    CALC_BTN_CLEAR_ALL = 25,
    CALC_BTN_MEM_CLEAR = 26,
    CALC_BTN_MEM_RECALL = 27,
    CALC_BTN_MEM_STORE = 28,
    CALC_BTN_MEM_ADD = 29,
    CALC_BTN_MODE_BASIC = 30,
    CALC_BTN_MODE_SCI = 31,
    CALC_BTN_MODE_PROG = 32
} CalcButtonID;

/* Aliases for backward compatibility */
#define CALC_BTN_ADD CALC_BTN_PLUS
#define CALC_BTN_SUBTRACT CALC_BTN_MINUS
#define CALC_BTN_DECIMAL CALC_BTN_POINT

/* Calculator Number */
typedef struct {
    double value;
    SInt64 intValue;
    CalcBase base;
    Boolean isInteger;
} CalcNumber;

/* Calculator History Entry */
typedef struct {
    CalcNumber operand1;
    CalcNumber operand2;
    CalcOperation operation;
    CalcNumber result;
    char expression[CALC_DISPLAY_DIGITS + 1];
} CalcHistoryEntry;

/* Calculator State */
typedef struct {
    double value;
    double accumulator;
    SInt64 intValue;
    CalcBase base;
    Boolean isInteger;
    double memory[CALC_MEMORY_SLOTS];
    int memoryUsed[CALC_MEMORY_SLOTS];
    char display[CALC_DISPLAY_DIGITS + 1];
    int left, top, right, bottom;
    CalcHistoryEntry *history;
    int historyCount;
    CalcMode mode;
    CalcState state;
    CalcOperation pendingOp;
    Boolean newNumber;
    Boolean decimalEntered;
    Boolean angleRadians;
    int precision;
    int errorCode;
    char errorMessage[256];
    void *buttonRects;
    SInt16 iconID;
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

/* Calculator Error Codes */
#define CALC_ERR_NONE           0       /* No error */
#define CALC_ERR_DIVIDE_BY_ZERO -1      /* Division by zero */
#define CALC_ERR_OVERFLOW       -2      /* Numeric overflow */
#define CALC_ERR_UNDERFLOW      -3      /* Numeric underflow */
#define CALC_ERR_DOMAIN         -4      /* Domain error (e.g., sqrt(-1)) */
#define CALC_ERR_INVALID_OP     -5      /* Invalid operation */
#define CALC_ERR_INVALID_BASE   -6      /* Invalid number base */
#define CALC_ERR_MEMORY_EMPTY   -7      /* Memory slot empty */

#ifdef __cplusplus
}
#endif

#endif /* CALCULATOR_H */

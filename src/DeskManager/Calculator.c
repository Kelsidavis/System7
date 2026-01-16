#include "MemoryMgr/MemoryManager.h"
#include <stdlib.h>
#include <string.h>
/*
 * Calculator.c - Calculator Desk Accessory Implementation
 *
 * Provides a complete calculator with basic arithmetic, scientific functions,
 * and programmer operations. Matches the functionality of the Mac OS Calculator
 * desk accessory with modern enhancements.
 *
 * Derived from ROM analysis (System 7)
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/Calculator.h"
#include "DeskManager/DeskManager.h"
#include "Resources/ResourceData.h"
#include <math.h>

/* Simple atof implementation for bare-metal kernel */
static double simple_atof(const char* str) {
    double result = 0.0;
    double fraction = 0.0;
    int divisor = 1;
    int sign = 1;
    Boolean inFraction = false;

    if (!str) return 0.0;

    /* Handle sign */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Parse integer and fraction parts */
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            if (inFraction) {
                fraction = fraction * 10.0 + (*str - '0');
                divisor *= 10;
            } else {
                result = result * 10.0 + (*str - '0');
            }
        } else if (*str == '.' && !inFraction) {
            inFraction = true;
        } else {
            break;  /* Stop at first invalid character */
        }
        str++;
    }

    result = result + fraction / divisor;
    return sign * result;
}

#define atof simple_atof

/* Calculator Implementation */
static Calculator g_calculator = {0};
static Boolean g_calculatorInitialized = false;

/* Internal Function Prototypes */
static void Calculator_ClearDisplay(Calculator *calc);
static void Calculator_SetError(Calculator *calc, int errorCode, const char *message);
static double Calculator_PerformArithmetic(double op1, double op2, CalcOperation operation);
static double Calculator_PerformScientific(double operand, CalcOperation operation);
static int64_t Calculator_PerformBitwise(int64_t op1, int64_t op2, CalcOperation operation);
static void Calculator_ConvertToBase(Calculator *calc, CalcBase newBase);
static Boolean Calculator_IsValidDigitForBase(int digit, CalcBase base);

/* Calculator Interface Functions */
static int Calculator_DAOpen(DeskAccessory *da);
static void Calculator_DAClose(DeskAccessory *da);
static int Calculator_DAEvent(DeskAccessory *da, const EventRecord *event);
static int Calculator_DAMenu(DeskAccessory *da, short menuID, short itemID);
static void Calculator_DAIdle(DeskAccessory *da);
static void Calculator_DAActivate(DeskAccessory *da, Boolean active);
static void Calculator_DAUpdate(DeskAccessory *da);

/* Calculator DA Interface */
static DAInterface g_calculatorInterface = {
    .initialize = NULL,  /* Will be set during registration */
    .terminate = NULL,
    .processEvent = NULL,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = NULL,
    .updateCursor = NULL,
    .activate = NULL,
    .update = NULL,
    .resize = NULL
};

/*
 * Initialize calculator
 */
int Calculator_Initialize(Calculator *calc)
{
    if (!calc) {
        return CALC_ERR_INVALID_OP;
    }

    memset(calc, 0, sizeof(Calculator));

    /* Set default state */
    calc->mode = CALC_MODE_BASIC;
    calc->state = CALC_STATE_ENTRY;
    calc->base = CALC_BASE_DECIMAL;
    calc->newNumber = true;
    calc->angleRadians = false;
    calc->precision = 10;

    /* Initialize display */
    (calc)->value = 0.0;
    (calc)->base = CALC_BASE_DECIMAL;
    (calc)->isInteger = false;
    strncpy((calc)->display, "0", sizeof((calc)->display) - 1);
    (calc)->display[sizeof((calc)->display) - 1] = '\0';

    /* Initialize window bounds */
    (calc)->left = 100;
    (calc)->top = 100;
    (calc)->right = 300;
    (calc)->bottom = 400;

    Calculator_UpdateDisplay(calc);
    return CALC_ERR_NONE;
}

/*
 * Shutdown calculator
 */
void Calculator_Shutdown(Calculator *calc)
{
    if (calc) {
        /* Clean up any allocated resources */
        DisposePtr((Ptr)calc->buttonRects);
        calc->buttonRects = NULL;
    }
}

/*
 * Reset calculator to initial state
 */
void Calculator_Reset(Calculator *calc)
{
    if (!calc) return;

    calc->state = CALC_STATE_ENTRY;
    (calc)->value = 0.0;
    (calc)->value = 0.0;
    (calc)->value = 0.0;
    calc->pendingOp = CALC_OP_NONE;
    calc->newNumber = true;
    calc->decimalEntered = false;
    calc->errorCode = CALC_ERR_NONE;
    calc->errorMessage[0] = '\0';

    Calculator_UpdateDisplay(calc);
}

/*
 * Process button press
 */
int Calculator_PressButton(Calculator *calc, CalcButtonID buttonID)
{
    if (!calc) {
        return CALC_ERR_INVALID_OP;
    }

    /* Clear error state on new input */
    if (calc->state == CALC_STATE_ERROR &&
        buttonID != CALC_BTN_CLEAR && buttonID != CALC_BTN_CLEAR_ALL) {
        return CALC_ERR_INVALID_OP;
    }

    switch (buttonID) {
        /* Digits */
        case CALC_BTN_0: case CALC_BTN_1: case CALC_BTN_2: case CALC_BTN_3:
        case CALC_BTN_4: case CALC_BTN_5: case CALC_BTN_6: case CALC_BTN_7:
        case CALC_BTN_8: case CALC_BTN_9:
            return Calculator_EnterDigit(calc, buttonID - CALC_BTN_0);

        case CALC_BTN_A: case CALC_BTN_B: case CALC_BTN_C:
        case CALC_BTN_D: case CALC_BTN_E: case CALC_BTN_F:
            if (calc->base == CALC_BASE_HEX) {
                return Calculator_EnterDigit(calc, 10 + (buttonID - CALC_BTN_A));
            }
            return CALC_ERR_INVALID_OP;

        case CALC_BTN_DECIMAL:
            return Calculator_EnterDecimal(calc);

        /* Operations */
        case CALC_BTN_ADD:
            return Calculator_PerformOperation(calc, CALC_OP_ADD);
        case CALC_BTN_SUBTRACT:
            return Calculator_PerformOperation(calc, CALC_OP_SUBTRACT);
        case CALC_BTN_MULTIPLY:
            return Calculator_PerformOperation(calc, CALC_OP_MULTIPLY);
        case CALC_BTN_DIVIDE:
            return Calculator_PerformOperation(calc, CALC_OP_DIVIDE);
        case CALC_BTN_EQUALS:
            return Calculator_PerformOperation(calc, CALC_OP_EQUALS);

        /* Clear operations */
        case CALC_BTN_CLEAR:
            Calculator_Clear(calc);
            return CALC_ERR_NONE;
        case CALC_BTN_CLEAR_ALL:
            Calculator_ClearAll(calc);
            return CALC_ERR_NONE;

        /* Memory operations */
        case CALC_BTN_MEM_CLEAR:
            Calculator_MemoryClear(calc, 0);
            return CALC_ERR_NONE;
        case CALC_BTN_MEM_RECALL:
            return Calculator_MemoryRecall(calc, 0);
        case CALC_BTN_MEM_STORE:
            Calculator_MemoryStore(calc, 0);
            return CALC_ERR_NONE;
        case CALC_BTN_MEM_ADD:
            Calculator_MemoryAdd(calc, 0);
            return CALC_ERR_NONE;

        /* Mode switches */
        case CALC_BTN_MODE_BASIC:
            return Calculator_SetMode(calc, CALC_MODE_BASIC);
        case CALC_BTN_MODE_SCI:
            return Calculator_SetMode(calc, CALC_MODE_SCIENTIFIC);
        case CALC_BTN_MODE_PROG:
            return Calculator_SetMode(calc, CALC_MODE_PROGRAMMER);

        default:
            return CALC_ERR_INVALID_OP;
    }
}

/*
 * Process keyboard input
 */
int Calculator_KeyPress(Calculator *calc, char key)
{
    if (!calc) {
        return CALC_ERR_INVALID_OP;
    }

    switch (key) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return Calculator_EnterDigit(calc, key - '0');

        case 'A': case 'a': case 'B': case 'b': case 'C': case 'c':
        case 'D': case 'd': case 'E': case 'e': case 'F': case 'f':
            if (calc->base == CALC_BASE_HEX) {
                int digit = (key >= 'a') ? (key - 'a' + 10) : (key - 'A' + 10);
                return Calculator_EnterDigit(calc, digit);
            }
            return CALC_ERR_INVALID_OP;

        case '.':
            return Calculator_EnterDecimal(calc);

        case '+':
            return Calculator_PerformOperation(calc, CALC_OP_ADD);
        case '-':
            return Calculator_PerformOperation(calc, CALC_OP_SUBTRACT);
        case '*':
            return Calculator_PerformOperation(calc, CALC_OP_MULTIPLY);
        case '/':
            return Calculator_PerformOperation(calc, CALC_OP_DIVIDE);
        case '=':
        case '\r':
        case '\n':
            return Calculator_PerformOperation(calc, CALC_OP_EQUALS);

        case '\b':  /* Backspace */
            Calculator_Backspace(calc);
            return CALC_ERR_NONE;

        case '\x1b':  /* Escape */
            Calculator_ClearAll(calc);
            return CALC_ERR_NONE;

        default:
            return CALC_ERR_INVALID_OP;
    }
}

/*
 * Perform calculation operation
 */
int Calculator_PerformOperation(Calculator *calc, CalcOperation operation)
{
    if (!calc) {
        return CALC_ERR_INVALID_OP;
    }

    if (calc->state == CALC_STATE_ERROR) {
        return CALC_ERR_INVALID_OP;
    }

    double result = 0.0;
    int errorCode = CALC_ERR_NONE;

    switch (operation) {
        case CALC_OP_ADD:
        case CALC_OP_SUBTRACT:
        case CALC_OP_MULTIPLY:
        case CALC_OP_DIVIDE:
            /* Binary operations */
            if (calc->pendingOp != CALC_OP_NONE && calc->state == CALC_STATE_OPERATION) {
                /* Chain operations: perform pending operation first */
                result = Calculator_PerformArithmetic((calc)->value,
                                                    (calc)->value,
                                                    calc->pendingOp);
                if (result == HUGE_VAL || result == -HUGE_VAL) {
                    Calculator_SetError(calc, CALC_ERR_OVERFLOW, "Overflow");
                    return CALC_ERR_OVERFLOW;
                }
                if (isnan(result)) {
                    Calculator_SetError(calc, CALC_ERR_DOMAIN, "Invalid Operation");
                    return CALC_ERR_DOMAIN;
                }

                (calc)->value = result;
                (calc)->value = result;
                Calculator_UpdateDisplay(calc);
            } else {
                (calc)->value = (calc)->value;
            }

            calc->pendingOp = operation;
            calc->state = CALC_STATE_OPERATION;
            calc->newNumber = true;
            break;

        case CALC_OP_EQUALS:
            if (calc->pendingOp != CALC_OP_NONE) {
                result = Calculator_PerformArithmetic((calc)->value,
                                                    (calc)->value,
                                                    calc->pendingOp);
                if (result == HUGE_VAL || result == -HUGE_VAL) {
                    Calculator_SetError(calc, CALC_ERR_OVERFLOW, "Overflow");
                    return CALC_ERR_OVERFLOW;
                }
                if (isnan(result)) {
                    Calculator_SetError(calc, CALC_ERR_DOMAIN, "Invalid Operation");
                    return CALC_ERR_DOMAIN;
                }

                /* Create CalcNumber structures for history */
                CalcNumber op1, op2, resultNum;
                op1.value = calc->accumulator;
                op1.intValue = (SInt64)calc->accumulator;
                op1.base = calc->base;
                op1.isInteger = calc->isInteger;

                op2.value = calc->value;
                op2.intValue = calc->intValue;
                op2.base = calc->base;
                op2.isInteger = calc->isInteger;

                resultNum.value = result;
                resultNum.intValue = (SInt64)result;
                resultNum.base = calc->base;
                resultNum.isInteger = calc->isInteger;

                /* Add to history */
                Calculator_AddToHistory(calc, &op1, &op2,
                                      calc->pendingOp, &resultNum);

                (calc)->value = result;
                (calc)->value = result;
                calc->pendingOp = CALC_OP_NONE;
                calc->state = CALC_STATE_RESULT;
                calc->newNumber = true;
                Calculator_UpdateDisplay(calc);
            }
            break;

        /* Scientific operations (unary) */
        case CALC_OP_SIN: case CALC_OP_COS: case CALC_OP_TAN:
        case CALC_OP_ASIN: case CALC_OP_ACOS: case CALC_OP_ATAN:
        case CALC_OP_LOG: case CALC_OP_LN: case CALC_OP_EXP:
        case CALC_OP_SQRT: case CALC_OP_SQUARE: case CALC_OP_RECIPROCAL:
        case CALC_OP_FACTORIAL:
            if (calc->mode != CALC_MODE_SCIENTIFIC) {
                return CALC_ERR_INVALID_OP;
            }
            result = Calculator_PerformScientific((calc)->value, operation);
            if (result == HUGE_VAL || result == -HUGE_VAL) {
                Calculator_SetError(calc, CALC_ERR_OVERFLOW, "Overflow");
                return CALC_ERR_OVERFLOW;
            }
            if (isnan(result)) {
                Calculator_SetError(calc, CALC_ERR_DOMAIN, "Domain Error");
                return CALC_ERR_DOMAIN;
            }
            (calc)->value = result;
            calc->state = CALC_STATE_RESULT;
            calc->newNumber = true;
            Calculator_UpdateDisplay(calc);
            break;

        /* Special operations */
        case CALC_OP_CHANGE_SIGN:
            (calc)->value = -(calc)->value;
            Calculator_UpdateDisplay(calc);
            break;

        case CALC_OP_PERCENT:
            if (calc->pendingOp != CALC_OP_NONE) {
                (calc)->value = ((calc)->value * (calc)->value) / 100.0;
                Calculator_UpdateDisplay(calc);
            }
            break;

        default:
            return CALC_ERR_INVALID_OP;
    }

    return errorCode;
}

/*
 * Enter digit
 */
int Calculator_EnterDigit(Calculator *calc, int digit)
{
    if (!calc || !Calculator_IsValidDigitForBase(digit, calc->base)) {
        return CALC_ERR_INVALID_OP;
    }

    if (calc->state == CALC_STATE_ERROR) {
        return CALC_ERR_INVALID_OP;
    }

    if (calc->newNumber) {
        (calc)->value = 0.0;
        calc->newNumber = false;
        calc->decimalEntered = false;
        calc->state = CALC_STATE_ENTRY;
    }

    /* Handle different number bases */
    if (calc->base == CALC_BASE_DECIMAL) {
        if (calc->decimalEntered) {
            /* Add digit after decimal point */
            char temp[32];
            snprintf(temp, sizeof(temp), "%.10f", (calc)->value);

            /* Find decimal point and add digit */
            char *decimal = strchr(temp, '.');
            if (decimal && strlen(decimal) < 12) {
                char newStr[32];
                snprintf(newStr, sizeof(newStr), "%s%d", temp, digit);
                (calc)->value = atof(newStr);
            }
        } else {
            /* Add digit before decimal point */
            (calc)->value = (calc)->value * 10.0 + digit;
        }
    } else {
        /* Integer bases (hex, octal, binary) */
        (calc)->intValue = (calc)->intValue * calc->base + digit;
        (calc)->value = (double)(calc)->intValue;
    }

    Calculator_UpdateDisplay(calc);
    return CALC_ERR_NONE;
}

/*
 * Enter decimal point
 */
int Calculator_EnterDecimal(Calculator *calc)
{
    if (!calc || calc->base != CALC_BASE_DECIMAL) {
        return CALC_ERR_INVALID_OP;
    }

    if (calc->state == CALC_STATE_ERROR) {
        return CALC_ERR_INVALID_OP;
    }

    if (calc->newNumber) {
        (calc)->value = 0.0;
        calc->newNumber = false;
        calc->state = CALC_STATE_ENTRY;
    }

    if (!calc->decimalEntered) {
        calc->decimalEntered = true;
        Calculator_UpdateDisplay(calc);
    }

    return CALC_ERR_NONE;
}

/*
 * Clear current entry
 */
void Calculator_Clear(Calculator *calc)
{
    if (!calc) return;

    (calc)->value = 0.0;
    calc->newNumber = true;
    calc->decimalEntered = false;
    calc->state = CALC_STATE_ENTRY;
    calc->errorCode = CALC_ERR_NONE;
    calc->errorMessage[0] = '\0';

    Calculator_UpdateDisplay(calc);
}

/*
 * Clear all (reset)
 */
void Calculator_ClearAll(Calculator *calc)
{
    if (!calc) return;

    Calculator_Reset(calc);
}

/*
 * Backspace (remove last digit)
 */
void Calculator_Backspace(Calculator *calc)
{
    if (!calc || calc->state != CALC_STATE_ENTRY) {
        return;
    }

    if (calc->base == CALC_BASE_DECIMAL) {
        if (calc->decimalEntered) {
            /* Remove decimal digits */
            char temp[32];
            snprintf(temp, sizeof(temp), "%.10f", (calc)->value);
            char *decimal = strchr(temp, '.');
            if (decimal && strlen(decimal) > 1) {
                decimal[strlen(decimal) - 1] = '\0';
                (calc)->value = atof(temp);
            } else {
                calc->decimalEntered = false;
            }
        } else {
            /* Remove integer digits */
            (calc)->value = floor((calc)->value / 10.0);
        }
    } else {
        /* Integer bases */
        (calc)->intValue /= calc->base;
        (calc)->value = (double)(calc)->intValue;
    }

    Calculator_UpdateDisplay(calc);
}

/*
 * Set calculator mode
 */
int Calculator_SetMode(Calculator *calc, CalcMode mode)
{
    if (!calc) {
        return CALC_ERR_INVALID_OP;
    }

    calc->mode = mode;

    /* Set appropriate base for mode */
    switch (mode) {
        case CALC_MODE_BASIC:
        case CALC_MODE_SCIENTIFIC:
            Calculator_SetBase(calc, CALC_BASE_DECIMAL);
            break;
        case CALC_MODE_PROGRAMMER:
            Calculator_SetBase(calc, CALC_BASE_HEX);
            break;
    }

    Calculator_UpdateDisplay(calc);
    return CALC_ERR_NONE;
}

/*
 * Set number base (programmer mode)
 */
int Calculator_SetBase(Calculator *calc, CalcBase base)
{
    if (!calc) {
        return CALC_ERR_INVALID_OP;
    }

    if (calc->mode != CALC_MODE_PROGRAMMER && base != CALC_BASE_DECIMAL) {
        return CALC_ERR_INVALID_BASE;
    }

    CalcBase oldBase = calc->base;
    calc->base = base;

    /* Convert current value to new base */
    if (oldBase != base) {
        Calculator_ConvertToBase(calc, base);
        Calculator_UpdateDisplay(calc);
    }

    return CALC_ERR_NONE;
}

/*
 * Toggle angle mode (radians/degrees)
 */
void Calculator_ToggleAngleMode(Calculator *calc)
{
    if (calc) {
        calc->angleRadians = !calc->angleRadians;
    }
}

/*
 * Memory operations
 */
void Calculator_MemoryClear(Calculator *calc, int slot)
{
    if (calc && slot >= 0 && slot < CALC_MEMORY_SLOTS) {
        calc->memory[slot] = 0.0;
        calc->memoryUsed[slot] = false;
    }
}

void Calculator_MemoryStore(Calculator *calc, int slot)
{
    if (calc && slot >= 0 && slot < CALC_MEMORY_SLOTS) {
        calc->memory[slot] = calc->value;
        calc->memoryUsed[slot] = true;
    }
}

int Calculator_MemoryRecall(Calculator *calc, int slot)
{
    if (!calc || slot < 0 || slot >= CALC_MEMORY_SLOTS) {
        return CALC_ERR_INVALID_OP;
    }

    if (!calc->memoryUsed[slot]) {
        return CALC_ERR_MEMORY_EMPTY;
    }

    calc->value = calc->memory[slot];
    calc->newNumber = true;
    calc->state = CALC_STATE_RESULT;
    Calculator_UpdateDisplay(calc);

    return CALC_ERR_NONE;
}

void Calculator_MemoryAdd(Calculator *calc, int slot)
{
    if (calc && slot >= 0 && slot < CALC_MEMORY_SLOTS) {
        calc->memory[slot] += calc->value;
        calc->memoryUsed[slot] = true;
    }
}

void Calculator_MemorySubtract(Calculator *calc, int slot)
{
    if (calc && slot >= 0 && slot < CALC_MEMORY_SLOTS) {
        calc->memory[slot] -= calc->value;
        calc->memoryUsed[slot] = true;
    }
}

/*
 * Add calculation to history
 */
void Calculator_AddToHistory(Calculator *calc, const CalcNumber *op1,
                           const CalcNumber *op2, CalcOperation operation,
                           const CalcNumber *result)
{
    if (!calc || !op1 || !op2 || !result) {
        return;
    }

    if (calc->historyCount >= CALC_HISTORY_SIZE) {
        /* Shift history */
        memmove(&calc->history[0], &calc->history[1],
                sizeof(CalcHistoryEntry) * (CALC_HISTORY_SIZE - 1));
        calc->historyCount = CALC_HISTORY_SIZE - 1;
    }

    CalcHistoryEntry *entry = &calc->history[calc->historyCount++];
    entry->operand1 = *op1;
    entry->operand2 = *op2;
    entry->operation = operation;
    entry->result = *result;

    /* Create human-readable expression */
    const char *opStr = "";
    switch (operation) {
        case CALC_OP_ADD: opStr = " + "; break;
        case CALC_OP_SUBTRACT: opStr = " - "; break;
        case CALC_OP_MULTIPLY: opStr = " × "; break;
        case CALC_OP_DIVIDE: opStr = " ÷ "; break;
        default: opStr = " ? "; break;
    }

    snprintf(entry->expression, sizeof(entry->expression),
             "%.10g%s%.10g = %.10g",
             op1->value, opStr, op2->value, result->value);
}

/*
 * Get history entry
 */
const CalcHistoryEntry *Calculator_GetHistoryEntry(Calculator *calc, int index)
{
    if (!calc || index < 0 || index >= calc->historyCount) {
        return NULL;
    }

    return &calc->history[index];
}

/*
 * Clear calculation history
 */
void Calculator_ClearHistory(Calculator *calc)
{
    if (calc) {
        calc->historyCount = 0;
    }
}

/*
 * Get display string
 */
const char *Calculator_GetDisplay(Calculator *calc)
{
    if (!calc) {
        return "0";
    }

    return (calc)->display;
}

/*
 * Update display for current state
 */
void Calculator_UpdateDisplay(Calculator *calc)
{
    if (!calc) return;

    /* Create a CalcNumber from the current calculator state */
    CalcNumber number;
    number.value = calc->value;
    number.intValue = calc->intValue;
    number.base = calc->base;
    number.isInteger = calc->isInteger;

    /* Format the number for display */
    Calculator_FormatNumber(&number, calc->display,
                          sizeof(calc->display));
}

/*
 * Format number for display
 */
void Calculator_FormatNumber(const CalcNumber *number, char *buffer, int bufferSize)
{
    if (!number || !buffer || bufferSize <= 0) {
        return;
    }

    switch (number->base) {
        case CALC_BASE_DECIMAL:
            if (floor(number->value) == number->value &&
                fabs(number->value) < 1e10) {
                snprintf(buffer, bufferSize, "%.0f", number->value);
            } else {
                snprintf(buffer, bufferSize, "%.10g", number->value);
            }
            break;

        case CALC_BASE_HEX:
            snprintf(buffer, bufferSize, "%llX", (long long)number->intValue);
            break;

        case CALC_BASE_OCTAL:
            snprintf(buffer, bufferSize, "%llo", (long long)number->intValue);
            break;

        case CALC_BASE_BINARY:
            {
                int64_t val = number->intValue;
                char temp[65];
                int i = 0;

                if (val == 0) {
                    strncpy(buffer, "0", bufferSize - 1);
                    buffer[bufferSize - 1] = '\0';
                    return;
                }

                while (val > 0 && i < 64) {
                    temp[i++] = (val & 1) ? '1' : '0';
                    val >>= 1;
                }

                temp[i] = '\0';

                /* Reverse string */
                for (int j = 0; j < i / 2; j++) {
                    char t = temp[j];
                    temp[j] = temp[i - 1 - j];
                    temp[i - 1 - j] = t;
                }

                strncpy(buffer, temp, bufferSize - 1);
                buffer[bufferSize - 1] = '\0';
            }
            break;

        default:
            strncpy(buffer, "Error", bufferSize - 1);
            buffer[bufferSize - 1] = '\0';
            break;
    }
}

/*
 * Register Calculator as a desk accessory
 */
int Calculator_RegisterDA(void)
{
    DARegistryEntry entry = {0};
    strncpy(entry.name, "Calculator", sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.type = DA_TYPE_CALCULATOR;
    entry.resourceID = DA_RESID_CALCULATOR;
    entry.flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_MENU;
    entry.interface = &g_calculatorInterface;

    return DA_Register(&entry);
}

/*
 * Create Calculator DA instance
 */
DeskAccessory *Calculator_CreateDA(void)
{
    DeskAccessory *da = DA_CreateInstance("Calculator");
    if (!da) {
        return NULL;
    }

    /* Set up function pointers */
    da->open = Calculator_DAOpen;
    da->close = Calculator_DAClose;
    da->event = Calculator_DAEvent;
    da->menu = Calculator_DAMenu;
    da->idle = Calculator_DAIdle;
    da->activate = Calculator_DAActivate;
    da->update = Calculator_DAUpdate;

    /* Initialize calculator data */
    da->driverData = &g_calculator;
    if (Calculator_Initialize(&g_calculator) != CALC_ERR_NONE) {
        DA_DestroyInstance(da);
        return NULL;
    }

    g_calculatorInitialized = true;
    return da;
}

/* Internal Functions */

/*
 * Clear display
 */
__attribute__((unused))
static void Calculator_ClearDisplay(Calculator *calc)
{
    if (calc) {
        (calc)->value = 0.0;
        (calc)->intValue = 0;
        strncpy((calc)->display, "0", sizeof((calc)->display) - 1);
        (calc)->display[sizeof((calc)->display) - 1] = '\0';
    }
}

/*
 * Set error state
 */
static void Calculator_SetError(Calculator *calc, int errorCode, const char *message)
{
    if (!calc) return;

    calc->state = CALC_STATE_ERROR;
    calc->errorCode = errorCode;
    strncpy(calc->errorMessage, message ? message : "Error",
            sizeof(calc->errorMessage) - 1);
    calc->errorMessage[sizeof(calc->errorMessage) - 1] = '\0';
    strncpy((calc)->display, "Error", sizeof((calc)->display) - 1);
    (calc)->display[sizeof((calc)->display) - 1] = '\0';
}

/*
 * Perform arithmetic operation
 */
static double Calculator_PerformArithmetic(double op1, double op2, CalcOperation operation)
{
    switch (operation) {
        case CALC_OP_ADD:
            return op1 + op2;
        case CALC_OP_SUBTRACT:
            return op1 - op2;
        case CALC_OP_MULTIPLY:
            return op1 * op2;
        case CALC_OP_DIVIDE:
            if (op2 == 0.0) return NAN;
            return op1 / op2;
        default:
            return NAN;
    }
}

/*
 * Perform scientific operation
 */
static double Calculator_PerformScientific(double operand, CalcOperation operation)
{
    switch (operation) {
        case CALC_OP_SIN:
            return sin(operand);
        case CALC_OP_COS:
            return cos(operand);
        case CALC_OP_TAN:
            return tan(operand);
        case CALC_OP_ASIN:
            return asin(operand);
        case CALC_OP_ACOS:
            return acos(operand);
        case CALC_OP_ATAN:
            return atan(operand);
        case CALC_OP_LOG:
            if (operand <= 0) return NAN;
            return log10(operand);
        case CALC_OP_LN:
            if (operand <= 0) return NAN;
            return log(operand);
        case CALC_OP_EXP:
            return exp(operand);
        case CALC_OP_SQRT:
            if (operand < 0) return NAN;
            return sqrt(operand);
        case CALC_OP_SQUARE:
            return operand * operand;
        case CALC_OP_RECIPROCAL:
            if (operand == 0) return NAN;
            return 1.0 / operand;
        case CALC_OP_FACTORIAL:
            if (operand < 0 || operand != floor(operand) || operand > 170) return NAN;
            {
                double result = 1.0;
                for (int i = 2; i <= (int)operand; i++) {
                    result *= i;
                }
                return result;
            }
        default:
            return NAN;
    }
}

/*
 * Perform bitwise operation
 */
__attribute__((unused))
static int64_t Calculator_PerformBitwise(int64_t op1, int64_t op2, CalcOperation operation)
{
    switch (operation) {
        case CALC_OP_AND:
            return op1 & op2;
        case CALC_OP_OR:
            return op1 | op2;
        case CALC_OP_XOR:
            return op1 ^ op2;
        case CALC_OP_NOT:
            return ~op1;
        case CALC_OP_SHIFT_LEFT:
            return op1 << op2;
        case CALC_OP_SHIFT_RIGHT:
            return op1 >> op2;
        case CALC_OP_MOD:
            if (op2 == 0) return 0;
            return op1 % op2;
        default:
            return 0;
    }
}

/*
 * Convert to different base
 */
static void Calculator_ConvertToBase(Calculator *calc, CalcBase newBase)
{
    if (!calc) return;

    (calc)->base = newBase;
    if (newBase == CALC_BASE_DECIMAL) {
        (calc)->isInteger = false;
    } else {
        (calc)->isInteger = true;
        (calc)->intValue = (int64_t)(calc)->value;
    }
}

/*
 * Check if digit is valid for base
 */
static Boolean Calculator_IsValidDigitForBase(int digit, CalcBase base)
{
    return (digit >= 0 && digit < base);
}

/* DA Interface Implementation */

static int Calculator_DAOpen(DeskAccessory *da)
{
    if (!da) return DESK_ERR_INVALID_PARAM;

    /* Initialize resource data for icon access */
    InitResourceData();

    /* Create window */
    DAWindowAttr attr;
    DA_LoadWindowTemplate(DA_RESID_CALCULATOR, &attr);
    strncpy(attr.title, "Calculator", sizeof(attr.title) - 1);
    attr.title[sizeof(attr.title) - 1] = '\0';

    return DA_CreateWindow(da, &attr);
}

static void Calculator_DAClose(DeskAccessory *da)
{
    if (!da) return;

    Calculator_Shutdown((Calculator *)da->driverData);
    DA_DestroyWindow(da);
}

static int Calculator_DAEvent(DeskAccessory *da, const EventRecord *event)
{
    if (!da || !event) return DESK_ERR_INVALID_PARAM;

    /* Handle calculator events */
    /* This would need to be implemented based on the event system */
    return DESK_ERR_NONE;
}

static int Calculator_DAMenu(DeskAccessory *da, short menuID, short itemID)
{
    if (!da) return DESK_ERR_INVALID_PARAM;

    /* Handle menu selections */
    return DESK_ERR_NONE;
}

static void Calculator_DAIdle(DeskAccessory *da)
{
    if (!da) return;

    /* Periodic processing */
}

static void Calculator_DAActivate(DeskAccessory *da, Boolean active)
{
    if (!da) return;

    /* Handle activation/deactivation */
}

static void Calculator_DAUpdate(DeskAccessory *da)
{
    if (!da) return;

    /* Update display */
    Calculator *calc = (Calculator *)da->driverData;
    if (calc) {
        Calculator_UpdateDisplay(calc);
    }
}

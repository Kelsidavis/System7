#include "DeskManager/Calculator.h"
#include "System71StdLib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("%s:%d: calculator regression failed: %s\n", __FILE__, __LINE__, #condition); \
        abort(); \
    } \
} while (0)

/* The native math header declares the kernel classification function. */
int isnan(double value) { return __builtin_isnan(value); }

static void type(Calculator *calc, const char *keys)
{
    for (; *keys; ++keys) CHECK(Calculator_KeyPress(calc, *keys) == CALC_ERR_NONE);
}

static void expect(Calculator *calc, double value, const char *display)
{
    CHECK(fabs(calc->value - value) <= fabs(value) * 1e-14);
    CHECK(strcmp(Calculator_GetDisplay(calc), display) == 0);
}

static void testEntry(void)
{
    Calculator calc;
    CHECK(Calculator_Initialize(&calc) == CALC_ERR_NONE);
    expect(&calc, 0, "0");
    type(&calc, "00012.50");
    expect(&calc, 12.5, "12.50");
    type(&calc, ".");
    expect(&calc, 12.5, "12.50");
    type(&calc, "\b");
    expect(&calc, 12.5, "12.5");
    type(&calc, "\b");
    expect(&calc, 12, "12.");
    type(&calc, "\b");
    expect(&calc, 12, "12");
    CHECK(Calculator_PressButton(&calc, CALC_BTN_NEGATE) == 0);
    expect(&calc, -12, "-12");
    type(&calc, "\b3.04");
    expect(&calc, -13.04, "-13.04");
    CHECK(Calculator_PerformOperation(&calc, CALC_OP_NEGATE) == 0);
    expect(&calc, 13.04, "13.04");
    Calculator_Clear(&calc);
    CHECK(Calculator_PressButton(&calc, CALC_BTN_NEGATE) == 0);
    type(&calc, ".25");
    expect(&calc, -0.25, "-0.25");
    Calculator_ClearAll(&calc);
    type(&calc, ".00000000001");
    expect(&calc, 1e-11, "0.00000000001");
    Calculator_ClearAll(&calc);
    type(&calc, "1234567890123456789012345678901234567890");
    CHECK(strlen(calc.entry) == CALC_DISPLAY_DIGITS - 1);
    char saved[sizeof(calc.entry)];
    strcpy(saved, calc.entry);
    CHECK(Calculator_PressButton(&calc, CALC_BTN_NEGATE) == 0);
    CHECK(strlen(calc.entry) == CALC_DISPLAY_DIGITS);
    type(&calc, "9.");
    CHECK(strcmp(calc.entry + 1, saved) == 0);
    CHECK(Calculator_PressButton(&calc, CALC_BTN_NEGATE) == 0);
    CHECK(strcmp(calc.entry, saved) == 0);
    for (int i = 0; i < 40; ++i) Calculator_Backspace(&calc);
    expect(&calc, 0, "0");
}

static void testOperations(void)
{
    Calculator calc;
    CHECK(Calculator_Initialize(&calc) == 0);
    type(&calc, "12.5+0.25=");
    expect(&calc, 12.75, "12.75");
    type(&calc, "3");
    expect(&calc, 3, "3");
    Calculator_ClearAll(&calc);
    type(&calc, "7+8*");
    expect(&calc, 15, "15");
    type(&calc, "2=");
    expect(&calc, 30, "30");
    Calculator_ClearAll(&calc);
    type(&calc, "5+*3=");
    expect(&calc, 15, "15");
    Calculator_ClearAll(&calc);
    type(&calc, "200+10");
    CHECK(Calculator_PressButton(&calc, CALC_BTN_PERCENT) == 0);
    expect(&calc, 20, "20");
    type(&calc, "=");
    expect(&calc, 220, "220");
    Calculator_ClearAll(&calc);
    type(&calc, "200*10");
    CHECK(Calculator_PressButton(&calc, CALC_BTN_PERCENT) == 0);
    type(&calc, "=");
    expect(&calc, 20, "20");
    Calculator_ClearAll(&calc);
    type(&calc, "10");
    CHECK(Calculator_PressButton(&calc, CALC_BTN_PERCENT) == 0);
    expect(&calc, 0.1, "0.1");
    Calculator_ClearAll(&calc);
    type(&calc, "8/0");
    CHECK(Calculator_KeyPress(&calc, '=') == CALC_ERR_DOMAIN);
    CHECK(calc.state == CALC_STATE_ERROR);
    CHECK(Calculator_KeyPress(&calc, '1') == CALC_ERR_INVALID_OP);
    Calculator_Clear(&calc);
    type(&calc, "2=");
    expect(&calc, 4, "4");
    Calculator_MemoryStore(&calc, 0);
    Calculator_ClearAll(&calc);
    type(&calc, "2+");
    CHECK(Calculator_MemoryRecall(&calc, 0) == 0);
    type(&calc, "*3=");
    expect(&calc, 18, "18");
    Calculator_ClearAll(&calc);
    type(&calc, "99999999999999999999+1=");
    CHECK(calc.state == CALC_STATE_RESULT);
}

static void testHistory(void)
{
    struct {
        unsigned before;
        Calculator calc;
        unsigned after;
    } guarded = { .before = 0x12345678, .after = 0x87654321 };
    Calculator *calc = &guarded.calc;
    CHECK(Calculator_Initialize(calc) == 0);
    CHECK(Calculator_GetHistoryEntry(calc, 0) == NULL);
    for (int i = 0; i < CALC_HISTORY_SIZE + 5; ++i) {
        Calculator_ClearAll(calc);
        for (int digit = 0; digit < i; ++digit) CHECK(Calculator_EnterDigit(calc, 1) == 0);
        type(calc, "+2=");
        const CalcHistoryEntry *entry = Calculator_GetHistoryEntry(calc, calc->historyCount - 1);
        CHECK(entry != NULL);
        CHECK(entry->operand1.value + entry->operand2.value == entry->result.value);
        CHECK(entry->operation == CALC_OP_ADD);
        CHECK(entry->result.intValue == 0);
        CHECK(entry->expression[sizeof(entry->expression) - 1] == '\0');
    }
    CHECK(calc->historyCount == CALC_HISTORY_SIZE);
    CHECK(Calculator_GetHistoryEntry(calc, 0)->operand1.value == 11111.0);
    CHECK(Calculator_GetHistoryEntry(calc, -1) == NULL);
    CHECK(Calculator_GetHistoryEntry(calc, CALC_HISTORY_SIZE) == NULL);
    CHECK(guarded.before == 0x12345678 && guarded.after == 0x87654321);
    Calculator_ClearHistory(calc);
    CHECK(calc->historyCount == 0);
    CHECK(Calculator_GetHistoryEntry(calc, 0) == NULL);
    CHECK(Calculator_Initialize(calc) == 0);
    CHECK(calc->historyCount == 0);
}

static void testBases(void)
{
    Calculator calc;
    CHECK(Calculator_Initialize(&calc) == 0);
    CHECK(Calculator_SetBase(&calc, CALC_BASE_HEX) == CALC_ERR_INVALID_BASE);
    CHECK(Calculator_SetMode(&calc, (CalcMode)99) == CALC_ERR_INVALID_OP);
    CHECK(Calculator_SetMode(&calc, CALC_MODE_PROGRAMMER) == 0);
    CHECK(Calculator_SetBase(&calc, (CalcBase)0) == CALC_ERR_INVALID_BASE);
    CHECK(Calculator_EnterDigit(&calc, 16) == CALC_ERR_INVALID_OP);
    type(&calc, "A+2=");
    expect(&calc, 12, "C");
    const CalcHistoryEntry *entry = Calculator_GetHistoryEntry(&calc, 0);
    CHECK(entry->operand1.intValue == 10 && entry->operand2.intValue == 2);
    CHECK(entry->result.intValue == 12);
    CHECK(strcmp(entry->expression, "A + 2 = C") == 0);
    Calculator_MemoryStore(&calc, 0);
    Calculator_ClearAll(&calc);
    type(&calc, "7F");
    Calculator_Backspace(&calc);
    expect(&calc, 7, "7");
    CHECK(Calculator_MemoryRecall(&calc, 0) == 0);
    expect(&calc, 12, "C");
    CHECK(Calculator_SetMode(&calc, CALC_MODE_BASIC) == 0);
    expect(&calc, 12, "12");
    type(&calc, "2");
    expect(&calc, 2, "2");
    CHECK(Calculator_SetMode(&calc, CALC_MODE_PROGRAMMER) == 0);
    type(&calc, "FFFFFFFFFFFFFFFF");
    CHECK(calc.intValue == -1);
    CHECK(Calculator_PressButton(&calc, CALC_BTN_NEGATE) == 0);
    CHECK(calc.intValue == 1);
    Calculator_ClearAll(&calc);
    type(&calc, "7FFFFFFFFFFFFFFF");
    CHECK(calc.intValue == INT64_MAX);
    CHECK(Calculator_SetBase(&calc, CALC_BASE_OCTAL) == 0);
    CHECK(calc.intValue == INT64_MAX);
    CHECK(Calculator_SetBase(&calc, CALC_BASE_BINARY) == 0);
    CHECK(calc.intValue == INT64_MAX);
    CalcNumber binary = { .intValue = -1, .base = CALC_BASE_BINARY, .isInteger = true };
    char bits[65];
    Calculator_FormatNumber(&binary, bits, sizeof(bits));
    CHECK(strlen(bits) == 64 && strspn(bits, "1") == 64);
    Calculator_ClearAll(&calc);
    CHECK(Calculator_SetMode(&calc, CALC_MODE_BASIC) == 0);
    type(&calc, "9999999999999999999999999");
    CHECK(Calculator_SetMode(&calc, CALC_MODE_PROGRAMMER) == CALC_ERR_OVERFLOW);
    CHECK(calc.state == CALC_STATE_ERROR);
}

int main(void)
{
    testEntry();
    testOperations();
    testHistory();
    testBases();
    puts("Native calculator entry, arithmetic, history, and base regressions passed.");
    return 0;
}

#include "MemoryMgr/MemoryManager.h"
// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>
/*
 * BuiltinDAs.c - Built-in Desk Accessories Registration
 *
 * Registers the built-in desk accessories (Calculator, Key Caps, Alarm Clock,
 * Chooser) with the Desk Manager. Provides the interface between the generic
 * DA system and the specific implementations.
 *
 * Derived from ROM analysis (System 7)
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/DeskManager.h"
#include "DeskManager/DeskAccessory.h"
#include "DeskManager/Calculator.h"
#include "DeskManager/KeyCaps.h"
#include "DeskManager/AlarmClock.h"
#include "DeskManager/Chooser.h"


/* QuickDraw drawing for DAs */
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void FrameRect(const Rect* r);
extern void EraseRect(const Rect* r);
extern void InvertRect(const Rect* r);
extern void DrawText(const void* textBuf, SInt16 firstByte, SInt16 byteCount);
extern void FillRect(const Rect* r, const Pattern* pat);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void PenSize(short width, short height);

/* Forward declarations for DA interfaces */
static int Calculator_DAInitialize(DeskAccessory *da, const DADriverHeader *header);
static int Calculator_DATerminate(DeskAccessory *da);
static int Calculator_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);
static int Calculator_DAHandleMenu(DeskAccessory *da, const DAMenuInfo *menu);

/* ============================================================================
 * Calculator Button Layout & Rendering
 *
 * Classic System 7 Calculator: 200 x 300 window
 * Display area at top, 5 rows x 4 columns of buttons below.
 * Layout matches the real Mac Calculator DA.
 * ============================================================================ */

/* Calculator button geometry */
#define CALC_DISPLAY_TOP    8
#define CALC_DISPLAY_LEFT   8
#define CALC_DISPLAY_RIGHT  192
#define CALC_DISPLAY_BOTTOM 36
#define CALC_BTN_COLS       4
#define CALC_BTN_ROWS       5
#define CALC_BTN_W          42
#define CALC_BTN_H          28
#define CALC_BTN_GAP        4
#define CALC_BTN_START_X    10
#define CALC_BTN_START_Y    44

/* Button layout: 5 rows x 4 columns, matching classic Mac Calculator */
typedef struct {
    CalcButtonID id;
    const char*  label;
} CalcBtnDef;

static const CalcBtnDef kCalcButtons[CALC_BTN_ROWS][CALC_BTN_COLS] = {
    /* Row 0: C  =  /  * */
    { {CALC_BTN_CLEAR, "C"}, {CALC_BTN_EQUALS, "="}, {CALC_BTN_DIVIDE, "/"}, {CALC_BTN_MULTIPLY, "*"} },
    /* Row 1: 7  8  9  - */
    { {CALC_BTN_7, "7"}, {CALC_BTN_8, "8"}, {CALC_BTN_9, "9"}, {CALC_BTN_SUBTRACT, "-"} },
    /* Row 2: 4  5  6  + */
    { {CALC_BTN_4, "4"}, {CALC_BTN_5, "5"}, {CALC_BTN_6, "6"}, {CALC_BTN_ADD, "+"} },
    /* Row 3: 1  2  3  (unused placeholder for tall = button) */
    { {CALC_BTN_1, "1"}, {CALC_BTN_2, "2"}, {CALC_BTN_3, "3"}, {CALC_BTN_CLEAR_ALL, "AC"} },
    /* Row 4: 0 (wide)   .   (placeholder) */
    { {CALC_BTN_0, "0"}, {CALC_BTN_0, ""}, {CALC_BTN_DECIMAL, "."}, {CALC_BTN_NEGATE, "+/-"} },
};

/* Get the rectangle for a button at grid position (row, col) */
static void CalcDA_GetButtonRect(int row, int col, Rect* r) {
    r->left   = CALC_BTN_START_X + col * (CALC_BTN_W + CALC_BTN_GAP);
    r->top    = CALC_BTN_START_Y + row * (CALC_BTN_H + CALC_BTN_GAP);
    r->right  = r->left + CALC_BTN_W;
    r->bottom = r->top + CALC_BTN_H;

    /* Row 4, col 0: "0" button is double-wide */
    if (row == 4 && col == 0) {
        r->right = r->left + CALC_BTN_W * 2 + CALC_BTN_GAP;
    }
}

/* Hit-test: convert local coordinates to CalcButtonID, or -1 if no hit */
static CalcButtonID CalcDA_HitTest(short localH, short localV) {
    for (int row = 0; row < CALC_BTN_ROWS; row++) {
        for (int col = 0; col < CALC_BTN_COLS; col++) {
            /* Skip the second cell of the wide "0" button */
            if (row == 4 && col == 1) continue;

            Rect r;
            CalcDA_GetButtonRect(row, col, &r);

            if (localH >= r.left && localH < r.right &&
                localV >= r.top  && localV < r.bottom) {
                return kCalcButtons[row][col].id;
            }
        }
    }
    return (CalcButtonID)-1;
}

/* Draw the full calculator UI: display + buttons */
static void CalcDA_Draw(DeskAccessory *da) {
    if (!da || !da->driverData) return;
    Calculator *calc = (Calculator *)da->driverData;

    /* Draw display area */
    Rect displayRect = { CALC_DISPLAY_TOP, CALC_DISPLAY_LEFT,
                         CALC_DISPLAY_BOTTOM, CALC_DISPLAY_RIGHT };
    EraseRect(&displayRect);
    FrameRect(&displayRect);

    /* Draw display text (right-aligned) */
    const char* dispStr = Calculator_GetDisplay(calc);
    int len = 0;
    while (dispStr[len]) len++;
    /* Right-align: approx 7px per char */
    short textX = CALC_DISPLAY_RIGHT - 6 - (len * 7);
    if (textX < CALC_DISPLAY_LEFT + 4) textX = CALC_DISPLAY_LEFT + 4;
    MoveTo(textX, CALC_DISPLAY_BOTTOM - 8);
    DrawText(dispStr, 0, len);

    /* Draw buttons */
    for (int row = 0; row < CALC_BTN_ROWS; row++) {
        for (int col = 0; col < CALC_BTN_COLS; col++) {
            if (row == 4 && col == 1) continue;  /* Skip wide-0 placeholder */

            const CalcBtnDef* btn = &kCalcButtons[row][col];
            if (btn->label[0] == '\0') continue;  /* Skip empty */

            Rect r;
            CalcDA_GetButtonRect(row, col, &r);

            /* Draw button frame */
            EraseRect(&r);
            FrameRect(&r);

            /* Draw 3D shadow effect (bottom-right edges) */
            PenSize(1, 1);
            MoveTo(r.left + 1, r.bottom);
            LineTo(r.right, r.bottom);
            MoveTo(r.right, r.top + 1);
            LineTo(r.right, r.bottom);

            /* Draw button label (centered) */
            int labelLen = 0;
            while (btn->label[labelLen]) labelLen++;
            short labelX = r.left + (r.right - r.left - labelLen * 7) / 2;
            short labelY = r.top + (r.bottom - r.top + 10) / 2;
            MoveTo(labelX, labelY);
            DrawText(btn->label, 0, labelLen);
        }
    }
}
static int Calculator_DAIdle(DeskAccessory *da);

static int KeyCaps_DAInitialize(DeskAccessory *da, const DADriverHeader *header);
static int KeyCaps_DATerminate(DeskAccessory *da);
static int KeyCaps_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);

static int AlarmClock_DAInitialize(DeskAccessory *da, const DADriverHeader *header);
static int AlarmClock_DATerminate(DeskAccessory *da);
static int AlarmClock_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);
static int AlarmClock_DAIdle(DeskAccessory *da);

static int Chooser_DAInitialize(DeskAccessory *da, const DADriverHeader *header);
static int Chooser_DATerminate(DeskAccessory *da);
static int Chooser_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);

/* DA Interface implementations */
static DAInterface g_calculatorInterface = {
    .initialize = Calculator_DAInitialize,
    .terminate = Calculator_DATerminate,
    .processEvent = Calculator_DAProcessEvent,
    .handleMenu = Calculator_DAHandleMenu,
    .doEdit = NULL,
    .idle = Calculator_DAIdle,
    .updateCursor = NULL,
    .activate = NULL,
    .update = NULL,
    .resize = NULL,
    .suspend = NULL,
    .resume = NULL,
    .sleep = NULL,
    .wakeup = NULL
};

static DAInterface g_keyCapsInterface = {
    .initialize = KeyCaps_DAInitialize,
    .terminate = KeyCaps_DATerminate,
    .processEvent = KeyCaps_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = NULL,
    .updateCursor = NULL,
    .activate = NULL,
    .update = NULL,
    .resize = NULL,
    .suspend = NULL,
    .resume = NULL,
    .sleep = NULL,
    .wakeup = NULL
};

__attribute__((unused))
static DAInterface g_alarmClockInterface = {
    .initialize = AlarmClock_DAInitialize,
    .terminate = AlarmClock_DATerminate,
    .processEvent = AlarmClock_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = AlarmClock_DAIdle,
    .updateCursor = NULL,
    .activate = NULL,
    .update = NULL,
    .resize = NULL,
    .suspend = NULL,
    .resume = NULL,
    .sleep = NULL,
    .wakeup = NULL
};

__attribute__((unused))
static DAInterface g_chooserInterface = {
    .initialize = Chooser_DAInitialize,
    .terminate = Chooser_DATerminate,
    .processEvent = Chooser_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = NULL,
    .updateCursor = NULL,
    .activate = NULL,
    .update = NULL,
    .resize = NULL,
    .suspend = NULL,
    .resume = NULL,
    .sleep = NULL,
    .wakeup = NULL
};

/*
 * Register built-in desk accessories
 */
int DeskManager_RegisterBuiltinDAs(void)
{
    int result;

    /* Register Calculator */
    DARegistryEntry calculatorEntry = {0};
    strncpy(calculatorEntry.name, "Calculator", sizeof(calculatorEntry.name) - 1);
    calculatorEntry.name[sizeof(calculatorEntry.name) - 1] = '\0';
    calculatorEntry.type = DA_TYPE_CALCULATOR;
    calculatorEntry.resourceID = DA_RESID_CALCULATOR;
    calculatorEntry.flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_TIME | DA_FLAG_NEEDS_MENU;
    calculatorEntry.interface = &g_calculatorInterface;

    result = DA_Register(&calculatorEntry);
    if (result != 0) {
        return result;
    }

    /* Register Key Caps */
    DARegistryEntry keyCapsEntry = {0};
    strncpy(keyCapsEntry.name, "Key Caps", sizeof(keyCapsEntry.name) - 1);
    keyCapsEntry.name[sizeof(keyCapsEntry.name) - 1] = '\0';
    keyCapsEntry.type = DA_TYPE_KEYCAPS;
    keyCapsEntry.resourceID = DA_RESID_KEYCAPS;
    keyCapsEntry.flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_CURSOR;
    keyCapsEntry.interface = &g_keyCapsInterface;

    result = DA_Register(&keyCapsEntry);
    if (result != 0) {
        return result;
    }

    /* TODO: Register Alarm Clock - requires system time library
    DARegistryEntry alarmEntry = {0};
    strncpy(alarmEntry.name, "Alarm Clock", sizeof(alarmEntry.name) - 1);
    alarmEntry.name[sizeof(alarmEntry.name) - 1] = '\0';
    alarmEntry.type = DA_TYPE_ALARM;
    alarmEntry.resourceID = DA_RESID_ALARM;
    alarmEntry.flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_TIME;
    alarmEntry.interface = &g_alarmClockInterface;

    result = DA_Register(&alarmEntry);
    if (result != 0) {
        return result;
    }
    */

    /* TODO: Register Chooser - requires network/device enumeration
    DARegistryEntry chooserEntry = {0};
    strncpy(chooserEntry.name, "Chooser", sizeof(chooserEntry.name) - 1);
    chooserEntry.name[sizeof(chooserEntry.name) - 1] = '\0';
    chooserEntry.type = DA_TYPE_CHOOSER;
    chooserEntry.resourceID = DA_RESID_CHOOSER;
    chooserEntry.flags = DA_FLAG_NEEDS_EVENTS;
    chooserEntry.interface = &g_chooserInterface;

    result = DA_Register(&chooserEntry);
    if (result != 0) {
        return result;
    }
    */

    return DESK_ERR_NONE;
}

/* Calculator Interface Implementation */

static int Calculator_DAInitialize(DeskAccessory *da, const DADriverHeader *header)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate calculator data */
    Calculator *calc = NewPtr(sizeof(Calculator));
    if (!calc) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize calculator */
    int result = Calculator_Initialize(calc);
    if (result != 0) {
        DisposePtr((Ptr)calc);
        return result;
    }

    /* Link to DA */
    da->driverData = calc;

    /* Create window */
    DAWindowAttr attr;
    attr.bounds.left = 100;
    attr.bounds.top = 100;
    attr.bounds.right = 300;     /* 200px wide */
    attr.bounds.bottom = 320;    /* 220px tall - fits display + 5x4 button grid */
    attr.procID = 0;
    attr.visible = true;
    attr.hasGoAway = true;
    attr.refCon = 0;
    strncpy(attr.title, "Calculator", sizeof(attr.title) - 1);
    attr.title[sizeof(attr.title) - 1] = '\0';

    return DA_CreateWindow(da, &attr);
}

static int Calculator_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    Calculator *calc = (Calculator *)da->driverData;
    Calculator_Shutdown(calc);
    DisposePtr((Ptr)calc);
    da->driverData = NULL;

    return DESK_ERR_NONE;
}

static int Calculator_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event)
{
    if (!da || !da->driverData || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    Calculator *calc = (Calculator *)da->driverData;

    /* Convert event to calculator input */
    switch (event->what) {
        case 1: /* mouseDown */
            {
                /* Hit-test against calculator buttons using local coordinates */
                CalcButtonID hitBtn = CalcDA_HitTest(event->h, event->v);
                if ((int)hitBtn >= 0) {
                    /* Visual feedback: briefly invert the button */
                    int row = -1, col = -1;
                    for (int r = 0; r < CALC_BTN_ROWS && row < 0; r++) {
                        for (int c = 0; c < CALC_BTN_COLS; c++) {
                            if (r == 4 && c == 1) continue;
                            if (kCalcButtons[r][c].id == hitBtn) {
                                row = r; col = c; break;
                            }
                        }
                    }
                    if (row >= 0) {
                        Rect r;
                        CalcDA_GetButtonRect(row, col, &r);
                        InvertRect(&r);
                    }

                    /* Process the button press */
                    Calculator_PressButton(calc, hitBtn);

                    /* Redraw the calculator to show updated display */
                    CalcDA_Draw(da);
                }
            }
            break;

        case 3: /* keyDown */
            {
                char key = (char)(event->message & 0xFF);
                Calculator_KeyPress(calc, key);
                /* Redraw after key input */
                CalcDA_Draw(da);
            }
            break;

        case 6: /* updateEvt */
            CalcDA_Draw(da);
            break;

        default:
            break;
    }

    return DESK_ERR_NONE;
}

static int Calculator_DAHandleMenu(DeskAccessory *da, const DAMenuInfo *menu)
{
    if (!da || !da->driverData || !menu) {
        return DESK_ERR_INVALID_PARAM;
    }

    Calculator *calc = (Calculator *)da->driverData;

    /* Handle calculator menu items */
    switch (menu->menuID) {
        case 1: /* Apple menu */
            break;

        case 100: /* Calculator menu */
            switch (menu->itemID) {
                case 1: /* Clear */
                    Calculator_Clear(calc);
                    break;
                case 2: /* Clear All */
                    Calculator_ClearAll(calc);
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    return DESK_ERR_NONE;
}

static int Calculator_DAIdle(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Calculator doesn't need idle processing */
    return DESK_ERR_NONE;
}

/* Key Caps Interface Implementation */

static int KeyCaps_DAInitialize(DeskAccessory *da, const DADriverHeader *header)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate Key Caps data */
    KeyCaps *keyCaps = NewPtr(sizeof(KeyCaps));
    if (!keyCaps) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize Key Caps */
    int result = KeyCaps_Initialize(keyCaps);
    if (result != 0) {
        DisposePtr((Ptr)keyCaps);
        return result;
    }

    /* Link to DA */
    da->driverData = keyCaps;

    /* Create window */
    DAWindowAttr attr;
    attr.bounds.left = 120;
    attr.bounds.top = 120;
    attr.bounds.right = 520;
    attr.bounds.bottom = 320;
    attr.procID = 0;
    attr.visible = true;
    attr.hasGoAway = true;
    attr.refCon = 0;
    strncpy(attr.title, "Key Caps", sizeof(attr.title) - 1);
    attr.title[sizeof(attr.title) - 1] = '\0';

    return DA_CreateWindow(da, &attr);
}

static int KeyCaps_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    KeyCaps *keyCaps = (KeyCaps *)da->driverData;
    KeyCaps_Shutdown(keyCaps);
    DisposePtr((Ptr)keyCaps);
    da->driverData = NULL;

    return DESK_ERR_NONE;
}

static int KeyCaps_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event)
{
    if (!da || !da->driverData || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    KeyCaps *keyCaps = (KeyCaps *)da->driverData;

    /* Convert event to Key Caps input */
    switch (event->what) {
        case 1: /* mouseDown */
            {
                Point point = { .v = event->v, .h = event->h };
                return KeyCaps_HandleClick(keyCaps, point, event->modifiers);
            }

        case 3: /* keyDown */
            {
                UInt8 scanCode = (event->message >> 8) & 0xFF;
                return KeyCaps_HandleKeyPress(keyCaps, scanCode, event->modifiers);
            }

        case 6: /* updateEvt */
            KeyCaps_DrawKeyboard(keyCaps, NULL);
            break;

        default:
            break;
    }

    return DESK_ERR_NONE;
}

/* Alarm Clock Interface Implementation */

static int AlarmClock_DAInitialize(DeskAccessory *da, const DADriverHeader *header)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate Alarm Clock data */
    AlarmClock *clock = NewPtr(sizeof(AlarmClock));
    if (!clock) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize Alarm Clock */
    int result = AlarmClock_Initialize(clock);
    if (result != 0) {
        DisposePtr((Ptr)clock);
        return result;
    }

    /* Link to DA */
    da->driverData = clock;

    /* Create window */
    DAWindowAttr attr;
    attr.bounds.left = 140;
    attr.bounds.top = 140;
    attr.bounds.right = 340;
    attr.bounds.bottom = 240;
    attr.procID = 0;
    attr.visible = true;
    attr.hasGoAway = true;
    attr.refCon = 0;
    strncpy(attr.title, "Alarm Clock", sizeof(attr.title) - 1);
    attr.title[sizeof(attr.title) - 1] = '\0';

    return DA_CreateWindow(da, &attr);
}

static int AlarmClock_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    AlarmClock *clock = (AlarmClock *)da->driverData;
    AlarmClock_Shutdown(clock);
    DisposePtr((Ptr)clock);
    da->driverData = NULL;

    return DESK_ERR_NONE;
}

static int AlarmClock_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event)
{
    if (!da || !da->driverData || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    AlarmClock *clock = (AlarmClock *)da->driverData;

    /* Convert event to Alarm Clock input */
    switch (event->what) {
        case 1: /* mouseDown */
            /* Handle clock clicks */
            break;

        case 6: /* updateEvt */
            AlarmClock_Draw(clock, NULL);
            break;

        default:
            break;
    }

    return DESK_ERR_NONE;
}

static int AlarmClock_DAIdle(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    AlarmClock *clock = (AlarmClock *)da->driverData;

    /* Update time and check alarms */
    AlarmClock_UpdateTime(clock);
    AlarmClock_CheckAlarms(clock);

    /* Redraw clock display every idle cycle to keep time current */
    if (da->window) {
        extern void GetPort(GrafPtr* port);
        extern void SetPort(GrafPtr port);
        GrafPtr savePort;
        GetPort(&savePort);
        SetPort((GrafPtr)da->window);
        AlarmClock_Draw(clock, NULL);
        SetPort(savePort);
    }

    return DESK_ERR_NONE;
}

/* Chooser Interface Implementation */

static int Chooser_DAInitialize(DeskAccessory *da, const DADriverHeader *header)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate Chooser data */
    Chooser *chooser = NewPtr(sizeof(Chooser));
    if (!chooser) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize Chooser */
    int result = Chooser_Initialize(chooser);
    if (result != 0) {
        DisposePtr((Ptr)chooser);
        return result;
    }

    /* Link to DA */
    da->driverData = chooser;

    /* Create window */
    DAWindowAttr attr;
    attr.bounds.left = 160;
    attr.bounds.top = 160;
    attr.bounds.right = 560;
    attr.bounds.bottom = 400;
    attr.procID = 0;
    attr.visible = true;
    attr.hasGoAway = true;
    attr.refCon = 0;
    strncpy(attr.title, "Chooser", sizeof(attr.title) - 1);
    attr.title[sizeof(attr.title) - 1] = '\0';

    return DA_CreateWindow(da, &attr);
}

static int Chooser_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    Chooser *chooser = (Chooser *)da->driverData;
    Chooser_Shutdown(chooser);
    DisposePtr((Ptr)chooser);
    da->driverData = NULL;

    return DESK_ERR_NONE;
}

static int Chooser_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event)
{
    if (!da || !da->driverData || !event) {
        return DESK_ERR_INVALID_PARAM;
    }

    Chooser *chooser = (Chooser *)da->driverData;

    /* Convert event to Chooser input */
    switch (event->what) {
        case 1: /* mouseDown */
            {
                Point point = { .v = event->v, .h = event->h };
                return Chooser_HandleClick(chooser, point, event->modifiers);
            }

        case 3: /* keyDown */
            {
                char key = (char)(event->message & 0xFF);
                return Chooser_HandleKeyPress(chooser, key, event->modifiers);
            }

        case 6: /* updateEvt */
            Chooser_Draw(chooser, NULL);
            break;

        default:
            break;
    }

    return DESK_ERR_NONE;
}

#include "MemoryMgr/MemoryManager.h"
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
#include "EventManager/EventTypes.h"
#include "DeskManager/AlarmClock.h"
#include "DeskManager/Chooser.h"
#include "DeskManager/Notepad.h"
#include "ProcessMgr/ProcessMgr.h"
#include "QuickDraw/QuickDraw.h"

/* Forward declarations for DA interfaces */
static int Calculator_DAInitialize(DeskAccessory *da);
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
static int CalcDA_HitTest(short localH, short localV) {
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
    return -1;
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

static int KeyCaps_DAInitialize(DeskAccessory *da);
static int KeyCaps_DATerminate(DeskAccessory *da);
static int KeyCaps_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);
static int KeyCaps_DAIdle(DeskAccessory *da);

static int AlarmClock_DAInitialize(DeskAccessory *da);
static int AlarmClock_DATerminate(DeskAccessory *da);
static int AlarmClock_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);
static int AlarmClock_DAIdle(DeskAccessory *da);

static int Chooser_DAInitialize(DeskAccessory *da);
static int Chooser_DATerminate(DeskAccessory *da);
static int Chooser_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);

static int Notepad_DAInitialize(DeskAccessory *da);
static int Notepad_DATerminate(DeskAccessory *da);
static int Notepad_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);

/* DA Interface implementations */
static DAInterface g_calculatorInterface = {
    .initialize = Calculator_DAInitialize,
    .terminate = Calculator_DATerminate,
    .processEvent = Calculator_DAProcessEvent,
    .handleMenu = Calculator_DAHandleMenu,
    .doEdit = NULL,
    .idle = NULL,
    .activate = NULL,
    .update = NULL,
    .suspend = NULL,
    .resume = NULL,
};

static DAInterface g_keyCapsInterface = {
    .initialize = KeyCaps_DAInitialize,
    .terminate = KeyCaps_DATerminate,
    .processEvent = KeyCaps_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = KeyCaps_DAIdle,
    .activate = NULL,
    .update = NULL,
    .suspend = NULL,
    .resume = NULL,
};

static DAInterface g_alarmClockInterface = {
    .initialize = AlarmClock_DAInitialize,
    .terminate = AlarmClock_DATerminate,
    .processEvent = AlarmClock_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = AlarmClock_DAIdle,
    .activate = NULL,
    .update = NULL,
    .suspend = NULL,
    .resume = NULL,
};

static DAInterface g_notepadInterface = {
    .initialize = Notepad_DAInitialize,
    .terminate = Notepad_DATerminate,
    .processEvent = Notepad_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = NULL,
    .activate = NULL,
    .suspend = NULL,
    .resume = NULL,
};

static DAInterface g_chooserInterface = {
    .initialize = Chooser_DAInitialize,
    .terminate = Chooser_DATerminate,
    .processEvent = Chooser_DAProcessEvent,
    .handleMenu = NULL,
    .doEdit = NULL,
    .idle = NULL,
    .activate = NULL,
    .update = NULL,
    .suspend = NULL,
    .resume = NULL,
};

/*
 * Register built-in desk accessories
 */
int DeskManager_RegisterBuiltinDAs(void)
{
    static const DARegistryEntry entries[] = {
        {.name = "Calculator", .type = DA_TYPE_CALCULATOR,
         .flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_MENU, .interface = &g_calculatorInterface},
        {.name = "Key Caps", .type = DA_TYPE_KEYCAPS,
         .flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_TIME, .interface = &g_keyCapsInterface},
        {.name = "Alarm Clock", .type = DA_TYPE_ALARM,
         .flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_TIME, .interface = &g_alarmClockInterface},
        {.name = "Chooser", .type = DA_TYPE_CHOOSER,
         .flags = DA_FLAG_NEEDS_EVENTS, .interface = &g_chooserInterface},
        {.name = "Note Pad", .type = DA_TYPE_NOTEPAD,
         .flags = DA_FLAG_NEEDS_EVENTS, .interface = &g_notepadInterface}
    };
    enum { kBuiltinCount = sizeof(entries) / sizeof(entries[0]) };
    Boolean added[kBuiltinCount] = {false};
    int result = DESK_ERR_NONE;

    for (int i = 0; i < kBuiltinCount; ++i) {
        DARegistryEntry *existing = DA_FindRegistryEntry(entries[i].name);
        if (existing) {
            if (existing->interface == entries[i].interface &&
                existing->type == entries[i].type && existing->flags == entries[i].flags) {
                continue;
            }
            result = DESK_ERR_ALREADY_OPEN;
        } else {
            result = DA_Register(&entries[i]);
            added[i] = result == DESK_ERR_NONE;
        }
        if (result != DESK_ERR_NONE) {
            /* Preserve earlier registrations; undo only this call's additions. */
            for (int j = 0; j < i; ++j) {
                if (added[j]) DA_Unregister(entries[j].name);
            }
            return result;
        }
    }
    return DESK_ERR_NONE;
}

/* Calculator Interface Implementation */

static int Calculator_DAInitialize(DeskAccessory *da)
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
    /* The frame, title bar included, around 208 of content: the button grid
     * ends at 200 and an 8-pixel margin. At 320 the bottom row was cut off. */
    attr.bounds.bottom = 330;
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
                int hitBtn = CalcDA_HitTest(event->h, event->v);
                if (hitBtn >= 0) {
                    /* Visual feedback: briefly invert the button */
                    int row = -1, col = -1;
                    for (int r = 0; r < CALC_BTN_ROWS && row < 0; r++) {
                        for (int c = 0; c < CALC_BTN_COLS; c++) {
                            if (r == 4 && c == 1) continue;
                            if (kCalcButtons[r][c].id == (CalcButtonID)hitBtn) {
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
                    Calculator_PressButton(calc, (CalcButtonID)hitBtn);

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

/* Key Caps Interface Implementation */

static int KeyCaps_DAIdle(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }
    KeyCaps_Idle((KeyCaps *)da->driverData, GetCurrentModifiers());
    return DESK_ERR_NONE;
}

static int KeyCaps_DAInitialize(DeskAccessory *da)
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
    attr.bounds.left = 120;     /* the frame around the keyboard KeyCaps.c lays out */
    attr.bounds.top = 120;
    attr.bounds.right = 518;
    attr.bounds.bottom = 312;
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
        case 5: /* autoKey */
            return KeyCaps_HandleKeyPress(keyCaps, (UInt16)event->message, event->modifiers);

        case 6: /* updateEvt */
            KeyCaps_DrawKeyboard(keyCaps);
            break;

        default:
            break;
    }

    return DESK_ERR_NONE;
}

/* Alarm Clock Interface Implementation */

static int AlarmClock_DAInitialize(DeskAccessory *da)
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
    attr.bounds.left = 140;     /* room for the time, as System 7's is */
    attr.bounds.top = 140;
    attr.bounds.right = 300;
    attr.bounds.bottom = 186;
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
    char shown[sizeof(clock->timeString)];
    memcpy(shown, clock->timeString, sizeof(shown));
    AlarmClock_UpdateTime(clock);
    AlarmClock_CheckAlarms(clock);

    /* Redraw when the time shown changes - every idle pass flickered */
    if (da->window && memcmp(shown, clock->timeString, sizeof(shown)) != 0) {
        GrafPtr savePort;
        GetPort(&savePort);
        SetPort((GrafPtr)da->window);
        AlarmClock_Draw(clock, NULL);
        SetPort(savePort);
    }

    return DESK_ERR_NONE;
}

/* Chooser Interface Implementation */

static int Chooser_DAInitialize(DeskAccessory *da)
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

/* Note Pad DA Interface Wrappers */

static int Notepad_DAInitialize(DeskAccessory *da)
{
    if (!da) return DESK_ERR_INVALID_PARAM;

    OSErr err = Notepad_Initialize();
    if (err != noErr) return DESK_ERR_NO_MEMORY;

    WindowPtr noteWindow = NULL;
    err = Notepad_Open(&noteWindow);
    if (err != noErr) return DESK_ERR_NO_MEMORY;

    da->driverData = (void*)noteWindow;
    return DESK_ERR_NONE;
}

static int Notepad_DATerminate(DeskAccessory *da)
{
    if (!da) return DESK_ERR_INVALID_PARAM;
    Notepad_Close();
    Notepad_Shutdown();
    da->driverData = NULL;
    return DESK_ERR_NONE;
}

static int Notepad_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event)
{
    if (!da || !event) return DESK_ERR_INVALID_PARAM;

    /* Convert DAEventInfo to EventRecord for Notepad */
    EventRecord er;
    er.what = event->what;
    er.message = event->message;
    er.when = event->when;
    er.where.h = event->h;
    er.where.v = event->v;
    er.modifiers = event->modifiers;

    Notepad_HandleEvent(&er);

    if (event->what == 6) { /* updateEvt */
        Notepad_Draw();
    }

    return DESK_ERR_NONE;
}

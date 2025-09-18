/*
 * BuiltinDAs.c - Built-in Desk Accessories Registration
 *
 * Registers the built-in desk accessories (Calculator, Key Caps, Alarm Clock,
 * Chooser) with the Desk Manager. Provides the interface between the generic
 * DA system and the specific implementations.
 *
 * Based on Apple's built-in desk accessories from System 7.1
 */

#include "DeskManager/DeskManager.h"
#include "DeskManager/DeskAccessory.h"
#include "DeskManager/Calculator.h"
#include "DeskManager/KeyCaps.h"
#include "DeskManager/AlarmClock.h"
#include "DeskManager/Chooser.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations for DA interfaces */
static int Calculator_DAInitialize(DeskAccessory *da, const DADriverHeader *header);
static int Calculator_DATerminate(DeskAccessory *da);
static int Calculator_DAProcessEvent(DeskAccessory *da, const DAEventInfo *event);
static int Calculator_DAHandleMenu(DeskAccessory *da, const DAMenuInfo *menu);
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
    strcpy(calculatorEntry.name, "Calculator");
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
    strcpy(keyCapsEntry.name, "Key Caps");
    keyCapsEntry.type = DA_TYPE_KEYCAPS;
    keyCapsEntry.resourceID = DA_RESID_KEYCAPS;
    keyCapsEntry.flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_CURSOR;
    keyCapsEntry.interface = &g_keyCapsInterface;

    result = DA_Register(&keyCapsEntry);
    if (result != 0) {
        return result;
    }

    /* Register Alarm Clock */
    DARegistryEntry alarmEntry = {0};
    strcpy(alarmEntry.name, "Alarm Clock");
    alarmEntry.type = DA_TYPE_ALARM;
    alarmEntry.resourceID = DA_RESID_ALARM;
    alarmEntry.flags = DA_FLAG_NEEDS_EVENTS | DA_FLAG_NEEDS_TIME;
    alarmEntry.interface = &g_alarmClockInterface;

    result = DA_Register(&alarmEntry);
    if (result != 0) {
        return result;
    }

    /* Register Chooser */
    DARegistryEntry chooserEntry = {0};
    strcpy(chooserEntry.name, "Chooser");
    chooserEntry.type = DA_TYPE_CHOOSER;
    chooserEntry.resourceID = DA_RESID_CHOOSER;
    chooserEntry.flags = DA_FLAG_NEEDS_EVENTS;
    chooserEntry.interface = &g_chooserInterface;

    result = DA_Register(&chooserEntry);
    if (result != 0) {
        return result;
    }

    return DESK_ERR_NONE;
}

/* Calculator Interface Implementation */

static int Calculator_DAInitialize(DeskAccessory *da, const DADriverHeader *header)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate calculator data */
    Calculator *calc = malloc(sizeof(Calculator));
    if (!calc) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize calculator */
    int result = Calculator_Initialize(calc);
    if (result != 0) {
        free(calc);
        return result;
    }

    /* Link to DA */
    da->driverData = calc;

    /* Create window */
    DAWindowAttr attr;
    attr.bounds.left = 100;
    attr.bounds.top = 100;
    attr.bounds.right = 300;
    attr.bounds.bottom = 400;
    attr.procID = 0;
    attr.visible = true;
    attr.goAwayFlag = true;
    attr.refCon = 0;
    strcpy(attr.title, "Calculator");

    return DA_CreateWindow(da, &attr);
}

static int Calculator_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    Calculator *calc = (Calculator *)da->driverData;
    Calculator_Shutdown(calc);
    free(calc);
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
            /* Handle button clicks */
            /* This would need coordinate-to-button mapping */
            break;

        case 3: /* keyDown */
            {
                char key = (char)(event->message & 0xFF);
                return Calculator_KeyPress(calc, key);
            }

        case 6: /* updateEvt */
            Calculator_UpdateDisplay(calc);
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
    KeyCaps *keyCaps = malloc(sizeof(KeyCaps));
    if (!keyCaps) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize Key Caps */
    int result = KeyCaps_Initialize(keyCaps);
    if (result != 0) {
        free(keyCaps);
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
    attr.goAwayFlag = true;
    attr.refCon = 0;
    strcpy(attr.title, "Key Caps");

    return DA_CreateWindow(da, &attr);
}

static int KeyCaps_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    KeyCaps *keyCaps = (KeyCaps *)da->driverData;
    KeyCaps_Shutdown(keyCaps);
    free(keyCaps);
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
                Point point = {event->where.h, event->where.v};
                return KeyCaps_HandleClick(keyCaps, point, event->modifiers);
            }

        case 3: /* keyDown */
            {
                uint8_t scanCode = (event->message >> 8) & 0xFF;
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
    AlarmClock *clock = malloc(sizeof(AlarmClock));
    if (!clock) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize Alarm Clock */
    int result = AlarmClock_Initialize(clock);
    if (result != 0) {
        free(clock);
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
    attr.goAwayFlag = true;
    attr.refCon = 0;
    strcpy(attr.title, "Alarm Clock");

    return DA_CreateWindow(da, &attr);
}

static int AlarmClock_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    AlarmClock *clock = (AlarmClock *)da->driverData;
    AlarmClock_Shutdown(clock);
    free(clock);
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

    return DESK_ERR_NONE;
}

/* Chooser Interface Implementation */

static int Chooser_DAInitialize(DeskAccessory *da, const DADriverHeader *header)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Allocate Chooser data */
    Chooser *chooser = malloc(sizeof(Chooser));
    if (!chooser) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Initialize Chooser */
    int result = Chooser_Initialize(chooser);
    if (result != 0) {
        free(chooser);
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
    attr.goAwayFlag = true;
    attr.refCon = 0;
    strcpy(attr.title, "Chooser");

    return DA_CreateWindow(da, &attr);
}

static int Chooser_DATerminate(DeskAccessory *da)
{
    if (!da || !da->driverData) {
        return DESK_ERR_INVALID_PARAM;
    }

    Chooser *chooser = (Chooser *)da->driverData;
    Chooser_Shutdown(chooser);
    free(chooser);
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
                Point point = {event->where.h, event->where.v};
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
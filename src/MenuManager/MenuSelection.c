/* #include "SystemTypes.h" */
#include <string.h>
/*
 * MenuSelection.c - Menu Tracking and Selection Implementation
 *
 * This file implements menu selection functionality including mouse tracking,
 * command key processing, menu choice dispatch, and user interaction handling.
 * It provides the complete menu interaction system for Mac OS compatibility.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Menu Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuInternalTypes.h"
#include "MenuManager/MenuSelection.h"
#include "MenuManager/MenuDisplay.h"

/* Serial output functions */
extern void serial_serial_printf(const char* format, ...);

/* Menu item standard height */
#define menuItemStdHeight 16

/* Forward declaration */
typedef struct MenuTrackingState MenuTrackingState;

/* ============================================================================
 * Menu Tracking State Definition
 * ============================================================================ */

typedef struct MenuTrackingState {
    Boolean isTracking;
    short currentMenu;
    short currentItem;
    short previousMenu;
    short previousItem;
    Point startPoint;
    Point currentPoint;
    Boolean mouseWasDown;
    unsigned long startTime;
    short trackingState;
    Rect currentMenuRect;
    MenuHandle currentMenuHandle;
    Boolean inMenu;
    short menuBarItem;
    Handle savedBits;
    Boolean inited;
    unsigned long trackStartTime;
    Handle savedRegion;
    Point lastMousePoint;
    Point mousePoint;
    Boolean mouseDown;
    Boolean mouseMoved;
    short h;
    short v;
    unsigned long lastMoveTime;
} MenuTrackingState;

/* Tracking states */
enum {
    kMenuTrackingNone = 0,
    kMenuTrackingIdle = 0,      /* Alias for None */
    kMenuTrackingMenuBar = 1,
    kMenuTrackingPullDown = 2,
    kMenuTrackingPopUp = 3,
    kMenuTrackingHierarchical = 4
};

/* Menu selection result codes */
enum {
    kMenuSelectionValid = 0,
    kMenuSelectionCancelled = 1,
    kMenuNoSelection = 2,
    kMenuSelectionError = -1
};

/* ============================================================================
 * Selection State and Tracking
 * ============================================================================ */

static MenuTrackingState gTrackingState;
static MenuSelection gLastSelection;
static Boolean gTrackingActive = false;
static long gLastMenuChoice = 0;

/* Platform function prototypes */
extern Boolean Platform_TrackMouse(Point* mousePt, Boolean* mouseDown);
extern Boolean Platform_GetKeyModifiers(unsigned long* modifiers);
extern void Platform_SetMenuCursor(short cursorType);
extern Boolean Platform_IsMenuVisible(MenuHandle theMenu);
extern void Platform_MenuFeedback(short feedbackType, short menuID, short item);

/* External function from MenuTitleTracking.c */
extern short FindMenuAtPoint_Internal(Point pt);

/* External function from MenuTrack.c */
extern long TrackMenu(short menuID, Point startPt);

/* Internal function prototypes */
static void InitializeTrackingState(MenuTrackInfo* state);
static void CleanupTrackingState(MenuTrackInfo* state);
static Boolean HandleMenuBarTracking(Point mousePt, MenuTrackInfo* state);
static Boolean HandlePullDownTracking(Point mousePt, MenuTrackInfo* state);
static short FindMenuAtPoint(Point pt);
static short FindMenuItemAtPoint(MenuHandle theMenu, Point pt, const Rect* menuRect);
/* IsPointInMenuBar is declared in MenuSelection.h */
static Boolean IsPointInMenu(Point pt, const Rect* menuRect);
static void UpdateMenuHighlight(MenuTrackInfo* state, short newMenu, short newItem);
static void ShowMenuAtPoint(short menuID, Point pt);
static void HideCurrentMenu(void);
static Boolean ProcessMenuCommand(short cmdChar, unsigned long modifiers, MenuSelection* result);
static void FlashMenuFeedback(short menuID, short item);
static Boolean ValidateMenuSelection(short menuID, short item);

/* Mouse and timing utilities */
static void GetCurrentMouseState(Point* mousePt, Boolean* mouseDown, unsigned long* modifiers);
static Boolean WaitForMouseChange(unsigned long timeout);
static unsigned long GetCurrentTime(void);

/* ============================================================================
 * Menu Selection Core Functions
 * ============================================================================ */

/*
 * MenuSelect - Track menu selection
 */
long MenuSelect(Point startPt)
{
    serial_printf("MenuSelect: startPt = (h=%d, v=%d)\n", startPt.h, startPt.v);

    /* Simple implementation - just detect which menu was clicked */
    /* Check if click is in menu bar */
    if (startPt.v < 0 || startPt.v >= 20) {
        serial_printf("MenuSelect: Not in menu bar (v=%d)\n", startPt.v);
        return 0;
    }

    /* Find which menu was clicked using the tracking system */
    short menuID = FindMenuAtPoint_Internal(startPt);

    if (menuID != 0) {
        serial_printf("MenuSelect: Found menu ID %d at (h=%d,v=%d)\n",
                     menuID, startPt.h, startPt.v);

        /* Highlight the menu title */
        extern void HiliteMenu(short menuID);
        HiliteMenu(menuID);

        /* Show dropdown and track item selection */
        long trackResult = TrackMenu(menuID, startPt);

        long result;
        if (trackResult != 0) {
            /* User selected an item - TrackMenu already returns packed format */
            result = trackResult;
            short item = trackResult & 0xFFFF;
            serial_printf("MenuSelect: User selected item %d from menu %d\n", item, menuID);
        } else {
            /* User cancelled or clicked outside */
            result = 0;
            serial_printf("MenuSelect: User cancelled menu selection\n");
        }

        /* Unhighlight the menu title */
        extern void HiliteMenu(short menuID);
        HiliteMenu(0);

        gLastMenuChoice = result;
        serial_printf("MenuSelect: Returning 0x%08lX\n", result);
        return result;
    }

    serial_printf("MenuSelect: No menu found at (h=%d,v=%d)\n", startPt.h, startPt.v);
    return 0;
}

/*
 * MenuSelectEx - Extended menu selection with options
 */
short MenuSelectEx(Point startPt, MenuTrackInfo* trackInfo, MenuSelection* selection)
{
    Point currentPt;
    Boolean mouseDown;
    unsigned long modifiers;
    Boolean isInMenuBar, isInMenu;
    short currentMenu = 0;
    short currentItem = 0;

    if (selection == NULL) {
        return kMenuSelectionError;
    }

    /* Initialize selection result */
    selection->menuID = 0;
    selection->itemID = 0;
    selection->menuChoice = 0;
    selection->valid = false;
    selection->cancelled = false;
    selection->clickPoint = startPt;

    /* Set up tracking if not provided */
    if (trackInfo == NULL) {
        trackInfo = &gTrackingState;
        InitializeTrackingState(trackInfo);
    }

    /* Begin tracking session */
    BeginMenuTracking(trackInfo);

    /* Check initial mouse position */
    GetCurrentMouseState(&currentPt, &mouseDown, &modifiers);
    isInMenuBar = IsPointInMenuBar(startPt); /* Use startPt, not currentPt for initial check */

    if (!isInMenuBar) {
        /* Click was not in menu bar - no selection */
        selection->cancelled = true;
        EndMenuTracking(trackInfo);
        return kMenuSelectionCancelled;
    }

    serial_printf("Starting menu selection at (%d,%d), inMenuBar=%s\n",
           startPt.h, startPt.v, isInMenuBar ? "Yes" : "No");

    /* Main tracking loop */
    int loopCount = 0; /* Add loop counter to prevent infinite loops */
    const int MAX_LOOPS = 1000; /* Prevent infinite loops */

    while ((mouseDown || currentMenu != 0) && loopCount < MAX_LOOPS) {
        loopCount++;
        GetCurrentMouseState(&currentPt, &mouseDown, &modifiers);

        /* Update tracking state */
        UpdateMenuTracking(trackInfo, currentPt, mouseDown);

        isInMenuBar = IsPointInMenuBar(currentPt);
        isInMenu = (currentMenu != 0) && IsPointInMenu(currentPt, &trackInfo->currentMenuRect);

        if (isInMenuBar) {
            /* Handle menu bar tracking */
            if (HandleMenuBarTracking(currentPt, trackInfo)) {
                short newMenu = FindMenuAtPoint(currentPt);
                if (newMenu != currentMenu) {
                    /* Switched to different menu */
                    if (currentMenu != 0) {
                        HideCurrentMenu();
                    }
                    if (newMenu != 0) {
                        ShowMenuAtPoint(newMenu, currentPt);
                    }
                    currentMenu = newMenu;
                    currentItem = 0;
                    UpdateMenuHighlight(trackInfo, currentMenu, currentItem);
                }
            }
        } else if (isInMenu && currentMenu != 0) {
            /* Handle pull-down menu tracking */
            if (HandlePullDownTracking(currentPt, trackInfo)) {
                MenuHandle theMenu = GetMenuHandle(currentMenu);
                if (theMenu != NULL) {
                    short newItem = FindMenuItemAtPoint(theMenu, currentPt, &trackInfo->currentMenuRect);
                    if (newItem != currentItem) {
                        currentItem = newItem;
                        UpdateMenuHighlight(trackInfo, currentMenu, currentItem);
                    }
                }
            }
        } else if (!mouseDown && !isInMenuBar && !isInMenu) {
            /* Mouse released outside menu AND menu bar - cancel selection */
            if (currentMenu != 0) {
                HideCurrentMenu();
                currentMenu = 0;
            }
            currentItem = 0;
            break;
        }

        /* Check for mouse release in menu item */
        if (!mouseDown && currentMenu != 0 && currentItem != 0) {
            /* Validate selection */
            if (ValidateMenuSelection(currentMenu, currentItem)) {
                selection->menuID = currentMenu;
                selection->itemID = currentItem;
                selection->menuChoice = PackMenuChoice(currentMenu, currentItem);
                selection->valid = true;
                selection->finalMousePoint = currentPt;
                selection->selectionTime = GetCurrentTime();

                /* Provide feedback */
                FlashMenuFeedback(currentMenu, currentItem);

                /* serial_printf("Menu selection: menu %d, item %d\n", currentMenu, currentItem); */
                break;
            } else {
                /* Invalid selection - cancel */
                currentItem = 0;
            }
        }

        /* Small delay to prevent excessive CPU usage */
        WaitForMouseChange(1); /* 1 tick delay */
    }

    /* Clean up */
    if (currentMenu != 0) {
        HideCurrentMenu();
        UpdateMenuHighlight(trackInfo, 0, 0);
    }

    /* End tracking session */
    EndMenuTracking(trackInfo);

    /* Set result based on selection */
    if (selection->valid) {
        return kMenuSelectionValid;
    } else if (selection->cancelled) {
        return kMenuSelectionCancelled;
    } else {
        return kMenuNoSelection;
    }
}

/*
 * MenuKey - Process command key equivalent
 */
long MenuKey(short ch)
{
    MenuCmdSearch search;
    unsigned long modifiers;

    /* Get current modifier keys */
    Platform_GetKeyModifiers(&modifiers);

    /* Search for command key */
    return MenuKeyEx(ch, modifiers, &search);
}

/*
 * MenuKeyEx - Extended command key processing
 */
long MenuKeyEx(short keyChar, unsigned long modifiers, MenuCmdSearch* search)
{
    long result = 0;

    /* Set up search parameters */
    if (search == NULL) {
        search = &(MenuCmdSearch){0};
    }

    search->searchChar = keyChar;
    search->modifiers = modifiers;
    search->found = false;
    search->enabled = false;

    /* Search for command key in all menus */
    if (FindMenuCommand(keyChar, modifiers, search)) {
        if (search->enabled) {
            /* Execute the command */
            result = PackMenuChoice(search->foundMenuID, search->foundItem);

            /* Provide feedback */
            FlashMenuFeedback(search->foundMenuID, search->foundItem);

            serial_printf("Command key '%c' found: menu %d, item %d\n",
                   keyChar, search->foundMenuID, search->foundItem);
        } else {
            serial_printf("Command key '%c' found but disabled: menu %d, item %d\n",
                   keyChar, search->foundMenuID, search->foundItem);
        }
    } else {
        /* serial_printf("Command key '%c' not found\n", keyChar); */
    }

    /* Store last choice */
    gLastMenuChoice = result;

    return result;
}

/*
 * MenuChoice - Get last menu choice
 */
long MenuChoice(void)
{
    return gLastMenuChoice;
}

/* ============================================================================
 * Menu Tracking Functions
 * ============================================================================ */

/*
 * TrackMenuBar - Track mouse in menu bar
 */
short TrackMenuBar(Point startPt, MenuTrackInfo* trackInfo)
{
    Point currentPt;
    Boolean mouseDown;
    short menuUnderMouse = 0;

    if (!IsPointInMenuBar(startPt)) {
        return 0;
    }

    /* Find menu under initial point */
    menuUnderMouse = FindMenuAtPoint(startPt);

    /* serial_printf("Tracking menu bar starting at menu %d\n", menuUnderMouse); */

    /* Track mouse until it leaves menu bar or button is released */
    do {
        GetCurrentMouseState(&currentPt, &mouseDown, NULL);

        if (IsPointInMenuBar(currentPt)) {
            short newMenu = FindMenuAtPoint(currentPt);
            if (newMenu != menuUnderMouse) {
                menuUnderMouse = newMenu;
                if (trackInfo != NULL) {
                    trackInfo->menuBarItem = menuUnderMouse;
                }
            }
        } else {
            menuUnderMouse = 0;
        }

        WaitForMouseChange(1);
    } while (mouseDown && IsPointInMenuBar(currentPt));

    return menuUnderMouse;
}

/*
 * TrackPullDownMenu - Track pull-down menu selection
 */
short TrackPullDownMenu(MenuHandle theMenu, const Rect* menuRect,
                       Point startPt, MenuTrackInfo* trackInfo)
{
    Point currentPt;
    Boolean mouseDown;
    short itemUnderMouse = 0;
    short lastItem = 0;

    if (theMenu == NULL || menuRect == NULL) {
        return 0;
    }

    /* serial_printf("Tracking pull-down menu %d\n", (*(MenuInfo**)theMenu)->menuID); */

    /* Track mouse in menu */
    do {
        GetCurrentMouseState(&currentPt, &mouseDown, NULL);

        if (IsPointInMenu(currentPt, menuRect)) {
            itemUnderMouse = FindMenuItemAtPoint(theMenu, currentPt, menuRect);
            if (itemUnderMouse != lastItem) {
                /* Highlight changed */
                if (lastItem > 0) {
                    HiliteMenuItem(theMenu, lastItem, false);
                }
                if (itemUnderMouse > 0) {
                    HiliteMenuItem(theMenu, itemUnderMouse, true);
                }
                lastItem = itemUnderMouse;

                if (trackInfo != NULL) {
                    trackInfo->currentItem = itemUnderMouse;
                }
            }
        } else {
            /* Mouse outside menu */
            if (lastItem > 0) {
                HiliteMenuItem(theMenu, lastItem, false);
                lastItem = 0;
                itemUnderMouse = 0;
            }
        }

        WaitForMouseChange(1);
    } while (mouseDown);

    /* Clean up highlighting */
    if (lastItem > 0) {
        HiliteMenuItem(theMenu, lastItem, false);
    }

    return itemUnderMouse;
}

/* ============================================================================
 * Command Key Processing
 * ============================================================================ */

/*
 * FindMenuCommand - Find menu item with command key
 */
Boolean FindMenuCommand(short cmdChar, unsigned long modifiers, MenuCmdSearch* search)
{
    /* TODO: Need proper implementation that doesn't access MenuManagerState internals */
    if (search == NULL) {
        return false;
    }

    search->found = false;
    search->foundMenuID = 0;
    search->foundItem = 0;
    search->enabled = false;

    /* Simplified implementation for now - just return false */
    return false;

#if 0  /* Original code that needs refactoring */
    MenuManagerState* state = GetMenuManagerState();
    Handle menuList;
    MenuBarList* menuBar;

    /* Search through all menus in menu bar */
    for (int m = 0; m < menuBar->numMenus; m++) {
        MenuHandle theMenu = GetMenuHandle(menuBar->menus[m].menuID);
        if (theMenu == NULL) {
            continue;
        }

        short itemCount = CountMItems(theMenu);

        /* Search through all items in menu */
        for (short i = 1; i <= itemCount; i++) {
            short itemCmd;
            GetItemCmd(theMenu, i, &itemCmd);

            if (itemCmd == cmdChar) {
                /* Found matching command key */
                search->found = true;
                search->foundMenu = theMenu;
                search->foundItem = i;

                /* Check if item is enabled */
                long enableFlags = (*theMenu)->enableFlags;
                search->enabled = IsMenuItemEnabled(enableFlags, i);

                serial_printf("Found command key '%c' in menu %d, item %d (enabled: %s)\n",
                       cmdChar, (*theMenu)->menuID, i, search->enabled ? "Yes" : "No");

                return true;
            }
        }
    }

    return false;
#endif
}

/*
 * IsValidMenuCommand - Check if menu command is valid
 */
Boolean IsValidMenuCommand(short menuID, short item)
{
    MenuHandle theMenu = GetMenuHandle(menuID);
    if (theMenu == NULL) {
        return false;
    }

    if (item < 1 || item > CountMItems(theMenu)) {
        return false;
    }

    /* Check if item is enabled */
    long enableFlags = (*(MenuInfo**)theMenu)->enableFlags;
    return IsMenuItemEnabled(enableFlags, item);
}

/*
 * ExecuteMenuCommand - Execute menu command
 */
Boolean ExecuteMenuCommand(short menuID, short item, Boolean flash)
{
    if (!IsValidMenuCommand(menuID, item)) {
        return false;
    }

    if (flash) {
        FlashMenuFeedback(menuID, item);
    }

    /* Store as last choice */
    gLastMenuChoice = PackMenuChoice(menuID, item);

    /* serial_printf("Executed menu command: menu %d, item %d\n", menuID, item); */

    return true;
}

/* ============================================================================
 * Menu Selection State Management
 * ============================================================================ */

/*
 * BeginMenuTracking - Begin menu tracking session
 */
void BeginMenuTracking(MenuTrackInfo* trackInfo)
{
    if (trackInfo == NULL) {
        trackInfo = &gTrackingState;
    }

    InitializeTrackingState(trackInfo);
    gTrackingActive = true;

    /* serial_printf("Beginning menu tracking session\n"); */
}

/*
 * EndMenuTracking - End menu tracking session
 */
void EndMenuTracking(MenuTrackInfo* trackInfo)
{
    if (trackInfo == NULL) {
        trackInfo = &gTrackingState;
    }

    CleanupTrackingState(trackInfo);
    gTrackingActive = false;

    /* serial_printf("Ending menu tracking session\n"); */
}

/*
 * UpdateMenuTracking - Update menu tracking state
 */
void UpdateMenuTracking(MenuTrackInfo* trackInfo, Point mousePt, Boolean mouseDown)
{
    if (trackInfo == NULL) {
        return;
    }

    trackInfo->lastMousePoint = trackInfo->mousePoint;
    trackInfo->mousePoint = mousePt;
    trackInfo->mouseDown = mouseDown;
    trackInfo->mouseMoved = (mousePt.h != (trackInfo)->h ||
                            mousePt.v != (trackInfo)->v);
    trackInfo->lastMoveTime = GetCurrentTime();
}

/*
 * GetCurrentMenuSelection - Get current menu selection
 */
void GetCurrentMenuSelection(const MenuTrackInfo* trackInfo, MenuSelection* selection)
{
    if (trackInfo == NULL || selection == NULL) {
        return;
    }

    memset(selection, 0, sizeof(MenuSelection));

    if (trackInfo->currentMenu != 0) {
        selection->menuID = trackInfo->currentMenu;
        selection->itemID = trackInfo->currentItem;
        selection->menuChoice = PackMenuChoice(selection->menuID, selection->itemID);
        selection->valid = (selection->itemID > 0);
        selection->clickPoint = trackInfo->mousePoint;
    }
}

/* ============================================================================
 * Menu Feedback and Animation
 * ============================================================================ */

/*
 * FlashMenuSelection - Flash selected menu item
 */
void FlashMenuSelection(short menuID, short item, short flashes)
{
    MenuHandle theMenu = GetMenuHandle(menuID);
    if (theMenu == NULL || item < 1) {
        return;
    }

    /* Flash menu title first */
    for (short i = 0; i < flashes; i++) {
        HiliteMenu(menuID);
        WaitForMouseChange(4); /* Brief delay */
        HiliteMenu(0);
        WaitForMouseChange(4);
    }
}

/*
 * AnimateMenuSelection - Animate menu selection
 */
void AnimateMenuSelection(const MenuSelection* selection, short animation)
{
    if (selection == NULL || !selection->valid) {
        return;
    }

    serial_printf("Animating menu selection: menu %d, item %d (animation %d)\n",
           selection->menuID, selection->itemID, animation);

    /* TODO: Implement selection animation */
}

/*
 * PlayMenuSound - Play menu sound effect
 */
void PlayMenuSound(short soundType)
{
    /* serial_printf("Playing menu sound type %d\n", soundType); */

    /* TODO: Implement sound playback */
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/*
 * PackMenuChoice - Pack menu choice into long
 */
long PackMenuChoice(short menuID, short item)
{
    return MakeMenuChoice(menuID, item);
}

/*
 * UnpackMenuChoice - Unpack menu choice from long
 */
void UnpackMenuChoice(long menuChoice, short* menuID, short* item)
{
    if (menuID != NULL) {
        *menuID = MenuChoiceID(menuChoice);
    }
    if (item != NULL) {
        *item = MenuChoiceItem(menuChoice);
    }
}

/*
 * IsMenuChoiceValid - Check if menu choice is valid
 */
Boolean IsMenuChoiceValid(long menuChoice)
{
    short menuID = MenuChoiceID(menuChoice);
    short item = MenuChoiceItem(menuChoice);

    if (menuID == 0 || item == 0) {
        return false;
    }

    return IsValidMenuCommand(menuID, item);
}

/*
 * CompareMenuChoices - Compare two menu choices
 */
Boolean CompareMenuChoices(long choice1, long choice2)
{
    return choice1 == choice2;
}

/* ============================================================================
 * Mouse and Event Handling
 * ============================================================================ */

/*
 * GetMenuMouseState - Get current mouse state for menus
 */
void GetMenuMouseState(Point* mousePt, Boolean* mouseDown, unsigned long* modifiers)
{
    GetCurrentMouseState(mousePt, mouseDown, modifiers);
}

/*
 * WaitForMouseMove - Wait for mouse movement
 */
Boolean WaitForMouseMove(unsigned long timeout)
{
    return WaitForMouseChange(timeout);
}

/*
 * ConvertMenuPoint - Convert point between coordinate systems
 */
void ConvertMenuPoint(Point* pt, Boolean fromGlobal)
{
    /* TODO: Implement coordinate conversion */
    /* For now, assume all coordinates are global */
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/*
 * InitializeTrackingState - Initialize tracking state
 */
static void InitializeTrackingState(MenuTrackInfo* state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(MenuTrackingState));
    state->trackingState = kMenuTrackingIdle;
    state->trackStartTime = GetCurrentTime();
}

/*
 * CleanupTrackingState - Clean up tracking state
 */
static void CleanupTrackingState(MenuTrackInfo* state)
{
    if (state == NULL) {
        return;
    }

    /* Clean up any allocated resources */
    if (state->savedRegion != NULL) {
        /* TODO: Dispose region */
    }

    if (state->savedBits != NULL) {
        DisposeMenuBits(state->savedBits);
        state->savedBits = NULL;
    }
}

/*
 * HandleMenuBarTracking - Handle tracking in menu bar
 */
static Boolean HandleMenuBarTracking(Point mousePt, MenuTrackInfo* state)
{
    if (state == NULL) {
        return false;
    }

    /* Set tracking state for menu bar */
    state->trackingState = kMenuTrackingMenuBar;

    return true;
}

/*
 * HandlePullDownTracking - Handle tracking in pull-down menu
 */
static Boolean HandlePullDownTracking(Point mousePt, MenuTrackInfo* state)
{
    if (state == NULL) {
        return false;
    }

    state->inMenu = true;
    state->trackingState = kMenuTrackingPullDown;

    return true;
}

/*
 * FindMenuAtPoint - Find menu at point
 */
static short FindMenuAtPoint(Point pt)
{
    return FindMenuAtPoint_Internal(pt);
}

/*
 * FindMenuItemAtPoint - Find menu item at point
 */
static short FindMenuItemAtPoint(MenuHandle theMenu, Point pt, const Rect* menuRect)
{
    if (theMenu == NULL || menuRect == NULL) {
        return 0;
    }

    if (!IsPointInMenu(pt, menuRect)) {
        return 0;
    }

    /* Calculate which item is at this point */
    short itemCount = CountMItems(theMenu);
    short itemHeight = menuItemStdHeight;
    short itemY = pt.v - menuRect->top - 4; /* Account for top margin */

    if (itemY < 0) {
        return 0;
    }

    short item = (itemY / itemHeight) + 1;

    if (item > itemCount) {
        return 0;
    }

    return item;
}

/*
 * IsPointInMenuBar - Check if point is in menu bar
 */
Boolean IsPointInMenuBar(Point pt)
{
    return (pt.v >= 0 && pt.v < 20); /* Standard menu bar height */
}

/*
 * IsPointInMenu - Check if point is in menu
 */
static Boolean IsPointInMenu(Point pt, const Rect* menuRect)
{
    if (menuRect == NULL) {
        return false;
    }

    return PtInRect(pt, menuRect);
}

/*
 * UpdateMenuHighlight - Update menu highlighting
 */
static void UpdateMenuHighlight(MenuTrackInfo* state, short newMenu, short newItem)
{
    if (state == NULL) {
        return;
    }

    /* Update menu highlight */
    if (newMenu != state->menuBarItem) {
        if (state->menuBarItem > 0) {
            HiliteMenu(0); /* Unhighlight old menu */
        }
        if (newMenu > 0) {
            HiliteMenu(newMenu); /* Highlight new menu */
        }
        state->menuBarItem = newMenu;
    }

    /* Update item highlight */
    if (newItem != state->currentItem) {
        if (state->currentMenu != NULL && state->currentItem > 0) {
            HiliteMenuItem(state->currentMenu, state->currentItem, false);
        }
        if (state->currentMenu != NULL && newItem > 0) {
            HiliteMenuItem(state->currentMenu, newItem, true);
        }
        state->currentItem = newItem;
    }
}

/*
 * ShowMenuAtPoint - Show menu at point
 */
static void ShowMenuAtPoint(short menuID, Point pt)
{
    MenuHandle theMenu = GetMenuHandle(menuID);
    if (theMenu == NULL) {
        return;
    }

    /* Calculate menu position */
    Point menuLocation;
    menuLocation.h = pt.h;
    menuLocation.v = 20; /* Position below menu bar (standard height) */

    ShowMenu(theMenu, menuLocation, NULL);
}

/*
 * HideCurrentMenu - Hide current menu
 */
static void HideCurrentMenu(void)
{
    HideMenu();
}

/*
 * ValidateMenuSelection - Validate menu selection
 */
static Boolean ValidateMenuSelection(short menuID, short item)
{
    return IsValidMenuCommand(menuID, item);
}

/*
 * FlashMenuFeedback - Flash menu for feedback
 */
static void FlashMenuFeedback(short menuID, short item)
{
    FlashMenuSelection(menuID, item, 1);
}

/*
 * GetCurrentMouseState - Get current mouse state
 */
static void GetCurrentMouseState(Point* mousePt, Boolean* mouseDown, unsigned long* modifiers)
{
    extern void GetMouse(Point* mouseLoc);
    extern Boolean Button(void);

    if (mousePt != NULL) {
        /* Get actual mouse position */
        GetMouse(mousePt);
    }

    if (mouseDown != NULL) {
        /* Get actual mouse button state */
        *mouseDown = Button();
    }

    if (modifiers != NULL) {
        Platform_GetKeyModifiers(modifiers);
    }
}

/*
 * WaitForMouseChange - Wait for mouse change
 */
static Boolean WaitForMouseChange(unsigned long timeout)
{
    /* TODO: Implement proper waiting with timeout */
    return true;
}

/*
 * GetCurrentTime - Get current time in ticks
 */
static unsigned long GetCurrentTime(void)
{
    /* TODO: Get actual system time */
    return 0;
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

#ifdef DEBUG
void PrintMenuSelectionState(void)
{
    /* serial_printf("=== Menu Selection State ===\n"); */
    /* serial_printf("Tracking active: %s\n", gTrackingActive ? "Yes" : "No"); */
    /* serial_printf("Last menu choice: 0x%08lX\n", gLastMenuChoice); */
    /* serial_printf("Last selection:\n"); */
    /* serial_printf("  Menu ID: %d\n", gLastSelection.menuID); */
    /* serial_printf("  Item ID: %d\n", gLastSelection.itemID); */
    /* serial_printf("  Valid: %s\n", gLastSelection.valid ? "Yes" : "No"); */
    /* serial_printf("  Cancelled: %s\n", gLastSelection.cancelled ? "Yes" : "No"); */
    /* serial_printf("Tracking state:\n"); */
    /* serial_printf("  State: %d\n", gTrackingState.trackingState); */
    /* serial_printf("  Current menu: %s\n", gTrackingState.currentMenu ? "Yes" : "No"); */
    /* serial_printf("  Current item: %d\n", gTrackingState.currentItem); */
    /* serial_printf("  Mouse down: %s\n", gTrackingState.mouseDown ? "Yes" : "No"); */
    /* serial_printf("==========================\n"); */
}
#endif

/* #include "SystemTypes.h" */
#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/menu_private.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>
/*
 * MenuSelection.c - Menu Tracking and Selection Implementation
 *
 * This file implements menu selection functionality including mouse tracking,
 * command key processing, menu choice dispatch, and user interaction handling.
 * It provides the complete menu interaction system for Mac OS compatibility.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

// #include "CompatibilityFix.h" // Removed

/* Disable optimization for this file to avoid stack alignment issues */
#pragma GCC optimize ("O0")

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuLogging.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuInternalTypes.h"
#include "MenuManager/MenuSelection.h"
#include "SoundManager/SoundEffects.h"
#include "MenuManager/MenuDisplay.h"

/* Serial output functions */

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
    Boolean buttonDown;
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
/* static MenuSelection gLastSelection; */  /* Unused - reserved for future use */
static Boolean gTrackingActive = false;
static long gLastMenuChoice = 0;

/* Platform function prototypes */
/* Platform functions declared in menu_private.h */

/* External function from MenuTitleTracking.c */
extern short FindMenuAtPoint_Internal(Point pt);

/* External function from MenuTrack.c */
extern long TrackMenu(short menuID, Point *startPt);

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
/* static Boolean ProcessMenuCommand(short cmdChar, unsigned long modifiers, MenuSelection* result); */  /* Reserved for keyboard shortcut handling */
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
    /* CRITICAL: Menu drawing must use WMgrPort, NOT the current port!
     * If a window port is active, its portBits.bounds offset will cause menus to render offset.
     * The WMgrPort has portBits.bounds at screen coordinates (0,0,width,height). */
    extern void GetPort(GrafPtr* port);
    extern void SetPort(GrafPtr port);
    extern void GetWMgrPort(GrafPtr* wmgrPort);

    /* Declare ALL local variables at function start to avoid mid-function stack issues */
    GrafPtr savedPort, wmgrPort;
    short menuID;
    Rect titleRect;
    Point dropdownPt;
    long trackResult;
    long result;
    short item;
    volatile int stack_align;

    GetPort(&savedPort);
    GetWMgrPort(&wmgrPort);
    /* MUST use WMgrPort for all menu drawing */
    if (wmgrPort) {
        SetPort(wmgrPort);
    }

    MENU_LOG_TRACE("MenuSelect: startPt = (h=%d, v=%d)\n", startPt.h, startPt.v);

    /* Simple implementation - just detect which menu was clicked */
    /* Check if click is in menu bar */
    if (startPt.v < 0 || startPt.v >= 20) {
        MENU_LOG_TRACE("MenuSelect: Not in menu bar (v=%d)\n", startPt.v);
        /* Restore port before returning */
        if (savedPort) SetPort(savedPort);
        return 0;
    }

    /* Find which menu was clicked using the tracking system */
    menuID = FindMenuAtPoint_Internal(startPt);

    if (menuID != 0) {
        MENU_LOG_TRACE("MenuSelect: Found menu ID %d at (h=%d,v=%d)\n",
                     menuID, startPt.h, startPt.v);

        /* Highlight the menu title */
        extern void HiliteMenu(short menuID);
        HiliteMenu(menuID);
        serial_puts("DEBUG: Returned from HiliteMenu\n");
        stack_align = 0;  /* Fix stack alignment after HiliteMenu */
        stack_align++;
        serial_puts("DEBUG: After stack_align\n");

        /* Get the actual menu title position for proper dropdown placement */

        extern Boolean GetMenuTitleRectByID(short menuID, Rect* outRect);
        serial_puts("DEBUG: About to call GetMenuTitleRectByID\n");
        if (GetMenuTitleRectByID(menuID, &titleRect)) {
            serial_puts("DEBUG: GetMenuTitleRectByID returned TRUE\n");
            /* Use the left edge of the menu title */
            MENU_LOG_TRACE("DEBUG: titleRect.left=%d\n", titleRect.left);
            dropdownPt.h = titleRect.left;
            serial_puts("DEBUG: Set dropdownPt.h\n");
            dropdownPt.v = 20; /* Position below menu bar */
            serial_puts("DEBUG: Set dropdownPt.v\n");
        } else {
            /* Fallback to mouse position if title rect not found */
            dropdownPt.h = startPt.h;
            dropdownPt.v = 20;
        }

        serial_puts("DEBUG: About to call TrackMenu\n");
        stack_align = 0;  /* Fix stack before TrackMenu call */
        stack_align++;
        /* Show dropdown and track item selection */
        trackResult = TrackMenu(menuID, &dropdownPt);

        if (trackResult != 0) {
            /* User selected an item - TrackMenu already returns packed format */
            result = trackResult;
            item = trackResult & 0xFFFF;
            MENU_LOG_TRACE("MenuSelect: User selected item %d from menu %d\n", item, menuID);
        } else {
            /* User cancelled or clicked outside */
            result = 0;
            MENU_LOG_TRACE("MenuSelect: User cancelled menu selection\n");
        }

        /* Unhighlight the menu title */
        extern void HiliteMenu(short menuID);
        HiliteMenu(0);

        gLastMenuChoice = result;
        MENU_LOG_TRACE("MenuSelect: Returning 0x%08lX\n", result);

        /* Restore saved port before returning */
        if (savedPort) SetPort(savedPort);
        return result;
    }

    MENU_LOG_TRACE("MenuSelect: No menu found at (h=%d,v=%d)\n", startPt.h, startPt.v);
    /* Restore saved port before returning */
    if (savedPort) SetPort(savedPort);
    return 0;
}

/*
 * MenuSelectEx - Extended menu selection with options
 */
short MenuSelectEx(Point startPt, MenuTrackInfo* trackInfo, MenuSelection* selection)
{
    Point currentPt;
    Boolean buttonDown = false;
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
    GetCurrentMouseState(&currentPt, &buttonDown, &modifiers);
    isInMenuBar = IsPointInMenuBar(startPt); /* Use startPt, not currentPt for initial check */

    if (!isInMenuBar) {
        /* Click was not in menu bar - no selection */
        selection->cancelled = true;
        EndMenuTracking(trackInfo);
        return kMenuSelectionCancelled;
    }

    MENU_LOG_TRACE("Starting menu selection at (%d,%d), inMenuBar=%s\n",
           startPt.h, startPt.v, isInMenuBar ? "Yes" : "No");

    /* Main tracking loop */
    int loopCount = 0; /* Add loop counter to prevent infinite loops */
    const int MAX_LOOPS = 1000; /* Prevent infinite loops */

    while ((buttonDown || currentMenu != 0) && loopCount < MAX_LOOPS) {
        loopCount++;
        GetCurrentMouseState(&currentPt, &buttonDown, &modifiers);

        /* Update tracking state */
        UpdateMenuTracking(trackInfo, currentPt, buttonDown);

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

                /* MENU_LOG_TRACE("Menu selection: menu %d, item %d\n", currentMenu, currentItem); */
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
    MenuCmdSearch localSearch = {0};

    /* Set up search parameters */
    if (search == NULL) {
        search = &localSearch;
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

            MENU_LOG_TRACE("Command key '%c' found: menu %d, item %d\n",
                   keyChar, search->foundMenuID, search->foundItem);
        } else {
            MENU_LOG_TRACE("Command key '%c' found but disabled: menu %d, item %d\n",
                   keyChar, search->foundMenuID, search->foundItem);
        }
    } else {
        /* MENU_LOG_TRACE("Command key '%c' not found\n", keyChar); */
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
    Boolean buttonDown = false;
    short menuUnderMouse = 0;

    if (!IsPointInMenuBar(startPt)) {
        return 0;
    }

    /* Find menu under initial point */
    menuUnderMouse = FindMenuAtPoint(startPt);

    /* MENU_LOG_TRACE("Tracking menu bar starting at menu %d\n", menuUnderMouse); */

    /* Track mouse until it leaves menu bar or button is released */
    do {
        GetCurrentMouseState(&currentPt, &buttonDown, NULL);

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
    } while (buttonDown && IsPointInMenuBar(currentPt));

    return menuUnderMouse;
}

/*
 * TrackPullDownMenu - Track pull-down menu selection
 */
short TrackPullDownMenu(MenuHandle theMenu, const Rect* menuRect,
                       Point startPt, MenuTrackInfo* trackInfo)
{
    Point currentPt;
    Boolean buttonDown = false;
    short itemUnderMouse = 0;
    short lastItem = 0;

    if (theMenu == NULL || menuRect == NULL) {
        return 0;
    }

    /* MENU_LOG_TRACE("Tracking pull-down menu %d\n", (*(MenuInfo**)theMenu)->menuID); */

    /* Track mouse in menu */
    do {
        GetCurrentMouseState(&currentPt, &buttonDown, NULL);

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
    } while (buttonDown);

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
    Handle menuBarHandle;
    MenuBarList* menuBar;
    int m, i;
    char searchChar;

    /* Forward declarations for internal query functions */
    extern Boolean CheckMenuItemEnabled(MenuHandle theMenu, short item);
    extern char GetMenuItemCmdKey(MenuHandle theMenu, short item);

    if (search == NULL) {
        return false;
    }

    /* Initialize search result */
    search->found = false;
    search->foundMenuID = 0;
    search->foundItem = 0;
    search->enabled = false;

    /* Get menu bar */
    menuBarHandle = GetMenuBar();
    if (menuBarHandle == NULL) {
        return false;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(menuBarHandle);
    menuBar = (MenuBarList*)*menuBarHandle;

    /* Convert command key to lowercase for comparison (our storage is lowercase) */
    searchChar = cmdChar;
    if (searchChar >= 'A' && searchChar <= 'Z') {
        searchChar = searchChar - 'A' + 'a';
    }

    /* Search through all menus in menu bar */
    for (m = 0; m < menuBar->numMenus; m++) {
        MenuHandle theMenu = GetMenuHandle(menuBar->menus[m].menuID);
        if (theMenu == NULL) {
            continue;
        }

        short itemCount = CountMItems(theMenu);

        /* Search through all items in menu */
        for (i = 1; i <= itemCount; i++) {
            char itemCmd = GetMenuItemCmdKey(theMenu, i);

            if (itemCmd == searchChar && itemCmd != 0) {
                /* Found matching command key */
                search->found = true;
                search->foundMenuID = menuBar->menus[m].menuID;
                search->foundItem = i;
                search->enabled = CheckMenuItemEnabled(theMenu, i);

                /* Unlock handle before returning */
                HUnlock(menuBarHandle);
                return true;
            }
        }
    }

    /* Unlock handle before returning */
    HUnlock(menuBarHandle);
    return false;
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

    /* MENU_LOG_TRACE("Executed menu command: menu %d, item %d\n", menuID, item); */

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

    /* MENU_LOG_TRACE("Beginning menu tracking session\n"); */
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

    /* MENU_LOG_TRACE("Ending menu tracking session\n"); */
}

/*
 * UpdateMenuTracking - Update menu tracking state
 */
void UpdateMenuTracking(MenuTrackInfo* trackInfo, Point mousePt, Boolean buttonDown)
{
    if (trackInfo == NULL) {
        return;
    }

    trackInfo->lastMousePoint = trackInfo->mousePoint;
    trackInfo->mousePoint = mousePt;
    trackInfo->mouseWasDown = buttonDown;
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

    /* Flash menu item AND title for complete feedback */
    extern void FlashMenuItem(MenuHandle theMenu, short item, short flashes);

    /* Flash menu title in menu bar */
    for (short i = 0; i < flashes; i++) {
        HiliteMenu(menuID);
        WaitForMouseChange(2); /* Brief delay */
        HiliteMenu(0);
        WaitForMouseChange(2);
    }

    /* Flash the menu item itself (if menu is still shown) */
    FlashMenuItem(theMenu, item, flashes);
}

/*
 * AnimateMenuSelection - Animate menu selection
 */
void AnimateMenuSelection(const MenuSelection* selection, short animation)
{
    if (selection == NULL || !selection->valid) {
        return;
    }

    MENU_LOG_TRACE("Animating menu selection: menu %d, item %d (animation %d)\n",
           selection->menuID, selection->itemID, animation);

    /* Implement selection animation by flashing the selected item */
    if (selection->menuHandle && selection->itemID > 0) {
        /* Get menu info */
        MenuInfo* menuInfo = (MenuInfo*)(*selection->menuHandle);
        if (menuInfo && selection->itemID <= menuInfo->lastItem) {
            /* Calculate item rectangle */
            Rect itemRect = selection->itemRect;

            /* Flash the item by inverting it a few times */
            short flashCount = (animation == kMenuAnimationNone) ? 0 : 2;
            for (short i = 0; i < flashCount; i++) {
                /* Invert the item rectangle */
                InvertRect(&itemRect);

                /* Brief delay for visual effect */
                extern void Platform_WaitTicks(short ticks);
                Platform_WaitTicks(2); /* ~33ms at 60 Hz */

                /* Invert back */
                InvertRect(&itemRect);

                if (i < flashCount - 1) {
                    Platform_WaitTicks(1);
                }
            }
        }
    }
}

/*
 * PlayMenuSound - Play menu sound effect
 */
void PlayMenuSound(short soundType)
{
    (void)soundType;
    (void)SoundEffects_Play(kSoundEffectMenuSelect);
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
void GetMenuMouseState(Point* mousePt, Boolean* buttonDown, unsigned long* modifiers)
{
    GetCurrentMouseState(mousePt, buttonDown, modifiers);
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
        extern void DisposeRgn(RgnHandle rgn);
        DisposeRgn((RgnHandle)state->savedRegion);
        state->savedRegion = NULL;
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
        if (state->currentMenu != 0 && state->currentItem > 0) {
            MenuHandle menuHandle = GetMenuHandle(state->currentMenu);
            if (menuHandle) HiliteMenuItem(menuHandle, state->currentItem, false);
        }
        if (state->currentMenu != 0 && newItem > 0) {
            MenuHandle menuHandle = GetMenuHandle(state->currentMenu);
            if (menuHandle) HiliteMenuItem(menuHandle, newItem, true);
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

    /* Get the actual menu title position */
    Rect titleRect;
    Point menuLocation;

    extern Boolean GetMenuTitleRectByID(short menuID, Rect* outRect);
    if (GetMenuTitleRectByID(menuID, &titleRect)) {
        /* Use the left edge of the menu title */
        menuLocation.h = titleRect.left;
        menuLocation.v = 20; /* Position below menu bar (standard height) */
    } else {
        /* Fallback to mouse position if title rect not found */
        menuLocation.h = pt.h;
        menuLocation.v = 20;
    }

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
static void GetCurrentMouseState(Point* mousePt, Boolean* buttonDown, unsigned long* modifiers)
{
    extern void GetMouse(Point* mouseLoc);
    extern Boolean Button(void);

    if (mousePt != NULL) {
        /* Get actual mouse position */
        GetMouse(mousePt);
    }

    if (buttonDown != NULL) {
        /* Get actual mouse button state */
        *buttonDown = Button();
    }

    if (modifiers != NULL) {
        Platform_GetKeyModifiers(modifiers);
    }
}

/*
 * WaitForMouseChange - Wait for mouse change or timeout
 */
static Boolean WaitForMouseChange(unsigned long timeout)
{
    extern UInt32 TickCount(void);
    extern void SystemTask(void);

    Point startPt, currentPt;
    Boolean startMouseDown, currentMouseDown;
    unsigned long startMods, currentMods;
    unsigned long startTime = TickCount();

    GetCurrentMouseState(&startPt, &startMouseDown, &startMods);

    while ((TickCount() - startTime) < timeout) {
        GetCurrentMouseState(&currentPt, &currentMouseDown, &currentMods);

        /* Check if mouse position, button state, or modifiers changed */
        if ((currentPt.h != startPt.h) || (currentPt.v != startPt.v) ||
            (currentMouseDown != startMouseDown) || (currentMods != startMods)) {
            return true;
        }

        SystemTask();
    }

    return false;
}

/*
 * GetCurrentTime - Get current time in ticks
 */
static unsigned long GetCurrentTime(void)
{
    extern UInt32 TickCount(void);
    return TickCount();
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

#ifdef DEBUG
void PrintMenuSelectionState(void)
{
    /* MENU_LOG_TRACE("=== Menu Selection State ===\n"); */
    /* MENU_LOG_TRACE("Tracking active: %s\n", gTrackingActive ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("Last menu choice: 0x%08lX\n", gLastMenuChoice); */
    /* MENU_LOG_TRACE("Last selection:\n"); */
    /* MENU_LOG_TRACE("  Menu ID: %d\n", gLastSelection.menuID); */
    /* MENU_LOG_TRACE("  Item ID: %d\n", gLastSelection.itemID); */
    /* MENU_LOG_TRACE("  Valid: %s\n", gLastSelection.valid ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("  Cancelled: %s\n", gLastSelection.cancelled ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("Tracking state:\n"); */
    /* MENU_LOG_TRACE("  State: %d\n", gTrackingState.trackingState); */
    /* MENU_LOG_TRACE("  Current menu: %s\n", gTrackingState.currentMenu ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("  Current item: %d\n", gTrackingState.currentItem); */
    /* MENU_LOG_TRACE("  Mouse down: %s\n", gTrackingState.mouseDown ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("==========================\n"); */
}
#endif

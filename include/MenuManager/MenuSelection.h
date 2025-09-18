/*
 * MenuSelection.h - Menu Tracking and Selection APIs
 *
 * Header for menu selection functions including menu tracking,
 * mouse handling, command key processing, and menu choice dispatch.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Menu Manager
 */

#ifndef __MENU_SELECTION_H__
#define __MENU_SELECTION_H__

#include "MenuManager.h"
#include "MenuTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Menu Selection Constants
 * ============================================================================ */

/* Menu tracking states */
enum {
    kMenuTrackingIdle       = 0,        /* Not tracking any menu */
    kMenuTrackingMenuBar    = 1,        /* Tracking in menu bar */
    kMenuTrackingPullDown   = 2,        /* Tracking pull-down menu */
    kMenuTrackingPopUp      = 3,        /* Tracking popup menu */
    kMenuTrackingHierarchical = 4,      /* Tracking hierarchical menu */
    kMenuTrackingContextual = 5         /* Tracking contextual menu */
};

/* Menu selection results */
enum {
    kMenuNoSelection        = 0,        /* No menu selection made */
    kMenuSelectionValid     = 1,        /* Valid menu selection */
    kMenuSelectionCancelled = 2,        /* Selection was cancelled */
    kMenuSelectionError     = -1        /* Error during selection */
};

/* Mouse tracking constants */
enum {
    kMenuTrackingSlop       = 4,        /* Pixel slop for menu tracking */
    kMenuHiliteDelay        = 8,        /* Delay before highlighting items (ticks) */
    kMenuScrollDelay        = 15,       /* Delay for scrolling menus (ticks) */
    kMenuFlashDuration      = 8,        /* Duration of menu flash (ticks) */
    kMenuSubmenuDelay       = 30        /* Delay before showing submenu (ticks) */
};

/* Command key modifiers */
enum {
    kMenuCmdModifiers       = 0x0100,   /* Command key */
    kMenuShiftModifiers     = 0x0200,   /* Shift key */
    kMenuOptionModifiers    = 0x0800,   /* Option key */
    kMenuControlModifiers   = 0x1000    /* Control key */
};

/* ============================================================================
 * Menu Selection Data Structures
 * ============================================================================ */

/* Menu selection result */
typedef struct MenuSelection {
    short           menuID;             /* Selected menu ID */
    short           itemID;             /* Selected item number */
    long            menuChoice;         /* Packed menu choice (compatibility) */
    Boolean         valid;              /* TRUE if selection is valid */
    Boolean         cancelled;          /* TRUE if selection was cancelled */
    Point           clickPoint;         /* Original click point */
    unsigned long   modifiers;          /* Modifier keys held */
} MenuSelection;

/* Menu tracking information */
typedef struct MenuTrackInfo {
    short           trackingState;      /* Current tracking state */
    MenuHandle      currentMenu;        /* Menu being tracked */
    short           currentItem;        /* Item being highlighted */
    short           lastItem;           /* Previously highlighted item */
    Point           mousePoint;         /* Current mouse position */
    Point           lastMousePoint;     /* Previous mouse position */
    Boolean         mouseDown;          /* Mouse button is down */
    Boolean         mouseMoved;         /* Mouse has moved */
    unsigned long   trackStartTime;     /* When tracking started */
    unsigned long   lastMoveTime;       /* When mouse last moved */
    Rect            currentMenuRect;    /* Rectangle of current menu */
    Boolean         inMenuBar;          /* Mouse is in menu bar */
    Boolean         inMenu;             /* Mouse is in pull-down menu */
    short           menuBarItem;        /* Menu bar item under mouse */
} MenuTrackInfo;

/* Command key search information */
typedef struct MenuCmdSearch {
    short           searchChar;         /* Character to search for */
    unsigned long   modifiers;          /* Required modifier keys */
    MenuHandle      foundMenu;          /* Menu containing command */
    short           foundItem;          /* Item with command key */
    Boolean         found;              /* TRUE if command was found */
    Boolean         enabled;            /* TRUE if found item is enabled */
} MenuCmdSearch;

/* Hierarchical menu tracking */
typedef struct HierMenuTrack {
    MenuHandle      parentMenu;         /* Parent menu */
    short           parentItem;         /* Parent item number */
    MenuHandle      submenu;            /* Submenu handle */
    Point           submenuLocation;    /* Submenu location */
    Rect            submenuRect;        /* Submenu rectangle */
    Boolean         submenuVisible;     /* Submenu is visible */
    unsigned long   showTime;           /* When submenu was shown */
} HierMenuTrack;

/* ============================================================================
 * Menu Selection Core Functions
 * ============================================================================ */

/*
 * MenuSelectEx - Extended menu selection with options
 *
 * Enhanced version of MenuSelect with additional tracking options.
 *
 * Parameters:
 *   startPt     - Initial mouse position (global coordinates)
 *   trackInfo   - Tracking information (can be NULL)
 *   selection   - Receives selection result
 *
 * Returns: Menu selection result code
 */
short MenuSelectEx(Point startPt, MenuTrackInfo* trackInfo, MenuSelection* selection);

/*
 * TrackMenuBar - Track mouse in menu bar
 *
 * Tracks mouse movement in the menu bar and handles menu title highlighting.
 *
 * Parameters:
 *   startPt   - Initial mouse position
 *   trackInfo - Tracking information
 *
 * Returns: Menu ID of selected menu title (0 = none)
 */
short TrackMenuBar(Point startPt, MenuTrackInfo* trackInfo);

/*
 * TrackPullDownMenu - Track pull-down menu selection
 *
 * Tracks mouse movement in a pull-down menu and handles item selection.
 *
 * Parameters:
 *   theMenu   - Menu to track
 *   menuRect  - Menu rectangle
 *   startPt   - Initial mouse position
 *   trackInfo - Tracking information
 *
 * Returns: Selected item number (0 = none)
 */
short TrackPullDownMenu(MenuHandle theMenu, const Rect* menuRect,
                       Point startPt, MenuTrackInfo* trackInfo);

/*
 * TrackPopUpMenu - Track popup menu selection
 *
 * Tracks mouse movement in a popup menu with positioning options.
 *
 * Parameters:
 *   theMenu    - Menu to track
 *   location   - Menu location
 *   popUpItem  - Item to position at click point
 *   trackInfo  - Tracking information
 *
 * Returns: Selected item number (0 = none)
 */
short TrackPopUpMenu(MenuHandle theMenu, Point location, short popUpItem,
                    MenuTrackInfo* trackInfo);

/*
 * FindMenuItemUnderMouse - Find item under mouse cursor
 *
 * Determines which menu item is under the current mouse position.
 *
 * Parameters:
 *   theMenu   - Menu to search
 *   menuRect  - Menu rectangle
 *   mousePt   - Mouse position
 *
 * Returns: Item number under mouse (0 = none)
 */
short FindMenuItemUnderMouse(MenuHandle theMenu, const Rect* menuRect, Point mousePt);

/*
 * IsPointInMenuBar - Check if point is in menu bar
 *
 * Determines if the specified point is within the menu bar area.
 *
 * Parameters:
 *   pt - Point to test (global coordinates)
 *
 * Returns: TRUE if point is in menu bar
 */
Boolean IsPointInMenuBar(Point pt);

/*
 * FindMenuUnderPoint - Find menu title under point
 *
 * Finds which menu title in the menu bar is under the specified point.
 *
 * Parameters:
 *   pt - Point to test (global coordinates)
 *
 * Returns: Menu ID under point (0 = none)
 */
short FindMenuUnderPoint(Point pt);

/* ============================================================================
 * Command Key Processing
 * ============================================================================ */

/*
 * MenuKeyEx - Extended command key processing
 *
 * Enhanced version of MenuKey with modifier key support.
 *
 * Parameters:
 *   keyChar   - Character code of key pressed
 *   modifiers - Modifier keys held
 *   search    - Receives search information (can be NULL)
 *
 * Returns: Menu selection as long (menu ID in high word, item in low word)
 */
long MenuKeyEx(short keyChar, unsigned long modifiers, MenuCmdSearch* search);

/*
 * FindMenuCommand - Find menu item with command key
 *
 * Searches all menus for an item with the specified command key.
 *
 * Parameters:
 *   cmdChar   - Command character to find
 *   modifiers - Required modifier keys
 *   search    - Receives detailed search results
 *
 * Returns: TRUE if command key was found
 */
Boolean FindMenuCommand(short cmdChar, unsigned long modifiers, MenuCmdSearch* search);

/*
 * IsValidMenuCommand - Check if menu command is valid
 *
 * Checks if a menu command exists and is currently enabled.
 *
 * Parameters:
 *   menuID - Menu ID
 *   item   - Item number
 *
 * Returns: TRUE if command is valid and enabled
 */
Boolean IsValidMenuCommand(short menuID, short item);

/*
 * ExecuteMenuCommand - Execute menu command
 *
 * Executes a menu command and provides appropriate feedback.
 *
 * Parameters:
 *   menuID - Menu ID
 *   item   - Item number
 *   flash  - TRUE to flash menu title
 *
 * Returns: TRUE if command was executed successfully
 */
Boolean ExecuteMenuCommand(short menuID, short item, Boolean flash);

/* ============================================================================
 * Hierarchical Menu Support
 * ============================================================================ */

/*
 * ShowHierarchicalMenu - Show hierarchical submenu
 *
 * Displays a hierarchical submenu attached to a parent menu item.
 *
 * Parameters:
 *   parentMenu - Parent menu
 *   parentItem - Parent item number
 *   submenuID  - Submenu ID
 *   location   - Submenu location
 *
 * Returns: Handle to displayed submenu
 */
MenuHandle ShowHierarchicalMenu(MenuHandle parentMenu, short parentItem,
                               short submenuID, Point location);

/*
 * HideHierarchicalMenu - Hide hierarchical submenu
 *
 * Hides a hierarchical submenu and restores the background.
 *
 * Parameters:
 *   submenu - Submenu to hide
 */
void HideHierarchicalMenu(MenuHandle submenu);

/*
 * TrackHierarchicalMenu - Track hierarchical menu selection
 *
 * Tracks selection in hierarchical menus with proper submenu handling.
 *
 * Parameters:
 *   parentMenu - Parent menu
 *   parentRect - Parent menu rectangle
 *   startPt    - Initial mouse position
 *   hierTrack  - Hierarchical tracking info
 *
 * Returns: Menu selection result
 */
long TrackHierarchicalMenu(MenuHandle parentMenu, const Rect* parentRect,
                          Point startPt, HierMenuTrack* hierTrack);

/*
 * CalcSubmenuPosition - Calculate submenu position
 *
 * Calculates the optimal position for a submenu relative to its parent item.
 *
 * Parameters:
 *   parentMenu - Parent menu
 *   parentRect - Parent menu rectangle
 *   parentItem - Parent item number
 *   submenuSize - Submenu size
 *   position   - Receives calculated position
 */
void CalcSubmenuPosition(MenuHandle parentMenu, const Rect* parentRect,
                        short parentItem, Point submenuSize, Point* position);

/* ============================================================================
 * Menu Selection State Management
 * ============================================================================ */

/*
 * BeginMenuTracking - Begin menu tracking session
 *
 * Initializes menu tracking state and prepares for user interaction.
 *
 * Parameters:
 *   trackInfo - Tracking information to initialize
 */
void BeginMenuTracking(MenuTrackInfo* trackInfo);

/*
 * EndMenuTracking - End menu tracking session
 *
 * Cleans up menu tracking state and restores normal operation.
 *
 * Parameters:
 *   trackInfo - Tracking information to clean up
 */
void EndMenuTracking(MenuTrackInfo* trackInfo);

/*
 * UpdateMenuTracking - Update menu tracking state
 *
 * Updates tracking state based on current mouse position and button state.
 *
 * Parameters:
 *   trackInfo - Tracking information to update
 *   mousePt   - Current mouse position
 *   mouseDown - Current mouse button state
 */
void UpdateMenuTracking(MenuTrackInfo* trackInfo, Point mousePt, Boolean mouseDown);

/*
 * GetCurrentMenuSelection - Get current menu selection
 *
 * Returns the current menu selection based on tracking state.
 *
 * Parameters:
 *   trackInfo - Current tracking information
 *   selection - Receives current selection
 */
void GetCurrentMenuSelection(const MenuTrackInfo* trackInfo, MenuSelection* selection);

/* ============================================================================
 * Menu Feedback and Animation
 * ============================================================================ */

/*
 * FlashMenuSelection - Flash selected menu item
 *
 * Provides visual feedback by flashing the selected menu item.
 *
 * Parameters:
 *   menuID - Menu ID
 *   item   - Item number
 *   flashes - Number of flashes
 */
void FlashMenuSelection(short menuID, short item, short flashes);

/*
 * AnimateMenuSelection - Animate menu selection
 *
 * Provides animated feedback for menu selection.
 *
 * Parameters:
 *   selection - Menu selection to animate
 *   animation - Animation type
 */
void AnimateMenuSelection(const MenuSelection* selection, short animation);

/*
 * PlayMenuSound - Play menu sound effect
 *
 * Plays an appropriate sound effect for menu actions.
 *
 * Parameters:
 *   soundType - Type of sound to play
 */
void PlayMenuSound(short soundType);

/* ============================================================================
 * Menu Selection Utilities
 * ============================================================================ */

/*
 * PackMenuChoice - Pack menu choice into long
 *
 * Packs menu ID and item number into a single long value.
 *
 * Parameters:
 *   menuID - Menu ID
 *   item   - Item number
 *
 * Returns: Packed menu choice
 */
long PackMenuChoice(short menuID, short item);

/*
 * UnpackMenuChoice - Unpack menu choice from long
 *
 * Unpacks menu ID and item number from a packed long value.
 *
 * Parameters:
 *   menuChoice - Packed menu choice
 *   menuID     - Receives menu ID
 *   item       - Receives item number
 */
void UnpackMenuChoice(long menuChoice, short* menuID, short* item);

/*
 * IsMenuChoiceValid - Check if menu choice is valid
 *
 * Validates a menu choice to ensure it refers to an existing, enabled item.
 *
 * Parameters:
 *   menuChoice - Menu choice to validate
 *
 * Returns: TRUE if menu choice is valid
 */
Boolean IsMenuChoiceValid(long menuChoice);

/*
 * CompareMenuChoices - Compare two menu choices
 *
 * Compares two menu choices for equality.
 *
 * Parameters:
 *   choice1 - First menu choice
 *   choice2 - Second menu choice
 *
 * Returns: TRUE if choices are equal
 */
Boolean CompareMenuChoices(long choice1, long choice2);

/* ============================================================================
 * Mouse and Event Handling
 * ============================================================================ */

/*
 * GetMenuMouseState - Get current mouse state for menus
 *
 * Gets the current mouse position and button state for menu tracking.
 *
 * Parameters:
 *   mousePt   - Receives mouse position (global coordinates)
 *   mouseDown - Receives mouse button state
 *   modifiers - Receives modifier key state
 */
void GetMenuMouseState(Point* mousePt, Boolean* mouseDown, unsigned long* modifiers);

/*
 * WaitForMouseMove - Wait for mouse movement
 *
 * Waits for the mouse to move or the button state to change.
 *
 * Parameters:
 *   timeout - Maximum time to wait (ticks)
 *
 * Returns: TRUE if mouse moved or button changed
 */
Boolean WaitForMouseMove(unsigned long timeout);

/*
 * ConvertMenuPoint - Convert point between coordinate systems
 *
 * Converts a point between global and local coordinate systems.
 *
 * Parameters:
 *   pt        - Point to convert
 *   fromGlobal - TRUE to convert from global to local
 */
void ConvertMenuPoint(Point* pt, Boolean fromGlobal);

/* ============================================================================
 * Platform Integration for Selection
 * ============================================================================ */

/*
 * Platform selection functions that must be implemented:
 *
 * Boolean Platform_TrackMouse(Point* mousePt, Boolean* mouseDown);
 * Boolean Platform_GetKeyModifiers(unsigned long* modifiers);
 * void Platform_SetMenuCursor(short cursorType);
 * Boolean Platform_IsMenuVisible(MenuHandle theMenu);
 * void Platform_MenuFeedback(short feedbackType, short menuID, short item);
 */

/* ============================================================================
 * Selection State Macros
 * ============================================================================ */

/* Extract menu ID and item from packed choice */
#define MenuChoiceID(choice)    ((short)((choice) >> 16))
#define MenuChoiceItem(choice)  ((short)((choice) & 0xFFFF))

/* Create packed menu choice */
#define MakeMenuChoice(id, item) \
    (((long)(id) << 16) | ((long)(item) & 0xFFFF))

/* Check if menu choice indicates no selection */
#define IsNoMenuChoice(choice)  ((choice) == 0)

/* Check if tracking state is active */
#define IsMenuTracking(info)    ((info)->trackingState != kMenuTrackingIdle)

/* Check if mouse is down during tracking */
#define IsMouseDownTracking(info) ((info)->mouseDown)

#ifdef __cplusplus
}
#endif

#endif /* __MENU_SELECTION_H__ */
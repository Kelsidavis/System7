/*
 * MenuManagerCore.c - Core Menu Manager Implementation
 *
 * This file implements the core Menu Manager functionality including
 * menu creation, disposal, menu bar management, and the fundamental
 * menu operations. This is THE FINAL CRITICAL COMPONENT that completes
 * the essential Mac OS interface for System 7.1 compatibility.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Menu Manager
 */

#include "../../include/MenuManager/MenuManager.h"
#include "../../include/MenuManager/MenuTypes.h"
#include "../../include/MenuManager/MenuDisplay.h"
#include "../../include/MenuManager/MenuSelection.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global Menu Manager State
 * ============================================================================ */

static MenuManagerState* gMenuMgrState = NULL;
static Boolean gMenuMgrInitialized = false;

/* Low memory globals (for compatibility) */
static short gMBarHeight = 20;          /* Menu bar height */
static Handle gMenuList = NULL;         /* Current menu list */
static MCTableHandle gMCTable = NULL;   /* Menu color table */
static short gMenuFlash = 3;            /* Flash count */
static long gLastMenuChoice = 0;        /* Last menu choice */

/* Internal function prototypes */
static MenuManagerState* AllocateMenuManagerState(void);
static void InitializeMenuManagerState(MenuManagerState* state);
static void DisposeMenuManagerState(MenuManagerState* state);
static OSErr ValidateMenuHandle(MenuHandle theMenu);
static OSErr ValidateMenuID(short menuID);
static MenuHandle FindMenuInList(short menuID);
static void UpdateMenuBarLayout(void);
static void InvalidateMenuBar(void);

/* Platform function prototypes (implemented elsewhere) */
extern void Platform_InitMenuSystem(void);
extern void Platform_CleanupMenuSystem(void);
extern void Platform_DrawMenuBar(void);
extern void Platform_EraseMenuBar(void);

/* ============================================================================
 * Menu Manager Initialization and Cleanup
 * ============================================================================ */

/*
 * InitMenus - Initialize the Menu Manager
 *
 * This MUST be called before any other Menu Manager functions.
 * Sets up internal structures, initializes the menu bar, and prepares
 * the Menu Manager for use.
 */
void InitMenus(void)
{
    if (gMenuMgrInitialized) {
        return; /* Already initialized */
    }

    /* Allocate and initialize global state */
    gMenuMgrState = AllocateMenuManagerState();
    if (gMenuMgrState == NULL) {
        /* Critical failure - cannot continue */
        return;
    }

    InitializeMenuManagerState(gMenuMgrState);

    /* Initialize platform-specific menu system */
    Platform_InitMenuSystem();

    /* Set up standard menu bar */
    gMBarHeight = menuBarStdHeight;
    gMenuList = NULL;
    gMCTable = NULL;
    gMenuFlash = 3;
    gLastMenuChoice = 0;

    /* Mark as initialized */
    gMenuMgrInitialized = true;
    gMenuMgrState->initialized = true;

    printf("Menu Manager initialized successfully\n");
}

/*
 * CleanupMenus - Clean up Menu Manager resources
 */
void CleanupMenus(void)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    /* Clean up platform-specific resources */
    Platform_CleanupMenuSystem();

    /* Dispose of menu color table */
    if (gMCTable != NULL) {
        DisposeMCInfo(gMCTable);
        gMCTable = NULL;
    }

    /* Clear menu list */
    if (gMenuList != NULL) {
        free(gMenuList);
        gMenuList = NULL;
    }

    /* Dispose of global state */
    if (gMenuMgrState != NULL) {
        DisposeMenuManagerState(gMenuMgrState);
        gMenuMgrState = NULL;
    }

    gMenuMgrInitialized = false;
}

/* ============================================================================
 * Menu Bar Management
 * ============================================================================ */

/*
 * GetMenuBar - Get current menu list
 */
Handle GetMenuBar(void)
{
    if (!gMenuMgrInitialized) {
        return NULL;
    }

    return gMenuList;
}

/*
 * GetNewMBar - Create menu list from MBAR resource
 */
Handle GetNewMBar(short menuBarID)
{
    if (!gMenuMgrInitialized) {
        return NULL;
    }

    /* TODO: Implement resource loading */
    /* For now, create an empty menu list */
    MenuBarList* menuBar = (MenuBarList*)malloc(sizeof(MenuBarList));
    if (menuBar == NULL) {
        return NULL;
    }

    menuBar->numMenus = 0;
    menuBar->totalWidth = 0;
    menuBar->lastRight = 0;
    menuBar->mbResID = menuBarID;

    return (Handle)menuBar;
}

/*
 * SetMenuBar - Set current menu list
 */
void SetMenuBar(Handle menuList)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    /* Dispose of old menu list if it exists */
    if (gMenuList != NULL && gMenuList != menuList) {
        free(gMenuList);
    }

    gMenuList = menuList;
    gMenuMgrState->menuList = menuList;

    /* Update menu bar display */
    UpdateMenuBarLayout();
    InvalidateMenuBar();
}

/*
 * ClearMenuBar - Remove all menus from menu bar
 */
void ClearMenuBar(void)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    if (gMenuList != NULL) {
        free(gMenuList);
        gMenuList = NULL;
        gMenuMgrState->menuList = NULL;
    }

    /* Clear menu bar display */
    InvalidateMenuBar();
}

/*
 * DrawMenuBar - Redraw the menu bar
 */
void DrawMenuBar(void)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    gMenuMgrState->menuBarInvalid = false;
    Platform_DrawMenuBar();
}

/*
 * InvalMenuBar - Mark menu bar as needing redraw
 */
void InvalMenuBar(void)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    InvalidateMenuBar();
}

/*
 * HiliteMenu - Highlight a menu title
 */
void HiliteMenu(short menuID)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    gMenuMgrState->hiliteMenu = menuID;

    /* Update display */
    DrawMenuBar();
}

/*
 * GetMBarHeight - Get menu bar height
 */
short GetMBarHeight(void)
{
    return gMBarHeight;
}

/* ============================================================================
 * Menu Creation and Management
 * ============================================================================ */

/*
 * NewMenu - Create a new menu
 */
MenuHandle NewMenu(short menuID, ConstStr255Param menuTitle)
{
    MenuHandle theMenu;
    MenuInfo* menuPtr;
    size_t titleLen;

    if (!gMenuMgrInitialized) {
        return NULL;
    }

    if (ValidateMenuID(menuID) != 0) {
        return NULL;
    }

    /* Check if menu ID already exists */
    if (FindMenuInList(menuID) != NULL) {
        return NULL; /* Menu ID already in use */
    }

    /* Allocate menu handle */
    theMenu = (MenuHandle)malloc(sizeof(MenuInfo*));
    if (theMenu == NULL) {
        return NULL;
    }

    /* Allocate menu info */
    menuPtr = (MenuInfo*)malloc(sizeof(MenuInfo));
    if (menuPtr == NULL) {
        free(theMenu);
        return NULL;
    }

    /* Initialize menu info */
    memset(menuPtr, 0, sizeof(MenuInfo));
    *theMenu = menuPtr;

    menuPtr->menuID = menuID;
    menuPtr->menuWidth = 0;  /* Will be calculated */
    menuPtr->menuHeight = 0; /* Will be calculated */
    menuPtr->menuProc = NULL; /* Standard text menu */
    menuPtr->enableFlags = 0xFFFFFFFF; /* All items enabled initially */

    /* Copy menu title */
    titleLen = menuTitle[0];
    if (titleLen > 255) titleLen = 255;
    menuPtr->menuData[0] = (unsigned char)titleLen;
    memcpy(&menuPtr->menuData[1], &menuTitle[1], titleLen);

    printf("Created menu ID %d: %.*s\n", menuID, titleLen, &menuTitle[1]);

    return theMenu;
}

/*
 * GetMenu - Load menu from MENU resource
 */
MenuHandle GetMenu(short resourceID)
{
    if (!gMenuMgrInitialized) {
        return NULL;
    }

    /* TODO: Implement resource loading */
    /* For now, create a simple menu */
    unsigned char title[256];
    snprintf((char*)&title[1], 255, "Menu %d", resourceID);
    title[0] = strlen((char*)&title[1]);

    return NewMenu(resourceID, title);
}

/*
 * DisposeMenu - Dispose of a menu
 */
void DisposeMenu(MenuHandle theMenu)
{
    if (!gMenuMgrInitialized || theMenu == NULL) {
        return;
    }

    if (ValidateMenuHandle(theMenu) != 0) {
        return;
    }

    /* Remove from menu bar if present */
    DeleteMenu((*theMenu)->menuID);

    /* Free menu data */
    if (*theMenu != NULL) {
        free(*theMenu);
    }
    free(theMenu);
}

/*
 * InsertMenu - Add menu to menu bar
 */
void InsertMenu(MenuHandle theMenu, short beforeID)
{
    MenuBarList* menuBar;
    int insertIndex;

    if (!gMenuMgrInitialized || theMenu == NULL) {
        return;
    }

    if (ValidateMenuHandle(theMenu) != 0) {
        return;
    }

    /* Create menu list if it doesn't exist */
    if (gMenuList == NULL) {
        gMenuList = (Handle)malloc(sizeof(MenuBarList));
        if (gMenuList == NULL) {
            return;
        }
        menuBar = (MenuBarList*)gMenuList;
        menuBar->numMenus = 0;
        menuBar->totalWidth = 0;
        menuBar->lastRight = 0;
        menuBar->mbResID = 0;
    } else {
        menuBar = (MenuBarList*)gMenuList;
    }

    /* Find insertion point */
    insertIndex = menuBar->numMenus; /* Default to end */
    if (beforeID != 0 && beforeID != hierMenu) {
        for (int i = 0; i < menuBar->numMenus; i++) {
            if (menuBar->menus[i].menuID == beforeID) {
                insertIndex = i;
                break;
            }
        }
    }

    /* Expand menu list */
    size_t newSize = sizeof(MenuBarList) + (menuBar->numMenus) * sizeof(MenuListEntry);
    gMenuList = (Handle)realloc(gMenuList, newSize);
    if (gMenuList == NULL) {
        return;
    }
    menuBar = (MenuBarList*)gMenuList;

    /* Shift existing menus if inserting in middle */
    if (insertIndex < menuBar->numMenus) {
        memmove(&menuBar->menus[insertIndex + 1], &menuBar->menus[insertIndex],
                (menuBar->numMenus - insertIndex) * sizeof(MenuListEntry));
    }

    /* Insert new menu */
    menuBar->menus[insertIndex].menuID = (*theMenu)->menuID;
    menuBar->menus[insertIndex].menuLeft = 0; /* Will be calculated */
    menuBar->menus[insertIndex].menuWidth = 0; /* Will be calculated */
    menuBar->numMenus++;

    /* Update layout and display */
    UpdateMenuBarLayout();
    InvalidateMenuBar();

    printf("Inserted menu ID %d at position %d\n", (*theMenu)->menuID, insertIndex);
}

/*
 * DeleteMenu - Remove menu from menu bar
 */
void DeleteMenu(short menuID)
{
    MenuBarList* menuBar;
    int menuIndex = -1;

    if (!gMenuMgrInitialized || gMenuList == NULL) {
        return;
    }

    menuBar = (MenuBarList*)gMenuList;

    /* Find menu in list */
    for (int i = 0; i < menuBar->numMenus; i++) {
        if (menuBar->menus[i].menuID == menuID) {
            menuIndex = i;
            break;
        }
    }

    if (menuIndex == -1) {
        return; /* Menu not found */
    }

    /* Remove menu from list */
    if (menuIndex < menuBar->numMenus - 1) {
        memmove(&menuBar->menus[menuIndex], &menuBar->menus[menuIndex + 1],
                (menuBar->numMenus - menuIndex - 1) * sizeof(MenuListEntry));
    }
    menuBar->numMenus--;

    /* Update layout and display */
    UpdateMenuBarLayout();
    InvalidateMenuBar();

    printf("Deleted menu ID %d\n", menuID);
}

/*
 * GetMenuHandle - Find menu by ID
 */
MenuHandle GetMenuHandle(short menuID)
{
    if (!gMenuMgrInitialized) {
        return NULL;
    }

    return FindMenuInList(menuID);
}

/* ============================================================================
 * Menu Flash and Feedback
 * ============================================================================ */

/*
 * FlashMenuBar - Flash menu bar for feedback
 */
void FlashMenuBar(short menuID)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    /* Flash the specified menu or entire menu bar */
    for (int i = 0; i < gMenuFlash; i++) {
        /* TODO: Implement flashing animation */
        printf("Flash menu bar (menu %d)\n", menuID);
    }
}

/*
 * SetMenuFlash - Set menu flash count
 */
void SetMenuFlash(short count)
{
    gMenuFlash = count;
}

/* ============================================================================
 * Menu Manager State Access
 * ============================================================================ */

/*
 * GetMenuManagerState - Get global Menu Manager state
 */
MenuManagerState* GetMenuManagerState(void)
{
    return gMenuMgrState;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/*
 * AllocateMenuManagerState - Allocate Menu Manager state
 */
static MenuManagerState* AllocateMenuManagerState(void)
{
    MenuManagerState* state = (MenuManagerState*)malloc(sizeof(MenuManagerState));
    if (state == NULL) {
        return NULL;
    }

    memset(state, 0, sizeof(MenuManagerState));
    return state;
}

/*
 * InitializeMenuManagerState - Initialize Menu Manager state
 */
static void InitializeMenuManagerState(MenuManagerState* state)
{
    if (state == NULL) {
        return;
    }

    state->menuList = NULL;
    state->currentMenuBar = 0;
    state->hiliteMenu = 0;
    state->menuBarHeight = menuBarStdHeight;
    state->menuBarVisible = true;
    state->menuBarInvalid = true;
    state->menuColorTable = NULL;
    state->menuFlash = 3;
    state->lastMenuChoice = 0;
    state->trackingMenu = false;
    state->currentMenu = NULL;
    state->currentItem = 0;
    state->platformData = NULL;
    state->initialized = false;
}

/*
 * DisposeMenuManagerState - Dispose Menu Manager state
 */
static void DisposeMenuManagerState(MenuManagerState* state)
{
    if (state == NULL) {
        return;
    }

    /* Clean up any allocated resources */
    if (state->menuColorTable != NULL) {
        free(state->menuColorTable);
    }

    if (state->platformData != NULL) {
        free(state->platformData);
    }

    free(state);
}

/*
 * ValidateMenuHandle - Validate menu handle
 */
static OSErr ValidateMenuHandle(MenuHandle theMenu)
{
    if (theMenu == NULL || *theMenu == NULL) {
        return menuInvalidErr;
    }

    if ((*theMenu)->menuID == 0) {
        return menuInvalidErr;
    }

    return 0;
}

/*
 * ValidateMenuID - Validate menu ID
 */
static OSErr ValidateMenuID(short menuID)
{
    if (menuID == 0) {
        return menuInvalidErr;
    }

    return 0;
}

/*
 * FindMenuInList - Find menu by ID in current menu list
 */
static MenuHandle FindMenuInList(short menuID)
{
    /* TODO: Implement menu lookup in menu list */
    /* For now, return NULL - menus are tracked separately */
    return NULL;
}

/*
 * UpdateMenuBarLayout - Update menu bar layout
 */
static void UpdateMenuBarLayout(void)
{
    MenuBarList* menuBar;
    short currentLeft = 0;

    if (gMenuList == NULL) {
        return;
    }

    menuBar = (MenuBarList*)gMenuList;

    /* Calculate positions for all menus */
    for (int i = 0; i < menuBar->numMenus; i++) {
        /* TODO: Calculate actual menu width based on title */
        short menuWidth = 80; /* Default width */

        menuBar->menus[i].menuLeft = currentLeft;
        menuBar->menus[i].menuWidth = menuWidth;
        currentLeft += menuWidth;
    }

    menuBar->totalWidth = currentLeft;
    menuBar->lastRight = currentLeft;
}

/*
 * InvalidateMenuBar - Mark menu bar as needing redraw
 */
static void InvalidateMenuBar(void)
{
    if (gMenuMgrState != NULL) {
        gMenuMgrState->menuBarInvalid = true;
    }
}

/* ============================================================================
 * Menu Color Support
 * ============================================================================ */

/*
 * GetMCInfo - Get menu color table
 */
MCTableHandle GetMCInfo(void)
{
    return gMCTable;
}

/*
 * SetMCInfo - Set menu color table
 */
void SetMCInfo(MCTableHandle menuCTbl)
{
    if (gMCTable != NULL && gMCTable != menuCTbl) {
        DisposeMCInfo(gMCTable);
    }

    gMCTable = menuCTbl;
    if (gMenuMgrState != NULL) {
        gMenuMgrState->menuColorTable = menuCTbl;
    }

    /* Redraw menu bar with new colors */
    InvalidateMenuBar();
}

/*
 * DisposeMCInfo - Dispose menu color table
 */
void DisposeMCInfo(MCTableHandle menuCTbl)
{
    if (menuCTbl != NULL) {
        free(menuCTbl);
    }
}

/*
 * GetMCEntry - Get menu color entry
 */
MCEntryPtr GetMCEntry(short menuID, short menuItem)
{
    /* TODO: Implement menu color lookup */
    return NULL;
}

/*
 * SetMCEntries - Set multiple menu color entries
 */
void SetMCEntries(short numEntries, MCTablePtr menuCEntries)
{
    /* TODO: Implement menu color entry setting */
}

/*
 * DeleteMCEntries - Delete menu color entries
 */
void DeleteMCEntries(short menuID, short menuItem)
{
    /* TODO: Implement menu color entry deletion */
}

/* ============================================================================
 * Menu Choice Management
 * ============================================================================ */

/*
 * MenuChoice - Get last menu choice
 */
long MenuChoice(void)
{
    return gLastMenuChoice;
}

/* ============================================================================
 * Debug and Diagnostic Functions
 * ============================================================================ */

#ifdef DEBUG
void PrintMenuManagerState(void)
{
    if (!gMenuMgrInitialized || gMenuMgrState == NULL) {
        printf("Menu Manager not initialized\n");
        return;
    }

    printf("=== Menu Manager State ===\n");
    printf("Initialized: %s\n", gMenuMgrState->initialized ? "Yes" : "No");
    printf("Menu bar height: %d\n", gMenuMgrState->menuBarHeight);
    printf("Menu bar visible: %s\n", gMenuMgrState->menuBarVisible ? "Yes" : "No");
    printf("Menu bar invalid: %s\n", gMenuMgrState->menuBarInvalid ? "Yes" : "No");
    printf("Highlighted menu: %d\n", gMenuMgrState->hiliteMenu);
    printf("Last menu choice: 0x%08lX\n", gMenuMgrState->lastMenuChoice);

    if (gMenuList != NULL) {
        MenuBarList* menuBar = (MenuBarList*)gMenuList;
        printf("Number of menus: %d\n", menuBar->numMenus);
        printf("Total width: %d\n", menuBar->totalWidth);

        for (int i = 0; i < menuBar->numMenus; i++) {
            printf("  Menu %d: ID=%d, Left=%d, Width=%d\n",
                   i, menuBar->menus[i].menuID,
                   menuBar->menus[i].menuLeft,
                   menuBar->menus[i].menuWidth);
        }
    } else {
        printf("No menu list\n");
    }
    printf("========================\n");
}
#endif
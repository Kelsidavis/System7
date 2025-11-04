#include <stdlib.h>
#include <string.h>
#include "MenuManager/menu_private.h"
/*
 * MenuManagerCore.c - Core Menu Manager Implementation
 *
 * This file implements the core Menu Manager functionality including
 * menu creation, disposal, menu bar management, and the fundamental
 * menu operations. This is THE FINAL CRITICAL COMPONENT that completes
 * the essential Mac OS interface for System 7.1 compatibility.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

#include "../include/MacTypes.h"
#include "../include/QuickDraw/QuickDraw.h"
#include "../include/QuickDraw/DisplayBezel.h"
#include "../include/QuickDrawConstants.h"
#include "../include/ResourceMgr/ResourceMgr.h"
#include "../include/MemoryMgr/MemoryManager.h"

#include "../include/MenuManager/MenuManager.h"
#include "../include/MenuManager/MenuTypes.h"
#include "../include/MenuManager/MenuLogging.h"
#include "../include/WindowManager/WindowManager.h"
#include "../include/FontManager/FontManager.h"
#include "MenuManager/MenuAppleIcon.h"
#include "MenuManager/MenuAppIcon.h"

/* QuickDraw globals */
extern QDGlobals qd;

/* Serial printf for debugging */
extern void serial_puts(const char* str);
extern void DrawText(const void* textBuf, short firstByte, short byteCount);
extern short StringWidth(ConstStr255Param s);

/* ============================================================================
 * Menu Manager Types and Structures
 * ============================================================================ */

/* Menu list entry */
typedef struct MenuListEntry {
    short menuID;
    short menuLeft;
    short menuWidth;
} MenuListEntry;

/* Define struct Menu to match MenuHandle definition in SystemTypes.h */
struct Menu {
    SInt16   menuID;
    SInt16   menuWidth;
    SInt16   menuHeight;
    Handle   menuProc;
    SInt32   enableFlags;
    Str255   menuData;
};

/* Menu bar list structure */
typedef struct MenuBarList {
    short numMenus;
    short totalWidth;
    short lastRight;
    short mbResID;
    MenuListEntry menus[1];  /* Variable length array */
} MenuBarList;

/* Menu error codes */
#define menuInvalidErr -150
#define hierMenu -1

/* ============================================================================
 * Menu Manager State Structure
 * ============================================================================ */

typedef struct MenuManagerState {
    Boolean initialized;
    Boolean menuBarVisible;
    Ptr menuBar;               /* Non-relocatable menu bar */
    Ptr menuList;              /* Non-relocatable menu list */
    short hiliteMenu;
    Boolean menuBarInvalid;
    short currentMenuBar;
    short menuBarHeight;
    Handle menuColorTable;
    short menuFlash;
    long lastMenuChoice;
    Boolean trackingMenu;
    MenuHandle currentMenu;
    short currentItem;
    void* platformData;
} MenuManagerState;

/* ============================================================================
 * Global Menu Manager State
 * ============================================================================ */

static MenuManagerState* gMenuMgrState = NULL;
static Boolean gMenuMgrInitialized = false;

/* Menu bar constants */
#define menuBarStdHeight 20
#define kMenuBarHeight 20

/* Low memory globals (for compatibility) */
static short gMBarHeight = 20;          /* Menu bar height */
static Ptr gMenuList = NULL;            /* Current menu list (non-relocatable) */
static MCTableHandle gMCTable = NULL;   /* Menu color table */
static short gMenuFlash = 3;            /* Flash count */
static long gLastMenuChoice = 0;        /* Last menu choice */

/* Simple menu handle tracking */
#define MAX_MENUS 32
static struct {
    short menuID;
    MenuHandle handle;
} gMenuHandles[MAX_MENUS];
static int gNumMenuHandles = 0;

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
/* Platform_DrawMenuBar declared in menu_private.h */
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

    /* Menu Manager initialized successfully */
}

/*
 * CleanupMenus - Clean up Menu Manager resources
 */
void CleanupMenus(void)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    /* CRITICAL: Clean up menu extended data to prevent memory leaks */
    CleanupMenuExtData();

    /* CRITICAL: Clear menu handle tracking array to prevent stale pointers */
    for (int i = 0; i < gNumMenuHandles; i++) {
        gMenuHandles[i].menuID = 0;
        gMenuHandles[i].handle = NULL;
    }
    gNumMenuHandles = 0;

    /* Clean up platform-specific resources */
    Platform_CleanupMenuSystem();

    /* Dispose of menu color table */
    if (gMCTable != NULL) {
        DisposeMCInfo(gMCTable);
        gMCTable = NULL;
    }

    /* Clear menu list */
    if (gMenuList != NULL) {
        DisposePtr((Ptr)gMenuList);
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

    return (Handle)gMenuList;  /* Cast Ptr to Handle for API compatibility */
}

/*
 * GetNewMBar - Create menu list from MBAR resource
 *
 * Loads an MBAR resource by ID and creates menu list with MENU resources
 */
Handle GetNewMBar(short menuBarID)
{
    if (!gMenuMgrInitialized) {
        return NULL;
    }

    /* Load MBAR resource */
    Handle mbarHandle = GetResource('MBAR', menuBarID);
    if (mbarHandle == NULL) {
        MENU_LOG_WARN("GetNewMBar: MBAR resource %d not found\n", menuBarID);
        /* Return empty menu list instead of NULL */
        size_t menuBarSize = sizeof(MenuBarList) + (MAX_MENUS - 1) * sizeof(MenuListEntry);
        MenuBarList* menuBar = (MenuBarList*)NewPtr(menuBarSize);
        if (menuBar) {
            menuBar->numMenus = 0;
            menuBar->totalWidth = 0;
            menuBar->lastRight = 0;
            menuBar->mbResID = menuBarID;
        }
        return (Handle)menuBar;  /* Cast Ptr to Handle for API compatibility */
    }

    /* Parse MBAR resource to get menu ID array */
    extern short* ParseMBARResource(Handle resourceHandle, short* outMenuCount);
    short menuCount = 0;
    short* menuIDs = ParseMBARResource(mbarHandle, &menuCount);

    if (!menuIDs || menuCount <= 0) {
        MENU_LOG_ERROR("GetNewMBar: Failed to parse MBAR %d\n", menuBarID);
        return NULL;
    }

    /* Allocate MenuBarList for the menus */
    size_t menuBarSize = sizeof(MenuBarList) + (menuCount - 1) * sizeof(MenuListEntry);
    MenuBarList* menuBar = (MenuBarList*)NewPtr(menuBarSize);
    if (!menuBar) {
        DisposePtr((Ptr)menuIDs);
        return NULL;
    }

    menuBar->numMenus = menuCount;
    menuBar->totalWidth = 0;
    menuBar->lastRight = 0;
    menuBar->mbResID = menuBarID;

    /* Load each MENU resource and add to menu bar */
    for (int i = 0; i < menuCount; i++) {
        MenuHandle theMenu = GetMenu(menuIDs[i]);
        if (theMenu) {
            menuBar->menus[i].menuID = menuIDs[i];
            menuBar->menus[i].menuLeft = 0;
            menuBar->menus[i].menuWidth = 0;
        } else {
            MENU_LOG_WARN("GetNewMBar: Could not load MENU resource %d\n", menuIDs[i]);
            /* Skip this menu */
        }
    }

    DisposePtr((Ptr)menuIDs);

    MENU_LOG_DEBUG("GetNewMBar: Created menu bar %d with %d menus\n", menuBarID, menuCount);
    return (Handle)menuBar;  /* Cast Ptr to Handle for API compatibility */
}

/*
 * SetMenuBar - Set current menu list
 *
 * IMPORTANT: This function takes ownership of menuList. The caller should NOT
 * dispose of menuList after calling this function, as it will be disposed when
 * a new menu list is set or during CleanupMenus().
 */
void SetMenuBar(Handle menuList)
{
    if (!gMenuMgrInitialized) {
        return;
    }

    /* SAFETY: Only dispose old menu list if it's different from new one */
    if (gMenuList != NULL && gMenuList != (Ptr)menuList) {
        /* Dispose old menu list - we own this memory */
        DisposePtr(gMenuList);
        gMenuList = NULL;  /* Prevent use-after-free */
    }

    /* Take ownership of new menu list */
    gMenuList = (Ptr)menuList;  /* Cast Handle to Ptr (assuming non-relocatable) */
    gMenuMgrState->menuList = (Ptr)menuList;

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
        DisposePtr((Ptr)gMenuList);
        gMenuList = NULL;
        gMenuMgrState->menuList = NULL;
    }

    /* CRITICAL: Clear menu handle tracking array to prevent stale pointers */
    /* NOTE: We don't dispose the menu handles themselves here, only clear references */
    for (int i = 0; i < gNumMenuHandles; i++) {
        gMenuHandles[i].menuID = 0;
        gMenuHandles[i].handle = NULL;
    }
    gNumMenuHandles = 0;

    /* Clear menu bar display */
    InvalidateMenuBar();
}

/*
 * SetupDefaultMenus - Manually populate default menus for testing
 * This is a temporary workaround to ensure menus display
 */
void SetupDefaultMenus(void)
{
    MenuBarList* menuBar;

    if (!gMenuList) {
        MENU_LOG_ERROR("SetupDefaultMenus: gMenuList is NULL, cannot setup\n");
        return;
    }

    menuBar = (MenuBarList*)gMenuList;

    /* If menus already exist (Finder populated them), leave them intact */
    if (menuBar->numMenus > 0) {
        MENU_LOG_INFO("SetupDefaultMenus: existing menu list detected (numMenus=%d), skipping fallback\n",
                      menuBar->numMenus);
        return;
    }

    /* Manually populate the 7 standard menus (Apple, File, Edit, View, Label, Special, Application) */
    menuBar->numMenus = 7;
    menuBar->totalWidth = 0;
    menuBar->lastRight = 10; /* Start after left margin */

    /* Apple Menu (ID 128) */
    menuBar->menus[0].menuID = 128;
    menuBar->menus[0].menuLeft = 10;
    menuBar->menus[0].menuWidth = 30;  /* Width for Apple symbol */

    /* File Menu (ID 129) */
    menuBar->menus[1].menuID = 129;
    menuBar->menus[1].menuLeft = 40;
    menuBar->menus[1].menuWidth = 40;  /* Width for "File" */

    /* Edit Menu (ID 130) */
    menuBar->menus[2].menuID = 130;
    menuBar->menus[2].menuLeft = 80;
    menuBar->menus[2].menuWidth = 40;  /* Width for "Edit" */

    /* View Menu (ID 131) */
    menuBar->menus[3].menuID = 131;
    menuBar->menus[3].menuLeft = 120;
    menuBar->menus[3].menuWidth = 45;  /* Width for "View" */

    /* Label Menu (ID 132) */
    menuBar->menus[4].menuID = 132;
    menuBar->menus[4].menuLeft = 165;
    menuBar->menus[4].menuWidth = 50;  /* Width for "Label" */

    /* Special Menu (ID 133) */
    menuBar->menus[5].menuID = 133;
    menuBar->menus[5].menuLeft = 215;
    menuBar->menus[5].menuWidth = 65;  /* Width for "Special" */

    /* Application Menu (Finder icon) */
    menuBar->menus[6].menuID = (short)kApplicationMenuID;
    menuBar->menus[6].menuLeft = 280;
    menuBar->menus[6].menuWidth = 20;  /* Icon-only */

    menuBar->lastRight = 300;
    menuBar->totalWidth = 290;

    if (gMenuMgrState) {
        gMenuMgrState->menuBar = (Ptr)menuBar;
    }

    MENU_LOG_INFO("SetupDefaultMenus: Manually set up %d menus\n", menuBar->numMenus);
}

/*
 * DrawMenuBar - Redraw the menu bar
 */
void DrawMenuBar(void)
{
    extern void serial_puts(const char* str);
    serial_puts("DrawMenuBar called\n");

    if (!gMenuMgrInitialized) {
        /* serial_puts("DrawMenuBar: MenuManager not initialized\n"); */
        return;
    }

    GrafPtr savePort;
    GetPort(&savePort);

    /* Make sure we draw relative to the screen's coordinate space */
    GrafPtr menuPort = NULL;
    GetWMgrPort(&menuPort);
    if (menuPort && menuPort->portBits.baseAddr) {
        SetPort(menuPort);  /* Draw into Window Manager port */
    } else if (qd.thePort) {
        SetPort(qd.thePort);  /* Fallback */
    }

    /* Menu bar rectangle */
    Rect menuBarRect;
    SetRect(&menuBarRect, 0, 0, qd.screenBits.bounds.right, 20);

    /* Clear menu bar area with white */
    BackColor(whiteColor);
    ForeColor(blackColor);
    PenNormal();
    ClipRect(&menuBarRect);
    FillRect(&menuBarRect, &qd.white);  /* White background */

    /* Set proper font for menu bar text */
    TextFont(0);   /* System font (Chicago) */
    TextSize(12);  /* Standard menu bar size */
    TextFace(0);   /* Plain text */

    /* Draw bottom line */
    MoveTo(0, 19);
    LineTo(qd.screenBits.bounds.right - 1, 19);

    /* Initialize menu title tracking */
    extern void InitMenuTitleTracking(void);
    extern void AddMenuTitle(short menuID, short left, short width, const char* title);
    InitMenuTitleTracking();

    /* Draw menu titles */
    short x = 0;
    UpdateMenuBarLayout();

    serial_puts("DrawMenuBar: Checking menu state...\n");
    if (gMenuMgrState) {
        serial_puts("DrawMenuBar: gMenuMgrState exists\n");
        if (gMenuMgrState->menuBar) {
            MenuBarList* menuBar = (MenuBarList*)gMenuMgrState->menuBar;
            serial_puts("DrawMenuBar: menuBar exists\n");
            /* MENU_LOG_TRACE used to debug drawing when serial IO is unstable */

            MENU_LOG_DEBUG("DrawMenuBar: numMenus = %d\n", menuBar->numMenus);

            for (int i = 0; i < menuBar->numMenus; i++) {
                MENU_LOG_DEBUG("DrawMenuBar: Processing menu %d (ID=%d)\n", i, menuBar->menus[i].menuID);
                short currentMenuID = menuBar->menus[i].menuID;
                MenuHandle menu = GetMenuHandle(currentMenuID);
                if (menu) {
                    /* serial_puts("DrawMenuBar: Menu handle found\n"); */

                    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
                    HLock((Handle)menu);

                    /* Debug: show MenuInfo offsets */
                    MenuInfo* mptr = (MenuInfo*)*menu;
                    /* MENU_LOG_TRACE("  MenuID: %d\n", mptr->menuID); */
                    /* MENU_LOG_TRACE("  sizeof(MenuInfo): %d\n", sizeof(MenuInfo)); */
                    /* MENU_LOG_TRACE("  offsetof menuData: %d\n", ((char*)&(mptr->menuData) - (char*)mptr)); */

                    /* Get menu title */
                    unsigned char titleLen = (**menu).menuData[0];

                    /* Use precomputed left from layout for consistent placement */
                    x = menuBar->menus[i].menuLeft;
                    short menuWidth = 0;
                    char titleText[256] = {0};

                    /* Skip duplicated placeholder menu */
                    if (mptr->menuID == 1) {
                        continue;
                    }

                    /* Draw Apple glyph even when the menu title is blank */
                    if (mptr->menuID == 128) {
                        menuWidth = 24;  /* Standard icon width */
                        /* Skip drawing if highlighted - same as text menus */
                        if (!(gMenuMgrState && gMenuMgrState->hiliteMenu == mptr->menuID)) {
                            menuWidth = MenuAppleIcon_Draw(qd.thePort, x, 0, false);
                            extern void serial_puts(const char* str);
                            serial_puts("[DRAWBAR] Drew Apple icon\n");
                        } else {
                            extern void serial_puts(const char* str);
                            serial_puts("[DRAWBAR] SKIPPING Apple icon (highlighted)\n");
                        }
                        AddMenuTitle(mptr->menuID, x, menuWidth, "Apple");
                        x += menuWidth;
                        continue;
                    }

                    /* Draw Finder application icon regardless of title length */
                    if (mptr->menuID == (short)kApplicationMenuID) {
                        menuWidth = 24;  /* Standard icon width */
                        /* Skip drawing if highlighted - same as text menus */
                        if (!(gMenuMgrState && gMenuMgrState->hiliteMenu == mptr->menuID)) {
                            menuWidth = MenuAppIcon_Draw(qd.thePort, x, 0, false);
                            extern void serial_puts(const char* str);
                            serial_puts("[DRAWBAR] Drew Finder icon\n");
                        } else {
                            extern void serial_puts(const char* str);
                            serial_puts("[DRAWBAR] SKIPPING Finder icon (highlighted)\n");
                        }
                        AddMenuTitle(mptr->menuID, x, menuWidth, "Application");
                        x += menuWidth;
                        continue;
                    }

                    /* CRITICAL FIX: Ensure titleLen is reasonable */
                    if (titleLen > 20) {
                        titleLen = 4; /* Default to 4 for "File", "Edit", etc */
                    }

                    /* Check if title is at wrong offset */
                    if (titleLen > 0 && titleLen <= 20) { /* More restrictive sanity check */
                        /* Get precomputed menuWidth from layout */
                        menuWidth = menuBar->menus[i].menuWidth;
                        memcpy(titleText, &(**menu).menuData[1], titleLen);
                        titleText[titleLen] = '\0';

                        /* CRITICAL FIX: Skip TEXT DRAWING (but not tracking!) if highlighted
                         * DrawMenuBar should not redraw over a menu that HiliteMenu has inverted.
                         * If we redraw now, we would erase the inverted highlight and leave
                         * BOTH black text and inverted traces visible, creating the double-text
                         * artifact.
                         *
                         * HOWEVER, we MUST still call AddMenuTitle to update the titleRect tracking,
                         * otherwise the titleRect becomes invalid and the menu can't be opened again.
                         * So we skip ONLY the text rendering, not the tracking update. */
                        Boolean isHighlighted = (gMenuMgrState && gMenuMgrState->hiliteMenu == mptr->menuID);

                        if (!isHighlighted) {
                            /* Draw normal text title - moved 4px right and 1px down */
                            ForeColor(blackColor);  /* Ensure black text */
                            MoveTo(x + 4, 14);  /* Shifted right 4px and down 1px */

                            /* Use actual menu data for title, not hardcoded values */
                            /* This allows custom menus to display their own titles */
                            MENU_LOG_DEBUG("DrawMenuBar: Drawing menu ID %d, title len=%d at x=%d\n", mptr->menuID, titleLen, x + 4);

                            /* Draw title from menu data */
                            extern void serial_puts(const char* str);

                            /* CRITICAL FIX: Use DrawString (FontManager) instead of DrawText (QuickDraw)
                             * to ensure consistent rendering path. Both DrawMenuBar and DrawMenuTitle
                             * must use the SAME text rendering function to avoid position mismatches. */
                            serial_puts("[DRAWBAR] Normal text being drawn via DrawString (FontManager)\n");

                            /* Create Pascal string for DrawString */
                            unsigned char pascalStr[256];
                            pascalStr[0] = titleLen;
                            memcpy(&pascalStr[1], &(**menu).menuData[1], titleLen);
                            DrawString((ConstStr255Param)pascalStr);

                            MENU_LOG_TRACE("DrawMenuBar: Drew title '%.*s' for menu ID %d\n", titleLen, titleText, mptr->menuID);

                            /* Log the titleRect that will be recorded */
                            static char buf[256];
                            extern int snprintf(char*, size_t, const char*, ...);
                            snprintf(buf, sizeof(buf), "[DRAWBAR] Recording AddMenuTitle: menuID=%d, left=%d, right=%d, width=%d (x=%d+4 for text)\n",
                                     mptr->menuID, x, x+menuWidth, menuWidth, x);
                            serial_puts(buf);
                        } else {
                            extern void serial_puts(const char* str);
                            serial_puts("[DRAWBAR] SKIPPING text for highlighted menu, but updating tracking\n");
                        }

                        /* ALWAYS update titleRect tracking, even if menu is highlighted
                         * This ensures the titleRect stays valid and the menu can be opened again */
                        AddMenuTitle(mptr->menuID, x, menuWidth, titleText);
                        x += menuWidth;
                    }

                    /* Unlock handle after use */
                    HUnlock((Handle)menu);
                } else {
                    MENU_LOG_DEBUG("DrawMenuBar: GetMenuHandle returned NULL for ID %d\n", currentMenuID);
                }
            }
        } else {
            /* serial_puts("DrawMenuBar: menuBar is NULL\n"); */
        }
    } else {
        /* serial_puts("DrawMenuBar: gMenuMgrState is NULL\n"); */
        /* Draw default Apple menu if no menus installed */
        (void)MenuAppleIcon_Draw(qd.thePort, x, 0, false);
    }

    /* NOTE: Finder icon is drawn as part of the menu list loop above (Application menu).
     * Do NOT draw it again here - that was causing a double-draw artifact. */

    QD_DrawCRTBezel();

    SetPort(savePort);
    gMenuMgrState->menuBarInvalid = false;
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
    MENU_LOG_TRACE("HiliteMenu ENTER: menuID=%d\n", menuID);
    if (!gMenuMgrInitialized) {
        serial_puts("HiliteMenu: Not initialized\n");
        return;
    }

    MENU_LOG_TRACE("HiliteMenu: Current hilite=%d\n", gMenuMgrState->hiliteMenu);
    /* Unhighlight previous menu if any */
    if (gMenuMgrState->hiliteMenu != 0 && gMenuMgrState->hiliteMenu != menuID) {
        extern void HiliteMenuTitle(short menuID, Boolean hilite);
        serial_puts("HiliteMenu: About to unhighlight previous\n");
        HiliteMenuTitle(gMenuMgrState->hiliteMenu, false);
        serial_puts("HiliteMenu: Unhighlighted previous\n");
    }

    /* Set new hilite menu */
    gMenuMgrState->hiliteMenu = menuID;

    /* Highlight new menu if not 0 */
    if (menuID != 0) {
        extern void HiliteMenuTitle(short menuID, Boolean hilite);
        serial_puts("HiliteMenu: About to highlight new menu\n");
        HiliteMenuTitle(menuID, true);
        serial_puts("HiliteMenu: Highlighted new menu\n");
    }
    serial_puts("HiliteMenu EXIT\n");
}

/* GetMBarHeight is defined as a macro in MenuManager.h */

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
    theMenu = (MenuHandle)NewPtr(sizeof(MenuInfo*));
    if (theMenu == NULL) {
        return NULL;
    }

    /* Allocate menu info */
    menuPtr = (MenuInfo*)NewPtr(sizeof(MenuInfo));
    if (menuPtr == NULL) {
        DisposePtr((Ptr)theMenu);
        return NULL;
    }

    /* Initialize menu info */
    memset(menuPtr, 0, sizeof(MenuInfo));
    *theMenu = (struct Menu*)menuPtr;

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
    /* Clear rest of menuData to prevent garbage */
    memset(&menuPtr->menuData[1 + titleLen], 0, 255 - titleLen);

    /* Created menu */

    /* Add to tracking array */
    if (gNumMenuHandles < MAX_MENUS) {
        gMenuHandles[gNumMenuHandles].menuID = menuID;
        gMenuHandles[gNumMenuHandles].handle = theMenu;
        gNumMenuHandles++;

        /* Debug output */
        MENU_LOG_TRACE("NewMenu: Created menu ID %d, title '%.*s' (handle %p, total menus: %d)\n",
                      menuID, titleLen, &menuTitle[1], theMenu, gNumMenuHandles);
    }

    return theMenu;
}

/*
 * GetMenu - Load menu from MENU resource
 *
 * Loads a MENU resource by ID and returns a MenuHandle
 */
MenuHandle GetMenu(short resourceID)
{
    if (!gMenuMgrInitialized) {
        return NULL;
    }

    /* Load MENU resource */
    Handle menuHandle = GetResource('MENU', resourceID);
    if (menuHandle == NULL) {
        MENU_LOG_WARN("GetMenu: MENU resource %d not found\n", resourceID);
        /* Return fallback menu with generic title */
        unsigned char title[256];
        snprintf((char*)&title[1], 255, "Menu %d", resourceID);
        title[0] = strlen((char*)&title[1]);
        return NewMenu(resourceID, title);
    }

    /* Parse MENU resource to create menu */
    extern MenuHandle ParseMENUResource(Handle resourceHandle);
    MenuHandle theMenu = ParseMENUResource(menuHandle);

    if (!theMenu) {
        MENU_LOG_ERROR("GetMenu: Failed to parse MENU resource %d\n", resourceID);
        return NULL;
    }

    MENU_LOG_DEBUG("GetMenu: Successfully loaded MENU resource %d\n", resourceID);
    return theMenu;
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

    /* CRITICAL: Remove from gMenuHandles tracking array to prevent stale pointers */
    for (int i = 0; i < gNumMenuHandles; i++) {
        if (gMenuHandles[i].handle == theMenu) {
            /* Shift array down to remove this entry */
            for (int j = i; j < gNumMenuHandles - 1; j++) {
                gMenuHandles[j] = gMenuHandles[j + 1];
            }
            /* Clear last entry */
            gMenuHandles[gNumMenuHandles - 1].menuID = 0;
            gMenuHandles[gNumMenuHandles - 1].handle = NULL;
            gNumMenuHandles--;
            break;
        }
    }

    /* Free menu data */
    if (*theMenu != NULL) {
        DisposePtr((Ptr)*theMenu);
    }
    DisposePtr((Ptr)theMenu);
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
        MENU_LOG_ERROR("InsertMenu: ValidateMenuHandle failed for ID %d\n", (*theMenu)->menuID);
        return;
    }

    /* Create menu list if it doesn't exist */
    if (gMenuList == NULL) {
        /* Allocate space for MenuBarList + room for MAX_MENUS menu entries */
        size_t menuBarSize = sizeof(MenuBarList) + (MAX_MENUS - 1) * sizeof(MenuListEntry);
        gMenuList = NewPtr(menuBarSize);
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

    /* Make sure menu bar is set in state */
    if (gMenuMgrState) {
        gMenuMgrState->menuBar = gMenuList;  /* Already a Ptr */
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

    /* Expand menu list - allocate new memory and copy */
    size_t newSize = sizeof(MenuBarList) + (menuBar->numMenus) * sizeof(MenuListEntry);
    Ptr newMenuList = NewPtr(newSize);
    if (newMenuList == NULL) {
        MENU_LOG_ERROR("InsertMenu: NewPtr failed for size %zu\n", newSize);
        return;
    }

    /* Copy existing data to new buffer BEFORE disposing old buffer */
    size_t oldSize = sizeof(MenuBarList) + (menuBar->numMenus - 1) * sizeof(MenuListEntry);
    memcpy(newMenuList, gMenuList, oldSize);

    /* CRITICAL: Dispose old buffer and immediately update all pointers */
    Ptr oldMenuList = gMenuList;  /* Save old pointer for disposal */
    gMenuList = newMenuList;      /* Update global first */
    menuBar = (MenuBarList*)gMenuList;  /* Update local pointer */
    if (gMenuMgrState) {
        gMenuMgrState->menuBar = gMenuList;  /* Update state */
    }
    DisposePtr(oldMenuList);  /* Dispose old buffer AFTER all pointers updated */

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

    /* Debug output */
    MENU_LOG_TRACE("InsertMenu: Inserted menu ID %d at position %d (total in bar: %d)\n",
                  (*theMenu)->menuID, insertIndex, menuBar->numMenus);

    /* Update layout and display */
    UpdateMenuBarLayout();
    InvalidateMenuBar();

    /* Inserted menu */
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

    /* Deleted menu */
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
        /* Get menu bar rectangle - standard Mac menu bar is full screen width, 20 pixels high */
        Rect menuBarRect;
        extern void Platform_GetScreenBounds(Rect* bounds);
        Platform_GetScreenBounds(&menuBarRect);
        menuBarRect.bottom = menuBarRect.top + 20; /* Menu bar height */

        /* Invert the menu bar for flash effect */
        InvertRect(&menuBarRect);

        /* Brief delay for visual effect */
        extern void Platform_WaitTicks(short ticks);
        Platform_WaitTicks(2); /* ~33ms at 60 Hz */

        /* Invert back to restore */
        InvertRect(&menuBarRect);

        /* Delay between flashes */
        if (i < gMenuFlash - 1) {
            Platform_WaitTicks(1);
        }
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
    MenuManagerState* state = (MenuManagerState*)NewPtr(sizeof(MenuManagerState));
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
        DisposePtr((Ptr)state->menuColorTable);
    }

    if (state->platformData != NULL) {
        DisposePtr((Ptr)state->platformData);
    }

    DisposePtr((Ptr)state);
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
    /* Look up in tracking array */
    for (int i = 0; i < gNumMenuHandles; i++) {
        if (gMenuHandles[i].menuID == menuID) {
            MENU_LOG_TRACE("FindMenuInList: Found menu ID %d at index %d (handle %p)\n",
                          menuID, i, gMenuHandles[i].handle);
            return gMenuHandles[i].handle;
        }
    }
    MENU_LOG_TRACE("FindMenuInList: Menu ID %d not found (searched %d menus)\n",
                  menuID, gNumMenuHandles);
    return NULL;
}

static short MeasureMenuTitleWidth(short menuID)
{
    const short kIconMenuWidth = 24;
    const short kFallbackWidth = 48;
    const short kPadding = 12;

    if (menuID == 128 || menuID == (short)kApplicationMenuID) {
        return kIconMenuWidth;
    }

    MenuHandle menu = GetMenuHandle(menuID);
    if (menu == NULL) {
        return kFallbackWidth;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)menu);

    unsigned char titleLen = (**menu).menuData[0];
    if (titleLen == 0) {
        HUnlock((Handle)menu);
        return kFallbackWidth;
    }
    Str255 title;
    title[0] = titleLen;
    memcpy(&title[1], &(**menu).menuData[1], titleLen);

    /* Unlock handle after copying data */
    HUnlock((Handle)menu);

    GrafPtr savePort;
    GetPort(&savePort);
    if (qd.thePort) {
        SetPort(qd.thePort);
    }

    short textWidth = StringWidth((ConstStr255Param)title);

    if (savePort) {
        SetPort(savePort);
    }

    return textWidth + kPadding;
}

/*
 * UpdateMenuBarLayout - Update menu bar layout
 */
static void UpdateMenuBarLayout(void)
{
    MenuBarList* menuBar;
    short currentLeft = 0;
    short systemRight;

    if (gMenuList == NULL) {
        return;
    }

    menuBar = (MenuBarList*)gMenuList;

    /* Position application (non-system) menus from left, system menus from right */
    short screenWidth = qd.screenBits.bounds.right;
    systemRight = screenWidth;

    const short kMenuSpacing = 12;
    for (int i = 0; i < menuBar->numMenus; i++) {
        short id = menuBar->menus[i].menuID;
        short menuWidth = MeasureMenuTitleWidth(id);

        /* Treat classic system menu IDs and Application menu as right-aligned */
        if ((id >= (short)0xB000 && id <= (short)0xBFFF) || id == (short)0xBF97) {
            systemRight -= menuWidth;
            menuBar->menus[i].menuLeft = systemRight;
            menuBar->menus[i].menuWidth = menuWidth;
            systemRight -= kMenuSpacing;
        } else {
            menuBar->menus[i].menuLeft = currentLeft;
            menuBar->menus[i].menuWidth = menuWidth;
            currentLeft += menuWidth + kMenuSpacing;
        }
        char layoutBuf[128];
        snprintf(layoutBuf, sizeof(layoutBuf),
                 "UpdateMenuBarLayout: ID=%d left=%d width=%d systemRight=%d currentLeft=%d\n",
                 id, menuBar->menus[i].menuLeft, menuBar->menus[i].menuWidth, systemRight, currentLeft);
        serial_puts(layoutBuf);
    }

    if (currentLeft > 0 && currentLeft >= kMenuSpacing) {
        currentLeft -= kMenuSpacing; /* remove trailing spacing */
    }

    short usedRight = screenWidth - systemRight;
    if (usedRight >= kMenuSpacing) {
        usedRight -= kMenuSpacing;
    }

    menuBar->totalWidth = currentLeft + usedRight;
    menuBar->lastRight = systemRight >= 0 ? systemRight + kMenuSpacing : screenWidth;
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
        gMenuMgrState->menuColorTable = (Handle)menuCTbl;
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
        DisposePtr((Ptr)menuCTbl);
    }
}

/* Global menu color table - simple implementation for basic color support */
static MCTable* gMenuColorTable = NULL;

/*
 * GetMCEntry - Get menu color entry
 */
MCEntryPtr GetMCEntry(short menuID, short menuItem)
{
    /* Return NULL if no color table exists (use default colors) */
    if (gMenuColorTable == NULL) {
        return NULL;
    }

    /* Search for matching menu color entry */
    for (short i = 0; i < gMenuColorTable->mctSize; i++) {
        MCEntry* entry = &gMenuColorTable->mctTable[i];
        if (entry->mctID == menuID && entry->mctItem == menuItem) {
            return entry;
        }
    }

    return NULL;  /* No custom colors for this menu/item */
}

/*
 * SetMCEntries - Set multiple menu color entries
 */
void SetMCEntries(short numEntries, MCTablePtr menuCEntries)
{
    if (numEntries <= 0 || menuCEntries == NULL) {
        return;
    }

    /* Allocate or reallocate color table */
    if (gMenuColorTable == NULL) {
        gMenuColorTable = (MCTable*)NewPtrClear(sizeof(MCTable) + (numEntries * sizeof(MCEntry)));
        if (gMenuColorTable == NULL) {
            return;  /* Allocation failed */
        }
        gMenuColorTable->mctSize = numEntries;
    }

    /* Copy color entries */
    for (short i = 0; i < numEntries && i < gMenuColorTable->mctSize; i++) {
        gMenuColorTable->mctTable[i] = menuCEntries->mctTable[i];
    }
}

/*
 * DeleteMCEntries - Delete menu color entries
 */
void DeleteMCEntries(short menuID, short menuItem)
{
    if (gMenuColorTable == NULL) {
        return;
    }

    /* Find and remove matching entries by shifting array */
    short writeIndex = 0;
    for (short readIndex = 0; readIndex < gMenuColorTable->mctSize; readIndex++) {
        MCEntry* entry = &gMenuColorTable->mctTable[readIndex];
        /* Keep entry if it doesn't match the deletion criteria */
        if (!(entry->mctID == menuID &&
              (menuItem == 0 || entry->mctItem == menuItem))) {
            if (writeIndex != readIndex) {
                gMenuColorTable->mctTable[writeIndex] = gMenuColorTable->mctTable[readIndex];
            }
            writeIndex++;
        }
    }
    gMenuColorTable->mctSize = writeIndex;
}

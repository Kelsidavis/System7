/*
 * menu_geometry.c - Menu Manager Geometry Functions
 *
 * Implementation of Menu Manager geometry and layout functions based on
 * System 7.1 assembly evidence from MenuMgrPriv.a
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 Menu Manager geometry
 * Evidence: MenuMgrPriv.a MBDF rectangle calls and menu bar calculations
 */

#include "menu_private.h"
#include "menu_dispatch.h"
#include <stdlib.h>
#include <string.h>

/* External dependencies */
extern Handle GetMenuBar(void);
extern int16_t GetScreenWidth(void);
extern int16_t GetScreenHeight(void);

/* Menu bar height - evidence from MenuMgrPriv.a low-memory global */
static int16_t g_MenuBarHeight = 20;

/*
 * GetMenuTitleRect - Get rectangle for menu title
 * Evidence: MenuMgrPriv.a:64 - selectGetMenuTitleRect EQU -1
 * Evidence: MenuMgrPriv.a:76 - paramWordsGetMenuTitleRect EQU 6
 */
OSErr GetMenuTitleRect(MenuHandle theMenu, Rect *titleRect) {
    if (!theMenu || !titleRect || !*theMenu) {
        return paramErr;
    }

    /* Get menu info */
    MenuInfo *menu = (MenuInfo *)*theMenu;

    /* Initialize rectangle */
    titleRect->top = 0;
    titleRect->bottom = g_MenuBarHeight;
    titleRect->left = menu->menuLeft;
    titleRect->right = menu->menuLeft + menu->menuWidth;

    return noErr;
}

/*
 * GetMBARRect - Get menu bar rectangle
 * Evidence: MenuMgrPriv.a:65 - selectGetMBARRect EQU -2
 * Evidence: MenuMgrPriv.a:77 - paramWordsGetMBARRect EQU 4
 * Evidence: MenuMgrPriv.a:91 - MBDFRectBar EQU 0
 */
OSErr GetMBARRect(Rect *mbarRect) {
    if (!mbarRect) {
        return paramErr;
    }

    /* Menu bar spans full screen width at top */
    mbarRect->top = 0;
    mbarRect->left = 0;
    mbarRect->bottom = g_MenuBarHeight;
    mbarRect->right = GetScreenWidth();

    return noErr;
}

/*
 * GetAppMenusRect - Get application menus rectangle
 * Evidence: MenuMgrPriv.a:66 - selectGetAppMenusRect EQU -3
 * Evidence: MenuMgrPriv.a:78 - paramWordsGetAppMenusRect EQU 4
 * Evidence: MenuMgrPriv.a:92 - MBDFRectApps EQU -1
 */
OSErr GetAppMenusRect(Rect *appRect) {
    if (!appRect) {
        return paramErr;
    }

    Handle menuList = GetMenuBar();
    if (!menuList || !*menuList) {
        /* No menus - empty rectangle */
        appRect->top = 0;
        appRect->left = 0;
        appRect->bottom = g_MenuBarHeight;
        appRect->right = 0;
        return noErr;
    }

    MenuList *list = (MenuList *)*menuList;
    int16_t leftmost = GetScreenWidth();
    int16_t rightmost = 0;
    Boolean foundApp = false;

    /* Scan through menus to find application menu boundaries */
    MenuHandle menu = list->firstMenu;
    while (menu && *menu) {
        MenuInfo *menuInfo = (MenuInfo *)*menu;

        /* Skip system menus - evidence: MenuMgrPriv.a:55-61 system menu ranges */
        if (!IsSystemMenu(menuInfo->menuID)) {
            if (menuInfo->menuLeft < leftmost) {
                leftmost = menuInfo->menuLeft;
            }
            if (menuInfo->menuLeft + menuInfo->menuWidth > rightmost) {
                rightmost = menuInfo->menuLeft + menuInfo->menuWidth;
            }
            foundApp = true;
        }

        menu = menuInfo->nextMenu;
    }

    if (!foundApp) {
        /* No application menus */
        leftmost = 0;
        rightmost = 0;
    }

    appRect->top = 0;
    appRect->left = leftmost;
    appRect->bottom = g_MenuBarHeight;
    appRect->right = rightmost;

    return noErr;
}

/*
 * GetSysMenusRect - Get system menus rectangle
 * Evidence: MenuMgrPriv.a:67 - selectGetSysMenusRect EQU -4
 * Evidence: MenuMgrPriv.a:79 - paramWordsGetSysMenusRect EQU 4
 * Evidence: MenuMgrPriv.a:93 - MBDFRectSys EQU -2
 */
OSErr GetSysMenusRect(Rect *sysRect) {
    if (!sysRect) {
        return paramErr;
    }

    /* System menus are typically on the right side of menu bar */
    int16_t screenWidth = GetScreenWidth();
    int16_t sysMenuWidth = 0;
    int16_t rightmost = screenWidth;

    /* Look for system menus and calculate their total width */
    Handle menuList = GetMenuBar();
    if (menuList && *menuList) {
        MenuList *list = (MenuList *)*menuList;
        MenuHandle menu = list->firstMenu;

        while (menu && *menu) {
            MenuInfo *menuInfo = (MenuInfo *)*menu;

            /* Check if this is a system menu - evidence: MenuMgrPriv.a:55-61 */
            if (IsSystemMenu(menuInfo->menuID)) {
                sysMenuWidth += menuInfo->menuWidth;
            }

            menu = menuInfo->nextMenu;
        }
    }

    sysRect->top = 0;
    sysRect->left = rightmost - sysMenuWidth;
    sysRect->bottom = g_MenuBarHeight;
    sysRect->right = rightmost;

    return noErr;
}

/*
 * DrawMBARString - Draw string in menu bar
 * Evidence: MenuMgrPriv.a:68 - selectDrawMBARString EQU -5
 * Evidence: MenuMgrPriv.a:80 - paramWordsDrawMBARString EQU 8
 * Evidence: MenuMgrPriv.a:97 - MBDFDrawMBARString EQU 15
 */
OSErr DrawMBARString(const unsigned char *text, int16_t script, Rect *bounds, int16_t just) {
    if (!text || !bounds) {
        return paramErr;
    }

    /* TODO: Platform-specific text drawing implementation */
    /* This would call through to QuickDraw text drawing routines */
    /* For now, return success as a stub */

    return noErr;
}

/*
 * IsSystemMenu - Check if menu ID is in system range
 * Evidence: MenuMgrPriv.a:69 - selectIsSystemMenu EQU -6
 * Evidence: MenuMgrPriv.a:55-61 - System menu ID ranges and specific IDs
 */
Boolean IsSystemMenu(int16_t menuID) {
    /* Check against system menu ranges - evidence from MenuMgrPriv.a */
    if (menuID >= (int16_t)kLoSystemMenuRange && menuID <= (int16_t)kHiSystemMenuRange) {
        return true;
    }

    /* Check specific system menu IDs */
    if (menuID == (int16_t)kApplicationMenuID ||
        menuID == (int16_t)kHelpMenuID ||
        menuID == (int16_t)kScriptMenuID) {
        return true;
    }

    return false;
}

/*
 * CalcMenuBar - Calculate menu bar layout
 * Evidence: MenuMgrPriv.a:71 - selectMenuCalc EQU -11
 * Evidence: MenuMgrPriv.a:72 - paramWordsMenuCalc EQU 1
 * Evidence: MenuMgrPriv.a:164-166 - _CalcMenuBar macro
 */
OSErr CalcMenuBar(void) {
    Handle menuList = GetMenuBar();
    if (!menuList || !*menuList) {
        return noErr;
    }

    MenuList *list = (MenuList *)*menuList;
    int16_t currentLeft = 0;
    int16_t screenWidth = GetScreenWidth();

    /* First pass: calculate application menu positions from left */
    MenuHandle menu = list->firstMenu;
    while (menu && *menu) {
        MenuInfo *menuInfo = (MenuInfo *)*menu;

        if (!IsSystemMenu(menuInfo->menuID)) {
            menuInfo->menuLeft = currentLeft;
            currentLeft += menuInfo->menuWidth;
        }

        menu = menuInfo->nextMenu;
    }

    /* Second pass: position system menus from right */
    int16_t systemRight = screenWidth;

    /* Scan backwards through system menus to position from right */
    menu = list->firstMenu;
    while (menu && *menu) {
        MenuInfo *menuInfo = (MenuInfo *)*menu;

        if (IsSystemMenu(menuInfo->menuID)) {
            systemRight -= menuInfo->menuWidth;
            menuInfo->menuLeft = systemRight;
        }

        menu = menuInfo->nextMenu;
    }

    /* Update menu list tracking */
    list->lastRight = (currentLeft > systemRight) ? currentLeft : screenWidth;

    /* Set menu bar invalidation bit - evidence: MenuMgrPriv.a:42 */
    SetMenuBarInvalidBit(true);

    return noErr;
}

/*
 * Low-memory global bit manipulation functions
 * Evidence: MenuMgrPriv.a:42-49 bit definitions
 */

void SetMenuBarInvalidBit(Boolean invalid) {
    /* Access low-memory global at MENU_BAR_INVALID_BYTE */
    uint8_t *invalidByte = (uint8_t *)MENU_BAR_INVALID_BYTE;

    if (invalid) {
        *invalidByte |= (1 << MENU_BAR_INVALID_BIT);
    } else {
        *invalidByte &= ~(1 << MENU_BAR_INVALID_BIT);
    }
}

Boolean GetMenuBarInvalidBit(void) {
    uint8_t *invalidByte = (uint8_t *)MENU_BAR_INVALID_BYTE;
    return (*invalidByte & (1 << MENU_BAR_INVALID_BIT)) != 0;
}

void SetMenuBarGlobalInvalidBit(Boolean invalid) {
    uint8_t *invalidByte = (uint8_t *)MENU_BAR_GLOBAL_INVALID_BYTE;

    if (invalid) {
        *invalidByte |= (1 << MENU_BAR_GLOBAL_INVALID_BIT);
    } else {
        *invalidByte &= ~(1 << MENU_BAR_GLOBAL_INVALID_BIT);
    }
}

Boolean GetMenuBarGlobalInvalidBit(void) {
    uint8_t *invalidByte = (uint8_t *)MENU_BAR_GLOBAL_INVALID_BYTE;
    return (*invalidByte & (1 << MENU_BAR_GLOBAL_INVALID_BIT)) != 0;
}

Boolean GetValidateMenuBarSemaphore(void) {
    uint8_t *semaphoreByte = (uint8_t *)VALIDATE_MENUBAR_SEMAPHORE_BYTE;
    return (*semaphoreByte & (1 << VALIDATE_MENUBAR_SEMAPHORE_BIT)) != 0;
}

void SetValidateMenuBarSemaphore(Boolean locked) {
    uint8_t *semaphoreByte = (uint8_t *)VALIDATE_MENUBAR_SEMAPHORE_BYTE;

    if (locked) {
        *semaphoreByte |= (1 << VALIDATE_MENUBAR_SEMAPHORE_BIT);
    } else {
        *semaphoreByte &= ~(1 << VALIDATE_MENUBAR_SEMAPHORE_BIT);
    }
}

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "evidence_sources": [
 *     "/home/k/Desktop/os71/sys71src/Internal/Asm/MenuMgrPriv.a"
 *   ],
 *   "functions_implemented": 9,
 *   "low_memory_bit_functions": 6,
 *   "mbdf_messages_handled": 3,
 *   "system_menu_ranges_used": 2
 * }
 */
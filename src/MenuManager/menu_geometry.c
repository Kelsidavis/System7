/* #include "SystemTypes.h" */
/*
 * menu_geometry.c - Menu Manager Geometry Functions
 *
 * Implementation of Menu Manager geometry and layout functions based on
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 Menu Manager geometry
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "MenuManager/menu_private.h"
#include "menu_dispatch.h"


/* External dependencies */
extern Handle GetMenuBar(void);
extern SInt16 GetScreenWidth(void);
extern SInt16 GetScreenHeight(void);

static SInt16 g_MenuBarHeight = 20;

/*
 * GetMenuTitleRect - Get rectangle for menu title
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
 */
OSErr GetAppMenusRect(Rect *appRect) {
    if (!appRect) {
        return paramErr;
    }

    extern MenuBarList* MenuMgr_GetMenuBarList(void);
    MenuList *menuListPtr = (MenuList *)MenuMgr_GetMenuBarList();
    if (!menuListPtr) {
        /* No menus - empty rectangle */
        appRect->top = 0;
        appRect->left = 0;
        appRect->bottom = g_MenuBarHeight;
        appRect->right = 0;
        return noErr;
    }

    MenuList *list = menuListPtr;
    SInt16 leftmost = GetScreenWidth();
    SInt16 rightmost = 0;
    Boolean foundApp = false;

    /* Scan through menus to find application menu boundaries */
    MenuHandle menu = list->firstMenu;
    while (menu && *menu) {
        MenuInfo *menuInfo = (MenuInfo *)*menu;

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
 */
OSErr GetSysMenusRect(Rect *sysRect) {
    if (!sysRect) {
        return paramErr;
    }

    /* System menus are typically on the right side of menu bar */
    SInt16 screenWidth = GetScreenWidth();
    SInt16 sysMenuWidth = 0;
    SInt16 rightmost = screenWidth;

    /* Look for system menus and calculate their total width */
    extern MenuBarList* MenuMgr_GetMenuBarList(void);
    MenuList *menuListPtr = (MenuList *)MenuMgr_GetMenuBarList();
    if (menuListPtr) {
        MenuList *list = menuListPtr;
        MenuHandle menu = list->firstMenu;

        while (menu && *menu) {
            MenuInfo *menuInfo = (MenuInfo *)*menu;

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
 */
OSErr DrawMBARString(const unsigned char *text, SInt16 script, Rect *bounds, SInt16 just) {
    if (!text || !bounds) {
        return paramErr;
    }

    extern void MoveTo(SInt16 h, SInt16 v);
    extern void DrawString(const unsigned char *s);
    extern SInt16 StringWidth(const unsigned char *s);
    extern void TextFont(SInt16 font);
    extern void TextSize(SInt16 size);
    extern void TextFace(SInt16 face);

    /* Set menu bar font (Chicago 12) */
    TextFont(0);
    TextSize(12);
    TextFace(0);

    /* Calculate text width for justification */
    SInt16 textWidth = StringWidth(text);
    SInt16 boundsWidth = bounds->right - bounds->left;
    SInt16 h = bounds->left;

    /* Apply justification */
    switch (just) {
        case -1:  /* Left justify */
            h = bounds->left;
            break;
        case 0:   /* Center */
            h = bounds->left + (boundsWidth - textWidth) / 2;
            break;
        case 1:   /* Right justify */
            h = bounds->right - textWidth;
            break;
        default:
            h = bounds->left;
            break;
    }

    /* Calculate baseline (typically at bottom - 3 for menu bar text) */
    SInt16 v = bounds->bottom - 3;

    /* Draw the text */
    MoveTo(h, v);
    DrawString(text);

    /* Ignore script parameter for now - System 7 handles this internally */
    (void)script;

    return noErr;
}

/*
 * IsSystemMenu - Check if menu ID is in system range
 */
Boolean IsSystemMenu(SInt16 menuID) {
    if (menuID >= (SInt16)kLoSystemMenuRange && menuID <= (SInt16)kHiSystemMenuRange) {
        return true;
    }

    /* Check specific system menu IDs */
    if (menuID == (SInt16)kApplicationMenuID ||
        menuID == (SInt16)kHelpMenuID ||
        menuID == (SInt16)kScriptMenuID) {
        return true;
    }

    return false;
}

/*
 * CalcMenuBar - Calculate menu bar layout
 */
OSErr CalcMenuBar(void) {
    extern MenuBarList* MenuMgr_GetMenuBarList(void);
    MenuList *menuListPtr = (MenuList *)MenuMgr_GetMenuBarList();
    if (!menuListPtr) {
        return noErr;
    }

    MenuList *list = menuListPtr;
    SInt16 currentLeft = 0;
    SInt16 screenWidth = GetScreenWidth();

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
    SInt16 systemRight = screenWidth;

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

    SetMenuBarInvalidBit(true);

    return noErr;
}

/*
 * Low-memory global bit manipulation functions
 */

void SetMenuBarInvalidBit(Boolean invalid) {
    /* Access low-memory global at MENU_BAR_INVALID_BYTE */
    UInt8 *invalidByte = (UInt8 *)MENU_BAR_INVALID_BYTE;

    if (invalid) {
        *invalidByte |= (1 << MENU_BAR_INVALID_BIT);
    } else {
        *invalidByte &= ~(1 << MENU_BAR_INVALID_BIT);
    }
}

Boolean GetMenuBarInvalidBit(void) {
    UInt8 *invalidByte = (UInt8 *)MENU_BAR_INVALID_BYTE;
    return (*invalidByte & (1 << MENU_BAR_INVALID_BIT)) != 0;
}

void SetMenuBarGlobalInvalidBit(Boolean invalid) {
    UInt8 *invalidByte = (UInt8 *)MENU_BAR_GLOBAL_INVALID_BYTE;

    if (invalid) {
        *invalidByte |= (1 << MENU_BAR_GLOBAL_INVALID_BIT);
    } else {
        *invalidByte &= ~(1 << MENU_BAR_GLOBAL_INVALID_BIT);
    }
}

Boolean GetMenuBarGlobalInvalidBit(void) {
    UInt8 *invalidByte = (UInt8 *)MENU_BAR_GLOBAL_INVALID_BYTE;
    return (*invalidByte & (1 << MENU_BAR_GLOBAL_INVALID_BIT)) != 0;
}

Boolean GetValidateMenuBarSemaphore(void) {
    UInt8 *semaphoreByte = (UInt8 *)VALIDATE_MENUBAR_SEMAPHORE_BYTE;
    return (*semaphoreByte & (1 << VALIDATE_MENUBAR_SEMAPHORE_BIT)) != 0;
}

void SetValidateMenuBarSemaphore(Boolean locked) {
    UInt8 *semaphoreByte = (UInt8 *)VALIDATE_MENUBAR_SEMAPHORE_BYTE;

    if (locked) {
        *semaphoreByte |= (1 << VALIDATE_MENUBAR_SEMAPHORE_BIT);
    } else {
        *semaphoreByte &= ~(1 << VALIDATE_MENUBAR_SEMAPHORE_BIT);
    }
}

/* */

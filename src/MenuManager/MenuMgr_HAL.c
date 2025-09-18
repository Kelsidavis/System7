/*
 * Menu Manager Hardware Abstraction Layer
 * Bridges classic Mac OS Menu Manager to modern platforms
 * Integrates with Memory, Resource, Window, and QuickDraw Managers
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include "MenuManager/menu_dispatch.h"
#include "MenuManager/menu_private.h"
#include "MemoryMgr/memory_manager.h"
#include "ResourceMgr/resource_manager.h"
#include "WindowManager/window_manager.h"
#include "QuickDraw/QuickDraw.h"

/* Menu Manager globals */
static struct {
    Handle          menuList;           /* Global menu list handle */
    MenuHandle      *menuBar;          /* Array of menu handles */
    int16_t         menuCount;          /* Number of menus in bar */
    int16_t         lastMenuID;         /* Last selected menu ID */
    int16_t         lastMenuItem;       /* Last selected item */
    int16_t         hiliteMenuID;       /* Currently highlighted menu */
    RgnHandle       menuBarRgn;        /* Menu bar region */
    Handle          menuHook;          /* Menu hook procedure */
    Handle          mbSaveLoc;         /* Saved bits behind menu */
    int16_t         theMenu;           /* Current menu for MenuSelect */
    pthread_mutex_t menuLock;          /* Thread safety */
    bool            initialized;

    /* Platform-specific */
#ifdef __linux__
    Display*        display;
    Window          menuWindow;
    GC              menuGC;
#endif

    /* Screen optimization */
    BitMap*         savedBits;         /* Saved screen bits */
    Rect            savedRect;         /* Rectangle of saved area */
    bool            bitsValid;         /* Whether saved bits are valid */
} gMenuMgr = {0};

/* Menu bar constants */
#define kMenuBarHeight      20
#define kMenuTitlePadding   12
#define kMenuItemHeight     19
#define kMenuItemPadding    14
#define kMenuSeparatorHeight 9

/* Forward declarations */
static OSErr MenuMgr_HAL_DrawMenuBar(void);
static void MenuMgr_HAL_SaveBehindMenu(const Rect* menuRect);
static void MenuMgr_HAL_RestoreBehindMenu(void);
static void MenuMgr_HAL_CalculateMenuGeometry(MenuHandle menu, Rect* bounds);

/* Initialize Menu Manager HAL */
OSErr MenuMgr_HAL_Initialize(void)
{
    if (gMenuMgr.initialized) {
        return noErr;
    }

    /* Initialize mutex */
    pthread_mutex_init(&gMenuMgr.menuLock, NULL);

    /* Create menu bar region using Memory Manager */
    gMenuMgr.menuBarRgn = NewRgn();
    if (!gMenuMgr.menuBarRgn) {
        return memFullErr;
    }

    /* Set up menu bar region */
    Rect menuBarBounds = {0, 0, kMenuBarHeight, 1024};
    RectRgn(gMenuMgr.menuBarRgn, &menuBarBounds);

    /* Allocate menu list handle */
    gMenuMgr.menuList = NewHandle(0);
    if (!gMenuMgr.menuList) {
        return memFullErr;
    }

    /* Allocate menu bar array */
    gMenuMgr.menuBar = (MenuHandle*)NewPtr(sizeof(MenuHandle) * 32);
    if (!gMenuMgr.menuBar) {
        return memFullErr;
    }

#ifdef __linux__
    /* Initialize X11 for menu display */
    gMenuMgr.display = XOpenDisplay(NULL);
    if (gMenuMgr.display) {
        int screen = DefaultScreen(gMenuMgr.display);
        Window rootWindow = RootWindow(gMenuMgr.display, screen);

        /* Create menu window */
        XSetWindowAttributes attrs;
        attrs.background_pixel = WhitePixel(gMenuMgr.display, screen);
        attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask;

        gMenuMgr.menuWindow = XCreateWindow(gMenuMgr.display, rootWindow,
                                           0, 0, 1024, kMenuBarHeight,
                                           0, CopyFromParent, InputOutput,
                                           CopyFromParent,
                                           CWBackPixel | CWEventMask, &attrs);

        gMenuMgr.menuGC = XCreateGC(gMenuMgr.display, gMenuMgr.menuWindow, 0, NULL);
        XMapWindow(gMenuMgr.display, gMenuMgr.menuWindow);
    }
#endif

    gMenuMgr.initialized = true;
    return noErr;
}

/* Terminate Menu Manager HAL */
void MenuMgr_HAL_Terminate(void)
{
    if (!gMenuMgr.initialized) {
        return;
    }

    /* Dispose all menus */
    for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
        if (gMenuMgr.menuBar[i]) {
            DisposeMenu(gMenuMgr.menuBar[i]);
        }
    }

    /* Dispose regions using Memory Manager */
    if (gMenuMgr.menuBarRgn) {
        DisposeRgn(gMenuMgr.menuBarRgn);
    }

    /* Dispose handles */
    if (gMenuMgr.menuList) {
        DisposeHandle(gMenuMgr.menuList);
    }

    /* Dispose menu bar array */
    if (gMenuMgr.menuBar) {
        DisposePtr((Ptr)gMenuMgr.menuBar);
    }

    /* Dispose saved bits if any */
    if (gMenuMgr.savedBits) {
        DisposePtr((Ptr)gMenuMgr.savedBits);
    }

#ifdef __linux__
    if (gMenuMgr.display) {
        if (gMenuMgr.menuGC) {
            XFreeGC(gMenuMgr.display, gMenuMgr.menuGC);
        }
        if (gMenuMgr.menuWindow) {
            XDestroyWindow(gMenuMgr.display, gMenuMgr.menuWindow);
        }
        XCloseDisplay(gMenuMgr.display);
    }
#endif

    pthread_mutex_destroy(&gMenuMgr.menuLock);
    gMenuMgr.initialized = false;
}

/* Create new menu - integrates with Memory Manager */
MenuHandle MenuMgr_HAL_NewMenu(int16_t menuID, ConstStr255Param menuTitle)
{
    MenuHandle menu;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Allocate menu handle using Memory Manager */
    menu = (MenuHandle)NewHandle(sizeof(MenuInfo));
    if (!menu) {
        pthread_mutex_unlock(&gMenuMgr.menuLock);
        return NULL;
    }

    /* Initialize menu info */
    (*menu)->menuID = menuID;
    (*menu)->menuWidth = 0;
    (*menu)->menuHeight = 0;
    (*menu)->menuProc = 0;  /* Standard MDEF */
    (*menu)->enableFlags = 0xFFFFFFFF;  /* All items enabled */

    /* Copy menu title */
    if (menuTitle && menuTitle[0] > 0) {
        BlockMoveData(menuTitle, (*menu)->menuData, menuTitle[0] + 1);
    } else {
        (*menu)->menuData[0] = 0;
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);
    return menu;
}

/* Get menu from resource - integrates with Resource Manager */
MenuHandle MenuMgr_HAL_GetMenu(int16_t resourceID)
{
    Handle menuResource;
    MenuHandle menu = NULL;

    /* Load MENU resource using Resource Manager */
    menuResource = GetResource('MENU', resourceID);
    if (!menuResource || !*menuResource) {
        return NULL;
    }

    /* Parse MENU resource structure */
    struct MENUResource {
        int16_t menuID;
        uint16_t placeholder1;
        uint16_t placeholder2;
        uint32_t enableFlags;
        Str255 title;
        /* Items follow */
    } *menuData;

    HLock(menuResource);
    menuData = (struct MENUResource*)*menuResource;

    /* Create menu from resource data */
    menu = MenuMgr_HAL_NewMenu(menuData->menuID, menuData->title);
    if (menu) {
        (*menu)->enableFlags = menuData->enableFlags;

        /* Parse menu items */
        uint8_t* itemPtr = (uint8_t*)&menuData->title[menuData->title[0] + 1];
        while (*itemPtr != 0) {
            /* Each item is a Pascal string followed by metadata */
            uint8_t itemLen = *itemPtr;

            /* Add item to menu */
            AppendMenu(menu, (ConstStr255Param)itemPtr);

            /* Skip to next item */
            itemPtr += itemLen + 1;
            itemPtr += 4;  /* Skip item metadata (cmd, mark, style) */
        }
    }

    HUnlock(menuResource);
    ReleaseResource(menuResource);

    return menu;
}

/* Dispose menu */
void MenuMgr_HAL_DisposeMenu(MenuHandle menu)
{
    if (!menu) return;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Remove from menu bar if present */
    for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
        if (gMenuMgr.menuBar[i] == menu) {
            /* Shift remaining menus */
            for (int16_t j = i; j < gMenuMgr.menuCount - 1; j++) {
                gMenuMgr.menuBar[j] = gMenuMgr.menuBar[j + 1];
            }
            gMenuMgr.menuCount--;
            break;
        }
    }

    /* Dispose menu handle using Memory Manager */
    DisposeHandle((Handle)menu);

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Append menu item */
void MenuMgr_HAL_AppendMenu(MenuHandle menu, ConstStr255Param data)
{
    if (!menu || !data) return;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Grow menu data handle */
    Size oldSize = GetHandleSize((Handle)menu);
    Size dataLen = data[0] + 1 + 4;  /* String + metadata */
    SetHandleSize((Handle)menu, oldSize + dataLen);

    /* Append item data */
    uint8_t* menuData = (uint8_t*)*menu;
    uint8_t* itemPtr = menuData + oldSize;

    /* Copy item string */
    BlockMoveData(data, itemPtr, data[0] + 1);
    itemPtr += data[0] + 1;

    /* Set default metadata */
    *itemPtr++ = 0;    /* No command key */
    *itemPtr++ = 0;    /* No mark */
    *itemPtr++ = 0;    /* Plain style */
    *itemPtr++ = 0;    /* Reserved */

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Insert menu into menu bar */
void MenuMgr_HAL_InsertMenu(MenuHandle menu, int16_t beforeID)
{
    if (!menu) return;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    if (beforeID == 0 || beforeID == -1) {
        /* Add at end */
        if (gMenuMgr.menuCount < 32) {
            gMenuMgr.menuBar[gMenuMgr.menuCount++] = menu;
        }
    } else {
        /* Insert before specified menu */
        int16_t insertPos = gMenuMgr.menuCount;

        /* Find insertion position */
        for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
            if ((*gMenuMgr.menuBar[i])->menuID == beforeID) {
                insertPos = i;
                break;
            }
        }

        /* Shift menus and insert */
        if (insertPos < 32) {
            for (int16_t i = gMenuMgr.menuCount; i > insertPos; i--) {
                gMenuMgr.menuBar[i] = gMenuMgr.menuBar[i - 1];
            }
            gMenuMgr.menuBar[insertPos] = menu;
            if (gMenuMgr.menuCount < 32) {
                gMenuMgr.menuCount++;
            }
        }
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Delete menu from menu bar */
void MenuMgr_HAL_DeleteMenu(int16_t menuID)
{
    pthread_mutex_lock(&gMenuMgr.menuLock);

    for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
        if ((*gMenuMgr.menuBar[i])->menuID == menuID) {
            /* Don't dispose, just remove from bar */
            for (int16_t j = i; j < gMenuMgr.menuCount - 1; j++) {
                gMenuMgr.menuBar[j] = gMenuMgr.menuBar[j + 1];
            }
            gMenuMgr.menuCount--;
            break;
        }
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Draw menu bar */
void MenuMgr_HAL_DrawMenuBar(void)
{
    GrafPtr savePort;
    Rect menuBarRect = {0, 0, kMenuBarHeight, 1024};
    int16_t titleOffset = 10;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Save current port */
    GetPort(&savePort);

    /* Clear menu bar area */
    EraseRect(&menuBarRect);

    /* Draw menu bar line */
    MoveTo(0, kMenuBarHeight - 1);
    LineTo(1024, kMenuBarHeight - 1);

    /* Draw each menu title */
    for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
        MenuHandle menu = gMenuMgr.menuBar[i];
        Str255 title;

        /* Get menu title */
        BlockMoveData((*menu)->menuData, title, (*menu)->menuData[0] + 1);

        /* Draw title */
        MoveTo(titleOffset, 14);
        DrawString(title);

        /* Calculate next position */
        titleOffset += StringWidth(title) + kMenuTitlePadding;
    }

#ifdef __linux__
    /* Update X11 display */
    if (gMenuMgr.display && gMenuMgr.menuWindow) {
        XFlush(gMenuMgr.display);
    }
#endif

    /* Restore port */
    SetPort(savePort);

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Invalidate menu bar */
void MenuMgr_HAL_InvalMenuBar(void)
{
    /* Mark menu bar region for redraw */
    InvalRgn(gMenuMgr.menuBarRgn);

    /* Redraw immediately */
    MenuMgr_HAL_DrawMenuBar();
}

/* Menu selection */
int32_t MenuMgr_HAL_MenuSelect(Point startPt)
{
    int32_t result = 0;
    int16_t menuID = 0;
    int16_t menuItem = 0;
    Rect menuRect;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Check if click is in menu bar */
    if (startPt.v < kMenuBarHeight) {
        /* Find which menu was clicked */
        int16_t titleOffset = 10;

        for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
            MenuHandle menu = gMenuMgr.menuBar[i];
            Str255 title;
            BlockMoveData((*menu)->menuData, title, (*menu)->menuData[0] + 1);

            int16_t titleWidth = StringWidth(title) + kMenuTitlePadding;

            if (startPt.h >= titleOffset && startPt.h < titleOffset + titleWidth) {
                menuID = (*menu)->menuID;

                /* Calculate menu bounds */
                MenuMgr_HAL_CalculateMenuGeometry(menu, &menuRect);
                menuRect.left = titleOffset;
                menuRect.top = kMenuBarHeight;

                /* Save bits behind menu */
                MenuMgr_HAL_SaveBehindMenu(&menuRect);

                /* Draw the menu */
                /* This would involve drawing the menu items */

                /* Track mouse for selection */
                /* This would track until mouse up */

                menuItem = 1;  /* Placeholder - would track actual selection */

                /* Restore bits behind menu */
                MenuMgr_HAL_RestoreBehindMenu();

                break;
            }

            titleOffset += titleWidth;
        }
    }

    /* Pack result */
    result = ((int32_t)menuID << 16) | menuItem;

    gMenuMgr.lastMenuID = menuID;
    gMenuMgr.lastMenuItem = menuItem;

    pthread_mutex_unlock(&gMenuMgr.menuLock);

    return result;
}

/* Menu key equivalent */
int32_t MenuMgr_HAL_MenuKey(int16_t ch)
{
    int32_t result = 0;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Search all menus for command key */
    for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
        MenuHandle menu = gMenuMgr.menuBar[i];

        /* Search menu items for command key match */
        /* This would parse menu items and check command keys */

        /* Placeholder - would return actual match */
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);

    return result;
}

/* Hilite menu */
void MenuMgr_HAL_HiliteMenu(int16_t menuID)
{
    pthread_mutex_lock(&gMenuMgr.menuLock);

    /* Unhilite previous menu if any */
    if (gMenuMgr.hiliteMenuID != 0) {
        /* Redraw previous menu title normally */
    }

    /* Hilite new menu */
    gMenuMgr.hiliteMenuID = menuID;

    if (menuID != 0) {
        /* Find menu and hilite its title */
        for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
            if ((*gMenuMgr.menuBar[i])->menuID == menuID) {
                /* Draw menu title in hilite mode */
                break;
            }
        }
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Flash menu bar */
void MenuMgr_HAL_FlashMenuBar(int16_t menuID)
{
    /* Flash menu bar or specific menu */
    if (menuID == 0) {
        /* Flash entire menu bar */
        InvertRect(&(Rect){0, 0, kMenuBarHeight, 1024});
        /* Delay */
        InvertRect(&(Rect){0, 0, kMenuBarHeight, 1024});
    } else {
        /* Flash specific menu title */
        /* Would find menu and invert its title area */
    }
}

/* Enable/disable menu items */
void MenuMgr_HAL_EnableItem(MenuHandle menu, int16_t item)
{
    if (!menu) return;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    if (item == 0) {
        /* Enable entire menu */
        (*menu)->enableFlags = 0xFFFFFFFF;
    } else if (item > 0 && item <= 31) {
        /* Enable specific item */
        (*menu)->enableFlags |= (1L << item);
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

void MenuMgr_HAL_DisableItem(MenuHandle menu, int16_t item)
{
    if (!menu) return;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    if (item == 0) {
        /* Disable entire menu */
        (*menu)->enableFlags = 0x00000001;  /* Keep menu enabled, items disabled */
    } else if (item > 0 && item <= 31) {
        /* Disable specific item */
        (*menu)->enableFlags &= ~(1L << item);
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);
}

/* Check/uncheck menu items */
void MenuMgr_HAL_CheckItem(MenuHandle menu, int16_t item, Boolean checked)
{
    if (!menu || item <= 0) return;

    /* Would modify item mark character */
    /* Items have metadata after text: cmd, mark, style */
}

/* Set menu item text */
void MenuMgr_HAL_SetItem(MenuHandle menu, int16_t item, ConstStr255Param itemString)
{
    if (!menu || item <= 0 || !itemString) return;

    /* Would locate item in menu data and replace text */
}

/* Get menu item text */
void MenuMgr_HAL_GetItem(MenuHandle menu, int16_t item, Str255 itemString)
{
    if (!menu || item <= 0 || !itemString) return;

    /* Would locate item in menu data and copy text */
    itemString[0] = 0;  /* Empty for now */
}

/* Get menu handle by ID */
MenuHandle MenuMgr_HAL_GetMHandle(int16_t menuID)
{
    MenuHandle result = NULL;

    pthread_mutex_lock(&gMenuMgr.menuLock);

    for (int16_t i = 0; i < gMenuMgr.menuCount; i++) {
        if ((*gMenuMgr.menuBar[i])->menuID == menuID) {
            result = gMenuMgr.menuBar[i];
            break;
        }
    }

    pthread_mutex_unlock(&gMenuMgr.menuLock);

    return result;
}

/* Count menu items */
int16_t MenuMgr_HAL_CountMItems(MenuHandle menu)
{
    int16_t count = 0;

    if (!menu) return 0;

    /* Would parse menu data and count items */
    /* Each item is a Pascal string followed by metadata */

    return count;
}

/* Calculate menu geometry */
static void MenuMgr_HAL_CalculateMenuGeometry(MenuHandle menu, Rect* bounds)
{
    int16_t maxWidth = 0;
    int16_t height = 4;  /* Top margin */

    /* Calculate from menu items */
    /* Each item adds kMenuItemHeight or kMenuSeparatorHeight */
    /* Width is max of all item widths plus padding */

    /* Placeholder dimensions */
    bounds->left = 0;
    bounds->top = 0;
    bounds->right = 200;
    bounds->bottom = 100;
}

/* Save bits behind menu - screen optimization */
static void MenuMgr_HAL_SaveBehindMenu(const Rect* menuRect)
{
    Size bitsSize;

    /* Calculate size needed */
    int16_t width = menuRect->right - menuRect->left;
    int16_t height = menuRect->bottom - menuRect->top;
    bitsSize = ((width + 15) / 16) * 2 * height;  /* Round to word boundary */

    /* Allocate buffer if needed */
    if (!gMenuMgr.savedBits || gMenuMgr.savedBits->baseAddr == NULL) {
        gMenuMgr.savedBits = (BitMap*)NewPtr(sizeof(BitMap));
        if (gMenuMgr.savedBits) {
            gMenuMgr.savedBits->baseAddr = NewPtr(bitsSize);
            gMenuMgr.savedBits->rowBytes = ((width + 15) / 16) * 2;
            gMenuMgr.savedBits->bounds = *menuRect;
        }
    }

    /* Copy screen bits */
    if (gMenuMgr.savedBits && gMenuMgr.savedBits->baseAddr) {
        /* Would use CopyBits to save screen content */
        gMenuMgr.savedRect = *menuRect;
        gMenuMgr.bitsValid = true;
    }
}

/* Restore bits behind menu */
static void MenuMgr_HAL_RestoreBehindMenu(void)
{
    if (gMenuMgr.bitsValid && gMenuMgr.savedBits && gMenuMgr.savedBits->baseAddr) {
        /* Would use CopyBits to restore screen content */
        gMenuMgr.bitsValid = false;
    }
}

/* Dispatch mechanism implementation */
OSErr MenuMgr_HAL_Dispatch(int16_t selector, void* params)
{
    OSErr result = noErr;

    switch (selector) {
        case 0:  /* InsertFontResMenu */
            {
                InsertFontResMenuParams* p = (InsertFontResMenuParams*)params;
                /* Would insert font menu items */
            }
            break;

        case 1:  /* InsertIntlResMenu */
            {
                InsertIntlResMenuParams* p = (InsertIntlResMenuParams*)params;
                /* Would insert international menu items */
            }
            break;

        case -1: /* GetMenuTitleRect */
            {
                GetMenuTitleRectParams* p = (GetMenuTitleRectParams*)params;
                /* Calculate title rectangle */
                if (p && p->titleRect) {
                    p->titleRect->left = 0;
                    p->titleRect->top = 0;
                    p->titleRect->right = 100;
                    p->titleRect->bottom = kMenuBarHeight;
                }
            }
            break;

        case -2: /* GetMBARRect */
            {
                GetMBARRectParams* p = (GetMBARRectParams*)params;
                if (p && p->mbarRect) {
                    p->mbarRect->left = 0;
                    p->mbarRect->top = 0;
                    p->mbarRect->right = 1024;
                    p->mbarRect->bottom = kMenuBarHeight;
                }
            }
            break;

        case -3: /* GetAppMenusRect */
            {
                /* Would return application menu area */
            }
            break;

        default:
            result = paramErr;
            break;
    }

    return result;
}

/* Initialize on first use */
__attribute__((constructor))
static void MenuMgr_HAL_Init(void)
{
    MenuMgr_HAL_Initialize();
}

/* Cleanup on exit */
__attribute__((destructor))
static void MenuMgr_HAL_Cleanup(void)
{
    MenuMgr_HAL_Terminate();
}
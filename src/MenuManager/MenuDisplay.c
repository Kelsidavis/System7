/* #include "SystemTypes.h" */
#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/menu_private.h"
#include "MemoryMgr/MemoryManager.h"
#include <string.h>
/*
 * MenuDisplay.c - Menu Drawing and Visual Management
 *
 * This file implements all menu display functionality including menu bar
 * rendering, pull-down menu display, menu item drawing, and visual effects.
 * It provides the complete visual representation of the Mac OS menu system.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDrawConstants.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuInternalTypes.h"
#include "MenuManager/MenuDisplay.h"
#include "FontManager/FontManager.h"
#include "MenuManager/MenuLogging.h"
#include "FontManager/FontTypes.h"
#include "FontManager/FontInternal.h"

#include <math.h>


/* Serial output */

/* Menu item standard height */
#define menuItemStdHeight 16

/* Menu item flags */
#define kMenuItemChecked    0x0001
#define kMenuItemHasCmdKey  0x0002
#define kMenuItemHasIcon    0x0004
#define kMenuItemIsSeparator 0x0008
#define kMenuItemDisabled   0x0010
#define kMenuItemSelected   0x0020
#define kMenuDrawNormal     0x0000
#define kMenuItemNormal     0x0000


/* ============================================================================
 * Display State and Context
 * ============================================================================ */

static MenuDrawContext gDrawingContext;
static Boolean gColorMode = false;
static Boolean gAntiAlias = true;
static Handle gCurrentSavedBits = NULL;
static MenuHandle gCurrentlyShownMenu = NULL;
static Rect gCurrentMenuRect;
extern QDGlobals qd;

/* Platform function prototypes declared in menu_private.h */

/* Forward declarations */
Handle SaveMenuBits_Display(const Rect* menuRect);
void RestoreMenuBits_Display(Handle savedBits, const Rect* menuRect);
extern void Platform_FlashMenuBar(short menuID);

/* Internal function prototypes */
static void InitializeDrawingContext(MenuDrawContext* context);
static void SetupMenuDrawingColors(short menuID, short itemID);
static void DrawMenuFrameInternal(const Rect* menuRect, Boolean selected);
static void DrawMenuBackgroundInternal(const Rect* menuRect, short menuID);
static void DrawMenuItemTextInternal(const Rect* itemRect, ConstStr255Param itemText,
                                   short textStyle, Boolean enabled, Boolean selected,
                                   Boolean isMenuTitle);
static void DrawMenuItemIconInternal(const Rect* iconRect, short iconID,
                                   Boolean enabled, Boolean selected);
static void DrawMenuItemMarkInternal(const Rect* markRect, unsigned char markChar,
                                   Boolean enabled, Boolean selected);
static void DrawMenuItemCmdKeyInternal(const Rect* cmdRect, unsigned char cmdChar,
                                     Boolean enabled, Boolean selected);
static void CalcMenuItemRects(MenuHandle theMenu, short item, const Rect* menuRect,
                            Rect* textRect, Rect* iconRect, Rect* markRect, Rect* cmdRect);
static short MeasureMenuItemWidth(MenuHandle theMenu, short item);
static short GetMenuItemTextWidth(ConstStr255Param text, Style textStyle);

/* ============================================================================
 * Menu Bar Display Functions
 * ============================================================================ */

/*
 * DrawMenuBarEx - Extended menu bar drawing
 */
void DrawMenuBarEx(const MenuBarDrawInfo* drawInfo)
{
    if (drawInfo == NULL) {
        return;
    }

    /* Set up drawing context */
    InitializeDrawingContext(&gDrawingContext);

    /* Use platform-specific drawing if available */
    Platform_DrawMenuBar(drawInfo);

    /* MENU_LOG_TRACE("Drawing menu bar (mode: %d, hilite: %d)\n",
           drawInfo->drawMode, drawInfo->hiliteMenu); */
}

/*
 * EraseMenuBar - Erase menu bar area
 */
void EraseMenuBar(const Rect* menuBarRect)
{
    Rect eraseRect;

    if (menuBarRect != NULL) {
        eraseRect = *menuBarRect;
    } else {
        /* Use standard menu bar rectangle */
        eraseRect.left = 0;
        eraseRect.top = 0;
        eraseRect.right = 640; /* Default screen width */
        eraseRect.bottom = 20; /* Standard menu bar height */
    }

    /* Fill with background pattern */
    /* MENU_LOG_TRACE("Erasing menu bar rect (%d,%d,%d,%d)\n",
           eraseRect.left, eraseRect.top, eraseRect.right, eraseRect.bottom); */

    FillRect(&eraseRect, &qd.white);
}

/*
 * HiliteMenuTitle - Highlight menu title
 */
void HiliteMenuTitle(short menuID, Boolean hilite)
{
    Rect titleRect;

    extern void serial_puts(const char* str);
    serial_puts("HiliteMenuTitle ENTRY\n");

    /* Get menu title rectangle using the tracking system */
    extern Boolean GetMenuTitleRectByID(short menuID, Rect* outRect);
    Boolean gotRect = GetMenuTitleRectByID(menuID, &titleRect);

    if (gotRect) {
        serial_puts("HiliteMenuTitle: GetMenuTitleRectByID succeeded, calling DrawMenuTitle\n");
        /* Draw highlighted or normal title */
        DrawMenuTitle(menuID, &titleRect, hilite);
        serial_puts("HiliteMenuTitle: DrawMenuTitle returned\n");
    } else {
        serial_puts("HiliteMenuTitle: GetMenuTitleRectByID FAILED\n");
    }
}

/*
 * DrawMenuTitle - Draw individual menu title
 */
void DrawMenuTitle(short menuID, const Rect* titleRect, Boolean hilited)
{
    MenuHandle theMenu;
    unsigned char titleText[256];
    Rect textRect;
    GrafPtr savePort;

    if (titleRect == NULL) {
        return;
    }

    theMenu = GetMenuHandle(menuID);
    if (theMenu == NULL) {
        return;
    }

    /* CRITICAL: Menu bar titles must be drawn in WMgrPort (screen port)
     * titleRect is in screen/global coordinates, so we need WMgrPort which has
     * portBits.bounds=(0,0,width,height) to avoid coordinate offset issues */
    GetPort(&savePort);
    extern QDGlobals qd;
    if (qd.thePort) {
        SetPort(qd.thePort);  /* WMgrPort */
    }

    /* CRITICAL FIX: Reset pnLoc before drawing
     * pnLoc may be left in a bad state (e.g., 321,146) from previous dropdown menu
     * drawing or other operations. This causes text to be drawn at wrong position.
     * Reset to (0,0) to ensure DrawChar positioning is correct. */
    if (qd.thePort) {
        qd.thePort->pnLoc.h = 0;
        qd.thePort->pnLoc.v = 0;
    }

    /* Get menu title */
    short titleLen = (*(MenuInfo**)theMenu)->menuData[0];
    if (titleLen > 255) titleLen = 255;
    titleText[0] = titleLen;
    memcpy(&titleText[1], &(*(MenuInfo**)theMenu)->menuData[1], titleLen);

    /* Use titleRect directly - DrawMenuItemTextInternal will add its own padding */
    textRect = *titleRect;

    extern void serial_puts(const char* str);
    if (hilited) {
        serial_puts("[DRAWTITLE] HIGHLIGHTED menu title\n");
    } else {
        serial_puts("[DRAWTITLE] NORMAL menu title\n");
    }

    /* Debug: Log coordinates being used */
    static char debugBuf[256];
    extern int snprintf(char*, size_t, const char*, ...);
    extern GrafPtr g_currentPort;
    snprintf(debugBuf, sizeof(debugBuf), "[DRAWTITLE] titleRect=(%d,%d,%d,%d) bounds=(%d,%d,%d,%d) width=%d\n",
             titleRect->left, titleRect->top, titleRect->right, titleRect->bottom,
             g_currentPort->portBits.bounds.left, g_currentPort->portBits.bounds.top,
             g_currentPort->portBits.bounds.right, g_currentPort->portBits.bounds.bottom,
             titleRect->right - titleRect->left);
    serial_puts(debugBuf);

    /* CRITICAL: Always erase the title rect first to remove any old text
     * This prevents InvertRect from inverting old text, which would create
     * a "ghost" effect where inverted old text appears offset from new text.
     *
     * Use FillRect with white to erase, which goes through QuickDraw's
     * coordinate system and respects the port's clipping region. */
    extern void serial_puts(const char* str);
    serial_puts("[DRAW-TITLE] About to call FillRect\n");
    FillRect(titleRect, &qd.white);
    serial_puts("[DRAW-TITLE] FillRect returned\n");

    /* Set drawing colors based on hilite state */
    if (hilited) {
        /* Highlighted state - invert the ORIGINAL titleRect (not expanded)
         * We already erased the expanded region above to prevent ghost pixels,
         * but InvertRect must use the original titleRect to ensure text is
         * positioned correctly within the inverted area */
        serial_puts("[DRAW-TITLE] About to call InvertRect\n");
        InvertRect(titleRect);
        serial_puts("[DRAW-TITLE] InvertRect returned\n");

        MENU_LOG_TRACE("Drew highlighted menu title: %.*s\n", titleLen, &titleText[1]);
    } else {
        /* Normal state - already erased above */
        MENU_LOG_TRACE("Drew normal menu title: %.*s\n", titleLen, &titleText[1]);
    }

    /* Draw the title text */
    extern void serial_puts(const char* str);
    extern GrafPtr g_currentPort;
    static char pnLocBuf[256];
    extern int snprintf(char*, size_t, const char*, ...);

    snprintf(pnLocBuf, sizeof(pnLocBuf), "[MENU-PNLOC-BEFORE] pnLoc=(%d,%d) titleRect.left=%d\n",
             g_currentPort->pnLoc.h, g_currentPort->pnLoc.v, titleRect->left);
    serial_puts(pnLocBuf);

    if (hilited) {
        serial_puts("[MENU] Drawing HIGHLIGHTED menu title, calling DrawMenuItemTextInternal\n");
    } else {
        serial_puts("[MENU] Drawing NORMAL menu title, calling DrawMenuItemTextInternal\n");
    }
    DrawMenuItemTextInternal(&textRect, titleText, normal, true, hilited, true);  /* true = isMenuTitle */

    snprintf(pnLocBuf, sizeof(pnLocBuf), "[MENU-PNLOC-AFTER] pnLoc=(%d,%d)\n",
             g_currentPort->pnLoc.h, g_currentPort->pnLoc.v);
    serial_puts(pnLocBuf);
    serial_puts("[MENU] DrawMenuItemTextInternal returned\n");

    /* Check what InvertRect actually did */
    if (hilited) {
        extern void QD_GetLastInvertRect(short* left, short* right);
        short invLeft, invRight;
        QD_GetLastInvertRect(&invLeft, &invRight);
        static char invBuf[256];
        snprintf(invBuf, sizeof(invBuf), "[MENU-INVERT-ACTUAL] left=%d right=%d titleRect.left=%d width=%d\n",
                 invLeft, invRight, titleRect->left, titleRect->right - titleRect->left);
        serial_puts(invBuf);
    }

    /* Restore original port */
    if (savePort) {
        SetPort(savePort);
    }
}

/*
 * CalcMenuBarLayout - Calculate menu bar layout
 */
short CalcMenuBarLayout(Handle menuList, const Rect* menuBarRect,
                       Point positions[], short widths[])
{
    MenuBarList* menuBar;
    short currentLeft = 0;
    short menuCount = 0;

    if (menuList == NULL || menuBarRect == NULL) {
        return 0;
    }

    menuBar = (MenuBarList*)menuList;
    if (menuBar->numMenus == 0) {
        return 0;
    }

    /* Calculate position for each menu */
    for (int i = 0; i < menuBar->numMenus && i < 32; i++) {
        MenuHandle theMenu = GetMenuHandle(menuBar->menus[i].menuID);
        short menuWidth = 80; /* Default width */

        if (theMenu != NULL) {
            menuWidth = GetMenuTitleWidth(theMenu);
        }

        if (positions != NULL) {
            positions[i].h = currentLeft;
            positions[i].v = menuBarRect->top;
        }

        if (widths != NULL) {
            widths[i] = menuWidth;
        }

        currentLeft += menuWidth;
        menuCount++;
    }

    return menuCount;
}

/* ============================================================================
 * Pull-Down Menu Display Functions
 * ============================================================================ */

/*
 * ShowMenu - Display a pull-down menu
 */
void ShowMenu(MenuHandle theMenu, Point location, const MenuDrawInfo* drawInfo)
{
    Rect menuRect;

    if (theMenu == NULL) {
        return;
    }

    /* Calculate menu rectangle */
    CalcMenuRect(theMenu, location, &menuRect);

    /* CRITICAL: Hide any currently shown menu BEFORE showing new one
     * Otherwise old menu stays visible when switching menus */
    if (gCurrentlyShownMenu != NULL) {
        HideMenu();
    }

    /* Save screen bits under menu */
    gCurrentSavedBits = SaveMenuBits_Display(&menuRect);

    /* Draw the menu */
    DrawMenu(theMenu, &menuRect, 0);

    /* Remember current menu */
    gCurrentlyShownMenu = theMenu;
    gCurrentMenuRect = menuRect;

    /* MENU_LOG_TRACE("Showing menu ID %d at (%d,%d)\n",
           (*(MenuInfo**)theMenu)->menuID, location.h, location.v); */
}

/*
 * HideMenu - Hide currently displayed menu
 */
void HideMenu(void)
{
    if (gCurrentlyShownMenu == NULL) {
        return;
    }

    /* Restore screen bits OR manually erase if restore fails */
    if (gCurrentSavedBits != NULL) {
        RestoreMenuBits_Display(gCurrentSavedBits, &gCurrentMenuRect);
        DisposeMenuBits(gCurrentSavedBits);
        gCurrentSavedBits = NULL;
    } else {
        /* CRITICAL: Manually erase menu rect if SaveBits failed
         * This happens when memory is low and SaveBits couldn't allocate */
        extern void* framebuffer;
        extern uint32_t fb_pitch;

        if (framebuffer) {
            uint32_t bytes_per_pixel = 4;
            SInt16 width = gCurrentMenuRect.right - gCurrentMenuRect.left;
            SInt16 height = gCurrentMenuRect.bottom - gCurrentMenuRect.top;

            /* Fill menu rect with white (desktop color) */
            for (SInt16 y = 0; y < height; y++) {
                SInt16 screenY = gCurrentMenuRect.top + y;
                if (screenY >= 0 && screenY < 600) {
                    for (SInt16 x = 0; x < width; x++) {
                        SInt16 screenX = gCurrentMenuRect.left + x;
                        if (screenX >= 0 && screenX < 800) {
                            uint32_t offset = screenY * (fb_pitch / bytes_per_pixel) + screenX;
                            ((uint32_t*)framebuffer)[offset] = 0xFFFFFFFF;
                        }
                    }
                }
            }
        }
    }

    /* MENU_LOG_TRACE("Hiding menu ID %d\n", (*(MenuInfo**)gCurrentlyShownMenu)->menuID); */

    ShowCursor();

    gCurrentlyShownMenu = NULL;
}

/*
 * DrawMenu - Draw a menu
 */
void DrawMenu(MenuHandle theMenu, const Rect* menuRect, short hiliteItem)
{
    MENU_LOG_TRACE("DEBUG: DrawMenu (new) called with menuRect=%p, hiliteItem=%d\n", menuRect, hiliteItem);
    Boolean cursorHidden = false;

    if (theMenu == NULL || menuRect == NULL) {
        return;
    }

    /* Initialize drawing context */
    InitializeDrawingContext(&gDrawingContext);

    /* Hide cursor while drawing menu to prevent Z-ordering issues */
    HideCursor();
    cursorHidden = true;

    /* Draw menu frame */
    DrawMenuFrame(menuRect, false);

    /* Draw menu background */
    DrawMenuBackground(menuRect, (*(MenuInfo**)theMenu)->menuID);

    /* Draw menu items */
    short itemCount = CountMItems(theMenu);

    for (short i = 1; i <= itemCount; i++) {
        Rect itemRect;
        Style tempStyle;
        CalcMenuItemRect(theMenu, i, menuRect, &itemRect);

        MenuItemDrawInfo itemDrawInfo;
        itemDrawInfo.menu = theMenu;
        itemDrawInfo.itemNum = i;
        itemDrawInfo.itemRect = itemRect;
        itemDrawInfo.itemFlags = kMenuItemNormal;
        if (i == hiliteItem) {
            itemDrawInfo.itemFlags |= kMenuItemSelected;
        }
        itemDrawInfo.context = &gDrawingContext;

        /* Get item properties */
        GetMenuItemText(theMenu, i, itemDrawInfo.itemText);
        GetItemIcon(theMenu, i, (short*)&itemDrawInfo.iconID);
        GetItemMark(theMenu, i, (short*)&itemDrawInfo.markChar);
        GetItemCmd(theMenu, i, (short*)&itemDrawInfo.cmdChar);
        GetItemStyle(theMenu, i, &tempStyle);
        itemDrawInfo.textStyle = tempStyle;

        DrawMenuItem(&itemDrawInfo);
    }

    /* MENU_LOG_TRACE("Drew menu ID %d with %d items (hilite: %d)\n",
           (*(MenuInfo**)theMenu)->menuID, itemCount, hiliteItem); */

    if (cursorHidden) {
        ShowCursor();
    }
}

/*
 * DrawMenuFrame - Draw menu frame
 */
void DrawMenuFrame(const Rect* menuRect, Boolean selected)
{
    if (menuRect == NULL) {
        return;
    }

    DrawMenuFrameInternal(menuRect, selected);
}

/*
 * DrawMenuBackground - Draw menu background
 */
void DrawMenuBackground(const Rect* menuRect, short menuID)
{
    if (menuRect == NULL) {
        return;
    }

    DrawMenuBackgroundInternal(menuRect, menuID);
}

/* ============================================================================
 * Menu Item Display Functions
 * ============================================================================ */

/*
 * DrawMenuItem - Draw a menu item
 */
void DrawMenuItem(const MenuItemDrawInfo* drawInfo)
{
    Rect textRect, iconRect, markRect, cmdRect;
    Boolean enabled, selected;
    MenuHandle theMenu;
    short menuID;

    if (drawInfo == NULL) {
        return;
    }

    theMenu = drawInfo->menu;
    if (theMenu == NULL) {
        return;
    }

    enabled = !(drawInfo->itemFlags & kMenuItemDisabled);
    selected = (drawInfo->itemFlags & kMenuItemSelected) != 0;

    /* Calculate item component rectangles */
    CalcMenuItemRects(theMenu, drawInfo->itemNum, &drawInfo->itemRect,
                     &textRect, &iconRect, &markRect, &cmdRect);

    menuID = (*(MenuInfo**)theMenu)->menuID;
    SetupMenuDrawingColors(menuID, drawInfo->itemNum);

    /* Check for separator */
    if (drawInfo->itemFlags & kMenuItemIsSeparator) {
        DrawMenuSeparator(&drawInfo->itemRect, (*(MenuInfo**)(drawInfo->menu))->menuID);
        return;
    }

    /* Draw selection background if selected */
    if (selected) {
        /* MENU_LOG_TRACE("Drawing selected background for item %d\n", drawInfo->itemNum); */
        FillRect(&drawInfo->itemRect, &qd.ltGray);
    }

    /* Draw item components */
    if (drawInfo->itemFlags & kMenuItemHasIcon) {
        DrawMenuItemIconInternal(&iconRect, drawInfo->iconID, enabled, selected);
    }

    if (drawInfo->itemFlags & kMenuItemChecked) {
        DrawMenuItemMarkInternal(&markRect, drawInfo->markChar, enabled, selected);
    }

    DrawMenuItemTextInternal(&textRect, drawInfo->itemText, drawInfo->textStyle,
                           enabled, selected, false);  /* false = not a menu title */

    if (drawInfo->itemFlags & kMenuItemHasCmdKey) {
        DrawMenuItemCmdKeyInternal(&cmdRect, drawInfo->cmdChar, enabled, selected);
    }
}

/*
 * DrawMenuItemText - Draw menu item text
 */
void DrawMenuItemText(const Rect* itemRect, ConstStr255Param itemText,
                     Style textStyle, Boolean enabled, Boolean selected)
{
    if (itemRect == NULL || itemText == NULL) {
        return;
    }

    DrawMenuItemTextInternal(itemRect, itemText, textStyle, enabled, selected, false);  /* false = not a menu title */
}

/*
 * DrawMenuItemIcon - Draw menu item icon
 */
void DrawMenuItemIcon(const Rect* iconRect, short iconID,
                     Boolean enabled, Boolean selected)
{
    if (iconRect == NULL || iconID == 0) {
        return;
    }

    DrawMenuItemIconInternal(iconRect, iconID, enabled, selected);
}

/*
 * DrawMenuItemMark - Draw menu item mark
 */
void DrawMenuItemMark(const Rect* markRect, unsigned char markChar,
                     Boolean enabled, Boolean selected)
{
    if (markRect == NULL || markChar == 0) {
        return;
    }

    DrawMenuItemMarkInternal(markRect, markChar, enabled, selected);
}

/*
 * DrawMenuItemCmdKey - Draw command key equivalent
 */
void DrawMenuItemCmdKey(const Rect* cmdRect, unsigned char cmdChar,
                       Boolean enabled, Boolean selected)
{
    if (cmdRect == NULL || cmdChar == 0) {
        return;
    }

    DrawMenuItemCmdKeyInternal(cmdRect, cmdChar, enabled, selected);
}

/*
 * DrawMenuSeparator - Draw menu separator line
 */
void DrawMenuSeparator(const Rect* itemRect, short menuID)
{
    Rect lineRect;

    if (itemRect == NULL) {
        return;
    }

    /* Calculate separator line rectangle */
    lineRect = *itemRect;
    lineRect.left += 8;
    lineRect.right -= 8;
    lineRect.top += (RectHeight(itemRect) / 2) - 1;
    lineRect.bottom = lineRect.top + 1;

    /* MENU_LOG_TRACE("Drawing separator line in menu %d\n", menuID); */

    (void)menuID;

    PenNormal();
    PenPat(&qd.gray);
    PaintRect(&lineRect);
    PenNormal();
}

/*
 * HiliteMenuItem - Highlight menu item
 */
void HiliteMenuItem(MenuHandle theMenu, short item, Boolean hilite)
{
    if (theMenu == NULL || item < 0) {
        return;
    }

    /* Use platform-specific highlighting if available */
    Platform_HiliteMenuItem(theMenu, item, hilite);

    /* MENU_LOG_TRACE("%s menu item %d in menu %d\n",
           hilite ? "Highlighting" : "Unhighlighting",
           item, (*(MenuInfo**)theMenu)->menuID); */
}

/* ============================================================================
 * Menu Layout and Measurement Functions
 * ============================================================================ */

/*
 * CalcMenuRect - Calculate menu rectangle
 */
void CalcMenuRect(MenuHandle theMenu, Point location, Rect* menuRect)
{
    short itemCount, menuWidth = 0, menuHeight = 0;

    if (theMenu == NULL || menuRect == NULL) {
        return;
    }

    itemCount = CountMItems(theMenu);

    /* Calculate menu width */
    for (short i = 1; i <= itemCount; i++) {
        short itemWidth = MeasureMenuItemWidth(theMenu, i);
        if (itemWidth > menuWidth) {
            menuWidth = itemWidth;
        }
    }

    /* Add margins */
    menuWidth += 32; /* Left and right margins */

    /* Calculate menu height */
    menuHeight = itemCount * menuItemStdHeight + 8; /* Top and bottom margins */

    /* Set up rectangle */
    menuRect->left = location.h;
    menuRect->top = location.v;
    menuRect->right = menuRect->left + menuWidth;
    menuRect->bottom = menuRect->top + menuHeight;

    /* Update menu info */
    (*(MenuInfo**)theMenu)->menuWidth = menuWidth;
    (*(MenuInfo**)theMenu)->menuHeight = menuHeight;
}

/*
 * CalcMenuItemRect - Calculate menu item rectangle
 */
void CalcMenuItemRect(MenuHandle theMenu, short item, const Rect* menuRect,
                     Rect* itemRect)
{
    if (theMenu == NULL || menuRect == NULL || itemRect == NULL || item < 1) {
        return;
    }

    itemRect->left = menuRect->left + 4; /* Left margin */
    itemRect->right = menuRect->right - 4; /* Right margin */
    itemRect->top = menuRect->top + 4 + (item - 1) * menuItemStdHeight; /* Top margin + item offset */
    itemRect->bottom = itemRect->top + menuItemStdHeight;
}

/*
 * MeasureMenuText - Measure menu text
 */
void MeasureMenuText(ConstStr255Param text, Style textStyle, short textSize,
                    short* width, short* height)
{
    GrafPtr savedPort = NULL;
    short savedFont = 0;
    short savedSize = 0;
    UInt8 savedFace = 0;
    short effectiveSize;

    if (text == NULL) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    effectiveSize = (textSize > 0) ? textSize : 12;

    GetPort(&savedPort);
    if (savedPort != NULL) {
        savedFont = savedPort->txFont;
        savedSize = savedPort->txSize;
        savedFace = savedPort->txFace;
    }

    TextFont(chicagoFont);
    TextSize(effectiveSize);
    TextFace(textStyle);

    if (width != NULL) {
        *width = StringWidth(text);
    }

    if (height != NULL) {
        FMetricRec metrics = {0};
        GetFontMetrics(&metrics);
        short computedHeight = (short)(metrics.ascent + metrics.descent + metrics.leading);
        *height = computedHeight > 0 ? computedHeight : effectiveSize;
    }

    if (savedPort != NULL) {
        TextFont(savedFont);
        TextSize(savedSize);
        TextFace(savedFace);
    }
}

/*
 * GetMenuItemHeight - Get height for menu item
 */
short GetMenuItemHeight(MenuHandle theMenu, short item)
{
    if (theMenu == NULL || item < 1) {
        return 0;
    }

    /* TODO: Check for custom item heights */
    return menuItemStdHeight;
}

/*
 * GetMenuTitleWidth - Get width for menu title
 */
short GetMenuTitleWidth(MenuHandle theMenu)
{
    short titleWidth;

    if (theMenu == NULL) {
        return 0;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)theMenu);
    titleWidth = GetMenuItemTextWidth(&(*(MenuInfo**)theMenu)->menuData[0], normal);
    HUnlock((Handle)theMenu);

    return titleWidth + 16; /* Add margins */
}

/* ============================================================================
 * Visual Effects and Animation
 * ============================================================================ */

/*
 * FlashMenuItem - Flash menu item
 *
 * Provides visual feedback by briefly flashing a menu item. The item is
 * redrawn with highlighting toggled on/off. This works by directly redrawing
 * the menu item rather than relying on Platform_HiliteMenuItem stub.
 *
 * Only works if the menu is currently shown (gCurrentlyShownMenu).
 */
void FlashMenuItem(MenuHandle theMenu, short item, short flashes)
{
    if (theMenu == NULL || item < 1 || flashes < 1) {
        return;
    }

    /* Only flash if this menu is currently shown on screen */
    if (gCurrentlyShownMenu != theMenu) {
        return;
    }

    /* Get item properties to build draw info */
    Rect itemRect;
    Style tempStyle;
    CalcMenuItemRect(theMenu, item, &gCurrentMenuRect, &itemRect);

    MenuItemDrawInfo itemDrawInfo;
    itemDrawInfo.menu = theMenu;
    itemDrawInfo.itemNum = item;
    itemDrawInfo.itemRect = itemRect;
    itemDrawInfo.itemFlags = kMenuItemNormal;
    itemDrawInfo.context = &gDrawingContext;

    /* Get item properties */
    GetMenuItemText(theMenu, item, itemDrawInfo.itemText);
    GetItemIcon(theMenu, item, (short*)&itemDrawInfo.iconID);
    GetItemMark(theMenu, item, (short*)&itemDrawInfo.markChar);
    GetItemCmd(theMenu, item, (short*)&itemDrawInfo.cmdChar);
    GetItemStyle(theMenu, item, &tempStyle);
    itemDrawInfo.textStyle = tempStyle;

    /* Check if item is disabled */
    extern Boolean CheckMenuItemEnabled(MenuHandle theMenu, short item);
    if (!CheckMenuItemEnabled(theMenu, item)) {
        itemDrawInfo.itemFlags |= kMenuItemDisabled;
    }

    /* Flash multiple times by redrawing item with highlight toggled */
    extern UInt32 TickCount(void);
    extern void SystemTask(void);

    for (short i = 0; i < flashes; i++) {
        /* Draw highlighted */
        itemDrawInfo.itemFlags |= kMenuItemSelected;
        DrawMenuItem(&itemDrawInfo);

        /* Brief delay ~5 ticks (83ms at 60Hz) */
        UInt32 startTick = TickCount();
        while ((TickCount() - startTick) < 5) {
            SystemTask();
        }

        /* Draw normal */
        itemDrawInfo.itemFlags &= ~kMenuItemSelected;
        DrawMenuItem(&itemDrawInfo);

        /* Brief delay between flashes */
        if (i < flashes - 1) {
            startTick = TickCount();
            while ((TickCount() - startTick) < 5) {
                SystemTask();
            }
        }
    }
}

/*
 * AnimateMenuShow - Animate menu appearance
 */
void AnimateMenuShow(MenuHandle theMenu, const Rect* startRect,
                    const Rect* endRect, short duration)
{
    if (theMenu == NULL || startRect == NULL || endRect == NULL) {
        return;
    }

    /* MENU_LOG_TRACE("Animating menu show for menu %d (duration: %d)\n",
           (*(MenuInfo**)theMenu)->menuID, duration); */

    /* Implement menu show animation by drawing expanding rectangles */
    if (duration > 0) {
        short steps = duration / 2; /* Number of animation steps */
        if (steps < 1) steps = 1;
        if (steps > 10) steps = 10; /* Cap at 10 steps for performance */

        for (short step = 0; step <= steps; step++) {
            /* Calculate intermediate rectangle by linear interpolation */
            Rect currentRect;
            currentRect.left = startRect->left +
                ((endRect->left - startRect->left) * step) / steps;
            currentRect.top = startRect->top +
                ((endRect->top - startRect->top) * step) / steps;
            currentRect.right = startRect->right +
                ((endRect->right - startRect->right) * step) / steps;
            currentRect.bottom = startRect->bottom +
                ((endRect->bottom - startRect->bottom) * step) / steps;

            /* Draw the expanding frame */
            FrameRect(&currentRect);

            /* Brief delay */
            extern void Platform_WaitTicks(short ticks);
            Platform_WaitTicks(1);

            /* Erase the frame (redraw background or invert again) */
            FrameRect(&currentRect);
        }
    }
}

/*
 * AnimateMenuHide - Animate menu disappearance
 */
void AnimateMenuHide(MenuHandle theMenu, const Rect* startRect,
                    const Rect* endRect, short duration)
{
    if (theMenu == NULL || startRect == NULL || endRect == NULL) {
        return;
    }

    /* MENU_LOG_TRACE("Animating menu hide for menu %d (duration: %d)\n",
           (*(MenuInfo**)theMenu)->menuID, duration); */

    /* Implement menu hide animation by drawing collapsing rectangles */
    if (duration > 0) {
        short steps = duration / 2; /* Number of animation steps */
        if (steps < 1) steps = 1;
        if (steps > 10) steps = 10; /* Cap at 10 steps for performance */

        for (short step = 0; step <= steps; step++) {
            /* Calculate intermediate rectangle by linear interpolation (reverse) */
            Rect currentRect;
            currentRect.left = startRect->left +
                ((endRect->left - startRect->left) * step) / steps;
            currentRect.top = startRect->top +
                ((endRect->top - startRect->top) * step) / steps;
            currentRect.right = startRect->right +
                ((endRect->right - startRect->right) * step) / steps;
            currentRect.bottom = startRect->bottom +
                ((endRect->bottom - startRect->bottom) * step) / steps;

            /* Draw the collapsing frame */
            FrameRect(&currentRect);

            /* Brief delay */
            extern void Platform_WaitTicks(short ticks);
            Platform_WaitTicks(1);

            /* Erase the frame */
            FrameRect(&currentRect);
        }
    }
}

/* ============================================================================
 * Screen Management Functions
 * ============================================================================ */

/*
 * SaveMenuBits - Save screen bits under menu
 */
Handle SaveMenuBits_Display(const Rect* menuRect)
{
    if (menuRect == NULL) {
        return NULL;
    }

    /* Use platform-specific screen capture if available */
    return Platform_SaveScreenBits(menuRect);
}

/*
 * RestoreMenuBits - Restore saved screen bits
 */
void RestoreMenuBits_Display(Handle savedBits, const Rect* menuRect)
{
    if (savedBits == NULL || menuRect == NULL) {
        return;
    }

    Platform_RestoreScreenBits(savedBits, menuRect);
}

/*
 * DisposeMenuBits - Dispose saved screen bits
 */
void DisposeMenuBits(Handle savedBits)
{
    if (savedBits == NULL) {
        return;
    }

    Platform_DisposeScreenBits(savedBits);
}

/* ============================================================================
 * Color and Appearance Functions
 * ============================================================================ */

/*
 * GetMenuColors - Get colors for menu components
 */
void GetMenuColors(short menuID, short itemID, short componentID,
                  RGBColor* foreColor, RGBColor* backColor)
{
    /* Default colors */
    if (foreColor != NULL) {
        foreColor->red = 0x0000;
        foreColor->green = 0x0000;
        foreColor->blue = 0x0000;
    }

    if (backColor != NULL) {
        backColor->red = 0xFFFF;
        backColor->green = 0xFFFF;
        backColor->blue = 0xFFFF;
    }

    /* TODO: Look up colors in menu color table */
}

/*
 * SetMenuDrawingMode - Set drawing mode
 */
void SetMenuDrawingMode(Boolean useColor, Boolean antiAlias, Boolean usePatterns)
{
    gColorMode = useColor;
    gAntiAlias = antiAlias;

    /* MENU_LOG_TRACE("Set menu drawing mode: color=%s, antiAlias=%s, patterns=%s\n",
           useColor ? "Yes" : "No", antiAlias ? "Yes" : "No", usePatterns ? "Yes" : "No"); */
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/*
 * InitializeDrawingContext - Initialize drawing context
 */
static void InitializeDrawingContext(MenuDrawContext* context)
{
    if (context == NULL) {
        return;
    }

    memset(context, 0, sizeof(MenuDrawContext));
    context->textFont = 0; /* System font */
    context->textSize = 12; /* 12 point */
    context->textStyle = normal;
    context->colorMode = gColorMode;
    context->antiAlias = gAntiAlias;
}

/*
 * SetupMenuDrawingColors - Set up colors for drawing
 */
static void SetupMenuDrawingColors(short menuID, short itemID)
{
    RGBColor foreColor, backColor;

    GetMenuColors(menuID, itemID, 0, &foreColor, &backColor);

    (void)itemID;

    if (gColorMode) {
        RGBForeColor(&foreColor);
        RGBBackColor(&backColor);
    } else {
        ForeColor(blackColor);
        BackColor(whiteColor);
    }
}

/*
 * DrawMenuFrameInternal - Internal menu frame drawing
 */
static void DrawMenuFrameInternal(const Rect* menuRect, Boolean selected)
{
    /* MENU_LOG_TRACE("Drawing menu frame (%d,%d,%d,%d) selected=%s\n",
           menuRect->left, menuRect->top, menuRect->right, menuRect->bottom,
           selected ? "Yes" : "No"); */

    (void)selected;

    PenNormal();
    PenSize(1, 1);
    FrameRect(menuRect);
}

/*
 * DrawMenuBackgroundInternal - Internal menu background drawing
 */
static void DrawMenuBackgroundInternal(const Rect* menuRect, short menuID)
{
    /* MENU_LOG_TRACE("Drawing menu background for menu %d\n", menuID); */

    (void)menuID;

    FillRect(menuRect, &qd.white);
}

/*
 * DrawMenuItemTextInternal - Internal menu item text drawing
 * isMenuTitle: If true, use rect directly (no padding). For menu bar titles.
 *              If false, add padding. For dropdown menu items.
 */
static void DrawMenuItemTextInternal(const Rect* itemRect, ConstStr255Param itemText,
                                   short textStyle, Boolean enabled, Boolean selected,
                                   Boolean isMenuTitle)
{
    /* MENU_LOG_TRACE("Drawing item text: %.*s (enabled=%s, selected=%s)\n",
           itemText[0], &itemText[1], enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    /* Set font for menu items (Chicago 12pt) */
    TextFont(chicagoFont);
    TextSize(12);
    TextFace(textStyle);  /* Apply style (bold, italic, etc.) */

    /* Set text color based on selected and enabled state */
    if (selected) {
        /* CRITICAL: When menu is highlighted (inverted background), draw WHITE text */
        extern void serial_puts(const char* str);
        serial_puts("[HILITE-TEXT] Drawing highlighted text - setting ForeColor WHITE\n");
        ForeColor(whiteColor);  /* White text on black (inverted) background */
    } else if (!enabled) {
        ForeColor(8);  /* Gray color for disabled items */
    }

    /* Calculate text position (left-aligned with padding) */
    /* Always add 4 pixels for left padding - matches DrawMenuBar's MoveTo(x + 4, ...) */
    short textX = itemRect->left + 4;
    short textY = itemRect->top + ((itemRect->bottom - itemRect->top) + 9) / 2; /* Vertically centered */

    /* Move to drawing position */
    if (selected) {
        extern void serial_puts(const char* str);
        static char moveBuf[256];
        extern int snprintf(char*, size_t, const char*, ...);
        snprintf(moveBuf, sizeof(moveBuf), "[HILITE-TEXT] About to MoveTo(%d,%d), itemRect.left=%d+4 padding\n",
                 textX, textY, itemRect->left);
        serial_puts(moveBuf);
    }
    MoveTo(textX, textY);
    if (selected) {
        extern void serial_puts(const char* str);
        extern GrafPtr g_currentPort;
        static char afterMoveBuf[256];
        extern int snprintf(char*, size_t, const char*, ...);
        snprintf(afterMoveBuf, sizeof(afterMoveBuf), "[HILITE-TEXT] After MoveTo, pnLoc=(%d,%d)\n",
                 g_currentPort->pnLoc.h, g_currentPort->pnLoc.v);
        serial_puts(afterMoveBuf);
    }
    if (selected) {
        extern void serial_puts(const char* str);
        serial_puts("[HILITE-TEXT] MoveTo completed\n");
    }

    /* Draw the menu item text using Font Manager */
    DrawString(itemText);

    /* Restore black color if we changed it */
    if (selected || !enabled) {
        ForeColor(blackColor);  /* Restore to black */
    }
}

/*
 * DrawMenuItemIconInternal - Internal menu item icon drawing
 */
static void DrawMenuItemIconInternal(const Rect* iconRect, short iconID,
                                   Boolean enabled, Boolean selected)
{
    /* MENU_LOG_TRACE("Drawing item icon %d (enabled=%s, selected=%s)\n",
           iconID, enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    /* TODO: Draw actual icon */

    (void)iconRect;
    (void)iconID;
    (void)enabled;
    (void)selected;
}

/*
 * DrawMenuItemMarkInternal - Internal menu item mark drawing
 */
static void DrawMenuItemMarkInternal(const Rect* markRect, unsigned char markChar,
                                   Boolean enabled, Boolean selected)
{
    Str255 markStr;

    /* MENU_LOG_TRACE("Drawing item mark '%c' (enabled=%s, selected=%s)\n",
           markChar, enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    if (markChar == 0) {
        return;
    }

    /* Set font for mark character */
    TextFont(chicagoFont);
    TextSize(12);

    /* Set color based on enabled state */
    if (!enabled) {
        ForeColor(8);  /* Gray for disabled */
    }

    /* Position mark character in mark column */
    short markX = markRect->left + 2;
    short markY = markRect->bottom - 3;
    MoveTo(markX, markY);

    /* Draw mark character (typically checkMark = 18) */
    markStr[0] = 1;
    markStr[1] = markChar;
    DrawString(markStr);

    /* Restore color */
    if (!enabled) {
        ForeColor(blackColor);  /* Restore to black */
    }

    (void)selected;
}

/*
 * DrawMenuItemCmdKeyInternal - Internal command key drawing
 */
static void DrawMenuItemCmdKeyInternal(const Rect* cmdRect, unsigned char cmdChar,
                                     Boolean enabled, Boolean selected)
{
    Str255 cmdStr;
    short cmdWidth;
    char upperChar;

    MENU_LOG_TRACE("DEBUG: DrawMenuItemCmdKeyInternal called, cmdChar=%d\n", (int)cmdChar);

    if (cmdChar == 0) {
        MENU_LOG_TRACE("DEBUG: cmdChar is 0, returning\n");
        return;
    }

    /* Set font for command key */
    TextFont(chicagoFont);
    TextSize(12);

    MENU_LOG_TRACE("DEBUG: Font set, checking enabled\n");

    /* Set color based on enabled state */
    if (!enabled) {
        ForeColor(8);  /* Gray for disabled */
    }

    /* Convert command key to uppercase for display */
    upperChar = cmdChar;
    if (upperChar >= 'a' && upperChar <= 'z') {
        upperChar = upperChar - 'a' + 'A';
    }

    MENU_LOG_TRACE("DEBUG: Building cmd string\n");

    /* Build command key string with ⌘ symbol (0x11) and key */
    cmdStr[0] = 2;
    cmdStr[1] = 0x11;  /* Command symbol */
    cmdStr[2] = upperChar;

    MENU_LOG_TRACE("DEBUG: Calling StringWidth\n");

    /* Calculate width and right-align in command rectangle */
    cmdWidth = StringWidth(cmdStr);

    MENU_LOG_TRACE("DEBUG: StringWidth returned, calculating position\n");

    short cmdX = cmdRect->right - cmdWidth - 4;
    short cmdY = cmdRect->bottom - 3;
    MoveTo(cmdX, cmdY);

    MENU_LOG_TRACE("DEBUG: About to DrawString cmd key\n");

    /* Draw command key */
    DrawString(cmdStr);

    MENU_LOG_TRACE("DEBUG: Drew cmd key successfully\n");

    /* Restore color */
    if (!enabled) {
        ForeColor(blackColor);  /* Restore to black */
    }

    (void)selected;
}

/*
 * CalcMenuItemRects - Calculate rectangles for menu item components
 */
static void CalcMenuItemRects(MenuHandle theMenu, short item, const Rect* menuRect,
                            Rect* textRect, Rect* iconRect, Rect* markRect, Rect* cmdRect)
{
    Rect itemRect;

    CalcMenuItemRect(theMenu, item, menuRect, &itemRect);

    /* Icon rectangle (left side) */
    if (iconRect != NULL) {
        iconRect->left = itemRect.left + 2;
        iconRect->top = itemRect.top + 2;
        iconRect->right = iconRect->left + 12;
        iconRect->bottom = iconRect->top + 12;
    }

    /* Mark rectangle (left side, after icon) */
    if (markRect != NULL) {
        markRect->left = itemRect.left + 16;
        markRect->top = itemRect.top + 2;
        markRect->right = markRect->left + 12;
        markRect->bottom = markRect->top + 12;
    }

    /* Command key rectangle (right side) */
    if (cmdRect != NULL) {
        cmdRect->right = itemRect.right - 4;
        cmdRect->top = itemRect.top + 2;
        cmdRect->left = cmdRect->right - 16;
        cmdRect->bottom = cmdRect->top + 12;
    }

    /* Text rectangle (center, between mark and command key) */
    if (textRect != NULL) {
        textRect->left = itemRect.left + 30;
        textRect->top = itemRect.top + 2;
        textRect->right = itemRect.right - 20;
        textRect->bottom = itemRect.bottom - 2;
    }
}

/*
 * MeasureMenuItemWidth - Measure width needed for menu item
 */
static short MeasureMenuItemWidth(MenuHandle theMenu, short item)
{
    Str255 itemText;
    short textWidth = 0;
    short totalWidth = 0;

    if (theMenu == NULL || item < 1) {
        return 0;
    }

    GetMenuItemText(theMenu, item, itemText);
    textWidth = GetMenuItemTextWidth(itemText, normal);

    totalWidth = 30 + textWidth + 20; /* Icon + mark + text + command key + margins */

    return totalWidth;
}

/*
 * GetMenuItemTextWidth - Get text width for menu item
 */
static short GetMenuItemTextWidth(ConstStr255Param text, Style textStyle)
{
    GrafPtr savedPort = NULL;
    short savedFont = 0;
    short savedSize = 0;
    UInt8 savedFace = 0;
    short width = 0;

    if (text == NULL) {
        return 0;
    }

    GetPort(&savedPort);
    if (savedPort != NULL) {
        savedFont = savedPort->txFont;
        savedSize = savedPort->txSize;
        savedFace = savedPort->txFace;
    }

    TextFont(chicagoFont);
    TextSize(12);
    TextFace(textStyle);

    width = StringWidth(text);

    if (savedPort != NULL) {
        TextFont(savedFont);
        TextSize(savedSize);
        TextFace(savedFace);
    }

    return width;
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

#ifdef DEBUG
void PrintMenuDisplayState(void)
{
    /* MENU_LOG_TRACE("=== Menu Display State ===\n"); */
    /* MENU_LOG_TRACE("Color mode: %s\n", gColorMode ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("Anti-alias: %s\n", gAntiAlias ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("Current menu: %s\n", gCurrentlyShownMenu ? "Yes" : "No"); */
    if (gCurrentlyShownMenu != NULL) {
        /* MENU_LOG_TRACE("  Menu ID: %d\n", (*gCurrentlyShownMenu)->menuID); */
        /* MENU_LOG_TRACE("  Menu rect: (%d,%d,%d,%d)\n",
               gCurrentMenuRect.left, gCurrentMenuRect.top,
               gCurrentMenuRect.right, gCurrentMenuRect.bottom); */
    }
    /* MENU_LOG_TRACE("Saved bits: %s\n", gCurrentSavedBits ? "Yes" : "No"); */
    /* MENU_LOG_TRACE("========================\n"); */
}
#endif

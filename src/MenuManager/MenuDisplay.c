/* #include "SystemTypes.h" */
#include <string.h>
/*
 * MenuDisplay.c - Menu Drawing and Visual Management
 *
 * This file implements all menu display functionality including menu bar
 * rendering, pull-down menu display, menu item drawing, and visual effects.
 * It provides the complete visual representation of the Mac OS menu system.
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
#include "MenuManager/MenuDisplay.h"

#include <math.h>

/* Serial output */
extern void serial_printf(const char* format, ...);

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

/* Platform function prototypes */
extern void Platform_DrawMenuBar(const MenuBarDrawInfo* drawInfo);
extern void Platform_DrawMenu(const MenuDrawInfo* drawInfo);
extern void Platform_DrawMenuItem(const MenuItemDrawInfo* drawInfo);

/* Forward declarations */
Handle SaveMenuBits_Display(const Rect* menuRect);
void RestoreMenuBits_Display(Handle savedBits, const Rect* menuRect);
extern void Platform_HiliteMenuItem(MenuHandle theMenu, short item, Boolean hilite);
extern void Platform_FlashMenuBar(short menuID);
extern Handle Platform_SaveScreenBits(const Rect* rect);
extern void Platform_RestoreScreenBits(Handle savedBits, const Rect* rect);
extern void Platform_DisposeScreenBits(Handle savedBits);

/* Internal function prototypes */
static void InitializeDrawingContext(MenuDrawContext* context);
static void SetupMenuDrawingColors(short menuID, short itemID);
static void DrawMenuFrameInternal(const Rect* menuRect, Boolean selected);
static void DrawMenuBackgroundInternal(const Rect* menuRect, short menuID);
static void DrawMenuItemTextInternal(const Rect* itemRect, ConstStr255Param itemText,
                                   Style textStyle, Boolean enabled, Boolean selected);
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

    /* serial_printf("Drawing menu bar (mode: %d, hilite: %d)\n",
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
    /* serial_printf("Erasing menu bar rect (%d,%d,%d,%d)\n",
           eraseRect.left, eraseRect.top, eraseRect.right, eraseRect.bottom); */

    /* TODO: Implement actual background fill */
}

/*
 * HiliteMenuTitle - Highlight menu title
 */
void HiliteMenuTitle(short menuID, Boolean hilite)
{
    Rect titleRect;

    /* Get menu title rectangle using the tracking system */
    extern Boolean GetMenuTitleRectByID(short menuID, Rect* outRect);
    if (GetMenuTitleRectByID(menuID, &titleRect)) {
        /* Draw highlighted or normal title */
        DrawMenuTitle(menuID, &titleRect, hilite);

        /* Debug output */
        /* serial_printf already declared above as void */
        serial_printf("HiliteMenuTitle: menuID=%d hilite=%d rect=(%d,%d,%d,%d)\n",
                      menuID, hilite, titleRect.left, titleRect.top,
                      titleRect.right, titleRect.bottom);
    } else {
        /* serial_printf already declared above as void */
        serial_printf("HiliteMenuTitle: GetMenuTitleRectByID failed for menuID=%d\n", menuID);
    }
}

/*
 * DrawMenuTitle - Draw individual menu title
 */
void DrawMenuTitle(short menuID, const Rect* titleRect, Boolean hilited)
{
    MenuHandle theMenu;
    unsigned char titleText[256];
    short textWidth, textHeight;
    Rect textRect;

    if (titleRect == NULL) {
        return;
    }

    theMenu = GetMenuHandle(menuID);
    if (theMenu == NULL) {
        return;
    }

    /* Get menu title */
    short titleLen = (*(MenuInfo**)theMenu)->menuData[0];
    if (titleLen > 255) titleLen = 255;
    titleText[0] = titleLen;
    memcpy(&titleText[1], &(*(MenuInfo**)theMenu)->menuData[1], titleLen);

    /* Set up text rectangle */
    textRect = *titleRect;
    textRect.left += 8;  /* Left margin */
    textRect.right -= 8; /* Right margin */
    textRect.top += 2;   /* Top margin */
    textRect.bottom -= 2; /* Bottom margin */

    /* Set drawing colors based on hilite state */
    if (hilited) {
        /* Highlighted state - invert the title rect */
        extern void InvertRect(const Rect* rect);
        InvertRect(titleRect);

        /* serial_printf already declared above as void */
        serial_printf("Drew highlighted menu title: %.*s\n", titleLen, &titleText[1]);
    } else {
        /* Normal state - erase with white background first to unhighlight */
        extern void EraseRect(const Rect* rect);
        EraseRect(titleRect);

        /* Then draw the text normally */
        /* serial_printf already declared above as void */
        serial_printf("Drew normal menu title: %.*s\n", titleLen, &titleText[1]);
    }

    /* Draw the title text */
    DrawMenuItemTextInternal(&textRect, titleText, normal, true, hilited);
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

    /* Save screen bits under menu */
    if (gCurrentSavedBits != NULL) {
        DisposeMenuBits(gCurrentSavedBits);
    }
    gCurrentSavedBits = SaveMenuBits_Display(&menuRect);

    /* Draw the menu */
    DrawMenu(theMenu, &menuRect, 0);

    /* Remember current menu */
    gCurrentlyShownMenu = theMenu;
    gCurrentMenuRect = menuRect;

    /* serial_printf("Showing menu ID %d at (%d,%d)\n",
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

    /* Restore screen bits */
    if (gCurrentSavedBits != NULL) {
        RestoreMenuBits_Display(gCurrentSavedBits, &gCurrentMenuRect);
        DisposeMenuBits(gCurrentSavedBits);
        gCurrentSavedBits = NULL;
    }

    /* serial_printf("Hiding menu ID %d\n", (*(MenuInfo**)gCurrentlyShownMenu)->menuID); */

    /* TODO: Show cursor again after menu is hidden */
    /* extern void ShowCursor(void); */
    /* ShowCursor(); */

    gCurrentlyShownMenu = NULL;
}

/*
 * DrawMenu - Draw a menu
 */
void DrawMenu(MenuHandle theMenu, const Rect* menuRect, short hiliteItem)
{
    MenuDrawInfo drawInfo;

    if (theMenu == NULL || menuRect == NULL) {
        return;
    }

    /* Set up drawing info */
    drawInfo.menu = theMenu;
    drawInfo.menuRect = *menuRect;
    drawInfo.drawFlags = kMenuDrawNormal;
    drawInfo.hiliteItem = hiliteItem;
    drawInfo.context = &gDrawingContext;
    drawInfo.useColor = gColorMode;
    drawInfo.antiAlias = gAntiAlias;

    /* Initialize drawing context */
    InitializeDrawingContext(&gDrawingContext);

    /* TODO: Hide cursor while drawing menu to prevent Z-ordering issues */
    /* extern void HideCursor(void); */
    /* HideCursor(); */

    /* Draw menu frame */
    DrawMenuFrame(menuRect, false);

    /* Draw menu background */
    DrawMenuBackground(menuRect, (*(MenuInfo**)theMenu)->menuID);

    /* Draw menu items */
    short itemCount = CountMItems(theMenu);
    for (short i = 1; i <= itemCount; i++) {
        Rect itemRect;
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
        GetItemStyle(theMenu, i, &itemDrawInfo.textStyle);

        DrawMenuItem(&itemDrawInfo);
    }

    /* serial_printf("Drew menu ID %d with %d items (hilite: %d)\n",
           (*(MenuInfo**)theMenu)->menuID, itemCount, hiliteItem); */
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

    if (drawInfo == NULL) {
        return;
    }

    enabled = !(drawInfo->itemFlags & kMenuItemDisabled);
    selected = (drawInfo->itemFlags & kMenuItemSelected) != 0;

    /* Calculate item component rectangles */
    CalcMenuItemRects(drawInfo->menu, drawInfo->itemNum, &drawInfo->itemRect,
                     &textRect, &iconRect, &markRect, &cmdRect);

    /* Check for separator */
    if (drawInfo->itemFlags & kMenuItemIsSeparator) {
        DrawMenuSeparator(&drawInfo->itemRect, (*(MenuInfo**)(drawInfo->menu))->menuID);
        return;
    }

    /* Draw selection background if selected */
    if (selected) {
        /* TODO: Draw highlight background */
        /* serial_printf("Drawing selected background for item %d\n", drawInfo->itemNum); */
    }

    /* Draw item components */
    if (drawInfo->itemFlags & kMenuItemHasIcon) {
        DrawMenuItemIconInternal(&iconRect, drawInfo->iconID, enabled, selected);
    }

    if (drawInfo->itemFlags & kMenuItemChecked) {
        DrawMenuItemMarkInternal(&markRect, drawInfo->markChar, enabled, selected);
    }

    DrawMenuItemTextInternal(&textRect, drawInfo->itemText, drawInfo->textStyle,
                           enabled, selected);

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

    DrawMenuItemTextInternal(itemRect, itemText, textStyle, enabled, selected);
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

    /* serial_printf("Drawing separator line in menu %d\n", menuID); */

    /* TODO: Draw actual separator line */
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

    /* serial_printf("%s menu item %d in menu %d\n",
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
    if (text == NULL) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    /* Simple text measurement - TODO: Implement proper text metrics */
    short textLen = text[0];
    if (width) *width = textLen * 6; /* Approximate character width */
    if (height) *height = textSize > 0 ? textSize : 12; /* Default to 12pt */
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
    short titleLen, titleWidth;

    if (theMenu == NULL) {
        return 0;
    }

    titleLen = (*(MenuInfo**)theMenu)->menuData[0];
    titleWidth = GetMenuItemTextWidth(&(*(MenuInfo**)theMenu)->menuData[0], normal);

    return titleWidth + 16; /* Add margins */
}

/* ============================================================================
 * Visual Effects and Animation
 * ============================================================================ */

/*
 * FlashMenuItem - Flash menu item
 */
void FlashMenuItem(MenuHandle theMenu, short item, short flashes)
{
    if (theMenu == NULL || item < 1) {
        return;
    }

    for (short i = 0; i < flashes; i++) {
        HiliteMenuItem(theMenu, item, true);
        /* TODO: Add delay */
        HiliteMenuItem(theMenu, item, false);
        /* TODO: Add delay */
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

    /* serial_printf("Animating menu show for menu %d (duration: %d)\n",
           (*(MenuInfo**)theMenu)->menuID, duration); */

    /* TODO: Implement animation */
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

    /* serial_printf("Animating menu hide for menu %d (duration: %d)\n",
           (*(MenuInfo**)theMenu)->menuID, duration); */

    /* TODO: Implement animation */
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

    /* serial_printf("Set menu drawing mode: color=%s, antiAlias=%s, patterns=%s\n",
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

    /* TODO: Set actual drawing colors */
}

/*
 * DrawMenuFrameInternal - Internal menu frame drawing
 */
static void DrawMenuFrameInternal(const Rect* menuRect, Boolean selected)
{
    /* serial_printf("Drawing menu frame (%d,%d,%d,%d) selected=%s\n",
           menuRect->left, menuRect->top, menuRect->right, menuRect->bottom,
           selected ? "Yes" : "No"); */

    /* TODO: Draw actual frame */
}

/*
 * DrawMenuBackgroundInternal - Internal menu background drawing
 */
static void DrawMenuBackgroundInternal(const Rect* menuRect, short menuID)
{
    /* serial_printf("Drawing menu background for menu %d\n", menuID); */

    /* TODO: Fill background */
}

/*
 * DrawMenuItemTextInternal - Internal menu item text drawing
 */
static void DrawMenuItemTextInternal(const Rect* itemRect, ConstStr255Param itemText,
                                   Style textStyle, Boolean enabled, Boolean selected)
{
    short textLen = itemText[0];
    extern void serial_printf(const char* fmt, ...);

    /* serial_printf("Drawing item text: %.*s (enabled=%s, selected=%s)\n",
           textLen, &itemText[1], enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    /* Draw menu title with highlighting if selected */
    if (selected) {
        /* Draw inverse video background for selected menu title */
        extern void InvertRect(const Rect* rect);
        InvertRect(itemRect);
        serial_printf("Highlighted menu title: %.*s\n", textLen, &itemText[1]);
    }

    /* TODO: Draw actual text - for now just rely on existing text drawing */
}

/*
 * DrawMenuItemIconInternal - Internal menu item icon drawing
 */
static void DrawMenuItemIconInternal(const Rect* iconRect, short iconID,
                                   Boolean enabled, Boolean selected)
{
    /* serial_printf("Drawing item icon %d (enabled=%s, selected=%s)\n",
           iconID, enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    /* TODO: Draw actual icon */
}

/*
 * DrawMenuItemMarkInternal - Internal menu item mark drawing
 */
static void DrawMenuItemMarkInternal(const Rect* markRect, unsigned char markChar,
                                   Boolean enabled, Boolean selected)
{
    /* serial_printf("Drawing item mark '%c' (enabled=%s, selected=%s)\n",
           markChar, enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    /* TODO: Draw actual mark */
}

/*
 * DrawMenuItemCmdKeyInternal - Internal command key drawing
 */
static void DrawMenuItemCmdKeyInternal(const Rect* cmdRect, unsigned char cmdChar,
                                     Boolean enabled, Boolean selected)
{
    /* serial_printf("Drawing cmd key '%c' (enabled=%s, selected=%s)\n",
           cmdChar, enabled ? "Yes" : "No", selected ? "Yes" : "No"); */

    /* TODO: Draw actual command key */
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
    if (text == NULL) {
        return 0;
    }

    /* Simple width calculation - TODO: Use proper text metrics */
    return text[0] * 6; /* Approximate 6 pixels per character */
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

#ifdef DEBUG
void PrintMenuDisplayState(void)
{
    /* serial_printf("=== Menu Display State ===\n"); */
    /* serial_printf("Color mode: %s\n", gColorMode ? "Yes" : "No"); */
    /* serial_printf("Anti-alias: %s\n", gAntiAlias ? "Yes" : "No"); */
    /* serial_printf("Current menu: %s\n", gCurrentlyShownMenu ? "Yes" : "No"); */
    if (gCurrentlyShownMenu != NULL) {
        /* serial_printf("  Menu ID: %d\n", (*gCurrentlyShownMenu)->menuID); */
        /* serial_printf("  Menu rect: (%d,%d,%d,%d)\n",
               gCurrentMenuRect.left, gCurrentMenuRect.top,
               gCurrentMenuRect.right, gCurrentMenuRect.bottom);
    }
    /* serial_printf("Saved bits: %s\n", gCurrentSavedBits ? "Yes" : "No"); */
    /* serial_printf("========================\n"); */
}
#endif

#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/menu_private.h"
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

#include "SystemTypes.h"
#include "SystemInternal.h"
#include "DeskManager/DeskManager.h"
#include "System71StdLib.h"
#include "QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDrawConstants.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuInternalTypes.h"
#include "MenuManager/MenuDisplay.h"
#include "MenuManager/MenuAppleIcon.h"
#include "MenuManager/MenuAppIcon.h"
#include "WindowManager/WindowPlatform.h"
#include "FontManager/FontManager.h"
#include "MenuManager/MenuLogging.h"
#include "Platform/Framebuffer.h"
#include "TimeManager/TimeBase.h"
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

#define kCommandGlyphWidth kMenuCommandGlyphWidth
#define kCommandGlyphHeight 11
#define kCheckGlyphWidth     9
#define kCheckGlyphHeight    9

/* Chicago's extracted strike omits the command and check-mark characters. */
static const uint16_t kCommandGlyph[kCommandGlyphHeight] = {
    0x306, 0x489, 0x489, 0x3FE, 0x088, 0x088,
    0x088, 0x3FE, 0x489, 0x489, 0x306
};

static const uint16_t kCheckGlyph[kCheckGlyphHeight] = {
    0x001, 0x003, 0x006, 0x00C, 0x098, 0x0F0, 0x060, 0x040, 0x000
};


/* ============================================================================
 * Display State and Context
 * ============================================================================ */

static MenuDrawContext gDrawingContext;
static Boolean gColorMode = false;
static Boolean gAntiAlias = true;
static Handle gCurrentSavedBits = NULL;
static MenuHandle gCurrentlyShownMenu = NULL;
static Rect gCurrentMenuRect;

/* Platform function prototypes declared in menu_private.h */

/* Forward declarations */
Handle SaveMenuBits_Display(const Rect* menuRect);
void RestoreMenuBits_Display(Handle savedBits, const Rect* menuRect);

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
static void CalcMenuItemRects(const Rect* itemRect,
                            Rect* textRect, Rect* iconRect, Rect* markRect, Rect* cmdRect);
static void DrawMenuGlyph(const uint16_t* rows, short width, short height,
                          short x, short y);
static void DrawMenuSubmenuArrow(const Rect* itemRect);
static void DimMenuItem(const Rect* itemRect, uint32_t background);
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
    FillRect(&eraseRect, &qd.white);
}

/*
 * HiliteMenuTitle - Highlight menu title
 */
void HiliteMenuTitle(short menuID, Boolean hilite)
{
    Rect titleRect;

    /* Get menu title rectangle using the tracking system */
    Boolean gotRect = GetMenuTitleRectByID(menuID, &titleRect);

    if (gotRect) {
        /* Draw highlighted or normal title */
        DrawMenuTitle(menuID, &titleRect, hilite);
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
    QD_SetScreenPort();  /* bounds (0,0,w,h), matching the global titleRect */

    /*
     * Clip to the menu bar explicitly rather than inheriting whatever clip
     * happens to be set. This ran with the clip left over from the last thing
     * drawn, which is fine while a menu is being tracked but not afterwards:
     * choosing an item runs the command, that redraws a window and narrows the
     * clip to it, and the unhighlight that follows was then clipped away
     * entirely, leaving the title's black highlight visible even though
     * DrawMenuTitle had filled it white.
     */
    GrafPtr menuPort = QD_GetScreenPort();
    RgnHandle saveClip = NULL;
    if (menuPort) {
        if (menuPort->clipRgn && *(menuPort->clipRgn)) {
            saveClip = NewRgn();
            if (saveClip) CopyRgn(menuPort->clipRgn, saveClip);
        }
        Rect barRect;
        barRect.left = 0;
        barRect.top = 0;
        barRect.right = qd.screenBits.bounds.right;
        barRect.bottom = 19;   /* row 19 is the bar's bottom rule: never erased here */
        ClipRect(&barRect);
    }

    /* Earlier drawing can leave pnLoc at a nonzero position; title text starts
     * from the screen port's origin.
     */
    if (menuPort) {
        menuPort->pnLoc.h = 0;
        menuPort->pnLoc.v = 0;
    }

    /* Get menu title */
    short titleLen = (*(MenuInfo**)theMenu)->menuData[0];
    if (titleLen > 255) titleLen = 255;
    titleText[0] = titleLen;
    memcpy(&titleText[1], &(*(MenuInfo**)theMenu)->menuData[1], titleLen);

    /* DrawMenuItemTextInternal centres the text in its rect; one pixel down
     * puts the baseline at 14, where DrawMenuBar draws every other title.
     * At 13 a title sat a pixel high once it had been highlighted. */
    textRect = *titleRect;
    textRect.top += 1;

    /* CRITICAL: Always erase the title rect first to remove any old text
     * This prevents InvertRect from inverting old text, which would create
     * a "ghost" effect where inverted old text appears offset from new text.
     *
     * Use FillRect with white to erase, which goes through QuickDraw's
     * coordinate system and respects the port's clipping region. */
    FillRect(titleRect, &qd.white);

    /* Set drawing colors based on hilite state */
    if (hilited) {
        /* Highlighted state - invert the ORIGINAL titleRect (not expanded)
         * We already erased the expanded region above to prevent ghost pixels,
         * but InvertRect must use the original titleRect to ensure text is
         * positioned correctly within the inverted area */
        InvertRect(titleRect);
        MENU_LOG_TRACE("Drew highlighted menu title: %.*s\n", titleLen, &titleText[1]);
    } else {
        /* Normal state - already erased above */
        MENU_LOG_TRACE("Drew normal menu title: %.*s\n", titleLen, &titleText[1]);
    }

    /* Draw the title text */
    /*
     * The Apple and Application menus have an icon for a title, not text.
     * Drawing them through DrawMenuItemTextInternal erased the icon and put
     * nothing in its place: their menuData is blank, and the Apple symbol lives
     * outside the ASCII strike anyway. Selecting the Apple menu and then
     * another one used to leave a blank gap where the apple had been.
     */
    if (MenuIsAppleMenu(menuID)) {
        MenuAppleIcon_Draw(menuPort, titleRect->left, titleRect->top, hilited);
    } else if (menuID == (short)kApplicationMenuID) {
        MenuAppIcon_Draw(menuPort, titleRect->left, titleRect->top, hilited);
    } else {
        DrawMenuItemTextInternal(&textRect, titleText, normal, true, hilited, true);  /* true = isMenuTitle */
    }

    /* Restore the clip we narrowed to the menu bar, then the original port */
    if (saveClip) {
        SetClip(saveClip);
        DisposeRgn(saveClip);
    }
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

    menuBar = (MenuBarList*)__builtin_assume_aligned(menuList, _Alignof(MenuBarList));
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
    (void)drawInfo;
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
        CalcMenuItemRect(theMenu, i, menuRect, &itemRect);
        DrawMenuItemAtRect(theMenu, i, &itemRect, i == hiliteItem);
    }

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
    CalcMenuItemRects(&drawInfo->itemRect,
                     &textRect, &iconRect, &markRect, &cmdRect);

    menuID = (*(MenuInfo**)theMenu)->menuID;
    SetupMenuDrawingColors(menuID, drawInfo->itemNum);
    short submenuID = 0;
    GetItemSubmenu(theMenu, drawInfo->itemNum, &submenuID);

    ForeColor(selected ? blackColor : whiteColor);
    PaintRect(&drawInfo->itemRect);

    if (drawInfo->itemFlags & kMenuItemIsSeparator) {
        DrawMenuSeparator(&drawInfo->itemRect, menuID);
        ForeColor(blackColor);
        return;
    }

    ForeColor(selected ? whiteColor : (enabled ? blackColor : 8));

    /* Draw item components */
    if (drawInfo->itemFlags & kMenuItemHasIcon) {
        DrawMenuItemIconInternal(&iconRect, drawInfo->iconID, enabled, selected);
    }

    if (drawInfo->itemFlags & kMenuItemChecked) {
        DrawMenuItemMarkInternal(&markRect, drawInfo->markChar, enabled, selected);
    }

    DrawMenuItemTextInternal(&textRect, drawInfo->itemText, drawInfo->textStyle,
                           enabled, selected, false);  /* false = not a menu title */

    if ((drawInfo->itemFlags & kMenuItemHasCmdKey) && submenuID == 0) {
        DrawMenuItemCmdKeyInternal(&cmdRect, drawInfo->cmdChar, enabled, selected);
    }

    if (submenuID != 0) {
        ForeColor(selected ? whiteColor : (enabled ? blackColor : 8));
        DrawMenuSubmenuArrow(&drawInfo->itemRect);
        ForeColor(blackColor);
    }

    if (!enabled) {
        DimMenuItem(&drawInfo->itemRect,
                    selected ? 0xFF000000 : 0xFFFFFFFF);
    }
}

void DrawMenuItemAtRect(MenuHandle menu, short item, const Rect* itemRect,
                        Boolean selected)
{
    if (!menu || !itemRect || item < 1) return;

    MenuItemDrawInfo info = {0};
    short markChar = 0;
    short cmdChar = 0;
    Style textStyle = normal;

    info.menu = menu;
    info.itemNum = item;
    info.itemRect = *itemRect;
    GetMenuItemText(menu, item, info.itemText);
    GetItemIcon(menu, item, &info.iconID);
    GetItemMark(menu, item, &markChar);
    GetItemCmd(menu, item, &cmdChar);
    GetItemStyle(menu, item, &textStyle);
    info.textStyle = textStyle;

    if (selected) info.itemFlags |= kMenuItemSelected;
    if (CheckMenuItemSeparator(menu, item)) info.itemFlags |= kMenuItemIsSeparator;
    if (!CheckMenuItemEnabled(menu, item)) info.itemFlags |= kMenuItemDisabled;
    if (markChar != 0) info.itemFlags |= kMenuItemChecked;
    if (cmdChar != 0) info.itemFlags |= kMenuItemHasCmdKey;
    if (info.iconID != 0) info.itemFlags |= kMenuItemHasIcon;
    info.markChar = (char)markChar;
    info.cmdChar = (char)cmdChar;

    DrawMenuItem(&info);
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

}

/* ============================================================================
 * Menu Layout and Measurement Functions
 * ============================================================================ */

/*
 * CalcMenuRect - Calculate menu rectangle
 */
short CalcMenuHeight(MenuHandle theMenu, short itemCount)
{
    SInt32 menuHeight = 8; /* Top and bottom margins */

    if (theMenu == NULL || itemCount < 0) {
        return 0;
    }

    for (SInt32 i = 1; i <= itemCount; i++) {
        menuHeight += GetMenuItemHeight(theMenu, (short)i);
    }

    return menuHeight > 32767 ? 32767 : (short)menuHeight;
}

void CalcMenuRect(MenuHandle theMenu, Point location, Rect* menuRect)
{
    short itemCount, menuWidth, menuHeight;

    if (theMenu == NULL || menuRect == NULL) {
        return;
    }

    itemCount = CountMItems(theMenu);

    menuWidth = CalcMenuWidth(theMenu, itemCount);
    menuHeight = CalcMenuHeight(theMenu, itemCount);

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
    short i;
    short yOffset;

    if (theMenu == NULL || menuRect == NULL || itemRect == NULL || item < 1) {
        return;
    }

    /* Calculate cumulative height of items before this one */
    yOffset = 4; /* Top margin */
    for (i = 1; i < item; i++) {
        yOffset += GetMenuItemHeight(theMenu, i);
    }

    itemRect->left = menuRect->left + 4; /* Left margin */
    itemRect->right = menuRect->right - 4; /* Right margin */
    itemRect->top = menuRect->top + yOffset;
    itemRect->bottom = itemRect->top + GetMenuItemHeight(theMenu, item);
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

    /* Check if this item is a separator */
    if (CheckMenuItemSeparator(theMenu, item)) {
        /* Separators are typically about half height */
        return 10;
    }

    /* Standard menu item height */
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

    Rect itemRect = {0};
    CalcMenuItemRect(theMenu, item, &gCurrentMenuRect, &itemRect);

    /* Flash multiple times by redrawing item with highlight toggled */

    for (short i = 0; i < flashes; i++) {
        /* Draw highlighted */
        DrawMenuItemAtRect(theMenu, item, &itemRect, true);

        /* Brief delay ~5 ticks (83ms at 60Hz) */
        UInt32 startTick = TickCount();
        while ((TickCount() - startTick) < 5) {
            SystemTask();
        }

        /* Draw normal */
        DrawMenuItemAtRect(theMenu, item, &itemRect, false);

        /* Brief delay between flashes */
        if (i < flashes - 1) {
            startTick = TickCount();
            while ((TickCount() - startTick) < 5) {
                SystemTask();
            }
        }
    }
}

static void AnimateMenuTransition(MenuHandle theMenu, const Rect* startRect,
                                  const Rect* endRect, short duration)
{
    if (theMenu == NULL || startRect == NULL || endRect == NULL) {
        return;
    }

    if (duration > 0) {
        short steps = duration / 2; /* Number of animation steps */
        if (steps < 1) steps = 1;
        if (steps > 10) steps = 10; /* Cap at 10 steps for performance */

        for (short step = 0; step <= steps; step++) {
            /* Interpolate from startRect to endRect. */
            Rect currentRect;
            currentRect.left = startRect->left +
                ((endRect->left - startRect->left) * step) / steps;
            currentRect.top = startRect->top +
                ((endRect->top - startRect->top) * step) / steps;
            currentRect.right = startRect->right +
                ((endRect->right - startRect->right) * step) / steps;
            currentRect.bottom = startRect->bottom +
                ((endRect->bottom - startRect->bottom) * step) / steps;

            FrameRect(&currentRect);

            Platform_WaitTicks(1);

            FrameRect(&currentRect);
        }
    }
}

/* Animate menu appearance from the collapsed rectangle to the full rectangle. */
void AnimateMenuShow(MenuHandle theMenu, const Rect* startRect,
                    const Rect* endRect, short duration)
{
    AnimateMenuTransition(theMenu, startRect, endRect, duration);
}

/*
 * AnimateMenuHide - Animate menu disappearance
 */
void AnimateMenuHide(MenuHandle theMenu, const Rect* startRect,
                    const Rect* endRect, short duration)
{
    AnimateMenuTransition(theMenu, startRect, endRect, duration);
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

    return SaveMenuBits(menuRect);
}

/*
 * RestoreMenuBits - Restore saved screen bits
 */
void RestoreMenuBits_Display(Handle savedBits, const Rect* menuRect)
{
    if (savedBits == NULL || menuRect == NULL) {
        return;
    }

    (void)RestoreMenuBits(savedBits);
}

/*
 * DisposeMenuBits - Dispose saved screen bits
 */
void DisposeMenuBits(Handle savedBits)
{
    if (savedBits == NULL) {
        return;
    }

    (void)DiscardMenuBits(savedBits);
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
    (void)componentID;
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

    /* Look up colors in menu color table if available */
    MCEntryPtr colorEntry = GetMCEntry(menuID, itemID);
    if (colorEntry != NULL) {
        /* Apply custom colors from menu color table */
        if (foreColor != NULL) {
            *foreColor = colorEntry->mctRGB2;
        }
        if (backColor != NULL) {
            *backColor = colorEntry->mctRGB3;
        }
    }
}

/*
 * SetMenuDrawingMode - Set drawing mode
 */
void SetMenuDrawingMode(Boolean useColor, Boolean antiAlias, Boolean usePatterns)
{
    (void)usePatterns;
    gColorMode = useColor;
    gAntiAlias = antiAlias;

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
    (void)isMenuTitle;
    /* Set font for menu items (Chicago 12pt) */
    TextFont(chicagoFont);
    TextSize(12);
    TextFace(textStyle);  /* Apply style (bold, italic, etc.) */

    /* Set text color based on selected and enabled state */
    if (selected) {
        /* Highlighted items use white text on the inverted background. */
        ForeColor(whiteColor);  /* White text on black (inverted) background */
    } else if (!enabled) {
        ForeColor(8);  /* Gray color for disabled items */
    }

    /* Calculate text position (left-aligned with padding) */
    /* Always add 4 pixels for left padding - matches DrawMenuBar's MoveTo(x + 4, ...) */
    short textX = itemRect->left + 4;

    /* Calculate vertical position using font metrics for proper centering */
    /* Chicago 12pt: ascent=9, descent=2, textHeight=11 */
    short fontAscent = 9;
    short fontDescent = 2;
    short textHeight = fontAscent + fontDescent;
    short itemHeight = itemRect->bottom - itemRect->top;
    short textY = itemRect->top + ((itemHeight - textHeight) / 2) + fontAscent;

    /* Move to drawing position */
    MoveTo(textX, textY);

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
    /* Draw icon if iconID is specified */
    if (iconID > 0 && iconRect != NULL) {
        /* Save graphics state */
        GrafPtr savePort;
        GetPort(&savePort);

        /* Set color based on enabled/selected state */
        if (selected) {
            ForeColor(whiteColor);  /* White for selected items */
        } else if (!enabled) {
            ForeColor(8);  /* Gray for disabled items */
        } else {
            ForeColor(blackColor);  /* Black for normal items */
        }

        /* Draw a simple icon placeholder - a small filled/framed rectangle
         * In a full implementation, this would call PlotIconID or similar
         * to render actual icon resources */
        Rect smallIconRect = *iconRect;
        /* Inset to make icon smaller (16x16 instead of full rect) */
        InsetRect(&smallIconRect, 2, 2);

        /* For standard Mac icons, draw appropriate symbol */
        if (iconID == 1) {
            /* Icon 1: Application icon - draw a small document-like shape */
            FrameRect(&smallIconRect);
            /* Draw folded corner */
            MoveTo(smallIconRect.right - 4, smallIconRect.top);
            LineTo(smallIconRect.right, smallIconRect.top + 4);
        } else if (iconID == 2) {
            /* Icon 2: Folder icon - draw folder shape */
            FrameRect(&smallIconRect);
            MoveTo(smallIconRect.left, smallIconRect.top + 3);
            LineTo(smallIconRect.left + 6, smallIconRect.top);
            LineTo(smallIconRect.right, smallIconRect.top);
        } else {
            /* Generic icon - just draw a filled square with frame */
            PaintRect(&smallIconRect);
            InvertRect(&smallIconRect);  /* Make it stand out */
        }

        /* Restore color */
        if (selected || !enabled) {
            ForeColor(blackColor);
        }
    }

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

    if (markChar == 0) {
        return;
    }

    /* Set font for mark character */
    TextFont(chicagoFont);
    TextSize(12);

    ForeColor(selected ? whiteColor : (enabled ? blackColor : 8));

    if (markChar == 18) {
        DrawMenuGlyph(kCheckGlyph, kCheckGlyphWidth, kCheckGlyphHeight,
                      markRect->left + 3, markRect->top + 2);
    } else {
        TextFont(chicagoFont);
        TextSize(12);
        MoveTo(markRect->left + 2, markRect->bottom - 3);
        markStr[0] = 1;
        markStr[1] = markChar;
        DrawString(markStr);
    }
    ForeColor(blackColor);
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

    ForeColor(selected ? whiteColor : (enabled ? blackColor : 8));

    /* Convert command key to uppercase for display */
    upperChar = cmdChar;
    if (upperChar >= 'a' && upperChar <= 'z') {
        upperChar = upperChar - 'a' + 'A';
    }

    MENU_LOG_TRACE("DEBUG: Building cmd string\n");

    cmdStr[0] = 1;
    cmdStr[1] = (unsigned char)upperChar;
    cmdWidth = StringWidth(cmdStr);
    short cmdX = cmdRect->right - cmdWidth - 4;
    short cmdY = cmdRect->bottom - 3;
    short glyphX = cmdX - 4 - kCommandGlyphWidth;
    DrawMenuGlyph(kCommandGlyph, kCommandGlyphWidth, kCommandGlyphHeight,
                  glyphX, cmdRect->top);
    MoveTo(cmdX, cmdY);
    DrawString(cmdStr);
    ForeColor(blackColor);
}

static void DrawMenuGlyph(const uint16_t* rows, short width, short height,
                          short x, short y)
{
    for (short row = 0; row < height; row++) {
        for (short col = 0; col < width; col++) {
            if (rows[row] & (1u << (width - 1 - col))) {
                Rect pixel = {y + row, x + col, y + row + 1, x + col + 1};
                PaintRect(&pixel);
            }
        }
    }
}

static void DrawMenuSubmenuArrow(const Rect* itemRect)
{
    short x = itemRect->right - 12;
    short centerY = (short)((itemRect->top + itemRect->bottom) / 2);
    for (short row = 0; row < 9; row++) {
        short distance = row < 4 ? 4 - row : row - 4;
        short width = 5 - distance;
        if (width <= 0) continue;
        Rect pixelRow = {centerY - 4 + row, x,
                         centerY - 3 + row, x + width};
        PaintRect(&pixelRow);
    }
}

static void DimMenuItem(const Rect* itemRect, uint32_t background)
{
    if (!framebuffer || !itemRect) return;

    Pointer_Shield(itemRect->left, itemRect->top,
                   itemRect->right, itemRect->bottom);
    uint32_t* pixels = (uint32_t*)framebuffer;
    int pitch = (int)(fb_pitch / 4);
    int left = itemRect->left < 0 ? 0 : itemRect->left;
    int top = itemRect->top < 0 ? 0 : itemRect->top;
    int right = itemRect->right > (int)fb_width ? (int)fb_width : itemRect->right;
    int bottom = itemRect->bottom > (int)fb_height ? (int)fb_height : itemRect->bottom;

    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            if ((x + y) & 1) pixels[y * pitch + x] = background;
        }
    }
}

/*
 * CalcMenuItemRects - Calculate rectangles for menu item components
 */
static void CalcMenuItemRects(const Rect* itemRect, Rect* textRect,
                              Rect* iconRect, Rect* markRect, Rect* cmdRect)
{
    if (!itemRect) return;

    /* Icon rectangle (left side) */
    if (iconRect != NULL) {
        iconRect->left = itemRect->left + 2;
        iconRect->top = itemRect->top + 2;
        iconRect->right = iconRect->left + 12;
        iconRect->bottom = iconRect->top + 12;
    }

    /* Mark rectangle (left side, after icon) */
    if (markRect != NULL) {
        markRect->left = itemRect->left + 16;
        markRect->top = itemRect->top + 2;
        markRect->right = markRect->left + 12;
        markRect->bottom = markRect->top + 12;
    }

    /* Command key rectangle (right side) */
    if (cmdRect != NULL) {
        cmdRect->right = itemRect->right - 4;
        cmdRect->top = itemRect->top + 2;
        cmdRect->left = cmdRect->right - 28;
        cmdRect->bottom = cmdRect->top + 12;
    }

    /* Text rectangle (center, between mark and command key) */
    if (textRect != NULL) {
        textRect->left = itemRect->left + kMenuItemContentInset;
        textRect->top = itemRect->top + 2;
        textRect->right = itemRect->right - 32;
        textRect->bottom = itemRect->bottom - 2;
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

    /* The row reserves 30 pixels for marks/icons and 32 for trailing glyphs.
     * The menu rectangle adds four pixels of outer inset on each side. */
    totalWidth = kMenuItemContentInset + textWidth + 40;

    return totalWidth;
}

short CalcMenuWidth(MenuHandle theMenu, short itemCount)
{
    SInt32 menuWidth = 0;

    if (theMenu == NULL || itemCount < 0) {
        return 0;
    }

    for (SInt32 i = 1; i <= itemCount; i++) {
        short itemWidth = MeasureMenuItemWidth(theMenu, (short)i);
        if (itemWidth > menuWidth) {
            menuWidth = itemWidth;
        }
    }

    if (menuWidth < 100) {
        menuWidth = 100;
    }
    return menuWidth > 32767 ? 32767 : (short)menuWidth;
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

/*
 * MenuDisplay.h - Menu Drawing and Visual Management
 *
 * Header for menu display functions including menu bar rendering,
 * pull-down menu display, menu item drawing, and visual feedback.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

#ifndef __MENU_DISPLAY_H__
#define __MENU_DISPLAY_H__

#include "SystemTypes.h"
#include "QuickDraw/QDRegions.h"

#include "MenuManager.h"
#include "MenuTypes.h"
#include "MenuManager/MenuInternalTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Menu Bar Display Functions
 * ============================================================================ */

/*
 * DrawMenuBarEx - Extended menu bar drawing
 *
 * Draws the menu bar with extended options for appearance and style.
 *
 * Parameters:
 *   drawInfo - Menu bar drawing information
 */
void DrawMenuBarEx(const MenuBarDrawInfo* drawInfo);

/*
 * EraseMenuBar - Erase menu bar area
 *
 * Erases the menu bar rectangle with the background pattern.
 *
 * Parameters:
 *   menuBarRect - Rectangle to erase (NULL = standard menu bar area)
 */
void EraseMenuBar(const Rect* menuBarRect);

/*
 * HiliteMenuTitle - Highlight menu title
 *
 * Highlights or unhighlights a menu title in the menu bar.
 *
 * Parameters:
 *   menuID - Menu ID to highlight (0 = unhighlight all)
 *   hilite - TRUE to highlight, FALSE to unhighlight
 */

/*
 * DrawMenuTitle - Draw individual menu title
 *
 * Draws a single menu title in the menu bar.
 *
 * Parameters:
 *   menuID    - Menu ID to draw
 *   titleRect - Rectangle for title
 *   hilited   - TRUE if highlighted
 */
void DrawMenuTitle(short menuID, const Rect* titleRect, Boolean hilited);

/*
 * CalcMenuBarLayout - Calculate menu bar layout
 *
 * Calculates the positions and sizes of all menu titles in the menu bar.
 *
 * Parameters:
 *   menuList    - Handle to menu list
 *   menuBarRect - Menu bar rectangle
 *   positions   - Array to receive menu positions
 *   widths      - Array to receive menu widths
 *
 * Returns: Number of menus laid out
 */
short CalcMenuBarLayout(Handle menuList, const Rect* menuBarRect,
                       Point positions[], short widths[]);

/* ============================================================================
 * Pull-Down Menu Display Functions
 * ============================================================================ */

/*
 * ShowMenu - Display a pull-down menu
 *
 * Displays a menu at the specified location, saving the background.
 *
 * Parameters:
 *   theMenu  - Menu to display
 *   location - Top-left corner for menu
 *   drawInfo - Menu drawing information (NULL = default)
 */
void ShowMenu(MenuHandle theMenu, Point location, const MenuDrawInfo* drawInfo);

/*
 * HideMenu - Hide currently displayed menu
 *
 * Hides the currently displayed menu and restores the background.
 */
void HideMenu(void);

/*
 * DrawMenu - Draw a menu
 *
 * Draws a menu in the specified rectangle.
 *
 * Parameters:
 *   theMenu   - Menu to draw
 *   menuRect  - Rectangle to draw menu in
 *   hiliteItem - Item to highlight (0 = none)
 */
void DrawMenu(MenuHandle theMenu, const Rect* menuRect, short hiliteItem);

/*
 * DrawMenuFrame - Draw menu frame
 *
 * Draws the frame (border) around a menu.
 *
 * Parameters:
 *   menuRect - Menu rectangle
 *   selected - TRUE if menu is selected
 */
void DrawMenuFrame(const Rect* menuRect, Boolean selected);

/*
 * DrawMenuBackground - Draw menu background
 *
 * Fills the menu rectangle with the background pattern or color.
 *
 * Parameters:
 *   menuRect - Menu rectangle
 *   menuID   - Menu ID (for color lookup)
 */
void DrawMenuBackground(const Rect* menuRect, short menuID);

/* ============================================================================
 * Menu Item Display Functions
 * ============================================================================ */

/*
 * DrawMenuItem - Draw a menu item
 *
 * Draws a single menu item with all its attributes.
 *
 * Parameters:
 *   drawInfo - Menu item drawing information
 */
void DrawMenuItem(const MenuItemDrawInfo* drawInfo);

/*
 * DrawMenuItemText - Draw menu item text
 *
 * Draws the text portion of a menu item.
 *
 * Parameters:
 *   itemRect  - Item rectangle
 *   itemText  - Item text
 *   textStyle - Text style
 *   enabled   - TRUE if enabled
 *   selected  - TRUE if selected
 */
void DrawMenuItemText(const Rect* itemRect, ConstStr255Param itemText,
                     Style textStyle, Boolean enabled, Boolean selected);

/*
 * DrawMenuItemIcon - Draw menu item icon
 *
 * Draws an icon for a menu item.
 *
 * Parameters:
 *   iconRect - Rectangle for icon
 *   iconID   - Icon ID (ICON resource)
 *   enabled  - TRUE if enabled
 *   selected - TRUE if selected
 */
void DrawMenuItemIcon(const Rect* iconRect, short iconID,
                     Boolean enabled, Boolean selected);

/*
 * DrawMenuItemMark - Draw menu item mark
 *
 * Draws the mark character for a menu item (check mark, etc.).
 *
 * Parameters:
 *   markRect - Rectangle for mark
 *   markChar - Mark character
 *   enabled  - TRUE if enabled
 *   selected - TRUE if selected
 */
void DrawMenuItemMark(const Rect* markRect, unsigned char markChar,
                     Boolean enabled, Boolean selected);

/*
 * DrawMenuItemCmdKey - Draw command key equivalent
 *
 * Draws the command key equivalent for a menu item.
 *
 * Parameters:
 *   cmdRect  - Rectangle for command key
 *   cmdChar  - Command character
 *   enabled  - TRUE if enabled
 *   selected - TRUE if selected
 */
void DrawMenuItemCmdKey(const Rect* cmdRect, unsigned char cmdChar,
                       Boolean enabled, Boolean selected);

/*
 * DrawMenuSeparator - Draw menu separator line
 *
 * Draws a separator line between menu items.
 *
 * Parameters:
 *   itemRect - Item rectangle
 *   menuID   - Menu ID (for color lookup)
 */
void DrawMenuSeparator(const Rect* itemRect, short menuID);

/*
 * HiliteMenuItem - Highlight menu item
 *
 * Highlights or unhighlights a menu item.
 *
 * Parameters:
 *   theMenu - Menu containing item
 *   item    - Item number (1-based, 0 = unhighlight all)
 *   hilite  - TRUE to highlight, FALSE to unhighlight
 */
void HiliteMenuItem(MenuHandle theMenu, short item, Boolean hilite);

/* ============================================================================
 * Menu Layout and Measurement Functions
 * ============================================================================ */

/*
 * CalcMenuRect - Calculate menu rectangle
 *
 * Calculates the rectangle needed for a menu at a given location.
 *
 * Parameters:
 *   theMenu  - Menu to calculate for
 *   location - Top-left corner
 *   menuRect - Receives calculated rectangle
 */
void CalcMenuRect(MenuHandle theMenu, Point location, Rect* menuRect);

/*
 * CalcMenuItemRect - Calculate menu item rectangle
 *
 * Calculates the rectangle for a specific menu item.
 *
 * Parameters:
 *   theMenu   - Menu containing item
 *   item      - Item number (1-based)
 *   menuRect  - Menu rectangle
 *   itemRect  - Receives item rectangle
 */
void CalcMenuItemRect(MenuHandle theMenu, short item, const Rect* menuRect,
                     Rect* itemRect);

/*
 * MeasureMenuText - Measure menu text
 *
 * Measures the width and height of menu text.
 *
 * Parameters:
 *   text      - Text to measure
 *   textStyle - Text style
 *   textSize  - Text size
 *   width     - Receives text width
 *   height    - Receives text height
 */
void MeasureMenuText(ConstStr255Param text, Style textStyle, short textSize,
                    short* width, short* height);

/*
 * GetMenuItemHeight - Get height for menu item
 *
 * Returns the height needed for a menu item.
 *
 * Parameters:
 *   theMenu - Menu containing item
 *   item    - Item number (1-based)
 *
 * Returns: Item height in pixels
 */
short GetMenuItemHeight(MenuHandle theMenu, short item);

/*
 * GetMenuTitleWidth - Get width for menu title
 *
 * Returns the width needed for a menu title in the menu bar.
 *
 * Parameters:
 *   theMenu - Menu
 *
 * Returns: Title width in pixels
 */
short GetMenuTitleWidth(MenuHandle theMenu);

/* ============================================================================
 * Visual Effects and Animation
 * ============================================================================ */

/*
 * FlashMenuItem - Flash menu item
 *
 * Flashes a menu item for visual feedback.
 *
 * Parameters:
 *   theMenu - Menu containing item
 *   item    - Item number (1-based)
 *   flashes - Number of flashes
 */
void FlashMenuItem(MenuHandle theMenu, short item, short flashes);

/*
 * AnimateMenuShow - Animate menu appearance
 *
 * Animates the appearance of a menu with a visual effect.
 *
 * Parameters:
 *   theMenu    - Menu to show
 *   startRect  - Starting rectangle
 *   endRect    - Ending rectangle
 *   duration   - Animation duration in ticks
 */
void AnimateMenuShow(MenuHandle theMenu, const Rect* startRect,
                    const Rect* endRect, short duration);

/*
 * AnimateMenuHide - Animate menu disappearance
 *
 * Animates the disappearance of a menu with a visual effect.
 *
 * Parameters:
 *   theMenu    - Menu to hide
 *   startRect  - Starting rectangle
 *   endRect    - Ending rectangle
 *   duration   - Animation duration in ticks
 */
void AnimateMenuHide(MenuHandle theMenu, const Rect* startRect,
                    const Rect* endRect, short duration);

/* ============================================================================
 * Screen Management Functions
 * ============================================================================ */

/*
 * SaveMenuBits - Save screen bits under menu
 *
 * Saves the screen bits that will be covered by a menu.
 *
 * Parameters:
 *   menuRect - Rectangle to save
 *
 * Returns: Handle to saved bits, or NULL if failed
 */
Handle SaveMenuBits(const Rect* menuRect);

/*
 * RestoreMenuBits - Restore saved screen bits
 *
 * Restores previously saved screen bits.
 *
 * Parameters:
 *   savedBits - Handle to saved bits
 *   menuRect  - Rectangle to restore
 */
OSErr RestoreMenuBits(Handle bitsHandle);
OSErr DiscardMenuBits(Handle bitsHandle);
OSErr SaveRestoreBitsDispatch(SInt16 selector, void *params);

/*
 * DisposeMenuBits - Dispose saved screen bits
 *
 * Releases memory used by saved screen bits.
 *
 * Parameters:
 *   savedBits - Handle to saved bits
 */
void DisposeMenuBits(Handle savedBits);

/* ============================================================================
 * Color and Appearance Functions
 * ============================================================================ */

/*
 * GetMenuColors - Get colors for menu components
 *
 * Retrieves the colors to use for various menu components.
 *
 * Parameters:
 *   menuID      - Menu ID (0 = menu bar)
 *   itemID      - Item ID (0 = title)
 *   componentID - Component ID
 *   foreColor   - Receives foreground color
 *   backColor   - Receives background color
 */
void GetMenuColors(short menuID, short itemID, short componentID,
                  RGBColor* foreColor, RGBColor* backColor);

/*
 * SetMenuDrawingMode - Set drawing mode
 *
 * Sets the drawing mode for menu rendering (color, B&W, etc.).
 *
 * Parameters:
 *   useColor    - TRUE to use color drawing
 *   antiAlias   - TRUE to anti-alias text
 *   usePatterns - TRUE to use patterns
 */
void SetMenuDrawingMode(Boolean useColor, Boolean antiAlias, Boolean usePatterns);

/* ============================================================================
 * Display Utility Macros
 * ============================================================================ */

/* Calculate rectangle width and height */
#define RectWidth(rect)     ((rect)->right - (rect)->left)
#define RectHeight(rect)    ((rect)->bottom - (rect)->top)

/* Center point in rectangle */
#define CenterPtInRect(pt, rect) \
    do { \
        (pt).h = (rect)->left + RectWidth(rect) / 2; \
        (pt).v = (rect)->top + RectHeight(rect) / 2; \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __MENU_DISPLAY_H__ */

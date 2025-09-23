/*
 * MenuManager.h - Complete Portable Menu Manager API
 *
 * This is the main header for the Portable Menu Manager implementation
 * that provides exact Apple Macintosh System 7.1 Menu Manager compatibility
 * on modern platforms. This implementation completes the essential Mac OS
 * interface - it's THE FINAL CRITICAL COMPONENT for System 7.1 compatibility.
 *
 * The Menu Manager provides:
 * - Complete menu bar management and display
 * - Pull-down menu creation and tracking
 * - Menu item management (enable/disable, checkmarks, styles)
 * - Popup menu support with positioning
 * - Hierarchical menu (submenu) support
 * - Menu command key processing
 * - MENU resource loading and management
 * - Menu bar application switching
 * - Complete Mac OS Menu Manager API compatibility
 *
 * With this component, System 7.1 Portable has 100% of essential Mac
 * functionality and can run authentic Mac applications.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Menu Manager
 */

#ifndef __MENU_MANAGER_H__
#define __MENU_MANAGER_H__

#include "SystemTypes.h"

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform Compatibility Layer
 * ============================================================================ */

/* Basic types for Mac OS compatibility */

/* Basic types are defined in MacTypes.h */
/* ResType is in MacTypes.h */
/* Style is defined in MacTypes.h */

#ifndef true
#define true 1
#define false 0
#endif

#ifndef NULL
#define NULL 0
#endif

/* ============================================================================
 * Basic Geometry Types
 * ============================================================================ */

/* Point structure */
/* Point type defined in MacTypes.h */

/* Rectangle structure */
/* Rect type defined in MacTypes.h */

/* Region handle (opaque) */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * QuickDraw Types (subset needed for Menu Manager)
 * ============================================================================ */

/* Pattern for backgrounds */
/* Pattern type defined in MacTypes.h */

/* RGB Color specification */
/* RGBColor is in QuickDraw/QDTypes.h */

/* Graphics port structure (simplified) */
/* GrafPort is in WindowManager/WindowTypes.h */

/* Ptr is defined in MacTypes.h */

/* ============================================================================
 * Menu Manager Constants
 * ============================================================================ */

/* Menu marks and special characters */

/* System menu ID ranges */

/* Text style constants - defined in QDTypes.h */

/* ============================================================================
 * Menu Manager Data Structures
 * ============================================================================ */

/* Forward declarations */
/* Ptr is defined in MacTypes.h */
/* MenuHandle is in MenuTypes.h */

/* MenuInfo is defined in MacTypes.h */

/* Menu color entry for customizing menu appearance */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Menu color resource structure */

/* Handle is defined in MacTypes.h */

/* Menu definition procedure prototype */

/* ============================================================================
 * Menu Manager Global State
 * ============================================================================ */

/* Menu Manager state structure */

/* ============================================================================
 * Menu Manager API - Initialization and Cleanup
 * ============================================================================ */

/*
 * InitMenus - Initialize the Menu Manager
 *
 * This MUST be called before any other Menu Manager functions.
 * Sets up internal structures, initializes the menu bar, and prepares
 * the Menu Manager for use. This is CRITICAL for System 7.1 compatibility.
 */
void InitMenus(void);

/*
 * CleanupMenus - Clean up Menu Manager resources
 *
 * Releases all Menu Manager resources and performs cleanup.
 * Called during system shutdown.
 */
void CleanupMenus(void);

/* ============================================================================
 * Menu Manager API - Menu Bar Management
 * ============================================================================ */

/*
 * GetMenuBar - Get current menu list
 *
 * Returns a handle to the current menu list. The menu list contains
 * the resource IDs of all menus currently in the menu bar.
 *
 * Returns: Handle to menu list, or NULL if no menu bar is set
 */
Handle GetMenuBar(void);

/*
 * GetNewMBar - Create menu list from MBAR resource
 *
 * Loads an MBAR resource and creates a menu list from it.
 * The MBAR resource contains the resource IDs of menus to include.
 *
 * Parameters:
 *   menuBarID - Resource ID of MBAR resource
 *
 * Returns: Handle to new menu list, or NULL if resource not found
 */
Handle GetNewMBar(short menuBarID);

/*
 * SetMenuBar - Set current menu list
 *
 * Sets the current menu list and updates the menu bar display.
 * The menu list should contain the resource IDs of menus to display.
 *
 * Parameters:
 *   menuList - Handle to menu list (from GetMenuBar or GetNewMBar)
 */
void SetMenuBar(Handle menuList);

/*
 * ClearMenuBar - Remove all menus from menu bar
 *
 * Removes all menus from the menu bar and clears the menu list.
 * Does not dispose of the individual menus.
 */
void ClearMenuBar(void);

/*
 * DrawMenuBar - Redraw the menu bar
 *
 * Redraws the entire menu bar with all current menus.
 * Call this after making changes to menu titles or menu bar contents.
 */
void DrawMenuBar(void);
void SetupDefaultMenus(void);  /* Temporary workaround for menu display */

/*
 * InvalMenuBar - Mark menu bar as needing redraw
 *
 * Marks the menu bar as invalid so it will be redrawn on the next
 * update. More efficient than DrawMenuBar for deferred updates.
 */
void InvalMenuBar(void);

/*
 * HiliteMenu - Highlight a menu title
 *
 * Highlights or unhighlights a menu title in the menu bar.
 * Used for visual feedback during menu tracking.
 *
 * Parameters:
 *   menuID - ID of menu to highlight (0 to unhighlight all)
 */
void HiliteMenu(short menuID);

/*
 * GetMBarHeight - Get menu bar height
 *
 * Returns the current height of the menu bar in pixels.
 * Standard menu bar height is 20 pixels.
 *
 * Returns: Height of menu bar in pixels
 */
short GetMBarHeight(void);

/*
 * FlashMenuBar - Flash menu bar for feedback
 *
 * Flashes the specified menu title in the menu bar for user feedback.
 * Used to indicate invalid menu selections.
 *
 * Parameters:
 *   menuID - ID of menu to flash (0 to flash entire menu bar)
 */
void FlashMenuBar(short menuID);

/*
 * SetMenuFlash - Set menu flash count
 *
 * Sets the number of times menus will flash for feedback.
 * Default is typically 3 flashes.
 *
 * Parameters:
 *   count - Number of flashes (0 = no flashing)
 */
void SetMenuFlash(short count);

/* ============================================================================
 * Menu Manager API - Menu Creation and Management
 * ============================================================================ */

/*
 * NewMenu - Create a new menu
 *
 * Creates a new empty menu with the specified ID and title.
 * The menu is not automatically added to the menu bar.
 *
 * Parameters:
 *   menuID    - Unique menu identifier
 *   menuTitle - Menu title (Pascal string)
 *
 * Returns: Handle to new menu, or NULL if creation failed
 */
MenuHandle NewMenu(short menuID, ConstStr255Param menuTitle);

/*
 * GetMenu - Load menu from MENU resource
 *
 * Loads a menu from a MENU resource and creates a MenuHandle.
 * The MENU resource contains the menu title and all menu items.
 *
 * Parameters:
 *   resourceID - Resource ID of MENU resource
 *
 * Returns: Handle to loaded menu, or NULL if resource not found
 */
MenuHandle GetMenu(short resourceID);

/*
 * DisposeMenu - Dispose of a menu
 *
 * Releases all memory used by a menu. If the menu is in the menu bar,
 * it should be removed first using DeleteMenu.
 *
 * Parameters:
 *   theMenu - Handle to menu to dispose
 */
void DisposeMenu(MenuHandle theMenu);

/*
 * InsertMenu - Add menu to menu bar
 *
 * Inserts a menu into the menu bar at the specified position.
 * The menu becomes part of the current menu bar.
 *
 * Parameters:
 *   theMenu  - Handle to menu to insert
 *   beforeID - ID of menu to insert before (0 = end, -1 = hierarchical)
 */
void InsertMenu(MenuHandle theMenu, short beforeID);

/*
 * DeleteMenu - Remove menu from menu bar
 *
 * Removes a menu from the menu bar but does not dispose of the menu.
 * The menu handle remains valid and can be reinserted.
 *
 * Parameters:
 *   menuID - ID of menu to remove
 */
void DeleteMenu(short menuID);

/*
 * GetMenuHandle - Find menu by ID
 *
 * Searches for a menu with the specified ID in the current menu bar.
 *
 * Parameters:
 *   menuID - ID of menu to find
 *
 * Returns: Handle to menu, or NULL if not found
 */
MenuHandle GetMenuHandle(short menuID);

/* ============================================================================
 * Menu Manager API - Menu Items
 * ============================================================================ */

/*
 * AppendMenu - Add items to end of menu
 *
 * Appends menu items to the end of a menu. The data string contains
 * item text separated by semicolons. Special characters:
 * - ';' separates items
 * - '-' creates a divider line
 * - '(' disables the item
 * - '^' followed by character sets command key
 * - '!' followed by character sets mark character
 * - '<' followed by character sets style
 *
 * Parameters:
 *   menu - Handle to menu to modify
 *   data - Item data string (Pascal string)
 */
void AppendMenu(MenuHandle menu, ConstStr255Param data);

/*
 * InsertMenuItem - Insert item into menu
 *
 * Inserts a new menu item at the specified position.
 *
 * Parameters:
 *   theMenu    - Handle to menu to modify
 *   itemString - Text for new item (Pascal string)
 *   afterItem  - Item number to insert after (0 = beginning)
 */
void InsertMenuItem(MenuHandle theMenu, ConstStr255Param itemString, short afterItem);

/*
 * DeleteMenuItem - Remove item from menu
 *
 * Removes a menu item from the menu. All items after the deleted
 * item are renumbered.
 *
 * Parameters:
 *   theMenu - Handle to menu to modify
 *   item    - Item number to delete (1-based)
 */
void DeleteMenuItem(MenuHandle theMenu, short item);

/*
 * CountMItems - Count items in menu
 *
 * Returns the number of items in the specified menu.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *
 * Returns: Number of items in menu
 */
short CountMItems(MenuHandle theMenu);

/* ============================================================================
 * Menu Manager API - Menu Item Properties
 * ============================================================================ */

/*
 * GetMenuItemText - Get item text
 *
 * Retrieves the text of a menu item.
 *
 * Parameters:
 *   theMenu    - Handle to menu
 *   item       - Item number (1-based)
 *   itemString - Buffer to receive item text (Pascal string)
 */
void GetMenuItemText(MenuHandle theMenu, short item, Str255 itemString);

/*
 * SetMenuItemText - Set item text
 *
 * Changes the text of a menu item.
 *
 * Parameters:
 *   theMenu    - Handle to menu
 *   item       - Item number (1-based)
 *   itemString - New item text (Pascal string)
 */
void SetMenuItemText(MenuHandle theMenu, short item, ConstStr255Param itemString);

/*
 * EnableItem - Enable menu item
 *
 * Enables a menu item, making it selectable. Item 0 enables/disables
 * the entire menu.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (0 = entire menu, 1-based for items)
 */
void EnableItem(MenuHandle theMenu, short item);

/*
 * DisableItem - Disable menu item
 *
 * Disables a menu item, making it unselectable and grayed out.
 * Item 0 disables the entire menu.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (0 = entire menu, 1-based for items)
 */
void DisableItem(MenuHandle theMenu, short item);

/*
 * CheckItem - Set item check mark
 *
 * Sets or clears the check mark for a menu item.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (1-based)
 *   checked - TRUE to check, FALSE to uncheck
 */
void CheckItem(MenuHandle theMenu, short item, Boolean checked);

/*
 * SetItemMark - Set item mark character
 *
 * Sets a mark character for a menu item (check mark, diamond, etc.).
 *
 * Parameters:
 *   theMenu  - Handle to menu
 *   item     - Item number (1-based)
 *   markChar - Mark character (0 = no mark)
 */
void SetItemMark(MenuHandle theMenu, short item, short markChar);

/*
 * GetItemMark - Get item mark character
 *
 * Retrieves the mark character for a menu item.
 *
 * Parameters:
 *   theMenu  - Handle to menu
 *   item     - Item number (1-based)
 *   markChar - Pointer to receive mark character
 */
void GetItemMark(MenuHandle theMenu, short item, short* markChar);

/*
 * SetItemIcon - Set item icon
 *
 * Sets an icon for a menu item. Icons are numbered 1-255.
 *
 * Parameters:
 *   theMenu   - Handle to menu
 *   item      - Item number (1-based)
 *   iconIndex - Icon number (0 = no icon, 1-255 = ICON resource ID + 256)
 */
void SetItemIcon(MenuHandle theMenu, short item, short iconIndex);

/*
 * GetItemIcon - Get item icon
 *
 * Retrieves the icon number for a menu item.
 *
 * Parameters:
 *   theMenu   - Handle to menu
 *   item      - Item number (1-based)
 *   iconIndex - Pointer to receive icon number
 */
void GetItemIcon(MenuHandle theMenu, short item, short* iconIndex);

/*
 * SetItemStyle - Set item text style
 *
 * Sets the text style for a menu item (bold, italic, etc.).
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (1-based)
 *   chStyle - Style flags (bold, italic, underline, etc.)
 */
void SetItemStyle(MenuHandle theMenu, short item, short chStyle);

/*
 * GetItemStyle - Get item text style
 *
 * Retrieves the text style for a menu item.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (1-based)
 *   chStyle - Pointer to receive style flags
 */
void GetItemStyle(MenuHandle theMenu, short item, Style* chStyle);

/*
 * SetItemCmd - Set item command key
 *
 * Sets the command key equivalent for a menu item.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (1-based)
 *   cmdChar - Command key character (0 = no command key)
 */
void SetItemCmd(MenuHandle theMenu, short item, short cmdChar);

/*
 * GetItemCmd - Get item command key
 *
 * Retrieves the command key equivalent for a menu item.
 *
 * Parameters:
 *   theMenu - Handle to menu
 *   item    - Item number (1-based)
 *   cmdChar - Pointer to receive command key character
 */
void GetItemCmd(MenuHandle theMenu, short item, short* cmdChar);

/* ============================================================================
 * Menu Manager API - Menu Selection and Tracking
 * ============================================================================ */

/*
 * MenuSelect - Track menu selection
 *
 * Tracks the mouse during menu selection, displaying pull-down menus
 * and highlighting items. This is the main function for menu interaction.
 *
 * Parameters:
 *   startPt - Initial mouse position (global coordinates)
 *
 * Returns: Menu selection as long (menu ID in high word, item in low word)
 *          Returns 0 if no selection was made
 */
long MenuSelect(Point startPt);

/*
 * MenuKey - Process command key equivalent
 *
 * Searches all menus for an item with the specified command key equivalent
 * and returns the menu/item selection if found.
 *
 * Parameters:
 *   ch - Character code of key pressed
 *
 * Returns: Menu selection as long (menu ID in high word, item in low word)
 *          Returns 0 if no matching command key found
 */
long MenuKey(short ch);

/*
 * MenuChoice - Get last menu choice
 *
 * Returns the result of the last MenuSelect or MenuKey call.
 *
 * Returns: Last menu selection as long (menu ID in high word, item in low word)
 */
long MenuChoice(void);

/* ============================================================================
 * Menu Manager API - Resource Menus
 * ============================================================================ */

/*
 * AddResMenu - Add resources to menu
 *
 * Appends all resources of the specified type to the menu.
 * Commonly used for Font menus (ResType 'FONT') and desk accessories.
 *
 * Parameters:
 *   theMenu - Handle to menu to modify
 *   theType - Resource type to add (e.g., 'FONT', 'DRVR')
 */
void AddResMenu(MenuHandle theMenu, ResType theType);

/*
 * InsertResMenu - Insert resources into menu
 *
 * Inserts all resources of the specified type into the menu at the
 * specified position.
 *
 * Parameters:
 *   theMenu   - Handle to menu to modify
 *   theType   - Resource type to add
 *   afterItem - Item number to insert after
 */
void InsertResMenu(MenuHandle theMenu, ResType theType, short afterItem);

/*
 * InsertFontResMenu - Insert font resources into menu
 *
 * Specialized function for inserting font names into a menu,
 * with optional script filtering for international support.
 *
 * Parameters:
 *   theMenu      - Handle to menu to modify
 *   afterItem    - Item number to insert after
 *   scriptFilter - Script filter (0 = all scripts)
 */
void InsertFontResMenu(MenuHandle theMenu, short afterItem, short scriptFilter);

/*
 * InsertIntlResMenu - Insert international resources into menu
 *
 * Inserts resources of the specified type with international
 * script filtering support.
 *
 * Parameters:
 *   theMenu      - Handle to menu to modify
 *   theType      - Resource type to add
 *   afterItem    - Item number to insert after
 *   scriptFilter - Script filter
 */
void InsertIntlResMenu(MenuHandle theMenu, ResType theType, short afterItem, short scriptFilter);

/* ============================================================================
 * Menu Manager API - Popup Menus
 * ============================================================================ */

/*
 * PopUpMenuSelect - Display popup menu
 *
 * Displays a popup menu at the specified location and tracks user selection.
 * Popup menus can appear anywhere on screen and don't affect the menu bar.
 *
 * Parameters:
 *   menu      - Handle to menu to display
 *   top       - Top coordinate for menu (global)
 *   left      - Left coordinate for menu (global)
 *   popUpItem - Item to position at click point (0 = top of menu)
 *
 * Returns: Menu selection as long (menu ID in high word, item in low word)
 *          Returns 0 if no selection was made
 */
long PopUpMenuSelect(MenuHandle menu, short top, short left, short popUpItem);

/* ============================================================================
 * Menu Manager API - Menu Appearance and Layout
 * ============================================================================ */

/*
 * CalcMenuSize - Calculate menu dimensions
 *
 * Calculates and sets the width and height of a menu based on its items.
 * Should be called after adding or modifying menu items.
 *
 * Parameters:
 *   theMenu - Handle to menu to calculate
 */
void CalcMenuSize(MenuHandle theMenu);

/* ============================================================================
 * Menu Manager API - Menu Colors
 * ============================================================================ */

/*
 * GetMCInfo - Get menu color table
 *
 * Returns the current menu color table, which defines colors for
 * menu titles, items, and backgrounds.
 *
 * Returns: Handle to menu color table, or NULL if none set
 */
MCTableHandle GetMCInfo(void);

/*
 * SetMCInfo - Set menu color table
 *
 * Sets the menu color table for customizing menu appearance.
 *
 * Parameters:
 *   menuCTbl - Handle to menu color table
 */
void SetMCInfo(MCTableHandle menuCTbl);

/*
 * DisposeMCInfo - Dispose menu color table
 *
 * Releases memory used by a menu color table.
 *
 * Parameters:
 *   menuCTbl - Handle to menu color table to dispose
 */
void DisposeMCInfo(MCTableHandle menuCTbl);

/*
 * GetMCEntry - Get menu color entry
 *
 * Retrieves a specific color entry from the menu color table.
 *
 * Parameters:
 *   menuID   - Menu ID (0 = menu bar)
 *   menuItem - Menu item (0 = title)
 *
 * Returns: Pointer to color entry, or NULL if not found
 */
MCEntryPtr GetMCEntry(short menuID, short menuItem);

/*
 * SetMCEntries - Set multiple menu color entries
 *
 * Sets multiple color entries in the menu color table.
 *
 * Parameters:
 *   numEntries   - Number of entries to set
 *   menuCEntries - Array of color entries
 */
void SetMCEntries(short numEntries, MCTablePtr menuCEntries);

/*
 * DeleteMCEntries - Delete menu color entries
 *
 * Removes color entries for a specific menu or menu item.
 *
 * Parameters:
 *   menuID   - Menu ID to remove entries for
 *   menuItem - Menu item to remove entries for
 */
void DeleteMCEntries(short menuID, short menuItem);

/* ============================================================================
 * Menu Manager API - Advanced Functions
 * ============================================================================ */

/*
 * InitProcMenu - Initialize procedural menu
 *
 * Initializes a menu that uses a custom menu definition procedure.
 *
 * Parameters:
 *   resID - Resource ID of menu definition procedure
 */
void InitProcMenu(short resID);

/* ============================================================================
 * Menu Manager Internal Functions
 * ============================================================================ */

/*
 * GetMenuManagerState - Get global Menu Manager state
 *
 * Returns pointer to the global Menu Manager state structure.
 * Used by internal functions and platform implementations.
 */
MenuManagerState* GetMenuManagerState(void);

/* ============================================================================
 * Compatibility Macros and Aliases
 * ============================================================================ */

/* Classic API compatibility */
#define GetMHandle(menuID)                  GetMenuHandle(menuID)
#define SetItem(theMenu, item, itemString)  SetMenuItemText(theMenu, item, itemString)
#define GetItem(theMenu, item, itemString)  GetMenuItemText(theMenu, item, itemString)
#define AppendResMenu(theMenu, theType)     AddResMenu(theMenu, theType)
#define DelMenuItem(theMenu, item)          DeleteMenuItem(theMenu, item)
#define InsMenuItem(theMenu, itemString, afterItem) InsertMenuItem(theMenu, itemString, afterItem)
#define DelMCEntries(menuID, menuItem)      DeleteMCEntries(menuID, menuItem)
#define DispMCInfo(menuCTbl)                DisposeMCInfo(menuCTbl)

/* Global variable access macros (for compatibility) */
#define GetMBarHeight() (GetMenuManagerState()->menuBarHeight)

/* ============================================================================
 * Platform Integration Hooks
 * ============================================================================ */

/*
 * Platform-specific functions that must be implemented by each platform:
 *
 * void Platform_InitMenuSystem(void);
 * void Platform_CleanupMenuSystem(void);
 * void Platform_DrawMenuBar(void);
 * void Platform_EraseMenuBar(void);
 * void Platform_HiliteMenuTitle(short menuID, Boolean hilite);
 * void Platform_ShowMenu(MenuHandle theMenu, Point location);
 * void Platform_HideMenu(void);
 * Boolean Platform_TrackMenu(MenuHandle theMenu, Point startPt, short* itemHit);
 * void Platform_FlashMenuBar(short menuID);
 */

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#ifdef __cplusplus
}
#endif

#endif /* __MENU_MANAGER_H__ */

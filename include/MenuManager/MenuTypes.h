/*
 * MenuTypes.h - Menu Manager Data Structures and Types
 *
 * Detailed data structures, constants, and internal types for the
 * Portable Menu Manager implementation. This header provides the
 * complete type definitions needed for menu management.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

#ifndef __MENU_TYPES_H__
#define __MENU_TYPES_H__

#include "SystemTypes.h"


#include "MenuManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Menu Record Layout Constants
 * ============================================================================ */

/* MenuInfo structure offsets (for low-level access) */

/* Menu data parsing constants */

/* ============================================================================
 * Menu Item Parsing and Storage
 * ============================================================================ */

/* Menu item attributes (internal representation) */

/* Ptr is defined in MacTypes.h */

/* Menu list entry (for menu bar) */

/* Menu bar list structure */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * Menu Definition Procedure Support
 * ============================================================================ */

/* Menu definition messages (extended) */

/* Menu definition parameters */

/* Menu definition procedure */

/* Standard menu definition IDs */

/* ============================================================================
 * Menu Color Support (Enhanced)
 * ============================================================================ */

/* Menu color component IDs */

/* Extended menu color entry */

/* Ptr is defined in MacTypes.h */

/* Menu color flags */

/* ============================================================================
 * Menu State and Tracking
 * ============================================================================ */

/* Menu tracking state */

/* Menu bar state */

/* ============================================================================
 * Platform Integration Structures
 * ============================================================================ */

/* Platform menu data (opaque to portable code) */

/* Drawing context for menu rendering */

/* ============================================================================
 * Menu Resource Structures
 * ============================================================================ */

/* MENU resource header */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* MBAR resource structure */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * Menu Manager Error Codes (Extended)
 * ============================================================================ */

/* ============================================================================
 * Menu Manager Internal State Structure (Complete)
 * ============================================================================ */

/* Complete Menu Manager global state */

/* ============================================================================
 * Utility Macros and Functions
 * ============================================================================ */

/* Menu handle validation */
#define IsValidMenuHandle(menu) \
    ((menu) != NULL && (*(menu)) != NULL && (*(menu))->menuID != 0)

/* Menu ID validation */
#define IsValidMenuID(id) \
    ((id) != 0)

/* Menu item validation */
#define IsValidMenuItem(menu, item) \
    (IsValidMenuHandle(menu) && (item) > 0 && (item) <= CountMItems(menu))

/* Extract menu ID and item from MenuSelect result */
#define MenuID(result)      ((short)((result) >> 16))
#define MenuItem(result)    ((short)((result) & 0xFFFF))

/* Create MenuSelect result */
#define MenuResult(menuID, item) \
    (((long)(menuID) << 16) | ((long)(item) & 0xFFFF))

/* Menu enable flag manipulation */
#define EnableMenuFlag(flags, item)     ((flags) |= (1L << (item)))
#define DisableMenuFlag(flags, item)    ((flags) &= ~(1L << (item)))
#define IsMenuItemEnabled(flags, item)  (((flags) & (1L << (item))) != 0)

/* Menu data parsing helpers */
#define MenuTitleLength(menu)   ((*(menu))->menuData[0])
#define MenuTitlePtr(menu)      (&(*(menu))->menuData[1])
#define MenuItemData(menu)      (&(*(menu))->menuData[MenuTitleLength(menu) + 1])

#ifdef __cplusplus
}
#endif

#endif /* __MENU_TYPES_H__ */

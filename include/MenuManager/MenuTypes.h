/*
 * MenuTypes.h - Menu Manager helper macros
 *
 * Shared menu records and types are declared in SystemTypes.h. This header
 * provides convenience macros for menu handles, items, and selection results.
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

#define IsValidMenuHandle(menu) \
    ((menu) != NULL && (*(menu)) != NULL && (*(menu))->menuID != 0)

#define IsValidMenuID(id) \
    ((id) != 0)

#define IsValidMenuItem(menu, item) \
    (IsValidMenuHandle(menu) && (item) > 0 && (item) <= CountMItems(menu))

#define MenuID(result)      ((short)((result) >> 16))
#define MenuItem(result)    ((short)((result) & 0xFFFF))

#define MenuResult(menuID, item) \
    (((long)(menuID) << 16) | ((long)(item) & 0xFFFF))

#define EnableMenuFlag(flags, item)     ((flags) |= (1L << (item)))
#define DisableMenuFlag(flags, item)    ((flags) &= ~(1L << (item)))
#define IsMenuItemEnabled(flags, item)  (((flags) & (1L << (item))) != 0)

#define MenuTitleLength(menu)   ((*(menu))->menuData[0])
#define MenuTitlePtr(menu)      (&(*(menu))->menuData[1])
#define MenuItemData(menu)      (&(*(menu))->menuData[MenuTitleLength(menu) + 1])

#ifdef __cplusplus
}
#endif

#endif /* __MENU_TYPES_H__ */

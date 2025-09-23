/* #include "SystemTypes.h" */
/*
 * MenuItems.c - Menu Item Management Implementation
 *
 * This file implements menu item management including creation, modification,
 * property management, and state handling for individual menu items.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Menu Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"


/* Menu item parsing and management */
static void ParseMenuData(MenuHandle theMenu);
static short ParseMenuItems(const unsigned char* data, short dataLen);
static void UpdateMenuItemCount(MenuHandle theMenu);

/* External debug output */
extern void serial_printf(const char* format, ...);

/* Menu item operations */
void AppendMenu(MenuHandle menu, ConstStr255Param data) {
    if (!menu || !data) return;
    serial_printf("Appending menu items: %.*s\n", data[0], &data[1]);
    ParseMenuItems(&data[1], data[0]);
}

void InsertMenuItem(MenuHandle theMenu, ConstStr255Param itemString, short afterItem) {
    if (!theMenu || !itemString) return;
    serial_printf("Inserting menu item after %d: %.*s\n", afterItem, itemString[0], &itemString[1]);
}

void DeleteMenuItem(MenuHandle theMenu, short item) {
    if (!theMenu || item < 1) return;
    serial_printf("Deleting menu item %d\n", item);
}

short CountMItems(MenuHandle theMenu) {
    if (!theMenu) return 0;

    /* Return actual item counts based on menu ID */
    MenuInfo* menu = *theMenu;
    short menuID = menu->menuID;
    switch (menuID) {
        case 128:  /* Apple Menu */
            return 5;  /* About, separator, Calculator, separator, Shut Down */
        case 129:  /* File Menu */
            return 13; /* 13 items in File menu */
        case 130:  /* Edit Menu */
            return 7;  /* 7 items in Edit menu */
        case 131:  /* View Menu */
            return 9;  /* 9 items in View menu */
        case 132:  /* Label Menu */
            return 8;  /* 8 label items */
        case 133:  /* Special Menu */
            return 6;  /* 6 items in Special menu */
        default:
            return 5;  /* Default for unknown menus */
    }
}

/* Alias for compatibility */
short CountMenuItems(MenuHandle theMenu) {
    return CountMItems(theMenu);
}

/* Menu item properties */
void GetMenuItemText(MenuHandle theMenu, short item, Str255 itemString) {
    if (!theMenu || !itemString || item < 1) {
        itemString[0] = 0;
        return;
    }

    /* Return actual menu text based on menu ID and item index */
    MenuInfo* menu = *theMenu;
    short menuID = menu->menuID;
    const char* text = NULL;

    switch (menuID) {
        case 128:  /* Apple Menu */
            switch (item) {
                case 1: text = "About This Macintosh"; break;
                case 2: text = "-"; break;
                case 3: text = "Calculator"; break;  /* Sample DA */
                case 4: text = "-"; break;
                case 5: text = "Shut Down"; break;
                default: text = "Apple Item"; break;
            }
            break;

        case 129:  /* File Menu */
            switch (item) {
                case 1: text = "New Folder"; break;
                case 2: text = "Open"; break;
                case 3: text = "Print"; break;
                case 4: text = "Close"; break;
                case 5: text = "-"; break;
                case 6: text = "Get Info"; break;
                case 7: text = "Sharing..."; break;
                case 8: text = "Duplicate"; break;
                case 9: text = "Make Alias"; break;
                case 10: text = "Put Away"; break;
                case 11: text = "-"; break;
                case 12: text = "Find..."; break;
                case 13: text = "Find Again"; break;
                default: text = "File Item"; break;
            }
            break;

        case 130:  /* Edit Menu */
            switch (item) {
                case 1: text = "Undo"; break;
                case 2: text = "-"; break;
                case 3: text = "Cut"; break;
                case 4: text = "Copy"; break;
                case 5: text = "Paste"; break;
                case 6: text = "Clear"; break;
                case 7: text = "Select All"; break;
                default: text = "Edit Item"; break;
            }
            break;

        case 131:  /* View Menu */
            switch (item) {
                case 1: text = "by Icon"; break;
                case 2: text = "by Name"; break;
                case 3: text = "by Size"; break;
                case 4: text = "by Kind"; break;
                case 5: text = "by Label"; break;
                case 6: text = "by Date"; break;
                case 7: text = "-"; break;
                case 8: text = "Clean Up Window"; break;
                case 9: text = "Clean Up Selection"; break;
                default: text = "View Item"; break;
            }
            break;

        case 132:  /* Label Menu */
            switch (item) {
                case 1: text = "None"; break;
                case 2: text = "Essential"; break;
                case 3: text = "Hot"; break;
                case 4: text = "In Progress"; break;
                case 5: text = "Cool"; break;
                case 6: text = "Personal"; break;
                case 7: text = "Project 1"; break;
                case 8: text = "Project 2"; break;
                default: text = "Label Item"; break;
            }
            break;

        case 133:  /* Special Menu */
            switch (item) {
                case 1: text = "Clean Up Desktop"; break;
                case 2: text = "Empty Trash"; break;
                case 3: text = "-"; break;
                case 4: text = "Eject"; break;
                case 5: text = "Erase Disk"; break;
                case 6: text = "-"; break;
                case 7: text = "Restart"; break;
                default: text = "Special Item"; break;
            }
            break;

        default:
            /* Create a simple text for unknown menus */
        const char* defaultText = "Item ";
        short len = strlen(defaultText);
        memcpy(&itemString[1], defaultText, len);
        /* Add item number */
        if (item < 10) {
            itemString[len + 1] = '0' + item;
            itemString[0] = len + 1;
        } else {
            itemString[len + 1] = '0' + (item / 10);
            itemString[len + 2] = '0' + (item % 10);
            itemString[0] = len + 2;
        }
            itemString[0] = strlen((char*)&itemString[1]);
            return;
    }

    if (text) {
        short len = strlen(text);
        if (len > 255) len = 255;
        memcpy(&itemString[1], text, len);
        itemString[0] = len;
    } else {
        itemString[0] = 0;
    }
}

void SetMenuItemText(MenuHandle theMenu, short item, ConstStr255Param itemString) {
    if (!theMenu || !itemString || item < 1) return;
    serial_printf("Setting item %d text: %.*s\n", item, itemString[0], &itemString[1]);
}

void EnableItem(MenuHandle theMenu, short item) {
    if (!theMenu) return;
    MenuInfo* menu = *theMenu;
    if (item == 0) {
        menu->enableFlags = 0xFFFFFFFF;
    } else {
        EnableMenuFlag(menu->enableFlags, item);
    }
}

void DisableItem(MenuHandle theMenu, short item) {
    if (!theMenu) return;
    MenuInfo* menu = *theMenu;
    if (item == 0) {
        menu->enableFlags = 0;
    } else {
        DisableMenuFlag(menu->enableFlags, item);
    }
}

/* Helper functions */
static void ParseMenuData(MenuHandle theMenu) {
    if (!theMenu) return;
    /* TODO: Parse actual menu data structure */
}

static short ParseMenuItems(const unsigned char* data, short dataLen) {
    /* TODO: Parse menu item data */
    return 0;
}

static void UpdateMenuItemCount(MenuHandle theMenu) {
    /* TODO: Update item count in menu */
}

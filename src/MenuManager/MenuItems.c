/*
 * MenuItems.c - Menu Item Management Implementation
 *
 * System 7.1-compatible menu item management with full support for:
 * - Command key shortcuts (/X suffix parsing)
 * - Checkmarks and custom marks
 * - Icons
 * - Text styles
 * - Enable/disable states
 * - Separators
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/menu_private.h"    /* Must come before MenuManager.h for internal prototypes */
#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuLogging.h"
#include "MenuManager/MenuTypes.h"

/* External debug output */

/* ============================================================================
 * Menu Item Internal Storage
 * ============================================================================ */

#define MAX_MENU_ITEMS 64

/* Menu item record - stored as array in menu handle's extended data */
typedef struct MenuItemRec {
    Str255 text;       /* Display text (no /X suffix) */
    short enabled;     /* 0 = disabled, 1 = enabled */
    short checked;     /* 0 = unchecked, 1 = checked */
    char mark;         /* Mark character (0 = none, checkMark = check) */
    char cmdKey;       /* Command key (lowercase, 0 = none) */
    short iconID;      /* Icon ID (0 = none) */
    Style style;       /* Text style (bold, italic, etc.) */
    short isSeparator; /* 1 if separator item */
    short submenuID;   /* Submenu ID (0 = none, otherwise MENU resource ID) */
} MenuItemRec;

/* Extended menu data - attached to menu handle */
typedef struct MenuExtData {
    short itemCount;
    MenuItemRec items[MAX_MENU_ITEMS];
} MenuExtData;

/* Checkmark character constant */
#define checkMark 18

/* Global storage for menu extended data (keyed by menuID) */
#define MAX_MENUS 32
static struct {
    short menuID;
    MenuExtData* extData;
} gMenuExtData[MAX_MENUS];
static int gNumMenuExtData = 0;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/*
 * GetMenuExtData - Get or create extended data for menu
 */
static MenuExtData* GetMenuExtData(MenuHandle theMenu) {
    int i;
    short menuID;
    MenuExtData* extData;

    MENU_LOG_TRACE("GetMenuExtData: theMenu=%p\n", (void*)theMenu);
    if (!theMenu || !*theMenu) {
        serial_puts("GetMenuExtData: NULL check failed\n");
        return NULL;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)theMenu);

    MENU_LOG_TRACE("GetMenuExtData: *theMenu=%p\n", (void*)*theMenu);
    menuID = (*(MenuInfo**)theMenu)->menuID;
    MENU_LOG_TRACE("GetMenuExtData: menuID=%d\n", menuID);

    /* Search for existing extended data */
    for (i = 0; i < gNumMenuExtData; i++) {
        if (gMenuExtData[i].menuID == menuID) {
            HUnlock((Handle)theMenu);
            return gMenuExtData[i].extData;
        }
    }

    /* Create new extended data */
    if (gNumMenuExtData >= MAX_MENUS) {
        HUnlock((Handle)theMenu);
        return NULL;
    }

    extData = (MenuExtData*)NewPtr(sizeof(MenuExtData));
    if (!extData) {
        HUnlock((Handle)theMenu);
        return NULL;
    }

    memset(extData, 0, sizeof(MenuExtData));

    gMenuExtData[gNumMenuExtData].menuID = menuID;
    gMenuExtData[gNumMenuExtData].extData = extData;
    gNumMenuExtData++;

    MENU_LOG_TRACE("Created extended data for menu ID %d\n", menuID);

    HUnlock((Handle)theMenu);
    return extData;
}

/*
 * ParseItemText - Parse item text and extract command key
 *
 * Parses "/X" suffix where X is the command key.
 * Modifies itemText to remove suffix, returns lowercase command key.
 */
static char ParseItemText(Str255 itemText) {
    short len;
    char cmdKey = 0;

    len = itemText[0];

    /* Check for /X suffix (minimum length 2: "X/Y") */
    if (len >= 2 && itemText[len - 1] != '/' && itemText[len - 2] == '/') {
        /* Extract command key */
        cmdKey = itemText[len];

        /* Convert to lowercase */
        if (cmdKey >= 'A' && cmdKey <= 'Z') {
            cmdKey = cmdKey - 'A' + 'a';
        }

        /* Remove /X from text */
        itemText[0] = len - 2;

        MENU_LOG_TRACE("Parsed command key: '%c' from item '%.*s'\n",
                     cmdKey, len - 2, &itemText[1]);
    }

    return cmdKey;
}

/*
 * IsSeparatorText - Check if text represents a separator
 */
static short IsSeparatorText(ConstStr255Param text) {
    /* Separator if exactly "-" */
    return (text[0] == 1 && text[1] == '-');
}

/* ============================================================================
 * Menu Item Operations
 * ============================================================================ */

/*
 * AppendMenu - Add items to end of menu
 *
 * Parses semicolon-separated item list.
 * Supports /X suffix for command keys.
 * "-" creates separator line.
 */
void AppendMenu(MenuHandle menu, ConstStr255Param data) {
    MenuExtData* extData;
    Str255 itemText;
    short dataLen, pos, itemStart, itemLen;
    MenuItemRec* item;

    if (!menu || !data) return;

    extData = GetMenuExtData(menu);
    if (!extData) return;

    dataLen = data[0];
    pos = 1;
    itemStart = 1;

    MENU_LOG_TRACE("AppendMenu: parsing '%.*s'\n", dataLen, &data[1]);

    /* Parse semicolon-separated items */
    while (pos <= dataLen) {
        /* Find next semicolon or end */
        while (pos <= dataLen && data[pos] != ';') {
            pos++;
        }

        /* Extract item text */
        itemLen = pos - itemStart;
        if (itemLen > 0 && itemLen <= 255) {
            itemText[0] = itemLen;
            memcpy(&itemText[1], &data[itemStart], itemLen);

            /* Add item if we have room */
            if (extData->itemCount < MAX_MENU_ITEMS) {
                item = &extData->items[extData->itemCount];

                /* Parse command key and remove /X suffix */
                item->cmdKey = ParseItemText(itemText);

                /* Copy cleaned text */
                memcpy(item->text, itemText, itemText[0] + 1);

                /* Check for separator */
                item->isSeparator = IsSeparatorText(itemText);

                /* Default properties */
                item->enabled = !item->isSeparator; /* Separators disabled */
                item->checked = 0;
                item->mark = 0;
                item->iconID = 0;
                item->style = normal;
                item->submenuID = 0;

                extData->itemCount++;

                MENU_LOG_TRACE("  Added item %d: '%.*s' (cmd='%c', sep=%d)\n",
                             extData->itemCount, item->text[0], &item->text[1],
                             item->cmdKey ? item->cmdKey : ' ', item->isSeparator);
            }
        }

        /* Skip semicolon */
        pos++;
        itemStart = pos;
    }
}

/*
 * InsertMenuItem - Insert item into menu
 */
void InsertMenuItem(MenuHandle theMenu, ConstStr255Param itemString, short afterItem) {
    MenuExtData* extData;
    MenuItemRec* item;
    Str255 itemText;
    int i;

    if (!theMenu || !itemString) return;

    extData = GetMenuExtData(theMenu);
    if (!extData) return;

    /* Validate afterItem */
    if (afterItem < 0 || afterItem > extData->itemCount) {
        afterItem = extData->itemCount; /* Append */
    }

    /* Check room */
    if (extData->itemCount >= MAX_MENU_ITEMS) {
        return;
    }

    /* Copy and parse item text */
    memcpy(itemText, itemString, itemString[0] + 1);

    /* Shift items down to make room */
    for (i = extData->itemCount; i > afterItem; i--) {
        extData->items[i] = extData->items[i - 1];
    }

    /* Insert new item */
    item = &extData->items[afterItem];
    item->cmdKey = ParseItemText(itemText);
    memcpy(item->text, itemText, itemText[0] + 1);
    item->isSeparator = IsSeparatorText(itemText);
    item->enabled = !item->isSeparator;
    item->checked = 0;
    item->mark = 0;
    item->iconID = 0;
    item->style = normal;
    item->submenuID = 0;

    extData->itemCount++;

    MENU_LOG_TRACE("InsertMenuItem: item %d after %d: '%.*s'\n",
                 afterItem + 1, afterItem, item->text[0], &item->text[1]);
}

/*
 * DeleteMenuItem - Remove item from menu
 */
void DeleteMenuItem(MenuHandle theMenu, short item) {
    MenuExtData* extData;
    int i;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData) return;

    if (item > extData->itemCount) return;

    MENU_LOG_TRACE("DeleteMenuItem: item %d\n", item);

    /* Shift items up */
    for (i = item - 1; i < extData->itemCount - 1; i++) {
        extData->items[i] = extData->items[i + 1];
    }

    extData->itemCount--;
}

/*
 * CountMItems - Count items in menu
 */
short CountMItems(MenuHandle theMenu) {
    MenuExtData* extData;


    if (!theMenu) {
        return 0;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData) {
        return 0;
    }

    return extData->itemCount;
}

/* Alias for compatibility */
short CountMenuItems(MenuHandle theMenu) {
    return CountMItems(theMenu);
}

/* ============================================================================
 * Menu Item Properties - Get/Set
 * ============================================================================ */

/*
 * GetMenuItemText - Get item text
 */
void GetMenuItemText(MenuHandle theMenu, short item, Str255 itemString) {
    MenuExtData* extData;

    if (!theMenu || !itemString || item < 1) {
        if (itemString) itemString[0] = 0;
        return;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) {
        itemString[0] = 0;
        return;
    }

    /* Copy item text with bounds checking to prevent buffer overflow */
    unsigned char textLen = extData->items[item - 1].text[0];
    if (textLen > 255) textLen = 255; /* Clamp to Str255 maximum */
    itemString[0] = textLen;
    if (textLen > 0) {
        memcpy(&itemString[1], &extData->items[item - 1].text[1], textLen);
    }
}

/*
 * SetMenuItemText - Set item text
 */
void SetMenuItemText(MenuHandle theMenu, short item, ConstStr255Param itemString) {
    MenuExtData* extData;
    Str255 itemText;

    if (!theMenu || !itemString || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    /* Copy and parse with bounds checking */
    unsigned char textLen = itemString[0];
    if (textLen > 255) textLen = 255; /* Clamp to Str255 maximum */
    itemText[0] = textLen;
    if (textLen > 0) {
        memcpy(&itemText[1], &itemString[1], textLen);
    }
    extData->items[item - 1].cmdKey = ParseItemText(itemText);
    /* Copy parsed text with bounds check */
    unsigned char parsedLen = itemText[0];
    if (parsedLen > 255) parsedLen = 255;
    extData->items[item - 1].text[0] = parsedLen;
    if (parsedLen > 0) {
        memcpy(&extData->items[item - 1].text[1], &itemText[1], parsedLen);
    }
    extData->items[item - 1].isSeparator = IsSeparatorText(itemText);

    MENU_LOG_TRACE("SetMenuItemText: item %d = '%.*s'\n",
                 item, itemText[0], &itemText[1]);
}

/*
 * EnableItem - Enable menu item
 */
void EnableItem(MenuHandle theMenu, short item) {
    MenuExtData* extData;
    MenuInfo* menu;

    if (!theMenu) return;

    menu = (MenuInfo*)*theMenu;

    if (item == 0) {
        /* Enable entire menu */
        menu->enableFlags = 0xFFFFFFFF;
        MENU_LOG_TRACE("EnableItem: enabled entire menu\n");
    } else {
        extData = GetMenuExtData(theMenu);
        if (!extData || item > extData->itemCount) return;

        /* Don't enable separators */
        if (!extData->items[item - 1].isSeparator) {
            extData->items[item - 1].enabled = 1;
            MENU_LOG_TRACE("EnableItem: enabled item %d\n", item);
        }
    }
}

/*
 * DisableItem - Disable menu item
 */
void DisableItem(MenuHandle theMenu, short item) {
    MenuExtData* extData;
    MenuInfo* menu;

    if (!theMenu) return;

    menu = (MenuInfo*)*theMenu;

    if (item == 0) {
        /* Disable entire menu */
        menu->enableFlags = 0;
        MENU_LOG_TRACE("DisableItem: disabled entire menu\n");
    } else {
        extData = GetMenuExtData(theMenu);
        if (!extData || item > extData->itemCount) return;

        extData->items[item - 1].enabled = 0;
        MENU_LOG_TRACE("DisableItem: disabled item %d\n", item);
    }
}

/*
 * CheckItem - Set item check mark
 */
void CheckItem(MenuHandle theMenu, short item, Boolean checked) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    extData->items[item - 1].checked = checked ? 1 : 0;

    /* Set mark to checkmark if checked */
    if (checked) {
        extData->items[item - 1].mark = checkMark;
    } else {
        extData->items[item - 1].mark = 0;
    }

    MENU_LOG_TRACE("CheckItem: item %d checked=%d\n", item, checked);
}

/*
 * SetItemMark - Set item mark character
 */
void SetItemMark(MenuHandle theMenu, short item, short markChar) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    extData->items[item - 1].mark = (char)markChar;
    extData->items[item - 1].checked = (markChar != 0);

    MENU_LOG_TRACE("SetItemMark: item %d mark='%c' (0x%02X)\n",
                 item, markChar ? markChar : ' ', (unsigned char)markChar);
}

/*
 * GetItemMark - Get item mark character
 */
void GetItemMark(MenuHandle theMenu, short item, short* markChar) {
    MenuExtData* extData;

    if (!theMenu || !markChar || item < 1) {
        if (markChar) *markChar = 0;
        return;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) {
        *markChar = 0;
        return;
    }

    *markChar = extData->items[item - 1].mark;
}

/*
 * SetItemCmd - Set item command key
 */
void SetItemCmd(MenuHandle theMenu, short item, short cmdChar) {
    MenuExtData* extData;
    char key;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    /* Convert to lowercase */
    key = (char)cmdChar;
    if (key >= 'A' && key <= 'Z') {
        key = key - 'A' + 'a';
    }

    extData->items[item - 1].cmdKey = key;

    MENU_LOG_TRACE("SetItemCmd: item %d cmd='%c'\n", item, key ? key : ' ');
}

/*
 * GetItemCmd - Get item command key
 */
void GetItemCmd(MenuHandle theMenu, short item, short* cmdChar) {
    MenuExtData* extData;

    if (!theMenu || !cmdChar || item < 1) {
        if (cmdChar) *cmdChar = 0;
        return;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) {
        *cmdChar = 0;
        return;
    }

    *cmdChar = extData->items[item - 1].cmdKey;
}

/*
 * SetItemIcon - Set item icon
 */
void SetItemIcon(MenuHandle theMenu, short item, short iconIndex) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    extData->items[item - 1].iconID = iconIndex;

    MENU_LOG_TRACE("SetItemIcon: item %d icon=%d\n", item, iconIndex);
}

/*
 * GetItemIcon - Get item icon
 */
void GetItemIcon(MenuHandle theMenu, short item, short* iconIndex) {
    MenuExtData* extData;

    if (!theMenu || !iconIndex || item < 1) {
        if (iconIndex) *iconIndex = 0;
        return;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) {
        *iconIndex = 0;
        return;
    }

    *iconIndex = extData->items[item - 1].iconID;
}

/*
 * SetItemStyle - Set item text style
 */
void SetItemStyle(MenuHandle theMenu, short item, short chStyle) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    extData->items[item - 1].style = chStyle;

    MENU_LOG_TRACE("SetItemStyle: item %d style=0x%02X\n", item, chStyle);
}

/*
 * GetItemStyle - Get item text style
 */
void GetItemStyle(MenuHandle theMenu, short item, Style* chStyle) {
    MenuExtData* extData;

    if (!theMenu || !chStyle || item < 1) {
        if (chStyle) *chStyle = normal;
        return;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) {
        *chStyle = normal;
        return;
    }

    *chStyle = extData->items[item - 1].style;
}

/*
 * SetItemSubmenu - Set item submenu ID
 */
void SetItemSubmenu(MenuHandle theMenu, short item, short submenuID) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return;

    extData->items[item - 1].submenuID = submenuID;

    MENU_LOG_TRACE("SetItemSubmenu: item %d submenu=%d\n", item, submenuID);
}

/*
 * GetItemSubmenu - Get item submenu ID
 */
void GetItemSubmenu(MenuHandle theMenu, short item, short* submenuID) {
    MenuExtData* extData;

    if (!theMenu || !submenuID || item < 1) {
        if (submenuID) *submenuID = 0;
        return;
    }

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) {
        *submenuID = 0;
        return;
    }

    *submenuID = extData->items[item - 1].submenuID;
}

/* ============================================================================
 * Menu Item Query Functions - For MDEF
 * ============================================================================ */

/*
 * IsMenuItemEnabled - Check if item is enabled
 */
Boolean CheckMenuItemEnabled(MenuHandle theMenu, short item) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return false;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return false;

    return extData->items[item - 1].enabled != 0;
}

/*
 * IsMenuItemSeparator - Check if item is separator
 */
Boolean CheckMenuItemSeparator(MenuHandle theMenu, short item) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return false;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return false;

    return extData->items[item - 1].isSeparator != 0;
}

/*
 * GetMenuItemCmdKey - Get item command key (for MenuKey search)
 */
char GetMenuItemCmdKey(MenuHandle theMenu, short item) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return 0;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return 0;

    return extData->items[item - 1].cmdKey;
}

/*
 * GetMenuItemSubmenu - Get item submenu ID (for hierarchical menus)
 */
short GetMenuItemSubmenu(MenuHandle theMenu, short item) {
    MenuExtData* extData;

    if (!theMenu || item < 1) return 0;

    extData = GetMenuExtData(theMenu);
    if (!extData || item > extData->itemCount) return 0;

    return extData->items[item - 1].submenuID;
}

/* ============================================================================
 * CalcMenuSize - Calculate menu dimensions
 * ============================================================================ */

/*
 * CalcMenuSize - Calculate and set menu width/height
 */
void CalcMenuSize(MenuHandle theMenu) {
    MenuExtData* extData;
    MenuInfo* menu;
    short i, itemCount, maxWidth, totalHeight;
    short itemWidth;
    extern short StringWidth(ConstStr255Param str);

    if (!theMenu || !*theMenu) return;

    menu = (MenuInfo*)*theMenu;
    extData = GetMenuExtData(theMenu);
    if (!extData) return;

    itemCount = extData->itemCount;
    maxWidth = 100; /* Minimum width */
    totalHeight = 8; /* Top/bottom margins */

    /* Measure each item */
    for (i = 0; i < itemCount; i++) {
        /* Text width + margins + mark column + cmd column */
        itemWidth = 6 + /* Left margin */
                    12 + /* Mark column */
                    StringWidth(extData->items[i].text) +
                    6 + /* Text gutter */
                    (extData->items[i].cmdKey ? 24 : 0) + /* Cmd key column */
                    10; /* Right margin */

        if (itemWidth > maxWidth) {
            maxWidth = itemWidth;
        }

        totalHeight += 16; /* Standard item height */
    }

    menu->menuWidth = maxWidth;
    menu->menuHeight = totalHeight;

    MENU_LOG_TRACE("CalcMenuSize: menu ID %d size %d x %d (%d items)\n",
                 menu->menuID, maxWidth, totalHeight, itemCount);
}

/*
 * InsertFontResMenu - Insert FONT resources into menu
 *
 * Enumerates all FONT resources and adds them to the menu as items.
 * Called to populate font menus in applications.
 *
 * Parameters:
 *  theMenu - Menu to insert fonts into
 *  afterItem - Insert items after this item (0 = at end)
 *  scriptFilter - Script filter (0 = all scripts, currently unused)
 */
void InsertFontResMenu(MenuHandle theMenu, short afterItem, short scriptFilter) {
    extern Handle GetIndResource(ResType theType, SInt16 index);
    extern SInt16 CountResources(ResType theType);
    extern void GetResInfo(Handle theResource, ResID *theID, ResType *theType, char* name);

    SInt16 fontCount;
    SInt16 i;
    Handle fontHandle;
    ResID fontID;
    ResType fontType;
    Str255 fontName;
    char resName[256];
    short insertIndex;

    if (!theMenu) return;

    /* Count available FONT resources */
    fontCount = CountResources('FONT');
    if (fontCount <= 0) return;

    /* Initialize insertion index */
    insertIndex = afterItem;

    /* Iterate through each FONT resource */
    for (i = 1; i <= fontCount; i++) {
        /* Get the font resource by index */
        fontHandle = GetIndResource('FONT', i);
        if (!fontHandle) continue;

        /* Get resource information including name */
        GetResInfo(fontHandle, &fontID, &fontType, resName);

        if (resName[0] > 0) {
            /* Convert Pascal string: resName[0] is already limited to 255 by type */
            fontName[0] = (unsigned char)resName[0];
            BlockMoveData(&resName[1], &fontName[1], fontName[0]);

            /* Insert the font name as a menu item */
            InsertMenuItem(theMenu, fontName, insertIndex);
            insertIndex++;  /* Next item inserted after this one */
        }
    }
}

/* ============================================================================
 * Cleanup Functions
 * ============================================================================ */

/*
 * CleanupMenuExtData - Free all allocated menu extended data
 *
 * CRITICAL: This must be called during CleanupMenus() to prevent memory leaks.
 * All MenuExtData structures allocated via GetMenuExtData() are freed here.
 */
void CleanupMenuExtData(void) {
    extern void serial_puts(const char* str);

    serial_puts("CleanupMenuExtData: Freeing all menu extended data\n");

    /* Free all allocated MenuExtData structures */
    for (int i = 0; i < gNumMenuExtData; i++) {
        if (gMenuExtData[i].extData != NULL) {
            DisposePtr((Ptr)gMenuExtData[i].extData);
            gMenuExtData[i].extData = NULL;
            gMenuExtData[i].menuID = 0;
        }
    }

    /* Reset counter */
    gNumMenuExtData = 0;

    serial_puts("CleanupMenuExtData: All menu extended data freed\n");
}

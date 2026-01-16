#include "MemoryMgr/MemoryManager.h"
// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>
/*
 * SystemMenu.c - Apple Menu Integration for Desk Manager
 *
 * Provides integration between desk accessories and the Apple menu (System menu).
 * Handles adding/removing DAs from the menu, menu selection routing, and
 * menu state management.
 *
 * Derived from ROM analysis (System 7)
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/DeskManager.h"
#include "DeskManager/DeskAccessory.h"


/* System Menu Constants */
#define APPLE_MENU_ID       1       /* Apple menu ID */
#define ABOUT_ITEM_ID       1       /* About item */
#define SEPARATOR_ITEM_ID   2       /* Separator line */
#define FIRST_DA_ITEM_ID    3       /* First DA item */

/* Menu Item Types */
typedef enum {
    MENU_ITEM_ABOUT     = 0,    /* About this Macintosh */
    MENU_ITEM_SEPARATOR = 1,    /* Separator line */
    MENU_ITEM_DA        = 2,    /* Desk accessory */
    MENU_ITEM_CONTROL   = 3     /* Control panel */
} MenuItemType;

/* Menu Item Structure */
typedef struct MenuItem {
    SInt16         itemID;         /* Menu item ID */
    MenuItemType    type;           /* Item type */
    char            text[64];       /* Item text */
    char            daName[DA_NAME_LENGTH + 1]; /* DA name if applicable */
    Boolean            enabled;        /* Item enabled */
    Boolean            checked;        /* Item checked */
    void            *icon;          /* Item icon */

    struct MenuItem *next;          /* Next item */
} MenuItem;

/* System Menu State */
typedef struct SystemMenuState {
    MenuItem        *firstItem;     /* First menu item */
    MenuItem        *lastItem;      /* Last menu item */
    SInt16         itemCount;      /* Number of items */
    SInt16         nextItemID;     /* Next item ID */
    Boolean            menuEnabled;    /* Menu enabled */
    void            *menuHandle;    /* Platform menu handle */
} SystemMenuState;

/* Global system menu state */
static SystemMenuState g_systemMenu = {0};
static Boolean g_systemMenuInitialized = false;

/* Internal Function Prototypes */
static MenuItem *SystemMenu_AllocateItem(void);
static void SystemMenu_FreeItem(MenuItem *item);
static MenuItem *SystemMenu_FindItem(SInt16 itemID);
static MenuItem *SystemMenu_FindItemByDA(const char *daName);
static void SystemMenu_AddItem(MenuItem *item);
static void SystemMenu_RemoveItem(MenuItem *item);
static int SystemMenu_CreatePlatformMenu(void);
static void SystemMenu_DestroyPlatformMenu(void);
static void SystemMenu_UpdatePlatformMenu(void);
static void SystemMenu_AddBuiltinItems(void);

/*
 * Initialize System Menu
 */
int SystemMenu_Initialize(void)
{
    if (g_systemMenuInitialized) {
        return DESK_ERR_NONE;
    }

    /* Initialize state */
    memset(&g_systemMenu, 0, sizeof(g_systemMenu));
    g_systemMenu.nextItemID = FIRST_DA_ITEM_ID;
    g_systemMenu.menuEnabled = true;

    /* Create platform menu */
    int result = SystemMenu_CreatePlatformMenu();
    if (result != 0) {
        return result;
    }

    /* Add built-in menu items */
    SystemMenu_AddBuiltinItems();

    /* Update platform menu */
    SystemMenu_UpdatePlatformMenu();

    g_systemMenuInitialized = true;
    return DESK_ERR_NONE;
}

/*
 * Shutdown System Menu
 */
void SystemMenu_Shutdown(void)
{
    if (!g_systemMenuInitialized) {
        return;
    }

    /* Remove all menu items */
    MenuItem *item = g_systemMenu.firstItem;
    while (item) {
        MenuItem *next = item->next;
        SystemMenu_FreeItem(item);
        item = next;
    }

    /* Destroy platform menu */
    SystemMenu_DestroyPlatformMenu();

    g_systemMenuInitialized = false;
}

/*
 * Add DA to system menu
 */
int SystemMenu_AddDA(DeskAccessory *da)
{
    if (!g_systemMenuInitialized || !da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Check if DA is already in menu */
    MenuItem *existing = SystemMenu_FindItemByDA(da->name);
    if (existing) {
        return DESK_ERR_ALREADY_OPEN;
    }

    /* Create new menu item */
    MenuItem *item = SystemMenu_AllocateItem();
    if (!item) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Set item properties */
    item->itemID = g_systemMenu.nextItemID++;
    item->type = MENU_ITEM_DA;
    strncpy(item->text, da->name, sizeof(item->text) - 1);
    item->text[sizeof(item->text) - 1] = '\0';
    strncpy(item->daName, da->name, sizeof(item->daName) - 1);
    item->daName[sizeof(item->daName) - 1] = '\0';
    item->enabled = true;
    item->checked = false;

    /* Add to menu */
    SystemMenu_AddItem(item);

    /* Update platform menu */
    SystemMenu_UpdatePlatformMenu();

    return DESK_ERR_NONE;
}

/*
 * Remove DA from system menu
 */
void SystemMenu_RemoveDA(DeskAccessory *da)
{
    if (!g_systemMenuInitialized || !da) {
        return;
    }

    MenuItem *item = SystemMenu_FindItemByDA(da->name);
    if (item) {
        SystemMenu_RemoveItem(item);
        SystemMenu_FreeItem(item);
        SystemMenu_UpdatePlatformMenu();
    }
}

/*
 * Update system menu
 */
void SystemMenu_Update(void)
{
    if (!g_systemMenuInitialized) {
        /* Initialize if not already done */
        SystemMenu_Initialize();
        return;
    }

    SystemMenu_UpdatePlatformMenu();
}

/*
 * Handle system menu selection
 */
int SystemMenu_HandleSelection(SInt16 itemID)
{
    if (!g_systemMenuInitialized) {
        return DESK_ERR_SYSTEM_ERROR;
    }

    MenuItem *item = SystemMenu_FindItem(itemID);
    if (!item) {
        return DESK_ERR_NOT_FOUND;
    }

    switch (item->type) {
        case MENU_ITEM_ABOUT:
            /* Show About dialog */
            /* This would need platform-specific implementation */
            return DESK_ERR_NONE;

        case MENU_ITEM_DA:
            /* Open or activate desk accessory */
            return OpenDeskAcc(item->daName);

        case MENU_ITEM_CONTROL:
            /* Open control panel */
            /* This would need implementation */
            return DESK_ERR_NONE;

        case MENU_ITEM_SEPARATOR:
            /* Separators can't be selected */
            return DESK_ERR_INVALID_PARAM;

        default:
            return DESK_ERR_INVALID_PARAM;
    }
}

/*
 * Set menu item enabled state
 */
void SystemMenu_SetItemEnabled(SInt16 itemID, Boolean enabled)
{
    if (!g_systemMenuInitialized) {
        return;
    }

    MenuItem *item = SystemMenu_FindItem(itemID);
    if (item) {
        item->enabled = enabled;
        SystemMenu_UpdatePlatformMenu();
    }
}

/*
 * Set menu item checked state
 */
void SystemMenu_SetItemChecked(SInt16 itemID, Boolean checked)
{
    if (!g_systemMenuInitialized) {
        return;
    }

    MenuItem *item = SystemMenu_FindItem(itemID);
    if (item) {
        item->checked = checked;
        SystemMenu_UpdatePlatformMenu();
    }
}

/*
 * Get menu item count
 */
__attribute__((unused))
static SInt16 SystemMenu_GetItemCount(void)
{
    return g_systemMenuInitialized ? g_systemMenu.itemCount : 0;
}

/*
 * Get menu item by index
 */
__attribute__((unused))
static const MenuItem *SystemMenu_GetItem(SInt16 index)
{
    if (!g_systemMenuInitialized || index < 0) {
        return NULL;
    }

    MenuItem *item = g_systemMenu.firstItem;
    for (SInt16 i = 0; i < index && item; i++) {
        item = item->next;
    }

    return item;
}

/*
 * Check if menu is enabled
 */
__attribute__((unused))
static Boolean SystemMenu_IsEnabled(void)
{
    return g_systemMenuInitialized ? g_systemMenu.menuEnabled : false;
}

/*
 * Set menu enabled state
 */
__attribute__((unused))
static void SystemMenu_SetEnabled(Boolean enabled)
{
    if (g_systemMenuInitialized) {
        g_systemMenu.menuEnabled = enabled;
        SystemMenu_UpdatePlatformMenu();
    }
}

/* Internal Functions */

/*
 * Allocate a new menu item
 */
static MenuItem *SystemMenu_AllocateItem(void)
{
    return NewPtrClear(sizeof(MenuItem));
}

/*
 * Free a menu item
 */
static void SystemMenu_FreeItem(MenuItem *item)
{
    if (item) {
        DisposePtr((Ptr)item);
    }
}

/*
 * Find menu item by ID
 */
static MenuItem *SystemMenu_FindItem(SInt16 itemID)
{
    MenuItem *item = g_systemMenu.firstItem;
    while (item) {
        if (item->itemID == itemID) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

/*
 * Find menu item by DA name
 */
static MenuItem *SystemMenu_FindItemByDA(const char *daName)
{
    if (!daName) {
        return NULL;
    }

    MenuItem *item = g_systemMenu.firstItem;
    while (item) {
        if (item->type == MENU_ITEM_DA && strcmp(item->daName, daName) == 0) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

/*
 * Add item to menu list
 */
static void SystemMenu_AddItem(MenuItem *item)
{
    if (!item) {
        return;
    }

    if (!g_systemMenu.firstItem) {
        g_systemMenu.firstItem = item;
        g_systemMenu.lastItem = item;
    } else {
        g_systemMenu.lastItem->next = item;
        g_systemMenu.lastItem = item;
    }

    g_systemMenu.itemCount++;
}

/*
 * Remove item from menu list
 */
static void SystemMenu_RemoveItem(MenuItem *item)
{
    if (!item) {
        return;
    }

    /* Find previous item */
    MenuItem *prev = NULL;
    MenuItem *curr = g_systemMenu.firstItem;

    while (curr && curr != item) {
        prev = curr;
        curr = curr->next;
    }

    if (curr) {
        /* Remove from list */
        if (prev) {
            prev->next = curr->next;
        } else {
            g_systemMenu.firstItem = curr->next;
        }

        if (curr == g_systemMenu.lastItem) {
            g_systemMenu.lastItem = prev;
        }

        g_systemMenu.itemCount--;
    }
}

/*
 * Create platform-specific menu
 */
static int SystemMenu_CreatePlatformMenu(void)
{
    /* In a real implementation, this would create a platform-specific menu
     * For now, just allocate a dummy handle */
    g_systemMenu.menuHandle = NewPtr(sizeof(void *));
    if (!g_systemMenu.menuHandle) {
        return DESK_ERR_NO_MEMORY;
    }

    return DESK_ERR_NONE;
}

/*
 * Destroy platform-specific menu
 */
static void SystemMenu_DestroyPlatformMenu(void)
{
    if (g_systemMenu.menuHandle) {
        DisposePtr((Ptr)g_systemMenu.menuHandle);
        g_systemMenu.menuHandle = NULL;
    }
}

/*
 * Update platform-specific menu
 */
static void SystemMenu_UpdatePlatformMenu(void)
{
    if (!g_systemMenu.menuHandle) {
        return;
    }

    /* In a real implementation, this would update the platform menu
     * with current items, enabled states, etc. */

    /* For now, just validate the menu structure */
    MenuItem *item = g_systemMenu.firstItem;
    int count = 0;
    while (item) {
        count++;
        item = item->next;
    }

    /* Ensure count matches */
    if (count != g_systemMenu.itemCount) {
        /* Fix count if inconsistent */
        g_systemMenu.itemCount = count;
    }
}

/*
 * Add built-in menu items
 */
static void SystemMenu_AddBuiltinItems(void)
{
    /* Add "About This Macintosh" item */
    MenuItem *aboutItem = SystemMenu_AllocateItem();
    if (aboutItem) {
        aboutItem->itemID = ABOUT_ITEM_ID;
        aboutItem->type = MENU_ITEM_ABOUT;
        strcpy(aboutItem->text, "About This Macintosh");
        aboutItem->enabled = true;
        SystemMenu_AddItem(aboutItem);
    }

    /* Add separator */
    MenuItem *separator = SystemMenu_AllocateItem();
    if (separator) {
        separator->itemID = SEPARATOR_ITEM_ID;
        separator->type = MENU_ITEM_SEPARATOR;
        strcpy(separator->text, "-");
        separator->enabled = false;
        SystemMenu_AddItem(separator);
    }
}

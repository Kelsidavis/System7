/*
 * Menu Manager Stubs - Quarantined stub functions
 *
 * NOTE: Most Menu Manager functions have real implementations in:
 * - MenuManagerCore.c: Menu creation, disposal, menu bar, hiliting
 * - MenuItems.c: Item manipulation, properties, counting, sizing
 * - MenuSelection.c: MenuSelect, MenuKey, MenuChoice
 * - PopupMenus.c: PopUpMenuSelect
 *
 * This file previously contained stubs that shadowed real implementations.
 * All such stubs have been removed to avoid link conflicts.
 *
 * Remaining stubs are functions without implementations yet.
 */
#include "MenuManager/MenuManager.h"
#include "SystemTypes.h"
#include "DeskManager/DeskAccessory.h"
#include "ResourceManager.h"
#include <string.h>

/* Forward declarations */
void AddResMenu(MenuHandle theMenu, ResType theType);
void InsertResMenu(MenuHandle theMenu, ResType theType, short afterItem);

/* Standard menu commands */

/*
 * AddResMenu - Add resource names to menu
 *
 * Enumerates resources of the specified type and adds each resource name
 * as a menu item. Commonly used for:
 * - Font menus (type 'FONT')
 * - Desk Accessory menus (type 'DRVR')
 * - Sound menus (type 'snd ')
 */
void AddResMenu(MenuHandle theMenu, ResType theType) {
    if (!theMenu) return;

    /* Desk accessories: the ones the Desk Manager has, by name. They are
     * built in rather than DRVR resources, so a program's Apple menu - built
     * with AddResMenu(appleMenu, 'DRVR') like every program's - had none. */
    if (theType == 'DRVR') {
        DARegistryEntry* entries[32];
        int n = DA_GetRegisteredDAs(entries, 32);
        for (int a = 1; a < n; a++) {
            DARegistryEntry* key = entries[a];
            int b = a - 1;
            while (b >= 0 && strcmp(entries[b]->name, key->name) > 0) {
                entries[b + 1] = entries[b];
                b--;
            }
            entries[b + 1] = key;
        }
        for (int i = 0; i < n; i++) {
            if (!entries[i] || !entries[i]->name[0]) continue;
            Str255 name;
            size_t len = strlen(entries[i]->name);
            if (len > 255) len = 255;
            name[0] = (unsigned char)len;
            memcpy(&name[1], entries[i]->name, len);
            /* Appended under a placeholder and then named, so nothing in a
             * name is read as an AppendMenu command */
            AppendMenu(theMenu, (ConstStr255Param)"\001x");
            SetMenuItemText(theMenu, CountMItems(theMenu), name);
        }
        return;
    }

    /* Every open resource file's, not only the current one's; names that
     * begin with a period or a percent sign are left out (IM I-353) */
    SInt16 count = CountResources(theType);
    for (SInt16 i = 1; i <= count; i++) {
        Handle resHandle = GetIndResource(theType, i);
        if (!resHandle) continue;
        ResID resID;
        ResType resType;
        unsigned char name[256];
        name[0] = 0;
        GetResInfo(resHandle, &resID, &resType, (char*)name);
        if (name[0] > 0 && name[1] != '.' && name[1] != '%') {
            AppendMenu(theMenu, (ConstStr255Param)"\001x");
            SetMenuItemText(theMenu, CountMItems(theMenu), name);
        }
    }
}

/*
 * InsertResMenu - Insert resource names into menu
 *
 * Similar to AddResMenu but inserts resources after a specific menu item.
 * Allows building menus with resources placed at specific positions.
 */
void InsertResMenu(MenuHandle theMenu, ResType theType, short afterItem) {
    if (!theMenu) return;

    /* Resource Manager functions - declared in ResourceManager.h */
    extern SInt16 Count1Resources(ResType theType);
    extern Handle Get1IndResource(ResType theType, SInt16 index);
    extern void GetResInfo(Handle theResource, ResID* theID, ResType* theType, char* name);
    /* InsertMenuItem is declared in MenuManager.h */

    /* Count resources of specified type */
    SInt16 count = Count1Resources(theType);
    if (count <= 0) {
        return; /* No resources of this type */
    }

    /* Insert each resource name after the specified item */
    short insertAfter = afterItem;
    for (SInt16 i = 1; i <= count; i++) {
        Handle resHandle = Get1IndResource(theType, i);
        if (resHandle) {
            ResID resID;
            ResType resType;
            unsigned char name[256];

            /* Get resource name */
            GetResInfo(resHandle, &resID, &resType, (char*)name);

            /* Only insert if resource has a name */
            if (name[0] > 0) {
                InsertMenuItem(theMenu, name, insertAfter);
                insertAfter++; /* Next item goes after this one */
            }
        }
    }
}
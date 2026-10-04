/* Resource and native accessory names share literal, sorted menu insertion. */
#include "MenuManager/menu_private.h"
#include "DeskManager/DeskAccessory.h"
#include "ResourceManager.h"
#include <string.h>

static int CompareNames(ConstStr255Param left, ConstStr255Param right)
{
    size_t length = left[0] < right[0] ? left[0] : right[0];
    int order = memcmp(left + 1, right + 1, length);
    return order ? order : (int)left[0] - (int)right[0];
}

short Menu_InsertSortedName(MenuHandle menu, ConstStr255Param name, short first, short last)
{
    if (!menu || !name || !name[0]) return 0;
    short count = CountMItems(menu);
    if (count == INT16_MAX || first < 1 || last < first - 1 || last > count) return 0;
    short after = (short)(first - 1);
    Str255 existing;
    while (after < last) {
        GetMenuItemText(menu, (short)(after + 1), existing);
        if (CompareNames(name, existing) < 0) break;
        ++after;
    }
    /* The placeholder prevents names from being interpreted as menu commands. */
    InsertMenuItem(menu, (ConstStr255Param)"\001x", after);
    if (CountMItems(menu) != count + 1) return 0;
    short inserted = (short)(after + 1);
    SetMenuItemText(menu, inserted, name);
    return inserted;
}

static Boolean IsResourceMenuName(ConstStr255Param name)
{
    return name[0] && name[1] != '.' && name[1] != '%';
}

static void InsertNamedResources(MenuHandle menu, ResType type, short first, short *last)
{
    SInt16 resourceCount = CountResources(type);
    for (int index = 1; index <= resourceCount; ++index) {
        Handle resource = GetIndResource(type, (SInt16)index);
        if (!resource) continue;
        ResID id;
        ResType resourceType;
        Str255 name = {0};
        GetResInfo(resource, &id, &resourceType, (char*)name);
        if (!IsResourceMenuName(name)) continue;
        if (!Menu_InsertSortedName(menu, name, first, *last)) return;
        ++*last;
    }
}

void InsertResMenu(MenuHandle menu, ResType type, short afterItem)
{
    if (!menu) return;
    SetResLoad(true);
    short count = CountMItems(menu);
    if (count == INT16_MAX) return;
    if (afterItem < 0 || afterItem > count) afterItem = count;
    short first = (short)(afterItem + 1);
    short last = afterItem;

    if (type == FOURCC('D', 'R', 'V', 'R')) {
        for (const DARegistryEntry *entry = DA_GetFirstRegisteredDA(); entry; entry = entry->next) {
            Str255 name;
            size_t length = strlen(entry->name);
            name[0] = (UInt8)length;
            memcpy(name + 1, entry->name, length);
            if (!IsResourceMenuName(name)) continue;
            if (!Menu_InsertSortedName(menu, name, first, last)) break;
            ++last;
        }
        return;
    }

    if (type == FOURCC('F', 'O', 'N', 'T') || type == FOURCC('F', 'O', 'N', 'D')) {
        InsertNamedResources(menu, FOURCC('F', 'O', 'N', 'D'), first, &last);
        InsertNamedResources(menu, FOURCC('F', 'O', 'N', 'T'), first, &last);
    } else {
        InsertNamedResources(menu, type, first, &last);
    }
}

void AddResMenu(MenuHandle menu, ResType type)
{
    if (menu) InsertResMenu(menu, type, CountMItems(menu));
}

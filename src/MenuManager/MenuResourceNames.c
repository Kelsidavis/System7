/* Resource and native accessory names share literal, sorted menu insertion. */
#include "MenuManager/menu_private.h"
#include "DeskManager/DeskAccessory.h"
#include "ResourceManager.h"
#include "QuickDraw/QuickDraw.h"
#include "TextEncoding/TextEncodingUtils.h"
#include <string.h>

#define MENU_SCRIPT_SYSTEM smSystemScript
#define MENU_SCRIPT_CURRENT smCurrentScript
#define MENU_SCRIPT_ALL smAllScripts

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

static ScriptCode ScriptCodeForResourceID(ResID id)
{
    if (id >= 0) {
        if (id < 0x4000) return 0;
        return (ScriptCode)(((UInt16)id - 0x4000) / 512 + 1);
    }

    if (id < -16384) {
        return (ScriptCode)(((SInt32)id + 32768) / 512 + 33);
    }

    return (ScriptCode)-32768;
}

static Boolean ResolveScriptFilter(short filter, ScriptCode *script)
{
    if (filter >= 0 && filter <= 64) {
        *script = (ScriptCode)filter;
        return true;
    }

    if (filter == MENU_SCRIPT_SYSTEM) {
        *script = GetStringPackageScript();
        return true;
    }

    if (filter == MENU_SCRIPT_CURRENT) {
        GrafPtr port = NULL;
        GetPort(&port);
        *script = port ? ScriptCodeForResourceID(port->txFont) : 0;
        return true;
    }

    return filter == MENU_SCRIPT_ALL;
}

static Boolean InsertNamedResources(MenuHandle menu, ResType type, short first, short *last,
                                    Boolean filterByScript, ScriptCode script)
{
    SInt16 resourceCount = CountResources(type);
    for (int index = 1; index <= resourceCount; ++index) {
        Handle resource = GetIndResource(type, (SInt16)index);
        if (!resource) continue;
        ResID id;
        ResType resourceType;
        Str255 name = {0};
        GetResInfo(resource, &id, &resourceType, (char*)name);
        if (filterByScript && ScriptCodeForResourceID(id) != script) continue;
        if (!IsResourceMenuName(name)) continue;
        short item = Menu_InsertSortedName(menu, name, first, *last);
        if (!item) return false;
        if (type == FOURCC('F', 'O', 'N', 'D')) {
            ScriptCode itemScript = ScriptCodeForResourceID(id);
            if (itemScript > smRoman && itemScript <= 64) {
                SetItemCmd(menu, item, MENU_SCRIPT_ITEM_COMMAND);
                SetItemIcon(menu, item, itemScript);
            }
        }
        ++*last;
    }
    return true;
}

static void InsertResourceTypes(MenuHandle menu, const ResType *types, short typeCount,
                                short afterItem, short scriptFilter)
{
    if (!menu) return;
    SetResLoad(true);

    ScriptCode script = 0;
    Boolean filterByScript = scriptFilter != MENU_SCRIPT_ALL;
    if (filterByScript && !ResolveScriptFilter(scriptFilter, &script)) return;

    short count = CountMItems(menu);
    if (count == INT16_MAX) return;
    if (afterItem < 0 || afterItem > count) afterItem = count;
    short first = (short)(afterItem + 1);
    short last = afterItem;
    for (short i = 0; i < typeCount; ++i) {
        if (!InsertNamedResources(menu, types[i], first, &last, filterByScript, script)) return;
    }
}

void InsertResMenu(MenuHandle menu, ResType type, short afterItem)
{
    if (!menu) return;

    if (type == FOURCC('D', 'R', 'V', 'R')) {
        SetResLoad(true);
        short count = CountMItems(menu);
        if (count == INT16_MAX) return;
        if (afterItem < 0 || afterItem > count) afterItem = count;
        short first = (short)(afterItem + 1);
        short last = afterItem;
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
        const ResType fontTypes[] = {FOURCC('F', 'O', 'N', 'D'), FOURCC('F', 'O', 'N', 'T')};
        InsertResourceTypes(menu, fontTypes, 2, afterItem, MENU_SCRIPT_ALL);
    } else {
        InsertResourceTypes(menu, &type, 1, afterItem, MENU_SCRIPT_ALL);
    }
}

void InsertIntlResMenu(MenuHandle menu, ResType type, short afterItem, short scriptFilter)
{
    InsertResourceTypes(menu, &type, 1, afterItem, scriptFilter);
}

void InsertFontResMenu(MenuHandle menu, short afterItem, short scriptFilter)
{
    const ResType fontTypes[] = {FOURCC('F', 'O', 'N', 'D'), FOURCC('F', 'O', 'N', 'T')};
    InsertResourceTypes(menu, fontTypes, 2, afterItem, scriptFilter);
}

void AddResMenu(MenuHandle menu, ResType type)
{
    if (menu) InsertResMenu(menu, type, CountMItems(menu));
}

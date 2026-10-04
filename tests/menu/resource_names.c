#include "MenuManager/menu_private.h"
#include "QuickDraw/QuickDraw.h"
#include "TextEncoding/TextEncodingUtils.h"
#include "DeskManager/DeskAccessory.h"
#include "ResourceManager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("%s:%d: menu regression failed: %s\n", __FILE__, __LINE__, #condition); \
        abort(); \
    } \
} while (0)

static struct Menu* menuInfo;
static MenuHandle menu = &menuInfo;
static Str255 items[100];
static short itemCount;
static short capacity = 100;
static unsigned writes;
static unsigned resourceQueries;
static Boolean resourcesLoaded;
static DARegistryEntry entries[70];
static const DARegistryEntry* firstEntry;
static const char* resourceNames[] = {"Zulu", ".hidden", "%private", "", "A;B/X", "Alpha"};
static char* resourceData[6];
static const char* intlResourceNames[] = {"Roman Item", "Japanese Item"};
static char* intlResourceData[2];

static void Pascal(Str255 text, const char* name)
{
    size_t length = strlen(name);
    CHECK(length <= 255);
    text[0] = (UInt8)length;
    memcpy(text + 1, name, length);
}

static void Reset(void)
{
    itemCount = 0;
    capacity = 100;
    writes = 0;
    resourceQueries = 0;
    resourcesLoaded = false;
    firstEntry = NULL;
}

static Boolean IsItem(short index, const char* name)
{
    size_t length = strlen(name);
    return index >= 1 && index <= itemCount && items[index - 1][0] == length &&
           memcmp(items[index - 1] + 1, name, length) == 0;
}

short CountMItems(MenuHandle handle) { CHECK(handle == menu); return itemCount; }
void GetMenuItemText(MenuHandle handle, short index, Str255 text)
{
    CHECK(handle == menu && index >= 1 && index <= itemCount);
    memcpy(text, items[index - 1], items[index - 1][0] + 1);
}
void InsertMenuItem(MenuHandle handle, ConstStr255Param name, short after)
{
    CHECK(handle == menu && name[0] == 1 && name[1] == 'x');
    CHECK(after >= 0 && after <= itemCount);
    if (itemCount == capacity) return;
    memmove(items + after + 1, items + after, (size_t)(itemCount - after) * sizeof(items[0]));
    memcpy(items[after], name, 2);
    ++itemCount;
}
void SetMenuItemText(MenuHandle handle, short index, ConstStr255Param name)
{
    CHECK(handle == menu && index >= 1 && index <= itemCount);
    memcpy(items[index - 1], name, name[0] + 1);
    ++writes;
}

const DARegistryEntry* DA_GetFirstRegisteredDA(void) { return firstEntry; }
void SetResLoad(Boolean load) { resourcesLoaded = load; }
SInt16 CountResources(ResType type)
{
    CHECK(type == FOURCC('T', 'E', 'S', 'T') || type == FOURCC('I', 'N', 't', 'l'));
    ++resourceQueries;
    return type == FOURCC('T', 'E', 'S', 'T') ? 6 : 2;
}
Handle GetIndResource(ResType type, SInt16 index)
{
    CHECK(resourcesLoaded);
    if (type == FOURCC('T', 'E', 'S', 'T')) {
        CHECK(index >= 1 && index <= 6);
        return &resourceData[index - 1];
    }
    CHECK(type == FOURCC('I', 'N', 't', 'l') && index >= 1 && index <= 2);
    return &intlResourceData[index - 1];
}
void GetResInfo(Handle resource, ResID* id, ResType* type, char* name)
{
    for (unsigned i = 0; i < sizeof(resourceData) / sizeof(resourceData[0]); ++i) {
        if (resource == &resourceData[i]) {
            *id = (ResID)i;
            *type = FOURCC('T', 'E', 'S', 'T');
            Pascal((UInt8*)name, resourceNames[i]);
            return;
        }
    }
    for (unsigned i = 0; i < sizeof(intlResourceData) / sizeof(intlResourceData[0]); ++i) {
        if (resource == &intlResourceData[i]) {
            *id = i == 0 ? 10 : 0x4000;
            *type = FOURCC('I', 'N', 't', 'l');
            Pascal((UInt8*)name, intlResourceNames[i]);
            return;
        }
    }
    CHECK(false);
}

void GetPort(GrafPtr* port) { *port = NULL; }
ScriptCode GetStringPackageScript(void) { return smRoman; }

static void TestLiteralSorting(void)
{
    Reset();
    Str255 name;
    Pascal(items[itemCount++], "Prefix");
    Pascal(items[itemCount++], "Suffix");
    Pascal(name, "Zulu");
    CHECK(Menu_InsertSortedName(menu, name, 2, 1) == 2);
    Pascal(name, "A;B(/X");
    CHECK(Menu_InsertSortedName(menu, name, 2, 2) == 2);
    Pascal(name, "Alpha");
    CHECK(Menu_InsertSortedName(menu, name, 2, 3) == 3);
    CHECK(IsItem(1, "Prefix") && IsItem(2, "A;B(/X") && IsItem(3, "Alpha") &&
          IsItem(4, "Zulu") && IsItem(5, "Suffix"));
    capacity = itemCount;
    unsigned before = writes;
    CHECK(Menu_InsertSortedName(menu, name, 2, 4) == 0 && writes == before);
    CHECK(IsItem(5, "Suffix"));
    CHECK(Menu_InsertSortedName(menu, name, 0, 1) == 0);
    CHECK(Menu_InsertSortedName(menu, name, 2, 6) == 0);
    CHECK(Menu_InsertSortedName(menu, name, 4, 1) == 0);
    CHECK(Menu_InsertSortedName(NULL, name, 1, 0) == 0);
    CHECK(Menu_InsertSortedName(menu, NULL, 1, 0) == 0);
}

static void TestResourceNames(void)
{
    Reset();
    Pascal(items[itemCount++], "Prefix");
    Pascal(items[itemCount++], "Suffix");
    InsertResMenu(menu, FOURCC('T', 'E', 'S', 'T'), 1);
    CHECK(itemCount == 5 && resourceQueries == 1 && resourcesLoaded);
    CHECK(IsItem(1, "Prefix") && IsItem(2, "A;B/X") && IsItem(3, "Alpha") &&
          IsItem(4, "Zulu") && IsItem(5, "Suffix"));
    Reset();
    Pascal(items[itemCount++], "Not sorted with additions");
    AddResMenu(menu, FOURCC('T', 'E', 'S', 'T'));
    CHECK(IsItem(1, "Not sorted with additions") && IsItem(2, "A;B/X") &&
          IsItem(3, "Alpha") && IsItem(4, "Zulu"));
    Reset();
    InsertResMenu(menu, FOURCC('T', 'E', 'S', 'T'), 200);
    CHECK(itemCount == 3 && IsItem(1, "A;B/X"));
    Reset();
    InsertResMenu(menu, FOURCC('T', 'E', 'S', 'T'), -1);
    CHECK(itemCount == 3 && IsItem(1, "A;B/X"));
    Reset();
    AddResMenu(NULL, FOURCC('T', 'E', 'S', 'T'));
    InsertResMenu(NULL, FOURCC('T', 'E', 'S', 'T'), 0);
    CHECK(itemCount == 0 && resourceQueries == 0);
}

static void TestInternationalResourceNames(void)
{
    Reset();
    Pascal(items[itemCount++], "Prefix");
    Pascal(items[itemCount++], "Suffix");
    InsertIntlResMenu(menu, FOURCC('I', 'N', 't', 'l'), 1, smJapanese);
    CHECK(itemCount == 3 && IsItem(2, "Japanese Item"));

    Reset();
    Pascal(items[itemCount++], "Prefix");
    Pascal(items[itemCount++], "Suffix");
    InsertIntlResMenu(menu, FOURCC('I', 'N', 't', 'l'), 1, smRoman);
    CHECK(itemCount == 3 && IsItem(2, "Roman Item"));

    Reset();
    Pascal(items[itemCount++], "Prefix");
    Pascal(items[itemCount++], "Suffix");
    InsertIntlResMenu(menu, FOURCC('I', 'N', 't', 'l'), 1, smAllScripts);
    CHECK(itemCount == 4 && IsItem(2, "Japanese Item") && IsItem(3, "Roman Item"));
}

static void TestNativeAccessories(void)
{
    Reset();
    for (unsigned i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        snprintf(entries[i].name, sizeof(entries[i].name), "Accessory %02u", 69 - i);
        entries[i].next = i < 69 ? entries + i + 1 : NULL;
    }
    firstEntry = entries;
    Pascal(items[itemCount++], "About");
    Pascal(items[itemCount++], "-");
    AddResMenu(menu, FOURCC('D', 'R', 'V', 'R'));
    CHECK(itemCount == 72 && resourceQueries == 0);
    for (unsigned i = 0; i < 70; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "Accessory %02u", i);
        CHECK(IsItem((short)(i + 3), name));
    }
    Reset();
    firstEntry = entries;
    capacity = 4;
    Pascal(items[itemCount++], "About");
    AddResMenu(menu, FOURCC('D', 'R', 'V', 'R'));
    CHECK(itemCount == 4 && writes == 3 && IsItem(1, "About"));
    Reset();
    strcpy(entries[0].name, ".hidden");
    strcpy(entries[1].name, "%hidden");
    entries[1].next = NULL;
    firstEntry = entries;
    InsertResMenu(menu, FOURCC('D', 'R', 'V', 'R'), 0);
    CHECK(itemCount == 0);
}

int main(void)
{
    TestLiteralSorting();
    TestResourceNames();
    TestInternationalResourceNames();
    TestNativeAccessories();
    puts("Resource menu name regressions passed.");
    return 0;
}

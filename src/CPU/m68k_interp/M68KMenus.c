/*
 * M68KMenus.c - the Menu Manager for a 68K application
 *
 * A program numbers its menus from 128 like every program, and the Finder's
 * menus, out of the bar but not gone, hold those numbers - NewMenu refuses
 * one in use. So each of the program's menus is made native under a number
 * of this module's choosing, and the numbers are translated both ways:
 * MenuSelect and MenuKey answer with the program's, HiliteMenu and
 * DeleteMenu take them.
 *
 * Each menu also has a MenuInfo record in the program's memory - menuID,
 * menuWidth, menuHeight, menuProc, enableFlags and the title (IM I-346) - so
 * the handle the program holds can be followed. The items stay native.
 */

#include <string.h>

#include "M68KToolboxInternal.h"
#include "MenuManager/MenuManager.h"
#include "ResourceManager.h"
#include "System71StdLib.h"

/* A native menu is a MenuInfo behind its handle */
#define MI(h) ((MenuInfo*)*(h))

enum { kMaxMenus = 64, kFirstNativeID = 20000 };

typedef struct {
    SInt16 appID;
    MenuHandle native;
    UInt32 h;               /* the program's MenuHandle */
} MenuMap;

static MenuMap gMenus[kMaxMenus];
static int gMenuCount;
static SInt16 gNextNativeID = kFirstNativeID;

static MenuMap* ByHandle(UInt32 h) {
    h &= 0x00FFFFFF;
    for (int i = 0; i < gMenuCount; i++) if (gMenus[i].h == h) return &gMenus[i];
    return NULL;
}
static MenuMap* ByAppID(SInt16 id) {
    for (int i = 0; i < gMenuCount; i++) if (gMenus[i].appID == id) return &gMenus[i];
    return NULL;
}
static MenuMap* ByNativeID(SInt16 id) {
    for (int i = 0; i < gMenuCount; i++)
        if (gMenus[i].native && MI(gMenus[i].native)->menuID == id) return &gMenus[i];
    return NULL;
}

static MenuHandle PopMenu(void) {
    MenuMap* m = ByHandle(Pop32());
    return m ? m->native : NULL;
}

/* The record from the native menu */
static void SyncMenu(MenuMap* m) {
    UInt32 p = M68KHeap_Deref(m->h);
    if (!p || !m->native || !*m->native) return;
    MenuInfo* mi = MI(m->native);
    W16(p + 0, m->appID);
    W16(p + 2, mi->menuWidth);
    W16(p + 4, mi->menuHeight);
    W32(p + 6, 0);
    W32(p + 10, (UInt32)mi->enableFlags);
    WritePString(p + 14, mi->menuData);
}

static UInt32 Adopt(SInt16 appID, MenuHandle native) {
    if (gMenuCount >= kMaxMenus) return 0;
    UInt32 h = M68KHeap_NewHandle(14 + 256, true);
    if (!h) return 0;
    MenuMap* m = &gMenus[gMenuCount++];
    m->appID = appID;
    m->native = native;
    m->h = h;
    SyncMenu(m);
    return h;
}

/* The program's answer from a native MenuSelect or MenuKey */
static UInt32 Translate(long result) {
    SInt16 id = (SInt16)(result >> 16);
    if (!id) return (UInt32)result;
    MenuMap* m = ByNativeID(id);
    return m ? ((UInt32)(UInt16)m->appID << 16) | (UInt16)result : (UInt32)result;
}

/* FUNCTION NewMenu(menuID: INTEGER; menuTitle: Str255): MenuHandle */
TRAP(Trap_NewMenu) {
    UNUSED;
    Str255 title;
    ReadPString(Pop32(), title);
    SInt16 id = (SInt16)Pop16();
    MenuHandle native = NewMenu(gNextNativeID++, title);
    Result32(native ? Adopt(id, native) : 0);
    return noErr;
}

/* FUNCTION GetMenu(resourceID: INTEGER): MenuHandle - from a MENU resource:
 * menuID, width, height, procID, enableFlags, title, then items each a name,
 * icon, key, mark and style, ended by an empty name */
TRAP(Trap_GetMenu) {
    UNUSED;
    SInt16 resID = (SInt16)Pop16();
    MenuMap* have = ByAppID(resID);
    if (have) {
        Result32(have->h);
        return noErr;
    }
    Handle res = GetResource(FOURCC('M','E','N','U'), resID);
    if (!res || GetHandleSize(res) < 15) {
        Result32(0);
        return noErr;
    }
    HLock(res);
    const UInt8* p = (const UInt8*)*res;
    Size size = GetHandleSize(res);
    SInt16 appID = (SInt16)((p[0] << 8) | p[1]);
    UInt32 flags = ((UInt32)p[10] << 24) | ((UInt32)p[11] << 16) | ((UInt32)p[12] << 8) | p[13];
    Str255 title;
    memcpy(title, p + 14, (size_t)p[14] + 1);
    MenuHandle m = NewMenu(gNextNativeID++, title);
    Size at = 15 + p[14];
    short item = 0;
    while (m && at < size && p[at] != 0) {
        Str255 name;
        memcpy(name, p + at, (size_t)p[at] + 1);
        at += p[at] + 1;
        if (at + 4 > size) break;
        UInt8 icon = p[at], key = p[at + 1], mark = p[at + 2], style = p[at + 3];
        at += 4;
        item++;
        /* Appended as a placeholder and then named, so characters AppendMenu
         * would read as commands are only the item's text */
        AppendMenu(m, (ConstStr255Param)"\001x");
        SetMenuItemText(m, item, name);
        if (key) SetItemCmd(m, item, key);
        if (mark) SetItemMark(m, item, mark);
        if (style) SetItemStyle(m, item, style);
        if (icon) SetItemIcon(m, item, icon);
        if (item < 32 && !(flags & (1u << item))) DisableItem(m, item);
        if (name[0] == 1 && name[1] == '-') DisableItem(m, item);
    }
    if (m && !(flags & 1)) DisableItem(m, 0);
    HUnlock(res);
    ReleaseResource(res);
    Result32(m ? Adopt(appID, m) : 0);
    return noErr;
}

TRAP(Trap_DisposeMenu) {
    UNUSED;
    MenuMap* m = ByHandle(Pop32());
    if (m) {
        DeleteMenu(MI(m->native)->menuID);
        DisposeMenu(m->native);
        M68KHeap_DisposeHandle(m->h);
        *m = gMenus[--gMenuCount];
    }
    return noErr;
}

TRAP(Trap_AppendMenu) {
    UNUSED;
    Str255 data;
    ReadPString(Pop32(), data);
    MenuMap* m = ByHandle(Pop32());
    if (m) {
        AppendMenu(m->native, data);
        SyncMenu(m);
    }
    return noErr;
}

/* PROCEDURE InsertMenu(theMenu: MenuHandle; beforeID: INTEGER) */
TRAP(Trap_InsertMenu) {
    UNUSED;
    SInt16 before = (SInt16)Pop16();
    MenuMap* m = ByHandle(Pop32());
    if (!m) return noErr;
    MenuMap* b = before > 0 ? ByAppID(before) : NULL;
    InsertMenu(m->native, b ? MI(b->native)->menuID : before);
    return noErr;
}

TRAP(Trap_DeleteMenu) {
    UNUSED;
    MenuMap* m = ByAppID((SInt16)Pop16());
    if (m) DeleteMenu(MI(m->native)->menuID);
    return noErr;
}

TRAP(Trap_DrawMenuBar)  { UNUSED; DrawMenuBar(); return noErr; }
TRAP(Trap_ClearMenuBar) { UNUSED; ClearMenuBar(); return noErr; }

TRAP(Trap_HiliteMenu) {
    UNUSED;
    SInt16 id = (SInt16)Pop16();
    MenuMap* m = id ? ByAppID(id) : NULL;
    HiliteMenu(m ? MI(m->native)->menuID : 0);
    return noErr;
}

TRAP(Trap_FlashMenuBar) {
    UNUSED;
    SInt16 id = (SInt16)Pop16();
    MenuMap* m = id ? ByAppID(id) : NULL;
    FlashMenuBar(m ? MI(m->native)->menuID : 0);
    return noErr;
}

TRAP(Trap_MenuSelect) {
    UNUSED;
    Point p = PopPoint();
    Result32(Translate(MenuSelect(p)));
    return noErr;
}

TRAP(Trap_MenuKey) {
    UNUSED;
    UInt8 ch = (UInt8)Pop16();
    Result32(Translate(MenuKey(ch)));
    return noErr;
}

#define ITEM_VERB(name, call) \
    TRAP(name) { UNUSED; short item = (short)Pop16(); MenuMap* m = ByHandle(Pop32()); \
                 if (m) { call(m->native, item); SyncMenu(m); } return noErr; }
ITEM_VERB(Trap_EnableItem, EnableItem)
ITEM_VERB(Trap_DisableItem, DisableItem)
ITEM_VERB(Trap_DelMenuItem, DeleteMenuItem)

TRAP(Trap_CheckItem) {
    UNUSED;
    Boolean on = PopBool();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    if (m) CheckItem(m, item, on);
    return noErr;
}

TRAP(Trap_SetItemMark) {
    UNUSED;
    UInt8 mark = (UInt8)Pop16();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    if (m) SetItemMark(m, item, mark);
    return noErr;
}

TRAP(Trap_GetItemMark) {
    UNUSED;
    UInt32 var = Pop32();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    short mark = 0;
    if (m) GetItemMark(m, item, &mark);
    W16(var, (UInt16)(UInt8)mark);
    return noErr;
}

TRAP(Trap_SetItemCmd) {
    UNUSED;
    UInt8 key = (UInt8)Pop16();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    if (m) SetItemCmd(m, item, key);
    return noErr;
}

TRAP(Trap_GetItemCmd) {
    UNUSED;
    UInt32 var = Pop32();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    short key = 0;
    if (m) GetItemCmd(m, item, &key);
    W16(var, (UInt16)(UInt8)key);
    return noErr;
}

TRAP(Trap_SetItemStyle) {
    UNUSED;
    UInt8 style = (UInt8)Pop16();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    if (m) SetItemStyle(m, item, style);
    return noErr;
}

TRAP(Trap_GetItemStyle) {
    UNUSED;
    UInt32 var = Pop32();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    Style style = 0;
    if (m) GetItemStyle(m, item, &style);
    W8(var, style);
    return noErr;
}

TRAP(Trap_SetItemIcon) {
    UNUSED;
    UInt8 icon = (UInt8)Pop16();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    if (m) SetItemIcon(m, item, icon);
    return noErr;
}

TRAP(Trap_GetItemIcon) {
    UNUSED;
    UInt32 var = Pop32();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    short icon = 0;
    if (m) GetItemIcon(m, item, &icon);
    W16(var, (UInt16)(UInt8)icon);
    return noErr;
}

/* PROCEDURE SetItem(theMenu: MenuHandle; item: INTEGER; itemString: Str255) */
TRAP(Trap_SetItem) {
    UNUSED;
    Str255 text;
    ReadPString(Pop32(), text);
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    if (m) SetMenuItemText(m, item, text);
    return noErr;
}

TRAP(Trap_GetItem) {
    UNUSED;
    UInt32 var = Pop32();
    short item = (short)Pop16();
    MenuHandle m = PopMenu();
    Str255 text;
    text[0] = 0;
    if (m) GetMenuItemText(m, item, text);
    WritePString(var, text);
    return noErr;
}

TRAP(Trap_CountMItems) {
    UNUSED;
    MenuHandle m = PopMenu();
    Result16((UInt16)(m ? CountMItems(m) : 0));
    return noErr;
}

TRAP(Trap_GetMHandle) {
    UNUSED;
    MenuMap* m = ByAppID((SInt16)Pop16());
    Result32(m ? m->h : 0);
    return noErr;
}

TRAP(Trap_AddResMenu) {
    UNUSED;
    ResType type = Pop32();
    MenuMap* m = ByHandle(Pop32());
    if (m) {
        AddResMenu(m->native, type);
        SyncMenu(m);
    }
    return noErr;
}

TRAP(Trap_InsertResMenu) {
    UNUSED;
    short after = (short)Pop16();
    ResType type = Pop32();
    MenuMap* m = ByHandle(Pop32());
    if (m) {
        InsertResMenu(m->native, type, after);
        SyncMenu(m);
    }
    return noErr;
}

/* PROCEDURE InsMenuItem(theMenu: MenuHandle; itemString: Str255; afterItem: INTEGER) */
TRAP(Trap_InsMenuItem) {
    UNUSED;
    short after = (short)Pop16();
    Str255 text;
    ReadPString(Pop32(), text);
    MenuMap* m = ByHandle(Pop32());
    if (m) {
        InsertMenuItem(m->native, text, after);
        SyncMenu(m);
    }
    return noErr;
}

TRAP(Trap_CalcMenuSize) {
    UNUSED;
    MenuMap* m = ByHandle(Pop32());
    if (m) {
        CalcMenuSize(m->native);
        SyncMenu(m);
    }
    return noErr;
}

TRAP(Trap_SetMenuFlash) { UNUSED; (void)Pop16(); return noErr; }

/* A menu list, as this side keeps one: a count and the program's menu IDs.
 * GetNewMBar makes one from an MBAR resource, which has the same layout. */
static UInt32 NewMenuList(const SInt16* ids, int n) {
    UInt32 h = M68KHeap_NewHandle((UInt32)(2 + 2 * n), false);
    if (!h) return 0;
    UInt32 p = M68KHeap_Deref(h);
    W16(p, (UInt16)n);
    for (int i = 0; i < n; i++) W16(p + 2 + 2 * i, (UInt16)ids[i]);
    return h;
}

TRAP(Trap_GetNewMBar) {
    UNUSED;
    SInt16 id = (SInt16)Pop16();
    Handle mbar = GetResource(FOURCC('M','B','A','R'), id);
    if (!mbar || GetHandleSize(mbar) < 2) {
        Result32(0);
        return noErr;
    }
    const UInt8* p = (const UInt8*)*mbar;
    int n = (p[0] << 8) | p[1];
    if (n > kMaxMenus) n = kMaxMenus;
    SInt16 ids[kMaxMenus];
    for (int i = 0; i < n; i++) {
        ids[i] = (SInt16)((p[2 + 2 * i] << 8) | p[3 + 2 * i]);
        if (!ByAppID(ids[i])) {
            /* Load it as GetMenu would; the result goes nowhere */
            UInt32 sp = A(7);
            A(7) -= 6;
            W16(A(7), (UInt16)ids[i]);
            Trap_GetMenu(ctx, pc, regs);
            A(7) = sp;
        }
    }
    ReleaseResource(mbar);
    Result32(NewMenuList(ids, n));
    return noErr;
}

TRAP(Trap_SetMenuBar) {
    UNUSED;
    UInt32 h = Pop32();
    UInt32 p = h ? M68KHeap_Deref(h) : 0;
    if (!p) return noErr;
    ClearMenuBar();
    int n = R16(p);
    for (int i = 0; i < n && i < kMaxMenus; i++) {
        MenuMap* m = ByAppID((SInt16)R16(p + 2 + 2 * i));
        if (m) InsertMenu(m->native, 0);
    }
    return noErr;
}

TRAP(Trap_GetMenuBar) {
    UNUSED;
    SInt16 ids[kMaxMenus];
    int n = 0;
    for (int i = 0; i < gMenuCount && n < kMaxMenus; i++) {
        if (GetMenuHandle(MI(gMenus[i].native)->menuID) == gMenus[i].native) ids[n++] = gMenus[i].appID;
    }
    Result32(NewMenuList(ids, n));
    return noErr;
}

void M68KMenus_Finish(void) {
    ClearMenuBar();
    for (int i = 0; i < gMenuCount; i++) DisposeMenu(gMenus[i].native);
    gMenuCount = 0;
}

const M68KTrapEntry kM68KMenuTraps[] = {
    { 0xA931, Trap_NewMenu },       { 0xA9BF, Trap_GetMenu },       { 0xA932, Trap_DisposeMenu },
    { 0xA933, Trap_AppendMenu },    { 0xA935, Trap_InsertMenu },    { 0xA936, Trap_DeleteMenu },
    { 0xA937, Trap_DrawMenuBar },   { 0xA934, Trap_ClearMenuBar },  { 0xA938, Trap_HiliteMenu },
    { 0xA94C, Trap_FlashMenuBar },  { 0xA93D, Trap_MenuSelect },    { 0xA93E, Trap_MenuKey },
    { 0xA939, Trap_EnableItem },    { 0xA93A, Trap_DisableItem },   { 0xA952, Trap_DelMenuItem },
    { 0xA945, Trap_CheckItem },     { 0xA944, Trap_SetItemMark },   { 0xA943, Trap_GetItemMark },
    { 0xA84F, Trap_SetItemCmd },    { 0xA84E, Trap_GetItemCmd },    { 0xA942, Trap_SetItemStyle },
    { 0xA941, Trap_GetItemStyle },  { 0xA940, Trap_SetItemIcon },   { 0xA93F, Trap_GetItemIcon },
    { 0xA947, Trap_SetItem },       { 0xA946, Trap_GetItem },       { 0xA950, Trap_CountMItems },
    { 0xA949, Trap_GetMHandle },    { 0xA94D, Trap_AddResMenu },    { 0xA951, Trap_InsertResMenu },
    { 0xA826, Trap_InsMenuItem },   { 0xA948, Trap_CalcMenuSize },  { 0xA94A, Trap_SetMenuFlash },
    { 0xA9C0, Trap_GetNewMBar },    { 0xA93C, Trap_SetMenuBar },    { 0xA93B, Trap_GetMenuBar },
};
const int kM68KMenuTrapCount = (int)(sizeof(kM68KMenuTraps) / sizeof(kM68KMenuTraps[0]));

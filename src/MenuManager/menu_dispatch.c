/* #include "SystemTypes.h" */
/*
 * menu_dispatch.c - Menu Manager Dispatch Implementation
 *
 * Implementation of Menu Manager dispatch mechanism based on
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 Menu Manager dispatch
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "menu_dispatch.h"
#include "MenuManager/menu_private.h"


/* Forward declarations for internal functions */
static OSErr DispatchInsertFontResMenu(InsertFontResMenuParams *params);
static OSErr DispatchInsertIntlResMenu(InsertIntlResMenuParams *params);
static OSErr DispatchGetMenuTitleRect(GetMenuTitleRectParams *params);
static OSErr DispatchGetMBARRect(GetMBARRectParams *params);
static OSErr DispatchGetAppMenusRect(GetAppMenusRectParams *params);
static OSErr DispatchGetSysMenusRect(GetSysMenusRectParams *params);
static OSErr DispatchDrawMBARString(DrawMBARStringParams *params);
static OSErr DispatchIsSystemMenu(IsSystemMenuParams *params);
static OSErr DispatchCalcMenuBar(CalcMenuBarParams *params);

/*
 * MenuDispatch - Main dispatch function
 */
OSErr MenuDispatch(SInt16 selector, void *params) {
    if (!params && selector != selectMenuCalc) {
        return paramErr;
    }

    switch (selector) {
        case selectInsertFontResMenu:
            return DispatchInsertFontResMenu((InsertFontResMenuParams *)params);

        case selectInsertIntlResMenu:
            return DispatchInsertIntlResMenu((InsertIntlResMenuParams *)params);

        case selectGetMenuTitleRect:
            return DispatchGetMenuTitleRect((GetMenuTitleRectParams *)params);

        case selectGetMBARRect:
            return DispatchGetMBARRect((GetMBARRectParams *)params);

        case selectGetAppMenusRect:
            return DispatchGetAppMenusRect((GetAppMenusRectParams *)params);

        case selectGetSysMenusRect:
            return DispatchGetSysMenusRect((GetSysMenusRectParams *)params);

        case selectDrawMBARString:
            return DispatchDrawMBARString((DrawMBARStringParams *)params);

        case selectIsSystemMenu:
            return DispatchIsSystemMenu((IsSystemMenuParams *)params);

        case selectMenuCalc:
            return DispatchCalcMenuBar((CalcMenuBarParams *)params);

        default:
            return unimpErr;
    }
}

/*
 * DispatchInsertFontResMenu - Insert font resource menu
 */
static OSErr DispatchInsertFontResMenu(InsertFontResMenuParams *params) {
    /* Call existing implementation from menu_resources.c */
    /*
     * Three arguments, not two. The declaration here said two and the call
     * below passed two, while the function - declared in MenuManager.h and
     * defined in MenuItems.c - takes a script filter as well. It was reading
     * whatever happened to be on the stack as the script to filter by.
     */
    extern void InsertFontResMenu(MenuHandle theMenu, SInt16 afterItem,
                                  SInt16 scriptFilter);

    if (!params || !params->theMenu) {
        return paramErr;
    }

    InsertFontResMenu(params->theMenu, params->afterItem, 0 /* all scripts */);
    return noErr;
}

/*
 * DispatchInsertIntlResMenu - Insert international resource menu
 */
static OSErr DispatchInsertIntlResMenu(InsertIntlResMenuParams *params) {
    /* Call existing implementation from menu_resources.c */
    /*
     * This disagrees with MenuManager.h too, which declares four parameters -
     * theMenu, theType, afterItem, scriptFilter - and the dispatch parameter
     * block has no resource type in it to supply. It is left as it stands
     * because the function has no definition anywhere in the tree: correcting
     * the declaration would only move an unbuildable call from wrong
     * arguments to a missing symbol. Both need doing together.
     */
    extern void InsertIntlResMenu(MenuHandle theMenu, SInt16 afterItem, SInt16 scriptTag);

    if (!params || !params->theMenu) {
        return paramErr;
    }

    InsertIntlResMenu(params->theMenu, params->afterItem, params->scriptTag);
    return noErr;
}

/*
 * DispatchGetMenuTitleRect - Get menu title rectangle
 */
static OSErr DispatchGetMenuTitleRect(GetMenuTitleRectParams *params) {
    if (!params || !params->theMenu || !params->titleRect) {
        return paramErr;
    }

    return GetMenuTitleRect(params->theMenu, params->titleRect);
}

/*
 * DispatchGetMBARRect - Get menu bar rectangle
 */
static OSErr DispatchGetMBARRect(GetMBARRectParams *params) {
    if (!params || !params->mbarRect) {
        return paramErr;
    }

    return GetMBARRect(params->mbarRect);
}

/*
 * DispatchGetAppMenusRect - Get application menus rectangle
 */
static OSErr DispatchGetAppMenusRect(GetAppMenusRectParams *params) {
    if (!params || !params->appRect) {
        return paramErr;
    }

    return GetAppMenusRect(params->appRect);
}

/*
 * DispatchGetSysMenusRect - Get system menus rectangle
 */
static OSErr DispatchGetSysMenusRect(GetSysMenusRectParams *params) {
    if (!params || !params->sysRect) {
        return paramErr;
    }

    return GetSysMenusRect(params->sysRect);
}

/*
 * DispatchDrawMBARString - Draw string in menu bar
 */
static OSErr DispatchDrawMBARString(DrawMBARStringParams *params) {
    if (!params || !params->text || !params->bounds) {
        return paramErr;
    }

    return DrawMBARString(params->text, params->script, params->bounds, params->just);
}

/*
 * DispatchIsSystemMenu - Check if menu is system menu
 */
static OSErr DispatchIsSystemMenu(IsSystemMenuParams *params) {
    if (!params || !params->result) {
        return paramErr;
    }

    *params->result = IsSystemMenu(params->menuID);
    return noErr;
}

/*
 * DispatchCalcMenuBar - Calculate menu bar layout
 */
static OSErr DispatchCalcMenuBar(CalcMenuBarParams *params) {
    /* CalcMenuBar takes no parameters */
    return CalcMenuBar();
}

/* Convenience wrapper functions */

OSErr CallInsertFontResMenu(MenuHandle theMenu, SInt16 afterItem) {
    InsertFontResMenuParams params = { theMenu, afterItem };
    return MenuDispatch(selectInsertFontResMenu, &params);
}

OSErr CallInsertIntlResMenu(MenuHandle theMenu, SInt16 afterItem, SInt16 scriptTag) {
    InsertIntlResMenuParams params = { theMenu, afterItem, scriptTag };
    return MenuDispatch(selectInsertIntlResMenu, &params);
}

OSErr CallGetMenuTitleRect(MenuHandle theMenu, Rect *titleRect) {
    GetMenuTitleRectParams params = { theMenu, titleRect };
    return MenuDispatch(selectGetMenuTitleRect, &params);
}

OSErr CallGetMBARRect(Rect *mbarRect) {
    GetMBARRectParams params = { mbarRect };
    return MenuDispatch(selectGetMBARRect, &params);
}

OSErr CallGetAppMenusRect(Rect *appRect) {
    GetAppMenusRectParams params = { appRect };
    return MenuDispatch(selectGetAppMenusRect, &params);
}

OSErr CallGetSysMenusRect(Rect *sysRect) {
    GetSysMenusRectParams params = { sysRect };
    return MenuDispatch(selectGetSysMenusRect, &params);
}

OSErr CallDrawMBARString(const unsigned char *text, SInt16 script, Rect *bounds, SInt16 just) {
    DrawMBARStringParams params = { text, script, bounds, just };
    return MenuDispatch(selectDrawMBARString, &params);
}

Boolean CallIsSystemMenu(SInt16 menuID) {
    Boolean result = false;
    IsSystemMenuParams params = { menuID, &result };
    MenuDispatch(selectIsSystemMenu, &params);
    return result;
}

OSErr CallCalcMenuBar(void) {
    CalcMenuBarParams params = { 0 };
    return MenuDispatch(selectMenuCalc, &params);
}

/*
 */

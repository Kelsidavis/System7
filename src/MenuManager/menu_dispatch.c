/*
 * menu_dispatch.c - Menu Manager Dispatch Implementation
 *
 * Implementation of Menu Manager dispatch mechanism based on
 * System 7.1 assembly evidence from MenuMgrPriv.a
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 Menu Manager dispatch
 * Evidence: MenuMgrPriv.a dispatch trap $A825 and selector definitions
 */

#include "menu_dispatch.h"
#include "menu_private.h"
#include <stdlib.h>
#include <string.h>

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
 * Evidence: MenuMgrPriv.a:52 - _MenuDispatch OPWORD $A825
 */
OSErr MenuDispatch(int16_t selector, void *params) {
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
 * Evidence: MenuMgrPriv.a:37 - selectInsertFontResMenu EQU 0
 */
static OSErr DispatchInsertFontResMenu(InsertFontResMenuParams *params) {
    /* Call existing implementation from menu_resources.c */
    extern void InsertFontResMenu(MenuHandle theMenu, int16_t afterItem);

    if (!params || !params->theMenu) {
        return paramErr;
    }

    InsertFontResMenu(params->theMenu, params->afterItem);
    return noErr;
}

/*
 * DispatchInsertIntlResMenu - Insert international resource menu
 * Evidence: MenuMgrPriv.a:38 - selectInsertIntlResMenu EQU 1
 */
static OSErr DispatchInsertIntlResMenu(InsertIntlResMenuParams *params) {
    /* Call existing implementation from menu_resources.c */
    extern void InsertIntlResMenu(MenuHandle theMenu, int16_t afterItem, int16_t scriptTag);

    if (!params || !params->theMenu) {
        return paramErr;
    }

    InsertIntlResMenu(params->theMenu, params->afterItem, params->scriptTag);
    return noErr;
}

/*
 * DispatchGetMenuTitleRect - Get menu title rectangle
 * Evidence: MenuMgrPriv.a:64 - selectGetMenuTitleRect EQU -1
 */
static OSErr DispatchGetMenuTitleRect(GetMenuTitleRectParams *params) {
    if (!params || !params->theMenu || !params->titleRect) {
        return paramErr;
    }

    return GetMenuTitleRect(params->theMenu, params->titleRect);
}

/*
 * DispatchGetMBARRect - Get menu bar rectangle
 * Evidence: MenuMgrPriv.a:65 - selectGetMBARRect EQU -2
 */
static OSErr DispatchGetMBARRect(GetMBARRectParams *params) {
    if (!params || !params->mbarRect) {
        return paramErr;
    }

    return GetMBARRect(params->mbarRect);
}

/*
 * DispatchGetAppMenusRect - Get application menus rectangle
 * Evidence: MenuMgrPriv.a:66 - selectGetAppMenusRect EQU -3
 */
static OSErr DispatchGetAppMenusRect(GetAppMenusRectParams *params) {
    if (!params || !params->appRect) {
        return paramErr;
    }

    return GetAppMenusRect(params->appRect);
}

/*
 * DispatchGetSysMenusRect - Get system menus rectangle
 * Evidence: MenuMgrPriv.a:67 - selectGetSysMenusRect EQU -4
 */
static OSErr DispatchGetSysMenusRect(GetSysMenusRectParams *params) {
    if (!params || !params->sysRect) {
        return paramErr;
    }

    return GetSysMenusRect(params->sysRect);
}

/*
 * DispatchDrawMBARString - Draw string in menu bar
 * Evidence: MenuMgrPriv.a:68 - selectDrawMBARString EQU -5
 */
static OSErr DispatchDrawMBARString(DrawMBARStringParams *params) {
    if (!params || !params->text || !params->bounds) {
        return paramErr;
    }

    return DrawMBARString(params->text, params->script, params->bounds, params->just);
}

/*
 * DispatchIsSystemMenu - Check if menu is system menu
 * Evidence: MenuMgrPriv.a:69 - selectIsSystemMenu EQU -6
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
 * Evidence: MenuMgrPriv.a:71 - selectMenuCalc EQU -11
 */
static OSErr DispatchCalcMenuBar(CalcMenuBarParams *params) {
    /* CalcMenuBar takes no parameters */
    return CalcMenuBar();
}

/* Convenience wrapper functions */

OSErr CallInsertFontResMenu(MenuHandle theMenu, int16_t afterItem) {
    InsertFontResMenuParams params = { theMenu, afterItem };
    return MenuDispatch(selectInsertFontResMenu, &params);
}

OSErr CallInsertIntlResMenu(MenuHandle theMenu, int16_t afterItem, int16_t scriptTag) {
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

OSErr CallDrawMBARString(const unsigned char *text, int16_t script, Rect *bounds, int16_t just) {
    DrawMBARStringParams params = { text, script, bounds, just };
    return MenuDispatch(selectDrawMBARString, &params);
}

Boolean CallIsSystemMenu(int16_t menuID) {
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
 * RE-AGENT-TRAILER-JSON: {
 *   "evidence_sources": [
 *     "/home/k/Desktop/os71/sys71src/Internal/Asm/MenuMgrPriv.a"
 *   ],
 *   "trap_implemented": "0xA825",
 *   "dispatch_cases": 9,
 *   "wrapper_functions": 9,
 *   "selector_constants_used": 9
 * }
 */
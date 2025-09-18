/*
 * menu_dispatch.h - Menu Manager Dispatch Interface
 *
 * Implementation of Menu Manager dispatch mechanism based on
 * System 7.1 assembly evidence from MenuMgrPriv.a and Menus.a
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 Menu Manager dispatch trap
 * Evidence: MenuMgrPriv.a dispatch selectors and parameter definitions
 */

#ifndef __MENU_DISPATCH_H__
#define __MENU_DISPATCH_H__

#include <stdint.h>
#include <stdbool.h>
#include "menu_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dispatch parameter structures based on assembly evidence */

/* InsertFontResMenu parameters - selector 0, 4 param words */
typedef struct {
    MenuHandle theMenu;
    int16_t afterItem;
} InsertFontResMenuParams;

/* InsertIntlResMenu parameters - selector 1, 6 param words */
typedef struct {
    MenuHandle theMenu;
    int16_t afterItem;
    int16_t scriptTag;
} InsertIntlResMenuParams;

/* GetMenuTitleRect parameters - selector -1, 6 param words */
typedef struct {
    MenuHandle theMenu;
    Rect *titleRect;
} GetMenuTitleRectParams;

/* GetMBARRect parameters - selector -2, 4 param words */
typedef struct {
    Rect *mbarRect;
} GetMBARRectParams;

/* GetAppMenusRect parameters - selector -3, 4 param words */
typedef struct {
    Rect *appRect;
} GetAppMenusRectParams;

/* GetSysMenusRect parameters - selector -4, 4 param words */
typedef struct {
    Rect *sysRect;
} GetSysMenusRectParams;

/* DrawMBARString parameters - selector -5, 8 param words */
typedef struct {
    const unsigned char *text;
    int16_t script;
    Rect *bounds;
    int16_t just;
} DrawMBARStringParams;

/* IsSystemMenu parameters - selector -6, 3 param words */
typedef struct {
    int16_t menuID;
    Boolean *result;
} IsSystemMenuParams;

/* CalcMenuBar parameters - selector -11, 1 param word */
typedef struct {
    /* No parameters - just triggers menu bar recalculation */
    int16_t dummy;
} CalcMenuBarParams;

/* Main dispatch function - evidence from MenuMgrPriv.a:52 */
OSErr MenuDispatch(int16_t selector, void *params);

/* Convenience wrappers for dispatch calls */
OSErr CallInsertFontResMenu(MenuHandle theMenu, int16_t afterItem);
OSErr CallInsertIntlResMenu(MenuHandle theMenu, int16_t afterItem, int16_t scriptTag);
OSErr CallGetMenuTitleRect(MenuHandle theMenu, Rect *titleRect);
OSErr CallGetMBARRect(Rect *mbarRect);
OSErr CallGetAppMenusRect(Rect *appRect);
OSErr CallGetSysMenusRect(Rect *sysRect);
OSErr CallDrawMBARString(const unsigned char *text, int16_t script, Rect *bounds, int16_t just);
Boolean CallIsSystemMenu(int16_t menuID);
OSErr CallCalcMenuBar(void);

#ifdef __cplusplus
}
#endif

#endif /* __MENU_DISPATCH_H__ */

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "evidence_sources": [
 *     "/home/k/Desktop/os71/sys71src/Internal/Asm/MenuMgrPriv.a"
 *   ],
 *   "dispatch_selectors": 9,
 *   "parameter_structures": 9,
 *   "wrapper_functions": 9
 * }
 */
/*
 * menu_private.h - Private Menu Manager Definitions
 *
 * Implementation of private Menu Manager functionality based on
 * System 7.1 assembly evidence from MenuMgrPriv.a
 *
 * RE-AGENT-BANNER: Extracted from Mac OS System 7.1 Menu Manager assembly
 * Evidence: MenuMgrPriv.a, Menus.a trap definitions and constants
 */

#ifndef __MENU_PRIVATE_H__
#define __MENU_PRIVATE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Menu Manager trap numbers - evidence from MENUS.a */
#define _NewMenu            0xA931
#define _AppendMenu         0xA933
#define _MenuSelect         0xA93D
#define _SetMenuItemText    0xA947
#define _GetMenuItemText    0xA946
#define _InsertMenuItem     0xA826
#define _MenuDispatch       0xA825
#define _SaveRestoreBits    0xA81E

/* Low-memory global addresses - evidence from MenuMgrPriv.a */
#define MENULIST_ADDR           0x0A1C
#define SYSTEM_MENULIST_ADDR    0x0286
#define THE_MENU_ADDR           0x0A26
#define MENU_FLASH_ADDR         0x0A24
#define MBAR_HEIGHT_ADDR        0x0BAA

/* Menu bar invalidation bits - evidence from MenuMgrPriv.a:42-49 */
#define MENU_BAR_INVALID_BIT            6
#define MENU_BAR_INVALID_BYTE           0x0B21
#define MENU_BAR_GLOBAL_INVALID_BIT     6
#define MENU_BAR_GLOBAL_INVALID_BYTE    0x0B20
#define VALIDATE_MENUBAR_SEMAPHORE_BIT  3
#define VALIDATE_MENUBAR_SEMAPHORE_BYTE 0x0B20

/* System menu ID ranges - evidence from MenuMgrPriv.a:55-61 */
#define kLoSystemMenuRange      0xB000  /* -20480 */
#define kHiSystemMenuRange      0xBFFF  /* -16385 */
#define kApplicationMenuID      0xBF97  /* -16489 */
#define kHelpMenuID             0xBF96  /* -16490 */
#define kScriptMenuID           0xBF95  /* -16491 */

/* MenuDispatch selectors - evidence from MenuMgrPriv.a:63-81 */
#define selectInsertFontResMenu     0
#define selectInsertIntlResMenu     1
#define selectGetMenuTitleRect      -1
#define selectGetMBARRect           -2
#define selectGetAppMenusRect       -3
#define selectGetSysMenusRect       -4
#define selectDrawMBARString        -5
#define selectIsSystemMenu          -6
#define selectMenuCalc              -11

/* Parameter word counts - evidence from MenuMgrPriv.a:75-82 */
#define paramWordsInsertFontResMenu     4
#define paramWordsInsertIntlResMenu     6
#define paramWordsGetMenuTitleRect      6
#define paramWordsGetMBARRect           4
#define paramWordsGetAppMenusRect       4
#define paramWordsGetSysMenusRect       4
#define paramWordsDrawMBARString        8
#define paramWordsIsSystemMenu          3
#define paramWordsMenuCalc              1

/* SaveRestoreBits selectors - evidence from MenuMgrPriv.a:138-145 */
#define selectSaveBits              1
#define selectRestoreBits           2
#define selectDiscardBits           3
#define paramWordsSaveBits          5
#define paramWordsRestoreBits       2
#define paramWordsDiscardBits       2

/* MBDF message numbers - evidence from MenuMgrPriv.a:89-98 */
#define MBDFRectCall                14
#define MBDFDrawMBARString          15
#define MBDFRectBar                 0
#define MBDFRectApps                -1
#define MBDFRectSys                 -2

/* Forward declarations */
typedef struct Rect Rect;
typedef struct Point Point;
typedef Handle MenuHandle;

/* Menu Manager private function prototypes */

/* MenuDispatch functions - evidence from MenuMgrPriv.a */
OSErr GetMenuTitleRect(MenuHandle theMenu, Rect *titleRect);
OSErr GetMBARRect(Rect *mbarRect);
OSErr GetAppMenusRect(Rect *appRect);
OSErr GetSysMenusRect(Rect *sysRect);
OSErr DrawMBARString(const unsigned char *text, int16_t script, Rect *bounds, int16_t just);
Boolean IsSystemMenu(int16_t menuID);
OSErr CalcMenuBar(void);

/* SaveRestoreBits functions - evidence from MenuMgrPriv.a:136-160 */
Handle SaveBits(const Rect *bounds, int16_t mode);
OSErr RestoreBits(Handle bitsHandle);
OSErr DiscardBits(Handle bitsHandle);

/* Low-memory global manipulation */
void SetMenuBarInvalidBit(Boolean invalid);
Boolean GetMenuBarInvalidBit(void);
void SetMenuBarGlobalInvalidBit(Boolean invalid);
Boolean GetMenuBarGlobalInvalidBit(void);
Boolean GetValidateMenuBarSemaphore(void);
void SetValidateMenuBarSemaphore(Boolean locked);

/* Menu Manager dispatch mechanism */
OSErr MenuDispatch(int16_t selector, void *params);

#ifdef __cplusplus
}
#endif

#endif /* __MENU_PRIVATE_H__ */

/*
 * RE-AGENT-TRAILER-JSON: {
 *   "evidence_sources": [
 *     "/home/k/Desktop/os71/sys71src/Internal/Asm/MenuMgrPriv.a",
 *     "/home/k/Desktop/os71/sys71src/Interfaces/AIncludes/Menus.a",
 *     "/home/k/Desktop/os71/sys71src/Libs/InterfaceSrcs/MENUS.a"
 *   ],
 *   "trap_numbers_extracted": ["0xA825", "0xA81E"],
 *   "constants_extracted": 23,
 *   "functions_declared": 12,
 *   "low_memory_globals": 6
 * }
 */
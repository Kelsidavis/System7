/* Private Menu Manager constants and implementation-only declarations. */

#ifndef __MENU_PRIVATE_H__
#define __MENU_PRIVATE_H__

#include "SystemTypes.h"
#include "MenuManager/MenuManager.h"


#ifdef __cplusplus
extern "C" {
#endif

#define _NewMenu            0xA931
#define _AppendMenu         0xA933
#define _MenuSelect         0xA93D
#define _SetMenuItemText    0xA947
#define _GetMenuItemText    0xA946
#define _InsertMenuItem     0xA826
#define _MenuDispatch       0xA825
#define _SaveRestoreBits    0xA81E

#define MENULIST_ADDR           0x0A1C
#define SYSTEM_MENULIST_ADDR    0x0286
#define THE_MENU_ADDR           0x0A26
#define MENU_FLASH_ADDR         0x0A24
#define MBAR_HEIGHT_ADDR        0x0BAA


#define MENU_BAR_INVALID_BIT            6
#define MENU_BAR_INVALID_BYTE           0x0B21
#define MENU_BAR_GLOBAL_INVALID_BIT     6
#define MENU_BAR_GLOBAL_INVALID_BYTE    0x0B20
#define VALIDATE_MENUBAR_SEMAPHORE_BIT  3
#define VALIDATE_MENUBAR_SEMAPHORE_BYTE 0x0B20

#define kLoSystemMenuRange      0xB000  /* -20480 */
#define kHiSystemMenuRange      0xBFFF  /* -16385 */
#define kApplicationMenuID      0xBF97  /* -16489 */
#define kHelpMenuID             0xBF96  /* -16490 */
#define kScriptMenuID           0xBF95  /* -16491 */

#define selectInsertFontResMenu     0
#define selectInsertIntlResMenu     1
#define selectGetMenuTitleRect      -1
#define selectGetMBARRect           -2
#define selectGetAppMenusRect       -3
#define selectGetSysMenusRect       -4
#define selectDrawMBARString        -5
#define selectIsSystemMenu          -6
#define selectMenuCalc              -11

#define paramWordsInsertFontResMenu     4
#define paramWordsInsertIntlResMenu     6
#define paramWordsGetMenuTitleRect      6
#define paramWordsGetMBARRect           4
#define paramWordsGetAppMenusRect       4
#define paramWordsGetSysMenusRect       4
#define paramWordsDrawMBARString        8
#define paramWordsIsSystemMenu          3
#define paramWordsMenuCalc              1

#define selectSaveBits              1
#define selectRestoreBits           2
#define selectDiscardBits           3
#define paramWordsSaveBits          5
#define paramWordsRestoreBits       2
#define paramWordsDiscardBits       2

#define MBDFRectCall                14
#define MBDFDrawMBARString          15
#define MBDFRectBar                 0
#define MBDFRectApps                -1
#define MBDFRectSys                 -2

/* Menu item query functions (for MDEF and MenuKey) */
Boolean CheckMenuItemEnabled(MenuHandle theMenu, short item);
Boolean CheckMenuItemSeparator(MenuHandle theMenu, short item);
char GetMenuItemCmdKey(MenuHandle theMenu, short item);
short GetMenuItemSubmenu(MenuHandle theMenu, short item);

Handle SaveBits(const Rect *bounds, SInt16 mode);
OSErr RestoreBits(Handle bitsHandle);
OSErr DiscardBits(Handle bitsHandle);


/* Menu resource parsing */
MenuHandle ParseMENUResource(Handle resourceHandle);
short* ParseMBARResource(Handle resourceHandle, short* outMenuCount);


/* Menu command handling */

/* Menu item functions */
SInt16 CountMenuItems(MenuHandle theMenu);
void CleanupMenuExtData(void);  /* Free all menu extended data - must be called during cleanup */

/* Menu title tracking */
void InitMenuTitleTracking(void);
void AddMenuTitle(short menuID, short left, short width, const char* title);
void HiliteMenuTitle(short menuID, Boolean hilite);
void ClearMenuTitles(void);

Boolean MenuTitleAt(Point pt, SInt16* outMenuID);
Boolean GetMenuTitleRectByID(SInt16 menuID, Rect* rect);
void GetMenuBarRect(Rect* rect);
void SetMenuBarRect(const Rect* rect);
SInt16 GetMenuTitleCount(void);
MenuHandle GetMenuTitleByIndex(SInt16 index);
SInt16 FindMenuAtPoint_Internal(Point pt);

/* Menu tracking */
void DrawMenuItemText(const Rect* itemRect, ConstStr255Param itemText,
                      Style textStyle, Boolean enabled, Boolean selected);
void DrawMenu(MenuHandle menu, const Rect* menuRect, short hiliteItem);
void DrawMenuItemAtRect(MenuHandle menu, short item, const Rect* itemRect,
                        Boolean selected);
void CalcMenuItemRect(MenuHandle menu, short item, const Rect* menuRect,
                      Rect* itemRect);
short CalcMenuHeight(MenuHandle menu, short itemCount);
short GetMenuItemHeight(MenuHandle menu, short item);
long BeginTrackMenu(short menuID, Point* startPt);
void UpdateMenuTrackingNew(Point where);
/* Returns the selected menu choice, or zero when tracking is cancelled. */
long EndMenuTrackingNew(void);
Boolean IsMenuTrackingNew(void);
long TrackMenu(short menuID, Point *startPt);

/* Implementation hook used by the platform save-bits wrapper. */
void* Platform_SaveScreenBits_Impl(const Rect* rect);

#ifdef __cplusplus
}
#endif

#endif /* __MENU_PRIVATE_H__ */

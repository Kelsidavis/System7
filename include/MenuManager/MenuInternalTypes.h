/*
 * MenuInternalTypes.h - Internal Menu Manager Type Definitions
 *
 * Defines internal types used by Menu Manager implementation
 */

#ifndef __MENU_INTERNAL_TYPES_H__
#define __MENU_INTERNAL_TYPES_H__

#include "SystemTypes.h"
#include "MenuTypes.h"

/* Menu tracking information */
typedef struct MenuTrackingState MenuTrackInfo;

/* Menu selection result */
typedef struct {
    short menuID;
    short itemID;
    long menuChoice;
    Boolean valid;
    Boolean cancelled;
    Point clickPoint;
    Point finalMousePoint;
    unsigned long selectionTime;
} MenuSelection;

/* Command key search information */
typedef struct {
    short searchChar;
    unsigned long modifiers;
    short foundMenuID;
    short foundMenu;     /* Alias for foundMenuID */
    short foundItem;
    Boolean found;
    Boolean enabled;
} MenuCmdSearch;

/* Hierarchical menu tracking */
typedef struct {
    short parentMenuID;
    short parentItem;
    short childMenuID;
    Point anchorPoint;
    Rect parentRect;
} HierMenuTrack;

/* Menu list entry */
typedef struct {
    short menuID;
    short menuLeft;
    short menuWidth;
} MenuListEntry;

/* Menu bar list structure */
typedef struct {
    short numMenus;
    short totalWidth;
    short lastRight;
    short mbResID;
    MenuListEntry menus[1];  /* Variable length array */
} MenuBarList;

/* The Menu Manager's bar list, for code that reaches it by another route
 * than gMenuList. Declared here, next to the type it returns. */
MenuBarList* MenuMgr_GetMenuBarList(void);

/* Menu bar drawing information */
typedef struct {
    Rect menuBarRect;
    short drawMode;
    short hiliteMenu;
    Boolean useColor;
    Boolean antiAlias;
    void* context;
} MenuBarDrawInfo;

/* Menu drawing information */
typedef struct {
    MenuHandle menu;
    Rect menuRect;
    short drawFlags;
    short hiliteItem;
    Boolean useColor;
    Boolean antiAlias;
    void* context;
} MenuDrawInfo;

/* Menu item drawing information */
typedef struct {
    MenuHandle menu;
    short itemNum;
    Rect itemRect;
    Str255 itemText;
    short itemFlags;
    Boolean hilited;
    Boolean disabled;
    Boolean hasIcon;
    Boolean hasCmd;
    Boolean hasSubmenu;
    short iconID;
    char cmdChar;
    char markChar;
    short textStyle;
    void* context;
} MenuItemDrawInfo;

/* Menu drawing context */
typedef struct {
    GrafPtr port;
    RgnHandle clipRgn;
    Pattern fillPat;
    short textFont;
    short textSize;
    short textFace;
    short textMode;
    short textStyle;
    Boolean colorMode;
    Boolean antiAlias;
    RGBColor foreColor;
    RGBColor backColor;
} MenuDrawContext;

#endif /* __MENU_INTERNAL_TYPES_H__ */
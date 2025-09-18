/*
 * MenuTypes.h - Menu Manager Data Structures and Types
 *
 * Detailed data structures, constants, and internal types for the
 * Portable Menu Manager implementation. This header provides the
 * complete type definitions needed for menu management.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Menu Manager
 */

#ifndef __MENU_TYPES_H__
#define __MENU_TYPES_H__

#include "MenuManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Menu Record Layout Constants
 * ============================================================================ */

/* MenuInfo structure offsets (for low-level access) */
enum {
    menuIDOffset        = 0,            /* Offset to menuID field */
    menuWidthOffset     = 2,            /* Offset to menuWidth field */
    menuHeightOffset    = 4,            /* Offset to menuHeight field */
    menuProcOffset      = 6,            /* Offset to menuProc field */
    enableFlagsOffset   = 10,           /* Offset to enableFlags field */
    menuDataOffset      = 14            /* Offset to menuData field */
};

/* Menu data parsing constants */
enum {
    maxMenuItems        = 31,           /* Maximum items per menu for enableFlags */
    maxMenuTitleLength  = 255,          /* Maximum menu title length */
    maxMenuItemLength   = 255,          /* Maximum menu item text length */
    menuBarStdHeight    = 20,           /* Standard menu bar height */
    menuItemStdHeight   = 16,           /* Standard menu item height */
    menuMargin          = 8,            /* Standard menu margin */
    menuTextMargin      = 14,           /* Text margin in menu items */
    menuIconWidth       = 16,           /* Standard menu icon width */
    menuMarkWidth       = 16,           /* Width for mark characters */
    menuCmdKeyMargin    = 16            /* Margin for command key symbols */
};

/* ============================================================================
 * Menu Item Parsing and Storage
 * ============================================================================ */

/* Menu item attributes (internal representation) */
typedef struct MenuItemRec {
    struct MenuItemRec* next;           /* Next item in linked list */
    Str255          text;               /* Item display text */
    unsigned char   iconID;             /* Icon ID (0 = none) */
    unsigned char   cmdChar;            /* Command key character */
    unsigned char   markChar;           /* Mark character (check, etc.) */
    unsigned char   styleFlags;        /* Text style flags */
    short           hierarchicalID;     /* Submenu ID for hierarchical items */
    Boolean         enabled;            /* Item enabled state */
    Boolean         hasSubmenu;         /* TRUE if hierarchical item */
    Boolean         isDivider;          /* TRUE if divider line */
    short           itemHeight;         /* Cached item height */
    void*           customData;         /* Custom MDEF data */
} MenuItemRec;

typedef MenuItemRec* MenuItemPtr;

/* Menu list entry (for menu bar) */
typedef struct MenuListEntry {
    short           menuID;             /* Menu resource ID */
    short           menuLeft;           /* Left position in menu bar */
    short           menuWidth;          /* Width in menu bar */
} MenuListEntry;

/* Menu bar list structure */
typedef struct MenuBarList {
    short           numMenus;           /* Number of menus */
    short           totalWidth;         /* Total width of all menus */
    short           lastRight;          /* Right edge of last menu */
    short           mbResID;            /* MBAR resource ID */
    MenuListEntry   menus[1];           /* Variable-length array */
} MenuBarList;

typedef MenuBarList* MenuBarPtr;
typedef MenuBarList** MenuBarHandle;

/* ============================================================================
 * Menu Definition Procedure Support
 * ============================================================================ */

/* Menu definition messages (extended) */
enum {
    mDrawMsg            = 0,            /* Draw menu */
    mChooseMsg          = 1,            /* Handle menu selection */
    mSizeMsg            = 2,            /* Calculate menu size */
    mPopUpMsg           = 3,            /* Position popup menu */
    mDrawItemMsg        = 4,            /* Draw specific item */
    mCalcItemMsg        = 5,            /* Calculate item size */
    mInitMsg            = 6,            /* Initialize menu */
    mDisposeMsg         = 7,            /* Dispose menu */
    mThumbMsg           = 8,            /* Thumb feedback */
    mDrawItemIconMsg    = 9,            /* Draw item icon */
    mSaveMenuMsg        = 10,           /* Save menu state */
    mRestoreMenuMsg     = 11            /* Restore menu state */
};

/* Menu definition parameters */
typedef struct MenuDefParam {
    short           message;            /* Message code */
    MenuHandle      menu;               /* Menu handle */
    Rect*           menuRect;           /* Menu rectangle */
    Point           hitPoint;           /* Mouse location */
    short*          whichItem;          /* Item number result */
    void*           mdefData;           /* MDEF-specific data */
} MenuDefParam;

/* Menu definition procedure */
typedef OSErr (*MenuDefProcPtr)(MenuDefParam* params);

/* Standard menu definition IDs */
enum {
    textMenuDef         = 0,            /* Standard text menu */
    iconMenuDef         = 1,            /* Icon menu */
    colorMenuDef        = 2,            /* Color menu */
    pictMenuDef         = 3             /* Picture menu */
};

/* ============================================================================
 * Menu Color Support (Enhanced)
 * ============================================================================ */

/* Menu color component IDs */
enum {
    mcMenuBar           = 0,            /* Menu bar colors */
    mcMenuTitle         = 1,            /* Menu title colors */
    mcMenuItem          = 2,            /* Menu item colors */
    mcMenuBackground    = 3,            /* Menu background */
    mcMenuSeparator     = 4,            /* Separator line */
    mcMenuIcon          = 5,            /* Menu icon colors */
    mcMenuCmdKey        = 6,            /* Command key colors */
    mcMenuMark          = 7,            /* Mark character colors */
    mcMenuHilite        = 8,            /* Highlight colors */
    mcMenuDisabled      = 9             /* Disabled item colors */
};

/* Extended menu color entry */
typedef struct ExtMCEntry {
    short           componentID;        /* Color component ID */
    short           menuID;             /* Menu ID (0 = all menus) */
    short           itemID;             /* Item ID (0 = all items) */
    RGBColor        foreColor;          /* Foreground color */
    RGBColor        backColor;          /* Background color */
    RGBColor        hiliteColor;        /* Highlight color */
    RGBColor        dimColor;           /* Dimmed/disabled color */
    short           flags;              /* Color flags */
    short           reserved;           /* Reserved for future use */
} ExtMCEntry;

typedef ExtMCEntry* ExtMCEntryPtr;

/* Menu color flags */
enum {
    mcUseDefaultColors  = 0x0001,       /* Use system default colors */
    mcInheritColors     = 0x0002,       /* Inherit from parent */
    mcCustomPattern     = 0x0004,       /* Use custom pattern */
    mcAntiAlias         = 0x0008        /* Anti-alias text */
};

/* ============================================================================
 * Menu State and Tracking
 * ============================================================================ */

/* Menu tracking state */
typedef struct MenuTrackingState {
    Boolean         active;             /* Currently tracking */
    MenuHandle      currentMenu;        /* Menu being tracked */
    short           currentItem;        /* Current item */
    short           lastItem;           /* Last highlighted item */
    Point           initialPt;          /* Initial click point */
    Point           currentPt;          /* Current mouse point */
    unsigned long   trackingStartTime;  /* Start time for tracking */
    Boolean         mouseStillDown;     /* Mouse button state */
    Boolean         itemWasSelected;    /* An item was selected */
    short           selectedMenuID;     /* Selected menu ID */
    short           selectedItem;       /* Selected item number */
    Rect            menuRect;           /* Current menu rectangle */
    RgnHandle       savedRegion;        /* Saved screen region */
    void*           savedBits;          /* Saved screen bits */
} MenuTrackingState;

/* Menu bar state */
typedef struct MenuBarState {
    Boolean         visible;            /* Menu bar visible */
    Boolean         active;             /* Menu bar active */
    Boolean         needsRedraw;        /* Needs redrawing */
    short           hiliteMenu;         /* Currently highlighted menu */
    short           trackingMenu;       /* Menu being tracked */
    Rect            menuBarRect;        /* Menu bar rectangle */
    short           height;             /* Menu bar height */
    Handle          menuList;           /* Current menu list */
    short           menuCount;          /* Number of menus */
    MenuHandle*     menus;              /* Array of menu handles */
    short*          menuLefts;          /* Array of menu left positions */
    short*          menuWidths;         /* Array of menu widths */
} MenuBarState;

/* ============================================================================
 * Platform Integration Structures
 * ============================================================================ */

/* Platform menu data (opaque to portable code) */
typedef struct PlatformMenuData {
    void*           nativeMenu;         /* Platform-specific menu object */
    void*           nativeMenuBar;      /* Platform-specific menu bar */
    void*           renderingContext;   /* Platform rendering context */
    Boolean         useNativeMenus;     /* Use native platform menus */
    Boolean         supportsColors;     /* Platform supports menu colors */
    Boolean         supportsIcons;      /* Platform supports menu icons */
    Boolean         supportsHierarchical; /* Platform supports submenus */
    short           platformFlags;      /* Platform-specific flags */
} PlatformMenuData;

/* Drawing context for menu rendering */
typedef struct MenuDrawingContext {
    GrafPtr         grafPort;           /* Graphics port for drawing */
    Rect            bounds;             /* Drawing bounds */
    short           textFont;           /* Current text font */
    short           textSize;           /* Current text size */
    short           textStyle;          /* Current text style */
    RGBColor        foreColor;          /* Foreground color */
    RGBColor        backColor;          /* Background color */
    Boolean         colorMode;          /* TRUE if color drawing */
    Boolean         antiAlias;          /* TRUE if anti-aliased text */
    void*           platformContext;    /* Platform drawing context */
} MenuDrawingContext;

/* ============================================================================
 * Menu Resource Structures
 * ============================================================================ */

/* MENU resource header */
typedef struct MenuResource {
    short           menuID;             /* Menu ID */
    short           menuWidth;          /* Menu width (0 = calculate) */
    short           menuHeight;         /* Menu height (0 = calculate) */
    short           menuProc;           /* Menu definition procedure ID */
    short           filler;             /* Unused */
    long            enableFlags;       /* Enable flags for items 0-31 */
    Str255          menuTitle;          /* Menu title */
    /* Followed by variable-length item data */
} MenuResource;

typedef MenuResource* MenuResourcePtr;
typedef MenuResource** MenuResourceHandle;

/* MBAR resource structure */
typedef struct MBarResource {
    short           numMenus;           /* Number of menus */
    short           menuIDs[1];         /* Variable-length array of menu IDs */
} MBarResource;

typedef MBarResource* MBarResourcePtr;
typedef MBarResource** MBarResourceHandle;

/* ============================================================================
 * Menu Manager Error Codes (Extended)
 * ============================================================================ */

enum {
    /* Menu Manager specific errors */
    menuNoErr           = 0,            /* No error */
    menuNotFoundErr     = -126,         /* Menu not found */
    menuItemNotFoundErr = -127,         /* Menu item not found */
    menuInvalidErr      = -128,         /* Invalid menu handle */
    menuBarFullErr      = -129,         /* Menu bar is full */
    menuItemFullErr     = -130,         /* Menu has too many items */
    menuColorErr        = -131,         /* Menu color error */
    menuResourceErr     = -132,         /* Menu resource error */
    menuMemoryErr       = -133,         /* Menu memory allocation error */
    menuPlatformErr     = -134,         /* Platform-specific error */
    menuDefProcErr      = -135,         /* Menu definition procedure error */
    menuTrackingErr     = -136          /* Menu tracking error */
};

/* ============================================================================
 * Menu Manager Internal State Structure (Complete)
 * ============================================================================ */

/* Complete Menu Manager global state */
typedef struct MenuManagerGlobals {
    /* Basic state */
    Boolean             initialized;            /* Menu Manager initialized */
    Boolean             colorQDAvailable;       /* Color QuickDraw available */

    /* Menu bar state */
    MenuBarState        menuBar;                /* Menu bar information */

    /* Menu tracking */
    MenuTrackingState   tracking;               /* Menu tracking state */

    /* Colors and appearance */
    MCTableHandle       colorTable;             /* Menu color table */
    short               flashCount;             /* Flash count for feedback */
    Pattern             menuPattern;            /* Menu background pattern */

    /* System menus */
    Handle              systemMenuList;         /* System menu list */
    short               appleMenuID;            /* Apple menu ID */
    short               applicationMenuID;      /* Application menu ID */
    short               helpMenuID;             /* Help menu ID */

    /* Menu definition procedures */
    Handle              standardMDEFs[8];       /* Standard MDEFs */

    /* Platform integration */
    PlatformMenuData    platform;              /* Platform-specific data */

    /* Caching and performance */
    MenuHandle          lastMenuAccessed;       /* Last accessed menu (cache) */
    short               lastMenuID;             /* Last menu ID accessed */

    /* Menu validation */
    Boolean             menuListValid;          /* Menu list is valid */
    Boolean             menuBarValid;           /* Menu bar is valid */

    /* Resource management */
    short               nextTempMenuID;         /* Next temporary menu ID */
    Handle              menuResourceCache[16];  /* Menu resource cache */

    /* Compatibility flags */
    Boolean             classicMode;            /* Classic Mac OS compatibility */
    Boolean             modernEnhancements;     /* Modern enhancements enabled */
} MenuManagerGlobals;

/* ============================================================================
 * Utility Macros and Functions
 * ============================================================================ */

/* Menu handle validation */
#define IsValidMenuHandle(menu) \
    ((menu) != NULL && (*(menu)) != NULL && (*(menu))->menuID != 0)

/* Menu ID validation */
#define IsValidMenuID(id) \
    ((id) != 0)

/* Menu item validation */
#define IsValidMenuItem(menu, item) \
    (IsValidMenuHandle(menu) && (item) > 0 && (item) <= CountMItems(menu))

/* Extract menu ID and item from MenuSelect result */
#define MenuID(result)      ((short)((result) >> 16))
#define MenuItem(result)    ((short)((result) & 0xFFFF))

/* Create MenuSelect result */
#define MenuResult(menuID, item) \
    (((long)(menuID) << 16) | ((long)(item) & 0xFFFF))

/* Menu enable flag manipulation */
#define EnableMenuFlag(flags, item)     ((flags) |= (1L << (item)))
#define DisableMenuFlag(flags, item)    ((flags) &= ~(1L << (item)))
#define IsMenuItemEnabled(flags, item)  (((flags) & (1L << (item))) != 0)

/* Menu data parsing helpers */
#define MenuTitleLength(menu)   ((*(menu))->menuData[0])
#define MenuTitlePtr(menu)      (&(*(menu))->menuData[1])
#define MenuItemData(menu)      (&(*(menu))->menuData[MenuTitleLength(menu) + 1])

#ifdef __cplusplus
}
#endif

#endif /* __MENU_TYPES_H__ */
/* #include "SystemTypes.h" */
#include "MenuManager/menu_private.h"
/*
 * PopupMenus.c - Popup Menu Implementation
 *
 * This file implements popup menu functionality including positioning,
 * display, and context menu support.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/PopupMenus.h"


/* Popup menu selection */
long PopUpMenuSelect(MenuHandle menu, short top, short left, short popUpItem) {
    if (!menu) return 0;

    /* External functions for tracking */
    extern long TrackMenu(short menuID, Point *startPt);
    extern void HiliteMenu(short menuID);

    /* Calculate popup position - adjust if popUpItem is specified */
    Point location;
    location.v = top;
    location.h = left;

    /* If popUpItem is specified, adjust vertical position so that item appears at top */
    if (popUpItem > 0) {
        short lineHeight = 16;  /* Standard menu item height */
        location.v = top - (popUpItem - 1) * lineHeight - 2;  /* Account for top padding */
    }

    /* Track the popup menu - TrackMenu handles all mouse tracking and selection */
    long result = TrackMenu((*menu)->menuID, &location);

    /* Un-highlight the menu (if it was highlighted in menu bar) */
    HiliteMenu(0);

    return result;
}

/* Extended popup menu functions */
short PopUpMenuSelectEx(const PopupMenuInfo* popupInfo, PopupMenuResult* result) {
    if (!popupInfo || !result) return -1;

    /* Delegate to standard popup menu select */
    long menuResult = PopUpMenuSelect(popupInfo->menu, popupInfo->popupRect.top,
                                       popupInfo->popupRect.left, popupInfo->defaultItem);
    result->menuID = (menuResult >> 16) & 0xFFFF;
    result->menuItem = menuResult & 0xFFFF;
    return (menuResult != 0) ? 0 : -1;
}

Boolean ShowPopupMenu(MenuHandle theMenu, Point location, short positionMode, short alignItem) {
    if (!theMenu) return false;

    /* Use PopUpMenuSelect to display the menu at the specified location */
    long result = PopUpMenuSelect(theMenu, location.v, location.h, alignItem);
    return (result != 0);
}

void HidePopupMenu(void) {
    extern void HiliteMenu(short menuID);
    HiliteMenu(0);
}

/* Context menu support */
short ShowContextMenu(const ContextMenuInfo* contextInfo, PopupMenuResult* result) {
    if (!contextInfo || !result) return -1;

    /* Display context menu at the trigger location */
    if (!contextInfo->menu) return -1;
    long menuResult = PopUpMenuSelect(contextInfo->menu,
                                       contextInfo->triggerPoint.v,
                                       contextInfo->triggerPoint.h, 0);
    result->menuID = (menuResult >> 16) & 0xFFFF;
    result->menuItem = menuResult & 0xFFFF;
    return (menuResult != 0) ? 0 : -1;
}

Boolean IsContextMenuTrigger(Point mousePoint, unsigned long modifiers, short clickType) {
    /* Check for right-click or control-click */
    return (clickType == kContextMenuRightClick) ||
           ((modifiers & kMenuControlModifiers) && (clickType == kContextMenuControlClick));
}

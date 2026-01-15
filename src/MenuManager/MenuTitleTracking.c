/*
 * MenuTitleTracking.c - Menu Title Hit-Testing and Position Tracking
 *
 * Tracks menu title positions in the menu bar for proper hit-testing
 * and dropdown positioning.
 */

#include <string.h>
#include "SystemTypes.h"

/* Printf for debugging */
#include "MenuManager/MenuManager.h"
#include "MenuManager/menu_private.h"
#include "MenuManager/MenuLogging.h"
#include "MenuManager/MenuTypes.h"

/* Menu title tracking structure */
typedef struct {
    short menuID;
    Rect titleRect;
    char titleText[256];
} MenuTitleSlot;

/* Global menu title tracking */
#define MAX_MENU_TITLES 16
static MenuTitleSlot gMenuTitles[MAX_MENU_TITLES];
static short gMenuTitleCount = 0;
static Rect gMenuBarRect = {0, 0, 20, 800};  /* Default menu bar rect */

/* Initialize menu title tracking */
void InitMenuTitleTracking(void)
{
    gMenuTitleCount = 0;
    memset(gMenuTitles, 0, sizeof(gMenuTitles));

    /* Set default menu bar rect */
    gMenuBarRect.top = 0;
    gMenuBarRect.left = 0;
    gMenuBarRect.bottom = 20;  /* Standard menu bar height */
    gMenuBarRect.right = 800;  /* Screen width */
}

/* Add a menu title to tracking */
void AddMenuTitle(short menuID, short left, short width, const char* title)
{
    /* serial_printf removed - can hang on ARM64 */

    /* CRITICAL FIX: Check if this menuID already exists
     * If it does, UPDATE the existing entry instead of creating a duplicate.
     * This prevents multiple titleRect entries for the same menu which causes
     * duplicate inversions when highlighting menus. */
    for (int i = 0; i < gMenuTitleCount; i++) {
        if (gMenuTitles[i].menuID == menuID) {
            /* Update existing entry */
            gMenuTitles[i].titleRect.left = left;
            gMenuTitles[i].titleRect.right = left + width;
            gMenuTitles[i].titleRect.top = 0;
            gMenuTitles[i].titleRect.bottom = 20;

            if (title) {
                strncpy(gMenuTitles[i].titleText, title, 255);
                gMenuTitles[i].titleText[255] = '\0';
            }

            /* Debug removed - serial_printf can hang on ARM64 */
            MENU_LOG_TRACE("Updated menu title: ID=%d, left=%d, width=%d, title='%s'\n",
                          menuID, left, width, title ? title : "");
            return;
        }
    }

    /* No existing entry - create new one */
    if (gMenuTitleCount >= MAX_MENU_TITLES) {
        /* Debug removed - serial_printf can hang on ARM64 */
        return;
    }

    MenuTitleSlot* slot = &gMenuTitles[gMenuTitleCount];
    slot->menuID = menuID;
    slot->titleRect.left = left;
    slot->titleRect.right = left + width;
    slot->titleRect.top = 0;
    slot->titleRect.bottom = 20;

    if (title) {
        strncpy(slot->titleText, title, 255);
        slot->titleText[255] = '\0';
    }

    gMenuTitleCount++;

    /* Debug removed - serial_printf can hang on ARM64 */
    MENU_LOG_TRACE("Added menu title: ID=%d, left=%d, width=%d, title='%s'\n",
                  menuID, left, width, title ? title : "");
}

/* Clear all menu titles */
void ClearMenuTitles(void)
{
    gMenuTitleCount = 0;
    memset(gMenuTitles, 0, sizeof(gMenuTitles));
}

/* Find menu at a given point */
Boolean MenuTitleAt(Point pt, short* outMenuID)
{
    if (!outMenuID) {
        return false;
    }

    *outMenuID = 0;

    /* Check if point is in menu bar */
    if (pt.v < gMenuBarRect.top || pt.v >= gMenuBarRect.bottom) {
        return false;
    }

    /* Check each menu title */
    for (short i = 0; i < gMenuTitleCount; i++) {
        const Rect* r = &gMenuTitles[i].titleRect;
        if (pt.h >= r->left && pt.h < r->right &&
            pt.v >= r->top && pt.v < r->bottom) {
            *outMenuID = gMenuTitles[i].menuID;
            return true;
        }
    }

    return false;
}

/* Get menu title rect */
Boolean GetMenuTitleRectByID(short menuID, Rect* outRect)
{
    if (!outRect) {
        return false;
    }

    for (short i = 0; i < gMenuTitleCount; i++) {
        if (gMenuTitles[i].menuID == menuID) {
            *outRect = gMenuTitles[i].titleRect;
            return true;
        }
    }

    return false;
}

/* Get menu bar rect */
void GetMenuBarRect(Rect* outRect)
{
    if (outRect) {
        *outRect = gMenuBarRect;
    }
}

/* Update menu bar dimensions */
void SetMenuBarRect(const Rect* rect)
{
    if (rect) {
        gMenuBarRect = *rect;
    }
}

/* Get menu title count */
short GetMenuTitleCount(void)
{
    return gMenuTitleCount;
}

/* Get menu title info by index */
MenuHandle GetMenuTitleByIndex(SInt16 index)
{
    if (index < 0 || index >= gMenuTitleCount) {
        return NULL;
    }

    /* Get the menu by ID - this requires GetMenuHandle which is in MenuList.c */
    /* For now, return NULL as we don't have access to the menu list here */
    extern MenuHandle GetMenuHandle(short menuID);

    MenuTitleSlot* slot = &gMenuTitles[index];
    return GetMenuHandle(slot->menuID);
}

/* Export for MenuSelection.c to use */
short FindMenuAtPoint_Internal(Point pt)
{
    short menuID = 0;
    MenuTitleAt(pt, &menuID);
    return menuID;
}
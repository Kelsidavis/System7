/*
 * MenuPlatform.c - Platform integration for the Menu Manager
 *
 * Provides input tracking and compatibility fallbacks for menu operations
 * not handled directly by MenuDisplay. Legacy save-bit hooks delegate to the
 * shared menu save/restore implementation.
 */

#include "SystemTypes.h"
#include "EventManager/EventManager.h"
#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuDisplay.h"
#include "MenuManager/menu_private.h"
#include "Platform/PS2Input.h"

void Platform_RestoreScreenBits(Handle bits, const Rect* rect)
{
    (void)rect;
    (void)RestoreMenuBits(bits);
}

void Platform_DisposeScreenBits(Handle bits)
{
    (void)DiscardMenuBits(bits);
}

/* Platform drawing functions - forward to MenuDisplay routines */
/*
 * Platform_DrawMenuBar
 * Draws the menu bar (delegated to MenuDisplay implementation)
 */
void Platform_DrawMenuBar(const void* drawInfo)
{
    (void)drawInfo;
    /* MenuDisplay module handles actual rendering */
    /* This is just a platform abstraction point */
}

/*
 * Platform_DrawMenu
 * Draws a menu when it's opened (delegated to MenuDisplay)
 */
void Platform_DrawMenu(const void* drawInfo)
{
    (void)drawInfo;
    /* MenuDisplay module handles actual rendering */
}

/*
 * Platform_DrawMenuItem
 * Draws a single menu item (delegated to MenuDisplay)
 */
void Platform_DrawMenuItem(const void* drawInfo)
{
    (void)drawInfo;
    /* MenuDisplay module handles actual rendering */
}

/* Platform tracking functions */
/*
 * Platform_TrackMouse
 * Provides mouse position and button state for menu tracking
 */
Boolean Platform_TrackMouse(Point* mousePt, Boolean* isMouseDown)
{
    if (!mousePt || !isMouseDown) {
        return false;
    }

    /* Get current mouse state from system */
    GetMouse(mousePt);
    *isMouseDown = Button();

    return true;
}

/*
 * Platform_GetKeyModifiers
 * Returns current keyboard modifier state
 */
Boolean Platform_GetKeyModifiers(unsigned long* modifiers)
{
    if (!modifiers) {
        return false;
    }

    /* Get current modifier key state from PS2 controller */
    *modifiers = (unsigned long)GetPS2Modifiers();
    return true;
}

/*
 * Platform_SetMenuCursor
 * Changes cursor appearance during menu tracking
 */
void Platform_SetMenuCursor(short cursorType)
{
    (void)cursorType;
    /* Cursor management delegated to platform layer */
    /* cursorType: 0=arrow, 1=pointer, 2=watch, etc. */
}

/*
 * Platform_IsMenuVisible
 * Checks if a menu is currently visible on screen
 */
Boolean Platform_IsMenuVisible(void* theMenu)
{
    /* Menu visibility tracking would be implemented here */
    /* For now, assume menus are visible when requested */
    return theMenu != NULL;
}

/*
 * Platform_MenuFeedback
 * Provides visual feedback during menu interaction
 */
void Platform_MenuFeedback(short feedbackType, short menuID, short item)
{
    (void)feedbackType;
    (void)menuID;
    (void)item;
    /* feedbackType: 0=hilite, 1=unhilite, 2=flash, etc. */
    /* Could flash menu bar or provide other visual feedback */
}

Handle Platform_SaveScreenBits(const Rect* rect)
{
    return SaveMenuBits(rect);
}

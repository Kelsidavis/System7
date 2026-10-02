/*
 * platform_stubs.c - Platform-specific stub functions for MenuManager
 *
 * Provides minimal platform abstraction for menu rendering and interaction.
 * These functions bridge MenuManager to the host platform.
 */

#include "SystemTypes.h"
#include "MenuManager/MenuManager.h"
#include "MenuManager/menu_private.h"
#include "MemoryMgr/MemoryManager.h"
#include <stdlib.h>
#include <string.h>

/* Screen bits buffer structure for saving/restoring menu drawing */
typedef struct ScreenBits {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t* pixelData;
    size_t dataSize;
} ScreenBits;

/* Platform screen bit functions - implementation */
/*
 * Platform_SaveScreenBits
 * Captures screen area for later restoration before drawing menus
 */
void* Platform_SaveScreenBits_Impl(const Rect* rect)
{
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;

    if (!rect || !framebuffer) {
        return NULL;
    }

    ScreenBits* bits = (ScreenBits*)NewPtr(sizeof(ScreenBits));
    if (!bits) {
        return NULL;
    }

    /* Calculate region dimensions */
    uint32_t width = rect->right - rect->left;
    uint32_t height = rect->bottom - rect->top;

    if (width <= 0 || height <= 0 || width > fb_width || height > fb_height) {
        DisposePtr((Ptr)bits);
        return NULL;
    }

    bits->width = width;
    bits->height = height;
    bits->pitch = fb_pitch;
    bits->dataSize = height * fb_pitch;

    /* Allocate buffer for screen data */
    bits->pixelData = (uint8_t*)NewPtr(bits->dataSize);
    if (!bits->pixelData) {
        DisposePtr((Ptr)bits);
        return NULL;
    }

    /* Capture screen region */
    uint8_t* src = (uint8_t*)framebuffer + (rect->top * fb_pitch) + (rect->left * 4);
    uint8_t* dst = bits->pixelData;

    for (uint32_t y = 0; y < height; y++) {
        memcpy(dst, src, width * 4);
        src += fb_pitch;
        dst += fb_pitch;
    }

    return (void*)bits;
}

/*
 * Platform_RestoreScreenBits
 * Restores previously saved screen region
 */
void Platform_RestoreScreenBits(Handle bits, const Rect* rect)
{
    extern void* framebuffer;
    extern uint32_t fb_pitch;

    if (!bits || !rect || !framebuffer) {
        return;
    }

    ScreenBits* screenBits = (ScreenBits*)bits;

    if (!screenBits->pixelData) {
        return;
    }

    uint32_t width = rect->right - rect->left;
    uint32_t height = rect->bottom - rect->top;

    if (width != screenBits->width || height != screenBits->height) {
        return;  /* Size mismatch, don't restore */
    }

    /* Restore screen region */
    uint8_t* dst = (uint8_t*)framebuffer + (rect->top * fb_pitch) + (rect->left * 4);
    uint8_t* src = screenBits->pixelData;

    for (uint32_t y = 0; y < height; y++) {
        memcpy(dst, src, width * 4);
        src += fb_pitch;
        dst += fb_pitch;
    }
}

/*
 * Platform_DisposeScreenBits
 * Releases screen bits buffer
 */
void Platform_DisposeScreenBits(Handle bits)
{
    if (!bits) {
        return;
    }

    ScreenBits* screenBits = (ScreenBits*)bits;

    if (screenBits->pixelData) {
        DisposePtr((Ptr)screenBits->pixelData);
        screenBits->pixelData = NULL;
    }

    DisposePtr((Ptr)screenBits);
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
    extern void GetMouse(Point* mouseLoc);
    extern Boolean Button(void);

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
    extern UInt16 GetPS2Modifiers(void);
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

/*
 * Platform_HiliteMenuItem
 * Highlights or unhighlights a menu item visually
 */
void Platform_HiliteMenuItem(void* theMenu, short item, Boolean hilite)
{
    (void)theMenu;
    (void)item;
    (void)hilite;
    /* MenuItem highlighting delegated to MenuDisplay */
    /* hilite=true: draw item highlighted */
    /* hilite=false: draw item normal */
}

/*
 * Platform_SaveScreenBits (public wrapper)
 * Wrapper around internal implementation
 */
Handle Platform_SaveScreenBits(const Rect* rect)
{
    return (Handle)Platform_SaveScreenBits_Impl(rect);
}
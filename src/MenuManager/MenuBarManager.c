/* #include "SystemTypes.h" */
#include "MenuManager/menu_private.h"
#include <stdio.h>
/*
 * MenuBarManager.c - Menu Bar Management Implementation
 *
 * This file implements menu bar management and application switching.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"


/* Menu bar management */
void InitMenuBar(void) {
    extern void serial_puts(const char* str);
    serial_puts("Initializing menu bar\n");
    InitMenus();
}

void ShowMenuBar(void) {
    MenuManagerState* state = GetMenuManagerState();
    if (state) {
        state->menuBarVisible = true;
        DrawMenuBar();
    }
}

void HideMenuBar(void) {
    MenuManagerState* state = GetMenuManagerState();
    if (state) {
        state->menuBarVisible = false;
        EraseMenuBar(NULL);
    }
}

Boolean IsMenuBarVisible(void) {
    MenuManagerState* state = GetMenuManagerState();
    return state ? state->menuBarVisible : false;
}

/* Menu title utilities */
static OSErr GetMenuTitleRect_ByID(short menuID, Rect* theRect) {
    if (!theRect) return -1;

    /* Calculate menu title rectangle based on menu bar layout */
    theRect->left = 0;
    theRect->top = 0;
    theRect->right = 80;
    theRect->bottom = GetMBarHeight();

    return 0;
}

OSErr GetMBARRect(Rect* theRect) {
    if (!theRect) return -1;

    theRect->left = 0;
    theRect->top = 0;
    theRect->right = 640;  /* Default screen width */
    theRect->bottom = GetMBarHeight();

    return 0;
}

/* Private menu functions */
Boolean IsSystemMenu(short menuID) {
    return (menuID >= kLoSystemMenuRange && menuID <= kHiSystemMenuRange);
}

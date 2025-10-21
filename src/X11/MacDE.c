/**
 * Mac Classic Desktop Environment - MacDE
 *
 * Provides a classic Mac OS desktop environment with:
 * - Menu bar at top
 * - Finder (file manager)
 * - Application launcher
 * - Desktop icons
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "X11.h"

/* Menu bar structure */
typedef struct {
    char* name;
    char* items[10];
    int item_count;
} Menu;

/* Desktop environment state */
typedef struct {
    Menu menus[5];
    int menu_count;
} MacDE;

static MacDE de_state;

/**
 * Initialize desktop environment
 */
void MacDE_Initialize(void) {
    serial_puts("[MacDE] Initializing Mac Classic desktop environment...\n");

    /* Initialize DE state */
    memset(&de_state, 0, sizeof(de_state));

    /* Set up menu bar */
    MacDE_SetupMenuBar();

    serial_puts("[MacDE] Ready\n");
}

/**
 * Set up menu bar menus
 */
void MacDE_SetupMenuBar(void) {
    serial_puts("[MacDE] Setting up menu bar...\n");

    /* TODO: Create File menu */
    /* TODO: Create Edit menu */
    /* TODO: Create View menu */
    /* TODO: Create Special menu */
    /* TODO: Create Help menu */
}

/**
 * Draw menu bar
 */
void MacDE_DrawMenuBar(uint32_t* framebuffer, uint32_t pitch, uint32_t width) {
    if (!framebuffer) return;

    /* Draw menu bar background (dark gray) */
    for (uint32_t x = 0; x < width; x++) {
        framebuffer[x] = 0xC0C0C0;  /* Light gray */
    }

    serial_puts("[MacDE] Menu bar drawn\n");
}

/**
 * Open file browser
 */
void MacDE_OpenFinder(void) {
    serial_puts("[MacDE] Opening Finder...\n");

    extern MacWindow* MacWM_CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                         const char* title, bool has_close, bool has_zoom);

    MacWindow* finder = MacWM_CreateWindow(40, 40, 400, 300, "Finder", true, true);
    if (finder) {
        serial_puts("[MacDE] Finder opened\n");
    }
}

/**
 * Launch application
 */
void MacDE_LaunchApp(const char* app_name) {
    serial_puts("[MacDE] Launching %s...\n", app_name);

    if (strcmp(app_name, "Finder") == 0) {
        MacDE_OpenFinder();
    } else if (strcmp(app_name, "Terminal") == 0) {
        serial_puts("[MacDE] Launching Terminal\n");
        extern MacWindow* MacWM_CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                             const char* title, bool has_close, bool has_zoom);
        MacWM_CreateWindow(60, 100, 480, 300, "Terminal", true, true);
    }
}

/**
 * Draw desktop
 */
void MacDE_Redraw(void) {
    serial_puts("[MacDE] Redrawing desktop...\n");

    /* TODO: Draw desktop icons */
    /* TODO: Draw menu bar */
}

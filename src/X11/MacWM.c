/**
 * Mac Classic Window Manager - MacWM
 *
 * A window manager that provides Mac Classic (System 7/8) aesthetics:
 * - Platinum gray background
 * - Mac Classic window chrome (title bar, close/zoom buttons)
 * - Menu bar at top
 * - Classic window appearance
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "X11.h"

/* Mac Classic colors */
#define MAC_PLATINUM_GRAY   0xC0C0C0
#define MAC_DARK_GRAY       0x808080
#define MAC_WHITE           0xFFFFFF
#define MAC_BLACK           0x000000
#define MAC_MENU_BLUE       0x0000CC

#define MAX_WINDOWS 32
static MacWindow windows[MAX_WINDOWS];
static int window_count = 0;

/**
 * Initialize window manager
 */
void MacWM_Initialize(void) {
    serial_puts("[MacWM] Initializing Mac Classic window manager...\n");

    /* Initialize window array */
    window_count = 0;
    memset(windows, 0, sizeof(windows));

    serial_puts("[MacWM] Ready\n");
}

/**
 * Create a new window
 */
MacWindow* MacWM_CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                               const char* title, bool has_close, bool has_zoom) {
    if (window_count >= MAX_WINDOWS) {
        serial_puts("[MacWM] ERROR: Maximum windows reached\n");
        return NULL;
    }

    MacWindow* win = &windows[window_count++];
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->title = (char*)title;
    win->has_close_button = has_close;
    win->has_zoom_button = has_zoom;
    win->is_active = true;

    serial_puts("[MacWM] Created window\n");

    return win;
}

/**
 * Draw Mac Classic window frame
 *
 * Classic window has:
 * - Dark gray border (2px)
 * - Title bar with Mac blue gradient
 * - Close and zoom buttons (if enabled)
 * - Light gray client area
 */
void MacWM_DrawWindowFrame(MacWindow* win, uint32_t* framebuffer, uint32_t pitch) {
    if (!win || !framebuffer) return;

    uint32_t* fb = framebuffer;
    uint32_t x = win->x, y = win->y;
    uint32_t w = win->width, h = win->height;

    /* Draw border (dark gray, 2px) */
    for (uint32_t i = 0; i < w; i++) {
        fb[(y * pitch/4) + x + i] = MAC_DARK_GRAY;
        fb[((y+1) * pitch/4) + x + i] = MAC_DARK_GRAY;
        fb[((y + h - 1) * pitch/4) + x + i] = MAC_DARK_GRAY;
        fb[((y + h - 2) * pitch/4) + x + i] = MAC_DARK_GRAY;
    }

    for (uint32_t i = 0; i < h; i++) {
        fb[((y + i) * pitch/4) + x] = MAC_DARK_GRAY;
        fb[((y + i) * pitch/4) + x + 1] = MAC_DARK_GRAY;
        fb[((y + i) * pitch/4) + x + w - 1] = MAC_DARK_GRAY;
        fb[((y + i) * pitch/4) + x + w - 2] = MAC_DARK_GRAY;
    }

    /* Draw title bar (Mac blue) */
    uint32_t title_height = 20;
    for (uint32_t i = 0; i < title_height; i++) {
        for (uint32_t j = 2; j < w - 2; j++) {
            uint32_t color = win->is_active ? MAC_MENU_BLUE : 0xAAAAAA;
            fb[((y + 2 + i) * pitch/4) + x + j] = color;
        }
    }

    /* Draw client area (light gray) */
    for (uint32_t i = title_height + 2; i < h - 2; i++) {
        for (uint32_t j = 2; j < w - 2; j++) {
            fb[((y + i) * pitch/4) + x + j] = MAC_PLATINUM_GRAY;
        }
    }
}

/**
 * Draw menu bar at top of screen
 */
void MacWM_DrawMenuBar(uint32_t* framebuffer, uint32_t width, uint32_t height, uint32_t pitch) {
    if (!framebuffer) return;

    /* Menu bar is 20 pixels tall at top */
    uint32_t menu_height = 20;

    /* Draw menu bar background (light gray with slight 3D effect) */
    for (uint32_t y = 0; y < menu_height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t color;
            if (y == 0) {
                /* Top highlight */
                color = 0xFFFFFF;
            } else if (y == menu_height - 1) {
                /* Bottom shadow */
                color = 0x808080;
            } else {
                /* Main area */
                color = MAC_PLATINUM_GRAY;
            }
            framebuffer[(y * pitch/4) + x] = color;
        }
    }

    /* Draw Apple menu marker (simple rectangle for now) */
    uint32_t apple_x = 5;
    uint32_t apple_y = 2;
    for (uint32_t dy = 0; dy < 16 && apple_y + dy < menu_height; dy++) {
        for (uint32_t dx = 0; dx < 12 && apple_x + dx < width; dx++) {
            framebuffer[((apple_y + dy) * pitch/4) + apple_x + dx] = MAC_BLACK;
        }
    }
}

/**
 * Draw all windows
 */
void MacWM_DrawAll(void) {
    extern X11Display* X11_GetDisplay(void);
    X11Display* display = X11_GetDisplay();

    if (!display || !display->framebuffer) return;

    uint32_t pitch = display->width * 4;

    /* Draw menu bar first */
    MacWM_DrawMenuBar((uint32_t*)display->framebuffer, display->width, display->height, pitch);

    /* Draw all windows */
    for (int i = 0; i < window_count; i++) {
        MacWM_DrawWindowFrame(&windows[i], (uint32_t*)display->framebuffer, pitch);
    }
}

/**
 * Handle mouse click
 */
void MacWM_HandleClick(uint16_t x, uint16_t y) {
    /* Check if click is in menu bar area */
    if (y < 20) {
        serial_puts("[MacWM] Menu bar click\n");
        return;
    }

    /* Check if click hits any window */
    for (int i = 0; i < window_count; i++) {
        MacWindow* win = &windows[i];
        if (x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height) {

            /* Check if close button clicked */
            if (x < win->x + 20 && y < win->y + 20) {
                serial_puts("[MacWM] Window close clicked\n");
                return;
            }

            /* Check if title bar clicked (for dragging) */
            if (y < win->y + 22) {
                serial_puts("[MacWM] Window title bar clicked\n");
                /* Window was clicked, make it active */
                win->is_active = true;
                return;
            }

            /* Window content area clicked */
            serial_puts("[MacWM] Window content clicked\n");
            return;
        }
    }

    /* Desktop click */
    serial_puts("[MacWM] Desktop clicked\n");
}

/**
 * Redraw window manager
 */
void MacWM_Redraw(void) {
    /* Clear to Mac gray */
    extern void X11_ClearDisplay(uint32_t color);
    X11_ClearDisplay(MAC_PLATINUM_GRAY);

    /* Draw all windows */
    MacWM_DrawAll();
}

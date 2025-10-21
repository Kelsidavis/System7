/**
 * X11 Core - Minimal X11 Server Wrapper
 *
 * Provides a simplified X11 interface for Mac Classic-looking
 * window manager and desktop environment
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* X11 Display structure - static to avoid memory allocation before MM is ready */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    void* framebuffer;
} X11Display;

static X11Display x11_display_static = {640, 480, 32, NULL};
static X11Display* x11_display = &x11_display_static;

/**
 * Initialize X11 display
 */
bool X11_InitDisplay(void) {
    serial_puts("[X11] Initializing X11 display...\n");

    /* Use existing framebuffer set up by bootloader */
    extern void* framebuffer;

    if (!framebuffer) {
        serial_puts("[X11] ERROR: No framebuffer available\n");
        return false;
    }

    /* Set up display with framebuffer */
    x11_display->framebuffer = framebuffer;
    x11_display->width = 640;
    x11_display->height = 480;
    x11_display->depth = 32;

    serial_puts("[X11] Display initialized\n");

    return true;
}

/**
 * Get X11 display
 */
X11Display* X11_GetDisplay(void) {
    return x11_display;
}

/**
 * Clear display to background color
 */
void X11_ClearDisplay(uint32_t color) {
    if (!x11_display || !x11_display->framebuffer) return;

    uint32_t* fb = (uint32_t*)x11_display->framebuffer;
    size_t pixels = x11_display->width * x11_display->height;

    for (size_t i = 0; i < pixels; i++) {
        fb[i] = color;
    }
}

/**
 * Main X11 initialization function
 */
void X11_Initialize(void) {
    serial_puts("[X11] Starting X11 environment...\n");

    /* Initialize display */
    if (!X11_InitDisplay()) {
        serial_puts("[X11] ERROR: Failed to initialize display\n");
        return;
    }

    /* Clear display to Mac Classic platinum gray */
    #define MAC_GRAY 0xC0C0C0
    X11_ClearDisplay(MAC_GRAY);

    serial_puts("[X11] Display cleared\n");

    /* Initialize window manager */
    extern void MacWM_Initialize(void);
    MacWM_Initialize();

    serial_puts("[X11] Window manager initialized\n");

    /* Initialize desktop environment */
    extern void MacDE_Initialize(void);
    MacDE_Initialize();

    serial_puts("[X11] Desktop environment initialized\n");

    /* Start event loop */
    serial_puts("[X11] Starting event loop...\n");
    extern void X11_EventLoop(void);
    X11_EventLoop();
}

/**
 * X11 Event Loop
 */
void X11_EventLoop(void) {
    serial_puts("[X11] Event loop started\n");

    while (1) {
        /* TODO: Process X11 events */
        /* TODO: Handle mouse/keyboard input */
        /* TODO: Update display */

        /* Temporary: prevent busy loop with volatile loop */
        volatile int i;
        for (i = 0; i < 1000; i++) {
            __asm__("nop");
        }
    }
}

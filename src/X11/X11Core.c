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

/* X11 Display structure */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    void* framebuffer;
} X11Display;

static X11Display* x11_display = NULL;

/**
 * Initialize X11 display
 */
bool X11_InitDisplay(void) {
    printf("[X11] Initializing X11 display...\n");

    /* Allocate display structure */
    extern void* NewPtr(unsigned long size);
    x11_display = (X11Display*)NewPtr(sizeof(X11Display));
    if (!x11_display) {
        printf("[X11] ERROR: Failed to allocate display\n");
        return false;
    }

    /* Set display parameters (standard VGA 640x480) */
    x11_display->width = 640;
    x11_display->height = 480;
    x11_display->depth = 32;

    /* Use existing framebuffer */
    extern void* framebuffer;
    x11_display->framebuffer = framebuffer;

    printf("[X11] Display initialized: %dx%d@%d-bit\n",
           x11_display->width, x11_display->height, x11_display->depth);

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
    printf("[X11] Starting X11 environment...\n");

    /* Initialize display */
    if (!X11_InitDisplay()) {
        printf("[X11] ERROR: Failed to initialize display\n");
        return;
    }

    /* Clear display to Mac Classic platinum gray */
    #define MAC_GRAY 0xC0C0C0
    X11_ClearDisplay(MAC_GRAY);

    printf("[X11] Display cleared\n");

    /* Initialize window manager */
    extern void MacWM_Initialize(void);
    MacWM_Initialize();

    printf("[X11] Window manager initialized\n");

    /* Initialize desktop environment */
    extern void MacDE_Initialize(void);
    MacDE_Initialize();

    printf("[X11] Desktop environment initialized\n");

    /* Start event loop */
    printf("[X11] Starting event loop...\n");
    extern void X11_EventLoop(void);
    X11_EventLoop();
}

/**
 * X11 Event Loop
 */
void X11_EventLoop(void) {
    printf("[X11] Event loop started\n");

    while (1) {
        /* TODO: Process X11 events */
        /* TODO: Handle mouse/keyboard input */
        /* TODO: Update display */

        /* Temporary: prevent busy loop */
        extern void platform_sleep(uint32_t ms);
        platform_sleep(10);
    }
}

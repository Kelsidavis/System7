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

/* Input event types */
typedef enum {
    EVENT_MOUSE_MOVE = 1,
    EVENT_MOUSE_DOWN = 2,
    EVENT_MOUSE_UP = 3,
    EVENT_KEY_DOWN = 4,
    EVENT_KEY_UP = 5,
} EventType;

/* Mouse event */
typedef struct {
    EventType type;
    uint16_t x;
    uint16_t y;
    uint8_t button;
} MouseEvent;

/* Keyboard event */
typedef struct {
    EventType type;
    uint8_t keycode;
} KeyEvent;

static X11Display x11_display_static = {640, 480, 32, NULL};
static X11Display* x11_display = &x11_display_static;

/* Input state tracking */
static uint16_t last_mouse_x = 0;
static uint16_t last_mouse_y = 0;
static uint8_t mouse_button_state = 0;

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

    /* Draw initial UI (menu bar) */
    extern void MacWM_DrawAll(void);
    MacWM_DrawAll();

    serial_puts("[X11] Initial UI drawn\n");

    /* Start event loop */
    serial_puts("[X11] Starting event loop...\n");
    extern void X11_EventLoop(void);
    X11_EventLoop();
}

/**
 * Poll for mouse input from system
 */
static bool X11_PollMouseInput(MouseEvent* event) {
    /* Platform-specific mouse input would be polled here */
    /* For now, return false as input system is not fully implemented */
    (void)event;
    return false;
}

/**
 * Handle input event
 */
static void X11_HandleEvent(MouseEvent* event) {
    if (!event) return;

    switch (event->type) {
        case EVENT_MOUSE_MOVE:
            /* Update mouse cursor position tracking */
            last_mouse_x = event->x;
            last_mouse_y = event->y;
            break;

        case EVENT_MOUSE_DOWN:
            serial_puts("[X11] Mouse button down\n");
            /* Dispatch to window manager for click handling */
            extern void MacWM_HandleClick(uint16_t x, uint16_t y);
            MacWM_HandleClick(event->x, event->y);
            break;

        case EVENT_MOUSE_UP:
            serial_puts("[X11] Mouse button up\n");
            break;

        default:
            break;
    }
}

/**
 * X11 Event Loop
 */
void X11_EventLoop(void) {
    serial_puts("[X11] Event loop started\n");
    MouseEvent event;

    while (1) {
        /* Poll for mouse input */
        if (X11_PollMouseInput(&event)) {
            X11_HandleEvent(&event);
        }

        /* Temporary: prevent busy loop with volatile loop */
        volatile int i;
        for (i = 0; i < 500; i++) {
            __asm__("nop");
        }
    }
}

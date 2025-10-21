/**
 * X11 - Minimal X11 Implementation Header
 */

#ifndef X11_H
#define X11_H

#include <stdint.h>
#include <stdbool.h>

/* Display structure */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    void* framebuffer;
} X11Display;

/* X11 Core functions */
void X11_Initialize(void);
void X11_EventLoop(void);
bool X11_InitDisplay(void);
X11Display* X11_GetDisplay(void);
void X11_ClearDisplay(uint32_t color);

/* Window Manager functions */
void MacWM_Initialize(void);
typedef struct {
    uint32_t x, y;
    uint32_t width, height;
    char* title;
    bool has_close_button;
    bool has_zoom_button;
    bool is_active;
    void* content;
} MacWindow;
MacWindow* MacWM_CreateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                               const char* title, bool has_close, bool has_zoom);
void MacWM_DrawAll(void);
void MacWM_Redraw(void);

/* Desktop Environment functions */
void MacDE_Initialize(void);
void MacDE_SetupMenuBar(void);
void MacDE_DrawMenuBar(uint32_t* framebuffer, uint32_t pitch, uint32_t width);
void MacDE_OpenFinder(void);
void MacDE_LaunchApp(const char* app_name);
void MacDE_Redraw(void);

#endif

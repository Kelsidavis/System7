/* Demo Desktop Rendering - Reference Implementation */
/* This file contains the hardcoded desktop rendering that was used for testing */
/* The real desktop should be rendered by Finder */

#include <stdint.h>
#include <stdbool.h>

/* Framebuffer globals from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

/* External functions */
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);
extern void draw_text_string(uint32_t x, uint32_t y, const char* str, uint32_t color);
extern void serial_puts(const char* str);
extern void console_puts(const char* str);

/* Draw a filled rectangle */
static void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!framebuffer) return;

    for (uint32_t dy = 0; dy < height; dy++) {
        for (uint32_t dx = 0; dx < width; dx++) {
            if (x + dx < fb_width && y + dy < fb_height) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                *pixel = color;
            }
        }
    }
}

/* Draw a window with title bar */
static void draw_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* title, bool active) {
    /* Window background - white */
    uint32_t bg_color = pack_color(255, 255, 255);
    fill_rect(x, y, width, height, bg_color);

    /* Title bar */
    uint32_t title_color = active ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
    uint32_t title_bg = active ? pack_color(255, 255, 255) : pack_color(128, 128, 128);

    /* Draw title bar background with stripes if active */
    for (uint32_t dy = 0; dy < 20; dy++) {
        uint32_t stripe_color = title_bg;
        if (active && (dy % 2 == 0)) {
            stripe_color = pack_color(240, 240, 240);
        }
        for (uint32_t dx = 0; dx < width; dx++) {
            if (x + dx < fb_width && y + dy < fb_height) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                *pixel = stripe_color;
            }
        }
    }

    /* Close box */
    uint32_t close_box_color = pack_color(0, 0, 0);
    for (uint32_t dy = 4; dy < 16; dy++) {
        for (uint32_t dx = 4; dx < 16; dx++) {
            if ((dx == 4 || dx == 15 || dy == 4 || dy == 15)) {
                if (x + dx < fb_width && y + dy < fb_height) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                    *pixel = close_box_color;
                }
            }
        }
    }

    /* Title text - now properly positioned */
    draw_text_string(x + (width/2) - (6*5/2), y + 7, title, title_color);

    /* Window border */
    uint32_t border_color = pack_color(0, 0, 0);
    for (uint32_t dx = 0; dx < width; dx++) {
        /* Top border */
        if (y < fb_height && x + dx < fb_width) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + dx) * 4);
            *pixel = border_color;
        }
        /* Bottom border */
        if (y + height - 1 < fb_height && x + dx < fb_width) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + height - 1) * fb_pitch + (x + dx) * 4);
            *pixel = border_color;
        }
    }
    for (uint32_t dy = 0; dy < height; dy++) {
        /* Left border */
        if (y + dy < fb_height && x < fb_width) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + x * 4);
            *pixel = border_color;
        }
        /* Right border */
        if (y + dy < fb_height && x + width - 1 < fb_width) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + width - 1) * 4);
            *pixel = border_color;
        }
    }
}

/* Draw rainbow Apple logo */
static void draw_apple_logo(uint32_t x, uint32_t y) {
    /* Rainbow colors for classic Apple logo */
    uint32_t green = pack_color(97, 189, 79);
    uint32_t yellow = pack_color(254, 223, 0);
    uint32_t orange = pack_color(253, 150, 32);
    uint32_t red = pack_color(229, 52, 42);
    uint32_t purple = pack_color(146, 45, 137);
    uint32_t blue = pack_color(48, 164, 237);

    /* Simplified 14x17 Apple logo with rainbow stripes */
    /* Row 0-2: Leaf/stem area */
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 7; dx < 10; dx++) {
            if (y + dy < fb_height && x + dx < fb_width) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                *p = green;
            }
        }
    }

    /* Body stripes */
    uint32_t colors[] = {green, yellow, orange, red, purple, blue};

    for (int stripe = 0; stripe < 6; stripe++) {
        int start_y = 3 + stripe * 2;
        int end_y = start_y + 3;

        for (int dy = start_y; dy < end_y && dy < 17; dy++) {
            int width = 12 - abs(dy - 10) / 2;
            int start_x = 7 - width / 2;

            for (int dx = 0; dx < width; dx++) {
                if (y + dy < fb_height && x + start_x + dx < fb_width) {
                    /* Skip bite area */
                    if (!(dx > width - 4 && dy < 7)) {
                        uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + start_x + dx) * 4);
                        *p = colors[stripe % 6];
                    }
                }
            }
        }
    }
}

/* Draw classic Mac trash can */
static void draw_trash_icon(uint32_t x, uint32_t y) {
    uint32_t black = pack_color(0, 0, 0);
    uint32_t white = pack_color(255, 255, 255);
    uint32_t gray = pack_color(192, 192, 192);

    /* Trash can size: 32x32 */
    /* Fill white background */
    for (int dy = 0; dy < 32; dy++) {
        for (int dx = 0; dx < 32; dx++) {
            if (x + dx < fb_width && y + dy < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                *p = white;
            }
        }
    }

    /* Draw lid (top part) */
    for (int dx = 8; dx < 24; dx++) {
        for (int dy = 4; dy < 8; dy++) {
            if (x + dx < fb_width && y + dy < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                *p = gray;
            }
        }
    }

    /* Draw handle */
    for (int dx = 14; dx < 18; dx++) {
        for (int dy = 2; dy < 4; dy++) {
            if (x + dx < fb_width && y + dy < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + dx) * 4);
                *p = black;
            }
        }
    }

    /* Draw body */
    for (int dy = 8; dy < 28; dy++) {
        int width = 16 + (dy - 8) / 4;  /* Slightly tapered */
        int start_x = 16 - width / 2;

        for (int dx = 0; dx < width; dx++) {
            /* Draw outline */
            if (dx == 0 || dx == width - 1 || dy == 8 || dy == 27) {
                if (x + start_x + dx < fb_width && y + dy < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + start_x + dx) * 4);
                    *p = black;
                }
            }
            /* Fill interior */
            else {
                if (x + start_x + dx < fb_width && y + dy < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + start_x + dx) * 4);
                    *p = gray;
                }
            }
        }
    }

    /* Draw vertical lines on can */
    for (int line_x = 11; line_x < 22; line_x += 3) {
        for (int dy = 12; dy < 24; dy++) {
            if (x + line_x < fb_width && y + dy < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + (x + line_x) * 4);
                *p = black;
            }
        }
    }
}

/* Draw demo System 7.1 desktop */
void draw_demo_desktop(void) {
    if (!framebuffer) {
        console_puts("ERROR: No framebuffer!\n");
        serial_puts("ERROR: No framebuffer!\n");
        return;
    }

    serial_puts("Drawing System 7.1 desktop...\n");

    /* Classic Mac desktop pattern - 8x8 teal/blue checkerboard */
    uint32_t teal = pack_color(0, 128, 128);
    uint32_t blue = pack_color(0, 100, 150);

    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            uint32_t color = ((x/8 + y/8) % 2) ? blue : teal;
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
            *pixel = color;
        }
    }

    /* Menu bar background - white */
    uint32_t menu_bg = pack_color(255, 255, 255);
    fill_rect(0, 0, fb_width, 20, menu_bg);

    /* Menu bar bottom border */
    uint32_t black = pack_color(0, 0, 0);
    for (uint32_t x = 0; x < fb_width; x++) {
        if (20 < fb_height) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + 20 * fb_pitch + x * 4);
            *pixel = black;
        }
    }

    /* Draw Apple logo in menu bar */
    draw_apple_logo(8, 2);

    /* Menu items */
    draw_text_string(30, 7, "File", black);
    draw_text_string(70, 7, "Edit", black);
    draw_text_string(110, 7, "View", black);
    draw_text_string(150, 7, "Special", black);

    /* Clock on right side - properly positioned */
    draw_text_string(fb_width - 50, 7, "3:47 PM", black);

    /* Desktop icons */
    /* Trash at bottom right */
    draw_trash_icon(fb_width - 50, fb_height - 50);
    draw_text_string(fb_width - 50, fb_height - 15, "Trash", black);

    /* Example windows */
    draw_window(50, 60, 300, 200, "System Folder", false);
    draw_window(100, 100, 350, 250, "Macintosh HD", true);

    serial_puts("Desktop drawn\n");
}
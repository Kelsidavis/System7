/*
 * Mac OS System 7.1 Portable - Complete GUI with Text and Icons
 * 640x480 VESA Graphics with bitmap font and icon rendering
 */

#include <stdint.h>
#include <stddef.h>

/* Multiboot header info */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Screen constants */
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

/* Colors for 32-bit mode */
#define COLOR_BLACK     0x00000000
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_GRAY      0x00C0C0C0
#define COLOR_DARKGRAY  0x00808080
#define COLOR_LIGHTGRAY 0x00F0F0F0
#define COLOR_TEAL      0x0000CCCC
#define COLOR_LIGHTTEAL 0x0000E6E6
#define COLOR_BLUE      0x000000FF
#define COLOR_RED       0x00FF0000

/* Simple 8x8 bitmap font (subset of ASCII) */
static const uint8_t font8x8[128][8] = {
    {0,0,0,0,0,0,0,0}, // 0 - null
    {0,0,0,0,0,0,0,0}, // 1
    /* ... zeros for control chars ... */
    [32] = {0,0,0,0,0,0,0,0}, // space
    [33] = {0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x00}, // !
    [65] = {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00}, // A
    [66] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    [67] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    [68] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    [69] = {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // E
    [70] = {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // F
    [71] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    [72] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    [73] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    [74] = {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // J
    [75] = {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    [76] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    [77] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    [78] = {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    [79] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    [80] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    [81] = {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // Q
    [82] = {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // R
    [83] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    [84] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    [85] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    [86] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    [87] = {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    [88] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    [89] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    [90] = {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
    [97] = {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // a
    [98] = {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // b
    [99] = {0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00}, // c
    [100] = {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // d
    [101] = {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // e
    [102] = {0x0E,0x18,0x18,0x3E,0x18,0x18,0x18,0x00}, // f
    [103] = {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C}, // g
    [104] = {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // h
    [105] = {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
    [106] = {0x06,0x00,0x0E,0x06,0x06,0x06,0x66,0x3C}, // j
    [107] = {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // k
    [108] = {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
    [109] = {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // m
    [110] = {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // n
    [111] = {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // o
    [112] = {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // p
    [113] = {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // q
    [114] = {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // r
    [115] = {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // s
    [116] = {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // t
    [117] = {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // u
    [118] = {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // v
    [119] = {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    [120] = {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // x
    [121] = {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x7C}, // y
    [122] = {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // z
    [32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    [46] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    [48] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 0
    [49] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    [50] = {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00}, // 2
    [51] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    [52] = {0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0x00}, // 4
    [53] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    [54] = {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    [55] = {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, // 7
    [56] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    [57] = {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}, // 9
};

/* Simple icon bitmaps (16x16) */
static const uint8_t folder_icon[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,2,2,2,2,2,2,2,2,2,2,2,2,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,2,2,2,2,2,2,2,2,2,2,2,2,1,1},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

static const uint8_t trash_icon[16][16] = {
    {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,1,2,1,2,1,2,1,2,1,2,1,2,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

/* Apple logo (simplified) */
static const uint8_t apple_icon[12][10] = {
    {0,0,0,1,1,1,0,0,0,0},
    {0,0,1,1,1,1,0,0,0,0},
    {0,0,0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,1,1,0,0},
    {1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,0,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0}
};

/* Multiboot info structure */
typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    union {
        struct {
            uint32_t framebuffer_palette_addr;
            uint16_t framebuffer_palette_num_colors;
        };
        struct {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
} multiboot_info_t;

/* Global framebuffer info */
static uint8_t* framebuffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;

/* I/O functions */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Serial debugging */
static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
}

static void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_puts(const char* str) {
    while (*str) serial_putc(*str++);
}

/* Draw pixel */
static void set_pixel(int x, int y, uint32_t color) {
    if (!framebuffer || x < 0 || x >= fb_width || y < 0 || y >= fb_height) {
        return;
    }

    uint32_t pixel_offset = y * fb_pitch + x * (fb_bpp / 8);

    if (fb_bpp == 32) {
        *((uint32_t*)(framebuffer + pixel_offset)) = color;
    } else if (fb_bpp == 24) {
        framebuffer[pixel_offset] = color & 0xFF;
        framebuffer[pixel_offset + 1] = (color >> 8) & 0xFF;
        framebuffer[pixel_offset + 2] = (color >> 16) & 0xFF;
    }
}

/* Fill rectangle */
static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h && j < fb_height; j++) {
        for (int i = x; i < x + w && i < fb_width; i++) {
            set_pixel(i, j, color);
        }
    }
}

/* Draw character */
static void draw_char(int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return;

    const uint8_t* bitmap = font8x8[(int)c];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (1 << (7 - col))) {
                set_pixel(x + col, y + row, color);
            }
        }
    }
}

/* Draw string */
static void draw_string(int x, int y, const char* str, uint32_t color) {
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            y += 10;
            cur_x = x;
        } else {
            draw_char(cur_x, y, *str, color);
            cur_x += 8;
        }
        str++;
    }
}

/* Draw icon from bitmap */
static void draw_icon(int x, int y, const uint8_t icon[16][16], uint32_t color1, uint32_t color2) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            if (icon[row][col] == 1) {
                set_pixel(x + col, y + row, color1);
            } else if (icon[row][col] == 2) {
                set_pixel(x + col, y + row, color2);
            }
        }
    }
}

/* Draw apple logo */
static void draw_apple_logo(int x, int y) {
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 10; col++) {
            if (apple_icon[row][col] == 1) {
                set_pixel(x + col, y + row, COLOR_BLACK);
            }
        }
    }
}

/* Draw window with title */
static void draw_window(int x, int y, int w, int h, const char* title) {
    /* Shadow */
    fill_rect(x + 2, y + 2, w, h, COLOR_DARKGRAY);

    /* Window body */
    fill_rect(x, y, w, h, COLOR_WHITE);

    /* Title bar */
    fill_rect(x, y, w, 20, COLOR_GRAY);

    /* Title bar stripes */
    for (int i = 2; i < 18; i += 2) {
        for (int j = 1; j < w - 1; j += 4) {
            set_pixel(x + j, y + i, COLOR_LIGHTGRAY);
            set_pixel(x + j + 1, y + i, COLOR_LIGHTGRAY);
        }
    }

    /* Window border */
    fill_rect(x, y, w, 1, COLOR_BLACK);
    fill_rect(x, y + h - 1, w, 1, COLOR_BLACK);
    fill_rect(x, y, 1, h, COLOR_BLACK);
    fill_rect(x + w - 1, y, 1, h, COLOR_BLACK);

    /* Close box */
    fill_rect(x + 8, y + 6, 8, 8, COLOR_WHITE);
    fill_rect(x + 7, y + 5, 10, 10, COLOR_BLACK);
    fill_rect(x + 8, y + 6, 8, 8, COLOR_LIGHTGRAY);

    /* Title text */
    draw_string(x + 24, y + 6, title, COLOR_BLACK);
}

/* Draw desktop with all elements */
static void draw_desktop(void) {
    /* Desktop background pattern */
    for (int y = 0; y < fb_height; y++) {
        for (int x = 0; x < fb_width; x++) {
            if (((x / 4) + (y / 4)) % 2 == 0) {
                set_pixel(x, y, COLOR_TEAL);
            } else {
                set_pixel(x, y, COLOR_LIGHTTEAL);
            }
        }
    }

    /* Menu bar */
    fill_rect(0, 0, fb_width, 20, COLOR_LIGHTGRAY);
    fill_rect(0, 20, fb_width, 1, COLOR_BLACK);

    /* Apple logo in menu */
    draw_apple_logo(10, 4);

    /* Menu items */
    draw_string(30, 6, "File", COLOR_BLACK);
    draw_string(70, 6, "Edit", COLOR_BLACK);
    draw_string(110, 6, "View", COLOR_BLACK);
    draw_string(150, 6, "Special", COLOR_BLACK);

    /* Main window - Finder */
    draw_window(50, 50, 400, 300, "System 7.1");

    /* Draw folder icons inside window */
    draw_icon(70, 90, folder_icon, COLOR_DARKGRAY, COLOR_LIGHTGRAY);
    draw_string(68, 108, "System", COLOR_BLACK);

    draw_icon(150, 90, folder_icon, COLOR_DARKGRAY, COLOR_LIGHTGRAY);
    draw_string(144, 108, "Applications", COLOR_BLACK);

    draw_icon(250, 90, folder_icon, COLOR_DARKGRAY, COLOR_LIGHTGRAY);
    draw_string(246, 108, "Documents", COLOR_BLACK);

    /* About window */
    draw_window(200, 200, 300, 150, "About This Macintosh");
    draw_string(220, 240, "System Software 7.1", COLOR_BLACK);
    draw_string(220, 256, "Total Memory: 256 MB", COLOR_BLACK);
    draw_string(220, 272, "Built-in Memory: 256 MB", COLOR_BLACK);
    draw_string(220, 288, "Largest Unused Block: 200 MB", COLOR_BLACK);

    /* Desktop icons */
    /* Macintosh HD */
    draw_icon(fb_width - 80, 40, folder_icon, COLOR_DARKGRAY, COLOR_LIGHTGRAY);
    draw_string(fb_width - 92, 58, "Macintosh HD", COLOR_BLACK);

    /* Trash */
    draw_icon(fb_width - 60, fb_height - 70, trash_icon, COLOR_DARKGRAY, COLOR_LIGHTGRAY);
    draw_string(fb_width - 56, fb_height - 50, "Trash", COLOR_BLACK);
}

/* Main entry point */
void macos_gui_complete_main(uint32_t magic, multiboot_info_t* mbi) {
    /* Initialize serial */
    serial_init();
    serial_puts("Mac OS System 7.1 Complete GUI Boot\n");

    /* Check multiboot magic */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_puts("Error: Invalid multiboot magic!\n");
        while (1) { __asm__ volatile ("hlt"); }
    }

    /* Check for framebuffer */
    if (mbi->flags & 0x00001000) {
        uint32_t fb_addr_low = (uint32_t)(mbi->framebuffer_addr & 0xFFFFFFFF);
        framebuffer = (uint8_t*)(uintptr_t)fb_addr_low;
        fb_width = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_pitch = mbi->framebuffer_pitch;
        fb_bpp = mbi->framebuffer_bpp;

        serial_puts("Framebuffer initialized:\n");
        serial_puts("  640x480 display ready\n");

        /* Draw complete desktop */
        serial_puts("Drawing Mac OS 7.1 desktop with icons and text...\n");
        draw_desktop();

        serial_puts("Mac OS 7.1 Complete GUI loaded successfully!\n");
    } else {
        serial_puts("Error: No framebuffer available!\n");
    }

    /* Halt */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
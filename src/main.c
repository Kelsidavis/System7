/* System 7.1 Kernel Main Entry Point */
#include "multiboot.h"
/* No external font header needed - we'll define inline */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Debug flag for serial menu commands - set to 1 to enable, 0 to disable */
#define DEBUG_SERIAL_MENU_COMMANDS 0

/* Include actual System 7.1 headers */
#include "../include/MacTypes.h"
#include "../include/QuickDraw/QuickDraw.h"
#include "../include/ResourceManager.h"
#include "../include/EventManager/EventTypes.h"  /* Include EventTypes first to define activeFlag */
#include "../include/EventManager/EventManager.h"
#include "../include/MenuManager/MenuManager.h"

/* Menu command dispatcher */
extern void DoMenuCommand(short menuID, short item);
#include "../include/DialogManager/DialogManager.h"
#include "../include/ControlManager/ControlManager.h"
#include "../include/ListManager/ListManager.h"
#include "../include/WindowManager/WindowManager.h"
#include "../include/TextEdit/TextEdit.h"
#include "../include/FontManager/FontManager.h"
#include "../include/PS2Controller.h"
#include "../include/FS/vfs.h"
#include "../include/MemoryMgr/MemoryManager.h"
#include "../include/Resources/system7_resources.h"

/* Simple 5x7 font for basic ASCII characters */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x00, 0x08, 0x14, 0x22, 0x41}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x41, 0x22, 0x14, 0x08, 0x00}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // F
    {0x3E, 0x41, 0x41, 0x51, 0x32}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x00, 0x7F, 0x41, 0x41}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x41, 0x41, 0x7F, 0x00, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x08, 0x14, 0x54, 0x54, 0x3C}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x00, 0x7F, 0x10, 0x28, 0x44}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}  // z
};

/* Arrow cursor is already defined in Resources/system7_resources.h */

/* Multiboot2 header structures */


/* Serial port for debugging */
#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Serial functions moved to System71StdLib.c */
extern void serial_init(void);
extern void serial_putchar(char c);
extern void serial_puts(const char* str);
extern int serial_data_ready(void);
extern char serial_getchar(void);
extern void serial_print_hex(uint32_t value);

/* Commented out - moved to System71StdLib.c
void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}
*/

/* serial_putchar moved to System71StdLib.c
void serial_putchar(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
}
*/

/* serial_puts moved to System71StdLib.c
void serial_puts(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}
*/

/* serial_data_ready moved to System71StdLib.c
int serial_data_ready(void) {
    return (inb(COM1 + 5) & 0x01) != 0;
}
*/

/* serial_getchar moved to System71StdLib.c
char serial_getchar(void) {
    while (!serial_data_ready());
    return inb(COM1);
}
*/

/* Process serial commands for menu testing */
void process_serial_command(void) {
#if DEBUG_SERIAL_MENU_COMMANDS
    if (!serial_data_ready()) return;

    char cmd = serial_getchar();

    switch (cmd) {
        case 'm':  /* Activate menu with mouse click */
        case 'M':
            {
                serial_puts("\nSimulating menu click...\n");

                /* Simulate a click on the File menu (at x=50, y=10) */
                Point pt = {50, 10};
                long menuChoice = MenuSelect(pt);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    serial_printf("Menu selection: menu %d, item %d\n", menuID, item);
                    DoMenuCommand(menuID, item);
                }
                DrawMenuBar();
            }
            break;

        case 'a':  /* Apple menu */
        case 'A':
            {
                serial_puts("\nSimulating Apple menu click...\n");
                Point pt = {20, 10};
                long menuChoice = MenuSelect(pt);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    serial_printf("Menu selection: menu %d, item %d\n", menuID, item);
                    DoMenuCommand(menuID, item);
                }
                DrawMenuBar();
            }
            break;

        case 'f':  /* File menu */
        case 'F':
            {
                serial_puts("\nSimulating File menu click...\n");
                Point pt = {50, 10};
                long menuChoice = MenuSelect(pt);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    serial_printf("Menu selection: menu %d, item %d\n", menuID, item);
                    DoMenuCommand(menuID, item);
                }
                DrawMenuBar();
            }
            break;

        case 'k':  /* Test MenuKey with various keys */
        case 'K':
            {
                serial_puts("\nTesting MenuKey - enter command key: ");
                char key = serial_getchar();
                serial_putchar(key);
                serial_puts("\n");

                long menuChoice = MenuKey(key);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    serial_printf("MenuKey found: menu %d, item %d for key '%c'\n", menuID, item, key);
                    DoMenuCommand(menuID, item);
                } else {
                    serial_printf("No menu command for key '%c'\n", key);
                }
            }
            break;

        case 'h':  /* Help */
        case 'H':
        case '?':
            serial_puts("\n=== Serial Menu Test Commands ===\n");
            serial_puts("m/M - Simulate click on File menu\n");
            serial_puts("a/A - Simulate click on Apple menu\n");
            serial_puts("f/F - Simulate click on File menu\n");
            serial_puts("k/K - Test MenuKey (prompts for key)\n");
            serial_puts("h/H/? - Show this help\n");
            serial_puts("================================\n\n");
            break;

        case '\r':
        case '\n':
            /* Ignore newlines */
            break;

        default:
            serial_printf("Unknown command '%c' (0x%02x). Press 'h' for help.\n", cmd, cmd);
            break;
    }
#endif /* DEBUG_SERIAL_MENU_COMMANDS */
}

/* VGA text mode for early output */
static uint16_t* vga_buffer = (uint16_t*)0xB8000;
static size_t vga_row = 0;
static size_t vga_col = 0;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const uint8_t VGA_COLOR = 0x0F; /* White on black */

/* Framebuffer for graphics mode */
/* Framebuffer globals - accessible by Finder and other subsystems */
void* framebuffer = NULL;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;
uint8_t fb_red_pos = 0;
uint8_t fb_red_size = 0;
uint8_t fb_green_pos = 0;
uint8_t fb_green_size = 0;
uint8_t fb_blue_pos = 0;
uint8_t fb_blue_size = 0;

/* Window management */
static int window_count = 0;

/* QuickDraw globals structure */
QDGlobals qd;

/* Pack RGB color according to framebuffer format */
uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = 0;

    /* If we have no color info, assume standard RGB */
    if (fb_red_size == 0 && fb_green_size == 0 && fb_blue_size == 0) {
        /* Default to standard 0x00RRGGBB format */
        return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    }

    /* Shift color components to their positions */
    color |= ((uint32_t)(r >> (8 - fb_red_size)) << fb_red_pos);
    color |= ((uint32_t)(g >> (8 - fb_green_size)) << fb_green_pos);
    color |= ((uint32_t)(b >> (8 - fb_blue_size)) << fb_blue_pos);

    return color;
}

/* Icon types */
#define ICON_TRASH 1

/* Forward declarations */
void draw_text_string(uint32_t x, uint32_t y, const char* text, uint32_t color);
void draw_apple_logo(uint32_t x, uint32_t y, uint32_t color);
void draw_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* title);
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_icon(uint32_t x, uint32_t y, int icon_type);

/* Early console output */
void console_putchar(char c) {
    /* Disable console output when in graphics mode to prevent corruption */
    if (framebuffer != NULL) {
        return;
    }

    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_row = 0;
        }
        return;
    }

    size_t index = vga_row * VGA_WIDTH + vga_col;
    vga_buffer[index] = (uint16_t)c | ((uint16_t)VGA_COLOR << 8);

    vga_col++;
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_row = 0;
        }
    }
}

void console_puts(const char* str) {
    while (*str) {
        console_putchar(*str++);
    }
}

void console_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = ' ' | ((uint16_t)VGA_COLOR << 8);
        }
    }
    vga_row = 0;
    vga_col = 0;
}

/* Helper to print hex values */
void print_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    console_puts("0x");
    for (int i = 7; i >= 0; i--) {
        console_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

/* serial_print_hex moved to System71StdLib.c
void serial_print_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 7; i >= 0; i--) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}
*/

/* Parse Multiboot2 info */
void parse_multiboot2(uint32_t magic, uint32_t* mb2_info) {
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        console_puts("Error: Invalid Multiboot2 magic! Got: ");
        print_hex(magic);
        console_puts("\n");
        serial_puts("Error: Invalid Multiboot2 magic! Got: ");
        serial_print_hex(magic);
        serial_puts("\n");
        return;
    }

    console_puts("Multiboot2 detected\n");
    serial_puts("Multiboot2 detected\n");

    /* Get total size */
    uint32_t total_size = *mb2_info;
    console_puts("Multiboot2 info size: ");
    print_hex(total_size);
    console_puts("\n");
    serial_puts("Multiboot2 info size: ");
    serial_print_hex(total_size);
    serial_puts("\n");

    /* Skip the size field */
    struct multiboot_tag* tag = (struct multiboot_tag*)((uint8_t*)mb2_info + 8);

    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        console_puts("Tag type: ");
        print_hex(tag->type);
        console_puts(" size: ");
        print_hex(tag->size);
        console_puts("\n");

        serial_puts("Tag type: ");
        serial_print_hex(tag->type);
        serial_puts(" size: ");
        serial_print_hex(tag->size);
        serial_puts("\n");

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                {
                    struct multiboot_tag_framebuffer* fb_tag =
                        (struct multiboot_tag_framebuffer*)tag;

                    /* Map framebuffer address properly */
                    if (fb_tag->framebuffer_addr < 0x100000000ULL) {
                        /* 32-bit address, use directly */
                        framebuffer = (uint32_t*)(uintptr_t)fb_tag->framebuffer_addr;
                    } else {
                        /* 64-bit address, need to handle differently */
                        framebuffer = (uint32_t*)(uintptr_t)fb_tag->framebuffer_addr;
                        serial_puts("WARNING: 64-bit framebuffer address!\n");
                    }
                    fb_width = fb_tag->framebuffer_width;
                    fb_height = fb_tag->framebuffer_height;
                    fb_pitch = fb_tag->framebuffer_pitch;
                    fb_bpp = fb_tag->framebuffer_bpp;

                    /* Store color field positions and sizes */
                    fb_red_pos = fb_tag->framebuffer_red_field_position;
                    fb_red_size = fb_tag->framebuffer_red_mask_size;
                    fb_green_pos = fb_tag->framebuffer_green_field_position;
                    fb_green_size = fb_tag->framebuffer_green_mask_size;
                    fb_blue_pos = fb_tag->framebuffer_blue_field_position;
                    fb_blue_size = fb_tag->framebuffer_blue_mask_size;

                    console_puts("Framebuffer found!\n");
                    console_puts("  Address: ");
                    print_hex((uint32_t)(uintptr_t)framebuffer);
                    console_puts("\n  Width: ");
                    print_hex(fb_width);
                    console_puts("\n  Height: ");
                    print_hex(fb_height);
                    console_puts("\n  Pitch: ");
                    print_hex(fb_pitch);
                    console_puts("\n  BPP: ");
                    print_hex(fb_tag->framebuffer_bpp);
                    console_puts("\n  Type: ");
                    print_hex(fb_tag->framebuffer_type);
                    console_puts("\n");

                    serial_puts("Framebuffer found!\n");
                    serial_puts("  Address: ");
                    serial_print_hex((uint32_t)(uintptr_t)framebuffer);
                    serial_puts("\n  Width: ");
                    serial_print_hex(fb_width);
                    serial_puts("\n  Height: ");
                    serial_print_hex(fb_height);
                    serial_puts("\n  Pitch: ");
                    serial_print_hex(fb_pitch);
                    serial_puts("\n  BPP: ");
                    serial_print_hex(fb_tag->framebuffer_bpp);
                    serial_puts("\n  Type: ");
                    serial_print_hex(fb_tag->framebuffer_type);
                    serial_puts("\n");

                    if (fb_tag->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
                        console_puts("  RGB mode:\n");
                        console_puts("    Red pos: ");
                        print_hex(fb_tag->framebuffer_red_field_position);
                        console_puts(" size: ");
                        print_hex(fb_tag->framebuffer_red_mask_size);
                        console_puts("\n    Green pos: ");
                        print_hex(fb_tag->framebuffer_green_field_position);
                        console_puts(" size: ");
                        print_hex(fb_tag->framebuffer_green_mask_size);
                        console_puts("\n    Blue pos: ");
                        print_hex(fb_tag->framebuffer_blue_field_position);
                        console_puts(" size: ");
                        print_hex(fb_tag->framebuffer_blue_mask_size);
                        console_puts("\n");

                        serial_puts("  RGB mode:\n");
                        serial_puts("    Red pos: ");
                        serial_print_hex(fb_tag->framebuffer_red_field_position);
                        serial_puts(" size: ");
                        serial_print_hex(fb_tag->framebuffer_red_mask_size);
                        serial_puts("\n    Green pos: ");
                        serial_print_hex(fb_tag->framebuffer_green_field_position);
                        serial_puts(" size: ");
                        serial_print_hex(fb_tag->framebuffer_green_mask_size);
                        serial_puts("\n    Blue pos: ");
                        serial_print_hex(fb_tag->framebuffer_blue_field_position);
                        serial_puts(" size: ");
                        serial_print_hex(fb_tag->framebuffer_blue_mask_size);
                        serial_puts("\n");
                    }
                }
                break;
        }

        /* Move to next tag (aligned to 8 bytes) */
        tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }
}

/* Desktop drawing has been moved to Finder - see demo_desktop.c for reference */
/* void draw_system71_desktop(void) - REMOVED - Now handled by Finder */

#if 0
void draw_system71_desktop_REMOVED(void) {
    if (!framebuffer) {
        console_puts("ERROR: No framebuffer!\n");
        serial_puts("ERROR: No framebuffer!\n");
        return;
    }

    /* console_puts("Drawing System 7.1 desktop...\n"); - disabled in graphics mode */
    serial_puts("Drawing System 7.1 desktop...\n");

    /* Debug: Print actual resolution */
    serial_puts("Actual framebuffer resolution: ");
    serial_print_hex(fb_width);
    serial_puts(" x ");
    serial_print_hex(fb_height);
    serial_puts(" @ ");
    serial_print_hex((uint32_t)(uintptr_t)framebuffer);
    serial_puts("\n");

    /* Print framebuffer details */
    serial_puts("BPP: ");
    serial_print_hex(fb_bpp);
    serial_puts(" Pitch: ");
    serial_print_hex(fb_pitch);
    serial_puts("\n");

    /* Print color format info */
    serial_puts("Color format: R");
    serial_print_hex(fb_red_size);
    serial_puts("@");
    serial_print_hex(fb_red_pos);
    serial_puts(" G");
    serial_print_hex(fb_green_size);
    serial_puts("@");
    serial_print_hex(fb_green_pos);
    serial_puts(" B");
    serial_print_hex(fb_blue_size);
    serial_puts("@");
    serial_print_hex(fb_blue_pos);
    serial_puts("\n");

    /* Check if we got valid color masks */
    if (fb_red_size == 0 && fb_green_size == 0 && fb_blue_size == 0) {
        serial_puts("WARNING: No color masks! Framebuffer might be in text mode!\n");
    }


    /* Classic Mac System 7.1 colors - authentic palette */
    uint32_t teal = pack_color(0, 156, 156);          /* Teal color */
    uint32_t light_blue = pack_color(100, 200, 200);  /* Light blue/cyan */
    uint32_t menu_bar_white = pack_color(255, 255, 255);  /* White menu bar */
    uint32_t black = pack_color(0, 0, 0);  /* Black */
    uint32_t window_blue = pack_color(0, 100, 200);  /* Classic Mac blue */
    uint32_t red = pack_color(255, 0, 0);  /* Red for testing */

    /* Fill desktop with exact System 7.1 8x8 teal pattern */
    /* Define the exact 8x8 pattern */
    static const uint8_t pattern[8][8] = {
        {1, 0, 0, 1, 1, 0, 0, 1},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {1, 0, 0, 1, 1, 0, 0, 1},
        {1, 0, 0, 1, 1, 0, 0, 1},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {0, 1, 1, 0, 0, 1, 1, 0},
        {1, 0, 0, 1, 1, 0, 0, 1}
    };

    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++) {
            /* Tile the 8x8 pattern */
            int pattern_x = x % 8;
            int pattern_y = y % 8;

            /* Use the pattern lookup table */
            if (pattern[pattern_y][pattern_x]) {
                row[x] = teal;        /* Teal pixels */
            } else {
                row[x] = light_blue;  /* Light blue pixels */
            }
        }
    }

    /* Draw white menu bar at top (20 pixels high) */
    for (uint32_t y = 0; y < 20; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++) {
            row[x] = menu_bar_white;  /* White menu bar */
        }
    }

    /* Draw menu bar bottom border */
    uint32_t* border_row = (uint32_t*)((uint8_t*)framebuffer + 20 * fb_pitch);
    for (uint32_t x = 0; x < fb_width; x++) {
        border_row[x] = black;
    }

    /* Old static menu drawing disabled - using real Menu Manager now */
    #if 0
    /* Draw Apple logo and menu items (moved down 1 pixel) */
    draw_apple_logo(10, 2, black);
    draw_text_string(35, 5, "File", black);
    draw_text_string(75, 5, "Edit", black);
    draw_text_string(115, 5, "View", black);
    draw_text_string(160, 5, "Special", black);
    draw_text_string(220, 5, "Label", black);

    /* Draw right side of menu bar - clock */
    draw_text_string(fb_width - 80, 5, "12:00 PM", black);
    #endif

    /* Reset window counter for proper active/inactive state */
    window_count = 0;

    /* Old static window drawing disabled - using real Window Manager now */
    #if 0
    /* Draw a sample window (this will be inactive/background) */
    draw_window(100, 100, 400, 300, "System 7.1 Portable");

    /* Draw a second window to show overlapping (this will be active/foreground) */
    draw_window(250, 200, 350, 250, "About This Macintosh");

    /* Draw trash can in bottom right */
    draw_icon(fb_width - 80, fb_height - 80, ICON_TRASH);

    /* Draw "Trash" label centered under the icon */
    /* "Trash" = 5 chars * 7 pixels = 35 pixels wide, icon is 32 pixels wide */
    draw_text_string(fb_width - 80 + (32 - 35) / 2, fb_height - 80 + 36, "Trash", black);
    #endif

    /* console_puts("Desktop drawn\n"); - disabled in graphics mode */
    /* serial_puts("Desktop drawn\n"); */
}

/* Include the Chicago font data */
#include "chicago_font_simple.h"

/* Helper function to draw text using bitmap font */
void draw_text_string(uint32_t x, uint32_t y, const char* text, uint32_t color) {
    uint32_t orig_x = x;

    while (*text) {
        if (*text == '\n') {
            y += 9;
            x = orig_x;
            text++;
            continue;
        }

        unsigned char c = (unsigned char)*text;

        /* Use bitmap font data for rendering */
        if (c < 128) {
            /* Draw character from bitmap font - bits are stored LSB first */
            for (int row = 0; row < 8; row++) {
                uint8_t line = simple_font_8x8[c][row];
                for (int col = 0; col < 8; col++) {
                    /* Check if bit is set (LSB first, so read from right to left) */
                    if (line & (1 << col)) {
                        /* Pixel is set in font bitmap */
                        if (x + col < fb_width && y + row < fb_height) {
                            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                            (y + row) * fb_pitch + (x + col) * 4);
                            *pixel = color;
                        }
                    }
                }
            }
        } else if (0) {
            /* Draw F pattern */
            for (int i = 0; i < 5; i++) { /* Top horizontal */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            for (int i = 0; i < 7; i++) { /* Vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 0; i < 3; i++) { /* Middle horizontal */
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'E' || c == 'e') {
            /* Draw E pattern */
            for (int i = 0; i < 5; i++) { /* Top */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            for (int i = 0; i < 7; i++) { /* Vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 0; i < 3; i++) { /* Middle */
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            for (int i = 0; i < 5; i++) { /* Bottom */
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'i' || c == 'I' || c == 'l' || c == 'L' || c == '1') {
            /* Draw I pattern - just a vertical line */
            for (int i = 0; i < 7; i++) {
                if (x + 2 < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 2) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'T' || c == 't') {
            /* Draw T pattern */
            for (int i = 0; i < 5; i++) { /* Top horizontal */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 7; i++) { /* Vertical center */
                if (x + 2 < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 2) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'V' || c == 'v') {
            /* Draw V pattern */
            for (int i = 0; i < 5; i++) {
                if (x + i/2 < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + i/2) * 4);
                    *p1 = color;
                }
                if (x + 4 - i/2 < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4 - i/2) * 4);
                    *p2 = color;
                }
            }
        }
        else if (c == 'S' || c == 's') {
            /* Draw S pattern */
            for (int i = 1; i < 4; i++) { /* Top */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            /* Left top */
            if (x < fb_width && y + 1 < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 1) * fb_pitch + x * 4);
                *p = color;
            }
            for (int i = 1; i < 4; i++) { /* Middle */
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            /* Right bottom */
            if (x + 4 < fb_width && y + 5 < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 5) * fb_pitch + (x + 4) * 4);
                *p = color;
            }
            for (int i = 1; i < 4; i++) { /* Bottom */
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'a' || c == 'A') {
            /* Draw A pattern */
            /* Peak */
            if (x + 2 < fb_width && y < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + 2) * 4);
                *p = color;
            }
            /* Sides */
            for (int i = 1; i < 6; i++) {
                if (x + (i < 3 ? 2-i : i-3) < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + (i < 3 ? 2-i : i-3)) * 4);
                    *p1 = color;
                }
                if (x + (i < 3 ? 2+i : 5-(i-3)) < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + (i < 3 ? 2+i : 5-(i-3))) * 4);
                    *p2 = color;
                }
            }
            /* Crossbar */
            for (int i = 1; i < 4; i++) {
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'c' || c == 'C') {
            /* Draw C pattern */
            for (int i = 1; i < 4; i++) { /* Top */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 6; i++) { /* Left side */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 4; i++) { /* Bottom */
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'd' || c == 'D') {
            /* Draw D pattern */
            for (int i = 0; i < 7; i++) { /* Left side */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 3; i++) { /* Top/Bottom */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p1 = color;
                }
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p2 = color;
                }
            }
            for (int i = 1; i < 6; i++) { /* Right curve */
                if (x + 3 < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 3) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'w' || c == 'W') {
            /* Draw W pattern */
            for (int i = 0; i < 6; i++) {
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p1 = color;
                }
                if (x + 4 < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4) * 4);
                    *p2 = color;
                }
            }
            /* Middle valley */
            if (x + 2 < fb_width && y + 4 < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 4) * fb_pitch + (x + 2) * 4);
                *p = color;
            }
            /* Bottom */
            if (x + 1 < fb_width && y + 6 < fb_height) {
                uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + 1) * 4);
                *p1 = color;
            }
            if (x + 3 < fb_width && y + 6 < fb_height) {
                uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + 3) * 4);
                *p2 = color;
            }
        }
        else if (c == 'p' || c == 'P') {
            /* Draw P pattern */
            for (int i = 0; i < 7; i++) { /* Vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 4; i++) { /* Top */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            /* Right top */
            if (x + 4 < fb_width && y + 1 < fb_height) {
                uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + 1) * fb_pitch + (x + 4) * 4);
                *p1 = color;
            }
            if (x + 4 < fb_width && y + 2 < fb_height) {
                uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 2) * fb_pitch + (x + 4) * 4);
                *p2 = color;
            }
            for (int i = 1; i < 4; i++) { /* Middle */
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'M' || c == 'm') {
            /* Draw M pattern */
            for (int i = 0; i < 7; i++) { /* Left/Right verticals */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p1 = color;
                }
                if (x + 4 < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4) * 4);
                    *p2 = color;
                }
            }
            /* Peak */
            if (x + 1 < fb_width && y + 1 < fb_height) {
                uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + 1) * fb_pitch + (x + 1) * 4);
                *p1 = color;
            }
            if (x + 3 < fb_width && y + 1 < fb_height) {
                uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 1) * fb_pitch + (x + 3) * 4);
                *p2 = color;
            }
            if (x + 2 < fb_width && y + 2 < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 2) * fb_pitch + (x + 2) * 4);
                *p = color;
            }
        }
        else if (c == 'h' || c == 'H') {
            /* Draw H pattern */
            for (int i = 0; i < 7; i++) { /* Verticals */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p1 = color;
                }
                if (x + 4 < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4) * 4);
                    *p2 = color;
                }
            }
            for (int i = 1; i < 4; i++) { /* Crossbar */
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'o' || c == 'O' || c == '0') {
            /* Draw O pattern */
            for (int i = 1; i < 4; i++) { /* Top/Bottom */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p1 = color;
                }
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p2 = color;
                }
            }
            for (int i = 1; i < 6; i++) { /* Sides */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p1 = color;
                }
                if (x + 4 < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4) * 4);
                    *p2 = color;
                }
            }
        }
        else if (c == 'n' || c == 'N') {
            /* Draw N pattern */
            for (int i = 0; i < 7; i++) { /* Left vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 0; i < 7; i++) { /* Right vertical */
                if (x + 4 < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4) * 4);
                    *p = color;
                }
            }
            /* Diagonal */
            for (int i = 0; i < 5; i++) {
                if (x + 1 + i < fb_width && y + 1 + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 1 + i) * fb_pitch + (x + 1 + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'r' || c == 'R') {
            /* Draw R pattern - like P but with diagonal leg */
            for (int i = 0; i < 7; i++) { /* Vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 4; i++) { /* Top */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            /* Right top */
            if (x + 4 < fb_width && y + 1 < fb_height) {
                uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + 1) * fb_pitch + (x + 4) * 4);
                *p1 = color;
            }
            if (x + 4 < fb_width && y + 2 < fb_height) {
                uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 2) * fb_pitch + (x + 4) * 4);
                *p2 = color;
            }
            for (int i = 1; i < 4; i++) { /* Middle */
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
            /* Diagonal leg */
            for (int i = 0; i < 3; i++) {
                if (x + 2 + i < fb_width && y + 4 + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 4 + i) * fb_pitch + (x + 2 + i) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'y' || c == 'Y') {
            /* Draw Y pattern */
            for (int i = 0; i < 3; i++) { /* Upper arms */
                if (x + i < fb_width && y + i < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + i) * 4);
                    *p1 = color;
                }
                if (x + 4 - i < fb_width && y + i < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 4 - i) * 4);
                    *p2 = color;
                }
            }
            for (int i = 3; i < 7; i++) { /* Stem */
                if (x + 2 < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + (x + 2) * 4);
                    *p = color;
                }
            }
        }
        else if (c == 'b' || c == 'B') {
            /* Draw B pattern */
            for (int i = 0; i < 7; i++) { /* Vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 3; i++) { /* Top/Middle/Bottom */
                if (x + i < fb_width && y < fb_height) {
                    uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + i) * 4);
                    *p1 = color;
                }
                if (x + i < fb_width && y + 3 < fb_height) {
                    uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + i) * 4);
                    *p2 = color;
                }
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p3 = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p3 = color;
                }
            }
            /* Right curves */
            if (x + 3 < fb_width && y + 1 < fb_height) {
                uint32_t* p1 = (uint32_t*)((uint8_t*)framebuffer + (y + 1) * fb_pitch + (x + 3) * 4);
                *p1 = color;
            }
            if (x + 3 < fb_width && y + 2 < fb_height) {
                uint32_t* p2 = (uint32_t*)((uint8_t*)framebuffer + (y + 2) * fb_pitch + (x + 3) * 4);
                *p2 = color;
            }
            if (x + 3 < fb_width && y + 4 < fb_height) {
                uint32_t* p3 = (uint32_t*)((uint8_t*)framebuffer + (y + 4) * fb_pitch + (x + 3) * 4);
                *p3 = color;
            }
            if (x + 3 < fb_width && y + 5 < fb_height) {
                uint32_t* p4 = (uint32_t*)((uint8_t*)framebuffer + (y + 5) * fb_pitch + (x + 3) * 4);
                *p4 = color;
            }
        }
        else if (c == 'L' || c == 'l') {
            /* Draw L pattern */
            for (int i = 0; i < 7; i++) { /* Vertical */
                if (x < fb_width && y + i < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + i) * fb_pitch + x * 4);
                    *p = color;
                }
            }
            for (int i = 1; i < 5; i++) { /* Bottom */
                if (x + i < fb_width && y + 6 < fb_height) {
                    uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 6) * fb_pitch + (x + i) * 4);
                    *p = color;
                }
            }
        }
        else {
            /* For other characters, draw a simple dot pattern */
            if (x + 2 < fb_width && y + 3 < fb_height) {
                uint32_t* p = (uint32_t*)((uint8_t*)framebuffer + (y + 3) * fb_pitch + (x + 2) * 4);
                *p = color;
            }
        }

        x += 7;  /* Chicago font spacing - 7 pixels for better readability */
        text++;
    }
}

/* Draw the classic Apple logo */
void draw_apple_logo(uint32_t x, uint32_t y, uint32_t color) {
    /* Classic rainbow Apple logo 16x16 */
    static const uint16_t apple_shape[16] = {
        0x0700,  /* .....###........ */
        0x0F80,  /* ....#####....... */
        0x0F80,  /* ....#####....... */
        0x0700,  /* .....###........ */
        0x1FF0,  /* ..#########..... */
        0x3FF8,  /* .###########.... */
        0x7FFC,  /* ############.... */
        0x7FFC,  /* ############.... */
        0xFFFE,  /* #############... */
        0xFFFE,  /* #############... */
        0xFBFE,  /* ##### #######... */
        0x7BFC,  /* .#### ######.... */
        0x7FF8,  /* .###########.... */
        0x3FF0,  /* ..#########..... */
        0x1FE0,  /* ...########..... */
        0x07C0   /* .....#####...... */
    };

    /* Rainbow colors for the Apple logo stripes */
    uint32_t green = pack_color(97, 190, 70);     /* Green leaf/top */
    uint32_t yellow = pack_color(251, 186, 18);   /* Yellow */
    uint32_t orange = pack_color(253, 106, 2);    /* Orange */
    uint32_t red = pack_color(229, 28, 36);       /* Red */
    uint32_t purple = pack_color(146, 39, 143);   /* Purple */
    uint32_t blue = pack_color(30, 129, 176);     /* Blue */

    for (int dy = 0; dy < 16; dy++) {
        uint16_t row_data = apple_shape[dy];

        /* Select color based on row for rainbow effect */
        uint32_t stripe_color;
        if (dy < 3) stripe_color = green;        /* Leaf area */
        else if (dy < 5) stripe_color = green;   /* Top green */
        else if (dy < 7) stripe_color = yellow;  /* Yellow stripe */
        else if (dy < 9) stripe_color = orange;  /* Orange stripe */
        else if (dy < 11) stripe_color = red;    /* Red stripe */
        else if (dy < 13) stripe_color = purple; /* Purple stripe */
        else stripe_color = blue;                /* Blue stripe */

        for (int dx = 0; dx < 16; dx++) {
            if (row_data & (1 << (15 - dx))) {
                if (y + dy < fb_height && x + dx < fb_width) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                      (y + dy) * fb_pitch + (x + dx) * 4);
                    *pixel = stripe_color;
                }
            }
        }
    }
}

/* Draw a classic Mac window */
void draw_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const char* title) {
    /* Use pack_color for proper color format */
    uint32_t white = pack_color(255, 255, 255);
    uint32_t black = pack_color(0, 0, 0);
    uint32_t gray = pack_color(170, 170, 170);
    uint32_t dark_gray = pack_color(85, 85, 85);
    uint32_t title_bar_gray = pack_color(245, 245, 245);  /* Very light gray, almost white */

    /* Draw window shadow (2 pixels down and right) */
    for (uint32_t dy = 2; dy < height + 2; dy++) {
        for (uint32_t dx = 2; dx < width + 2; dx++) {
            if (y + dy < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy) * fb_pitch + (x + dx) * 4);
                /* Draw shadow */
                *pixel = dark_gray;
            }
        }
    }

    /* Draw window background */
    for (uint32_t dy = 0; dy < height; dy++) {
        for (uint32_t dx = 0; dx < width; dx++) {
            if (y + dy < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy) * fb_pitch + (x + dx) * 4);
                *pixel = white;
            }
        }
    }

    /* Draw title bar - last window drawn is active (on top) */
    static int total_windows = 2;  /* We're drawing 2 windows */
    int is_active = (window_count == total_windows - 1);  /* Last window is active/on top */

    for (uint32_t dy = 1; dy < 19; dy++) {
        for (uint32_t dx = 1; dx < width - 1; dx++) {
            if (y + dy < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy) * fb_pitch + (x + dx) * 4);
                if (is_active) {
                    /* Active window: horizontal stripe pattern */
                    if (dy % 2 == 0) {
                        *pixel = white;
                    } else {
                        *pixel = black;
                    }
                } else {
                    /* Inactive window: solid white */
                    *pixel = white;
                }
            }
        }
    }
    window_count++;

    /* Draw window frame (double lines for classic look) */
    draw_rect(x, y, width, height, black);
    draw_rect(x + 1, y + 1, width - 2, height - 2, dark_gray);

    /* Draw title bar separator */
    for (uint32_t dx = 0; dx < width; dx++) {
        if (y + 19 < fb_height && x + dx < fb_width) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                              (y + 19) * fb_pitch + (x + dx) * 4);
            *pixel = black;
        }
    }

    /* Draw close box */
    for (uint32_t dy = 0; dy < 13; dy++) {
        for (uint32_t dx = 0; dx < 13; dx++) {
            if (y + dy + 3 < fb_height && x + dx + 8 < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy + 3) * fb_pitch + (x + dx + 8) * 4);
                if (dy == 0 || dy == 12 || dx == 0 || dx == 12) {
                    *pixel = black;  /* Border */
                } else {
                    *pixel = white;  /* Interior */
                }
            }
        }
    }

    /* Draw zoom box (right side of title bar) */
    for (uint32_t dy = 0; dy < 13; dy++) {
        for (uint32_t dx = 0; dx < 13; dx++) {
            if (y + dy + 3 < fb_height && x + width - 21 + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy + 3) * fb_pitch + (x + width - 21 + dx) * 4);
                if (dy == 0 || dy == 12 || dx == 0 || dx == 12) {
                    *pixel = black;  /* Border */
                } else {
                    *pixel = white;  /* Interior */
                    /* Draw zoom icon (small box inside) */
                    if ((dy >= 4 && dy <= 8) && (dx >= 4 && dx <= 8)) {
                        if (dy == 4 || dy == 8 || dx == 4 || dx == 8) {
                            *pixel = black;
                        }
                    }
                }
            }
        }
    }

    /* Draw window title centered with proper background */
    int title_width = 0;
    const char* t = title;
    while (*t) {
        title_width += 7;  /* 7 pixels per character for Chicago font */
        t++;
    }

    /* Clear background behind title text */
    int title_x = x + (width - title_width) / 2;
    int title_y = y + 6;  /* Center vertically in 19-pixel title bar (moved down 1px) */

    /* Only draw gray background for ACTIVE windows (to make text readable over stripes) */
    if (is_active) {
        /* Draw gray background box behind title - full title bar height */
        for (uint32_t dy = 1; dy < 18; dy++) {  /* Full title bar height (1-18) */
            for (uint32_t dx = 0; dx < title_width + 12; dx++) {  /* Wider padding (6 pixels each side) */
                int px = title_x + dx - 6;
                if (y + dy < fb_height && px >= x + 1 && px < x + width - 1 && px < fb_width) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                     (y + dy) * fb_pitch + px * 4);
                    *pixel = title_bar_gray;  /* Very light gray background for text readability */
                }
            }
        }
    }

    /* Draw the title text */
    draw_text_string(title_x, title_y, title, black);
}

/* Draw a rectangle outline */
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    /* Top and bottom */
    for (uint32_t dx = 0; dx < width; dx++) {
        if (x + dx < fb_width) {
            if (y < fb_height) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + (x + dx) * 4);
                *pixel = color;
            }
            if (y + height - 1 < fb_height) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + height - 1) * fb_pitch + (x + dx) * 4);
                *pixel = color;
            }
        }
    }

    /* Left and right */
    for (uint32_t dy = 0; dy < height; dy++) {
        if (y + dy < fb_height) {
            if (x < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + (y + dy) * fb_pitch + x * 4);
                *pixel = color;
            }
            if (x + width - 1 < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy) * fb_pitch + (x + width - 1) * 4);
                *pixel = color;
            }
        }
    }
}

/* Draw icons */
void draw_icon(uint32_t x, uint32_t y, int icon_type) {
    uint32_t black = pack_color(0, 0, 0);
    uint32_t white = pack_color(255, 255, 255);
    uint32_t gray = pack_color(192, 192, 192);

    /* Classic Mac 32x32 trash can icon */
    if (icon_type == ICON_TRASH) {
        /* Clear background */
        for (uint32_t dy = 0; dy < 32; dy++) {
            for (uint32_t dx = 0; dx < 32; dx++) {
                if (y + dy < fb_height && x + dx < fb_width) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                      (y + dy) * fb_pitch + (x + dx) * 4);
                    /* Skip if it's teal background */
                }
            }
        }

        /* Draw trash can lid (open) */
        for (uint32_t dx = 11; dx < 21; dx++) {
            if (y + 4 < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + 4) * fb_pitch + (x + dx) * 4);
                *pixel = black;
            }
            if (y + 7 < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + 7) * fb_pitch + (x + dx) * 4);
                *pixel = black;
            }
        }
        /* Lid sides */
        for (uint32_t dy = 5; dy < 7; dy++) {
            if (y + dy < fb_height) {
                if (x + 11 < fb_width) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                      (y + dy) * fb_pitch + (x + 11) * 4);
                    *pixel = black;
                }
                if (x + 20 < fb_width) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                      (y + dy) * fb_pitch + (x + 20) * 4);
                    *pixel = black;
                }
            }
        }
        /* Lid handle */
        for (uint32_t dx = 14; dx < 18; dx++) {
            if (y + 2 < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + 2) * fb_pitch + (x + dx) * 4);
                *pixel = black;
            }
        }

        /* Draw trash can body */
        /* Top and bottom edges */
        for (uint32_t dx = 9; dx < 23; dx++) {
            if (y + 10 < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + 10) * fb_pitch + (x + dx) * 4);
                *pixel = black;
            }
            if (y + 28 < fb_height && x + dx < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + 28) * fb_pitch + (x + dx) * 4);
                *pixel = black;
            }
        }
        /* Body sides and ridges */
        for (uint32_t dy = 11; dy < 28; dy++) {
            /* Left edge */
            if (y + dy < fb_height && x + 9 < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy) * fb_pitch + (x + 9) * 4);
                *pixel = black;
            }
            /* Right edge */
            if (y + dy < fb_height && x + 22 < fb_width) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                  (y + dy) * fb_pitch + (x + 22) * 4);
                *pixel = black;
            }
            /* Vertical ridges */
            if (dy > 12 && dy < 26) {
                for (int ridge = 12; ridge < 21; ridge += 3) {
                    if (y + dy < fb_height && x + ridge < fb_width) {
                        uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer +
                                          (y + dy) * fb_pitch + (x + ridge) * 4);
                        *pixel = gray;
                    }
                }
            }
        }
    }
}
#endif /* Removed draw_system71_desktop */

/* Test framebuffer - now handled by Finder */
void test_framebuffer(void) {
    /* Desktop drawing is now done by Finder */
    serial_puts("Desktop rendering delegated to Finder\n");
}

/* External System 7.1 component initialization functions */
extern void InitMemoryManager(void);
extern void InitResourceManager(void);
extern void InitGraf(void *globalPtr);
extern void InitFonts(void);
extern void InitWindows(void);
extern void InitMenus(void);
extern void InitTE(void);
extern void InitDialogs(ResumeProcPtr resumeProc);
extern void InitCursor(void);
extern void InitListManager(void);
extern void InitControlManager_Sys7(void);
extern SInt16 InitEvents(SInt16 numEvents);

/* Window Manager functions */
extern WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                          ConstStr255Param title, Boolean visible,
                          short procID, WindowPtr behind, Boolean goAwayFlag,
                          long refCon);
extern void SetPort(GrafPtr port);
extern void ShowWindow(WindowPtr window);
extern void SelectWindow(WindowPtr window);
extern void DrawControls(WindowPtr window);
extern void DrawGrowIcon(WindowPtr window);

/* Menu Manager functions */
extern MenuHandle NewMenu(short menuID, ConstStr255Param menuTitle);
extern void AppendMenu(MenuHandle menu, ConstStr255Param data);
extern void InsertMenu(MenuHandle menu, short beforeID);
extern void DrawMenuBar(void);

/* Event Manager functions */
extern Boolean GetNextEvent(short eventMask, EventRecord* theEvent);
extern Boolean EventAvail(short eventMask, EventRecord* theEvent);
extern void SystemTask(void);

/* Finder functions */
extern OSErr InitializeFinder(void);
extern void FinderEventLoop(void);
extern OSErr CleanUpDesktop(void);
extern void DrawDesktop(void);

/* Initialize System 7.1 subsystems */
void init_system71(void) {
    /* console_puts("Initializing System 7.1 subsystems...\n"); - disabled in graphics mode */
    serial_puts("Initializing System 7.1 subsystems...\n");

    /* Initialize in proper System 7.1 order per Inside Macintosh */

    /* Memory Manager - foundation of everything */
    InitMemoryManager();
    serial_puts("  Memory Manager initialized\n");

    /* Resource Manager - needed for loading resources */
    InitResourceManager();
    serial_puts("  Resource Manager initialized\n");

    /* QuickDraw - graphics foundation */
    InitGraf(&qd.thePort);
    serial_puts("  QuickDraw initialized\n");

    /* Font Manager */
    InitFonts();
    serial_puts("  Font Manager initialized\n");

    /* Window Manager */
    InitWindows();
    serial_puts("  Window Manager initialized\n");

    /* Menu Manager */
    InitMenus();
    serial_puts("  Menu Manager initialized\n");

    /* Virtual File System */
    VFS_Init();
    serial_puts("  Virtual File System initialized\n");

    /* Mount boot volume */
    if (VFS_MountBootVolume("Macintosh HD")) {
        serial_puts("  Boot volume 'Macintosh HD' mounted\n");

        /* Initialize trash system for boot volume */
        extern bool Trash_Init(void);
        extern bool Trash_OnVolumeMount(uint32_t vref);
        Trash_Init();
        Trash_OnVolumeMount(1);  /* Boot volume is always vRef 1 */
        serial_puts("  Trash system initialized\n");
    } else {
        serial_puts("  WARNING: Failed to mount boot volume\n");
    }

    /* Create minimal Apple menu for compatibility - Finder will add its own */
    static unsigned char appleMenuTitle[] = {1, 0x14};  /* Pascal string: Apple symbol */
    MenuHandle appleMenu = NewMenu(1, appleMenuTitle);
    InsertMenu(appleMenu, 0);

    /* Additional menus will be created by Finder after initialization */

    /* TextEdit */
    InitTE();
    serial_puts("  TextEdit initialized\n");

    /* Dialog Manager */
    InitDialogs(NULL);
    serial_puts("  Dialog Manager initialized\n");

    /* Cursor */
    InitCursor();
    serial_puts("  Cursor initialized\n");

    /* Control Manager */
    InitControlManager_Sys7();
    serial_puts("  Control Manager initialized\n");

    /* List Manager */
    InitListManager();
    serial_puts("  List Manager initialized\n");

    /* Event Manager */
    InitEvents(20);  /* Initialize with 20 event queue entries */
    serial_puts("  Event Manager initialized\n");

    /* Event Dispatcher */
    extern void InitEventDispatcher(void);
    InitEventDispatcher();
    serial_puts("  Event Dispatcher initialized\n");

    /* Initialize Modern Input System for PS/2 devices */
    extern SInt16 InitModernInput(const char* platform);
    if (InitModernInput("PS2") == noErr) {
        serial_puts("  Modern Input System initialized for PS/2\n");
    } else {
        serial_puts("  WARNING: Modern Input System initialization failed\n");
    }

    /* Initialize PS/2 input devices */
    if (InitPS2Controller()) {
        serial_puts("  PS/2 controller initialized\n");
    } else {
        serial_puts("  WARNING: PS/2 controller initialization failed\n");
    }

    /* Initialize Finder */
    OSErr err = InitializeFinder();
    if (err == noErr) {
        serial_puts("  Finder initialized\n");
    } else {
        serial_puts("  Finder initialization failed\n");
    }

    /* console_puts("System 7.1 initialization complete\n"); - disabled in graphics mode */
}

/* Create System 7.1 windows using real Window Manager */
void create_system71_windows(void) {
    Rect windowBounds;
    WindowPtr window1, window2;
    MenuHandle appleMenu, fileMenu, editMenu;

    /* serial_puts("Creating System 7.1 windows...\n"); */

    /* Create Apple menu */
    static unsigned char appleTitle[] = {1, 0x14};  /* Pascal string: length 1, Apple symbol */
    appleMenu = NewMenu(128, appleTitle);
    if (appleMenu) {
        AppendMenu(appleMenu, "\pAbout System 7.1...");
        InsertMenu(appleMenu, 0);
        /* serial_puts("  Apple menu created\n"); */
    }

    /* Create File menu */
    static unsigned char fileTitle[] = {4, 'F', 'i', 'l', 'e'};  /* Pascal string: "File" */
    fileMenu = NewMenu(129, fileTitle);
    if (fileMenu) {
        AppendMenu(fileMenu, "\pNew/N;Open.../O;-;Close/W;Save/S;Save As...;-;Quit/Q");
        InsertMenu(fileMenu, 0);
        /* serial_puts("  File menu created\n"); */
    }

    /* Create Edit menu */
    static unsigned char editTitle[] = {4, 'E', 'd', 'i', 't'};  /* Pascal string: "Edit" */
    editMenu = NewMenu(130, editTitle);
    if (editMenu) {
        AppendMenu(editMenu, "\pUndo/Z;-;Cut/X;Copy/C;Paste/V;Clear");
        InsertMenu(editMenu, 0);
        /* serial_puts("  Edit menu created\n"); */
    }

    /* Draw the menu bar */
    serial_puts("MAIN: About to call DrawMenuBar\n");
    DrawMenuBar();
    serial_puts("MAIN: DrawMenuBar returned\n");

    /* Create first window */
    windowBounds.top = 60;
    windowBounds.left = 40;
    windowBounds.bottom = 250;
    windowBounds.right = 400;

    window1 = NewWindow(NULL, &windowBounds, "\pSystem 7.1 Window",
                       true,  /* visible */
                       0,     /* documentProc */
                       (WindowPtr)-1L,  /* frontmost */
                       true,  /* goAway box */
                       0);    /* refCon */

    if (window1) {
        SetPort((GrafPtr)window1);
        ShowWindow(window1);
        /* serial_puts("  Window 1 created and shown\n"); */
    }

    /* Create second window */
    windowBounds.top = 100;
    windowBounds.left = 100;
    windowBounds.bottom = 300;
    windowBounds.right = 500;

    window2 = NewWindow(NULL, &windowBounds, "\pAnother Window",
                       true,  /* visible */
                       0,     /* documentProc */
                       (WindowPtr)-1L,  /* frontmost */
                       true,  /* goAway box */
                       0);    /* refCon */

    if (window2) {
        SetPort((GrafPtr)window2);
        ShowWindow(window2);
        SelectWindow(window2);
        /* serial_puts("  Window 2 created and shown\n"); */
    }

    /* serial_puts("Windows created successfully\n"); */
}

/* Kernel main entry point */
void kernel_main(uint32_t magic, uint32_t* mb2_info) {
    /* Initialize serial port for debugging */
    serial_init();
    /* serial_puts("System 7.1 Portable - Serial Console Initialized\n"); */

    /* Clear screen and show startup message */
    console_clear();
    /* console_puts("System 7.1 Portable - Iteration2\n");
    console_puts("================================\n\n"); - disabled in graphics mode */

    /* serial_puts("System 7.1 Portable - Iteration2\n");
    serial_puts("================================\n\n"); */

    /* Parse Multiboot2 information */
    parse_multiboot2(magic, mb2_info);

    /* Framebuffer will be used by Finder if available */
    if (framebuffer) {
        /* serial_puts("Framebuffer available for Finder desktop\n"); */
    } else {
        /* console_puts("No framebuffer available, continuing in text mode\n"); */
        /* serial_puts("No framebuffer available, continuing in text mode\n"); */
    }

    /* Initialize System 7.1 */
    /* serial_puts("Initializing System 7.1...\n"); */
    init_system71();

    /* Remove early test - let DrawDesktop do all the drawing */
    if (framebuffer) {

        /* serial_puts("Setting up desktop port...\n"); */

        /* Create and open the desktop port */
        static GrafPort desktopPort;
        OpenPort(&desktopPort);

        /* serial_puts("Calling DrawDesktop...\n"); */
        DrawDesktop();
        /* serial_puts("DrawDesktop returned\n"); */
    }

    /* Create windows and menus using real System 7.1 APIs */
    serial_puts("MAIN: About to call create_system71_windows\n");
    create_system71_windows();
    serial_puts("MAIN: create_system71_windows returned\n");

    /* Main event loop using real Event Manager */
    /* Don't use console_puts after graphics mode - it overwrites framebuffer! */
    /* console_puts("\nSystem ready. Processing events...\n"); */
    /* serial_puts("\nSystem ready. Entering event loop.\n"); */

    EventRecord event;

    /* Initial desktop draw */
    /* WM_Update was a bootstrap function, now quarantined. TODO: Use real Toolbox APIs */
    serial_puts("MAIN: Desktop init complete\n");
    /* serial_puts("MAIN: About to call WM_Update\n"); */
    /* WM_Update(); */    /* Draw the desktop directly */
    /* serial_puts("MAIN: WM_Update returned\n"); */

    /* Draw the volume and trash icons */
    extern void DrawVolumeIcon(void);
    serial_puts("MAIN: About to call DrawVolumeIcon\n");
    DrawVolumeIcon();
    serial_puts("MAIN: DrawVolumeIcon returned\n");

    /* Track mouse position for movement detection */
    extern struct {
        int16_t x;
        int16_t y;
        uint8_t buttons;
        uint8_t packet[3];
        uint8_t packet_index;
    } g_mouseState;

    /* Simple cursor drawing - directly to framebuffer for responsiveness */
    static int16_t cursor_old_x = -1;
    static int16_t cursor_old_y = -1;
    static uint32_t cursor_saved_pixels[16][16];  /* Save area under cursor */
    static bool cursor_saved = false;

    /* Draw initial cursor */
#if 1
    /* Draw initial cursor with safety checks */
    if (framebuffer && fb_width > 0 && fb_height > 0) {
        int16_t x = g_mouseState.x;
        int16_t y = g_mouseState.y;

        /* Save pixels under cursor position */
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb_width && py >= 0 && py < fb_height) {
                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + py * fb_pitch + px * 4);
                    cursor_saved_pixels[row][col] = *pixel;
                }
            }
        }

        /* Draw cursor using arrow_cursor and arrow_cursor_mask data */
        for (int row = 0; row < 16; row++) {
            uint16_t cursor_row = (arrow_cursor[row*2] << 8) | arrow_cursor[row*2 + 1];
            uint16_t mask_row = (arrow_cursor_mask[row*2] << 8) | arrow_cursor_mask[row*2 + 1];

            for (int col = 0; col < 16; col++) {
                if (mask_row & (0x8000 >> col)) {  /* Check mask bit */
                    int px = x + col;
                    int py = y + row;
                    if (px >= 0 && px < fb_width && py >= 0 && py < fb_height) {
                        uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + py * fb_pitch + px * 4);
                        /* Draw black for cursor bits, white for background */
                        if (cursor_row & (0x8000 >> col)) {
                            *pixel = 0xFF000000;  /* Black */
                        } else {
                            *pixel = 0xFFFFFFFF;  /* White */
                        }
                    }
                }
            }
        }

        cursor_old_x = x;
        cursor_old_y = y;
        cursor_saved = true;
    }
#endif

    int16_t last_mouse_x = g_mouseState.x;
    int16_t last_mouse_y = g_mouseState.y;
    volatile uint32_t debug_counter = 0;

    /* serial_puts("Entering main event loop...\n"); */

    /* Simple immediate test */
    /* serial_puts("TEST: About to enter loop\n"); */

    /* Add cursor update counter to throttle cursor redraws */
    static uint32_t cursor_update_counter = 0;

    serial_printf("MAIN: Entering main event loop NOW!\n");

    while (1) {
        /* Simple alive message every 1 million iterations */
        static uint32_t simple_counter = 0;
        simple_counter++;
        if ((simple_counter % 1000000) == 0) {
            serial_puts(".");  /* Just print a dot to show we're alive */
            if ((simple_counter % 10000000) == 0) {
                serial_printf("\nLOOP: counter=%u\n", simple_counter);
                simple_counter = 0;
            }
        }

        /* Process modern input events (PS/2 keyboard and mouse) */
        extern void ProcessModernInput(void);
        ProcessModernInput();

        /* Check for events and dispatch them - MUST happen on every iteration */
        extern Boolean GetNextEvent(short eventMask, EventRecord* theEvent);
        extern Boolean DispatchEvent(EventRecord* event);

        EventRecord evt;
        /* Debug: before GetNextEvent */
        static int call_count = 0;
        if ((call_count++ % 10000) == 0) {
            serial_printf("MAIN: Calling GetNextEvent (call #%d)\n", call_count);
        }

        if (GetNextEvent(everyEvent, &evt)) {
            /* Debug: show what events we're getting */
            serial_printf("MAIN: Got event type=%d at (%d,%d)\n",
                         evt.what, evt.where.h, evt.where.v);
            if (evt.what == mouseDown) {
                serial_printf("MAIN: Got mouseDown at (%d,%d)\n", evt.where.h, evt.where.v);
            }
            DispatchEvent(&evt);
        }

        /* Throttle ONLY cursor drawing, not event processing */
        cursor_update_counter++;
        if (cursor_update_counter < 500) {
            /* Skip only the cursor drawing, but continue to process events below */
            goto skip_cursor_drawing;
        }
        cursor_update_counter = 0;

        /* Redraw cursor if mouse moved */
#if 1
        if (g_mouseState.x != last_mouse_x || g_mouseState.y != last_mouse_y) {
            /* Clamp mouse position to screen bounds */
            int16_t x = g_mouseState.x;
            int16_t y = g_mouseState.y;

            if (x < 0) x = 0;
            if (x >= fb_width) x = fb_width - 1;
            if (y < 0) y = 0;
            if (y >= fb_height) y = fb_height - 1;

            /* Simple direct cursor drawing */
            uint32_t* fb = (uint32_t*)framebuffer;
            int pitch_dwords = fb_pitch / 4;

            /* Erase old cursor */
            if (cursor_saved) {
                for (int row = 0; row < 16; row++) {
                    int py = cursor_old_y + row;
                    if (py >= 0 && py < fb_height) {
                        for (int col = 0; col < 16; col++) {
                            int px = cursor_old_x + col;
                            if (px >= 0 && px < fb_width) {
                                fb[py * pitch_dwords + px] = cursor_saved_pixels[row][col];
                            }
                        }
                    }
                }
            }

            /* Save and draw new cursor */
            for (int row = 0; row < 16; row++) {
                uint16_t cursor_row = (arrow_cursor[row*2] << 8) | arrow_cursor[row*2 + 1];
                uint16_t mask_row = (arrow_cursor_mask[row*2] << 8) | arrow_cursor_mask[row*2 + 1];

                int py = y + row;
                if (py >= 0 && py < fb_height) {
                    for (int col = 0; col < 16; col++) {
                        int px = x + col;
                        if (px >= 0 && px < fb_width) {
                            int idx = py * pitch_dwords + px;

                            /* Save pixel */
                            cursor_saved_pixels[row][col] = fb[idx];

                            /* Draw cursor */
                            if (mask_row & (0x8000 >> col)) {
                                if (cursor_row & (0x8000 >> col)) {
                                    fb[idx] = 0xFF000000;  /* Black */
                                } else {
                                    fb[idx] = 0xFFFFFFFF;  /* White */
                                }
                            }
                        }
                    }
                }
            }

            cursor_old_x = x;
            cursor_old_y = y;
            cursor_saved = true;

            last_mouse_x = g_mouseState.x;
            last_mouse_y = g_mouseState.y;

            /* Update menu highlighting if tracking */
            extern Boolean IsMenuTrackingNew(void);
            extern void UpdateMenuTrackingNew(Point mousePt);
            if (IsMenuTrackingNew()) {
                Point currentPos = {y, x};  /* Point is {v, h} in QuickDraw */
                UpdateMenuTrackingNew(currentPos);
            }

            /* Only redraw desktop very rarely - the cursor handles its own drawing */
            static int movement_count = 0;
            movement_count++;
            if (movement_count > 10000) {  /* Redraw desktop every 10000 movements (basically never during normal use) */
                serial_printf("MAIN: Full redraw after %d movements\n", movement_count);
                /* WM_Update();  */ /* Full redraw - WM_Update quarantined, TODO: Use real Toolbox APIs */
                /* Cursor will be redrawn on next movement */
                movement_count = 0;
                /* cursor_saved = false; */ /* Force redraw after WM_Update */
            }
        }
#endif /* End of disabled cursor drawing */

        /* Mouse button tracking moved to EventManager - events are properly dispatched now */

skip_cursor_drawing:
        /* Re-enable SystemTask and GetNextEvent for event processing */
#if 1
        /* System 7.1 cooperative multitasking */
        SystemTask();

        /* Process serial commands for menu testing */
#if DEBUG_SERIAL_MENU_COMMANDS
        process_serial_command();
#endif

        /* Get and process events via DispatchEvent */
        if (GetNextEvent(everyEvent, &event)) {
            /* Let DispatchEvent handle all events instead of duplicating here */
            DispatchEvent(&event);
        }
#endif /* #if 1 */

#if 0
        /* OLD CODE: Direct event handling - removed to avoid duplication */
        if (GetNextEvent(mouseDown | mouseUp | keyDown | autoKey, &event)) {
            switch (event.what) {
                case mouseDown:
                    {
                        Point pt = {event.where.h, event.where.v};

                        /* Check if click is in menu bar (top 20 pixels) */
                        if (pt.v >= 0 && pt.v < 20) {
                            serial_printf("MAIN: mouseDown at (%d,%d) in menu bar - calling MenuSelect\n", pt.h, pt.v);
                            long menuChoice = MenuSelect(pt);
                            short menuID = (short)(menuChoice >> 16);
                            short item = (short)(menuChoice & 0xFFFF);

                            if (menuID && item) {
                                serial_printf("Menu selection: menu %d, item %d\n", menuID, item);
                                DoMenuCommand(menuID, item);
                            }

                            /* Redraw menu bar after selection */
                            DrawMenuBar();
                        } else {
                            serial_puts("Mouse down outside menu bar\n");
                        }
                    }
                    break;
                case keyDown:
                case autoKey:
                    {
                        char key = event.message & 0xFF;
                        Boolean cmdDown = (event.modifiers & cmdKey) != 0;

                        serial_printf("Key event: key='%c' (0x%02X), cmd=%s\n",
                                     key, key, cmdDown ? "Yes" : "No");

                        /* Check for menu keyboard shortcuts */
                        if (cmdDown) {
                            long menuChoice = MenuKey(key);
                            short menuID = (short)(menuChoice >> 16);
                            short item = (short)(menuChoice & 0xFFFF);

                            if (menuID && item) {
                                serial_printf("Menu key shortcut: menu %d, item %d\n", menuID, item);
                                DoMenuCommand(menuID, item);

                                /* Flash menu title briefly */
                                HiliteMenu(menuID);
                                /* Small delay for visual feedback */
                                for (volatile int i = 0; i < 1000000; i++);
                                HiliteMenu(0);
                            }
                        }
                    }
                    break;
                case updateEvt:
                    {
                        serial_puts("Update event\n");
                        WindowPtr w = (WindowPtr)event.message;
                        BeginUpdate(w);

                        /* Window Manager draws chrome first */
                        extern void DrawWindow(WindowPtr window);
                        DrawWindow(w);

                        /* Then application draws content */
                        /* Check if this is a Finder window and call its proc */
                        if (w->refCon == 'DISK' || w->refCon == 'TRSH') {
                            extern void FolderWindowProc(WindowPtr window, short message, long param);
                            FolderWindowProc(w, 0, 0);  /* wDraw = 0 */
                        }

                        EndUpdate(w);
                    }
                    break;
                case activateEvt:
                    serial_puts("Activate event\n");
                    break;
                default:
                    break;
            }
        }
#endif /* Disabled SystemTask and GetNextEvent */

        /* Yield CPU when no events - but don't halt, it blocks PS/2 polling! */
        /* __asm__ volatile ("hlt"); */
    }
}
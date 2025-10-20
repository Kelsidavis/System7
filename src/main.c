/* System 7.1 Kernel Main Entry Point */
#include "multiboot.h"
/* No external font header needed - we'll define inline */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

/* Debug flag for serial menu commands - set to 1 to enable, 0 to disable */
#define DEBUG_SERIAL_MENU_COMMANDS 0

/* Include actual System 7.1 headers */
#include "../include/MacTypes.h"
#include "../include/QuickDraw/QuickDraw.h"
#include "../include/ResourceManager.h"
#include "../include/EventManager/EventTypes.h"  /* Include EventTypes first to define activeFlag */
#include "../include/EventManager/EventManager.h"
#include "../include/System71StdLib.h"                 /* for serial_printf & friends */
#include "../include/System/SystemLogging.h"
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
#include "Platform/include/boot.h"

#ifdef ENABLE_GESTALT
#include "../include/Gestalt/Gestalt.h"
#endif
#include "../include/Resources/system7_resources.h"
#include "../include/TimeManager/TimeManager.h"
#include "../include/ExtensionManager/DefLoader.h"
#ifdef ENABLE_PROCESS_COOP
#include "../include/ProcessMgr/ProcessTypes.h"
#endif

/* Forward declaration for DispatchEvent (no header available yet) */
extern Boolean DispatchEvent(EventRecord* evt);

#include "../include/SystemInternal.h"

/* Forward declarations for static functions in this file */
static void process_serial_command(void);
uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);
static void console_putchar(char c);
static void console_puts(const char* str);
static void console_clear(void);
static void print_hex(uint32_t value);
static void parse_multiboot2(uint32_t magic, uint32_t* mb2_info);
static void test_framebuffer(void);
static void init_system71(void);
static void run_performance_tests(void);
static void create_system71_windows(void);
void kernel_main(uint32_t magic, uint32_t* mb2_info);
#if defined(__powerpc__) || defined(__powerpc64__)
static void log_ppc_memory_map(void);
#endif

/* Simple 5x7 font for basic ASCII characters */
static const uint8_t font5x7[][5] __attribute__((unused)) = {
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

#include "Platform/include/io.h"

/* Serial functions are declared in System71StdLib.h */

/* Process serial commands for menu testing */
static void process_serial_command(void) __attribute__((unused));
static void process_serial_command(void) {
#if DEBUG_SERIAL_MENU_COMMANDS
    if (!serial_data_ready()) return;

    char cmd = serial_getchar();

    switch (cmd) {
        case 'm':  /* Activate menu with mouse click */
        case 'M':
            {
                serial_puts("\nSimulating menu click...\n");

                /* Simulate a click on the File menu (at x=50, y=10) */
                Point pt = { .v = 50, .h = 10 };
                long menuChoice = MenuSelect(pt);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    SYSTEM_LOG_DEBUG("Menu selection: menu %d, item %d\n", menuID, item);
                    DoMenuCommand(menuID, item);
                }
                DrawMenuBar();
            }
            break;

        case 'a':  /* Apple menu */
        case 'A':
            {
                serial_puts("\nSimulating Apple menu click...\n");
                Point pt = { .v = 20, .h = 10 };
                long menuChoice = MenuSelect(pt);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    SYSTEM_LOG_DEBUG("Menu selection: menu %d, item %d\n", menuID, item);
                    DoMenuCommand(menuID, item);
                }
                DrawMenuBar();
            }
            break;

#ifdef ENABLE_GESTALT
        case 'g':  /* Gestalt query */
        case 'G':
            {
                serial_puts("\nGestalt query - enter 4 characters: ");
                char selector[4];
                int i;
                for (i = 0; i < 4; i++) {
                    while (!serial_data_ready());
                    selector[i] = serial_getchar();
                    serial_putchar(selector[i]);
                }
                serial_puts("\n");

                OSType sel = FOURCC(selector[0], selector[1], selector[2], selector[3]);
                long value;
                OSErr err = Gestalt(sel, &value);

                if (err == noErr) {
                    serial_puts("Result: 0x");
                    print_hex(value);
                    serial_puts("\n");
                } else if (err == gestaltUnknownErr) {
                    serial_puts("Selector not found\n");
                } else {
                    SYSTEM_LOG_DEBUG("Error: %d\n", err);
                }
            }
            break;
#endif

        case 'f':  /* File menu */
        case 'F':
            {
                serial_puts("\nSimulating File menu click...\n");
                Point pt = { .v = 50, .h = 10 };
                long menuChoice = MenuSelect(pt);
                short menuID = (short)(menuChoice >> 16);
                short item = (short)(menuChoice & 0xFFFF);

                if (menuID && item) {
                    SYSTEM_LOG_DEBUG("Menu selection: menu %d, item %d\n", menuID, item);
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
                    SYSTEM_LOG_DEBUG("MenuKey found: menu %d, item %d for key '%c'\n", menuID, item, key);
                    DoMenuCommand(menuID, item);
                } else {
                    SYSTEM_LOG_DEBUG("No menu command for key '%c'\n", key);
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
            SYSTEM_LOG_DEBUG("Unknown command '%c' (0x%02x). Press 'h' for help.\n", cmd, cmd);
            break;
    }
#endif /* DEBUG_SERIAL_MENU_COMMANDS */
}

#if defined(__i386__) || defined(__x86_64__)
/* VGA text mode for early output (x86 only) */
static uint16_t* vga_buffer = (uint16_t*)0xB8000;
static size_t vga_row = 0;
static size_t vga_col = 0;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const uint8_t VGA_COLOR = 0x0F; /* White on black */
#endif

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

/* System memory globals */
uint32_t g_total_memory_kb = 8 * 1024;  /* Default 8MB if not detected */

/* Window management */
static int window_count __attribute__((unused)) = 0;

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
static void console_putchar(char c) {
#if defined(__i386__) || defined(__x86_64__)
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
#else
    (void)framebuffer;
    serial_putchar(c);
#endif
}

static void console_puts(const char* str) {
#if defined(__i386__) || defined(__x86_64__)
    while (*str) {
        console_putchar(*str++);
    }
#else
    serial_puts(str);
#endif
}

static void console_clear(void) {
#if defined(__i386__) || defined(__x86_64__)
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = ' ' | ((uint16_t)VGA_COLOR << 8);
        }
    }
    vga_row = 0;
    vga_col = 0;
#else
    serial_puts("\n");
#endif
}

#if defined(__powerpc__) || defined(__powerpc64__)
static void log_ppc_memory_map(void) {
    ofw_memory_range_t ranges[OFW_MAX_MEMORY_RANGES];
    size_t count = hal_ppc_get_memory_ranges(ranges, OFW_MAX_MEMORY_RANGES);
    if (count == 0) {
        serial_puts("[SYS] Firmware reported no memory ranges\n");
        return;
    }

    serial_printf("[SYS] Firmware memory map has %u range(s)\n", (unsigned)count);
    for (size_t i = 0; i < count; ++i) {
        uint64_t base = ranges[i].base;
        uint64_t size = ranges[i].size;
        uint32_t base_hi = (uint32_t)(base >> 32);
        uint32_t base_lo = (uint32_t)(base & 0xFFFFFFFFu);
        uint32_t size_hi = (uint32_t)(size >> 32);
        uint32_t size_lo = (uint32_t)(size & 0xFFFFFFFFu);
        serial_printf("[SYS]   range %u: base=0x%08x%08x size=0x%08x%08x\n",
                      (unsigned)i,
                      base_hi, base_lo,
                      size_hi, size_lo);
    }
}
#endif

/* Helper to print hex values */
static void print_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 7; i >= 0; i--) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

/* Parse Multiboot2 info */
static void parse_multiboot2(uint32_t magic, uint32_t* mb2_info) {
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
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                {
                    struct multiboot_tag_basic_meminfo* mem_tag =
                        (struct multiboot_tag_basic_meminfo*)tag;

                    /* Total memory = lower (up to 640KB) + upper (above 1MB) */
                    g_total_memory_kb = mem_tag->mem_lower + mem_tag->mem_upper;

                    serial_puts("Memory detected:\n");
                    serial_puts("  Lower: ");
                    serial_print_hex(mem_tag->mem_lower);
                    serial_puts(" KB\n");
                    serial_puts("  Upper: ");
                    serial_print_hex(mem_tag->mem_upper);
                    serial_puts(" KB\n");
                    serial_puts("  Total: ");
                    serial_print_hex(g_total_memory_kb);
                    serial_puts(" KB\n");
                }
                break;

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


/* Test framebuffer - now handled by Finder */
static void test_framebuffer(void) __attribute__((unused));
static void test_framebuffer(void) {
    /* Desktop drawing is now done by Finder */
    serial_puts("Desktop rendering delegated to Finder\n");
}

/* External System 7.1 component initialization functions */
extern void InitMemoryManager(void);
extern void InitResourceManager(void);

#ifdef ENABLE_RESOURCES
#include "ResourceMgr/ResourceMgr.h"
#endif
extern void InitGraf(void *globalPtr);
extern void InitFonts(void);
extern void InitWindows(void);
extern void InitMenus(void);
extern void InitDialogs(ResumeProcPtr resumeProc);
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
/* GetNextEvent, EventAvail declared in EventManager.h */
extern void SystemTask(void);

/* Finder functions */
extern OSErr InitializeFinder(void);
extern void FinderEventLoop(void);
extern OSErr CleanUpDesktop(void);
extern void DrawDesktop(void);

#ifdef TM_SMOKE_TEST
/* Time Manager test callback */
static struct TMTask gHelloTimer;
static void tm_hello(struct TMTask *t) {
    serial_puts("[TM] Hello from timer!\n");
}
#endif

/* Initialize System 7.1 subsystems */
static void init_system71(void) {
    serial_puts("Initializing System 7.1 subsystems...\n");

    /* Initialize in proper System 7.1 order per Inside Macintosh */

    /* Nanokernel Memory Manager - low-level foundation */
    extern void pmm_init(uint64_t mem_size_bytes, uintptr_t phys_base);
    extern void kheap_init(uintptr_t heap_start, uintptr_t heap_end);

    uint64_t total_bytes = (uint64_t)g_total_memory_kb * 1024;
    serial_puts("  Initializing Nanokernel Memory Manager...\n");
    serial_puts("    Total memory: ");
    serial_print_hex(g_total_memory_kb);
    serial_puts(" KB\n");

    /* Memory layout:
     * 0x000000 - 0x0FFFFF : Low memory (1MB, reserved for BIOS/kernel)
     * 0x100000 - 0x1FFFFF : Kernel code/data/stack
     * 0x200000 - 0x3FFFFF : PMM bitmap region (2MB, enough for 64GB)
     * 0x400000 - 0x4FFFFFF: Kernel heap (12MB)
     * 0x5000000+         : Available for PMM page allocation
     */

    /* Initialize PMM - it will place bitmap at phys_base */
    pmm_init(total_bytes, 0x200000);  /* Bitmap at 2MB */
    serial_puts("    PMM initialized\n");

    /* Initialize kernel heap after PMM bitmap */
    kheap_init(0x400000, 0x1400000);  /* 4MB to 20MB = 16MB heap */
    serial_puts("    Kernel heap initialized (16MB)\n");

    /* Run nanokernel test suite to validate implementation */
    extern void nk_memory_run_tests(void);
    nk_memory_run_tests();

    /* Memory Manager - foundation of everything */
    InitMemoryManager();
    serial_puts("  Memory Manager initialized\n");

    /* Time Manager - low-level timing services */
    OSErr tmErr = InitTimeManager();
    if (tmErr == noErr) {
        serial_puts("  Time Manager initialized\n");

#ifdef ENABLE_PROCESS_COOP
        /* Process Manager cooperative scheduling */
        Proc_Init();
        Event_InitQueue();
        serial_puts("  ProcessMgr (coop) + Event queue initialized\n");
#endif

#ifdef ENABLE_SCRAP
        /* Initialize ScrapManager after ProcessMgr */
        extern void Scrap_Zero(void);
        Scrap_Zero();
        serial_puts("  ScrapManager initialized\n");
#ifdef SCRAP_SELFTEST
        serial_puts("  About to run Scrap self-test\n");
        extern void Scrap_RunSelfTest(void);
        Scrap_RunSelfTest();
        serial_puts("  Scrap self-test complete\n");
#endif
#endif

        /* Smoke test: schedule a timer */
        #ifdef TM_SMOKE_TEST
        InsTime(&gHelloTimer);
        gHelloTimer.tmAddr = (Ptr)tm_hello;
        gHelloTimer.tmCount = 0;
        gHelloTimer.qType = 0; /* one-shot */
        PrimeTime(&gHelloTimer, 2000); /* 2ms */
        serial_puts("  [TM] Test timer scheduled for 2ms\n");
        #endif
    } else {
        serial_puts("  Time Manager init FAILED\n");
    }

    /* Gestalt Manager - must be after Memory Manager, before other subsystems query */
#ifdef ENABLE_GESTALT
    {
        extern void Gestalt_SetInitBit(int bit);

        OSErr err = Gestalt_Init();
        if (err == noErr) {
            serial_puts("  Gestalt Manager initialized\n");

            /* Mark Memory Manager as initialized */
            Gestalt_SetInitBit(0);  /* kGestaltInitBit_MemoryMgr */

            /* Mark Time Manager as initialized if it was successful */
            if (tmErr == noErr) {
                Gestalt_SetInitBit(1);  /* kGestaltInitBit_TimeMgr */
            }
        } else {
            serial_puts("  Gestalt Manager init FAILED\n");
        }
    }
#endif

    /* Resource Manager - needed for loading resources */
    InitResourceManager();
    serial_puts("  Resource Manager initialized\n");

#ifdef ENABLE_GESTALT
    /* Mark Resource Manager as initialized */
    extern void Gestalt_SetInitBit(int bit);
    Gestalt_SetInitBit(2);  /* kGestaltInitBit_ResourceMgr */
#endif

    /* Extension Manager - loads INIT resources and system extensions */
#ifdef ENABLE_EXTENSIONS
    {
        extern OSErr ExtensionManager_Initialize(void);
        extern SInt16 ExtensionManager_ScanForExtensions(Boolean rescan);
        extern OSErr ExtensionManager_LoadAllExtensions(void);
        extern OSErr CDEFLoader_Initialize(void);
        extern SInt16 CDEFLoader_LoadAllCDEFs(void);
        extern OSErr ControlPanelManager_Initialize(void);
        extern SInt16 ControlPanelManager_ScanForControlPanels(Boolean rescan);

        OSErr extErr = ExtensionManager_Initialize();
        if (extErr == noErr) {
            serial_puts("  Extension Manager initialized\n");

            /* Scan for INIT resources */
            SInt16 found = ExtensionManager_ScanForExtensions(false);
            if (found > 0) {
                serial_printf("  Found %d extension resources\n", found);

                /* Load all discovered extensions in priority order */
                extErr = ExtensionManager_LoadAllExtensions();
                if (extErr == noErr) {
                    extern SInt16 ExtensionManager_GetActiveCount(void);
                    SInt16 active = ExtensionManager_GetActiveCount();
                    serial_printf("  Loaded %d active extensions\n", active);
                } else {
                    serial_printf("  WARNING: Extension loading failed with error %d\n", extErr);
                }
            } else {
                serial_puts("  No extension resources found\n");
            }
        } else {
            serial_printf("  WARNING: Extension Manager initialization failed with error %d\n", extErr);
        }

        /* CDEF Loader - loads Control Definition resources */
        extErr = CDEFLoader_Initialize();
        if (extErr == noErr) {
            serial_puts("  CDEF Loader initialized\n");

            /* Load all CDEF resources */
            SInt16 cdefLoaded = CDEFLoader_LoadAllCDEFs();
            if (cdefLoaded > 0) {
                serial_printf("  Loaded %d CDEF resources\n", cdefLoaded);
            } else {
                serial_puts("  No CDEF resources found\n");
            }
        } else {
            serial_printf("  WARNING: CDEF Loader initialization failed with error %d\n", extErr);
        }

        /* Control Panel Manager - discovers and manages control panels */
        extErr = ControlPanelManager_Initialize();
        if (extErr == noErr) {
            serial_puts("  Control Panel Manager initialized\n");

            /* Scan for control panel resources */
            SInt16 cpanels = ControlPanelManager_ScanForControlPanels(false);
            if (cpanels > 0) {
                serial_printf("  Found %d control panels\n", cpanels);
            } else {
                serial_puts("  No control panels found\n");
            }
        } else {
            serial_printf("  WARNING: Control Panel Manager initialization failed with error %d\n", extErr);
        }

        /* DRVR Loader - loads device driver resources */
        extern OSErr DRVRLoader_Initialize(void);
        extern SInt16 DRVRLoader_LoadAllDrivers(void);
        extErr = DRVRLoader_Initialize();
        if (extErr == noErr) {
            serial_puts("  DRVR Loader initialized\n");

            /* Load all device drivers */
            SInt16 drvrsLoaded = DRVRLoader_LoadAllDrivers();
            if (drvrsLoaded > 0) {
                serial_printf("  Loaded %d device drivers\n", drvrsLoaded);
            } else {
                serial_puts("  No device drivers found\n");
            }
        } else {
            serial_printf("  WARNING: DRVR Loader initialization failed with error %d\n", extErr);
        }

        /* FKEY Loader - loads function key resources */
        extern OSErr FKEYLoader_Initialize(void);
        extern SInt16 FKEYLoader_LoadAllFKEYs(void);
        extErr = FKEYLoader_Initialize();
        if (extErr == noErr) {
            serial_puts("  FKEY Loader initialized\n");

            /* Load all function keys */
            SInt16 fkeysLoaded = FKEYLoader_LoadAllFKEYs();
            if (fkeysLoaded > 0) {
                serial_printf("  Loaded %d function key resources\n", fkeysLoaded);
            } else {
                serial_puts("  No function key resources found\n");
            }
        } else {
            serial_printf("  WARNING: FKEY Loader initialization failed with error %d\n", extErr);
        }

        /* Definition Loader - loads WDEF, LDEF, MDEF resources */
        extern OSErr DefLoader_Initialize(void);
        extern SInt16 DefLoader_LoadAllDefinitions(ResType defType);
        extErr = DefLoader_Initialize();
        if (extErr == noErr) {
            serial_puts("  Definition Loader initialized\n");

            /* Load window definitions */
            SInt16 wdefsLoaded = DefLoader_LoadAllDefinitions(WDEF_TYPE);
            if (wdefsLoaded > 0) {
                serial_printf("  Loaded %d WDEF resources\n", wdefsLoaded);
            }

            /* Load list definitions */
            SInt16 ldefsLoaded = DefLoader_LoadAllDefinitions(LDEF_TYPE);
            if (ldefsLoaded > 0) {
                serial_printf("  Loaded %d LDEF resources\n", ldefsLoaded);
            }

            /* Load menu definitions */
            SInt16 mdefsLoaded = DefLoader_LoadAllDefinitions(MDEF_TYPE);
            if (mdefsLoaded > 0) {
                serial_printf("  Loaded %d MDEF resources\n", mdefsLoaded);
            }

            if (wdefsLoaded == 0 && ldefsLoaded == 0 && mdefsLoaded == 0) {
                serial_puts("  No definition resources found\n");
            }
        } else {
            serial_printf("  WARNING: Definition Loader initialization failed with error %d\n", extErr);
        }
    }
#endif

#ifdef ENABLE_RESOURCES
    /* Resource Manager smoke test */
    {
        extern void serial_puts(const char*);
        OSErr err;
        Handle h;

        /* Try to load a PAT resource */
        h = GetResource('PAT ', 1);
        err = ResError();
        if (h && err == noErr) {
            serial_puts("[ResourceMgr] PAT 1 loaded successfully\n");
            ReleaseResource(h);
        } else {
            serial_puts("[ResourceMgr] PAT 1 load FAILED\n");
        }

        /* Try to load a ppat resource */
        h = GetResource('ppat', 100);
        err = ResError();
        if (h && err == noErr) {
            serial_puts("[ResourceMgr] ppat 100 loaded successfully\n");
            ReleaseResource(h);
        } else {
            serial_puts("[ResourceMgr] ppat 100 load FAILED\n");
        }

        /* Try non-existent resource to test error handling */
        h = GetResource('MENU', 256);
        err = ResError();
        if (err == resNotFound) {
            serial_puts("[ResourceMgr] MENU 256 correctly returned resNotFound\n");
        } else {
            serial_puts("[ResourceMgr] MENU 256 unexpected result\n");
        }
    }
#endif

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

    /* Menu Bits Pool - prevents heap fragmentation from menu operations */
    extern OSErr MenuBitsPool_Init(SInt16 numBuffers, SInt32 bufferSize);
    MenuBitsPool_Init(4, 160 * 1024);  /* 4 buffers × 160KB = 640KB pool */
    serial_puts("  Menu Bits Pool initialized (4 × 160KB buffers)\n");

    /* Startup Screen - show "Welcome to Macintosh" */
    extern OSErr InitStartupScreen(const void* config);
    extern OSErr ShowWelcomeScreen(void);
    extern OSErr SetStartupPhase(int phase);
    extern void HideStartupScreen(void);
    extern int nk_is_active(void);

    /* Skip startup screen in nanokernel mode due to graphics memory incompatibility */
    if (nk_is_active()) {
        serial_puts("  Nanokernel active - skipping startup screen\n");
    } else {
        serial_puts("  About to call InitStartupScreen...\n");
        if (InitStartupScreen(NULL) == noErr) {
            serial_puts("  Startup Screen initialized\n");
            serial_puts("  About to call ShowWelcomeScreen...\n");
            ShowWelcomeScreen();
            serial_puts("  Welcome screen displayed\n");
        } else {
            serial_puts("  InitStartupScreen failed!\n");
        }
    }

    /* Storage HAL (ATA/IDE Driver) */
    extern OSErr hal_storage_init(void);
    serial_puts("  Initializing storage subsystem...\n");
    OSErr ata_err = hal_storage_init();
    if (ata_err != noErr) {
        serial_puts("  WARNING: Storage initialization failed\n");
    } else {
        serial_puts("  Storage subsystem initialized\n");
    }

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

        /* Initial file system contents are now created during volume creation in HFS_CreateBlankVolume() */
        serial_puts("  Initial file system contents created during volume initialization\n");
    } else {
        serial_puts("  WARNING: Failed to mount boot volume\n");
    }

    /* ATA volumes will be mounted after Finder initializes */

    /* TextEdit */
    TEInit();
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

#ifdef CTRL_SMOKE_TEST
    /* Create control smoke test window */
    extern void InitControlSmokeTest(void);
    InitControlSmokeTest();
#endif

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

    /* Application Switcher */
    extern OSErr AppSwitcher_Init(void);
    AppSwitcher_Init();
    serial_puts("  Application Switcher initialized\n");

    /* Process Manager - for application launching */
    extern OSErr ProcessManager_Initialize(void);
    if (ProcessManager_Initialize() == noErr) {
        serial_puts("  Process Manager initialized\n");
    } else {
        serial_puts("  WARNING: Process Manager initialization failed\n");
    }

#ifdef TM_SMOKE_TEST
    /* Segment Loader Test Harness (smoke checks for first-light validation) */
    extern void SegmentLoader_TestBoot(void);
    serial_puts("\n");
    SegmentLoader_TestBoot();
    serial_puts("\n");
#endif

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

    /* Initialize Sound Manager */
    extern OSErr SoundManagerInit(void);
    if (SoundManagerInit() == noErr) {
        serial_puts("  Sound Manager initialized\n");
    } else {
        serial_puts("  WARNING: Sound Manager initialization failed\n");
    }

    /* Initialize Speech Manager */
    extern OSErr SpeechManagerInit(void);
    if (SpeechManagerInit() == noErr) {
        serial_puts("  Speech Manager initialized\n");
    } else {
        serial_puts("  WARNING: Speech Manager initialization failed\n");
    }

    /* Hide startup screen before starting Finder */
    HideStartupScreen();

    /* Initialize Finder */
    OSErr err = InitializeFinder();
    if (err == noErr) {
        serial_puts("  Finder initialized\n");

        /* Now mount ATA volumes (callback is registered) */
        extern int hal_storage_get_drive_count(void);
        extern bool VFS_MountATA(int ata_device_index, const char* volName, VRefNum* vref);
        extern bool VFS_FormatATA(int ata_device_index, const char* volName);

        int ata_count = hal_storage_get_drive_count();
        if (ata_count > 0) {
            serial_puts("  Mounting detected ATA volumes...\n");
            for (int i = 0; i < ata_count; i++) {
                VRefNum vref = 0;
                char volName[32];
                volName[0] = 'A'; volName[1] = 'T'; volName[2] = 'A'; volName[3] = ' ';
                volName[4] = 'D'; volName[5] = 'i'; volName[6] = 's'; volName[7] = 'k';
                volName[8] = ' '; volName[9] = '0' + i; volName[10] = '\0';

                /* Try to mount - will fail if disk is not formatted */
                if (VFS_MountATA(i, volName, &vref)) {
                    serial_puts("  ATA volume mounted and added to desktop\n");
                } else {
                    serial_puts("  WARNING: ATA disk is not formatted with HFS\n");
                    serial_puts("  Use VFS_FormatATA() to format this disk\n");
                }
            }
        }
    } else {
        serial_puts("  Finder initialization failed\n");
        serial_puts("  Activating fallback desktop menus\n");
        create_system71_windows();
    }

}

#if 1  /* Performance tests always available */
/* Performance measurement helpers */
static inline uint64_t rdtsc_now(void) {
#ifdef __i386__
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;  /* Not supported on this platform */
#endif
}

/* Simple 64-bit division helper (duplicated for freestanding) */
static uint64_t udiv64(uint64_t num, uint64_t den) {
    if (den == 0) return 0;
    uint64_t quot = 0;
    uint64_t bit = 1ULL << 63;
    while (bit && !(den & bit)) bit >>= 1;
    while (bit) {
        if (num >= den) {
            num -= den;
            quot |= bit;
        }
        den >>= 1;
        bit >>= 1;
    }
    return quot;
}

/* Resource Manager performance benchmark */
static void bench_getresource(void) {
    const int N = 100;
    uint64_t cold_start, cold_end, warm_start, warm_end;
    uint64_t tsc_hz = 2000000000ULL;  /* Default 2GHz if not available */

    /* Try to get actual frequency from TimeBase */
    /* For now, use default 2GHz - would need to expose TimeBase info properly */
    /* This would need to be exposed from TimeBase.c in a real implementation */

    /* Cold misses - first access */
    cold_start = rdtsc_now();
    for (int i = 0; i < N; i++) {
        Handle h = GetResource('PAT ', 1 + (i % 10));
        if (h) ReleaseResource(h);
    }
    cold_end = rdtsc_now();

    /* Warm hits - cached access */
    warm_start = rdtsc_now();
    for (int i = 0; i < N; i++) {
        Handle h = GetResource('PAT ', 1 + (i % 10));
        if (h) ReleaseResource(h);
    }
    warm_end = rdtsc_now();

    /* Convert cycles to microseconds */
    uint64_t cold_cycles = cold_end - cold_start;
    uint64_t warm_cycles = warm_end - warm_start;
    uint64_t cold_us = udiv64(cold_cycles * 1000000ULL, tsc_hz);
    uint64_t warm_us = udiv64(warm_cycles * 1000000ULL, tsc_hz);
    uint64_t cold_per = udiv64(cold_us, N);
    uint64_t warm_per = udiv64(warm_us, N);

    serial_puts("[RM PERF] ");
    print_hex((uint32_t)cold_per);
    serial_puts(" us/cold, ");
    print_hex((uint32_t)warm_per);
    serial_puts(" us/warm\n");
}

/* Time Manager stale callback test */
static volatile int tm_test_called = 0;
static void tm_test_cb(TMTask *t) {
    (void)t;
    tm_test_called++;
}

static void test_cancel_stale(void) {
    TMTask t = {0};

    /* Insert and prime task */
    InsTime(&t);
    t.tmAddr = (void*)tm_test_cb;
    PrimeTime(&t, 1000);  /* 1ms */

    /* Simulate ISR enqueue by calling TimerISR */
    TimeManager_TimerISR();

    /* Cancel the task */
    CancelTime(&t);

    /* Drain deferred queue */
    TimeManager_DrainDeferred(16, 2000);

    /* Check if callback fired */
    if (tm_test_called != 0) {
        serial_puts("[TM TEST] stale callback FIRED (BUG)\n");
    } else {
        serial_puts("[TM TEST] stale callback suppressed (OK)\n");
    }

    /* Clean up */
    RmvTime(&t);
    tm_test_called = 0;
}

/* Run all performance tests */
static void run_performance_tests(void) {
    serial_puts("\n=== Running Performance Tests ===\n");

#ifdef ENABLE_RESOURCES
    bench_getresource();
#endif

    test_cancel_stale();

    serial_puts("=== Performance Tests Complete ===\n\n");
}
#endif /* Performance tests */

/* Create System 7.1 windows using real Window Manager */
static void create_system71_windows(void) {
    MenuHandle appleMenu, fileMenu, editMenu;

    /* Create Apple menu */
    static unsigned char appleTitle[] = {1, 0x14};  /* Pascal string: length 1, Apple symbol */
    appleMenu = NewMenu(128, appleTitle);
    if (appleMenu) {
        static unsigned char aboutItem[] = {20, 'A','b','o','u','t',' ','S','y','s','t','e','m',' ','7','.','1','.','.','.'}; /* Pascal string */
        AppendMenu(appleMenu, aboutItem);
        InsertMenu(appleMenu, 0);
    }

    /* Create File menu */
    static unsigned char fileTitle[] = {4, 'F', 'i', 'l', 'e'};  /* Pascal string: "File" */
    fileMenu = NewMenu(129, fileTitle);
    if (fileMenu) {
        static unsigned char fileItems[] = {56, 'N','e','w','/','N',';','O','p','e','n','.','.','.','/','O',';','-',';','C','l','o','s','e','/','W',';','S','a','v','e','/','S',';','S','a','v','e',' ','A','s','.','.','.',';','-',';','Q','u','i','t','/','Q'}; /* Pascal string */
        AppendMenu(fileMenu, fileItems);
        InsertMenu(fileMenu, 0);
    }

    /* Create Edit menu */
    static unsigned char editTitle[] = {4, 'E', 'd', 'i', 't'};  /* Pascal string: "Edit" */
    editMenu = NewMenu(130, editTitle);
    if (editMenu) {
        static unsigned char editItems[] = {36, 'U','n','d','o','/','Z',';','-',';','C','u','t','/','X',';','C','o','p','y','/','C',';','P','a','s','t','e','/','V',';','C','l','e','a','r'}; /* Pascal string */
        AppendMenu(editMenu, editItems);
        InsertMenu(editMenu, 0);
    }

    /* Draw the menu bar */
    serial_puts("MAIN: About to call DrawMenuBar\n");
    DrawMenuBar();
    serial_puts("MAIN: DrawMenuBar returned\n");

    /* Test windows removed - let Finder/applications create their own windows */
}

/* Mouse state from PS2 controller */
extern struct {
    int16_t x;
    int16_t y;
    uint8_t buttons;
    uint8_t packet[3];
    uint8_t packet_index;
} g_mouseState;

/* Cursor state variables for direct framebuffer cursor drawing */
static int16_t cursor_old_x = -1;
static int16_t cursor_old_y = -1;
static uint32_t cursor_saved_pixels[16][16];  /* Save area under cursor */
static bool cursor_saved = false;

/* InvalidateCursor - Force cursor redraw by resetting cursor state */
void InvalidateCursor(void) {
    cursor_saved = false;
    cursor_old_x = -1;
    cursor_old_y = -1;
}

/* UpdateCursorDisplay - Update cursor on screen if mouse has moved */
void UpdateCursorDisplay(void) {
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;
    extern int IsCursorVisible(void);
    extern const Cursor* CursorManager_GetCurrentCursorImage(void);
    extern Point CursorManager_GetCursorHotspot(void);
    extern void CursorManager_HandleMouseMotion(Point newPos);
    extern Boolean IsMenuTrackingNew(void);

    /* Don't update cursor display while menu is being tracked - prevents cursor changes during menu operations */
    if (IsMenuTrackingNew()) {
        return;
    }

    const Cursor* cursorImage = CursorManager_GetCurrentCursorImage();
    if (!cursorImage) {
        return;
    }

    Point mousePoint = { .v = g_mouseState.y, .h = g_mouseState.x };
    CursorManager_HandleMouseMotion(mousePoint);

    /* Check if cursor is hidden */
    if (!IsCursorVisible()) {
        /* If cursor was previously visible, erase it */
        if (cursor_saved) {
            uint32_t* fb = (uint32_t*)framebuffer;
            int pitch_dwords = fb_pitch / 4;

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
            cursor_saved = false;
        }
        return;  /* Don't draw cursor */
    }

    static Point lastMouse = {SHRT_MIN, SHRT_MIN};

    if (cursor_saved &&
        mousePoint.h == lastMouse.h &&
        mousePoint.v == lastMouse.v) {
        return;
    }

    /* Only redraw if mouse moved or cursor was invalidated */
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
    Point hotSpot = CursorManager_GetCursorHotspot();
    int drawX = mousePoint.h - hotSpot.h;
    int drawY = mousePoint.v - hotSpot.v;

    for (int row = 0; row < 16; row++) {
        uint16_t cursor_row = cursorImage->data[row];
        uint16_t mask_row = cursorImage->mask[row];

        int py = drawY + row;
        if (py >= 0 && py < fb_height) {
            for (int col = 0; col < 16; col++) {
                int px = drawX + col;
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

    cursor_old_x = drawX;
    cursor_old_y = drawY;
    cursor_saved = true;

    lastMouse = mousePoint;
}

/* Kernel main entry point */
void kernel_main(uint32_t magic, uint32_t* mb2_info) {
    /* Initialize serial port for debugging */
    serial_init();
    serial_puts("System 7.1 Portable - Serial Console Initialized\n");

    /* Clear screen and show startup message */
    console_clear();

#if defined(__i386__) || defined(__x86_64__)
    /* Parse Multiboot2 information */
    parse_multiboot2(magic, mb2_info);
#else
    (void)magic;
    (void)mb2_info;
    uint32_t mem_bytes = hal_get_memory_size();
    if (mem_bytes != 0) {
        g_total_memory_kb = mem_bytes / 1024;
        serial_printf("[SYS] HAL memory probe (bytes): %u\n", mem_bytes);
    }
#if defined(__powerpc__) || defined(__powerpc64__)
    log_ppc_memory_map();
    {
        hal_framebuffer_info_t fw_fb;
        if (hal_get_framebuffer_info(&fw_fb) == 0) {
            uintptr_t fb_base = (uintptr_t)fw_fb.framebuffer;
            uint32_t fb_hi = (sizeof(uintptr_t) > 4) ? (uint32_t)(fb_base >> 32) : 0;
            uint32_t fb_lo = (uint32_t)(fb_base & 0xFFFFFFFFu);
            serial_printf("[SYS] Firmware framebuffer: base=0x%08x%08x %ux%u depth=%u pitch=%u\n",
                          fb_hi, fb_lo,
                          fw_fb.width, fw_fb.height,
                          fw_fb.depth, fw_fb.pitch);
        } else {
            serial_puts("[SYS] Firmware framebuffer info unavailable\n");
        }
    }
#endif
#endif

    if (!framebuffer) {
        hal_framebuffer_info_t fb_info;
        if (hal_get_framebuffer_info(&fb_info) == 0) {
            framebuffer = fb_info.framebuffer;
            fb_width = fb_info.width;
            fb_height = fb_info.height;
            fb_pitch = fb_info.pitch;
            fb_bpp = (uint8_t)fb_info.depth;
            fb_red_pos = fb_info.red_offset;
            fb_red_size = fb_info.red_size;
            fb_green_pos = fb_info.green_offset;
            fb_green_size = fb_info.green_size;
            fb_blue_pos = fb_info.blue_offset;
            fb_blue_size = fb_info.blue_size;
        }
    }

    /* Framebuffer will be used by Finder if available */
    if (framebuffer) {
        SYSTEM_LOG_DEBUG("Framebuffer available for Finder desktop\n");
    } else {
        console_puts("No framebuffer available, continuing in text mode\n");
        serial_puts("No framebuffer available, continuing in text mode\n");
    }

    /* Initialize System 7.1 */
    init_system71();

    /* Remove early test - let DrawDesktop do all the drawing */
    if (framebuffer) {
        /* Create and open the desktop port */
        static GrafPort desktopPort;
        OpenPort(&desktopPort);

        DrawDesktop();
        hal_framebuffer_present();
    }

    /* Create windows and menus using real System 7.1 APIs */
#ifdef ENABLE_GESTALT
    /* Gestalt smoke test */
    {
        long value;
        OSErr err;

        serial_puts("\n=== Gestalt Smoke Test ===\n");

        /* Test system version */
        err = Gestalt(FOURCC('s','y','s','v'), &value);
        if (err == noErr) {
            serial_puts("[Gestalt] sysv = 0x");
            print_hex(value);
            serial_puts(" (System 7.1)\n");
        } else {
            serial_puts("[Gestalt] sysv query failed\n");
        }

        /* Test Time Manager version */
        err = Gestalt(FOURCC('q','t','i','m'), &value);
        if (err == noErr) {
            serial_puts("[Gestalt] qtim = 0x");
            print_hex(value);
            if (value > 0) {
                serial_puts(" (Time Manager present)\n");
            } else {
                serial_puts(" (Time Manager not initialized)\n");
            }
        }

        /* Test Resource Manager */
        if (Gestalt_Has(FOURCC('r','s','r','c'))) {
            Gestalt(FOURCC('r','s','r','c'), &value);
            serial_puts("[Gestalt] rsrc = 0x");
            print_hex(value);
            serial_puts(" (Resource Manager present)\n");
        }

        /* Test machine type */
        err = Gestalt(FOURCC('m','a','c','h'), &value);
        if (err == noErr) {
            serial_puts("[Gestalt] mach = 0x");
            print_hex(value);
            serial_puts(" (x86 machine)\n");
        }

        /* Test processor type */
        err = Gestalt(FOURCC('p','r','o','c'), &value);
        if (err == noErr) {
            serial_puts("[Gestalt] proc = 0x");
            print_hex(value);
            serial_puts(" (x86 processor)\n");
        }

        /* Test FPU */
        err = Gestalt(FOURCC('f','p','u',' '), &value);
        if (err == noErr) {
            serial_puts("[Gestalt] fpu  = ");
            print_hex(value);
            serial_puts(value ? " (FPU present)\n" : " (No FPU)\n");
        }

        /* Test init bits */
        err = Gestalt(FOURCC('i','n','i','t'), &value);
        if (err == noErr) {
            serial_puts("[Gestalt] init = 0x");
            print_hex(value);
            serial_puts(" (subsystem init bits)\n");
        }

        /* Test unknown selector */
        err = Gestalt(FOURCC('t','e','s','t'), &value);
        if (err == gestaltUnknownErr) {
            serial_puts("[Gestalt] 'test' correctly returned gestaltUnknownErr\n");
        }

        /* Test SysEnv */
        SysEnvRec env;
        err = GetSysEnv(1, &env);
        if (err == noErr) {
            serial_puts("[Gestalt] GetSysEnv: machine=");
            print_hex(env.machineType);
            serial_puts(" sysVers=0x");
            print_hex(env.systemVersion);
            serial_puts(" FPU=");
            print_hex(env.hasFPU);
            serial_puts(" MMU=");
            print_hex(env.hasMMU);
            serial_puts("\n");
        }

        serial_puts("=== Gestalt Test Complete ===\n\n");
    }
#endif

    /* Always run performance tests after initialization for debugging */
    #if 1  /* Enable performance tests */
    run_performance_tests();
    #endif

    /* Main event loop using real Event Manager (console_puts would corrupt framebuffer) */
    EventRecord event __attribute__((unused));

    /* Initial desktop draw now handled by Finder/Desktop Manager */
    serial_puts("MAIN: Desktop init complete\n");

    /* Draw the volume and trash icons */
    extern void DrawVolumeIcon(void);
    serial_puts("MAIN: About to call DrawVolumeIcon\n");
    DrawVolumeIcon();
    serial_puts("MAIN: DrawVolumeIcon returned\n");

    /* Run alert dialog smoke tests */
    #ifdef ALERT_SMOKE_TEST
    extern void InitAlertSmokeTest(void);
    InitAlertSmokeTest();
    #endif

    /* Run List Manager smoke tests */
    #ifdef LIST_SMOKE_TEST
    extern void RunListSmokeTest(void);
    serial_puts("MAIN: Running List Manager smoke tests\n");
    RunListSmokeTest();
    serial_puts("MAIN: List Manager smoke tests complete\n");
    #endif

    /* Run Speech Manager smoke tests */
    #ifdef SPEECH_SMOKE_TEST
    extern void RunSpeechSmokeTest(void);
    serial_puts("MAIN: Running Speech Manager smoke tests\n");
    RunSpeechSmokeTest();
    serial_puts("MAIN: Speech Manager smoke tests complete\n");
    #endif

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
    volatile uint32_t debug_counter __attribute__((unused)) = 0;

    /* Add cursor update counter to throttle cursor redraws */
    static uint32_t cursor_update_counter = 0;

    SYSTEM_LOG_DEBUG("MAIN: Entering main event loop NOW!\n");

    while (1) {
        /* IMPORTANT: Call TimerISR each iteration for high-cadence timer checking */
        TimeManager_TimerISR();  /* Poll timer (simulated ISR) - must be called each loop */
        TimeManager_DrainDeferred(16, 1000);  /* Process up to 16 tasks, max 1ms */

#ifdef ENABLE_PROCESS_COOP
        /* Cooperative yield point - let other processes run */
        static EventRecord evt;
        if (GetNextEvent(everyEvent, &evt)) {
            /* Got an event - dispatch it */
            SYSTEM_LOG_DEBUG("MAIN: GetNextEvent -> 1, what=%d at (%d,%d)\n",
                         evt.what, evt.where.h, evt.where.v);
            SYSTEM_LOG_DEBUG("MAIN: About to call DispatchEvent(&evt) where evt.what=%d\n", evt.what);
            DispatchEvent(&evt);
            SYSTEM_LOG_DEBUG("MAIN: DispatchEvent returned\n");

            /* Process any pending deferred window creation (About dialog, etc.)
             * This must happen AFTER DispatchEvent completes to avoid event re-entry deadlock */
            extern void AboutWindow_ProcessPendingCreation(void);
            AboutWindow_ProcessPendingCreation();

            MemoryManager_CheckSuspectBlock("after_dispatch(coop)");
        } else {
            /* No events - yield to other processes */
            Proc_Yield();
        }
#endif /* ENABLE_PROCESS_COOP */

        /* Simple alive message every 1 million iterations */
        static uint32_t simple_counter = 0;
        simple_counter++;
        if ((simple_counter % 1000000) == 0) {
            serial_puts(".");  /* Just print a dot to show we're alive */
            if ((simple_counter % 10000000) == 0) {
                SYSTEM_LOG_DEBUG("\nLOOP: counter=%u\n", simple_counter);
                simple_counter = 0;
            }
        }

        /* Process modern input events (PS/2 keyboard and mouse) */
        extern void ProcessModernInput(void);
        ProcessModernInput();

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
                SYSTEM_LOG_DEBUG("MAIN: Full redraw after %d movements\n", movement_count);
                /* TODO: Hook real desktop invalidation once available */
                movement_count = 0;
            }
        }
#endif /* Cursor redraw */

        /* Mouse button tracking moved to EventManager - events are properly dispatched now */

skip_cursor_drawing:
        /* Re-enable SystemTask and GetNextEvent for event processing */
#if 1
        if (framebuffer) {
            hal_framebuffer_present();
        }

        /* System 7.1 cooperative multitasking */
        SystemTask();

        /* Update and render windows */
        extern void WM_Update(void);
        WM_Update();

        /* PS/2 polling is now done inside ProcessModernInput() above.
         * Do NOT call PollPS2Input() here - it would consume events twice! */

        /* Process serial commands for menu testing */
#if DEBUG_SERIAL_MENU_COMMANDS
        process_serial_command();
#endif

        /* Get and process events via DispatchEvent */
#ifndef ENABLE_PROCESS_COOP
        /* Only do this when NOT using ProcessMgr (already handled above) */
        if (GetNextEvent(everyEvent, &event)) {
            /* Log event retrieval */
            SYSTEM_LOG_DEBUG("MAIN: GetNextEvent -> 1, what=%d at (%d,%d)\n",
                         event.what, event.where.h, event.where.v);
            /* Let DispatchEvent handle all events */
            DispatchEvent(&event);
            MemoryManager_CheckSuspectBlock("after_dispatch(main)");
        }
#endif

        /* Process deferred Time Manager tasks
         * IMPORTANT: Call TimerISR each iteration for high-cadence timer checking.
         * This simulates hardware timer interrupts in our freestanding environment.
         */
        TimeManager_DrainDeferred(16, 1000); /* up to 16 callbacks or 1ms of work */
        TimeManager_TimerISR(); /* Poll timer (simulated ISR) - must be called each loop */
#endif /* #if 1 */

        /* Yield CPU when no events - but don't halt, it blocks PS/2 polling! */
        /* __asm__ volatile ("hlt"); */
    }
}

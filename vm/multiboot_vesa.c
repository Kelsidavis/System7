/*
 * Mac OS System 7.1 Portable - VESA VBE 640x480 Graphics Kernel
 * Uses VESA BIOS Extensions for proper 640x480 resolution
 */

#include <stdint.h>
#include <stddef.h>

/* Multiboot header info */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_VBE 0x00000200

/* VESA VBE constants */
#define VBE_MODE_640x480x256 0x101  /* VESA mode for 640x480 256 colors */
#define VGA_WIDTH 640
#define VGA_HEIGHT 480

/* VGA text mode for fallback */
#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25
#define VGA_TEXT_ADDRESS 0xB8000

/* Mac OS 7.1 Colors */
#define COLOR_BLACK     0
#define COLOR_WHITE     15
#define COLOR_TEAL      3
#define COLOR_GRAY      7
#define COLOR_DARKGRAY  8
#define COLOR_MENUBAR   7

/* Multiboot info structure with VBE info - properly packed */
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

/* VBE mode info structure */
typedef struct {
    uint16_t attributes;
    uint8_t winA, winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segmentA, segmentB;
    uint32_t realFctPtr;
    uint16_t pitch;
    uint16_t Xres, Yres;
    uint8_t Wchar, Ychar, planes, bpp, banks;
    uint8_t memory_model, bank_size, image_pages;
    uint8_t reserved0;
    uint8_t red_mask, red_position;
    uint8_t green_mask, green_position;
    uint8_t blue_mask, blue_position;
    uint8_t rsv_mask, rsv_position;
    uint8_t directcolor_attributes;
    uint32_t physbase;
    uint32_t reserved1;
    uint16_t reserved2;
} __attribute__((packed)) vbe_mode_info_t;

/* Global framebuffer pointer */
static uint8_t* framebuffer = NULL;
static uint32_t fb_pitch = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint8_t fb_bpp = 0;

/* Serial port output for debugging */
static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Simple serial output */
static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);    /* Disable interrupts */
    outb(0x3F8 + 3, 0x80);    /* Enable DLAB */
    outb(0x3F8 + 0, 0x03);    /* Divisor low byte (38400 baud) */
    outb(0x3F8 + 1, 0x00);    /* Divisor high byte */
    outb(0x3F8 + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(0x3F8 + 2, 0xC7);    /* Enable FIFO */
}

static void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_puts(const char* str) {
    while (*str) {
        serial_putc(*str++);
    }
}

/* Text mode output for errors */
static void text_puts(const char* str, uint8_t color) {
    static int cursor = 0;
    uint16_t* video = (uint16_t*)VGA_TEXT_ADDRESS;

    while (*str) {
        if (*str == '\n') {
            cursor = ((cursor / VGA_TEXT_WIDTH) + 1) * VGA_TEXT_WIDTH;
        } else {
            video[cursor++] = (*str) | (color << 8);
        }
        str++;
    }
}

/* Set pixel using linear framebuffer */
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
    } else if (fb_bpp == 16) {
        *((uint16_t*)(framebuffer + pixel_offset)) =
            ((color & 0xF80000) >> 8) |  /* Red */
            ((color & 0x00FC00) >> 5) |  /* Green */
            ((color & 0x0000F8) >> 3);   /* Blue */
    } else if (fb_bpp == 8) {
        framebuffer[pixel_offset] = color & 0xFF;
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

/* Draw a simple Mac OS style window */
static void draw_window(int x, int y, int w, int h, const char* title) {
    /* Window background */
    fill_rect(x, y, w, h, 0xFFFFFF);  /* White */

    /* Title bar */
    fill_rect(x, y, w, 20, 0xC0C0C0);  /* Gray */

    /* Window border */
    for (int i = 0; i < w; i++) {
        set_pixel(x + i, y, 0);
        set_pixel(x + i, y + h - 1, 0);
    }
    for (int j = 0; j < h; j++) {
        set_pixel(x, y + j, 0);
        set_pixel(x + w - 1, y + j, 0);
    }

    /* Close button */
    fill_rect(x + 5, y + 5, 10, 10, 0xFF0000);
}

/* Draw Mac OS desktop */
static void draw_desktop(void) {
    /* For 32-bit color, use full RGB values */
    uint32_t teal = 0x0000CCCC;      /* Teal/cyan */
    uint32_t light_teal = 0x0000E6E6; /* Light teal */
    uint32_t white = 0x00FFFFFF;
    uint32_t black = 0x00000000;
    uint32_t gray = 0x00C0C0C0;
    uint32_t dark_gray = 0x00808080;
    uint32_t menu_gray = 0x00F0F0F0;

    /* Clear screen with teal desktop pattern */
    for (int y = 0; y < fb_height; y++) {
        for (int x = 0; x < fb_width; x++) {
            /* Create a subtle pattern */
            if (((x / 4) + (y / 4)) % 2 == 0) {
                set_pixel(x, y, teal);
            } else {
                set_pixel(x, y, light_teal);
            }
        }
    }

    /* Menu bar at top */
    fill_rect(0, 0, fb_width, 20, menu_gray);
    /* Menu bar bottom border */
    fill_rect(0, 20, fb_width, 1, black);

    /* Apple logo area (simplified black rectangle for now) */
    fill_rect(10, 4, 12, 12, black);

    /* Draw a test window to verify rendering */
    int win_x = 100;
    int win_y = 100;
    int win_w = 400;
    int win_h = 300;

    /* Window shadow */
    fill_rect(win_x + 3, win_y + 3, win_w, win_h, dark_gray);

    /* Window body */
    fill_rect(win_x, win_y, win_w, win_h, white);

    /* Title bar */
    fill_rect(win_x, win_y, win_w, 22, gray);

    /* Window border */
    /* Top */
    fill_rect(win_x, win_y, win_w, 1, black);
    /* Bottom */
    fill_rect(win_x, win_y + win_h - 1, win_w, 1, black);
    /* Left */
    fill_rect(win_x, win_y, 1, win_h, black);
    /* Right */
    fill_rect(win_x + win_w - 1, win_y, 1, win_h, black);

    /* Close box */
    fill_rect(win_x + 8, win_y + 6, 10, 10, white);
    fill_rect(win_x + 7, win_y + 5, 12, 12, black);
    fill_rect(win_x + 8, win_y + 6, 10, 10, menu_gray);

    /* Desktop icons area - trash at bottom right */
    int trash_x = fb_width - 60;
    int trash_y = fb_height - 60;
    fill_rect(trash_x, trash_y, 40, 40, dark_gray);
    /* Trash border */
    fill_rect(trash_x - 1, trash_y - 1, 42, 1, black);
    fill_rect(trash_x - 1, trash_y + 40, 42, 1, black);
    fill_rect(trash_x - 1, trash_y, 1, 40, black);
    fill_rect(trash_x + 40, trash_y, 1, 40, black);
}

/* Helper to convert number to string */
static void itoa(uint32_t value, char* str) {
    char temp[12];
    int i = 0;

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

/* Helper to convert number to hex string */
static void itox(uint32_t value, char* str) {
    const char* hex = "0123456789ABCDEF";
    char temp[9];
    int i = 0;

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    while (value > 0) {
        temp[i++] = hex[value & 0xF];
        value >>= 4;
    }

    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

/* Main kernel entry point */
void macos_vesa_main(uint32_t magic, multiboot_info_t* mbi) {
    /* Initialize serial for debugging */
    serial_init();
    serial_puts("Mac OS System 7.1 Portable - VESA Graphics Boot\n");

    /* Check multiboot magic */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        text_puts("Error: Invalid multiboot magic!\n", 0x0C);
        serial_puts("Error: Invalid multiboot magic!\n");
        while (1) { __asm__ volatile ("hlt"); }
    }

    serial_puts("Multiboot flags: 0x");
    char buf[12];
    itoa(mbi->flags, buf);
    serial_puts(buf);
    serial_puts("\n");

    /* Check for VBE/framebuffer info - bit 12 */
    if (mbi->flags & 0x00001000) {  /* Framebuffer info present */
        /* The framebuffer address is 64-bit, but in 32-bit mode we use the lower 32 bits */
        uint32_t fb_addr_low = (uint32_t)(mbi->framebuffer_addr & 0xFFFFFFFF);
        uint32_t fb_addr_high = (uint32_t)(mbi->framebuffer_addr >> 32);

        /* In 32-bit protected mode, we can only access the lower 4GB */
        /* GRUB typically maps the framebuffer in the lower 4GB for us */
        framebuffer = (uint8_t*)(uintptr_t)fb_addr_low;

        fb_width = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_pitch = mbi->framebuffer_pitch;
        fb_bpp = mbi->framebuffer_bpp;

        serial_puts("Framebuffer detected:\n");
        serial_puts("  Address: 0x");
        itox(fb_addr_low, buf);
        serial_puts(buf);
        if (fb_addr_high != 0) {
            serial_puts(" (high: 0x");
            itox(fb_addr_high, buf);
            serial_puts(buf);
            serial_puts(")");
        }
        serial_puts("\n  Resolution: ");
        itoa(fb_width, buf);
        serial_puts(buf);
        serial_puts("x");
        itoa(fb_height, buf);
        serial_puts(buf);
        serial_puts("x");
        itoa(fb_bpp, buf);
        serial_puts(buf);
        serial_puts("\n  Pitch: ");
        itoa(fb_pitch, buf);
        serial_puts(buf);
        serial_puts(" bytes\n");

        /* Validate framebuffer parameters */
        if (fb_width == 0 || fb_height == 0 || fb_bpp == 0 || framebuffer == NULL) {
            serial_puts("Error: Invalid framebuffer parameters!\n");
            text_puts("Error: Invalid framebuffer parameters!\n", 0x0C);
            while (1) { __asm__ volatile ("hlt"); }
        }

        /* Draw the Mac OS desktop */
        serial_puts("Drawing Mac OS desktop...\n");
        draw_desktop();

        serial_puts("Mac OS 7.1 desktop rendered successfully!\n");
    } else {
        /* Fallback to text mode */
        text_puts("Mac OS System 7.1 Portable\n", 0x0F);
        text_puts("Error: No framebuffer available!\n", 0x0C);
        text_puts("Please enable VBE in bootloader.\n", 0x07);
        serial_puts("Error: No framebuffer info in multiboot!\n");
        serial_puts("Multiboot flags: 0x");
        itoa(mbi->flags, buf);
        serial_puts(buf);
        serial_puts("\n");
    }

    /* Halt */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
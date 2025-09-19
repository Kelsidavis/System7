/*
 * Mac OS System 7.1 Portable - SVGA 640x480x256 Graphics Kernel
 * Uses VGA mode 0x101 (640x480x256) via direct register programming
 */

#include <stdint.h>
#include <stddef.h>

/* Multiboot header info */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* SVGA constants */
#define SVGA_WIDTH 640
#define SVGA_HEIGHT 480
#define VGA_ADDRESS 0xA0000
#define VGA_BANK_SIZE 0x10000  /* 64KB window */

/* VGA ports */
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA 0x3C9
#define VGA_MISC_WRITE 0x3C2
#define VGA_MISC_READ 0x3CC
#define VGA_SEQUENCER_INDEX 0x3C4
#define VGA_SEQUENCER_DATA 0x3C5
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_GRAPHICS_INDEX 0x3CE
#define VGA_GRAPHICS_DATA 0x3CF
#define VGA_ATTRIBUTE_INDEX 0x3C0
#define VGA_ATTRIBUTE_DATA 0x3C1
#define VGA_INSTAT_READ 0x3DA

/* Mac OS 7.1 Colors (palette indices) */
#define COLOR_BLACK     0
#define COLOR_WHITE     255
#define COLOR_TEAL      3
#define COLOR_GRAY      7
#define COLOR_DARKGRAY  8
#define COLOR_MENUBAR   248

/* Multiboot info structure */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_t;

/* Current VGA bank */
static uint8_t current_bank = 0;

/* I/O port functions */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/* Serial port debugging */
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
    while (*str) {
        serial_putc(*str++);
    }
}

/* Bank switching for SVGA */
static void set_bank(uint8_t bank) {
    if (bank != current_bank) {
        /* Use VGA sequencer register to switch banks */
        outb(VGA_SEQUENCER_INDEX, 0x06);
        outb(VGA_SEQUENCER_DATA, bank);
        current_bank = bank;
    }
}

/* Initialize SVGA mode 640x480x256 */
static void init_svga_mode(void) {
    /* Disable interrupts during mode switch */
    __asm__ volatile ("cli");

    /* Reset sequencer */
    outb(VGA_SEQUENCER_INDEX, 0x00);
    outb(VGA_SEQUENCER_DATA, 0x01);

    /* Set up timing for 640x480 */
    outb(VGA_MISC_WRITE, 0xE3);  /* 25.175 MHz pixel clock, hsync-, vsync- */

    /* Sequencer registers */
    outb(VGA_SEQUENCER_INDEX, 0x00); outb(VGA_SEQUENCER_DATA, 0x03);  /* Reset */
    outb(VGA_SEQUENCER_INDEX, 0x01); outb(VGA_SEQUENCER_DATA, 0x01);  /* 8-dot clock */
    outb(VGA_SEQUENCER_INDEX, 0x02); outb(VGA_SEQUENCER_DATA, 0x0F);  /* Write all planes */
    outb(VGA_SEQUENCER_INDEX, 0x03); outb(VGA_SEQUENCER_DATA, 0x00);  /* No char map */
    outb(VGA_SEQUENCER_INDEX, 0x04); outb(VGA_SEQUENCER_DATA, 0x0E);  /* Chain-4 mode */

    /* Unlock CRTC registers */
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & 0x7F);

    /* CRTC registers for 640x480 @ 60Hz */
    static const uint8_t crtc_regs[] = {
        0x5F, 0x4F, 0x50, 0x82,  /* h total, h display end, h blank start, h blank end */
        0x54, 0x80, 0x0B, 0x3E,  /* h sync start, h sync end, vert total, overflow */
        0x00, 0x40, 0x00, 0x00,  /* preset row scan, max scan line, cursor start, cursor end */
        0x00, 0x00, 0x00, 0x00,  /* start addr high, start addr low, cursor loc high, cursor loc low */
        0xEA, 0x8C, 0xDF, 0x28,  /* v sync start, v sync end, v display end, offset */
        0x00, 0xE7, 0x04, 0xE3   /* underline loc, v blank start, v blank end, mode control */
    };

    for (int i = 0; i < sizeof(crtc_regs); i++) {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, crtc_regs[i]);
    }

    /* Graphics controller registers */
    outb(VGA_GRAPHICS_INDEX, 0x00); outb(VGA_GRAPHICS_DATA, 0x00);  /* Set/Reset */
    outb(VGA_GRAPHICS_INDEX, 0x01); outb(VGA_GRAPHICS_DATA, 0x00);  /* Enable Set/Reset */
    outb(VGA_GRAPHICS_INDEX, 0x02); outb(VGA_GRAPHICS_DATA, 0x00);  /* Color Compare */
    outb(VGA_GRAPHICS_INDEX, 0x03); outb(VGA_GRAPHICS_DATA, 0x00);  /* Data Rotate */
    outb(VGA_GRAPHICS_INDEX, 0x04); outb(VGA_GRAPHICS_DATA, 0x00);  /* Read Map Select */
    outb(VGA_GRAPHICS_INDEX, 0x05); outb(VGA_GRAPHICS_DATA, 0x40);  /* Mode: 256 color */
    outb(VGA_GRAPHICS_INDEX, 0x06); outb(VGA_GRAPHICS_DATA, 0x05);  /* Memory Map: A000-BFFF */
    outb(VGA_GRAPHICS_INDEX, 0x07); outb(VGA_GRAPHICS_DATA, 0x0F);  /* Color Don't Care */
    outb(VGA_GRAPHICS_INDEX, 0x08); outb(VGA_GRAPHICS_DATA, 0xFF);  /* Bit Mask */

    /* Attribute controller registers */
    inb(VGA_INSTAT_READ);  /* Reset flip-flop */
    for (int i = 0; i < 16; i++) {
        outb(VGA_ATTRIBUTE_INDEX, i);
        outb(VGA_ATTRIBUTE_INDEX, i);
    }
    outb(VGA_ATTRIBUTE_INDEX, 0x10); outb(VGA_ATTRIBUTE_INDEX, 0x41);  /* Graphics mode */
    outb(VGA_ATTRIBUTE_INDEX, 0x11); outb(VGA_ATTRIBUTE_INDEX, 0x00);  /* Overscan */
    outb(VGA_ATTRIBUTE_INDEX, 0x12); outb(VGA_ATTRIBUTE_INDEX, 0x0F);  /* Color plane enable */
    outb(VGA_ATTRIBUTE_INDEX, 0x13); outb(VGA_ATTRIBUTE_INDEX, 0x00);  /* Horiz pan */
    outb(VGA_ATTRIBUTE_INDEX, 0x14); outb(VGA_ATTRIBUTE_INDEX, 0x00);  /* Color select */

    /* Enable display */
    inb(VGA_INSTAT_READ);
    outb(VGA_ATTRIBUTE_INDEX, 0x20);

    /* Clear screen */
    uint8_t* vga = (uint8_t*)VGA_ADDRESS;
    for (int bank = 0; bank < 8; bank++) {
        set_bank(bank);
        for (int i = 0; i < VGA_BANK_SIZE; i++) {
            vga[i] = 0;
        }
    }
    set_bank(0);

    /* Re-enable interrupts */
    __asm__ volatile ("sti");
}

/* Set palette color */
static void set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    outb(VGA_DAC_WRITE_INDEX, index);
    outb(VGA_DAC_DATA, r >> 2);  /* VGA uses 6-bit color values */
    outb(VGA_DAC_DATA, g >> 2);
    outb(VGA_DAC_DATA, b >> 2);
}

/* Set pixel in SVGA mode with bank switching */
static void set_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= SVGA_WIDTH || y < 0 || y >= SVGA_HEIGHT) {
        return;
    }

    uint32_t offset = y * SVGA_WIDTH + x;
    uint8_t bank = offset / VGA_BANK_SIZE;
    uint16_t bank_offset = offset % VGA_BANK_SIZE;

    set_bank(bank);
    uint8_t* vga = (uint8_t*)VGA_ADDRESS;
    vga[bank_offset] = color;
}

/* Fill rectangle */
static void fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int j = y; j < y + h && j < SVGA_HEIGHT; j++) {
        for (int i = x; i < x + w && i < SVGA_WIDTH; i++) {
            set_pixel(i, j, color);
        }
    }
}

/* Draw frame */
static void draw_frame(int x, int y, int w, int h, uint8_t color) {
    /* Top and bottom */
    for (int i = 0; i < w; i++) {
        set_pixel(x + i, y, color);
        set_pixel(x + i, y + h - 1, color);
    }
    /* Left and right */
    for (int j = 0; j < h; j++) {
        set_pixel(x, y + j, color);
        set_pixel(x + w - 1, y + j, color);
    }
}

/* Draw a simple Mac OS style window */
static void draw_window(int x, int y, int w, int h, const char* title) {
    /* Window shadow */
    fill_rect(x + 2, y + 2, w, h, COLOR_DARKGRAY);

    /* Window background */
    fill_rect(x, y, w, h, COLOR_WHITE);

    /* Title bar */
    fill_rect(x, y, w, 20, COLOR_MENUBAR);

    /* Window border */
    draw_frame(x, y, w, h, COLOR_BLACK);

    /* Title bar stripes (classic Mac OS look) */
    for (int i = 2; i < 18; i += 2) {
        for (int j = 1; j < w - 1; j += 4) {
            set_pixel(x + j, y + i, COLOR_GRAY);
            set_pixel(x + j + 1, y + i, COLOR_GRAY);
        }
    }

    /* Close box */
    draw_frame(x + 8, y + 4, 12, 12, COLOR_BLACK);
    fill_rect(x + 9, y + 5, 10, 10, COLOR_WHITE);
}

/* Draw Mac OS desktop */
static void draw_desktop(void) {
    /* Fill background with Mac OS teal pattern */
    for (int y = 0; y < SVGA_HEIGHT; y++) {
        for (int x = 0; x < SVGA_WIDTH; x++) {
            /* Create checkerboard pattern */
            if (((x / 2) + (y / 2)) % 2 == 0) {
                set_pixel(x, y, COLOR_TEAL);
            } else {
                set_pixel(x, y, COLOR_TEAL + 1);  /* Lighter teal */
            }
        }
    }

    /* Menu bar */
    fill_rect(0, 0, SVGA_WIDTH, 20, COLOR_MENUBAR);
    draw_frame(0, 0, SVGA_WIDTH, 20, COLOR_BLACK);

    /* Apple logo (simplified) */
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < 10; x++) {
            if ((x > 1 && x < 8) && (y > 1 && y < 10)) {
                set_pixel(10 + x, 4 + y, COLOR_BLACK);
            }
        }
    }

    /* Draw windows */
    draw_window(50, 60, 320, 200, "System 7.1");
    draw_window(150, 140, 300, 180, "Welcome");
    draw_window(380, 240, 240, 160, "About This Mac");

    /* Trash can at bottom right */
    int trash_x = SVGA_WIDTH - 60;
    int trash_y = SVGA_HEIGHT - 60;
    fill_rect(trash_x + 2, trash_y + 2, 40, 40, COLOR_DARKGRAY);  /* Shadow */
    fill_rect(trash_x, trash_y, 40, 40, COLOR_GRAY);
    draw_frame(trash_x, trash_y, 40, 40, COLOR_BLACK);
}

/* Main kernel entry point */
void macos_svga_main(uint32_t magic, multiboot_info_t* mbi) {
    /* Initialize serial for debugging */
    serial_init();
    serial_puts("Mac OS System 7.1 Portable - SVGA 640x480 Boot\n");

    /* Check multiboot magic */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_puts("Error: Invalid multiboot magic!\n");
        while (1) { __asm__ volatile ("hlt"); }
    }

    serial_puts("Initializing SVGA mode 640x480x256...\n");

    /* Initialize SVGA mode */
    init_svga_mode();

    /* Set up Mac OS color palette */
    set_palette(COLOR_BLACK, 0, 0, 0);
    set_palette(COLOR_WHITE, 255, 255, 255);
    set_palette(COLOR_TEAL, 0, 204, 204);
    set_palette(COLOR_TEAL + 1, 51, 221, 221);  /* Lighter teal */
    set_palette(COLOR_GRAY, 192, 192, 192);
    set_palette(COLOR_DARKGRAY, 128, 128, 128);
    set_palette(COLOR_MENUBAR, 240, 240, 240);

    serial_puts("Drawing Mac OS desktop...\n");

    /* Draw the desktop */
    draw_desktop();

    serial_puts("Mac OS 7.1 SVGA desktop loaded successfully!\n");

    /* Halt */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
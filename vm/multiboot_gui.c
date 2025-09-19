/*
 * Mac OS System 7.1 Portable - VGA Graphics Multiboot Kernel
 * This implements proper VGA mode 13h graphics with Mac OS desktop
 */

#include <stdint.h>
#include <stddef.h>

/* Multiboot header info */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* VGA constants */
#define VGA_WIDTH 320
#define VGA_HEIGHT 200
#define VGA_ADDRESS 0xA0000

/* VGA ports */
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA 0x3C9
#define VGA_MISC_WRITE 0x3C2
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
#define COLOR_WHITE     15
#define COLOR_TEAL      248  /* Mac OS desktop teal */
#define COLOR_GRAY      7
#define COLOR_DARKGRAY  8
#define COLOR_MENUBAR   249  /* Light gray for menu bar */

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

/* Apple logo bitmap (16x14 pixels, 1-bit) */
static const uint8_t apple_logo[28] = {
    0x00, 0x00,  /* Row 0:  ................ */
    0x03, 0x80,  /* Row 1:  ......###....... */
    0x03, 0x80,  /* Row 2:  ......###....... */
    0x07, 0xC0,  /* Row 3:  .....#####...... */
    0x0F, 0xE0,  /* Row 4:  ....#######..... */
    0x1F, 0xF0,  /* Row 5:  ...#########.... */
    0x3F, 0xF8,  /* Row 6:  ..###########... */
    0x7F, 0xFC,  /* Row 7:  .#############.. */
    0xFF, 0xFE,  /* Row 8:  ###############. */
    0xFF, 0xFE,  /* Row 9:  ###############. */
    0x7F, 0xFC,  /* Row 10: .#############.. */
    0x3F, 0xF8,  /* Row 11: ..###########... */
    0x1F, 0xF0,  /* Row 12: ...#########.... */
    0x00, 0x00   /* Row 13: ................ */
};

/* Inline assembly helpers */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Serial output for debugging */
static void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_puts(const char* str) {
    while (*str) {
        if (*str == '\n') serial_putc('\r');
        serial_putc(*str++);
    }
}

/* Set VGA palette entry */
static void set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    outb(VGA_DAC_WRITE_INDEX, index);
    outb(VGA_DAC_DATA, r >> 2);  /* VGA uses 6-bit color values */
    outb(VGA_DAC_DATA, g >> 2);
    outb(VGA_DAC_DATA, b >> 2);
}

/* Initialize VGA mode 0x13 (320x200x256) */
static void init_vga_mode13(void) {
    /* Write VGA registers to set mode 13h */

    /* First disable the display */
    inb(VGA_INSTAT_READ);
    outb(VGA_ATTRIBUTE_INDEX, 0x00);

    /* Set sequencer registers */
    outb(VGA_SEQUENCER_INDEX, 0x00); outb(VGA_SEQUENCER_DATA, 0x03);  /* Reset */
    outb(VGA_SEQUENCER_INDEX, 0x01); outb(VGA_SEQUENCER_DATA, 0x01);  /* Clocking mode */
    outb(VGA_SEQUENCER_INDEX, 0x02); outb(VGA_SEQUENCER_DATA, 0x0F);  /* Map mask */
    outb(VGA_SEQUENCER_INDEX, 0x03); outb(VGA_SEQUENCER_DATA, 0x00);  /* Character map */
    outb(VGA_SEQUENCER_INDEX, 0x04); outb(VGA_SEQUENCER_DATA, 0x0E);  /* Memory mode */

    /* Misc output register - 320x200 256 color */
    outb(VGA_MISC_WRITE, 0x63);

    /* Unlock CRTC registers */
    outb(VGA_CRTC_INDEX, 0x11);
    uint8_t val = inb(VGA_CRTC_DATA) & 0x7F;
    outb(VGA_CRTC_DATA, val);

    /* Set CRTC registers for 320x200 */
    outb(VGA_CRTC_INDEX, 0x00); outb(VGA_CRTC_DATA, 0x5F);  /* Horizontal total */
    outb(VGA_CRTC_INDEX, 0x01); outb(VGA_CRTC_DATA, 0x4F);  /* Horizontal display end */
    outb(VGA_CRTC_INDEX, 0x02); outb(VGA_CRTC_DATA, 0x50);  /* Start horizontal blanking */
    outb(VGA_CRTC_INDEX, 0x03); outb(VGA_CRTC_DATA, 0x82);  /* End horizontal blanking */
    outb(VGA_CRTC_INDEX, 0x04); outb(VGA_CRTC_DATA, 0x54);  /* Start horizontal retrace */
    outb(VGA_CRTC_INDEX, 0x05); outb(VGA_CRTC_DATA, 0x80);  /* End horizontal retrace */
    outb(VGA_CRTC_INDEX, 0x06); outb(VGA_CRTC_DATA, 0xBF);  /* Vertical total */
    outb(VGA_CRTC_INDEX, 0x07); outb(VGA_CRTC_DATA, 0x1F);  /* Overflow */
    outb(VGA_CRTC_INDEX, 0x08); outb(VGA_CRTC_DATA, 0x00);  /* Preset row scan */
    outb(VGA_CRTC_INDEX, 0x09); outb(VGA_CRTC_DATA, 0x41);  /* Maximum scan line */
    outb(VGA_CRTC_INDEX, 0x10); outb(VGA_CRTC_DATA, 0x9C);  /* Vertical retrace start */
    outb(VGA_CRTC_INDEX, 0x11); outb(VGA_CRTC_DATA, 0x8E);  /* Vertical retrace end */
    outb(VGA_CRTC_INDEX, 0x12); outb(VGA_CRTC_DATA, 0x8F);  /* Vertical display end */
    outb(VGA_CRTC_INDEX, 0x13); outb(VGA_CRTC_DATA, 0x28);  /* Offset */
    outb(VGA_CRTC_INDEX, 0x14); outb(VGA_CRTC_DATA, 0x40);  /* Underline location */
    outb(VGA_CRTC_INDEX, 0x15); outb(VGA_CRTC_DATA, 0x96);  /* Start vertical blanking */
    outb(VGA_CRTC_INDEX, 0x16); outb(VGA_CRTC_DATA, 0xB9);  /* End vertical blanking */
    outb(VGA_CRTC_INDEX, 0x17); outb(VGA_CRTC_DATA, 0xA3);  /* CRTC mode control */

    /* Set graphics controller registers */
    outb(VGA_GRAPHICS_INDEX, 0x00); outb(VGA_GRAPHICS_DATA, 0x00);  /* Set/reset */
    outb(VGA_GRAPHICS_INDEX, 0x01); outb(VGA_GRAPHICS_DATA, 0x00);  /* Enable set/reset */
    outb(VGA_GRAPHICS_INDEX, 0x02); outb(VGA_GRAPHICS_DATA, 0x00);  /* Color compare */
    outb(VGA_GRAPHICS_INDEX, 0x03); outb(VGA_GRAPHICS_DATA, 0x00);  /* Data rotate */
    outb(VGA_GRAPHICS_INDEX, 0x04); outb(VGA_GRAPHICS_DATA, 0x00);  /* Read map select */
    outb(VGA_GRAPHICS_INDEX, 0x05); outb(VGA_GRAPHICS_DATA, 0x40);  /* Graphics mode */
    outb(VGA_GRAPHICS_INDEX, 0x06); outb(VGA_GRAPHICS_DATA, 0x05);  /* Miscellaneous */
    outb(VGA_GRAPHICS_INDEX, 0x07); outb(VGA_GRAPHICS_DATA, 0x0F);  /* Color don't care */
    outb(VGA_GRAPHICS_INDEX, 0x08); outb(VGA_GRAPHICS_DATA, 0xFF);  /* Bit mask */

    /* Set attribute controller registers */
    inb(VGA_INSTAT_READ);
    for (int i = 0; i < 16; i++) {
        outb(VGA_ATTRIBUTE_INDEX, i);
        outb(VGA_ATTRIBUTE_INDEX, i);
    }
    outb(VGA_ATTRIBUTE_INDEX, 0x10); outb(VGA_ATTRIBUTE_INDEX, 0x41);  /* Mode control */
    outb(VGA_ATTRIBUTE_INDEX, 0x11); outb(VGA_ATTRIBUTE_INDEX, 0x00);  /* Overscan color */
    outb(VGA_ATTRIBUTE_INDEX, 0x12); outb(VGA_ATTRIBUTE_INDEX, 0x0F);  /* Color plane enable */
    outb(VGA_ATTRIBUTE_INDEX, 0x13); outb(VGA_ATTRIBUTE_INDEX, 0x00);  /* Horizontal pixel panning */
    outb(VGA_ATTRIBUTE_INDEX, 0x14); outb(VGA_ATTRIBUTE_INDEX, 0x00);  /* Color select */

    /* Re-enable display */
    inb(VGA_INSTAT_READ);
    outb(VGA_ATTRIBUTE_INDEX, 0x20);

    /* Clear the screen */
    uint8_t* vga = (uint8_t*)VGA_ADDRESS;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = 0;
    }

    /* Set up Mac OS color palette */
    set_palette(COLOR_BLACK, 0, 0, 0);
    set_palette(COLOR_WHITE, 255, 255, 255);
    set_palette(COLOR_GRAY, 192, 192, 192);
    set_palette(COLOR_DARKGRAY, 128, 128, 128);
    set_palette(COLOR_TEAL, 0, 204, 204);       /* Mac OS desktop teal */
    set_palette(COLOR_MENUBAR, 240, 240, 240);   /* Light gray for menu */
}

/* Set pixel in VGA mode 13h */
static void set_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        uint8_t* vga = (uint8_t*)VGA_ADDRESS;
        vga[y * VGA_WIDTH + x] = color;
    }
}

/* Fill rectangle */
static void fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int j = y; j < y + h && j < VGA_HEIGHT; j++) {
        for (int i = x; i < x + w && i < VGA_WIDTH; i++) {
            set_pixel(i, j, color);
        }
    }
}

/* Draw rectangle outline */
static void draw_rect(int x, int y, int w, int h, uint8_t color) {
    for (int i = x; i < x + w && i < VGA_WIDTH; i++) {
        set_pixel(i, y, color);
        set_pixel(i, y + h - 1, color);
    }
    for (int j = y; j < y + h && j < VGA_HEIGHT; j++) {
        set_pixel(x, j, color);
        set_pixel(x + w - 1, j, color);
    }
}

/* Draw 1-bit bitmap */
static void draw_bitmap_1bpp(int x, int y, int width, int height,
                            const uint8_t* data, uint8_t fg_color, uint8_t bg_color) {
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int byte_idx = row * ((width + 7) / 8) + (col / 8);
            int bit_idx = 7 - (col % 8);
            uint8_t bit = (data[byte_idx] >> bit_idx) & 1;

            if (bit) {
                set_pixel(x + col, y + row, fg_color);
            } else if (bg_color != 255) { /* 255 = transparent */
                set_pixel(x + col, y + row, bg_color);
            }
        }
    }
}

/* Draw simple text (5x7 font placeholder) */
static void draw_text(const char* str, int x, int y, uint8_t color) {
    /* Simplified - just draw rectangles for text */
    int pos = x;
    while (*str) {
        if (*str != ' ') {
            fill_rect(pos, y, 4, 7, color);
        }
        pos += 6;
        str++;
    }
}

/* Draw Mac OS menubar */
static void draw_menubar(void) {
    /* Menu bar background */
    fill_rect(0, 0, VGA_WIDTH, 20, COLOR_MENUBAR);
    draw_rect(0, 0, VGA_WIDTH, 20, COLOR_BLACK);

    /* Draw Apple logo */
    draw_bitmap_1bpp(5, 3, 16, 14, apple_logo, COLOR_BLACK, 255);

    /* Draw menu items */
    draw_text("File", 30, 6, COLOR_BLACK);
    draw_text("Edit", 70, 6, COLOR_BLACK);
    draw_text("View", 110, 6, COLOR_BLACK);
    draw_text("Special", 150, 6, COLOR_BLACK);
    draw_text("Help", 210, 6, COLOR_BLACK);
}

/* Draw Mac window */
static void draw_window(int x, int y, int w, int h, const char* title) {
    /* Window shadow */
    fill_rect(x + 2, y + 2, w, h, COLOR_DARKGRAY);

    /* Window body */
    fill_rect(x, y, w, h, COLOR_WHITE);
    draw_rect(x, y, w, h, COLOR_BLACK);

    /* Title bar */
    fill_rect(x, y, w, 18, COLOR_GRAY);
    draw_rect(x, y, w, 18, COLOR_BLACK);

    /* Close box */
    draw_rect(x + 4, y + 4, 10, 10, COLOR_BLACK);

    /* Title text */
    draw_text(title, x + w/2 - 30, y + 5, COLOR_BLACK);
}

/* Draw folder icon */
static void draw_folder(int x, int y, const char* label) {
    /* Folder tab */
    fill_rect(x, y, 12, 5, COLOR_WHITE);
    draw_rect(x, y, 12, 5, COLOR_BLACK);

    /* Folder body */
    fill_rect(x, y + 4, 32, 20, COLOR_WHITE);
    draw_rect(x, y + 4, 32, 20, COLOR_BLACK);

    /* Label */
    draw_text(label, x - 10, y + 28, COLOR_BLACK);
}

/* Draw desktop */
static void draw_desktop(void) {
    /* Clear to Mac OS teal */
    fill_rect(0, 0, VGA_WIDTH, VGA_HEIGHT, COLOR_TEAL);

    /* Draw menu bar */
    draw_menubar();

    /* Draw a window */
    draw_window(50, 40, 200, 120, "Macintosh HD");

    /* Draw desktop icons */
    draw_folder(20, 30, "System");
    draw_folder(70, 30, "Apps");
    draw_folder(120, 30, "Docs");

    /* Draw trash at bottom right */
    draw_folder(VGA_WIDTH - 50, VGA_HEIGHT - 40, "Trash");
}

/* Main kernel entry point */
void macos_gui_main(uint32_t magic, multiboot_info_t* mbi) {
    /* Initialize serial for debug */
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);

    serial_puts("Mac OS System 7.1 Portable - VGA Graphics Boot\n");

    /* Check multiboot magic */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_puts("Error: Not booted by multiboot\n");
        return;
    }

    serial_puts("Initializing VGA mode 13h...\n");

    /* Initialize VGA */
    init_vga_mode13();

    serial_puts("Drawing Mac OS desktop...\n");

    /* Draw the desktop */
    draw_desktop();

    serial_puts("Mac OS 7.1 GUI loaded successfully!\n");

    /* Halt */
    while (1) {
        __asm__ volatile("hlt");
    }
}
/*
 * MenuAppIcon.c - Draw Application (top-right) menu icon
 * Uses 1-bit 16x16 glyph from system7_resources.h
 */
#include "MenuManager/MenuAppIcon.h"
#include "QuickDraw/QuickDraw.h"
#include "Resources/system7_resources.h"
#include "Finder/Icon/icon_types.h"
#include "Finder/Icon/icon_port.h"
#include "Resources/icons_generated.h"

#define MENU_APP_ICON_WIDTH  20
#define MENU_APP_ICON_HEIGHT 18

/* 16x16 1-bit Finder-like menu glyph (mono), two bytes per row MSB first */
static const unsigned char gFinderMenuIcon16[32] = {
    0x00, 0x00, /* row 0 */
    0x3F, 0xFC, /* row 1: outer top */
    0x20, 0x04, /* row 2: outer sides */
    0x2F, 0xF4, /* row 3: inner screen top */
    0x2D, 0x54, /* row 4: dots pattern A */
    0x2A, 0xB4, /* row 5: dots pattern B */
    0x2D, 0x54, /* row 6 */
    0x2A, 0xB4, /* row 7 */
    0x2D, 0x54, /* row 8 */
    0x2F, 0xF4, /* row 9: inner screen bottom */
    0x20, 0x04, /* row10: outer sides */
    0x20, 0x04, /* row11: outer sides */
    0x3F, 0xFC, /* row12: outer bottom */
    0x00, 0x00, /* row13 */
    0x03, 0xC0, /* row14: base */
    0x00, 0x00  /* row15 */
};

extern UInt32 pack_color(uint8_t r, uint8_t g, uint8_t b);

/* application_icon_16 is 16x16, 1-bit, 2 bytes per row */
short MenuAppIcon_Draw(GrafPtr port, short left, short top, Boolean highlighted)
{
    if (!port || !port->portBits.baseAddr) {
        return MENU_APP_ICON_WIDTH;
    }

    const SInt16 rowBytes = port->portBits.rowBytes & 0x3FFF;
    if (rowBytes <= 0) return MENU_APP_ICON_WIDTH;

    const SInt16 width = port->portBits.bounds.right - port->portBits.bounds.left;
    const SInt16 height = port->portBits.bounds.bottom - port->portBits.bounds.top;
    if (width <= 0 || height <= 0) return MENU_APP_ICON_WIDTH;

    static IconFamily sFinderIcon;
    static Boolean sFinderLoaded = false;
    static Boolean sFinderAvailable = false;

    if (!sFinderLoaded) {
        sFinderAvailable = IconGen_FindByID(12500, &sFinderIcon);
        sFinderLoaded = true;
    }

    const int iconLeft = left + (MENU_APP_ICON_WIDTH - 16) / 2;
    const int iconTop  = top  + (MENU_APP_ICON_HEIGHT - 16) / 2;

    if (sFinderAvailable) {
        GrafPtr prev;
        GetPort(&prev);
        SetPort(port);

        const IconBitmap *bmp = &sFinderIcon.large;
        if (bmp->depth == kIconColor32 && bmp->argb32 && bmp->w >= 32 && bmp->h >= 32) {
            for (int y = 0; y < 16; y++) {
                int srcY = y * 2;
                for (int x = 0; x < 16; x++) {
                    int srcX = x * 2;
                    uint32_t pixel = bmp->argb32[srcY * bmp->w + srcX];
                    uint8_t alpha = (pixel >> 24) & 0xFF;
                    if (alpha == 0) continue;
                    if (highlighted) {
                        uint8_t r = (pixel >> 16) & 0xFF;
                        uint8_t g = (pixel >> 8) & 0xFF;
                        uint8_t b = pixel & 0xFF;
                        r = (r + 0x00) / 2;
                        g = (g + 0x00) / 2;
                        b = (b + 0x80) / 2;
                        pixel = (alpha << 24) | (r << 16) | (g << 8) | b;
                    }
                    IconPort_WritePixel(iconLeft + x, iconTop + y, pixel);
                }
            }
        } else if (bmp->img1b && bmp->mask1b && bmp->w >= 32 && bmp->h >= 32) {
            for (int y = 0; y < 16; y++) {
                int srcY = y * 2;
                const uint8_t *maskRow = bmp->mask1b + (srcY * 4);
                const uint8_t *imageRow = bmp->img1b + (srcY * 4);
                for (int x = 0; x < 16; x++) {
                    int srcX = x * 2;
                    int byteIndex = srcX >> 3;
                    int bit = 7 - (srcX & 7);
                    if (((maskRow[byteIndex] >> bit) & 1) == 0) {
                        continue;
                    }
                    bool black = ((imageRow[byteIndex] >> bit) & 1) != 0;
                    uint32_t color = black ? pack_color(0,0,0) : pack_color(255,255,255);
                    if (highlighted) {
                        uint8_t r = (color >> 16) & 0xFF;
                        uint8_t g = (color >> 8) & 0xFF;
                        uint8_t b = color & 0xFF;
                        r = (r + 0x00) / 2;
                        g = (g + 0x00) / 2;
                        b = (b + 0x80) / 2;
                        color = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                    IconPort_WritePixel(iconLeft + x, iconTop + y, color);
                }
            }
        }

        SetPort(prev);
        return MENU_APP_ICON_WIDTH;
    }

    UInt32* pixels = (UInt32*)port->portBits.baseAddr;
    const int stride = rowBytes / 4; /* 32-bit pixels */

    const UInt32 onColor  = highlighted ? pack_color(255, 255, 255) : pack_color(0, 0, 0);

    for (int y = 0; y < 16; y++) {
        int dstY = iconTop + y;
        if (dstY < port->portBits.bounds.top || dstY >= port->portBits.bounds.bottom) continue;

        /* two bytes per row, MSB first */
        unsigned char b0 = gFinderMenuIcon16[y * 2 + 0];
        unsigned char b1 = gFinderMenuIcon16[y * 2 + 1];
        unsigned short rowBits = (unsigned short)((b0 << 8) | b1);

        for (int x = 0; x < 16; x++) {
            if (rowBits & (1u << (15 - x))) {
                int dstX = iconLeft + x;
                if (dstX < port->portBits.bounds.left || dstX >= port->portBits.bounds.right) continue;
                pixels[(dstY - port->portBits.bounds.top) * stride +
                       (dstX - port->portBits.bounds.left)] = onColor;
            }
        }
    }

    return MENU_APP_ICON_WIDTH;
}

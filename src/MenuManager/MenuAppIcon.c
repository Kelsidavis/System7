/*
 * MenuAppIcon.c - Draw Application (top-right) menu icon
 * Uses 1-bit 16x16 glyph from system7_resources.h
 */
#include "MenuManager/MenuAppIcon.h"
#include "QuickDraw/QuickDraw.h"
#include "Platform/Framebuffer.h"
#include "Resources/system7_resources.h"
#include "System71StdLib.h"
#include "Finder/Icon/icon_types.h"
#include "Finder/Icon/icon_port.h"
#include "Resources/icons_generated.h"

#define MENU_APP_ICON_WIDTH  20
#define MENU_APP_ICON_HEIGHT 18

/* 16x16 Finder face glyph mask & image */
static const unsigned char gFinderMenuMask16[32] = {
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF
};

static const unsigned char gFinderMenuImage16[32] = {
    0xFF, 0xFF,
    0x80, 0x01,
    0x98, 0x19,
    0x98, 0x19,
    0x80, 0x01,
    0x86, 0x31,
    0x8F, 0xF1,
    0x8F, 0xF1,
    0x86, 0x31,
    0x80, 0x01,
    0x84, 0x89,
    0x88, 0x05,
    0x90, 0x03,
    0xA0, 0x01,
    0xC0, 0x03,
    0xFF, 0xFF
};

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
        /* Load the Finder application icon (custom Finder icon)
         * ID 999 is the custom Finder icon from finder.png */
        sFinderAvailable = IconGen_FindByID(999, &sFinderIcon);
        if (sFinderAvailable) {
            serial_puts("MenuAppIcon: loaded custom Finder icon\n");
        } else {
            serial_puts("MenuAppIcon: failed to load Finder icon, using fallback glyph\n");
        }
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

        /* NOTE: Do NOT call DrawFinderGlyph16 here - that would overwrite the color icon
         * with a monochrome version. The icon has already been rendered above. */

        SetPort(prev);
        return MENU_APP_ICON_WIDTH;
    }

    UInt32* pixels = (UInt32*)__builtin_assume_aligned(
        port->portBits.baseAddr, _Alignof(UInt32));
    const int stride = rowBytes / 4; /* 32-bit pixels */
    const UInt32 fgColor = highlighted ? pack_color(255, 255, 255) : pack_color(0, 0, 0);
    const UInt32 bgColor = highlighted ? pack_color(0, 0, 0) : pack_color(255, 255, 255);

    for (int y = 0; y < 16; y++) {
        int dstY = iconTop + y;
        if (dstY < port->portBits.bounds.top || dstY >= port->portBits.bounds.bottom) continue;

        unsigned short maskRow = (unsigned short)((gFinderMenuMask16[y * 2] << 8) |
                                                 gFinderMenuMask16[y * 2 + 1]);
        unsigned short imgRow = (unsigned short)((gFinderMenuImage16[y * 2] << 8) |
                                                gFinderMenuImage16[y * 2 + 1]);

        for (int x = 0; x < 16; x++) {
            if (maskRow & (1u << (15 - x))) {
                int dstX = iconLeft + x;
                if (dstX < port->portBits.bounds.left || dstX >= port->portBits.bounds.right) continue;
                uint32_t color = (imgRow & (1u << (15 - x))) ? fgColor : bgColor;
                pixels[(dstY - port->portBits.bounds.top) * stride +
                       (dstX - port->portBits.bounds.left)] = color;
            }
        }
    }

    return MENU_APP_ICON_WIDTH;
}

#include "MenuManager/MenuAppleIcon.h"

#include <stdint.h>
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "apple16.h"

extern UInt32 pack_color(uint8_t r, uint8_t g, uint8_t b);

short MenuAppleIcon_Draw(GrafPtr port, short left, short top, Boolean highlighted)
{
    if (!port || !port->portBits.baseAddr) {
        return MENU_APPLE_ICON_WIDTH;
    }

    SInt16 rowBytes = port->portBits.rowBytes & 0x3FFF;
    if (rowBytes <= 0) {
        return MENU_APPLE_ICON_WIDTH;
    }

    SInt16 width = port->portBits.bounds.right - port->portBits.bounds.left;
    SInt16 height = port->portBits.bounds.bottom - port->portBits.bounds.top;
    if (width <= 0 || height <= 0) {
        return MENU_APPLE_ICON_WIDTH;
    }

    UInt32* pixels = (UInt32*)port->portBits.baseAddr;
    const int stride = rowBytes / 4; /* 32-bit pixels */

    const int iconLeft = left + (MENU_APPLE_ICON_WIDTH - APPLE16_W) / 2;
    const int iconTop = top + (MENU_APPLE_ICON_HEIGHT - APPLE16_H) / 2;

    for (int y = 0; y < APPLE16_H; y++) {
        int dstY = iconTop + y;
        if (dstY < port->portBits.bounds.top || dstY >= port->portBits.bounds.bottom) {
            continue;
        }

        for (int x = 0; x < APPLE16_W; x++) {
            int dstX = iconLeft + x;
            if (dstX < port->portBits.bounds.left || dstX >= port->portBits.bounds.right) {
                continue;
            }

            size_t srcIndex = ((size_t)y * APPLE16_W + x) * 4;
            uint8_t alpha = gApple16_RGBA8[srcIndex + 3];
            if (alpha < 32) {
                continue;
            }

            UInt32 color;
            if (highlighted) {
                color = pack_color(255, 255, 255);
            } else {
                uint8_t r = gApple16_RGBA8[srcIndex + 0];
                uint8_t g = gApple16_RGBA8[srcIndex + 1];
                uint8_t b = gApple16_RGBA8[srcIndex + 2];
                color = pack_color(r, g, b);
            }

            pixels[(dstY - port->portBits.bounds.top) * stride +
                   (dstX - port->portBits.bounds.left)] = color;
        }
    }

    return MENU_APPLE_ICON_WIDTH;
}

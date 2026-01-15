/* Icon Drawing with QuickDraw-faithful Compositing
 * Handles ICN# (1-bit) and cicn (color) icons
 */

#include "Finder/Icon/icon_types.h"
#include <stdint.h>
#include <stddef.h>
#include "System71StdLib.h"
#include "QuickDraw/QuickDraw.h"
#include "Finder/Icon/icon_port.h"

/* CRITICAL FIX: Disable logging on ARM64 - serial_logf uses va_list which hangs */
#if defined(__aarch64__) || defined(__arm64__)
#define FINDER_ICON_LOG_DEBUG(fmt, ...) ((void)0)
#else
#define FINDER_ICON_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleFinder, kLogLevelDebug, fmt, ##__VA_ARGS__)
#endif

/* Helper: Get bit from bitmap */
static inline uint8_t GetBit(const uint8_t* row, int x) {
    return (row[x >> 3] >> (7 - (x & 7))) & 1;
}

/* Draw 1-bit ICN# icon
 *
 * Classic Mac icons use a two-layer system:
 * 1. Mask layer: Defines which pixels are part of the icon (1 = opaque, 0 = transparent)
 * 2. Image layer: Defines the color of opaque pixels (1 = black, 0 = white)
 *
 * This allows icons to have white bodies with black outlines and details.
 * The mask creates the shape, while the image adds the black pixels for details.
 *
 * Example: A trash can icon would have:
 * - Mask: All 1s for the entire trash can shape
 * - Image: 1s only for the outline and vertical stripes, 0s for the white body
 */
static void DrawICN32(const IconBitmap* ib, int dx, int dy, bool selected) {
    uint32_t black = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;

    FINDER_ICON_LOG_DEBUG("[ICON_DRAW] DrawICN32 at (%d,%d) selected=%d\n", dx, dy, selected);

    /* Draw the icon using proper Mac mask/image compositing:
     * - Mask defines the shape (1 = opaque, 0 = transparent)
     * - Image defines the colors (1 = black, 0 = white)
     * - If selected, blend with dark blue to create highlight effect
     */
    for (int y = 0; y < 32; ++y) {
        const uint8_t* mrow = ib->mask1b + y * 4;  /* 32px = 4 bytes */
        const uint8_t* irow = ib->img1b + y * 4;

        for (int x = 0; x < 32; ++x) {
            /* Only draw where mask bit is set (opaque area) */
            if (GetBit(mrow, x)) {
                uint32_t color;
                /* If image bit is set, draw black; otherwise draw white */
                if (GetBit(irow, x)) {
                    color = black;
                } else {
                    color = white;
                }

                /* If selected, blend with dark blue for highlight */
                if (selected) {
                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;
                    /* Blend 50% with dark blue */
                    r = (r + 0x00) / 2;
                    g = (g + 0x00) / 2;
                    b = (b + 0x80) / 2;
                    color = 0xFF000000 | (r << 16) | (g << 8) | b;
                }

                IconPort_WritePixel(dx + x, dy + y, color);
            }
        }
    }
}

/* Draw color cicn icon */
static void DrawCICN32(const IconBitmap* ib, int dx, int dy, bool selected) {
    for (int y = 0; y < 32; ++y) {
        const uint32_t* src = ib->argb32 + y * 32;

        for (int x = 0; x < 32; ++x) {
            uint32_t pixel = src[x];
            uint8_t alpha = (pixel >> 24) & 0xFF;

            if (alpha > 0) {
                /* If selected, blend with dark blue */
                if (selected) {
                    uint8_t r = (pixel >> 16) & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t b = pixel & 0xFF;
                    /* Blend 50% with dark blue */
                    r = (r + 0x00) / 2;
                    g = (g + 0x00) / 2;
                    b = (b + 0x80) / 2;
                    pixel = (alpha << 24) | (r << 16) | (g << 8) | b;
                }
                IconPort_WritePixel(dx + x, dy + y, pixel);
            }
        }
    }
}

/* Public API: Draw 32x32 icon */
void Icon_Draw32(const IconHandle* h, int x, int y, bool selected) {
    if (!h || !h->fam) return;

    const IconBitmap* b = &h->fam->large;

    if (b->depth == kIconColor32 && b->argb32) {
        DrawCICN32(b, x, y, selected);
    } else if (b->img1b && b->mask1b) {
        DrawICN32(b, x, y, selected);
    }
}

/* Draw 1-bit SICN 16x16 icon */
static void DrawSICN16(const IconBitmap* ib, int dx, int dy) {
    uint32_t black = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;

    FINDER_ICON_LOG_DEBUG("[ICON_DRAW] DrawSICN16 at (%d,%d)\n", dx, dy);

    /* Draw 16x16 icon using mask/image compositing */
    for (int y = 0; y < 16; ++y) {
        const uint8_t* mrow = ib->mask1b + y * 2;  /* 16px = 2 bytes */
        const uint8_t* irow = ib->img1b + y * 2;

        for (int x = 0; x < 16; ++x) {
            /* Only draw where mask bit is set */
            if (GetBit(mrow, x)) {
                uint32_t color = GetBit(irow, x) ? black : white;
                IconPort_WritePixel(dx + x, dy + y, color);
            }
        }
    }
}

/* Draw 16x16 icon (for list views) */
void Icon_Draw16(const IconHandle* h, int x, int y) {
    if (!h || !h->fam) return;

    /* Use small icon if available */
    if (h->fam->hasSmall) {
        const IconBitmap* b = &h->fam->small;
        if (b->depth == kIconColor32 && b->argb32) {
            /* Draw color 16x16 (if available) */
            for (int py = 0; py < 16; ++py) {
                const uint32_t* src = b->argb32 + py * 16;
                for (int px = 0; px < 16; ++px) {
                    uint32_t pixel = src[px];
                    uint8_t alpha = (pixel >> 24) & 0xFF;
                    if (alpha > 0) {
                        IconPort_WritePixel(x + px, y + py, pixel);
                    }
                }
            }
        } else if (b->img1b && b->mask1b) {
            DrawSICN16(b, x, y);
        }
    } else {
        /* Fallback: scale down large icon (simplified - just skip every other pixel) */
        const IconBitmap* b = &h->fam->large;
        if (b->depth == kIconColor32 && b->argb32) {
            for (int py = 0; py < 16; ++py) {
                const uint32_t* src = b->argb32 + (py * 2) * 32;
                for (int px = 0; px < 16; ++px) {
                    uint32_t pixel = src[px * 2];
                    uint8_t alpha = (pixel >> 24) & 0xFF;
                    if (alpha > 0) {
                        IconPort_WritePixel(x + px, y + py, pixel);
                    }
                }
            }
        } else if (b->img1b && b->mask1b) {
            /* Scale 32x32 to 16x16 by sampling every other pixel */
            uint32_t black = 0xFF000000;
            uint32_t white = 0xFFFFFFFF;
            for (int py = 0; py < 16; ++py) {
                const uint8_t* mrow = b->mask1b + (py * 2) * 4;
                const uint8_t* irow = b->img1b + (py * 2) * 4;
                for (int px = 0; px < 16; ++px) {
                    if (GetBit(mrow, px * 2)) {
                        uint32_t color = GetBit(irow, px * 2) ? black : white;
                        IconPort_WritePixel(x + px, y + py, color);
                    }
                }
            }
        }
    }
}

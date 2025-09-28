/*
 * QuickDrawPlatform.c - Platform implementation for QuickDraw
 * Connects QuickDraw to the actual framebuffer
 */

#include "MacTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawPlatform.h"
#include "QuickDrawConstants.h"  /* For paint, frame, erase, patCopy */
#include <stdlib.h>  /* For abs() */

/* External framebuffer from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* Platform framebuffer instance */
static PlatformFramebuffer g_platformFB;

/* Initialize platform layer */
Boolean QDPlatform_Initialize(void) {
    g_platformFB.baseAddr = framebuffer;
    g_platformFB.width = fb_width;
    g_platformFB.height = fb_height;
    g_platformFB.pitch = fb_pitch;
    return (framebuffer != NULL);
}

/* Shutdown platform layer */
void QDPlatform_Shutdown(void) {
    /* Nothing to do */
}

/* Get framebuffer */
PlatformFramebuffer* QDPlatform_GetFramebuffer(void) {
    return &g_platformFB;
}

/* Lock framebuffer */
void QDPlatform_LockFramebuffer(PlatformFramebuffer* fb) {
    /* No locking needed in our simple implementation */
}

/* Unlock framebuffer */
void QDPlatform_UnlockFramebuffer(PlatformFramebuffer* fb) {
    /* No locking needed in our simple implementation */
}

/* VGA status register port for vsync detection */
#define VGA_INPUT_STATUS_1 0x3DA
#define VGA_VRETRACE_BIT 0x08

/* Inline assembly helpers for VGA I/O */
static inline uint8_t inb_vga(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Wait for VGA vertical retrace (vsync) to ensure screen update */
static void vga_wait_vsync(void) {
    /* Wait for any current retrace to end */
    while (inb_vga(VGA_INPUT_STATUS_1) & VGA_VRETRACE_BIT);
    /* Wait for next retrace to begin */
    while (!(inb_vga(VGA_INPUT_STATUS_1) & VGA_VRETRACE_BIT));
}

/* Update screen region */
void QDPlatform_UpdateScreen(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom) {
    /* Minimal delay to allow QEMU display refresh - faster than full vsync */
    volatile int delay;
    for (delay = 0; delay < 50; delay++) {
        /* Read VGA status register to yield CPU time to QEMU */
        (void)inb_vga(VGA_INPUT_STATUS_1);
    }
}

/* Flush entire screen */
void QDPlatform_FlushScreen(void) {
    /* Minimal delay to allow QEMU display refresh - faster than full vsync */
    volatile int delay;
    for (delay = 0; delay < 50; delay++) {
        /* Read VGA status register to yield CPU time to QEMU */
        (void)inb_vga(VGA_INPUT_STATUS_1);
    }
}

/* Set a pixel */
void QDPlatform_SetPixel(SInt32 x, SInt32 y, UInt32 color) {
    extern GrafPtr g_currentPort;

    /* Use the current port's bitmap if available */
    if (g_currentPort && g_currentPort->portBits.baseAddr) {
        /* Clip to port bounds */
        if (x < g_currentPort->portBits.bounds.left ||
            x >= g_currentPort->portBits.bounds.right ||
            y < g_currentPort->portBits.bounds.top ||
            y >= g_currentPort->portBits.bounds.bottom) {
            return;
        }

        /* Draw to port's bitmap */
        int rowBytes = g_currentPort->portBits.rowBytes;
        uint32_t* pixel = (uint32_t*)((uint8_t*)g_currentPort->portBits.baseAddr +
                                      y * rowBytes + x * 4);
        *pixel = color;
    } else {
        /* Fall back to framebuffer */
        if (!framebuffer) return;
        if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;

        uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
        *pixel = color;
    }
}

/* Get a pixel */
UInt32 QDPlatform_GetPixel(SInt32 x, SInt32 y) {
    if (!framebuffer) return 0;
    if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return 0;

    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
    return *pixel;
}

/* Draw line accelerated - return false to use software implementation */
Boolean QDPlatform_DrawLineAccelerated(SInt32 x1, SInt32 y1, SInt32 x2, SInt32 y2, UInt32 color) {
    return false;  /* Use software implementation */
}

/* Fill rectangle accelerated - simple implementation */
Boolean QDPlatform_FillRectAccelerated(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom, UInt32 color) {
    if (!framebuffer) return false;

    /* Clip to screen bounds */
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > fb_width) right = fb_width;
    if (bottom > fb_height) bottom = fb_height;

    for (SInt32 y = top; y < bottom; y++) {
        for (SInt32 x = left; x < right; x++) {
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
            *pixel = color;
        }
    }

    return true;
}

/* Blit accelerated - return false to use software implementation */
Boolean QDPlatform_BlitAccelerated(void* src, SInt32 srcX, SInt32 srcY,
                                void* dst, SInt32 dstX, SInt32 dstY,
                                SInt32 width, SInt32 height) {
    return false;  /* Use software implementation */
}

/* Convert RGB color to platform pixel format */
UInt32 QDPlatform_RGBToPixel(UInt8 red, UInt8 green, UInt8 blue) {
    return pack_color(red, green, blue);
}

/* Draw a line using platform capabilities - called from QuickDrawCore */
void QDPlatform_DrawLine(GrafPtr port, Point startPt, Point endPt,
                        const Pattern* pat, SInt16 mode) {
    /* Simple Bresenham's algorithm */
    SInt32 x1 = startPt.h;
    SInt32 y1 = startPt.v;
    SInt32 x2 = endPt.h;
    SInt32 y2 = endPt.v;

    SInt32 dx = abs(x2 - x1);
    SInt32 dy = abs(y2 - y1);
    SInt32 sx = (x1 < x2) ? 1 : -1;
    SInt32 sy = (y1 < y2) ? 1 : -1;
    SInt32 err = dx - dy;

    /* Use black for drawing */
    UInt32 drawColor = pack_color(0, 0, 0);

    while (1) {
        /* Handle XOR mode */
        if (mode == patXor) {
            /* Read current pixel */
            UInt32 current = QDPlatform_GetPixel(x1, y1);
            /* XOR with draw color */
            UInt32 xored = current ^ drawColor;
            QDPlatform_SetPixel(x1, y1, xored);
        } else {
            /* Normal copy mode */
            QDPlatform_SetPixel(x1, y1, drawColor);
        }

        if (x1 == x2 && y1 == y2) break;

        SInt32 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/* For Pattern Manager color patterns */
extern bool PM_GetColorPattern(uint32_t** patternData);

/* Draw a shape using platform capabilities - called from QuickDrawCore */
void QDPlatform_DrawShape(GrafPtr port, GrafVerb verb, const Rect* rect,
                         SInt16 shapeType, const Pattern* pat) {

    /* CRITICAL: Convert LOCAL coordinates to GLOBAL for SetPixel!
     * rect is in port-relative (LOCAL) coords, but SetPixel expects GLOBAL screen coords */
    SInt32 offsetX = port ? port->portBits.bounds.left : 0;
    SInt32 offsetY = port ? port->portBits.bounds.top : 0;

    extern void serial_printf(const char* fmt, ...);
    serial_printf("QDPlatform_DrawShape: verb=%d, rect=(%d,%d,%d,%d), offset=(%d,%d)\n",
                  verb, rect->left, rect->top, rect->right, rect->bottom, offsetX, offsetY);

    /* For now, just draw rectangles */
    if (shapeType == 0) {  /* Rectangle */
        if (verb == paint) {
            /* Fill rectangle with pattern or black */
            if (pat) {
                /* Draw with pattern */
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        /* Get pattern bit (8x8 repeating) */
                        SInt32 patY = (y - rect->top) % 8;
                        SInt32 patX = (x - rect->left) % 8;
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;

                        /* Use black for 1 bits, white for 0 bits */
                        UInt32 color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            } else {
                /* No pattern - fill with black */
                UInt32 color = pack_color(0, 0, 0);
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            }
        } else if (verb == fill) {
            /* Fill with pattern (used by FillRect) */
            if (pat) {
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        /* Get pattern bit (8x8 repeating) */
                        SInt32 patY = y % 8;  /* Use absolute position for desktop pattern */
                        SInt32 patX = x % 8;
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;

                        /* Use black for 1 bits, white for 0 bits */
                        UInt32 color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            }
        } else if (verb == frame) {
            /* Draw rectangle outline using port's pen mode */
            /* CRITICAL: Point is {v, h} not {h, v}! */
            Point tl = {rect->top, rect->left};
            Point tr = {rect->top, rect->right - 1};
            Point br = {rect->bottom - 1, rect->right - 1};
            Point bl = {rect->bottom - 1, rect->left};

            SInt16 mode = port ? port->pnMode : patCopy;
            QDPlatform_DrawLine(port, tl, tr, pat, mode);
            QDPlatform_DrawLine(port, tr, br, pat, mode);
            QDPlatform_DrawLine(port, br, bl, pat, mode);
            QDPlatform_DrawLine(port, bl, tl, pat, mode);
        } else if (verb == erase) {
            /* Erase should use port's background pattern, NOT desktop pattern */
            if (pat) {
                /* Use 1-bit pattern with background color */
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        /* Get pattern bit (8x8 repeating) */
                        SInt32 patY = y % 8;  /* Use absolute position for desktop pattern */
                        SInt32 patX = x % 8;
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;

                        /* Use black for 1 bits, white for 0 bits */
                        UInt32 color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            } else {
                /* No pattern - fill with white */
                UInt32 color = pack_color(255, 255, 255);
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            }
        }
    }
}

/* Convert RGB 16-bit values to native pixel format */
UInt32 QDPlatform_RGBToNative(UInt16 red, UInt16 green, UInt16 blue) {
    /* Convert 16-bit Mac colors (0-65535) to 8-bit (0-255) */
    UInt8 r = (red >> 8);
    UInt8 g = (green >> 8);
    UInt8 b = (blue >> 8);
    return pack_color(r, g, b);
}

/* Convert native pixel format to RGB 16-bit values */
void QDPlatform_NativeToRGB(UInt32 native, UInt16* red, UInt16* green, UInt16* blue) {
    /* Extract 8-bit components and convert to 16-bit Mac colors */
    if (red) *red = ((native >> 16) & 0xFF) * 257;    /* multiply by 257 to convert 0-255 to 0-65535 */
    if (green) *green = ((native >> 8) & 0xFF) * 257;
    if (blue) *blue = (native & 0xFF) * 257;
}
/* QuickDraw Platform region drawing implementation */
/* Renders region outline/fill based on mode */
void QDPlatform_DrawRegion(RgnHandle rgn, short mode, const Pattern* pat) {
    if (!rgn || !*rgn) return;

    Region* region = (Region*)*rgn;
    const Rect* r = &region->rgnBBox;

    if (!framebuffer || !r) return;

    /* Handle erase mode with Pattern Manager color patterns */
    if (mode == erase) {
        extern bool PM_GetColorPattern(uint32_t** patternData);
        uint32_t* colorPattern = NULL;

        if (PM_GetColorPattern(&colorPattern)) {
            /* Use color pattern - tile 8x8 across region bounds */
            int left = (r->left < 0) ? 0 : r->left;
            int top = (r->top < 0) ? 0 : r->top;
            int right = (r->right > fb_width) ? fb_width : r->right;
            int bottom = (r->bottom > fb_height) ? fb_height : r->bottom;

            for (int y = top; y < bottom; y++) {
                for (int x = left; x < right; x++) {
                    /* Get pattern pixel (8x8 tile) using absolute screen position */
                    int patRow = y & 7;
                    int patCol = x & 7;
                    uint32_t patColor = colorPattern[patRow * 8 + patCol];

                    /* Extract RGB from ARGB */
                    uint8_t red = (patColor >> 16) & 0xFF;
                    uint8_t green = (patColor >> 8) & 0xFF;
                    uint8_t blue = patColor & 0xFF;

                    uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
                    *pixel = pack_color(red, green, blue);
                }
            }
            return;
        }
    }

    /* For other modes or if pattern not available, use simple rect operations */
    if (mode == erase) {
        extern void EraseRect(const Rect* r);
        EraseRect(r);
    } else if (mode == paint && pat) {
        /* Simple paint with pattern */
        for (int y = r->top; y < r->bottom; y++) {
            for (int x = r->left; x < r->right; x++) {
                int patY = (y - r->top) % 8;
                int patX = (x - r->left) % 8;
                uint8_t patByte = pat->pat[patY];
                bool bit = (patByte >> (7 - patX)) & 1;
                uint32_t color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                QDPlatform_SetPixel(x, y, color);
            }
        }
    }
    /* frame, invert, fill modes not yet implemented */
}

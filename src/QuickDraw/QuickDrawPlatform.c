/*
#include "QuickDraw/QuickDrawInternal.h"
 * QuickDrawPlatform.c - Platform implementation for QuickDraw
 * Connects QuickDraw to the actual framebuffer
 */

#include "MacTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawPlatform.h"
#include "QuickDrawConstants.h"  /* For paint, frame, erase, patCopy */
#include "FontManager/FontTypes.h"  /* For FontStrike */
#include <stdlib.h>  /* For abs() */
#include <math.h>
#include "QuickDraw/QDLogging.h"

/* Define M_PI if not defined */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Maximum polygon points */
#ifndef MAX_POLY_POINTS
#define MAX_POLY_POINTS 1024
#endif

/* External framebuffer from main.c */
extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

/* Platform framebuffer instance */
static PlatformFramebuffer g_platformFB;

static inline Boolean QDPointInEllipse(SInt32 x, SInt32 y, const Rect* rect) {
    SInt32 width = rect->right - rect->left;
    SInt32 height = rect->bottom - rect->top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    double rx = width / 2.0;
    double ry = height / 2.0;
    double cx = rect->left + rx;
    double cy = rect->top + ry;

    double dx = (x + 0.5 - cx) / rx;
    double dy = (y + 0.5 - cy) / ry;

    return (dx * dx + dy * dy) <= 1.0;
}

UInt32 QDPlatform_MapQDColor(SInt32 qdColor) {
    switch (qdColor) {
        case whiteColor:
            return pack_color(255, 255, 255);
        case redColor:
            return pack_color(255, 0, 0);
        case greenColor:
            return pack_color(0, 255, 0);
        case blueColor:
            return pack_color(0, 0, 255);
        case cyanColor:
            return pack_color(0, 255, 255);
        case magentaColor:
            return pack_color(255, 0, 255);
        case yellowColor:
            return pack_color(255, 255, 0);
        case blackColor:
        default:
            return pack_color(0, 0, 0);
    }
}

static inline UInt32 QDPlatform_SelectPatternColor(GrafPtr port,
                                                  const Pattern* pat,
                                                  SInt32 x, SInt32 y,
                                                  UInt32 fallback) {
    if (!pat) {
        return fallback;
    }

    SInt32 patY = y & 7;
    SInt32 patX = x & 7;
    UInt8 patByte = pat->pat[patY];
    Boolean bit = (patByte >> (7 - patX)) & 1;

    UInt32 fg = fallback;
    UInt32 bg = pack_color(255, 255, 255);
    if (port) {
        fg = QDPlatform_MapQDColor(port->fgColor);
        bg = QDPlatform_MapQDColor(port->bkColor);
    }

    return bit ? fg : bg;
}

static inline Boolean QDPointInRoundRect(SInt32 x, SInt32 y, const Rect* rect,
                                         SInt16 radiusH, SInt16 radiusV) {
    if (x < rect->left || x >= rect->right ||
        y < rect->top || y >= rect->bottom) {
        return false;
    }

    if (radiusH <= 0 || radiusV <= 0) {
        return true; /* Degenerates to rectangle */
    }

    SInt32 innerLeft = rect->left + radiusH;
    SInt32 innerRight = rect->right - radiusH;
    SInt32 innerTop = rect->top + radiusV;
    SInt32 innerBottom = rect->bottom - radiusV;

    if ((x >= innerLeft && x < innerRight) ||
        (y >= innerTop && y < innerBottom)) {
        return true;
    }

    double rx = radiusH;
    double ry = radiusV;
    if (rx <= 0.0 || ry <= 0.0) {
        return true;
    }

    double cx = (x < innerLeft) ? (rect->left + radiusH)
                                : (rect->right - radiusH);
    double cy = (y < innerTop) ? (rect->top + radiusV)
                               : (rect->bottom - radiusV);

    double dx = (x + 0.5 - cx) / rx;
    double dy = (y + 0.5 - cy) / ry;
    return (dx * dx + dy * dy) <= 1.0;
}

/* Check if a point is inside an arc */
static inline Boolean QDPointInArc(SInt32 x, SInt32 y, const Rect* rect,
                                   SInt16 startAngle, SInt16 arcAngle) {
    /* First check if point is in the ellipse */
    if (!QDPointInEllipse(x, y, rect)) {
        return false;
    }

    /* Calculate center of ellipse */
    double cx = (rect->left + rect->right) / 2.0;
    double cy = (rect->top + rect->bottom) / 2.0;

    /* Calculate angle from center to point */
    /* QuickDraw angles: 0 = 3 o'clock, 90 = 12 o'clock, counter-clockwise */
    double dx = (x + 0.5) - cx;
    double dy = cy - (y + 0.5); /* Flip Y for Mac coordinate system */
    double angleRad = atan2(dy, dx);
    double angleDeg = angleRad * 180.0 / M_PI;

    /* Normalize to 0-360 */
    while (angleDeg < 0) angleDeg += 360;
    while (angleDeg >= 360) angleDeg -= 360;

    /* Normalize startAngle to 0-360 */
    double start = startAngle;
    while (start < 0) start += 360;
    while (start >= 360) start -= 360;

    /* Check if point angle is within arc angle range */
    double end = start + arcAngle;

    if (arcAngle >= 0) {
        /* Positive arc (counter-clockwise) */
        if (end <= 360) {
            return (angleDeg >= start && angleDeg <= end);
        } else {
            /* Arc wraps around 0 degrees */
            return (angleDeg >= start || angleDeg <= (end - 360));
        }
    } else {
        /* Negative arc (clockwise) */
        if (start + arcAngle >= 0) {
            return (angleDeg >= (start + arcAngle) && angleDeg <= start);
        } else {
            /* Arc wraps around 0 degrees */
            return (angleDeg >= (start + arcAngle + 360) || angleDeg <= start);
        }
    }
}

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

#include "Platform/include/io.h"

/* Inline assembly helpers for VGA I/O */
#define inb_vga(port) hal_inb(port)

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
    extern CGrafPtr g_currentCPort;  /* from ColorQuickDraw.c */

    if (!g_currentPort) {
        /* No port - draw to framebuffer */
        if (!framebuffer) return;
        if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;
        uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
        *pixel = color;
        return;
    }

    /* Check if this is a color port (CGrafPtr/GWorld) */
    Boolean isColorPort = (g_currentCPort != NULL && (GrafPtr)g_currentCPort == g_currentPort);

    if (isColorPort) {
        /* Drawing to CGrafPort/GWorld - use portPixMap */
        CGrafPtr cport = (CGrafPtr)g_currentPort;
        if (cport->portPixMap && *cport->portPixMap) {
            PixMapPtr pm = *cport->portPixMap;
            Ptr baseAddr = pm->baseAddr;
            SInt16 rowBytes = pm->rowBytes & 0x3FFF;
            SInt16 width = pm->bounds.right - pm->bounds.left;
            SInt16 height = pm->bounds.bottom - pm->bounds.top;

            /* x,y are local coords for GWorld - bounds check */
            if (x < 0 || x >= width || y < 0 || y >= height) return;

            /* Draw to GWorld buffer */
            uint32_t* pixel = (uint32_t*)((uint8_t*)baseAddr + y * rowBytes + x * 4);
            *pixel = color;
        }
    } else {
        /* Drawing to basic GrafPort - check if it's the framebuffer */
        if (g_currentPort->portBits.baseAddr == (Ptr)framebuffer) {
            /* Drawing to framebuffer - x,y are global screen coords */
            if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
            *pixel = color;
        } else {
            /* Drawing to offscreen basic bitmap (e.g., window GWorld backing) */
            Ptr baseAddr = g_currentPort->portBits.baseAddr;
            if (!baseAddr) return;

            SInt16 rowBytes = g_currentPort->portBits.rowBytes & 0x3FFF;
            if (rowBytes <= 0) return;

            SInt16 boundsLeft = g_currentPort->portBits.bounds.left;
            SInt16 boundsTop = g_currentPort->portBits.bounds.top;
            SInt16 localX = (SInt16)(x - boundsLeft);
            SInt16 localY = (SInt16)(y - boundsTop);

            /* DEBUG: Log first few pixel draws to window ports */
            extern void serial_puts(const char* str);
            extern int sprintf(char* buf, const char* fmt, ...);
            static int pixel_log = 0;
            if (pixel_log < 20) {
                char dbgbuf[256];
                sprintf(dbgbuf, "[SETPIXEL] input=(%d,%d) bounds=(%d,%d) local=(%d,%d) rowBytes=%d baseAddr=%p\n",
                        (int)x, (int)y, boundsLeft, boundsTop, localX, localY, rowBytes, baseAddr);
                serial_puts(dbgbuf);
                pixel_log++;
            }

            SInt16 portWidth = g_currentPort->portRect.right - g_currentPort->portRect.left;
            SInt16 portHeight = g_currentPort->portRect.bottom - g_currentPort->portRect.top;
            if (localX < 0 || localY < 0 || localX >= portWidth || localY >= portHeight) {
                return;
            }

            uint32_t* pixel = (uint32_t*)((uint8_t*)baseAddr + localY * rowBytes + localX * 4);
            *pixel = color;
            /* Do not fall back to framebuffer when drawing to offscreen port */
            return;
        }
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

    SInt32 penWidth = (port && port->pnSize.h > 0) ? port->pnSize.h : 1;
    SInt32 penHeight = (port && port->pnSize.v > 0) ? port->pnSize.v : 1;
    UInt32 fallbackColor = port ? QDPlatform_MapQDColor(port->fgColor)
                                : pack_color(0, 0, 0);

    while (1) {
        for (SInt32 penY = 0; penY < penHeight; penY++) {
            for (SInt32 penX = 0; penX < penWidth; penX++) {
                SInt32 drawX = x1 + penX;
                SInt32 drawY = y1 + penY;

                if (drawX < 0 || drawX >= (SInt32)fb_width ||
                    drawY < 0 || drawY >= (SInt32)fb_height) {
                    continue;
                }

                UInt32 patternColor = QDPlatform_SelectPatternColor(port, pat,
                                                                     drawX, drawY,
                                                                     fallbackColor);
                UInt32 current = QDPlatform_GetPixel(drawX, drawY);
                UInt32 outColor = patternColor;

                switch (mode) {
                    case patXor:
                    case srcXor:
                    case notSrcXor:
                    case notPatXor:
                        outColor = current ^ patternColor;
                        break;
                    case patOr:
                    case srcOr:
                    case notSrcOr:
                    case notPatOr:
                        outColor = current | patternColor;
                        break;
                    case patBic:
                    case srcBic:
                    case notSrcBic:
                    case notPatBic:
                        outColor = current & (~patternColor);
                        break;
                    case notSrcCopy:
                    case notPatCopy:
                        outColor = ~patternColor;
                        break;
                    default:
                        outColor = patternColor;
                        break;
                }

                QDPlatform_SetPixel(drawX, drawY, outColor);
            }
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
                         SInt16 shapeType, const Pattern* pat,
                         SInt16 ovalWidth, SInt16 ovalHeight) {

    /* CRITICAL: DrawPrimitive already converted LOCAL→GLOBAL!
     * rect parameter is already in GLOBAL coordinates.
     * Adding offset here causes DOUBLE transformation - coordinates offset twice!
     * Do NOT add portBits.bounds offset! */
    SInt32 offsetX = 0;
    SInt32 offsetY = 0;

    QD_LOG_TRACE("QDPlatform_DrawShape: verb=%d rect=(%d,%d,%d,%d) offset=(%d,%d)\n",
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
        } else if (verb == invert) {
            /* XOR pixels with white for authentic Mac OS invert/XOR feedback */
            QD_LOG_TRACE("QDPlatform_DrawShape: Inverting rect (%d,%d,%d,%d)\n",
                          rect->left, rect->top, rect->right, rect->bottom);
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    /* Get current pixel color */
                    UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                    /* XOR with white to invert */
                    UInt32 inverted = current ^ 0x00FFFFFF;
                    QDPlatform_SetPixel(x + offsetX, y + offsetY, inverted);
                }
            }
        }
    } else if (shapeType == 1) {  /* Oval */
        if (verb == paint || verb == fill || verb == erase) {
            UInt32 fallbackColor = (verb == erase) ? pack_color(255, 255, 255)
                                                   : pack_color(0, 0, 0);
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInEllipse(x, y, rect)) continue;

                    UInt32 color = fallbackColor;
                    if (pat) {
                        SInt32 patY = (verb == paint) ? ((y - rect->top) & 7) : (y & 7);
                        SInt32 patX = (verb == paint) ? ((x - rect->left) & 7) : (x & 7);
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;
                        color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                    }

                    QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                }
            }
        } else if (verb == frame) {
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInEllipse(x, y, rect)) continue;

                    Boolean neighborOutside =
                        !QDPointInEllipse(x - 1, y, rect) ||
                        !QDPointInEllipse(x + 1, y, rect) ||
                        !QDPointInEllipse(x, y - 1, rect) ||
                        !QDPointInEllipse(x, y + 1, rect);

                    if (neighborOutside) {
                        UInt32 color = pack_color(0, 0, 0);
                        if (pat) {
                            UInt8 patByte = pat->pat[(y - rect->top) & 7];
                            Boolean bit = (patByte >> (7 - ((x - rect->left) & 7))) & 1;
                            color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                        }
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            }
        } else if (verb == invert) {
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInEllipse(x, y, rect)) continue;

                    UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                    UInt32 inverted = current ^ 0x00FFFFFF;
                    QDPlatform_SetPixel(x + offsetX, y + offsetY, inverted);
                }
            }
        }
    } else if (shapeType == 2) {  /* Rounded rectangle */
        SInt16 width = rect->right - rect->left;
        SInt16 height = rect->bottom - rect->top;
        if (width <= 0 || height <= 0) {
            return;
        }

        SInt16 radiusH = ovalWidth / 2;
        SInt16 radiusV = ovalHeight / 2;
        if (radiusH < 0) radiusH = 0;
        if (radiusV < 0) radiusV = 0;
        if (radiusH > width / 2) radiusH = width / 2;
        if (radiusV > height / 2) radiusV = height / 2;

        SInt16 mode = port ? port->pnMode : patCopy;

        if (verb == paint || verb == fill || verb == erase) {
            UInt32 fallbackColor = (verb == erase) ? pack_color(255, 255, 255)
                                                   : pack_color(0, 0, 0);
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInRoundRect(x, y, rect, radiusH, radiusV)) {
                        continue;
                    }

                    UInt32 color = fallbackColor;
                    if (pat) {
                        SInt32 patY = (verb == paint) ? ((y - rect->top) & 7) : (y & 7);
                        SInt32 patX = (verb == paint) ? ((x - rect->left) & 7) : (x & 7);
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;
                        color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                    }

                    if (verb == erase && pat == NULL) {
                        color = pack_color(255, 255, 255);
                    }

                    if (mode == patXor && verb == paint) {
                        UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                        color = current ^ color;
                    }

                    QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                }
            }
        } else if (verb == frame) {
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInRoundRect(x, y, rect, radiusH, radiusV)) {
                        continue;
                    }

                    Boolean neighborOutside =
                        !QDPointInRoundRect(x - 1, y, rect, radiusH, radiusV) ||
                        !QDPointInRoundRect(x + 1, y, rect, radiusH, radiusV) ||
                        !QDPointInRoundRect(x, y - 1, rect, radiusH, radiusV) ||
                        !QDPointInRoundRect(x, y + 1, rect, radiusH, radiusV);

                    if (!neighborOutside) continue;

                    UInt32 color = pack_color(0, 0, 0);
                    if (pat) {
                        SInt32 patY = (y - rect->top) & 7;
                        SInt32 patX = (x - rect->left) & 7;
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;
                        color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                    }

                    if (mode == patXor) {
                        UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                        color = current ^ color;
                    }

                    QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                }
            }
        } else if (verb == invert) {
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInRoundRect(x, y, rect, radiusH, radiusV)) {
                        continue;
                    }

                    UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                    UInt32 inverted = current ^ 0x00FFFFFF;
                    QDPlatform_SetPixel(x + offsetX, y + offsetY, inverted);
                }
            }
        }
    } else if (shapeType == 3) {  /* Arc */
        /* ovalWidth = startAngle, ovalHeight = arcAngle */
        SInt16 startAngle = ovalWidth;
        SInt16 arcAngle = ovalHeight;

        SInt16 mode = port ? port->pnMode : patCopy;

        if (verb == paint || verb == fill || verb == erase) {
            UInt32 fallbackColor = (verb == erase) ? pack_color(255, 255, 255)
                                                   : pack_color(0, 0, 0);
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInArc(x, y, rect, startAngle, arcAngle)) {
                        continue;
                    }

                    UInt32 color = fallbackColor;
                    if (pat) {
                        SInt32 patY = (verb == paint) ? ((y - rect->top) & 7) : (y & 7);
                        SInt32 patX = (verb == paint) ? ((x - rect->left) & 7) : (x & 7);
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;
                        color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                    }

                    if (verb == erase && pat == NULL) {
                        color = pack_color(255, 255, 255);
                    }

                    if (mode == patXor && verb == paint) {
                        UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                        color = current ^ color;
                    }

                    QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                }
            }
        } else if (verb == frame) {
            /* Frame the arc - draw outline only */
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInArc(x, y, rect, startAngle, arcAngle)) {
                        continue;
                    }

                    /* Check if this is an edge pixel */
                    Boolean isEdge = false;

                    /* Check if any neighbor is outside the arc */
                    if (!QDPointInArc(x - 1, y, rect, startAngle, arcAngle) ||
                        !QDPointInArc(x + 1, y, rect, startAngle, arcAngle) ||
                        !QDPointInArc(x, y - 1, rect, startAngle, arcAngle) ||
                        !QDPointInArc(x, y + 1, rect, startAngle, arcAngle)) {
                        isEdge = true;
                    }

                    if (!isEdge) continue;

                    UInt32 color = pack_color(0, 0, 0);
                    if (pat) {
                        SInt32 patY = (y - rect->top) & 7;
                        SInt32 patX = (x - rect->left) & 7;
                        UInt8 patByte = pat->pat[patY];
                        Boolean bit = (patByte >> (7 - patX)) & 1;
                        color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                    }

                    if (mode == patXor) {
                        UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                        color = current ^ color;
                    }

                    QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                }
            }
        } else if (verb == invert) {
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInArc(x, y, rect, startAngle, arcAngle)) {
                        continue;
                    }

                    UInt32 current = QDPlatform_GetPixel(x + offsetX, y + offsetY);
                    UInt32 inverted = current ^ 0x00FFFFFF;
                    QDPlatform_SetPixel(x + offsetX, y + offsetY, inverted);
                }
            }
        }
    }
}

/* Polygon fill using scanline algorithm */
void QDPlatform_FillPoly(GrafPtr port, PolyHandle poly, const Pattern* pat,
                        SInt16 mode, GrafVerb verb) {
    if (!poly || !*poly || !port) return;

    Polygon* polyPtr = *poly;
    SInt16 numPoints = (polyPtr->polySize - sizeof(SInt16) - sizeof(Rect)) / sizeof(Point);

    if (numPoints < 3) return;  /* Need at least 3 points for a polygon */

    /* Get global offset */
    SInt16 offsetX = port->portBits.bounds.left;
    SInt16 offsetY = port->portBits.bounds.top;

    /* Scanline fill algorithm */
    Rect bbox = polyPtr->polyBBox;

    /* Convert LOCAL bbox to GLOBAL */
    bbox.left += offsetX;
    bbox.right += offsetX;
    bbox.top += offsetY;
    bbox.bottom += offsetY;

    /* Clip to screen bounds */
    if (bbox.left < 0) bbox.left = 0;
    if (bbox.top < 0) bbox.top = 0;
    if (bbox.right > (SInt16)fb_width) bbox.right = fb_width;
    if (bbox.bottom > (SInt16)fb_height) bbox.bottom = fb_height;

    /* For each scanline */
    for (SInt32 y = bbox.top; y < bbox.bottom; y++) {
        /* Find intersections with polygon edges */
        SInt16 intersections[MAX_POLY_POINTS];
        SInt16 numIntersections = 0;

        /* Check each edge */
        for (SInt16 i = 0; i < numPoints; i++) {
            SInt16 j = (i + 1) % numPoints;

            /* Get edge vertices in GLOBAL coords */
            SInt32 y1 = polyPtr->polyPoints[i].v + offsetY;
            SInt32 y2 = polyPtr->polyPoints[j].v + offsetY;
            SInt32 x1 = polyPtr->polyPoints[i].h + offsetX;
            SInt32 x2 = polyPtr->polyPoints[j].h + offsetX;

            /* Skip horizontal edges */
            if (y1 == y2) continue;

            /* Check if scanline intersects edge */
            if ((y1 <= y && y < y2) || (y2 <= y && y < y1)) {
                /* Calculate intersection x coordinate */
                SInt32 x = x1 + ((y - y1) * (x2 - x1)) / (y2 - y1);

                if (numIntersections < MAX_POLY_POINTS) {
                    intersections[numIntersections++] = x;
                }
            }
        }

        /* Sort intersections */
        for (SInt16 i = 0; i < numIntersections - 1; i++) {
            for (SInt16 j = i + 1; j < numIntersections; j++) {
                if (intersections[i] > intersections[j]) {
                    SInt16 temp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = temp;
                }
            }
        }

        /* Fill between pairs of intersections */
        for (SInt16 i = 0; i < numIntersections - 1; i += 2) {
            SInt32 x1 = intersections[i];
            SInt32 x2 = intersections[i + 1];

            if (x1 < 0) x1 = 0;
            if (x2 > (SInt32)fb_width) x2 = fb_width;

            for (SInt32 x = x1; x < x2; x++) {
                if (x >= 0 && x < (SInt32)fb_width && y >= 0 && y < (SInt32)fb_height) {
                    UInt32 color;

                    if (verb == invert) {
                        UInt32 current = QDPlatform_GetPixel(x, y);
                        color = current ^ 0x00FFFFFF;
                    } else {
                        /* Apply pattern */
                        if (pat) {
                            SInt32 patY = y & 7;
                            SInt32 patX = x & 7;
                            UInt8 patByte = pat->pat[patY];
                            Boolean bit = (patByte >> (7 - patX)) & 1;
                            color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);
                        } else {
                            color = (verb == erase) ? pack_color(255, 255, 255)
                                                   : pack_color(0, 0, 0);
                        }

                        if (mode == patXor) {
                            UInt32 current = QDPlatform_GetPixel(x, y);
                            color = current ^ color;
                        }
                    }

                    QDPlatform_SetPixel(x, y, color);
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
    } else if (mode == fill && pat) {
        /* Fill region with pattern */
        /* Clamp to screen bounds */
        int left = (r->left < 0) ? 0 : r->left;
        int top = (r->top < 0) ? 0 : r->top;
        int right = (r->right > fb_width) ? fb_width : r->right;
        int bottom = (r->bottom > fb_height) ? fb_height : r->bottom;

        for (int y = top; y < bottom; y++) {
            for (int x = left; x < right; x++) {
                /* Use screen position for pattern tiling (8x8 repeat) */
                int patY = y % 8;
                int patX = x % 8;
                uint8_t patByte = pat->pat[patY];
                bool bit = (patByte >> (7 - patX)) & 1;
                /* Pattern: 0=white, 1=black */
                uint32_t color = bit ? pack_color(0, 0, 0) : pack_color(255, 255, 255);

                /* Write directly to framebuffer */
                uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
                *pixel = color;
            }
        }
    }
    /* frame, invert modes not yet implemented */
}

/* ============================================================================
 * Text Rendering
 * ============================================================================ */

/* Helper to get a bit from MSB-first bitmap */
static inline UInt8 GetBitmapBit(const UInt8 *bitmap, SInt32 bitOffset) {
    return (bitmap[bitOffset >> 3] >> (7 - (bitOffset & 7))) & 1;
}

/*
 * QDPlatform_DrawGlyph - Draw a character glyph from a FontStrike
 *
 * Renders a character from a bitmap font strike to the framebuffer or offscreen GWorld.
 *
 * @param strike    Font strike containing bitmap data
 * @param ch        Character code to render
 * @param x         X position in local coordinates (top-left of glyph)
 * @param y         Y position in local coordinates (top-left of glyph)
 * @param port      Current graphics port (for coordinate conversion)
 * @param color     Pixel color to use
 * @return          Character advance width in pixels
 */
SInt16 QDPlatform_DrawGlyph(struct FontStrike *strike, UInt8 ch, SInt16 x, SInt16 y,
                            GrafPtr port, UInt32 color) {
    if (!strike) {
        return 0;
    }

    /* Check if character is in range */
    if (ch < strike->firstChar || ch > strike->lastChar) {
        return 0;
    }

    /* Get character index */
    SInt16 charIndex = ch - strike->firstChar;

    /* Get location in bitmap (from location table) */
    if (!strike->locTable) {
        return 0;
    }

    SInt16 locStart = strike->locTable[charIndex];
    SInt16 locEnd = strike->locTable[charIndex + 1];
    SInt16 charWidth = locEnd - locStart;

    /* Get bitmap data */
    if (!strike->bitmapData || !(*strike->bitmapData)) {
        return charWidth;
    }

    UInt8 *bitmap = (UInt8 *)(*strike->bitmapData);

    /* Determine rendering destination - GWorld or screen framebuffer */
    Ptr renderBuffer = NULL;
    UInt32 renderPitch = 0;
    UInt32 renderWidth = 0;
    UInt32 renderHeight = 0;
    SInt16 pixelX = x;
    SInt16 pixelY = y;

    /* Check if this is a color port (CGrafPtr) by checking current color port global */
    extern CGrafPtr g_currentCPort;  /* from ColorQuickDraw.c */
    Boolean isColorPort = (g_currentCPort != NULL && (GrafPtr)g_currentCPort == port);

    if (isColorPort) {
        /* Drawing to color port (possibly offscreen GWorld) */
        CGrafPtr cport = (CGrafPtr)port;

        if (cport->portPixMap && *cport->portPixMap) {
            PixMapPtr pm = *cport->portPixMap;
            renderBuffer = pm->baseAddr;
            renderPitch = pm->rowBytes & 0x3FFF;  /* Mask off high bit */
            renderWidth = pm->bounds.right - pm->bounds.left;
            renderHeight = pm->bounds.bottom - pm->bounds.top;

            /* Convert local coordinates to PixMap buffer coordinates */
            pixelX = x - cport->portRect.left;
            pixelY = y - cport->portRect.top;
        } else {
            /* Color port without PixMap - shouldn't happen, fall back to framebuffer */
            if (!framebuffer) return charWidth;
            renderBuffer = (Ptr)framebuffer;
            renderPitch = fb_pitch;
            renderWidth = fb_width;
            renderHeight = fb_height;
            pixelX = x;
            pixelY = y;
        }
    } else {
        /* Drawing to basic GrafPort (screen framebuffer) */
        if (!framebuffer) return charWidth;

        renderBuffer = (Ptr)framebuffer;
        renderPitch = fb_pitch;
        renderWidth = fb_width;
        renderHeight = fb_height;

        if (port) {
            pixelX = x - port->portRect.left + port->portBits.bounds.left;
            pixelY = y - port->portRect.top + port->portBits.bounds.top;
        }
    }

    if (!renderBuffer) return charWidth;

    /* Draw the glyph */
    UInt32 *pixels = (UInt32 *)renderBuffer;
    SInt16 rowWords = strike->rowWords;

    for (SInt16 row = 0; row < strike->fRectHeight; row++) {
        if (pixelY + row < 0 || pixelY + row >= renderHeight) {
            continue;
        }

        /* Calculate bit position in the strike's bitmap */
        SInt32 bitRowStart = row * rowWords * 16;  /* 16 bits per word */

        for (SInt16 col = 0; col < charWidth; col++) {
            if (pixelX + col < 0 || pixelX + col >= renderWidth) {
                continue;
            }

            SInt32 bitPos = bitRowStart + locStart + col;

            if (GetBitmapBit(bitmap, bitPos)) {
                SInt32 pixelOffset = (pixelY + row) * (renderPitch / 4) + (pixelX + col);
                pixels[pixelOffset] = color;
            }
        }
    }

    /* Return character width for pen advancement */
    if (strike->widthTable) {
        return strike->widthTable[charIndex];
    }

    return charWidth;
}

/**
 * QDPlatform_DrawGlyphBitmap - Draw a glyph bitmap at the specified position
 *
 * Draws a character glyph from a packed bitmap to the current port's
 * bitmap at the specified pen position.
 *
 * @param port      Graphics port to draw into
 * @param pen       Pen position in GLOBAL coordinates
 * @param bitmap    Packed bitmap data (bit-aligned rows)
 * @param width     Glyph width in pixels
 * @param height    Glyph height in pixels
 * @param pattern   Pattern to use for foreground pixels
 * @param mode      Transfer mode (srcCopy, srcOr, etc.)
 */
void QDPlatform_DrawGlyphBitmap(GrafPtr port, Point pen,
                         const uint8_t *bitmap,
                         SInt16 width, SInt16 height,
                         const Pattern *pattern, SInt16 mode) {
    static int call_count = 0;

    if (!port || !bitmap || width <= 0 || height <= 0) {
        return;
    }

    /* Get destination bitmap */
    BitMap *destBits = &port->portBits;
    if (!destBits->baseAddr) {
        return;
    }

    /* Convert global pen position to bitmap coordinates */
    SInt16 destX = pen.h - destBits->bounds.left;
    SInt16 destY = pen.v - destBits->bounds.top;

    /* Debug first few calls */
    if (call_count < 30) {
        extern void serial_printf(const char* fmt, ...);
        extern void* framebuffer;
        long baseOffset = (char*)destBits->baseAddr - (char*)framebuffer;
        serial_printf("[GLYPH] pen=(%d,%d) bounds=(%d,%d,%d,%d) dest=(%d,%d) fbOffset=%ld\n",
                     pen.h, pen.v,
                     destBits->bounds.left, destBits->bounds.top,
                     destBits->bounds.right, destBits->bounds.bottom,
                     destX, destY, baseOffset);
        call_count++;
    }

    /* Get bitmap dimensions */
    SInt16 destWidth = destBits->bounds.right - destBits->bounds.left;
    SInt16 destHeight = destBits->bounds.bottom - destBits->bounds.top;
    SInt16 destRowBytes = destBits->rowBytes & 0x3FFF;

    /* Determine foreground color from pattern */
    uint32_t fgColor = 0xFF000000;  /* Black default */
    if (pattern) {
        /* Use first byte of pattern to determine intensity */
        uint8_t intensity = 255 - pattern->pat[0];
        fgColor = pack_color(intensity, intensity, intensity);
    }

    /* Get framebuffer pointer */
    uint32_t *pixels = (uint32_t *)destBits->baseAddr;
    SInt32 pixelPitch = destRowBytes / 4;

    /* Draw each pixel of the glyph */
    for (SInt16 row = 0; row < height; row++) {
        SInt16 y = destY + row;
        if (y < 0 || y >= destHeight) {
            continue;
        }

        for (SInt16 col = 0; col < width; col++) {
            SInt16 x = destX + col;
            if (x < 0 || x >= destWidth) {
                continue;
            }

            /* Check if this pixel is set in the glyph bitmap */
            SInt32 bitIndex = row * ((width + 7) / 8) * 8 + col;
            SInt32 byteIndex = bitIndex / 8;
            SInt32 bitOffset = 7 - (bitIndex % 8);

            if (bitmap[byteIndex] & (1 << bitOffset)) {
                /* Pixel is set - draw with foreground color */
                SInt32 pixelOffset = y * pixelPitch + x;

                switch (mode) {
                    case srcCopy:
                    case patCopy:
                        pixels[pixelOffset] = fgColor;
                        break;
                    case srcOr:
                    case patOr:
                        pixels[pixelOffset] |= fgColor;
                        break;
                    case srcXor:
                    case patXor:
                        pixels[pixelOffset] ^= fgColor;
                        break;
                    case srcBic:
                    case patBic:
                        pixels[pixelOffset] &= ~fgColor;
                        break;
                    default:
                        pixels[pixelOffset] = fgColor;
                        break;
                }
            }
        }
    }
}

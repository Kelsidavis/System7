/*
 * QuickDrawPlatform.c - Platform implementation for QuickDraw
 * Connects QuickDraw to the actual framebuffer
 */
#include "QuickDraw/QuickDrawInternal.h"

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

/*
 * Draw one pen pixel at (x,y) in transfer mode `mode` (Inside Macintosh:
 * Imaging With QuickDraw, 3-8). The pattern's set bits are the "black" of
 * the source; Copy writes them in the foreground colour and the rest in the
 * background, Or writes only them, Xor inverts under them, Bic writes the
 * background under them, and the not- modes take the pattern reversed.
 *
 * The modes used to be applied to RGB values, where black is 0: Xor with a
 * black pen changed nothing and Or and Bic did the reverse of their job, so
 * XOR outlines were invisible - and rectangle painting ignored the mode
 * entirely, which turned the patBic grey used to dim inactive controls into
 * a solid checkerboard over them.
 */
static void QD_PenPixel(GrafPtr port, const Pattern* pat, SInt16 mode, SInt32 x, SInt32 y) {
    Boolean bit = true;
    if (pat) bit = (pat->pat[y & 7] >> (7 - (x & 7))) & 1;
    UInt32 fg = port ? QDPlatform_MapQDColor(port->fgColor) : pack_color(0, 0, 0);
    UInt32 bg = port ? QDPlatform_MapQDColor(port->bkColor) : pack_color(255, 255, 255);

    Boolean notMode = (mode == notPatCopy || mode == notPatOr || mode == notPatXor ||
                       mode == notPatBic || mode == notSrcCopy || mode == notSrcOr ||
                       mode == notSrcXor || mode == notSrcBic);
    if (notMode) bit = !bit;

    switch (mode) {
        case patOr: case notPatOr: case srcOr: case notSrcOr:
            if (bit) QDPlatform_SetPixel(x, y, fg);
            break;
        case patXor: case notPatXor: case srcXor: case notSrcXor:
            if (bit) QDPlatform_SetPixel(x, y, QDPlatform_GetPixel(x, y) ^ 0x00FFFFFF);
            break;
        case patBic: case notPatBic: case srcBic: case notSrcBic:
            if (bit) QDPlatform_SetPixel(x, y, bg);
            break;
        default:   /* the copy modes */
            QDPlatform_SetPixel(x, y, bit ? fg : bg);
            break;
    }
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
extern void Pointer_Shield(int left, int top, int right, int bottom);

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


/* ================================================================
 * Clipping
 *
 * Drawing on screen is limited to the port's rectangle, its clipRgn and,
 * for a window, the part of it no other window covers (Inside Macintosh:
 * Imaging With QuickDraw, 2-16 and 3-11). This clipped to the clipRgn's
 * bounding box only and never consulted the visible region, so a window
 * behind drew straight over the ones in front wherever its clip was not a
 * plain rectangle - and lines and region fills were not clipped at all.
 *
 * Each primitive builds the clip once, as a list of global rectangles, and
 * every pixel it writes to the screen is tested against it. Offscreen
 * buffers are not clipped here: EndUpdate copies them to the screen band by
 * band through the window's visible region.
 * ================================================================ */

enum { kQDClipMaxRects = 128 };
static struct {
    int    depth;
    Boolean active;
    int    count;
    Rect   rects[kQDClipMaxRects];
} gQDClip;

extern Boolean WM_PortVisibleRgn(GrafPtr port, RgnHandle out);

static void QD_ClipAddRegion(RgnHandle rgn) {
    Region* r = *rgn;
    if (EmptyRect(&r->rgnBBox)) return;
    if (r->rgnSize <= 10) {                       /* rectangular */
        gQDClip.rects[gQDClip.count++] = r->rgnBBox;
        return;
    }
    SInt16 n = *(SInt16*)((UInt8*)r + 10);
    const Rect* list = (const Rect*)((UInt8*)r + 12);
    for (SInt16 i = 0; i < n && gQDClip.count < kQDClipMaxRects; i++) {
        gQDClip.rects[gQDClip.count++] = list[i];
    }
}

void QD_ClipBegin(GrafPtr port) {
    if (gQDClip.depth++ > 0) return;              /* the outer primitive's clip holds */
    gQDClip.active = false;
    gQDClip.count = 0;
    extern CGrafPtr g_currentCPort;
    if (!port || !framebuffer || port->portBits.baseAddr != (Ptr)framebuffer) return;
    if (g_currentCPort && (GrafPtr)g_currentCPort == port) return;

    static RgnHandle clip = NULL, tmp = NULL;
    if (!clip) clip = NewRgn();
    if (!tmp) tmp = NewRgn();
    if (!clip || !tmp) return;

    /* The port rectangle, global */
    Rect pr = port->portRect;
    OffsetRect(&pr, port->portBits.bounds.left, port->portBits.bounds.top);
    RectRgn(clip, &pr);
    if (port->clipRgn && *port->clipRgn) SectRgn(clip, port->clipRgn, clip);
    if (WM_PortVisibleRgn(port, tmp)) SectRgn(clip, tmp, clip);

    QD_ClipAddRegion(clip);
    gQDClip.active = true;
}

void QD_ClipEnd(void) {
    if (gQDClip.depth > 0 && --gQDClip.depth == 0) gQDClip.active = false;
}

/* May (x,y), global, be drawn? */
Boolean QD_ClipHas(SInt32 x, SInt32 y) {
    if (!gQDClip.active) return true;
    for (int i = 0; i < gQDClip.count; i++) {
        const Rect* r = &gQDClip.rects[i];
        if (x >= r->left && x < r->right && y >= r->top && y < r->bottom) return true;
    }
    return false;
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
            if (!QD_ClipHas(x, y)) return;
            uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
            *pixel = color;
        } else {
            /* Drawing to offscreen basic bitmap (e.g., window GWorld backing or Direct Framebuffer) */
            Ptr baseAddr = g_currentPort->portBits.baseAddr;
            if (!baseAddr) return;

            SInt16 rowBytes = g_currentPort->portBits.rowBytes & 0x3FFF;
            if (rowBytes <= 0) return;

            /* The buffer starts at the portRect's corner: a pixel is local
             * plus bounds, so the corner is at bounds plus portRect's
             * top left. Measured from bounds alone, everything drawn after
             * SetOrigin - a scrolled document, during its update - fell
             * outside the buffer and vanished. */
            SInt16 boundsLeft = (SInt16)(g_currentPort->portBits.bounds.left + g_currentPort->portRect.left);
            SInt16 boundsTop = (SInt16)(g_currentPort->portBits.bounds.top + g_currentPort->portRect.top);
            SInt16 localX = (SInt16)(x - boundsLeft);
            SInt16 localY = (SInt16)(y - boundsTop);

            SInt16 portWidth = g_currentPort->portRect.right - g_currentPort->portRect.left;
            SInt16 portHeight = g_currentPort->portRect.bottom - g_currentPort->portRect.top;

            /* Log first pixel for debugging */
            static Boolean logged = false;
            if (!logged && baseAddr != (Ptr)framebuffer) {
                logged = true;
                extern void serial_printf(const char* fmt, ...);
                serial_printf("[QDP-PIX] First pixel x=%d y=%d localX=%d localY=%d portWidth=%d portHeight=%d\n",
                             (int)x, (int)y, (int)localX, (int)localY, (int)portWidth, (int)portHeight);
                serial_printf("[QDP-PIX] baseAddr=%p boundsLeft=%d boundsTop=%d rowBytes=%d\n",
                             baseAddr, (int)boundsLeft, (int)boundsTop, (int)rowBytes);
            }

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

/*
 * Get a pixel from wherever the current port draws.
 *
 * This has to agree with QDPlatform_SetPixel about where the pixels live,
 * because the two are used together: invert and patXor read a pixel, combine,
 * and write it back. SetPixel has always been port-aware - GWorld PixMap,
 * offscreen bitmap, or framebuffer - while this read the screen framebuffer
 * unconditionally, so every read-modify-write into an offscreen port mixed
 * two different images: it XORed whatever was on screen into the buffer.
 *
 * A window drawing through BeginUpdate draws into its offscreen GWorld while
 * the screen still holds the previous frame, so inverting a rectangle there
 * combined the new content with the old. Switching a folder window to a list
 * view and inverting the selected row produced that row's icon-view pixels,
 * inverted, instead of the list row - which is what it looked like: fragments
 * of icon labels in a black bar.
 */
UInt32 QDPlatform_GetPixel(SInt32 x, SInt32 y) {
    extern GrafPtr g_currentPort;
    extern CGrafPtr g_currentCPort;  /* from ColorQuickDraw.c */

    if (!g_currentPort) {
        if (!framebuffer) return 0;
        if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return 0;
        return *(uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
    }

    Boolean isColorPort = (g_currentCPort != NULL && (GrafPtr)g_currentCPort == g_currentPort);

    if (isColorPort) {
        CGrafPtr cport = (CGrafPtr)g_currentPort;
        if (cport->portPixMap && *cport->portPixMap) {
            PixMapPtr pm = *cport->portPixMap;
            Ptr baseAddr = pm->baseAddr;
            SInt16 rowBytes = pm->rowBytes & 0x3FFF;
            SInt16 width = pm->bounds.right - pm->bounds.left;
            SInt16 height = pm->bounds.bottom - pm->bounds.top;

            if (!baseAddr || rowBytes <= 0) return 0;
            if (x < 0 || x >= width || y < 0 || y >= height) return 0;

            return *(uint32_t*)((uint8_t*)baseAddr + y * rowBytes + x * 4);
        }
        return 0;
    }

    if (g_currentPort->portBits.baseAddr == (Ptr)framebuffer) {
        if (!framebuffer) return 0;
        if (x < 0 || x >= fb_width || y < 0 || y >= fb_height) return 0;
        return *(uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4);
    }

    /* Offscreen basic bitmap - same global-to-local mapping SetPixel uses */
    Ptr baseAddr = g_currentPort->portBits.baseAddr;
    if (!baseAddr) return 0;

    SInt16 rowBytes = g_currentPort->portBits.rowBytes & 0x3FFF;
    if (rowBytes <= 0) return 0;

    /* From the portRect's corner, as QDPlatform_SetPixel */
    SInt16 localX = (SInt16)(x - g_currentPort->portBits.bounds.left - g_currentPort->portRect.left);
    SInt16 localY = (SInt16)(y - g_currentPort->portBits.bounds.top - g_currentPort->portRect.top);

    SInt16 portWidth = g_currentPort->portRect.right - g_currentPort->portRect.left;
    SInt16 portHeight = g_currentPort->portRect.bottom - g_currentPort->portRect.top;
    if (localX < 0 || localY < 0 || localX >= portWidth || localY >= portHeight) return 0;

    return *(uint32_t*)((uint8_t*)baseAddr + localY * rowBytes + localX * 4);
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
    Pointer_Shield(left, top, right, bottom);

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
static void QDPlatform_DrawLine_Body(GrafPtr port, Point startPt, Point endPt,
                        const Pattern* pat, SInt16 mode) {
    /* Simple Bresenham's algorithm */
    SInt32 x1 = startPt.h;
    SInt32 y1 = startPt.v;
    SInt32 x2 = endPt.h;
    SInt32 y2 = endPt.v;
    {
        SInt32 pw = port ? port->pnSize.h : 1, ph = port ? port->pnSize.v : 1;
        Pointer_Shield((int)(x1 < x2 ? x1 : x2), (int)(y1 < y2 ? y1 : y2),
                       (int)((x1 > x2 ? x1 : x2) + pw), (int)((y1 > y2 ? y1 : y2) + ph));
    }

    SInt32 dx = abs(x2 - x1);
    SInt32 dy = abs(y2 - y1);
    SInt32 sx = (x1 < x2) ? 1 : -1;
    SInt32 sy = (y1 < y2) ? 1 : -1;
    SInt32 err = dx - dy;

    SInt32 penWidth = (port && port->pnSize.h > 0) ? port->pnSize.h : 1;
    SInt32 penHeight = (port && port->pnSize.v > 0) ? port->pnSize.v : 1;
    while (1) {
        for (SInt32 penY = 0; penY < penHeight; penY++) {
            for (SInt32 penX = 0; penX < penWidth; penX++) {
                SInt32 drawX = x1 + penX;
                SInt32 drawY = y1 + penY;

                if (drawX < 0 || drawX >= (SInt32)fb_width ||
                    drawY < 0 || drawY >= (SInt32)fb_height) {
                    continue;
                }

                QD_PenPixel(port, pat, mode, drawX, drawY);
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

void QDPlatform_DrawLine(GrafPtr port, Point startPt, Point endPt,
                        const Pattern* pat, SInt16 mode) {
    extern GrafPtr g_currentPort;
    QD_ClipBegin(port);
    QDPlatform_DrawLine_Body(port, startPt, endPt, pat, mode);
    QD_ClipEnd();
}

/* For Pattern Manager color patterns */
extern bool PM_GetColorPattern(uint32_t** patternData);

/* Draw a shape using platform capabilities - called from QuickDrawCore */
static void QDPlatform_DrawShape_Body(GrafPtr port, GrafVerb verb, const Rect* rect,
                         SInt16 shapeType, const Pattern* pat,
                         SInt16 ovalWidth, SInt16 ovalHeight) {

    /* CRITICAL: DrawPrimitive already converted LOCAL→GLOBAL!
     * rect parameter is already in GLOBAL coordinates.
     * Adding offset here causes DOUBLE transformation - coordinates offset twice!
     * Do NOT add portBits.bounds offset! */
    SInt32 offsetX = 0;
    SInt32 offsetY = 0;

    /* offsetX/offsetY are SInt32 (long on x86-32 with this toolchain);
     * %d would pass a 4-byte int where the printf expects 8 bytes. */
    QD_LOG_TRACE("QDPlatform_DrawShape: verb=%d rect=(%d,%d,%d,%d) offset=(%ld,%ld)\n",
                 verb, rect->left, rect->top, rect->right, rect->bottom,
                 (long)offsetX, (long)offsetY);

    Pointer_Shield(rect->left, rect->top, rect->right, rect->bottom);

    /* For now, just draw rectangles */
    if (shapeType == 0) {  /* Rectangle */
        if (verb == paint) {
            /* The pen's pattern, in the pen's mode */
            SInt16 mode = port ? port->pnMode : patCopy;
            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    QD_PenPixel(port, pat, mode, x + offsetX, y + offsetY);
                }
            }
        } else if (verb == fill) {
            /* Fill with pattern using port foreground/background colors */
            if (pat) {
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        UInt32 color = QDPlatform_SelectPatternColor(port, pat, x, y,
                                                                      pack_color(0, 0, 0));
                        QDPlatform_SetPixel(x + offsetX, y + offsetY, color);
                    }
                }
            }
        } else if (verb == frame) {
            /* The outline lies inside the rectangle, the pen's width thick
             * (Inside Macintosh: Imaging With QuickDraw, 3-60): four bands
             * that do not overlap. It was four lines that shared their
             * corner pixels - drawn twice, so in XOR mode the corners
             * vanished - each hanging the pen outside the rectangle. */
            SInt16 mode = port ? port->pnMode : patCopy;
            SInt32 pw = (port && port->pnSize.h > 0) ? port->pnSize.h : 1;
            SInt32 ph = (port && port->pnSize.v > 0) ? port->pnSize.v : 1;
            SInt32 L = rect->left, T = rect->top, R = rect->right, B = rect->bottom;
            if (R - L <= 2 * pw || B - T <= 2 * ph) {
                /* Too small to have a hole: it is all pen */
                for (SInt32 y = T; y < B; y++)
                    for (SInt32 x = L; x < R; x++) QD_PenPixel(port, pat, mode, x, y);
            } else {
                for (SInt32 y = T; y < T + ph; y++)
                    for (SInt32 x = L; x < R; x++) QD_PenPixel(port, pat, mode, x, y);
                for (SInt32 y = B - ph; y < B; y++)
                    for (SInt32 x = L; x < R; x++) QD_PenPixel(port, pat, mode, x, y);
                for (SInt32 y = T + ph; y < B - ph; y++) {
                    for (SInt32 x = L; x < L + pw; x++) QD_PenPixel(port, pat, mode, x, y);
                    for (SInt32 x = R - pw; x < R; x++) QD_PenPixel(port, pat, mode, x, y);
                }
            }
        } else if (verb == erase) {
            /* Erase should use port's background pattern, NOT desktop pattern */
            if (pat) {
                /* Use 1-bit pattern with port background color */
                for (SInt32 y = rect->top; y < rect->bottom; y++) {
                    for (SInt32 x = rect->left; x < rect->right; x++) {
                        UInt32 color = QDPlatform_SelectPatternColor(port, pat, x, y,
                                                                      pack_color(255, 255, 255));
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
                        color = QDPlatform_SelectPatternColor(port, pat, x, y, fallbackColor);
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
                        UInt32 color = QDPlatform_SelectPatternColor(port, pat, x, y,
                                                                      pack_color(0, 0, 0));
                        /* Apply pen size: also fill nearby pixels for thick outlines */
                        SInt16 penW = port ? (port->pnSize.h > 1 ? port->pnSize.h : 1) : 1;
                        SInt16 penH = port ? (port->pnSize.v > 1 ? port->pnSize.v : 1) : 1;
                        for (SInt16 py = 0; py < penH; py++) {
                            for (SInt16 px = 0; px < penW; px++) {
                                SInt32 dx = x + px + offsetX;
                                SInt32 dy = y + py + offsetY;
                                if (dx >= 0 && dx < (SInt32)fb_width &&
                                    dy >= 0 && dy < (SInt32)fb_height) {
                                    QDPlatform_SetPixel(dx, dy, color);
                                }
                            }
                        }
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

                    UInt32 color = pat ? QDPlatform_SelectPatternColor(port, pat, x, y, fallbackColor)
                                       : fallbackColor;

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
            /* The band between the outer shape and the same shape inset by the
             * pen. Testing "inside, with a neighbour outside" instead would
             * always give a one-pixel outline no matter what PenSize said, and
             * the default-button ring - System 7 draws it three pixels thick -
             * came out hairline because of it. Rect and oval framing already
             * consult pnSize; this is the case that did not. */
            SInt16 penW = (port && port->pnSize.h > 1) ? port->pnSize.h : 1;
            SInt16 penH = (port && port->pnSize.v > 1) ? port->pnSize.v : 1;

            Rect innerRect;
            innerRect.left   = rect->left + penW;
            innerRect.top    = rect->top + penH;
            innerRect.right  = rect->right - penW;
            innerRect.bottom = rect->bottom - penH;

            SInt16 innerRadiusH = radiusH - penW;
            SInt16 innerRadiusV = radiusV - penH;
            if (innerRadiusH < 0) innerRadiusH = 0;
            if (innerRadiusV < 0) innerRadiusV = 0;

            Boolean hasInterior = (innerRect.right > innerRect.left &&
                                   innerRect.bottom > innerRect.top);

            for (SInt32 y = rect->top; y < rect->bottom; y++) {
                if (y < 0 || y >= (SInt32)fb_height) continue;
                for (SInt32 x = rect->left; x < rect->right; x++) {
                    if (x < 0 || x >= (SInt32)fb_width) continue;
                    if (!QDPointInRoundRect(x, y, rect, radiusH, radiusV)) {
                        continue;
                    }
                    if (hasInterior &&
                        QDPointInRoundRect(x, y, &innerRect, innerRadiusH, innerRadiusV)) {
                        continue;
                    }

                    UInt32 color = QDPlatform_SelectPatternColor(port, pat, x, y,
                                                                  pack_color(0, 0, 0));

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

                    UInt32 color = pat ? QDPlatform_SelectPatternColor(port, pat, x, y, fallbackColor)
                                       : fallbackColor;

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

                    UInt32 color = QDPlatform_SelectPatternColor(port, pat, x, y,
                                                                  pack_color(0, 0, 0));

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

void QDPlatform_DrawShape(GrafPtr port, GrafVerb verb, const Rect* rect,
                         SInt16 shapeType, const Pattern* pat,
                         SInt16 ovalWidth, SInt16 ovalHeight) {
    extern GrafPtr g_currentPort;
    QD_ClipBegin(port);
    QDPlatform_DrawShape_Body(port, verb, rect, shapeType, pat, ovalWidth, ovalHeight);
    QD_ClipEnd();
}

/* Polygon fill using scanline algorithm */
static void QDPlatform_FillPoly_Body(GrafPtr port, PolyHandle poly, const Pattern* pat,
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
    Pointer_Shield(bbox.left, bbox.top, bbox.right, bbox.bottom);

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
                        /* Apply pattern with port colors */
                        UInt32 fallback = (verb == erase) ? pack_color(255, 255, 255)
                                                          : pack_color(0, 0, 0);
                        color = pat ? QDPlatform_SelectPatternColor(port, pat, x, y, fallback)
                                    : fallback;

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

void QDPlatform_FillPoly(GrafPtr port, PolyHandle poly, const Pattern* pat,
                        SInt16 mode, GrafVerb verb) {
    extern GrafPtr g_currentPort;
    QD_ClipBegin(port);
    QDPlatform_FillPoly_Body(port, poly, pat, mode, verb);
    QD_ClipEnd();
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
/* Erase a region in the current port: the desktop's colour pattern where
 * the Pattern Manager has one, else EraseRect. Only erasing comes here now;
 * Regions.c draws the other verbs rectangle by rectangle. */
static void QDPlatform_DrawRegion_Body(RgnHandle rgn, short mode, const Pattern* pat) {
    (void)pat;
    if (mode != erase || !rgn || !*rgn || !framebuffer) return;

    extern bool PM_GetColorPattern(uint32_t** patternData);
    extern void EraseRect(const Rect* r);
    extern GrafPtr g_currentPort;
    uint32_t* colorPattern = NULL;
    /* The desktop's pattern is for the desktop: a window erases to its own
     * background, through EraseRect. */
    extern Boolean WM_PortVisibleRgn(GrafPtr port, RgnHandle out);
    static RgnHandle probe = NULL;
    if (!probe) probe = NewRgn();
    Boolean isWindow = g_currentPort && probe && WM_PortVisibleRgn(g_currentPort, probe);
    Boolean colour = !isWindow && PM_GetColorPattern(&colorPattern) && g_currentPort &&
                     g_currentPort->portBits.baseAddr == (Ptr)framebuffer;
    SInt16 dh = colour ? g_currentPort->portBits.bounds.left - g_currentPort->portRect.left : 0;
    SInt16 dv = colour ? g_currentPort->portBits.bounds.top - g_currentPort->portRect.top : 0;

    Region* region = *rgn;
    SInt16 n = (region->rgnSize <= 10) ? (EmptyRect(&region->rgnBBox) ? 0 : 1)
                                       : *(SInt16*)((UInt8*)region + 10);
    for (SInt16 k = 0; k < n; k++) {
        region = *rgn;
        Rect r = (region->rgnSize <= 10) ? region->rgnBBox
                                         : ((const Rect*)((UInt8*)region + 12))[k];
        if (!colour) {
            EraseRect(&r);
            continue;
        }
        OffsetRect(&r, dh, dv);   /* local to global */
        int left = r.left < 0 ? 0 : r.left, top = r.top < 0 ? 0 : r.top;
        int right = r.right > (int)fb_width ? (int)fb_width : r.right;
        int bottom = r.bottom > (int)fb_height ? (int)fb_height : r.bottom;
        Pointer_Shield(left, top, right, bottom);
        for (int y = top; y < bottom; y++) {
            for (int x = left; x < right; x++) {
                if (!QD_ClipHas(x, y)) continue;
                uint32_t c = colorPattern[(y & 7) * 8 + (x & 7)];
                *(uint32_t*)((uint8_t*)framebuffer + y * fb_pitch + x * 4) =
                    pack_color((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
            }
        }
    }
}

/*
 * QD_FillRectColorPattern - fill a rectangle of the current port with an 8 by
 * 8 colour pattern, pixels already in screen format (as the Pattern Manager
 * decodes a 'ppat'). Aligned to the port's local coordinates, as QuickDraw
 * aligns patterns, and clipped like any other drawing. Desktop Patterns shows
 * its preview with it.
 */
void QD_FillRectColorPattern(const Rect* r, const uint32_t pattern[64]) {
    extern GrafPtr g_currentPort;
    if (!r || !pattern || !g_currentPort) return;
    GrafPtr port = g_currentPort;
    QD_ClipBegin(port);
    for (SInt16 v = r->top; v < r->bottom; v++) {
        for (SInt16 h = r->left; h < r->right; h++) {
            short px, py;
            QD_LocalToPixel(h, v, &px, &py);
            QDPlatform_SetPixel(px, py, pattern[(v & 7) * 8 + (h & 7)]);
        }
    }
    QD_ClipEnd();
}

void QDPlatform_DrawRegion(RgnHandle rgn, short mode, const Pattern* pat) {
    extern GrafPtr g_currentPort;
    QD_ClipBegin(g_currentPort);
    QDPlatform_DrawRegion_Body(rgn, mode, pat);
    QD_ClipEnd();
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
static SInt16 QDPlatform_DrawGlyph_Body(struct FontStrike *strike, UInt8 ch, SInt16 x, SInt16 y,
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
            /* local plus bounds, as QD_LocalToPixel */
            pixelX = x + port->portBits.bounds.left;
            pixelY = y + port->portBits.bounds.top;
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

            if (GetBitmapBit(bitmap, bitPos) &&
                (renderBuffer != (Ptr)framebuffer || QD_ClipHas(pixelX + col, pixelY + row))) {
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

SInt16 QDPlatform_DrawGlyph(struct FontStrike *strike, UInt8 ch, SInt16 x, SInt16 y,
                            GrafPtr port, UInt32 color) {
    QD_ClipBegin(port);
    SInt16 advance = QDPlatform_DrawGlyph_Body(strike, ch, x, y, port, color);
    QD_ClipEnd();
    return advance;
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
static void QDPlatform_DrawGlyphBitmap_Body(GrafPtr port, Point pen,
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

    Pointer_Shield(pen.h, pen.v, pen.h + width, pen.v + height);

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

    /* Determine foreground color from port's foreground color setting */
    uint32_t fgColor;
    if (port) {
        fgColor = QDPlatform_MapQDColor(port->fgColor);
    } else {
        fgColor = pack_color(0, 0, 0);  /* Black default */
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

void QDPlatform_DrawGlyphBitmap(GrafPtr port, Point pen,
                         const uint8_t *bitmap,
                         SInt16 width, SInt16 height,
                         const Pattern *pattern, SInt16 mode) {
    extern GrafPtr g_currentPort;
    QD_ClipBegin(port);
    QDPlatform_DrawGlyphBitmap_Body(port, pen, bitmap, width, height, pattern, mode);
    QD_ClipEnd();
}

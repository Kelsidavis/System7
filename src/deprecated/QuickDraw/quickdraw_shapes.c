/*
#include "QuickDraw/QuickDrawInternal.h"
 * QuickDraw Rounded Rectangle Functions
 * Implements PaintRoundRect and FrameRoundRect for System 7 window chrome.
 */

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawPlatform.h"
#include "QuickDraw/quickdraw_types.h"
#include "SystemTypes.h"
#include "QuickDraw/QDLogging.h"

/* Global current port - from QuickDrawCore.c */
extern GrafPtr g_currentPort;
#define thePort g_currentPort

/*
 * FrameRoundRect - Draw rounded rectangle outline
 * Trap: 0xA8B0
 */
void FrameRoundRect(const Rect* r, short ovalWidth, short ovalHeight)
{
    if (thePort == NULL || r == NULL) return;

    QD_LOG_TRACE("FrameRoundRect (%d,%d,%d,%d) radius=(%d,%d)\n",
                r->left, r->top, r->right, r->bottom, ovalWidth, ovalHeight);

    /* Get framebuffer info */
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;

    if (!framebuffer) {
        QD_LOG_WARN("FrameRoundRect framebuffer unavailable\n");
        return;
    }

    /* Validate framebuffer configuration */
    if (fb_pitch == 0) {
        QD_LOG_WARN("FrameRoundRect invalid fb_pitch (zero)\n");
        return;
    }

    uint32_t* fb = (uint32_t*)framebuffer;
    int pitch = fb_pitch / 4;

    /* Clip to screen bounds */
    short left = r->left;
    short top = r->top;
    short right = r->right;
    short bottom = r->bottom;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (short)fb_width) right = fb_width;
    if (bottom > (short)fb_height) bottom = fb_height;
    if (left >= right || top >= bottom) return;

    /* Get pen pattern color (approximation - use black if pattern has any bits set) */
    uint32_t color = 0xFF000000;  /* Black */

    /* Calculate corner radius (simplified - use ovalWidth/2 as radius) */
    short radius = ovalWidth / 2;
    if (radius > (right - left) / 2) radius = (right - left) / 2;
    if (radius > (bottom - top) / 2) radius = (bottom - top) / 2;

    /* Draw straight edges */
    /* Top edge */
    for (int x = left + radius; x < right - radius; x++) {
        if (x >= 0 && x < (short)fb_width && top >= 0 && top < (short)fb_height) {
            fb[top * pitch + x] = color;
        }
    }

    /* Bottom edge */
    for (int x = left + radius; x < right - radius; x++) {
        if (x >= 0 && x < (short)fb_width && (bottom - 1) >= 0 && (bottom - 1) < (short)fb_height) {
            fb[(bottom - 1) * pitch + x] = color;
        }
    }

    /* Left edge */
    for (int y = top + radius; y < bottom - radius; y++) {
        if (left >= 0 && left < (short)fb_width && y >= 0 && y < (short)fb_height) {
            fb[y * pitch + left] = color;
        }
    }

    /* Right edge */
    for (int y = top + radius; y < bottom - radius; y++) {
        if ((right - 1) >= 0 && (right - 1) < (short)fb_width && y >= 0 && y < (short)fb_height) {
            fb[y * pitch + (right - 1)] = color;
        }
    }

    /* Draw corners using simple circle approximation */
    for (int dy = 0; dy < radius; dy++) {
        int dx = 0;
        /* Simple Pythagorean approximation for rounded corners */
        while (dx * dx + dy * dy < radius * radius) dx++;

        /* Top-left corner */
        int x1 = left + radius - dx;
        int y1 = top + radius - dy;
        if (x1 >= 0 && x1 < (short)fb_width && y1 >= 0 && y1 < (short)fb_height) {
            fb[y1 * pitch + x1] = color;
        }

        /* Top-right corner */
        int x2 = right - radius + dx - 1;
        if (x2 >= 0 && x2 < (short)fb_width && y1 >= 0 && y1 < (short)fb_height) {
            fb[y1 * pitch + x2] = color;
        }

        /* Bottom-left corner */
        int y2 = bottom - radius + dy - 1;
        if (x1 >= 0 && x1 < (short)fb_width && y2 >= 0 && y2 < (short)fb_height) {
            fb[y2 * pitch + x1] = color;
        }

        /* Bottom-right corner */
        if (x2 >= 0 && x2 < (short)fb_width && y2 >= 0 && y2 < (short)fb_height) {
            fb[y2 * pitch + x2] = color;
        }
    }
}

/*
 * PaintRoundRect - Fill rounded rectangle with current pattern
 * Trap: 0xA8B1
 */
void PaintRoundRect(const Rect* r, short ovalWidth, short ovalHeight)
{
    if (thePort == NULL || r == NULL) return;

    QD_LOG_TRACE("PaintRoundRect (%d,%d,%d,%d) radius=(%d,%d)\n",
                r->left, r->top, r->right, r->bottom, ovalWidth, ovalHeight);

    /* Get framebuffer info */
    extern void* framebuffer;
    extern uint32_t fb_width, fb_height, fb_pitch;

    if (!framebuffer) {
        QD_LOG_WARN("PaintRoundRect framebuffer unavailable\n");
        return;
    }

    /* Validate framebuffer configuration */
    if (fb_pitch == 0) {
        QD_LOG_WARN("PaintRoundRect invalid fb_pitch (zero)\n");
        return;
    }

    uint32_t* fb = (uint32_t*)framebuffer;
    int pitch = fb_pitch / 4;

    /* Get current pen pattern */
    const Pattern* pat = &thePort->pnPat;

    /* Clip to screen bounds */
    short left = r->left;
    short top = r->top;
    short right = r->right;
    short bottom = r->bottom;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (short)fb_width) right = fb_width;
    if (bottom > (short)fb_height) bottom = fb_height;
    if (left >= right || top >= bottom) return;

    /* Calculate corner radius */
    short radius = ovalWidth / 2;
    if (radius > (right - left) / 2) radius = (right - left) / 2;
    if (radius > (bottom - top) / 2) radius = (bottom - top) / 2;

    extern UInt32 QDPlatform_MapQDColor(SInt32 qdColor);
    UInt32 fgColor = QDPlatform_MapQDColor(thePort->fgColor);
    UInt32 bgColor = QDPlatform_MapQDColor(thePort->bkColor);

    /* Fill the rounded rectangle with pattern */
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            /* Check if point is inside rounded rectangle */
            Boolean inside = 1;

            /* Check if we're in a corner region */
            if (x < left + radius && y < top + radius) {
                /* Top-left corner */
                int dx = (left + radius) - x;
                int dy = (top + radius) - y;
                if (dx * dx + dy * dy > radius * radius) inside = 0;
            } else if (x >= right - radius && y < top + radius) {
                /* Top-right corner */
                int dx = x - (right - radius - 1);
                int dy = (top + radius) - y;
                if (dx * dx + dy * dy > radius * radius) inside = 0;
            } else if (x < left + radius && y >= bottom - radius) {
                /* Bottom-left corner */
                int dx = (left + radius) - x;
                int dy = y - (bottom - radius - 1);
                if (dx * dx + dy * dy > radius * radius) inside = 0;
            } else if (x >= right - radius && y >= bottom - radius) {
                /* Bottom-right corner */
                int dx = x - (right - radius - 1);
                int dy = y - (bottom - radius - 1);
                if (dx * dx + dy * dy > radius * radius) inside = 0;
            }

            if (inside) {
                /* Apply 8x8 pattern */
                int patY = y & 7;
                int patX = x & 7;
                unsigned char patByte = pat->pat[patY];
                Boolean bitOn = (patByte & (0x80 >> patX)) != 0;
                fb[y * pitch + x] = bitOn ? fgColor : bgColor;
            }
        }
    }
}

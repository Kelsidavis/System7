#include <stdint.h>

#include "QuickDraw/DisplayBezel.h"
#include "Gestalt/Gestalt.h"

extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

#ifndef DEFAULT_BEZEL_STYLE
#define DEFAULT_BEZEL_STYLE 0
#endif

static QDBezelMode gConfiguredBezelMode =
#if DEFAULT_BEZEL_STYLE == 1
    kQDBezelRounded;
#elif DEFAULT_BEZEL_STYLE == 2
    kQDBezelFlat;
#else
    kQDBezelAuto;
#endif

void QD_SetBezelMode(QDBezelMode mode) {
    gConfiguredBezelMode = mode;
}

QDBezelMode QD_GetBezelMode(void) {
    return gConfiguredBezelMode;
}

static int compute_corner_radius(void) {
    int radius = (int)(fb_height / 55);
    if (radius < 4) {
        radius = 4;
    }
    if (radius > 12) {
        radius = 12;
    }
    return radius;
}

void QD_DrawCRTBezel(void) {
    QDBezelMode effectiveMode = gConfiguredBezelMode;
    if (effectiveMode == kQDBezelAuto) {
        UInt16 machine = Gestalt_GetMachineType();
        if (machine == 0x0196) { /* NewWorld default */
            effectiveMode = kQDBezelFlat;
        } else {
            effectiveMode = kQDBezelRounded;
        }
    }

    if (effectiveMode == kQDBezelFlat) {
        return;
    }

    if (!framebuffer || fb_width == 0 || fb_height == 0 || fb_pitch == 0) {
        return;
    }

    uint32_t* fb = (uint32_t*)framebuffer;
    int pitchPixels = fb_pitch / 4;
    const uint32_t black = 0xFF000000;

    int cornerRadius = compute_corner_radius();
    int radiusSquared = cornerRadius * cornerRadius;
    int topCenterY = cornerRadius - 1;
    int bottomCenterY = (int)fb_height - cornerRadius;
    int leftCenterX = cornerRadius - 1;
    int rightCenterX = (int)fb_width - cornerRadius;

    for (int y = 0; y < cornerRadius; ++y) {
        uint32_t* row = fb + y * pitchPixels;
        int dyTop = y - topCenterY;
        int yBottom = (int)fb_height - 1 - y;
        int dyBottom = yBottom - bottomCenterY;

        for (int x = 0; x < cornerRadius; ++x) {
            int dx = x - leftCenterX;
            if (dx * dx + dyTop * dyTop >= radiusSquared) {
                row[x] = black;
            }
        }
        for (int x = (int)fb_width - cornerRadius; x < (int)fb_width; ++x) {
            int dx = x - rightCenterX;
            if (dx * dx + dyTop * dyTop >= radiusSquared) {
                row[x] = black;
            }
        }

        uint32_t* bottomRow = fb + yBottom * pitchPixels;
        for (int x = 0; x < cornerRadius; ++x) {
            int dx = x - leftCenterX;
            if (dx * dx + dyBottom * dyBottom >= radiusSquared) {
                bottomRow[x] = black;
            }
        }
        for (int x = (int)fb_width - cornerRadius; x < (int)fb_width; ++x) {
            int dx = x - rightCenterX;
            if (dx * dx + dyBottom * dyBottom >= radiusSquared) {
                bottomRow[x] = black;
            }
        }
    }
}

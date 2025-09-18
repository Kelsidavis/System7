/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * ColorManager_HAL.c - Hardware Abstraction Layer for Color Manager
 * Platform-specific color management implementation
 */

#include "ColorManager/ColorTypes.h"
#include "ColorManager/ColorManager_HAL.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

/* Platform-specific color context */
typedef struct ColorContext {
#ifdef __APPLE__
    CGColorSpaceRef colorSpace;
    CGDirectDisplayID mainDisplay;
    CFDictionaryRef displayMode;
#elif defined(__linux__)
    Display *display;
    int screen;
    Colormap colormap;
    Visual *visual;
#endif
    uint32_t displayDepth;
    Boolean hasHardwareAccel;
    Boolean supportsWideGamut;
} ColorContext;

static ColorContext gColorContext;
static Boolean gHALInitialized = false;

/* Initialize HAL */
OSErr ColorManager_HAL_Init(void) {
    if (gHALInitialized) {
        return noErr;
    }

    memset(&gColorContext, 0, sizeof(ColorContext));

#ifdef __APPLE__
    /* Initialize Core Graphics color space */
    gColorContext.colorSpace = CGColorSpaceCreateDeviceRGB();
    if (gColorContext.colorSpace == NULL) {
        return memFullErr;
    }

    /* Get main display information */
    gColorContext.mainDisplay = CGMainDisplayID();
    gColorContext.displayMode = CGDisplayCopyDisplayMode(gColorContext.mainDisplay);

    /* Get display depth */
    size_t bitsPerPixel = CGDisplayBitsPerPixel(gColorContext.mainDisplay);
    gColorContext.displayDepth = (uint32_t)bitsPerPixel;

    /* Check for hardware acceleration */
    gColorContext.hasHardwareAccel = CGDisplayUsesOpenGLAcceleration(gColorContext.mainDisplay);

    /* Check for wide gamut support (P3, etc) */
    if (@available(macOS 10.12, *)) {
        CFStringRef colorSpaceName = CGColorSpaceCopyName(gColorContext.colorSpace);
        gColorContext.supportsWideGamut = (colorSpaceName != NULL &&
            CFStringCompare(colorSpaceName, kCGColorSpaceDisplayP3, 0) == kCFCompareEqualTo);
        if (colorSpaceName) CFRelease(colorSpaceName);
    }

#elif defined(__linux__)
    /* Open X11 display */
    gColorContext.display = XOpenDisplay(NULL);
    if (gColorContext.display == NULL) {
        return memFullErr;
    }

    gColorContext.screen = DefaultScreen(gColorContext.display);
    gColorContext.visual = DefaultVisual(gColorContext.display, gColorContext.screen);
    gColorContext.colormap = DefaultColormap(gColorContext.display, gColorContext.screen);

    /* Get display depth */
    gColorContext.displayDepth = DefaultDepth(gColorContext.display, gColorContext.screen);

    /* Check for hardware acceleration (simplified) */
    gColorContext.hasHardwareAccel = (gColorContext.displayDepth >= 24);

    /* Wide gamut not typically supported on X11 */
    gColorContext.supportsWideGamut = false;
#endif

    gHALInitialized = true;
    return noErr;
}

/* Cleanup HAL */
void ColorManager_HAL_Cleanup(void) {
    if (!gHALInitialized) {
        return;
    }

#ifdef __APPLE__
    if (gColorContext.colorSpace) {
        CGColorSpaceRelease(gColorContext.colorSpace);
        gColorContext.colorSpace = NULL;
    }
    if (gColorContext.displayMode) {
        CFRelease(gColorContext.displayMode);
        gColorContext.displayMode = NULL;
    }
#elif defined(__linux__)
    if (gColorContext.display) {
        XCloseDisplay(gColorContext.display);
        gColorContext.display = NULL;
    }
#endif

    gHALInitialized = false;
}

/* Convert RGB to platform native color */
uint32_t ColorManager_HAL_RGBToNative(const RGBColor *rgb) {
    if (rgb == NULL) {
        return 0;
    }

    /* Convert 16-bit Mac RGB to 8-bit components */
    uint8_t r = (rgb->red >> 8) & 0xFF;
    uint8_t g = (rgb->green >> 8) & 0xFF;
    uint8_t b = (rgb->blue >> 8) & 0xFF;

#ifdef __APPLE__
    /* Create native CGColor */
    CGFloat components[4] = {
        r / 255.0f,
        g / 255.0f,
        b / 255.0f,
        1.0f  /* alpha */
    };

    /* Return as packed ARGB */
    return (0xFF000000 | (r << 16) | (g << 8) | b);

#elif defined(__linux__)
    /* Create X11 color */
    if (gColorContext.display && gColorContext.displayDepth >= 24) {
        /* TrueColor visual - pack as RGB */
        return (r << 16) | (g << 8) | b;
    } else if (gColorContext.display) {
        /* Indexed color - find closest match */
        XColor color;
        color.red = rgb->red;
        color.green = rgb->green;
        color.blue = rgb->blue;
        color.flags = DoRed | DoGreen | DoBlue;

        if (XAllocColor(gColorContext.display, gColorContext.colormap, &color)) {
            return color.pixel;
        }
    }
#endif

    /* Default: pack as RGB */
    return (r << 16) | (g << 8) | b;
}

/* Convert native color to RGB */
void ColorManager_HAL_NativeToRGB(uint32_t native, RGBColor *rgb) {
    if (rgb == NULL) {
        return;
    }

#ifdef __APPLE__
    /* Unpack from ARGB */
    rgb->red = ((native >> 16) & 0xFF) * 257;
    rgb->green = ((native >> 8) & 0xFF) * 257;
    rgb->blue = (native & 0xFF) * 257;

#elif defined(__linux__)
    if (gColorContext.display && gColorContext.displayDepth >= 24) {
        /* TrueColor visual - unpack RGB */
        rgb->red = ((native >> 16) & 0xFF) * 257;
        rgb->green = ((native >> 8) & 0xFF) * 257;
        rgb->blue = (native & 0xFF) * 257;
    } else if (gColorContext.display) {
        /* Indexed color - query colormap */
        XColor color;
        color.pixel = native;
        XQueryColor(gColorContext.display, gColorContext.colormap, &color);
        rgb->red = color.red;
        rgb->green = color.green;
        rgb->blue = color.blue;
    } else {
        /* Default unpack */
        rgb->red = ((native >> 16) & 0xFF) * 257;
        rgb->green = ((native >> 8) & 0xFF) * 257;
        rgb->blue = (native & 0xFF) * 257;
    }
#else
    /* Default: unpack RGB */
    rgb->red = ((native >> 16) & 0xFF) * 257;
    rgb->green = ((native >> 8) & 0xFF) * 257;
    rgb->blue = (native & 0xFF) * 257;
#endif
}

/* Get display color depth */
uint32_t ColorManager_HAL_GetDisplayDepth(void) {
    return gColorContext.displayDepth;
}

/* Check if hardware acceleration is available */
Boolean ColorManager_HAL_HasHardwareAccel(void) {
    return gColorContext.hasHardwareAccel;
}

/* Check if wide gamut is supported */
Boolean ColorManager_HAL_SupportsWideGamut(void) {
    return gColorContext.supportsWideGamut;
}

/* Perform gamma correction */
void ColorManager_HAL_GammaCorrect(RGBColor *rgb, float gamma) {
    if (rgb == NULL || gamma <= 0) {
        return;
    }

    /* Normalize to 0-1 range */
    float r = rgb->red / 65535.0f;
    float g = rgb->green / 65535.0f;
    float b = rgb->blue / 65535.0f;

    /* Apply gamma correction */
    r = powf(r, gamma);
    g = powf(g, gamma);
    b = powf(b, gamma);

    /* Convert back to 16-bit */
    rgb->red = (uint16_t)(r * 65535);
    rgb->green = (uint16_t)(g * 65535);
    rgb->blue = (uint16_t)(b * 65535);
}

/* Convert RGB to HSV */
void ColorManager_HAL_RGBToHSV(const RGBColor *rgb, float *h, float *s, float *v) {
    if (rgb == NULL || h == NULL || s == NULL || v == NULL) {
        return;
    }

    /* Normalize RGB to 0-1 range */
    float r = rgb->red / 65535.0f;
    float g = rgb->green / 65535.0f;
    float b = rgb->blue / 65535.0f;

    float max = fmaxf(fmaxf(r, g), b);
    float min = fminf(fminf(r, g), b);
    float delta = max - min;

    /* Value */
    *v = max;

    /* Saturation */
    if (max != 0) {
        *s = delta / max;
    } else {
        *s = 0;
        *h = -1;
        return;
    }

    /* Hue */
    if (r == max) {
        *h = (g - b) / delta;       /* between yellow & magenta */
    } else if (g == max) {
        *h = 2 + (b - r) / delta;   /* between cyan & yellow */
    } else {
        *h = 4 + (r - g) / delta;   /* between magenta & cyan */
    }

    *h *= 60;   /* degrees */
    if (*h < 0) {
        *h += 360;
    }
}

/* Convert HSV to RGB */
void ColorManager_HAL_HSVToRGB(float h, float s, float v, RGBColor *rgb) {
    if (rgb == NULL) {
        return;
    }

    int i;
    float f, p, q, t;
    float r, g, b;

    if (s == 0) {
        /* Achromatic (grey) */
        r = g = b = v;
    } else {
        h /= 60;   /* sector 0 to 5 */
        i = (int)floor(h);
        f = h - i;   /* factorial part of h */
        p = v * (1 - s);
        q = v * (1 - s * f);
        t = v * (1 - s * (1 - f));

        switch(i) {
            case 0:
                r = v; g = t; b = p;
                break;
            case 1:
                r = q; g = v; b = p;
                break;
            case 2:
                r = p; g = v; b = t;
                break;
            case 3:
                r = p; g = q; b = v;
                break;
            case 4:
                r = t; g = p; b = v;
                break;
            default:  /* case 5: */
                r = v; g = p; b = q;
                break;
        }
    }

    /* Convert to 16-bit RGB */
    rgb->red = (uint16_t)(r * 65535);
    rgb->green = (uint16_t)(g * 65535);
    rgb->blue = (uint16_t)(b * 65535);
}

/* Convert RGB to grayscale using luminance weights */
uint16_t ColorManager_HAL_RGBToGrayscale(const RGBColor *rgb) {
    if (rgb == NULL) {
        return 0;
    }

    /* Use ITU-R BT.709 luminance weights */
    float luminance = 0.2126f * (rgb->red / 65535.0f) +
                     0.7152f * (rgb->green / 65535.0f) +
                     0.0722f * (rgb->blue / 65535.0f);

    return (uint16_t)(luminance * 65535);
}

/* Blend two colors */
void ColorManager_HAL_BlendColors(const RGBColor *src, const RGBColor *dst,
                                 RGBColor *result, float alpha) {
    if (src == NULL || dst == NULL || result == NULL) {
        return;
    }

    /* Clamp alpha to 0-1 range */
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;

    /* Linear interpolation */
    result->red = (uint16_t)(src->red * alpha + dst->red * (1 - alpha));
    result->green = (uint16_t)(src->green * alpha + dst->green * (1 - alpha));
    result->blue = (uint16_t)(src->blue * alpha + dst->blue * (1 - alpha));
}

/* Apply color matrix transformation */
void ColorManager_HAL_ApplyColorMatrix(RGBColor *rgb, const float matrix[9]) {
    if (rgb == NULL || matrix == NULL) {
        return;
    }

    /* Normalize to 0-1 range */
    float r = rgb->red / 65535.0f;
    float g = rgb->green / 65535.0f;
    float b = rgb->blue / 65535.0f;

    /* Apply 3x3 matrix */
    float newR = matrix[0] * r + matrix[1] * g + matrix[2] * b;
    float newG = matrix[3] * r + matrix[4] * g + matrix[5] * b;
    float newB = matrix[6] * r + matrix[7] * g + matrix[8] * b;

    /* Clamp to 0-1 range */
    if (newR < 0) newR = 0;
    if (newR > 1) newR = 1;
    if (newG < 0) newG = 0;
    if (newG > 1) newG = 1;
    if (newB < 0) newB = 0;
    if (newB > 1) newB = 1;

    /* Convert back to 16-bit */
    rgb->red = (uint16_t)(newR * 65535);
    rgb->green = (uint16_t)(newG * 65535);
    rgb->blue = (uint16_t)(newB * 65535);
}

/* Get system color palette */
OSErr ColorManager_HAL_GetSystemPalette(CTabHandle *palette) {
    if (palette == NULL) {
        return paramErr;
    }

    /* Create a standard 256-color palette */
    *palette = GetCTable(0);  /* This calls into ColorManager.c */

    if (*palette == NULL) {
        return memFullErr;
    }

    /* Optionally modify palette based on system preferences */
#ifdef __APPLE__
    /* Could query system colors here */
#elif defined(__linux__)
    /* Could query X11 colormap here */
#endif

    return noErr;
}

/* Set display gamma */
OSErr ColorManager_HAL_SetDisplayGamma(float gamma) {
    if (gamma <= 0) {
        return paramErr;
    }

#ifdef __APPLE__
    /* Use Core Graphics to set display gamma */
    CGGammaValue redMin, redMax, redGamma;
    CGGammaValue greenMin, greenMax, greenGamma;
    CGGammaValue blueMin, blueMax, blueGamma;

    CGGetDisplayTransferByFormula(gColorContext.mainDisplay,
        &redGamma, &redMin, &redMax,
        &greenGamma, &greenMin, &greenMax,
        &blueGamma, &blueMin, &blueMax);

    CGSetDisplayTransferByFormula(gColorContext.mainDisplay,
        redMin, redMax, gamma,
        greenMin, greenMax, gamma,
        blueMin, blueMax, gamma);

    return noErr;
#else
    /* Gamma adjustment not available on this platform */
    return unimpErr;
#endif
}

/* Dither RGB color for lower bit depths */
void ColorManager_HAL_DitherColor(const RGBColor *src, RGBColor *dst, uint32_t targetDepth) {
    if (src == NULL || dst == NULL) {
        return;
    }

    /* Simple ordered dithering */
    uint32_t mask = (1 << (16 - targetDepth)) - 1;

    dst->red = src->red & ~mask;
    dst->green = src->green & ~mask;
    dst->blue = src->blue & ~mask;

    /* Add dither noise */
    if (targetDepth < 16) {
        uint32_t ditherAmount = 1 << (15 - targetDepth);
        dst->red += (rand() % ditherAmount);
        dst->green += (rand() % ditherAmount);
        dst->blue += (rand() % ditherAmount);

        /* Clamp values */
        if (dst->red > 0xFFFF) dst->red = 0xFFFF;
        if (dst->green > 0xFFFF) dst->green = 0xFFFF;
        if (dst->blue > 0xFFFF) dst->blue = 0xFFFF;
    }
}
/* #include "SystemTypes.h" */
#include <string.h>
// #include "CompatibilityFix.h" // Removed
/*
 * Patterns.c - QuickDraw Pattern Operations Implementation
 *
 * Implementation of pattern fills, dithering, texture operations,
 * and pattern management for QuickDraw.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "QuickDrawConstants.h"

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDrawPlatform.h"


/* Standard patterns (8x8 pixel patterns) */
static const Pattern g_standardPatterns[] = {
    /* White pattern */
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},

    /* Black pattern */
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},

    /* Gray patterns */
    {{0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22}}, /* 25% gray */
    {{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}}, /* 50% gray */
    {{0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD}}, /* 75% gray */

    /* Diagonal patterns */
    {{0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}}, /* Diagonal lines */
    {{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}}, /* Reverse diagonal */

    /* Cross-hatch patterns */
    {{0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88, 0xFF}}, /* Horizontal lines */
    {{0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}}, /* Vertical lines */
    {{0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81}}, /* Cross-hatch */

    /* Dot patterns */
    {{0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00}}, /* Large dots */
    {{0x44, 0x00, 0x11, 0x00, 0x44, 0x00, 0x11, 0x00}}, /* Medium dots */
    {{0x22, 0x00, 0x08, 0x00, 0x22, 0x00, 0x08, 0x00}}, /* Small dots */

    /* Brick patterns */
    {{0xFF, 0x80, 0x80, 0x80, 0xFF, 0x08, 0x08, 0x08}}, /* Brick */
    {{0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88}}, /* Grid */

    /* Wave patterns */
    {{0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18}}  /* Diamond */
};

#define NUM_STANDARD_PATTERNS (sizeof(g_standardPatterns) / sizeof(g_standardPatterns[0]))

/* Dithering matrices for color quantization */
static const UInt8 g_ditherMatrix4x4[4][4] = {
    { 0, 8, 2, 10},
    {12, 4, 14, 6},
    { 3, 11, 1, 9},
    {15, 7, 13, 5}
};

static const UInt8 g_ditherMatrix8x8[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21}
};

/* Forward declarations */
static Boolean GetPatternPixel(ConstPatternParam pattern, SInt16 x, SInt16 y);
static void ApplyPatternToRect(const Rect *rect, ConstPatternParam pattern,
                              SInt16 mode, GrafPtr port);
static void DitherPixel(SInt16 x, SInt16 y, const RGBColor *color,
                       RGBColor *ditheredColor);
static Pattern CreateGrayPattern(UInt8 grayLevel);
static void ExpandPattern(ConstPatternParam srcPattern, Pattern *dstPattern,
                         SInt16 hStretch, SInt16 vStretch);

/* ================================================================
 * PATTERN OPERATIONS
 * ================================================================ */

void GetIndPattern(Pattern *thePat, SInt16 patternListID, SInt16 index) {
    assert(thePat != NULL);

    /* For now, ignore patternListID and use standard patterns */
    if (index >= 0 && index < NUM_STANDARD_PATTERNS) {
        *thePat = g_standardPatterns[index];
    } else {
        /* Default to 50% gray */
        *thePat = g_standardPatterns[3];
    }
}

Boolean PatternPixelValue(ConstPatternParam pattern, SInt16 x, SInt16 y) {
    assert(pattern != NULL);
    return GetPatternPixel(pattern, x, y);
}

void FillPatternRect(const Rect *rect, ConstPatternParam pattern, SInt16 mode) {
    extern GrafPtr g_currentPort; /* From QuickDrawCore.c */
    assert(rect != NULL);
    assert(pattern != NULL);

    if (g_currentPort) {
        ApplyPatternToRect(rect, pattern, mode, g_currentPort);
    }
}

/* ================================================================
 * PATTERN CREATION
 * ================================================================ */

Pattern MakeGrayPattern(UInt8 grayLevel) {
    return CreateGrayPattern(grayLevel);
}

Pattern MakeCheckerboardPattern(SInt16 checkerSize) {
    Pattern pattern;
    memset(pattern.pat, 0, 8);

    /* Create checkerboard pattern */
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Boolean bit = ((x / checkerSize) + (y / checkerSize)) % 2;
            if (bit) {
                pattern.pat[y] |= (0x80 >> x);
            }
        }
    }

    return pattern;
}

Pattern MakeDiagonalPattern(Boolean rising) {
    Pattern pattern;
    memset(pattern.pat, 0, 8);

    for (int y = 0; y < 8; y++) {
        int x = rising ? (7 - y) : y;
        pattern.pat[y] = 0x80 >> x;
    }

    return pattern;
}

/* ================================================================
 * PATTERN STRETCHING AND TRANSFORMATION
 * ================================================================ */

void StretchPattern(ConstPatternParam srcPattern, Pattern *dstPattern,
                   SInt16 hStretch, SInt16 vStretch) {
    assert(srcPattern != NULL);
    assert(dstPattern != NULL);

    ExpandPattern(srcPattern, dstPattern, hStretch, vStretch);
}

void RotatePattern(ConstPatternParam srcPattern, Pattern *dstPattern, SInt16 angle) {
    assert(srcPattern != NULL);
    assert(dstPattern != NULL);

    /* Simplified rotation - only support 90-degree increments */
    angle = (angle / 90) % 4;
    if (angle < 0) angle += 4;

    memset(dstPattern->pat, 0, 8);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Boolean srcBit = GetPatternPixel(srcPattern, x, y);
            if (srcBit) {
                int newX, newY;
                switch (angle) {
                    case 0: /* 0 degrees */
                        newX = x; newY = y;
                        break;
                    case 1: /* 90 degrees */
                        newX = 7 - y; newY = x;
                        break;
                    case 2: /* 180 degrees */
                        newX = 7 - x; newY = 7 - y;
                        break;
                    case 3: /* 270 degrees */
                        newX = y; newY = 7 - x;
                        break;
                    default:
                        newX = x; newY = y;
                        break;
                }
                dstPattern->pat[newY] |= (0x80 >> newX);
            }
        }
    }
}

void FlipPattern(ConstPatternParam srcPattern, Pattern *dstPattern,
                Boolean horizontal, Boolean vertical) {
    assert(srcPattern != NULL);
    assert(dstPattern != NULL);

    memset(dstPattern->pat, 0, 8);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Boolean srcBit = GetPatternPixel(srcPattern, x, y);
            if (srcBit) {
                int newX = horizontal ? (7 - x) : x;
                int newY = vertical ? (7 - y) : y;
                dstPattern->pat[newY] |= (0x80 >> newX);
            }
        }
    }
}

/* ================================================================
 * DITHERING OPERATIONS
 * ================================================================ */

void DitherColor(const RGBColor *color, SInt16 x, SInt16 y,
                RGBColor *ditheredColor, SInt16 ditherType) {
    assert(color != NULL);
    assert(ditheredColor != NULL);

    DitherPixel(x, y, color, ditheredColor);
}

void DitherRect(const Rect *rect, const RGBColor *color, SInt16 ditherType) {
    assert(rect != NULL);
    assert(color != NULL);

    for (SInt16 y = rect->top; y < rect->bottom; y++) {
        for (SInt16 x = rect->left; x < rect->right; x++) {
            RGBColor ditheredColor;
            DitherPixel(x, y, color, &ditheredColor);

            /* Set the dithered pixel */
            extern CGrafPtr g_currentCPort; /* From ColorQuickDraw.c */
            if (g_currentCPort) {
                QDPlatform_SetPixel(g_currentCPort, x, y, &ditheredColor);
            }
        }
    }
}

Pattern CreateDitheredPattern(const RGBColor *color) {
    assert(color != NULL);

    Pattern pattern;
    memset(pattern.pat, 0, 8);

    /* Create dithered pattern based on color intensity */
    UInt16 intensity = (color->red + color->green + color->blue) / 3;
    UInt8 threshold = (UInt8)(intensity >> 8); /* Convert to 8-bit */

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            UInt8 ditherValue = g_ditherMatrix8x8[y][x] * 4; /* Scale to 0-255 */
            if (threshold > ditherValue) {
                pattern.pat[y] |= (0x80 >> x);
            }
        }
    }

    return pattern;
}

/* ================================================================
 * PATTERN ANALYSIS
 * ================================================================ */

UInt8 CalculatePatternDensity(ConstPatternParam pattern) {
    assert(pattern != NULL);

    int setBits = 0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (GetPatternPixel(pattern, x, y)) {
                setBits++;
            }
        }
    }

    /* Return density as percentage (0-100) */
    return (UInt8)((setBits * 100) / 64);
}

Boolean IsUniformPattern(ConstPatternParam pattern) {
    assert(pattern != NULL);

    UInt8 firstByte = pattern->pat[0];
    for (int i = 1; i < 8; i++) {
        if (pattern->pat[i] != firstByte) {
            return false;
        }
    }
    return true;
}

Boolean PatternsEqual(ConstPatternParam pattern1, ConstPatternParam pattern2) {
    assert(pattern1 != NULL);
    assert(pattern2 != NULL);

    return memcmp(pattern1->pat, pattern2->pat, 8) == 0;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static Boolean GetPatternPixel(ConstPatternParam pattern, SInt16 x, SInt16 y) {
    /* Wrap coordinates to pattern size */
    x = x % 8;
    y = y % 8;
    if (x < 0) x += 8;
    if (y < 0) y += 8;

    return (pattern->pat[y] & (0x80 >> x)) != 0;
}

static void ApplyPatternToRect(const Rect *rect, ConstPatternParam pattern,
                              SInt16 mode, GrafPtr port) {
    /* This would fill the rectangle with the pattern using the specified mode */
    for (SInt16 y = rect->top; y < rect->bottom; y++) {
        for (SInt16 x = rect->left; x < rect->right; x++) {
            Boolean patternBit = GetPatternPixel(pattern, x - rect->left, y - rect->top);

            /* Apply the pattern bit based on mode */
            if (patternBit) {
                /* Set pixel to foreground color */
                QDPlatform_SetPixelInPort(port, x, y, true);
            } else if (mode == patCopy) {
                /* Set pixel to background color */
                QDPlatform_SetPixelInPort(port, x, y, false);
            }
            /* For other modes, combine with existing pixel */
        }
    }
}

static void DitherPixel(SInt16 x, SInt16 y, const RGBColor *color,
                       RGBColor *ditheredColor) {
    /* Use 4x4 dither matrix for simplicity */
    UInt8 ditherValue = g_ditherMatrix4x4[y % 4][x % 4];

    /* Scale dither value to 0-4095 for 16-bit color components */
    UInt16 threshold = ditherValue * 256;

    /* Dither each color component */
    ditheredColor->red = (color->red > threshold) ? 0xFFFF : 0x0000;
    ditheredColor->green = (color->green > threshold) ? 0xFFFF : 0x0000;
    ditheredColor->blue = (color->blue > threshold) ? 0xFFFF : 0x0000;
}

static Pattern CreateGrayPattern(UInt8 grayLevel) {
    Pattern pattern;
    memset(pattern.pat, 0, 8);

    /* Use dithering to create gray levels */
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            UInt8 ditherValue = g_ditherMatrix8x8[y][x] * 4; /* Scale to 0-255 */
            if (grayLevel > ditherValue) {
                pattern.pat[y] |= (0x80 >> x);
            }
        }
    }

    return pattern;
}

static void ExpandPattern(ConstPatternParam srcPattern, Pattern *dstPattern,
                         SInt16 hStretch, SInt16 vStretch) {
    memset(dstPattern->pat, 0, 8);

    if (hStretch <= 0) hStretch = 1;
    if (vStretch <= 0) vStretch = 1;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            /* Map expanded coordinates back to source */
            int srcX = x / hStretch;
            int srcY = y / vStretch;

            if (srcX < 8 && srcY < 8) {
                Boolean srcBit = GetPatternPixel(srcPattern, srcX, srcY);
                if (srcBit) {
                    dstPattern->pat[y] |= (0x80 >> x);
                }
            }
        }
    }
}

/* Additional platform function needed */
void QDPlatform_SetPixelInPort(GrafPtr port, SInt16 x, SInt16 y, Boolean foreground) {
    /* Platform-specific pixel setting implementation would go here */
    /* This is a stub for the pattern system */
}

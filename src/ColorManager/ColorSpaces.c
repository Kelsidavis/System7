/*
 * ColorSpaces.c - Color Space Conversions Implementation
 *
 * Implementation of color space conversion algorithms including RGB, CMYK,
 * HSV, HSL, XYZ, Lab, and other professional color spaces.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#include "../include/ColorManager/ColorSpaces.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ================================================================
 * CONSTANTS AND LOOKUP TABLES
 * ================================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Standard RGB primaries and white points */
static const CMColorMatrix g_sRGBToXYZ = {
    {{0.4124564f, 0.3575761f, 0.1804375f},
     {0.2126729f, 0.7151522f, 0.0721750f},
     {0.0193339f, 0.1191920f, 0.9503041f}},
    {0.0f, 0.0f, 0.0f}
};

static const CMColorMatrix g_XYZTosRGB = {
    {{ 3.2404542f, -1.5371385f, -0.4985314f},
     {-0.9692660f,  1.8760108f,  0.0415560f},
     { 0.0556434f, -0.2040259f,  1.0572252f}},
    {0.0f, 0.0f, 0.0f}
};

static const CMColorMatrix g_adobeRGBToXYZ = {
    {{0.5767309f, 0.1855540f, 0.1881852f},
     {0.2973769f, 0.6273491f, 0.0752741f},
     {0.0270343f, 0.0706872f, 0.9911085f}},
    {0.0f, 0.0f, 0.0f}
};

/* Standard illuminants */
static const CMXYZColor g_illuminantD50 = {96422, 100000, 82521};
static const CMXYZColor g_illuminantD65 = {95047, 100000, 108883};
static const CMXYZColor g_illuminantA   = {109850, 100000, 35585};

/* Standard color constants */
const CMRGBColor kStandardRed     = {65535, 0, 0};
const CMRGBColor kStandardGreen   = {0, 65535, 0};
const CMRGBColor kStandardBlue    = {0, 0, 65535};
const CMRGBColor kStandardCyan    = {0, 65535, 65535};
const CMRGBColor kStandardMagenta = {65535, 0, 65535};
const CMRGBColor kStandardYellow  = {65535, 65535, 0};
const CMRGBColor kStandardBlack   = {0, 0, 0};
const CMRGBColor kStandardWhite   = {65535, 65535, 65535};

/* Gamma lookup tables for performance */
static uint16_t g_gammaTable22[256];
static uint16_t g_invGammaTable22[256];
static bool g_gammaTablesInitialized = false;

/* Named colors registry */
static CMNamedColor *g_namedColors = NULL;
static uint32_t g_namedColorCount = 0;
static uint32_t g_namedColorCapacity = 0;

/* Forward declarations */
static void InitializeGammaTables(void);
static float ClampFloat(float value, float min, float max);
static uint16_t ClampUint16(int32_t value);
static float LinearizeRGB(uint16_t value);
static uint16_t DelinearizeRGB(float value);

/* ================================================================
 * COLOR SPACE INITIALIZATION
 * ================================================================ */

CMError CMInitColorSpaces(void) {
    if (!g_gammaTablesInitialized) {
        InitializeGammaTables();
    }

    /* Initialize named colors with standard colors */
    if (g_namedColors == NULL) {
        g_namedColorCapacity = 64;
        g_namedColors = (CMNamedColor *)calloc(g_namedColorCapacity, sizeof(CMNamedColor));
        if (!g_namedColors) return cmProfileError;

        /* Register standard colors */
        CMColor color;

        color.rgb = kStandardRed;
        CMRegisterNamedColor("red", &color);

        color.rgb = kStandardGreen;
        CMRegisterNamedColor("green", &color);

        color.rgb = kStandardBlue;
        CMRegisterNamedColor("blue", &color);

        color.rgb = kStandardCyan;
        CMRegisterNamedColor("cyan", &color);

        color.rgb = kStandardMagenta;
        CMRegisterNamedColor("magenta", &color);

        color.rgb = kStandardYellow;
        CMRegisterNamedColor("yellow", &color);

        color.rgb = kStandardBlack;
        CMRegisterNamedColor("black", &color);

        color.rgb = kStandardWhite;
        CMRegisterNamedColor("white", &color);
    }

    return cmNoError;
}

CMError CMGetSupportedColorSpaces(CMColorSpace *spaces, uint32_t *count) {
    if (!count) return cmParameterError;

    static const CMColorSpace supportedSpaces[] = {
        cmGraySpace, cmRGBSpace, cmCMYKSpace, cmHSVSpace, cmHLSSpace,
        cmXYZSpace, cmLABSpace, cmYCCSpace, cmYIQSpace
    };
    const uint32_t numSpaces = sizeof(supportedSpaces) / sizeof(supportedSpaces[0]);

    if (spaces && *count >= numSpaces) {
        memcpy(spaces, supportedSpaces, sizeof(supportedSpaces));
    }
    *count = numSpaces;

    return cmNoError;
}

bool CMIsColorSpaceSupported(CMColorSpace space) {
    switch (space) {
        case cmGraySpace:
        case cmRGBSpace:
        case cmCMYKSpace:
        case cmHSVSpace:
        case cmHLSSpace:
        case cmXYZSpace:
        case cmLABSpace:
        case cmYCCSpace:
        case cmYIQSpace:
            return true;
        default:
            return false;
    }
}

CMError CMGetColorSpaceInfo(CMColorSpace space, char *name, uint32_t *channels,
                           uint32_t *precision) {
    if (!channels || !precision) return cmParameterError;

    switch (space) {
        case cmGraySpace:
            if (name) strcpy(name, "Gray");
            *channels = 1;
            *precision = 16;
            break;
        case cmRGBSpace:
            if (name) strcpy(name, "RGB");
            *channels = 3;
            *precision = 16;
            break;
        case cmCMYKSpace:
            if (name) strcpy(name, "CMYK");
            *channels = 4;
            *precision = 16;
            break;
        case cmHSVSpace:
            if (name) strcpy(name, "HSV");
            *channels = 3;
            *precision = 16;
            break;
        case cmHLSSpace:
            if (name) strcpy(name, "HLS");
            *channels = 3;
            *precision = 16;
            break;
        case cmXYZSpace:
            if (name) strcpy(name, "XYZ");
            *channels = 3;
            *precision = 32;
            break;
        case cmLABSpace:
            if (name) strcpy(name, "Lab");
            *channels = 3;
            *precision = 32;
            break;
        default:
            return cmUnsupportedDataType;
    }

    return cmNoError;
}

/* ================================================================
 * RGB COLOR SPACE CONVERSIONS
 * ================================================================ */

CMError CMConvertRGBToHSV(const CMRGBColor *rgb, CMHSVColor *hsv) {
    if (!rgb || !hsv) return cmParameterError;

    float r = rgb->red / 65535.0f;
    float g = rgb->green / 65535.0f;
    float b = rgb->blue / 65535.0f;

    float max = fmaxf(r, fmaxf(g, b));
    float min = fminf(r, fminf(g, b));
    float delta = max - min;

    /* Value */
    hsv->value = (uint16_t)(max * 65535.0f);

    /* Saturation */
    if (max > 0.0f) {
        hsv->saturation = (uint16_t)((delta / max) * 65535.0f);
    } else {
        hsv->saturation = 0;
    }

    /* Hue */
    if (delta > 0.0f) {
        float hue;
        if (max == r) {
            hue = fmodf((g - b) / delta, 6.0f);
        } else if (max == g) {
            hue = (b - r) / delta + 2.0f;
        } else {
            hue = (r - g) / delta + 4.0f;
        }
        if (hue < 0.0f) hue += 6.0f;
        hsv->hue = (uint16_t)((hue / 6.0f) * 65535.0f);
    } else {
        hsv->hue = 0;
    }

    return cmNoError;
}

CMError CMConvertHSVToRGB(const CMHSVColor *hsv, CMRGBColor *rgb) {
    if (!hsv || !rgb) return cmParameterError;

    float h = (hsv->hue / 65535.0f) * 6.0f;
    float s = hsv->saturation / 65535.0f;
    float v = hsv->value / 65535.0f;

    int i = (int)floorf(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: r = g = b = 0; break;
    }

    rgb->red = ClampUint16((int32_t)(r * 65535.0f));
    rgb->green = ClampUint16((int32_t)(g * 65535.0f));
    rgb->blue = ClampUint16((int32_t)(b * 65535.0f));

    return cmNoError;
}

CMError CMConvertRGBToHSL(const CMRGBColor *rgb, CMHLSColor *hsl) {
    if (!rgb || !hsl) return cmParameterError;

    float r = rgb->red / 65535.0f;
    float g = rgb->green / 65535.0f;
    float b = rgb->blue / 65535.0f;

    float max = fmaxf(r, fmaxf(g, b));
    float min = fminf(r, fminf(g, b));
    float delta = max - min;

    /* Lightness */
    float lightness = (max + min) / 2.0f;
    hsl->lightness = (uint16_t)(lightness * 65535.0f);

    /* Saturation */
    if (delta > 0.0f) {
        float saturation;
        if (lightness < 0.5f) {
            saturation = delta / (max + min);
        } else {
            saturation = delta / (2.0f - max - min);
        }
        hsl->saturation = (uint16_t)(saturation * 65535.0f);
    } else {
        hsl->saturation = 0;
    }

    /* Hue (same as HSV) */
    if (delta > 0.0f) {
        float hue;
        if (max == r) {
            hue = fmodf((g - b) / delta, 6.0f);
        } else if (max == g) {
            hue = (b - r) / delta + 2.0f;
        } else {
            hue = (r - g) / delta + 4.0f;
        }
        if (hue < 0.0f) hue += 6.0f;
        hsl->hue = (uint16_t)((hue / 6.0f) * 65535.0f);
    } else {
        hsl->hue = 0;
    }

    return cmNoError;
}

CMError CMConvertHSLToRGB(const CMHLSColor *hsl, CMRGBColor *rgb) {
    if (!hsl || !rgb) return cmParameterError;

    float h = (hsl->hue / 65535.0f) * 6.0f;
    float s = hsl->saturation / 65535.0f;
    float l = hsl->lightness / 65535.0f;

    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    float m = l - c / 2.0f;

    float r, g, b;
    int hi = (int)floorf(h);
    switch (hi) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        case 5: r = c; g = 0; b = x; break;
        default: r = g = b = 0; break;
    }

    rgb->red = ClampUint16((int32_t)((r + m) * 65535.0f));
    rgb->green = ClampUint16((int32_t)((g + m) * 65535.0f));
    rgb->blue = ClampUint16((int32_t)((b + m) * 65535.0f));

    return cmNoError;
}

CMError CMConvertRGBToCMYK(const CMRGBColor *rgb, CMCMYKColor *cmyk) {
    if (!rgb || !cmyk) return cmParameterError;

    float r = rgb->red / 65535.0f;
    float g = rgb->green / 65535.0f;
    float b = rgb->blue / 65535.0f;

    float k = 1.0f - fmaxf(r, fmaxf(g, b));

    if (k < 1.0f) {
        float c = (1.0f - r - k) / (1.0f - k);
        float m = (1.0f - g - k) / (1.0f - k);
        float y = (1.0f - b - k) / (1.0f - k);

        cmyk->cyan = ClampUint16((int32_t)(c * 65535.0f));
        cmyk->magenta = ClampUint16((int32_t)(m * 65535.0f));
        cmyk->yellow = ClampUint16((int32_t)(y * 65535.0f));
    } else {
        cmyk->cyan = 0;
        cmyk->magenta = 0;
        cmyk->yellow = 0;
    }

    cmyk->black = ClampUint16((int32_t)(k * 65535.0f));

    return cmNoError;
}

CMError CMConvertCMYKToRGB(const CMCMYKColor *cmyk, CMRGBColor *rgb) {
    if (!cmyk || !rgb) return cmParameterError;

    float c = cmyk->cyan / 65535.0f;
    float m = cmyk->magenta / 65535.0f;
    float y = cmyk->yellow / 65535.0f;
    float k = cmyk->black / 65535.0f;

    float r = (1.0f - c) * (1.0f - k);
    float g = (1.0f - m) * (1.0f - k);
    float b = (1.0f - y) * (1.0f - k);

    rgb->red = ClampUint16((int32_t)(r * 65535.0f));
    rgb->green = ClampUint16((int32_t)(g * 65535.0f));
    rgb->blue = ClampUint16((int32_t)(b * 65535.0f));

    return cmNoError;
}

CMError CMConvertRGBToGray(const CMRGBColor *rgb, CMGrayColor *gray) {
    if (!rgb || !gray) return cmParameterError;

    /* Use ITU-R BT.709 luma coefficients */
    float luminance = 0.2126f * rgb->red + 0.7152f * rgb->green + 0.0722f * rgb->blue;
    gray->gray = ClampUint16((int32_t)luminance);

    return cmNoError;
}

CMError CMConvertGrayToRGB(const CMGrayColor *gray, CMRGBColor *rgb) {
    if (!gray || !rgb) return cmParameterError;

    rgb->red = gray->gray;
    rgb->green = gray->gray;
    rgb->blue = gray->gray;

    return cmNoError;
}

/* ================================================================
 * XYZ COLOR SPACE CONVERSIONS
 * ================================================================ */

CMError CMConvertRGBToXYZ(const CMRGBColor *rgb, CMXYZColor *xyz,
                         const CMColorMatrix *matrix) {
    if (!rgb || !xyz) return cmParameterError;

    const CMColorMatrix *m = matrix ? matrix : &g_sRGBToXYZ;

    /* Linearize RGB values */
    float r = LinearizeRGB(rgb->red);
    float g = LinearizeRGB(rgb->green);
    float b = LinearizeRGB(rgb->blue);

    /* Apply matrix transformation */
    float X = m->matrix[0][0] * r + m->matrix[0][1] * g + m->matrix[0][2] * b + m->offset[0];
    float Y = m->matrix[1][0] * r + m->matrix[1][1] * g + m->matrix[1][2] * b + m->offset[1];
    float Z = m->matrix[2][0] * r + m->matrix[2][1] * g + m->matrix[2][2] * b + m->offset[2];

    /* Convert to XYZ scale (0-100000) */
    xyz->X = (int32_t)(X * 100000.0f);
    xyz->Y = (int32_t)(Y * 100000.0f);
    xyz->Z = (int32_t)(Z * 100000.0f);

    return cmNoError;
}

CMError CMConvertXYZToRGB(const CMXYZColor *xyz, CMRGBColor *rgb,
                         const CMColorMatrix *matrix) {
    if (!xyz || !rgb) return cmParameterError;

    const CMColorMatrix *m = matrix ? matrix : &g_XYZTosRGB;

    /* Convert from XYZ scale */
    float X = xyz->X / 100000.0f;
    float Y = xyz->Y / 100000.0f;
    float Z = xyz->Z / 100000.0f;

    /* Apply matrix transformation */
    float r = m->matrix[0][0] * X + m->matrix[0][1] * Y + m->matrix[0][2] * Z + m->offset[0];
    float g = m->matrix[1][0] * X + m->matrix[1][1] * Y + m->matrix[1][2] * Z + m->offset[1];
    float b = m->matrix[2][0] * X + m->matrix[2][1] * Y + m->matrix[2][2] * Z + m->offset[2];

    /* Delinearize and convert to RGB */
    rgb->red = ClampUint16((int32_t)(DelinearizeRGB(r) * 65535.0f));
    rgb->green = ClampUint16((int32_t)(DelinearizeRGB(g) * 65535.0f));
    rgb->blue = ClampUint16((int32_t)(DelinearizeRGB(b) * 65535.0f));

    return cmNoError;
}

/* ================================================================
 * COLOR SPACE UTILITIES
 * ================================================================ */

bool CMIsValidRGBColor(const CMRGBColor *color) {
    return color != NULL; /* RGB values are always valid (16-bit) */
}

bool CMIsValidCMYKColor(const CMCMYKColor *color) {
    return color != NULL; /* CMYK values are always valid (16-bit) */
}

bool CMIsValidHSVColor(const CMHSVColor *color) {
    return color != NULL; /* HSV values are always valid (16-bit) */
}

bool CMIsValidXYZColor(const CMXYZColor *color) {
    if (!color) return false;
    /* XYZ values should be non-negative and within reasonable bounds */
    return (color->X >= 0 && color->Y >= 0 && color->Z >= 0 &&
            color->X <= 200000 && color->Y <= 200000 && color->Z <= 200000);
}

bool CMIsValidLabColor(const CMLABColor *color) {
    if (!color) return false;
    /* Lab values should be within standard ranges */
    return (color->L >= 0 && color->L <= 100 &&
            color->a >= -128 && color->a <= 127 &&
            color->b >= -128 && color->b <= 127);
}

void CMClampRGBColor(CMRGBColor *color) {
    if (!color) return;
    /* RGB values are already clamped by uint16_t type */
}

void CMClampCMYKColor(CMCMYKColor *color) {
    if (!color) return;
    /* CMYK values are already clamped by uint16_t type */
}

void CMClampHSVColor(CMHSVColor *color) {
    if (!color) return;
    /* HSV values are already clamped by uint16_t type */
}

CMError CMInterpolateRGB(const CMRGBColor *color1, const CMRGBColor *color2,
                        float t, CMRGBColor *result) {
    if (!color1 || !color2 || !result) return cmParameterError;

    t = ClampFloat(t, 0.0f, 1.0f);
    float oneMinusT = 1.0f - t;

    result->red = (uint16_t)(oneMinusT * color1->red + t * color2->red);
    result->green = (uint16_t)(oneMinusT * color1->green + t * color2->green);
    result->blue = (uint16_t)(oneMinusT * color1->blue + t * color2->blue);

    return cmNoError;
}

/* ================================================================
 * COLOR MATRICES AND TRANSFORMS
 * ================================================================ */

CMError CMGetSRGBToXYZMatrix(CMColorMatrix *matrix) {
    if (!matrix) return cmParameterError;
    *matrix = g_sRGBToXYZ;
    return cmNoError;
}

CMError CMGetAdobeRGBToXYZMatrix(CMColorMatrix *matrix) {
    if (!matrix) return cmParameterError;
    *matrix = g_adobeRGBToXYZ;
    return cmNoError;
}

CMError CMApplyColorMatrix(const CMColorMatrix *matrix, const CMXYZColor *input,
                          CMXYZColor *output) {
    if (!matrix || !input || !output) return cmParameterError;

    float X = input->X / 100000.0f;
    float Y = input->Y / 100000.0f;
    float Z = input->Z / 100000.0f;

    float newX = matrix->matrix[0][0] * X + matrix->matrix[0][1] * Y + matrix->matrix[0][2] * Z + matrix->offset[0];
    float newY = matrix->matrix[1][0] * X + matrix->matrix[1][1] * Y + matrix->matrix[1][2] * Z + matrix->offset[1];
    float newZ = matrix->matrix[2][0] * X + matrix->matrix[2][1] * Y + matrix->matrix[2][2] * Z + matrix->offset[2];

    output->X = (int32_t)(newX * 100000.0f);
    output->Y = (int32_t)(newY * 100000.0f);
    output->Z = (int32_t)(newZ * 100000.0f);

    return cmNoError;
}

/* ================================================================
 * ILLUMINANTS AND WHITE POINTS
 * ================================================================ */

CMError CMGetIlluminantD50(CMXYZColor *whitePoint) {
    if (!whitePoint) return cmParameterError;
    *whitePoint = g_illuminantD50;
    return cmNoError;
}

CMError CMGetIlluminantD65(CMXYZColor *whitePoint) {
    if (!whitePoint) return cmParameterError;
    *whitePoint = g_illuminantD65;
    return cmNoError;
}

CMError CMGetIlluminantA(CMXYZColor *whitePoint) {
    if (!whitePoint) return cmParameterError;
    *whitePoint = g_illuminantA;
    return cmNoError;
}

/* ================================================================
 * GAMMA CORRECTION
 * ================================================================ */

uint16_t CMApplyGamma(uint16_t value, float gamma) {
    float normalized = value / 65535.0f;
    float corrected = powf(normalized, gamma);
    return ClampUint16((int32_t)(corrected * 65535.0f));
}

uint16_t CMRemoveGamma(uint16_t value, float gamma) {
    float normalized = value / 65535.0f;
    float corrected = powf(normalized, 1.0f / gamma);
    return ClampUint16((int32_t)(corrected * 65535.0f));
}

uint16_t CMApplySRGBGamma(uint16_t value) {
    if (!g_gammaTablesInitialized) {
        InitializeGammaTables();
    }
    return g_gammaTable22[value >> 8];  /* Use high 8 bits for lookup */
}

uint16_t CMRemoveSRGBGamma(uint16_t value) {
    if (!g_gammaTablesInitialized) {
        InitializeGammaTables();
    }
    return g_invGammaTable22[value >> 8];  /* Use high 8 bits for lookup */
}

/* ================================================================
 * COLOR DIFFERENCE CALCULATIONS
 * ================================================================ */

float CMCalculateDeltaE76(const CMLABColor *lab1, const CMLABColor *lab2) {
    if (!lab1 || !lab2) return -1.0f;

    float dL = lab1->L - lab2->L;
    float da = lab1->a - lab2->a;
    float db = lab1->b - lab2->b;

    return sqrtf(dL * dL + da * da + db * db);
}

float CMCalculateRGBDistance(const CMRGBColor *rgb1, const CMRGBColor *rgb2) {
    if (!rgb1 || !rgb2) return -1.0f;

    float dr = (float)(rgb1->red - rgb2->red) / 65535.0f;
    float dg = (float)(rgb1->green - rgb2->green) / 65535.0f;
    float db = (float)(rgb1->blue - rgb2->blue) / 65535.0f;

    return sqrtf(dr * dr + dg * dg + db * db);
}

/* ================================================================
 * NAMED COLORS
 * ================================================================ */

CMError CMRegisterNamedColor(const char *name, const CMColor *color) {
    if (!name || !color) return cmParameterError;

    /* Check if we need to expand the array */
    if (g_namedColorCount >= g_namedColorCapacity) {
        g_namedColorCapacity *= 2;
        CMNamedColor *newArray = (CMNamedColor *)realloc(g_namedColors,
            g_namedColorCapacity * sizeof(CMNamedColor));
        if (!newArray) return cmProfileError;
        g_namedColors = newArray;
    }

    /* Add the named color */
    strncpy(g_namedColors[g_namedColorCount].name, name,
            sizeof(g_namedColors[g_namedColorCount].name) - 1);
    g_namedColors[g_namedColorCount].name[sizeof(g_namedColors[g_namedColorCount].name) - 1] = '\0';
    g_namedColors[g_namedColorCount].color = *color;
    g_namedColors[g_namedColorCount].index = g_namedColorCount;
    g_namedColorCount++;

    return cmNoError;
}

CMError CMLookupNamedColor(const char *name, CMColor *color) {
    if (!name || !color) return cmParameterError;

    for (uint32_t i = 0; i < g_namedColorCount; i++) {
        if (strcmp(g_namedColors[i].name, name) == 0) {
            *color = g_namedColors[i].color;
            return cmNoError;
        }
    }

    return cmNamedColorNotFound;
}

CMError CMGetNamedColorList(CMNamedColor *colors, uint32_t *count) {
    if (!count) return cmParameterError;

    if (colors && *count >= g_namedColorCount) {
        memcpy(colors, g_namedColors, g_namedColorCount * sizeof(CMNamedColor));
    }
    *count = g_namedColorCount;

    return cmNoError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static void InitializeGammaTables(void) {
    for (int i = 0; i < 256; i++) {
        float normalized = i / 255.0f;

        /* Apply 2.2 gamma */
        float gamma = powf(normalized, 2.2f);
        g_gammaTable22[i] = ClampUint16((int32_t)(gamma * 65535.0f));

        /* Apply inverse 2.2 gamma */
        float invGamma = powf(normalized, 1.0f / 2.2f);
        g_invGammaTable22[i] = ClampUint16((int32_t)(invGamma * 65535.0f));
    }

    g_gammaTablesInitialized = true;
}

static float ClampFloat(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static uint16_t ClampUint16(int32_t value) {
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return (uint16_t)value;
}

static float LinearizeRGB(uint16_t value) {
    float normalized = value / 65535.0f;

    /* sRGB gamma correction */
    if (normalized <= 0.04045f) {
        return normalized / 12.92f;
    } else {
        return powf((normalized + 0.055f) / 1.055f, 2.4f);
    }
}

static uint16_t DelinearizeRGB(float value) {
    /* Inverse sRGB gamma correction */
    float result;
    if (value <= 0.0031308f) {
        result = value * 12.92f;
    } else {
        result = 1.055f * powf(value, 1.0f / 2.4f) - 0.055f;
    }

    return ClampUint16((int32_t)(result * 65535.0f));
}
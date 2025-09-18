/*
 * ColorSpaces.h - Color Space Definitions and Conversions
 *
 * Color space management and conversion functions for RGB, CMYK, HSV, HSL,
 * XYZ, Lab, and other color spaces used in professional color management.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#ifndef COLORSPACES_H
#define COLORSPACES_H

#include "ColorManager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * COLOR SPACE CONSTANTS
 * ================================================================ */

/* Color temperature constants */
#define kColorTemp2856K     2856    /* Illuminant A */
#define kColorTemp5000K     5000    /* Illuminant D50 */
#define kColorTemp6500K     6500    /* Illuminant D65 */
#define kColorTemp9300K     9300    /* Illuminant D93 */

/* Gamma values */
#define kGamma18            1.8f    /* Mac gamma */
#define kGamma22            2.2f    /* PC/sRGB gamma */
#define kGamma24            2.4f    /* Adobe RGB gamma */

/* Color precision constants */
#define kColorPrecision8Bit     8
#define kColorPrecision16Bit    16
#define kColorPrecision32Bit    32

/* ================================================================
 * COLOR SPACE STRUCTURES
 * ================================================================ */

/* Extended RGB color with alpha */
typedef struct {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t alpha;
} CMRGBAColor;

/* Gray color */
typedef struct {
    uint16_t gray;
} CMGrayColor;

/* YIQ color space */
typedef struct {
    int16_t Y;      /* Luminance */
    int16_t I;      /* In-phase */
    int16_t Q;      /* Quadrature */
} CMYIQColor;

/* YUV color space */
typedef struct {
    uint16_t Y;     /* Luminance */
    int16_t  U;     /* Blue-Yellow */
    int16_t  V;     /* Red-Cyan */
} CMYUVColor;

/* YCbCr color space */
typedef struct {
    uint16_t Y;     /* Luminance */
    uint16_t Cb;    /* Blue chroma */
    uint16_t Cr;    /* Red chroma */
} CMYCbCrColor;

/* LUV color space */
typedef struct {
    int32_t L;      /* Lightness */
    int32_t u;      /* Green-Red axis */
    int32_t v;      /* Blue-Yellow axis */
} CMLUVColor;

/* Yxy color space */
typedef struct {
    uint16_t Y;     /* Luminance */
    uint16_t x;     /* x chromaticity */
    uint16_t y;     /* y chromaticity */
} CMYxyColor;

/* HiFi color space (6+ channels) */
typedef struct {
    uint16_t channels[16];  /* Up to 16 channels */
    uint8_t  numChannels;
} CMHiFiColor;

/* Named color */
typedef struct {
    char        name[64];   /* Color name */
    CMColor     color;      /* Actual color value */
    uint32_t    index;      /* Color index */
} CMNamedColor;

/* Color space conversion matrix */
typedef struct {
    float matrix[3][3];     /* 3x3 transformation matrix */
    float offset[3];        /* Offset vector */
} CMColorMatrix;

/* Illuminant data */
typedef struct {
    CMXYZColor  whitePoint;
    uint16_t    temperature;
    float       gamma;
    char        name[32];
} CMIlluminant;

/* ================================================================
 * COLOR SPACE INITIALIZATION
 * ================================================================ */

/* Initialize color space system */
CMError CMInitColorSpaces(void);

/* Get supported color spaces */
CMError CMGetSupportedColorSpaces(CMColorSpace *spaces, uint32_t *count);

/* Check if color space is supported */
bool CMIsColorSpaceSupported(CMColorSpace space);

/* Get color space info */
CMError CMGetColorSpaceInfo(CMColorSpace space, char *name, uint32_t *channels,
                           uint32_t *precision);

/* ================================================================
 * RGB COLOR SPACE CONVERSIONS
 * ================================================================ */

/* RGB to other color spaces */
CMError CMConvertRGBToHSV(const CMRGBColor *rgb, CMHSVColor *hsv);
CMError CMConvertRGBToHSL(const CMRGBColor *rgb, CMHLSColor *hsl);
CMError CMConvertRGBToCMYK(const CMRGBColor *rgb, CMCMYKColor *cmyk);
CMError CMConvertRGBToGray(const CMRGBColor *rgb, CMGrayColor *gray);
CMError CMConvertRGBToYIQ(const CMRGBColor *rgb, CMYIQColor *yiq);
CMError CMConvertRGBToYUV(const CMRGBColor *rgb, CMYUVColor *yuv);
CMError CMConvertRGBToYCbCr(const CMRGBColor *rgb, CMYCbCrColor *ycbcr);

/* Other color spaces to RGB */
CMError CMConvertHSVToRGB(const CMHSVColor *hsv, CMRGBColor *rgb);
CMError CMConvertHSLToRGB(const CMHLSColor *hsl, CMRGBColor *rgb);
CMError CMConvertCMYKToRGB(const CMCMYKColor *cmyk, CMRGBColor *rgb);
CMError CMConvertGrayToRGB(const CMGrayColor *gray, CMRGBColor *rgb);
CMError CMConvertYIQToRGB(const CMYIQColor *yiq, CMRGBColor *rgb);
CMError CMConvertYUVToRGB(const CMYUVColor *yuv, CMRGBColor *rgb);
CMError CMConvertYCbCrToRGB(const CMYCbCrColor *ycbcr, CMRGBColor *rgb);

/* ================================================================
 * XYZ COLOR SPACE CONVERSIONS
 * ================================================================ */

/* XYZ to other color spaces */
CMError CMConvertXYZToRGB(const CMXYZColor *xyz, CMRGBColor *rgb,
                         const CMColorMatrix *matrix);
CMError CMConvertXYZToLUV(const CMXYZColor *xyz, CMLUVColor *luv,
                         const CMXYZColor *whitePoint);
CMError CMConvertXYZToYxy(const CMXYZColor *xyz, CMYxyColor *yxy);

/* Other color spaces to XYZ */
CMError CMConvertRGBToXYZ(const CMRGBColor *rgb, CMXYZColor *xyz,
                         const CMColorMatrix *matrix);
CMError CMConvertLUVToXYZ(const CMLUVColor *luv, CMXYZColor *xyz,
                         const CMXYZColor *whitePoint);
CMError CMConvertYxyToXYZ(const CMYxyColor *yxy, CMXYZColor *xyz);

/* ================================================================
 * COLOR SPACE UTILITIES
 * ================================================================ */

/* Color space validation */
bool CMIsValidRGBColor(const CMRGBColor *color);
bool CMIsValidCMYKColor(const CMCMYKColor *color);
bool CMIsValidHSVColor(const CMHSVColor *color);
bool CMIsValidXYZColor(const CMXYZColor *color);
bool CMIsValidLabColor(const CMLABColor *color);

/* Color clamping */
void CMClampRGBColor(CMRGBColor *color);
void CMClampCMYKColor(CMCMYKColor *color);
void CMClampHSVColor(CMHSVColor *color);

/* Color interpolation */
CMError CMInterpolateRGB(const CMRGBColor *color1, const CMRGBColor *color2,
                        float t, CMRGBColor *result);
CMError CMInterpolateHSV(const CMHSVColor *color1, const CMHSVColor *color2,
                        float t, CMHSVColor *result);
CMError CMInterpolateXYZ(const CMXYZColor *color1, const CMXYZColor *color2,
                        float t, CMXYZColor *result);

/* ================================================================
 * COLOR MATRICES AND TRANSFORMS
 * ================================================================ */

/* Get standard color matrices */
CMError CMGetSRGBToXYZMatrix(CMColorMatrix *matrix);
CMError CMGetAdobeRGBToXYZMatrix(CMColorMatrix *matrix);
CMError CMGetProPhotoRGBToXYZMatrix(CMColorMatrix *matrix);

/* Matrix operations */
CMError CMMultiplyColorMatrix(const CMColorMatrix *m1, const CMColorMatrix *m2,
                             CMColorMatrix *result);
CMError CMInvertColorMatrix(const CMColorMatrix *matrix, CMColorMatrix *inverse);
CMError CMApplyColorMatrix(const CMColorMatrix *matrix, const CMXYZColor *input,
                          CMXYZColor *output);

/* Create custom matrices */
CMError CMCreateColorMatrix(const CMXYZColor *redPrimary,
                           const CMXYZColor *greenPrimary,
                           const CMXYZColor *bluePrimary,
                           const CMXYZColor *whitePoint,
                           CMColorMatrix *matrix);

/* ================================================================
 * ILLUMINANTS AND WHITE POINTS
 * ================================================================ */

/* Get standard illuminants */
CMError CMGetStandardIlluminant(uint16_t temperature, CMIlluminant *illuminant);
CMError CMGetIlluminantD50(CMXYZColor *whitePoint);
CMError CMGetIlluminantD65(CMXYZColor *whitePoint);
CMError CMGetIlluminantA(CMXYZColor *whitePoint);

/* Color temperature conversions */
CMError CMColorTemperatureToXYZ(uint16_t temperature, CMXYZColor *xyz);
CMError CMXYZToColorTemperature(const CMXYZColor *xyz, uint16_t *temperature);

/* Chromatic adaptation */
CMError CMChromaticAdaptation(const CMXYZColor *color,
                             const CMXYZColor *srcWhitePoint,
                             const CMXYZColor *dstWhitePoint,
                             CMXYZColor *adaptedColor);

/* ================================================================
 * GAMMA CORRECTION
 * ================================================================ */

/* Gamma correction functions */
uint16_t CMApplyGamma(uint16_t value, float gamma);
uint16_t CMRemoveGamma(uint16_t value, float gamma);

/* Apply gamma to colors */
CMError CMApplyGammaToRGB(CMRGBColor *color, float gamma);
CMError CMRemoveGammaFromRGB(CMRGBColor *color, float gamma);

/* sRGB gamma functions */
uint16_t CMApplySRGBGamma(uint16_t value);
uint16_t CMRemoveSRGBGamma(uint16_t value);

/* ================================================================
 * COLOR DIFFERENCE CALCULATIONS
 * ================================================================ */

/* Calculate color differences */
float CMCalculateDeltaE76(const CMLABColor *lab1, const CMLABColor *lab2);
float CMCalculateDeltaE94(const CMLABColor *lab1, const CMLABColor *lab2);
float CMCalculateDeltaE2000(const CMLABColor *lab1, const CMLABColor *lab2);

/* RGB color difference */
float CMCalculateRGBDistance(const CMRGBColor *rgb1, const CMRGBColor *rgb2);

/* ================================================================
 * NAMED COLORS
 * ================================================================ */

/* Named color management */
CMError CMRegisterNamedColor(const char *name, const CMColor *color);
CMError CMLookupNamedColor(const char *name, CMColor *color);
CMError CMGetNamedColorList(CMNamedColor *colors, uint32_t *count);

/* Standard color constants */
extern const CMRGBColor kStandardRed;
extern const CMRGBColor kStandardGreen;
extern const CMRGBColor kStandardBlue;
extern const CMRGBColor kStandardCyan;
extern const CMRGBColor kStandardMagenta;
extern const CMRGBColor kStandardYellow;
extern const CMRGBColor kStandardBlack;
extern const CMRGBColor kStandardWhite;

/* ================================================================
 * HIGH PRECISION COLOR OPERATIONS
 * ================================================================ */

/* Extended precision color types */
typedef struct {
    float red;
    float green;
    float blue;
    float alpha;
} CMFloatRGBAColor;

typedef struct {
    float X;
    float Y;
    float Z;
} CMFloatXYZColor;

typedef struct {
    float L;
    float a;
    float b;
} CMFloatLABColor;

/* High precision conversions */
CMError CMConvertFloatRGBToXYZ(const CMFloatRGBAColor *rgb, CMFloatXYZColor *xyz,
                              const CMColorMatrix *matrix);
CMError CMConvertFloatXYZToLab(const CMFloatXYZColor *xyz, CMFloatLABColor *lab,
                              const CMFloatXYZColor *whitePoint);

/* Precision conversion helpers */
CMError CMConvert16BitToFloat(const CMRGBColor *rgb16, CMFloatRGBAColor *rgbFloat);
CMError CMConvertFloatTo16Bit(const CMFloatRGBAColor *rgbFloat, CMRGBColor *rgb16);

#ifdef __cplusplus
}
#endif

#endif /* COLORSPACES_H */
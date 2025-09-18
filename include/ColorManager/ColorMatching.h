/*
 * ColorMatching.h - Color Matching and Gamut Mapping
 *
 * Professional color matching algorithms, gamut mapping, and color space
 * transformation for accurate color reproduction across devices.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager and ColorSync
 */

#ifndef COLORMATCHING_H
#define COLORMATCHING_H

#include "ColorManager.h"
#include "ColorSpaces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * COLOR MATCHING CONSTANTS
 * ================================================================ */

/* Transform types */
typedef enum {
    cmSimpleTransform       = 0,    /* Simple matrix transform */
    cmLUTTransform         = 1,    /* Lookup table transform */
    cmCurveTransform       = 2,    /* Curve-based transform */
    cmMatrixTRCTransform   = 3,    /* Matrix + TRC transform */
    cmNamedColorTransform  = 4,    /* Named color transform */
    cmPreviewTransform     = 5,    /* Preview/proofing transform */
    cmGamutTransform       = 6     /* Gamut checking transform */
} CMTransformType;

/* Matching quality levels */
typedef enum {
    cmDraftMatching     = 0,    /* Fast, lower quality */
    cmNormalMatching    = 1,    /* Normal quality */
    cmBestMatching      = 2     /* Slow, highest quality */
} CMMatchingQuality;

/* Gamut mapping methods */
typedef enum {
    cmClipGamut         = 0,    /* Simple clipping */
    cmCompressGamut     = 1,    /* Compress out-of-gamut colors */
    cmDesaturateGamut   = 2,    /* Desaturate to fit gamut */
    cmPerceptualGamut   = 3     /* Perceptual gamut mapping */
} CMGamutMethod;

/* Color difference algorithms */
typedef enum {
    cmDeltaE76      = 0,    /* CIE76 Delta E */
    cmDeltaE94      = 1,    /* CIE94 Delta E */
    cmDeltaE2000    = 2,    /* CIEDE2000 Delta E */
    cmCMC           = 3     /* CMC(l:c) formula */
} CMColorDifferenceAlgorithm;

/* ================================================================
 * COLOR TRANSFORM STRUCTURES
 * ================================================================ */

/* Color transform handle */
typedef struct CMColorTransform* CMTransformRef;

/* Transform parameters */
typedef struct {
    CMRenderingIntent       intent;
    CMQuality              quality;
    CMGamutMethod          gamutMethod;
    bool                   blackPointCompensation;
    bool                   useDisplayCompensation;
    float                  adaptationState;
    CMColorDifferenceAlgorithm deltaEAlgorithm;
} CMTransformParams;

/* Gamut checking result */
typedef struct {
    bool    inGamut;
    float   deltaE;
    float   saturationScale;
    float   lightnessScale;
} CMGamutResult;

/* Color matching statistics */
typedef struct {
    uint32_t    totalColors;
    uint32_t    inGamutColors;
    uint32_t    outOfGamutColors;
    float       averageDeltaE;
    float       maxDeltaE;
    uint32_t    transformTime;  /* microseconds */
} CMMatchingStats;

/* ================================================================
 * COLOR MATCHING INITIALIZATION
 * ================================================================ */

/* Initialize color matching system */
CMError CMInitColorMatching(void);

/* Set global matching preferences */
CMError CMSetMatchingPreferences(const CMTransformParams *params);

/* Get global matching preferences */
CMError CMGetMatchingPreferences(CMTransformParams *params);

/* ================================================================
 * COLOR TRANSFORM CREATION
 * ================================================================ */

/* Create color transform */
CMTransformRef CMCreateColorTransform(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                     CMRenderingIntent intent, CMQuality quality);

/* Create multi-profile transform */
CMTransformRef CMCreateMultiProfileTransform(CMProfileRef *profiles, uint32_t count,
                                            CMRenderingIntent intent, CMQuality quality);

/* Create proofing transform */
CMTransformRef CMCreateProofingTransform(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                        CMProfileRef proofProfile, CMRenderingIntent intent,
                                        CMRenderingIntent proofIntent, bool gamutCheck);

/* Create named color transform */
CMTransformRef CMCreateNamedColorTransform(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                          const char **colorNames, uint32_t nameCount);

/* Clone transform */
CMTransformRef CMCloneColorTransform(CMTransformRef transform);

/* Dispose transform */
void CMDisposeColorTransform(CMTransformRef transform);

/* ================================================================
 * COLOR TRANSFORM OPERATIONS
 * ================================================================ */

/* Apply transform to single color */
CMError CMApplyTransform(CMTransformRef transform, const CMColor *input, CMColor *output);

/* Apply transform to color array */
CMError CMApplyTransformArray(CMTransformRef transform, const CMColor *input,
                             CMColor *output, uint32_t count);

/* Apply transform with gamut checking */
CMError CMApplyTransformWithGamut(CMTransformRef transform, const CMColor *input,
                                 CMColor *output, CMGamutResult *gamutResult);

/* Get transform info */
CMError CMGetTransformInfo(CMTransformRef transform, CMTransformType *type,
                          CMColorSpace *srcSpace, CMColorSpace *dstSpace);

/* ================================================================
 * GAMUT OPERATIONS
 * ================================================================ */

/* Create gamut boundary */
CMError CMCreateGamutBoundary(CMProfileRef profile, void **gamutData);

/* Check color against gamut */
CMError CMCheckColorGamut(void *gamutData, const CMColor *color, CMGamutResult *result);

/* Map color to gamut */
CMError CMMapColorToGamut(void *gamutData, const CMColor *input, CMColor *output,
                         CMGamutMethod method);

/* Calculate gamut volume */
CMError CMCalculateGamutVolume(void *gamutData, float *volume);

/* Compare gamuts */
CMError CMCompareGamuts(void *gamut1, void *gamut2, float *coverage, float *overlap);

/* Dispose gamut data */
void CMDisposeGamutBoundary(void *gamutData);

/* ================================================================
 * COLOR DIFFERENCE CALCULATIONS
 * ================================================================ */

/* Calculate color difference */
float CMCalculateColorDifference(const CMColor *color1, const CMColor *color2,
                                CMColorDifferenceAlgorithm algorithm,
                                const CMXYZColor *whitePoint);

/* Calculate Delta E with specific algorithm */
float CMCalculateDeltaE(const CMLABColor *lab1, const CMLABColor *lab2,
                       CMColorDifferenceAlgorithm algorithm);

/* Calculate CMC color difference */
float CMCalculateCMCDifference(const CMLABColor *lab1, const CMLABColor *lab2,
                              float lightness, float chroma);

/* Calculate average color difference for array */
CMError CMCalculateAverageColorDifference(const CMColor *colors1, const CMColor *colors2,
                                         uint32_t count, float *averageDeltaE,
                                         CMColorDifferenceAlgorithm algorithm);

/* ================================================================
 * COLOR ADAPTATION
 * ================================================================ */

/* Chromatic adaptation methods */
typedef enum {
    cmBradfordAdaptation    = 0,    /* Bradford transform */
    cmVonKriesAdaptation   = 1,    /* Von Kries transform */
    cmXYZScalingAdaptation = 2     /* XYZ scaling */
} CMAdaptationMethod;

/* Apply chromatic adaptation */
CMError CMApplyChromaticAdaptation(const CMXYZColor *color,
                                  const CMXYZColor *srcWhitePoint,
                                  const CMXYZColor *dstWhitePoint,
                                  CMAdaptationMethod method,
                                  CMXYZColor *adaptedColor);

/* Create adaptation matrix */
CMError CMCreateAdaptationMatrix(const CMXYZColor *srcWhitePoint,
                                const CMXYZColor *dstWhitePoint,
                                CMAdaptationMethod method,
                                CMColorMatrix *matrix);

/* ================================================================
 * BLACK POINT COMPENSATION
 * ================================================================ */

/* Calculate black point */
CMError CMCalculateBlackPoint(CMProfileRef profile, CMXYZColor *blackPoint);

/* Apply black point compensation */
CMError CMApplyBlackPointCompensation(const CMXYZColor *color,
                                     const CMXYZColor *srcBlackPoint,
                                     const CMXYZColor *dstBlackPoint,
                                     const CMXYZColor *srcWhitePoint,
                                     const CMXYZColor *dstWhitePoint,
                                     CMXYZColor *compensatedColor);

/* ================================================================
 * PERFORMANCE MONITORING
 * ================================================================ */

/* Get matching statistics */
CMError CMGetMatchingStatistics(CMTransformRef transform, CMMatchingStats *stats);

/* Reset matching statistics */
CMError CMResetMatchingStatistics(CMTransformRef transform);

/* Benchmark transform performance */
CMError CMBenchmarkTransform(CMTransformRef transform, uint32_t iterations,
                            uint32_t *microseconds);

/* ================================================================
 * SPECIALIZED TRANSFORMS
 * ================================================================ */

/* Intent-specific color matching */
CMError CMPerformColorMatching(CMProfileRef srcProfile, CMProfileRef dstProfile,
                              CMColor *colors, uint32_t count, CMRenderingIntent intent);

/* Perceptual matching */
CMError CMPerceptualMatching(CMProfileRef srcProfile, CMProfileRef dstProfile,
                           CMColor *colors, uint32_t count);

/* Saturation matching */
CMError CMSaturationMatching(CMProfileRef srcProfile, CMProfileRef dstProfile,
                           CMColor *colors, uint32_t count);

/* Relative colorimetric matching */
CMError CMRelativeColorimetricMatching(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                      CMColor *colors, uint32_t count);

/* Absolute colorimetric matching */
CMError CMAbsoluteColorimetricMatching(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                      CMColor *colors, uint32_t count);

/* ================================================================
 * SOFT PROOFING
 * ================================================================ */

/* Create soft proof */
CMError CMCreateSoftProof(CMProfileRef srcProfile, CMProfileRef displayProfile,
                         CMProfileRef printerProfile, CMRenderingIntent intent,
                         bool showOutOfGamut, CMTransformRef *proofTransform);

/* Apply soft proofing */
CMError CMApplySoftProofing(CMTransformRef proofTransform, const CMColor *input,
                           CMColor *output, bool *outOfGamut);

/* ================================================================
 * COLOR LOOKUP TABLES
 * ================================================================ */

/* LUT types */
typedef enum {
    cmLUT8      = 8,     /* 8-bit LUT */
    cmLUT16     = 16     /* 16-bit LUT */
} CMLUTType;

/* Create color LUT */
CMError CMCreateColorLUT(CMProfileRef srcProfile, CMProfileRef dstProfile,
                        CMLUTType lutType, uint32_t gridPoints,
                        void **lutData, uint32_t *lutSize);

/* Apply LUT transform */
CMError CMApplyLUTTransform(const void *lutData, CMLUTType lutType,
                           const CMColor *input, CMColor *output);

/* Interpolate LUT values */
CMError CMInterpolateLUT(const void *lutData, CMLUTType lutType, uint32_t gridPoints,
                        const float *input, float *output);

/* ================================================================
 * ADVANCED COLOR MATCHING
 * ================================================================ */

/* Spectral color matching */
CMError CMSpectralColorMatching(const float *srcSpectral, const float *dstSpectral,
                               uint32_t wavelengths, CMColor *result);

/* Device link profiles */
CMError CMCreateDeviceLink(CMProfileRef srcProfile, CMProfileRef dstProfile,
                          CMRenderingIntent intent, CMProfileRef *deviceLink);

/* Custom rendering intent */
CMError CMCreateCustomRenderingIntent(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                     float perceptualWeight, float saturationWeight,
                                     float colorimetricWeight, CMTransformRef *transform);

#ifdef __cplusplus
}
#endif

#endif /* COLORMATCHING_H */
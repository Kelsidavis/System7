/*
 * ColorMatching.c - Color Matching and Gamut Mapping Implementation
 *
 * Implementation of professional color matching algorithms, gamut mapping,
 * and color space transformation for accurate color reproduction.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager and ColorSync
 */

#include "../include/ColorManager/ColorMatching.h"
#include "../include/ColorManager/ColorSpaces.h"
#include "../include/ColorManager/ICCProfiles.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ================================================================
 * INTERNAL STRUCTURES
 * ================================================================ */

struct CMColorTransform {
    CMTransformType         type;
    CMProfileRef           srcProfile;
    CMProfileRef           dstProfile;
    CMProfileRef           proofProfile;
    CMRenderingIntent      intent;
    CMQuality             quality;
    CMTransformParams     params;
    void                  *transformData;
    CMMatchingStats       stats;
    bool                  isValid;
};

/* Matrix-based transform data */
typedef struct {
    CMColorMatrix   matrix;
    uint16_t       *redTRC;
    uint16_t       *greenTRC;
    uint16_t       *blueTRC;
    uint32_t       trcSize;
    CMXYZColor     srcWhitePoint;
    CMXYZColor     dstWhitePoint;
    bool           hasAdaptation;
} CMMatrixTransformData;

/* LUT-based transform data */
typedef struct {
    uint8_t        *lut8;
    uint16_t       *lut16;
    uint32_t       gridPoints;
    uint32_t       inputChannels;
    uint32_t       outputChannels;
    CMLUTType      lutType;
} CMLUTTransformData;

/* Gamut boundary data */
typedef struct {
    float          *boundary;      /* Gamut boundary points */
    uint32_t       pointCount;
    CMXYZColor     whitePoint;
    CMXYZColor     blackPoint;
    float          volume;
    bool           isValid;
} CMGamutBoundaryData;

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static bool g_colorMatchingInitialized = false;
static CMTransformParams g_defaultParams = {
    cmPerceptual,               /* intent */
    cmNormalQuality,           /* quality */
    cmCompressGamut,           /* gamutMethod */
    true,                      /* blackPointCompensation */
    false,                     /* useDisplayCompensation */
    1.0f,                      /* adaptationState */
    cmDeltaE76                 /* deltaEAlgorithm */
};

/* Bradford adaptation matrix */
static const float g_bradfordMatrix[3][3] = {
    { 0.8951f,  0.2664f, -0.1614f},
    {-0.7502f,  1.7135f,  0.0367f},
    { 0.0389f, -0.0685f,  1.0296f}
};

static const float g_bradfordInverse[3][3] = {
    { 0.9869929f, -0.1470543f,  0.1599627f},
    { 0.4323053f,  0.5183603f,  0.0492912f},
    {-0.0085287f,  0.0400428f,  0.9684867f}
};

/* Forward declarations */
static CMError CreateMatrixTransform(CMTransformRef transform);
static CMError CreateLUTTransform(CMTransformRef transform);
static CMError ApplyMatrixTransform(CMTransformRef transform, const CMColor *input, CMColor *output);
static CMError ApplyLUTTransform(CMTransformRef transform, const CMColor *input, CMColor *output);
static void UpdateTransformStats(CMTransformRef transform, uint32_t colorCount, uint32_t timeUs);
static CMError CalculateGamutBoundaryPoints(CMProfileRef profile, CMGamutBoundaryData *gamut);
static float InterpolateLUTValue(const void *lut, CMLUTType type, uint32_t gridPoints,
                                uint32_t channel, const float *coords);

/* ================================================================
 * COLOR MATCHING INITIALIZATION
 * ================================================================ */

CMError CMInitColorMatching(void) {
    if (g_colorMatchingInitialized) {
        return cmNoError;
    }

    /* Initialize default parameters */
    g_defaultParams.intent = cmPerceptual;
    g_defaultParams.quality = cmNormalQuality;
    g_defaultParams.gamutMethod = cmCompressGamut;
    g_defaultParams.blackPointCompensation = true;
    g_defaultParams.useDisplayCompensation = false;
    g_defaultParams.adaptationState = 1.0f;
    g_defaultParams.deltaEAlgorithm = cmDeltaE76;

    g_colorMatchingInitialized = true;
    return cmNoError;
}

CMError CMSetMatchingPreferences(const CMTransformParams *params) {
    if (!params) return cmParameterError;
    g_defaultParams = *params;
    return cmNoError;
}

CMError CMGetMatchingPreferences(CMTransformParams *params) {
    if (!params) return cmParameterError;
    *params = g_defaultParams;
    return cmNoError;
}

/* ================================================================
 * COLOR TRANSFORM CREATION
 * ================================================================ */

CMTransformRef CMCreateColorTransform(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                     CMRenderingIntent intent, CMQuality quality) {
    if (!srcProfile || !dstProfile) return NULL;

    CMTransformRef transform = (CMTransformRef)calloc(1, sizeof(struct CMColorTransform));
    if (!transform) return NULL;

    transform->srcProfile = srcProfile;
    transform->dstProfile = dstProfile;
    transform->intent = intent;
    transform->quality = quality;
    transform->params = g_defaultParams;
    transform->params.intent = intent;
    transform->params.quality = quality;

    /* Clone profile references */
    CMCloneProfileRef(srcProfile);
    CMCloneProfileRef(dstProfile);

    /* Determine transform type and create appropriate transform data */
    CMColorSpace srcSpace, dstSpace;
    CMGetProfileSpace(srcProfile, &srcSpace);
    CMGetProfileSpace(dstProfile, &dstSpace);

    /* For RGB-to-RGB transforms, prefer matrix if possible */
    if (srcSpace == cmRGBSpace && dstSpace == cmRGBSpace && quality != cmBestQuality) {
        transform->type = cmMatrixTRCTransform;
        if (CreateMatrixTransform(transform) != cmNoError) {
            /* Fall back to LUT transform */
            transform->type = cmLUTTransform;
            CreateLUTTransform(transform);
        }
    } else {
        transform->type = cmLUTTransform;
        CreateLUTTransform(transform);
    }

    transform->isValid = (transform->transformData != NULL);
    return transform;
}

CMTransformRef CMCreateProofingTransform(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                        CMProfileRef proofProfile, CMRenderingIntent intent,
                                        CMRenderingIntent proofIntent, bool gamutCheck) {
    if (!srcProfile || !dstProfile || !proofProfile) return NULL;

    CMTransformRef transform = CMCreateColorTransform(srcProfile, dstProfile, intent, cmNormalQuality);
    if (!transform) return NULL;

    transform->proofProfile = proofProfile;
    transform->type = cmPreviewTransform;
    CMCloneProfileRef(proofProfile);

    /* Proofing transforms typically use LUT for flexibility */
    if (transform->transformData) {
        free(transform->transformData);
    }
    CreateLUTTransform(transform);

    return transform;
}

void CMDisposeColorTransform(CMTransformRef transform) {
    if (!transform) return;

    /* Release profile references */
    if (transform->srcProfile) CMCloseProfile(transform->srcProfile);
    if (transform->dstProfile) CMCloseProfile(transform->dstProfile);
    if (transform->proofProfile) CMCloseProfile(transform->proofProfile);

    /* Free transform data */
    if (transform->transformData) {
        switch (transform->type) {
            case cmMatrixTRCTransform: {
                CMMatrixTransformData *matrixData = (CMMatrixTransformData *)transform->transformData;
                if (matrixData->redTRC) free(matrixData->redTRC);
                if (matrixData->greenTRC) free(matrixData->greenTRC);
                if (matrixData->blueTRC) free(matrixData->blueTRC);
                break;
            }
            case cmLUTTransform: {
                CMLUTTransformData *lutData = (CMLUTTransformData *)transform->transformData;
                if (lutData->lut8) free(lutData->lut8);
                if (lutData->lut16) free(lutData->lut16);
                break;
            }
            default:
                break;
        }
        free(transform->transformData);
    }

    free(transform);
}

/* ================================================================
 * COLOR TRANSFORM OPERATIONS
 * ================================================================ */

CMError CMApplyTransform(CMTransformRef transform, const CMColor *input, CMColor *output) {
    if (!transform || !input || !output || !transform->isValid) {
        return cmParameterError;
    }

    CMError err = cmNoError;
    uint32_t startTime = 0; /* Would use platform timer */

    switch (transform->type) {
        case cmMatrixTRCTransform:
            err = ApplyMatrixTransform(transform, input, output);
            break;
        case cmLUTTransform:
        case cmPreviewTransform:
            err = ApplyLUTTransform(transform, input, output);
            break;
        default:
            err = cmMethodError;
            break;
    }

    uint32_t endTime = 0; /* Would use platform timer */
    UpdateTransformStats(transform, 1, endTime - startTime);

    return err;
}

CMError CMApplyTransformArray(CMTransformRef transform, const CMColor *input,
                             CMColor *output, uint32_t count) {
    if (!transform || !input || !output || count == 0) {
        return cmParameterError;
    }

    uint32_t startTime = 0; /* Would use platform timer */
    CMError err = cmNoError;

    for (uint32_t i = 0; i < count && err == cmNoError; i++) {
        err = CMApplyTransform(transform, &input[i], &output[i]);
    }

    uint32_t endTime = 0; /* Would use platform timer */
    UpdateTransformStats(transform, count, endTime - startTime);

    return err;
}

CMError CMApplyTransformWithGamut(CMTransformRef transform, const CMColor *input,
                                 CMColor *output, CMGamutResult *gamutResult) {
    if (!transform || !input || !output || !gamutResult) {
        return cmParameterError;
    }

    /* Apply normal transform */
    CMError err = CMApplyTransform(transform, input, output);
    if (err != cmNoError) return err;

    /* Check gamut */
    void *gamutData;
    err = CMCreateGamutBoundary(transform->dstProfile, &gamutData);
    if (err == cmNoError) {
        err = CMCheckColorGamut(gamutData, output, gamutResult);
        CMDisposeGamutBoundary(gamutData);
    } else {
        /* If can't create gamut, assume in gamut */
        gamutResult->inGamut = true;
        gamutResult->deltaE = 0.0f;
        gamutResult->saturationScale = 1.0f;
        gamutResult->lightnessScale = 1.0f;
    }

    return cmNoError;
}

/* ================================================================
 * GAMUT OPERATIONS
 * ================================================================ */

CMError CMCreateGamutBoundary(CMProfileRef profile, void **gamutData) {
    if (!profile || !gamutData) return cmParameterError;

    CMGamutBoundaryData *gamut = (CMGamutBoundaryData *)calloc(1, sizeof(CMGamutBoundaryData));
    if (!gamut) return cmProfileError;

    /* Get profile white and black points */
    CMError err = CMGetWhitePoint(profile, &gamut->whitePoint);
    if (err != cmNoError) {
        /* Use default D65 white point */
        CMGetIlluminantD65(&gamut->whitePoint);
    }

    err = CMCalculateBlackPoint(profile, &gamut->blackPoint);
    if (err != cmNoError) {
        /* Use default black point */
        gamut->blackPoint.X = 0;
        gamut->blackPoint.Y = 0;
        gamut->blackPoint.Z = 0;
    }

    /* Calculate gamut boundary points */
    err = CalculateGamutBoundaryPoints(profile, gamut);
    if (err != cmNoError) {
        free(gamut);
        return err;
    }

    gamut->isValid = true;
    *gamutData = gamut;
    return cmNoError;
}

CMError CMCheckColorGamut(void *gamutData, const CMColor *color, CMGamutResult *result) {
    if (!gamutData || !color || !result) return cmParameterError;

    CMGamutBoundaryData *gamut = (CMGamutBoundaryData *)gamutData;
    if (!gamut->isValid) return cmInvalidProfileID;

    /* Convert color to Lab for gamut checking */
    CMLABColor labColor;
    CMXYZColor xyzColor;

    /* Simple RGB to XYZ conversion for demonstration */
    CMConvertRGBToXYZ(&color->rgb, &xyzColor, NULL);
    CMConvertXYZToLab(&xyzColor, &labColor, &gamut->whitePoint);

    /* Simple gamut check - assume in gamut if within reasonable Lab bounds */
    bool inGamut = (labColor.L >= 0 && labColor.L <= 100 &&
                   labColor.a >= -128 && labColor.a <= 127 &&
                   labColor.b >= -128 && labColor.b <= 127);

    result->inGamut = inGamut;
    result->deltaE = inGamut ? 0.0f : 5.0f; /* Simplified */
    result->saturationScale = inGamut ? 1.0f : 0.8f;
    result->lightnessScale = inGamut ? 1.0f : 0.9f;

    return cmNoError;
}

CMError CMMapColorToGamut(void *gamutData, const CMColor *input, CMColor *output,
                         CMGamutMethod method) {
    if (!gamutData || !input || !output) return cmParameterError;

    CMGamutResult gamutResult;
    CMError err = CMCheckColorGamut(gamutData, input, &gamutResult);
    if (err != cmNoError) return err;

    if (gamutResult.inGamut) {
        *output = *input;
        return cmNoError;
    }

    /* Apply gamut mapping based on method */
    switch (method) {
        case cmClipGamut:
            /* Simple clipping */
            *output = *input;
            if (output->rgb.red > 65535) output->rgb.red = 65535;
            if (output->rgb.green > 65535) output->rgb.green = 65535;
            if (output->rgb.blue > 65535) output->rgb.blue = 65535;
            break;

        case cmCompressGamut:
            /* Compress by scaling */
            output->rgb.red = (uint16_t)(input->rgb.red * gamutResult.saturationScale);
            output->rgb.green = (uint16_t)(input->rgb.green * gamutResult.saturationScale);
            output->rgb.blue = (uint16_t)(input->rgb.blue * gamutResult.saturationScale);
            break;

        case cmDesaturateGamut:
            /* Desaturate toward neutral */
            {
                uint16_t gray = (uint16_t)((input->rgb.red + input->rgb.green + input->rgb.blue) / 3);
                float desatFactor = 0.7f; /* Move 30% toward gray */
                output->rgb.red = (uint16_t)(input->rgb.red * (1.0f - desatFactor) + gray * desatFactor);
                output->rgb.green = (uint16_t)(input->rgb.green * (1.0f - desatFactor) + gray * desatFactor);
                output->rgb.blue = (uint16_t)(input->rgb.blue * (1.0f - desatFactor) + gray * desatFactor);
            }
            break;

        case cmPerceptualGamut:
            /* Perceptual mapping using Lab */
            {
                CMXYZColor xyzColor;
                CMLABColor labColor;
                CMGamutBoundaryData *gamut = (CMGamutBoundaryData *)gamutData;

                CMConvertRGBToXYZ(&input->rgb, &xyzColor, NULL);
                CMConvertXYZToLab(&xyzColor, &labColor, &gamut->whitePoint);

                /* Compress chroma toward gamut boundary */
                float chromaScale = fminf(1.0f, 100.0f / sqrtf(labColor.a * labColor.a + labColor.b * labColor.b));
                labColor.a = (int32_t)(labColor.a * chromaScale);
                labColor.b = (int32_t)(labColor.b * chromaScale);

                CMConvertLabToXYZ(&labColor, &xyzColor, &gamut->whitePoint);
                CMConvertXYZToRGB(&xyzColor, &output->rgb, NULL);
            }
            break;

        default:
            *output = *input;
            break;
    }

    return cmNoError;
}

void CMDisposeGamutBoundary(void *gamutData) {
    if (!gamutData) return;

    CMGamutBoundaryData *gamut = (CMGamutBoundaryData *)gamutData;
    if (gamut->boundary) {
        free(gamut->boundary);
    }
    free(gamut);
}

/* ================================================================
 * COLOR DIFFERENCE CALCULATIONS
 * ================================================================ */

float CMCalculateColorDifference(const CMColor *color1, const CMColor *color2,
                                CMColorDifferenceAlgorithm algorithm,
                                const CMXYZColor *whitePoint) {
    if (!color1 || !color2) return -1.0f;

    /* Convert colors to Lab */
    CMLABColor lab1, lab2;
    CMXYZColor xyz1, xyz2;
    const CMXYZColor *wp = whitePoint;
    CMXYZColor defaultWP;

    if (!wp) {
        CMGetIlluminantD65(&defaultWP);
        wp = &defaultWP;
    }

    CMConvertRGBToXYZ(&color1->rgb, &xyz1, NULL);
    CMConvertRGBToXYZ(&color2->rgb, &xyz2, NULL);
    CMConvertXYZToLab(&xyz1, &lab1, wp);
    CMConvertXYZToLab(&xyz2, &lab2, wp);

    return CMCalculateDeltaE(&lab1, &lab2, algorithm);
}

float CMCalculateDeltaE(const CMLABColor *lab1, const CMLABColor *lab2,
                       CMColorDifferenceAlgorithm algorithm) {
    if (!lab1 || !lab2) return -1.0f;

    switch (algorithm) {
        case cmDeltaE76:
            return CMCalculateDeltaE76(lab1, lab2);

        case cmDeltaE94: {
            /* CIE94 formula - simplified implementation */
            float dL = lab1->L - lab2->L;
            float da = lab1->a - lab2->a;
            float db = lab1->b - lab2->b;
            float dC = sqrtf(da * da + db * db);
            float dH = sqrtf(dL * dL + da * da + db * db - dC * dC);

            float kL = 1.0f, kC = 1.0f, kH = 1.0f;
            float SL = 1.0f;
            float SC = 1.0f + 0.045f * dC;
            float SH = 1.0f + 0.015f * dC;

            float deltaL = dL / (kL * SL);
            float deltaC = dC / (kC * SC);
            float deltaH = dH / (kH * SH);

            return sqrtf(deltaL * deltaL + deltaC * deltaC + deltaH * deltaH);
        }

        case cmDeltaE2000:
            /* CIEDE2000 - very complex, use simplified version */
            return CMCalculateDeltaE76(lab1, lab2) * 0.8f; /* Rough approximation */

        case cmCMC:
            return CMCalculateCMCDifference(lab1, lab2, 2.0f, 1.0f);

        default:
            return CMCalculateDeltaE76(lab1, lab2);
    }
}

float CMCalculateCMCDifference(const CMLABColor *lab1, const CMLABColor *lab2,
                              float lightness, float chroma) {
    if (!lab1 || !lab2) return -1.0f;

    float dL = lab1->L - lab2->L;
    float da = lab1->a - lab2->a;
    float db = lab1->b - lab2->b;

    float C1 = sqrtf(lab1->a * lab1->a + lab1->b * lab1->b);
    float C2 = sqrtf(lab2->a * lab2->a + lab2->b * lab2->b);
    float dC = C1 - C2;
    float dH = sqrtf(da * da + db * db - dC * dC);

    float T = (C1 < 0.000001f) ? 0.0f : atan2f(lab1->b, lab1->a) * 180.0f / M_PI;
    if (T < 0) T += 360.0f;

    float F = sqrtf(C1 * C1 * C1 * C1 / (C1 * C1 * C1 * C1 + 1900.0f));

    float SL = (lab1->L < 16.0f) ? 0.511f : (0.040975f * lab1->L) / (1.0f + 0.01765f * lab1->L);
    float SC = ((0.0638f * C1) / (1.0f + 0.0131f * C1)) + 0.638f;
    float SH = SC * (F * T + 1.0f - F);

    float deltaL = dL / (lightness * SL);
    float deltaC = dC / (chroma * SC);
    float deltaH = dH / SH;

    return sqrtf(deltaL * deltaL + deltaC * deltaC + deltaH * deltaH);
}

/* ================================================================
 * CHROMATIC ADAPTATION
 * ================================================================ */

CMError CMApplyChromaticAdaptation(const CMXYZColor *color,
                                  const CMXYZColor *srcWhitePoint,
                                  const CMXYZColor *dstWhitePoint,
                                  CMAdaptationMethod method,
                                  CMXYZColor *adaptedColor) {
    if (!color || !srcWhitePoint || !dstWhitePoint || !adaptedColor) {
        return cmParameterError;
    }

    if (method == cmBradfordAdaptation) {
        /* Bradford chromatic adaptation */
        float srcXYZ[3] = {color->X / 100000.0f, color->Y / 100000.0f, color->Z / 100000.0f};
        float srcWP[3] = {srcWhitePoint->X / 100000.0f, srcWhitePoint->Y / 100000.0f, srcWhitePoint->Z / 100000.0f};
        float dstWP[3] = {dstWhitePoint->X / 100000.0f, dstWhitePoint->Y / 100000.0f, dstWhitePoint->Z / 100000.0f};

        /* Convert to Bradford RGB */
        float srcRGB[3], srcWPRGB[3], dstWPRGB[3];
        for (int i = 0; i < 3; i++) {
            srcRGB[i] = g_bradfordMatrix[i][0] * srcXYZ[0] +
                       g_bradfordMatrix[i][1] * srcXYZ[1] +
                       g_bradfordMatrix[i][2] * srcXYZ[2];
            srcWPRGB[i] = g_bradfordMatrix[i][0] * srcWP[0] +
                         g_bradfordMatrix[i][1] * srcWP[1] +
                         g_bradfordMatrix[i][2] * srcWP[2];
            dstWPRGB[i] = g_bradfordMatrix[i][0] * dstWP[0] +
                         g_bradfordMatrix[i][1] * dstWP[1] +
                         g_bradfordMatrix[i][2] * dstWP[2];
        }

        /* Apply adaptation */
        float adaptedRGB[3];
        for (int i = 0; i < 3; i++) {
            if (srcWPRGB[i] > 0.000001f) {
                adaptedRGB[i] = srcRGB[i] * dstWPRGB[i] / srcWPRGB[i];
            } else {
                adaptedRGB[i] = srcRGB[i];
            }
        }

        /* Convert back to XYZ */
        float adaptedXYZ[3];
        for (int i = 0; i < 3; i++) {
            adaptedXYZ[i] = g_bradfordInverse[i][0] * adaptedRGB[0] +
                           g_bradfordInverse[i][1] * adaptedRGB[1] +
                           g_bradfordInverse[i][2] * adaptedRGB[2];
        }

        adaptedColor->X = (int32_t)(adaptedXYZ[0] * 100000.0f);
        adaptedColor->Y = (int32_t)(adaptedXYZ[1] * 100000.0f);
        adaptedColor->Z = (int32_t)(adaptedXYZ[2] * 100000.0f);
    } else {
        /* Simple XYZ scaling adaptation */
        float scaleX = (float)dstWhitePoint->X / srcWhitePoint->X;
        float scaleY = (float)dstWhitePoint->Y / srcWhitePoint->Y;
        float scaleZ = (float)dstWhitePoint->Z / srcWhitePoint->Z;

        adaptedColor->X = (int32_t)(color->X * scaleX);
        adaptedColor->Y = (int32_t)(color->Y * scaleY);
        adaptedColor->Z = (int32_t)(color->Z * scaleZ);
    }

    return cmNoError;
}

/* ================================================================
 * SPECIALIZED TRANSFORMS
 * ================================================================ */

CMError CMPerformColorMatching(CMProfileRef srcProfile, CMProfileRef dstProfile,
                              CMColor *colors, uint32_t count, CMRenderingIntent intent) {
    if (!srcProfile || !dstProfile || !colors || count == 0) {
        return cmParameterError;
    }

    CMTransformRef transform = CMCreateColorTransform(srcProfile, dstProfile, intent, cmNormalQuality);
    if (!transform) return cmMethodError;

    CMError err = CMApplyTransformArray(transform, colors, colors, count);

    CMDisposeColorTransform(transform);
    return err;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static CMError CreateMatrixTransform(CMTransformRef transform) {
    CMMatrixTransformData *matrixData = (CMMatrixTransformData *)calloc(1, sizeof(CMMatrixTransformData));
    if (!matrixData) return cmProfileError;

    /* Get colorants and TRCs from profiles */
    CMXYZColor redPrimary, greenPrimary, bluePrimary;
    CMGetRedColorant(transform->srcProfile, &redPrimary);
    CMGetGreenColorant(transform->srcProfile, &greenPrimary);
    CMGetBlueColorant(transform->srcProfile, &bluePrimary);
    CMGetWhitePoint(transform->srcProfile, &matrixData->srcWhitePoint);
    CMGetWhitePoint(transform->dstProfile, &matrixData->dstWhitePoint);

    /* Get TRC curves */
    uint32_t trcSize;
    CMGetRedTRC(transform->srcProfile, NULL, &trcSize);
    if (trcSize > 0) {
        matrixData->redTRC = (uint16_t *)malloc(trcSize * sizeof(uint16_t));
        matrixData->greenTRC = (uint16_t *)malloc(trcSize * sizeof(uint16_t));
        matrixData->blueTRC = (uint16_t *)malloc(trcSize * sizeof(uint16_t));
        matrixData->trcSize = trcSize;

        if (matrixData->redTRC && matrixData->greenTRC && matrixData->blueTRC) {
            CMGetRedTRC(transform->srcProfile, matrixData->redTRC, &trcSize);
            CMGetGreenTRC(transform->srcProfile, matrixData->greenTRC, &trcSize);
            CMGetBlueTRC(transform->srcProfile, matrixData->blueTRC, &trcSize);
        }
    }

    /* Create transformation matrix */
    CMGetSRGBToXYZMatrix(&matrixData->matrix);

    transform->transformData = matrixData;
    return cmNoError;
}

static CMError CreateLUTTransform(CMTransformRef transform) {
    CMLUTTransformData *lutData = (CMLUTTransformData *)calloc(1, sizeof(CMLUTTransformData));
    if (!lutData) return cmProfileError;

    /* Create a simple 3D LUT for RGB-to-RGB transforms */
    lutData->gridPoints = (transform->quality == cmBestQuality) ? 33 : 17;
    lutData->inputChannels = 3;
    lutData->outputChannels = 3;
    lutData->lutType = cmLUT16;

    uint32_t lutSize = lutData->gridPoints * lutData->gridPoints * lutData->gridPoints *
                      lutData->outputChannels * sizeof(uint16_t);
    lutData->lut16 = (uint16_t *)malloc(lutSize);
    if (!lutData->lut16) {
        free(lutData);
        return cmProfileError;
    }

    /* Initialize LUT with identity transform for now */
    uint32_t index = 0;
    for (uint32_t r = 0; r < lutData->gridPoints; r++) {
        for (uint32_t g = 0; g < lutData->gridPoints; g++) {
            for (uint32_t b = 0; b < lutData->gridPoints; b++) {
                lutData->lut16[index++] = (uint16_t)((r * 65535) / (lutData->gridPoints - 1));
                lutData->lut16[index++] = (uint16_t)((g * 65535) / (lutData->gridPoints - 1));
                lutData->lut16[index++] = (uint16_t)((b * 65535) / (lutData->gridPoints - 1));
            }
        }
    }

    transform->transformData = lutData;
    return cmNoError;
}

static CMError ApplyMatrixTransform(CMTransformRef transform, const CMColor *input, CMColor *output) {
    CMMatrixTransformData *matrixData = (CMMatrixTransformData *)transform->transformData;
    if (!matrixData) return cmMethodError;

    /* Apply TRC curves if available */
    uint16_t linearR = input->rgb.red;
    uint16_t linearG = input->rgb.green;
    uint16_t linearB = input->rgb.blue;

    if (matrixData->redTRC && matrixData->trcSize > 1) {
        uint32_t index = (input->rgb.red * (matrixData->trcSize - 1)) / 65535;
        linearR = matrixData->redTRC[index];
        index = (input->rgb.green * (matrixData->trcSize - 1)) / 65535;
        linearG = matrixData->greenTRC[index];
        index = (input->rgb.blue * (matrixData->trcSize - 1)) / 65535;
        linearB = matrixData->blueTRC[index];
    }

    /* Convert to XYZ using matrix */
    CMRGBColor linearRGB = {linearR, linearG, linearB};
    CMXYZColor xyzColor;
    CMConvertRGBToXYZ(&linearRGB, &xyzColor, &matrixData->matrix);

    /* Apply chromatic adaptation if needed */
    if (matrixData->hasAdaptation) {
        CMXYZColor adaptedXYZ;
        CMApplyChromaticAdaptation(&xyzColor, &matrixData->srcWhitePoint,
                                  &matrixData->dstWhitePoint, cmBradfordAdaptation, &adaptedXYZ);
        xyzColor = adaptedXYZ;
    }

    /* Convert back to RGB */
    CMConvertXYZToRGB(&xyzColor, &output->rgb, NULL);

    return cmNoError;
}

static CMError ApplyLUTTransform(CMTransformRef transform, const CMColor *input, CMColor *output) {
    CMLUTTransformData *lutData = (CMLUTTransformData *)transform->transformData;
    if (!lutData || !lutData->lut16) return cmMethodError;

    /* Normalize input to 0-1 range */
    float coords[3] = {
        input->rgb.red / 65535.0f,
        input->rgb.green / 65535.0f,
        input->rgb.blue / 65535.0f
    };

    /* Interpolate LUT values */
    float result[3];
    for (int channel = 0; channel < 3; channel++) {
        result[channel] = InterpolateLUTValue(lutData->lut16, lutData->lutType,
                                            lutData->gridPoints, channel, coords);
    }

    /* Convert back to 16-bit */
    output->rgb.red = (uint16_t)(result[0] * 65535.0f);
    output->rgb.green = (uint16_t)(result[1] * 65535.0f);
    output->rgb.blue = (uint16_t)(result[2] * 65535.0f);

    return cmNoError;
}

static void UpdateTransformStats(CMTransformRef transform, uint32_t colorCount, uint32_t timeUs) {
    if (!transform) return;

    transform->stats.totalColors += colorCount;
    transform->stats.transformTime += timeUs;
    /* Other statistics would be updated here */
}

static CMError CalculateGamutBoundaryPoints(CMProfileRef profile, CMGamutBoundaryData *gamut) {
    /* Simplified gamut boundary calculation */
    gamut->pointCount = 1000; /* Number of boundary points */
    gamut->boundary = (float *)malloc(gamut->pointCount * 3 * sizeof(float));
    if (!gamut->boundary) return cmProfileError;

    /* Generate points on gamut boundary (simplified sphere for now) */
    for (uint32_t i = 0; i < gamut->pointCount; i++) {
        float theta = (2.0f * M_PI * i) / gamut->pointCount;
        float phi = M_PI * (i % 100) / 100.0f;

        gamut->boundary[i * 3 + 0] = 50.0f * sinf(phi) * cosf(theta); /* a* */
        gamut->boundary[i * 3 + 1] = 50.0f * sinf(phi) * sinf(theta); /* b* */
        gamut->boundary[i * 3 + 2] = 50.0f + 50.0f * cosf(phi);       /* L* */
    }

    gamut->volume = 4.0f * M_PI * 50.0f * 50.0f * 50.0f / 3.0f; /* Sphere volume */
    return cmNoError;
}

static float InterpolateLUTValue(const void *lut, CMLUTType type, uint32_t gridPoints,
                                uint32_t channel, const float *coords) {
    const uint16_t *lut16 = (const uint16_t *)lut;

    /* Trilinear interpolation in 3D LUT */
    float r = coords[0] * (gridPoints - 1);
    float g = coords[1] * (gridPoints - 1);
    float b = coords[2] * (gridPoints - 1);

    uint32_t r0 = (uint32_t)floorf(r);
    uint32_t g0 = (uint32_t)floorf(g);
    uint32_t b0 = (uint32_t)floorf(b);

    uint32_t r1 = fminf(r0 + 1, gridPoints - 1);
    uint32_t g1 = fminf(g0 + 1, gridPoints - 1);
    uint32_t b1 = fminf(b0 + 1, gridPoints - 1);

    float dr = r - r0;
    float dg = g - g0;
    float db = b - b0;

    /* Get 8 corner values */
    uint32_t outputChannels = 3;
    uint32_t stride = outputChannels;

    float c000 = lut16[((r0 * gridPoints + g0) * gridPoints + b0) * stride + channel] / 65535.0f;
    float c001 = lut16[((r0 * gridPoints + g0) * gridPoints + b1) * stride + channel] / 65535.0f;
    float c010 = lut16[((r0 * gridPoints + g1) * gridPoints + b0) * stride + channel] / 65535.0f;
    float c011 = lut16[((r0 * gridPoints + g1) * gridPoints + b1) * stride + channel] / 65535.0f;
    float c100 = lut16[((r1 * gridPoints + g0) * gridPoints + b0) * stride + channel] / 65535.0f;
    float c101 = lut16[((r1 * gridPoints + g0) * gridPoints + b1) * stride + channel] / 65535.0f;
    float c110 = lut16[((r1 * gridPoints + g1) * gridPoints + b0) * stride + channel] / 65535.0f;
    float c111 = lut16[((r1 * gridPoints + g1) * gridPoints + b1) * stride + channel] / 65535.0f;

    /* Trilinear interpolation */
    float c00 = c000 * (1.0f - db) + c001 * db;
    float c01 = c010 * (1.0f - db) + c011 * db;
    float c10 = c100 * (1.0f - db) + c101 * db;
    float c11 = c110 * (1.0f - db) + c111 * db;

    float c0 = c00 * (1.0f - dg) + c01 * dg;
    float c1 = c10 * (1.0f - dg) + c11 * dg;

    return c0 * (1.0f - dr) + c1 * dr;
}
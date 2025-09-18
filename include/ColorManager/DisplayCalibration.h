/*
 * DisplayCalibration.h - Display Calibration and Gamma Correction
 *
 * Display calibration, gamma correction, and monitor profiling for accurate
 * color reproduction and professional color management workflows.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#ifndef DISPLAYCALIBRATION_H
#define DISPLAYCALIBRATION_H

#include "ColorManager.h"
#include "ColorSpaces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * DISPLAY CALIBRATION CONSTANTS
 * ================================================================ */

/* Calibration states */
typedef enum {
    cmCalibrationNotStarted = 0,    /* Not started */
    cmCalibrationInProgress = 1,    /* In progress */
    cmCalibrationCompleted  = 2,    /* Completed successfully */
    cmCalibrationFailed     = 3,    /* Failed */
    cmCalibrationCancelled  = 4     /* Cancelled by user */
} CMCalibrationState;

/* Display types */
typedef enum {
    cmDisplayCRT        = 0,    /* CRT monitor */
    cmDisplayLCD        = 1,    /* LCD monitor */
    cmDisplayOLED       = 2,    /* OLED display */
    cmDisplayPlasma     = 3,    /* Plasma display */
    cmDisplayProjector  = 4,    /* Projector */
    cmDisplayGeneric    = 5     /* Generic display */
} CMDisplayType;

/* Gamma correction methods */
typedef enum {
    cmGammaSimple       = 0,    /* Simple gamma curve */
    cmGammaSRGB         = 1,    /* sRGB gamma function */
    cmGammaRec709       = 2,    /* Rec. 709 gamma */
    cmGammaRec2020      = 3,    /* Rec. 2020 gamma */
    cmGammaCustom       = 4,    /* Custom gamma curve */
    cmGammaLUT          = 5     /* Lookup table */
} CMGammaMethod;

/* ================================================================
 * DISPLAY CALIBRATION STRUCTURES
 * ================================================================ */

/* Display information */
typedef struct {
    char            name[128];          /* Display name */
    char            manufacturer[64];   /* Manufacturer */
    char            model[64];          /* Model number */
    char            serialNumber[32];   /* Serial number */
    CMDisplayType   displayType;       /* Display type */
    uint32_t        width;             /* Width in pixels */
    uint32_t        height;            /* Height in pixels */
    float           physicalWidth;     /* Physical width in mm */
    float           physicalHeight;    /* Physical height in mm */
    uint32_t        bitDepth;          /* Bits per channel */
    bool            isColorManaged;    /* Color management supported */
    bool            isHDR;             /* HDR capability */
    float           maxLuminance;      /* Maximum luminance (cd/m²) */
    float           minLuminance;      /* Minimum luminance (cd/m²) */
} CMDisplayInfo;

/* Gamma curve definition */
typedef struct {
    CMGammaMethod   method;            /* Gamma method */
    float           gamma;             /* Gamma value */
    uint32_t        lutSize;           /* LUT size (if using LUT) */
    uint16_t       *redLUT;            /* Red channel LUT */
    uint16_t       *greenLUT;          /* Green channel LUT */
    uint16_t       *blueLUT;           /* Blue channel LUT */
    bool            isValid;           /* Curve validity */
} CMGammaCurve;

/* Calibration target */
typedef struct {
    CMXYZColor      whitePoint;        /* Target white point */
    float           gamma;             /* Target gamma */
    float           luminance;         /* Target luminance (cd/m²) */
    float           blackLevel;        /* Target black level */
    float           contrast;          /* Target contrast ratio */
    uint16_t        temperature;       /* Color temperature (K) */
} CMCalibrationTarget;

/* Measurement data */
typedef struct {
    CMXYZColor      measuredXYZ;       /* Measured XYZ values */
    CMRGBColor      inputRGB;          /* Input RGB values */
    float           luminance;         /* Measured luminance */
    uint32_t        patchIndex;       /* Color patch index */
    bool            isValid;           /* Measurement validity */
} CMMeasurementData;

/* Calibration results */
typedef struct {
    CMCalibrationState  state;         /* Calibration state */
    CMGammaCurve       redCurve;       /* Red gamma curve */
    CMGammaCurve       greenCurve;     /* Green gamma curve */
    CMGammaCurve       blueCurve;      /* Blue gamma curve */
    CMXYZColor         achievedWhitePoint; /* Achieved white point */
    float              achievedGamma;  /* Achieved gamma */
    float              achievedLuminance; /* Achieved luminance */
    float              deltaE;         /* Average color error */
    float              maxDeltaE;      /* Maximum color error */
    uint32_t           measurementCount; /* Number of measurements */
    CMMeasurementData  *measurements;  /* Measurement data array */
    char               notes[256];     /* Calibration notes */
} CMCalibrationResults;

/* ================================================================
 * DISPLAY ENUMERATION
 * ================================================================ */

/* Get number of displays */
uint32_t CMGetDisplayCount(void);

/* Get display information */
CMError CMGetDisplayInfo(uint32_t displayIndex, CMDisplayInfo *displayInfo);

/* Get primary display */
CMError CMGetPrimaryDisplay(uint32_t *displayIndex);

/* Set primary display */
CMError CMSetPrimaryDisplay(uint32_t displayIndex);

/* Get display by name */
CMError CMGetDisplayByName(const char *name, uint32_t *displayIndex);

/* ================================================================
 * DISPLAY CALIBRATION
 * ================================================================ */

/* Start display calibration */
CMError CMStartDisplayCalibration(uint32_t displayIndex, const CMCalibrationTarget *target);

/* Stop display calibration */
CMError CMStopDisplayCalibration(uint32_t displayIndex);

/* Get calibration state */
CMError CMGetCalibrationState(uint32_t displayIndex, CMCalibrationState *state);

/* Add measurement */
CMError CMAddMeasurement(uint32_t displayIndex, const CMMeasurementData *measurement);

/* Complete calibration */
CMError CMCompleteCalibration(uint32_t displayIndex, CMCalibrationResults *results);

/* Apply calibration */
CMError CMApplyCalibration(uint32_t displayIndex, const CMCalibrationResults *calibration);

/* Remove calibration */
CMError CMRemoveCalibration(uint32_t displayIndex);

/* ================================================================
 * GAMMA CORRECTION
 * ================================================================ */

/* Create gamma curve */
CMError CMCreateGammaCurve(CMGammaMethod method, float gamma, uint32_t lutSize, CMGammaCurve *curve);

/* Apply gamma correction */
CMError CMApplyGammaCorrection(uint32_t displayIndex, const CMGammaCurve *curve);

/* Get current gamma curves */
CMError CMGetDisplayGammaCurves(uint32_t displayIndex, CMGammaCurve *redCurve,
                               CMGammaCurve *greenCurve, CMGammaCurve *blueCurve);

/* Reset gamma curves */
CMError CMResetDisplayGamma(uint32_t displayIndex);

/* Validate gamma curve */
bool CMValidateGammaCurve(const CMGammaCurve *curve);

/* ================================================================
 * WHITE POINT ADJUSTMENT
 * ================================================================ */

/* Set display white point */
CMError CMSetDisplayWhitePoint(uint32_t displayIndex, const CMXYZColor *whitePoint);

/* Get display white point */
CMError CMGetDisplayWhitePoint(uint32_t displayIndex, CMXYZColor *whitePoint);

/* Set color temperature */
CMError CMSetDisplayColorTemperature(uint32_t displayIndex, uint16_t temperature);

/* Get color temperature */
CMError CMGetDisplayColorTemperature(uint32_t displayIndex, uint16_t *temperature);

/* Adjust display brightness */
CMError CMSetDisplayBrightness(uint32_t displayIndex, float brightness);

/* Get display brightness */
CMError CMGetDisplayBrightness(uint32_t displayIndex, float *brightness);

/* ================================================================
 * MEASUREMENT INTEGRATION
 * ================================================================ */

/* Colorimeter interface */
typedef struct {
    char            name[64];          /* Colorimeter name */
    char            manufacturer[32];  /* Manufacturer */
    bool            isConnected;       /* Connection status */
    bool            supportsSpectral;  /* Spectral measurement support */
    float           accuracy;          /* Accuracy specification */
    void           *deviceHandle;      /* Device handle */
} CMColorimeter;

/* Enumerate colorimeters */
CMError CMEnumerateColorimeters(CMColorimeter *colorimeters, uint32_t *count);

/* Connect to colorimeter */
CMError CMConnectColorimeter(uint32_t colorimeterIndex);

/* Disconnect colorimeter */
CMError CMDisconnectColorimeter(uint32_t colorimeterIndex);

/* Measure color patch */
CMError CMeasureColorPatch(uint32_t colorimeterIndex, const CMRGBColor *rgb,
                          CMMeasurementData *measurement);

/* Calibrate colorimeter */
CMError CMCalibrateColorimeter(uint32_t colorimeterIndex);

/* ================================================================
 * DISPLAY PROFILING
 * ================================================================ */

/* Create display profile */
CMError CMCreateDisplayProfile(uint32_t displayIndex, const CMCalibrationResults *calibration,
                              CMProfileRef *profile);

/* Install display profile */
CMError CMInstallDisplayProfile(uint32_t displayIndex, CMProfileRef profile);

/* Get current display profile */
CMError CMGetDisplayProfile(uint32_t displayIndex, CMProfileRef *profile);

/* Remove display profile */
CMError CMRemoveDisplayProfile(uint32_t displayIndex);

/* ================================================================
 * CALIBRATION VALIDATION
 * ================================================================ */

/* Validate display calibration */
CMError CMValidateDisplayCalibration(uint32_t displayIndex, const CMRGBColor *testColors,
                                    uint32_t testCount, float *averageDeltaE, float *maxDeltaE);

/* Generate test patterns */
CMError CMGenerateTestPatterns(CMRGBColor *testColors, uint32_t *count, uint32_t maxColors);

/* Analyze calibration quality */
CMError CMAnalyzeCalibrationQuality(const CMCalibrationResults *calibration,
                                   float *uniformity, float *accuracy, float *repeatability);

/* ================================================================
 * AMBIENT LIGHT COMPENSATION
 * ================================================================ */

/* Ambient light sensor */
typedef struct {
    float           illuminance;       /* Ambient illuminance (lux) */
    CMXYZColor      ambientColor;      /* Ambient light color */
    uint16_t        temperature;       /* Ambient color temperature */
    bool            isValid;           /* Measurement validity */
} CMAmbientLight;

/* Measure ambient light */
CMError CMMeasureAmbientLight(CMAmbientLight *ambientLight);

/* Apply ambient compensation */
CMError CMApplyAmbientCompensation(uint32_t displayIndex, const CMAmbientLight *ambient);

/* Set ambient compensation mode */
CMError CMSetAmbientCompensationMode(uint32_t displayIndex, bool enabled);

/* ================================================================
 * HDR DISPLAY SUPPORT
 * ================================================================ */

/* HDR metadata */
typedef struct {
    float           maxLuminance;      /* Maximum luminance (cd/m²) */
    float           maxFrameAverage;   /* Maximum frame average (cd/m²) */
    float           minLuminance;      /* Minimum luminance (cd/m²) */
    CMXYZColor      redPrimary;        /* Red primary */
    CMXYZColor      greenPrimary;      /* Green primary */
    CMXYZColor      bluePrimary;       /* Blue primary */
    CMXYZColor      whitePoint;        /* White point */
} CMHDRMetadata;

/* Configure HDR display */
CMError CMConfigureHDRDisplay(uint32_t displayIndex, const CMHDRMetadata *hdrMetadata);

/* Get HDR capabilities */
CMError CMGetHDRCapabilities(uint32_t displayIndex, CMHDRMetadata *capabilities);

/* Set HDR tone mapping */
CMError CMSetHDRToneMapping(uint32_t displayIndex, float gamma, float maxLuminance);

/* ================================================================
 * CALIBRATION PERSISTENCE
 * ================================================================ */

/* Save calibration to file */
CMError CMSaveCalibrationToFile(const CMCalibrationResults *calibration, const char *filename);

/* Load calibration from file */
CMError CMLoadCalibrationFromFile(const char *filename, CMCalibrationResults *calibration);

/* Get calibration directory */
CMError CMGetCalibrationDirectory(char *directory, uint32_t maxLength);

/* Set calibration directory */
CMError CMSetCalibrationDirectory(const char *directory);

/* ================================================================
 * AUTOMATIC CALIBRATION
 * ================================================================ */

/* Automatic calibration configuration */
typedef struct {
    uint32_t        measurementCount; /* Number of measurements */
    float           tolerance;         /* Convergence tolerance */
    uint32_t        maxIterations;    /* Maximum iterations */
    bool            adaptivePatches;   /* Use adaptive patch selection */
    bool            quickMode;         /* Quick calibration mode */
} CMAutoCalibrationConfig;

/* Start automatic calibration */
CMError CMStartAutoCalibration(uint32_t displayIndex, uint32_t colorimeterIndex,
                              const CMCalibrationTarget *target,
                              const CMAutoCalibrationConfig *config);

/* Get auto calibration progress */
CMError CMGetAutoCalibrationProgress(uint32_t displayIndex, float *progress, char *status);

/* ================================================================
 * DISPLAY UNIFORMITY
 * ================================================================ */

/* Uniformity measurement */
typedef struct {
    uint32_t        gridWidth;         /* Measurement grid width */
    uint32_t        gridHeight;        /* Measurement grid height */
    float          *luminanceMap;      /* Luminance uniformity map */
    CMXYZColor     *colorMap;          /* Color uniformity map */
    float           averageLuminance;  /* Average luminance */
    float           luminanceUniformity; /* Luminance uniformity % */
    float           colorUniformity;   /* Color uniformity (Delta E) */
} CMUniformityResults;

/* Measure display uniformity */
CMError CMMeasureDisplayUniformity(uint32_t displayIndex, uint32_t colorimeterIndex,
                                  uint32_t gridWidth, uint32_t gridHeight,
                                  CMUniformityResults *results);

/* Apply uniformity correction */
CMError CMApplyUniformityCorrection(uint32_t displayIndex, const CMUniformityResults *uniformity);

/* Dispose uniformity results */
void CMDisposeUniformityResults(CMUniformityResults *results);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAYCALIBRATION_H */
/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * DisplayCalibration.c - Display Calibration and Gamma Correction Implementation
 *
 * Implementation of display calibration, gamma correction, and monitor profiling
 * for accurate color reproduction and professional color management.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#include "../include/ColorManager/DisplayCalibration.h"
#include "../include/ColorManager/ColorSpaces.h"
#include "../include/ColorManager/ICCProfiles.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

/* ================================================================
 * INTERNAL STRUCTURES
 * ================================================================ */

/* Display calibration context */
typedef struct {
    CMDisplayInfo           displayInfo;        /* Display information */
    CMCalibrationTarget     target;             /* Calibration target */
    CMCalibrationResults    results;            /* Calibration results */
    CMCalibrationState      state;              /* Current state */
    CMMeasurementData      *measurements;       /* Measurement buffer */
    uint32_t               measurementCount;    /* Number of measurements */
    uint32_t               measurementCapacity; /* Measurement buffer size */
    bool                   isActive;            /* Calibration active */
} CMDisplayCalibrationContext;

/* Colorimeter device */
typedef struct {
    CMColorimeter          info;                /* Device information */
    bool                   isConnected;         /* Connection status */
    void                  *platformHandle;     /* Platform-specific handle */
} CMColorimeterDevice;

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static CMDisplayCalibrationContext *g_displayContexts = NULL;
static uint32_t g_displayCount = 0;
static uint32_t g_primaryDisplay = 0;

static CMColorimeterDevice *g_colorimeters = NULL;
static uint32_t g_colorimeterCount = 0;

static char g_calibrationDirectory[256] = "";
static bool g_displaySystemInitialized = false;

/* Standard gamma values */
static const float g_standardGammas[] = {1.8f, 2.2f, 2.4f};
static const uint16_t g_standardTemperatures[] = {5000, 5500, 6500, 9300};

/* Forward declarations */
static CMError InitializeDisplaySystem(void);
static CMError EnumerateDisplays(void);
static CMError CreateDefaultGammaCurve(CMGammaCurve *curve, float gamma, uint32_t lutSize);
static CMError ApplyGammaCurveToLUT(const CMGammaCurve *curve, uint16_t *lut, uint32_t size);
static float CalculateGammaFromCurve(const uint16_t *curve, uint32_t size);
static CMError ValidateMeasurementData(const CMMeasurementData *measurement);
static CMError CalculateCalibrationCurves(CMDisplayCalibrationContext *context);

/* ================================================================
 * DISPLAY ENUMERATION
 * ================================================================ */

uint32_t CMGetDisplayCount(void) {
    if (!g_displaySystemInitialized) {
        InitializeDisplaySystem();
    }
    return g_displayCount;
}

CMError CMGetDisplayInfo(uint32_t displayIndex, CMDisplayInfo *displayInfo) {
    if (!displayInfo) return cmParameterError;
    if (displayIndex >= g_displayCount) return cmParameterError;

    if (!g_displaySystemInitialized) {
        CMError err = InitializeDisplaySystem();
        if (err != cmNoError) return err;
    }

    *displayInfo = g_displayContexts[displayIndex].displayInfo;
    return cmNoError;
}

CMError CMGetPrimaryDisplay(uint32_t *displayIndex) {
    if (!displayIndex) return cmParameterError;
    if (!g_displaySystemInitialized) {
        CMError err = InitializeDisplaySystem();
        if (err != cmNoError) return err;
    }

    *displayIndex = g_primaryDisplay;
    return cmNoError;
}

CMError CMSetPrimaryDisplay(uint32_t displayIndex) {
    if (displayIndex >= g_displayCount) return cmParameterError;
    g_primaryDisplay = displayIndex;
    return cmNoError;
}

CMError CMGetDisplayByName(const char *name, uint32_t *displayIndex) {
    if (!name || !displayIndex) return cmParameterError;

    if (!g_displaySystemInitialized) {
        CMError err = InitializeDisplaySystem();
        if (err != cmNoError) return err;
    }

    for (uint32_t i = 0; i < g_displayCount; i++) {
        if (strcmp(g_displayContexts[i].displayInfo.name, name) == 0) {
            *displayIndex = i;
            return cmNoError;
        }
    }

    return cmProfileError;
}

/* ================================================================
 * DISPLAY CALIBRATION
 * ================================================================ */

CMError CMStartDisplayCalibration(uint32_t displayIndex, const CMCalibrationTarget *target) {
    if (displayIndex >= g_displayCount || !target) return cmParameterError;

    if (!g_displaySystemInitialized) {
        CMError err = InitializeDisplaySystem();
        if (err != cmNoError) return err;
    }

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];

    /* Check if calibration is already in progress */
    if (context->state == cmCalibrationInProgress) {
        return cmMethodError;
    }

    /* Initialize calibration */
    context->target = *target;
    context->state = cmCalibrationInProgress;
    context->measurementCount = 0;
    context->isActive = true;

    /* Initialize results structure */
    memset(&context->results, 0, sizeof(CMCalibrationResults));
    context->results.state = cmCalibrationInProgress;

    /* Allocate measurement buffer */
    if (!context->measurements) {
        context->measurementCapacity = 1000; /* Default capacity */
        context->measurements = (CMMeasurementData *)calloc(context->measurementCapacity,
                                                          sizeof(CMMeasurementData));
        if (!context->measurements) {
            context->state = cmCalibrationFailed;
            return cmProfileError;
        }
    }

    return cmNoError;
}

CMError CMStopDisplayCalibration(uint32_t displayIndex) {
    if (displayIndex >= g_displayCount) return cmParameterError;

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];
    context->state = cmCalibrationCancelled;
    context->isActive = false;

    return cmNoError;
}

CMError CMGetCalibrationState(uint32_t displayIndex, CMCalibrationState *state) {
    if (displayIndex >= g_displayCount || !state) return cmParameterError;

    *state = g_displayContexts[displayIndex].state;
    return cmNoError;
}

CMError CMAddMeasurement(uint32_t displayIndex, const CMMeasurementData *measurement) {
    if (displayIndex >= g_displayCount || !measurement) return cmParameterError;

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];

    if (context->state != cmCalibrationInProgress) {
        return cmMethodError;
    }

    /* Validate measurement data */
    CMError err = ValidateMeasurementData(measurement);
    if (err != cmNoError) return err;

    /* Expand buffer if needed */
    if (context->measurementCount >= context->measurementCapacity) {
        context->measurementCapacity *= 2;
        CMMeasurementData *newBuffer = (CMMeasurementData *)realloc(context->measurements,
            context->measurementCapacity * sizeof(CMMeasurementData));
        if (!newBuffer) return cmProfileError;
        context->measurements = newBuffer;
    }

    /* Add measurement */
    context->measurements[context->measurementCount] = *measurement;
    context->measurementCount++;

    return cmNoError;
}

CMError CMCompleteCalibration(uint32_t displayIndex, CMCalibrationResults *results) {
    if (displayIndex >= g_displayCount || !results) return cmParameterError;

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];

    if (context->state != cmCalibrationInProgress) {
        return cmMethodError;
    }

    /* Calculate calibration curves from measurements */
    CMError err = CalculateCalibrationCurves(context);
    if (err != cmNoError) {
        context->state = cmCalibrationFailed;
        return err;
    }

    /* Finalize results */
    context->results.state = cmCalibrationCompleted;
    context->results.measurementCount = context->measurementCount;
    context->results.measurements = context->measurements;

    context->state = cmCalibrationCompleted;
    context->isActive = false;

    *results = context->results;
    return cmNoError;
}

/* ================================================================
 * GAMMA CORRECTION
 * ================================================================ */

CMError CMCreateGammaCurve(CMGammaMethod method, float gamma, uint32_t lutSize, CMGammaCurve *curve) {
    if (!curve || lutSize == 0) return cmParameterError;

    memset(curve, 0, sizeof(CMGammaCurve));
    curve->method = method;
    curve->gamma = gamma;
    curve->lutSize = lutSize;

    /* Allocate LUT arrays */
    curve->redLUT = (uint16_t *)malloc(lutSize * sizeof(uint16_t));
    curve->greenLUT = (uint16_t *)malloc(lutSize * sizeof(uint16_t));
    curve->blueLUT = (uint16_t *)malloc(lutSize * sizeof(uint16_t));

    if (!curve->redLUT || !curve->greenLUT || !curve->blueLUT) {
        if (curve->redLUT) free(curve->redLUT);
        if (curve->greenLUT) free(curve->greenLUT);
        if (curve->blueLUT) free(curve->blueLUT);
        return cmProfileError;
    }

    /* Generate curve based on method */
    switch (method) {
        case cmGammaSimple:
            for (uint32_t i = 0; i < lutSize; i++) {
                float input = (float)i / (lutSize - 1);
                float output = powf(input, 1.0f / gamma);
                uint16_t value = (uint16_t)(output * 65535.0f);
                curve->redLUT[i] = value;
                curve->greenLUT[i] = value;
                curve->blueLUT[i] = value;
            }
            break;

        case cmGammaSRGB:
            for (uint32_t i = 0; i < lutSize; i++) {
                float input = (float)i / (lutSize - 1);
                float output;

                /* sRGB gamma function */
                if (input <= 0.04045f) {
                    output = input / 12.92f;
                } else {
                    output = powf((input + 0.055f) / 1.055f, 2.4f);
                }

                uint16_t value = (uint16_t)(output * 65535.0f);
                curve->redLUT[i] = value;
                curve->greenLUT[i] = value;
                curve->blueLUT[i] = value;
            }
            break;

        case cmGammaRec709:
            /* Rec. 709 gamma */
            for (uint32_t i = 0; i < lutSize; i++) {
                float input = (float)i / (lutSize - 1);
                float output;

                if (input < 0.081f) {
                    output = input / 4.5f;
                } else {
                    output = powf((input + 0.099f) / 1.099f, 1.0f / 0.45f);
                }

                uint16_t value = (uint16_t)(output * 65535.0f);
                curve->redLUT[i] = value;
                curve->greenLUT[i] = value;
                curve->blueLUT[i] = value;
            }
            break;

        default:
            /* Linear curve */
            for (uint32_t i = 0; i < lutSize; i++) {
                uint16_t value = (uint16_t)((i * 65535) / (lutSize - 1));
                curve->redLUT[i] = value;
                curve->greenLUT[i] = value;
                curve->blueLUT[i] = value;
            }
            break;
    }

    curve->isValid = true;
    return cmNoError;
}

CMError CMApplyGammaCorrection(uint32_t displayIndex, const CMGammaCurve *curve) {
    if (displayIndex >= g_displayCount || !curve) return cmParameterError;
    if (!curve->isValid) return cmParameterError;

    /* In a real implementation, this would apply the gamma curves to the display hardware */
    /* For now, we'll store the curves in the display context */
    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];

    /* Copy the gamma curve */
    context->results.redCurve = *curve;
    context->results.greenCurve = *curve;
    context->results.blueCurve = *curve;

    return cmNoError;
}

CMError CMGetDisplayGammaCurves(uint32_t displayIndex, CMGammaCurve *redCurve,
                               CMGammaCurve *greenCurve, CMGammaCurve *blueCurve) {
    if (displayIndex >= g_displayCount) return cmParameterError;

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];

    if (redCurve) *redCurve = context->results.redCurve;
    if (greenCurve) *greenCurve = context->results.greenCurve;
    if (blueCurve) *blueCurve = context->results.blueCurve;

    return cmNoError;
}

CMError CMResetDisplayGamma(uint32_t displayIndex) {
    if (displayIndex >= g_displayCount) return cmParameterError;

    /* Create linear gamma curves */
    CMGammaCurve linearCurve;
    CMError err = CMCreateGammaCurve(cmGammaSimple, 1.0f, 256, &linearCurve);
    if (err != cmNoError) return err;

    err = CMApplyGammaCorrection(displayIndex, &linearCurve);

    /* Clean up */
    if (linearCurve.redLUT) free(linearCurve.redLUT);
    if (linearCurve.greenLUT) free(linearCurve.greenLUT);
    if (linearCurve.blueLUT) free(linearCurve.blueLUT);

    return err;
}

bool CMValidateGammaCurve(const CMGammaCurve *curve) {
    if (!curve || !curve->isValid) return false;
    if (!curve->redLUT || !curve->greenLUT || !curve->blueLUT) return false;
    if (curve->lutSize == 0) return false;

    /* Check for monotonic increasing */
    for (uint32_t i = 1; i < curve->lutSize; i++) {
        if (curve->redLUT[i] < curve->redLUT[i-1] ||
            curve->greenLUT[i] < curve->greenLUT[i-1] ||
            curve->blueLUT[i] < curve->blueLUT[i-1]) {
            return false;
        }
    }

    return true;
}

/* ================================================================
 * WHITE POINT ADJUSTMENT
 * ================================================================ */

CMError CMSetDisplayWhitePoint(uint32_t displayIndex, const CMXYZColor *whitePoint) {
    if (displayIndex >= g_displayCount || !whitePoint) return cmParameterError;

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];
    context->results.achievedWhitePoint = *whitePoint;

    /* In a real implementation, this would adjust the display hardware */
    return cmNoError;
}

CMError CMGetDisplayWhitePoint(uint32_t displayIndex, CMXYZColor *whitePoint) {
    if (displayIndex >= g_displayCount || !whitePoint) return cmParameterError;

    CMDisplayCalibrationContext *context = &g_displayContexts[displayIndex];
    *whitePoint = context->results.achievedWhitePoint;

    return cmNoError;
}

CMError CMSetDisplayColorTemperature(uint32_t displayIndex, uint16_t temperature) {
    if (displayIndex >= g_displayCount) return cmParameterError;

    /* Convert color temperature to XYZ white point */
    CMXYZColor whitePoint;
    CMError err = CMColorTemperatureToXYZ(temperature, &whitePoint);
    if (err != cmNoError) return err;

    return CMSetDisplayWhitePoint(displayIndex, &whitePoint);
}

CMError CMGetDisplayColorTemperature(uint32_t displayIndex, uint16_t *temperature) {
    if (displayIndex >= g_displayCount || !temperature) return cmParameterError;

    CMXYZColor whitePoint;
    CMError err = CMGetDisplayWhitePoint(displayIndex, &whitePoint);
    if (err != cmNoError) return err;

    return CMXYZToColorTemperature(&whitePoint, temperature);
}

/* ================================================================
 * MEASUREMENT INTEGRATION
 * ================================================================ */

CMError CMEnumerateColorimeters(CMColorimeter *colorimeters, uint32_t *count) {
    if (!count) return cmParameterError;

    /* Simulate detection of colorimeters */
    if (!g_colorimeters) {
        g_colorimeterCount = 1; /* Simulate one colorimeter */
        g_colorimeters = (CMColorimeterDevice *)calloc(g_colorimeterCount, sizeof(CMColorimeterDevice));
        if (!g_colorimeters) {
            *count = 0;
            return cmProfileError;
        }

        /* Set up simulated colorimeter */
        strcpy(g_colorimeters[0].info.name, "Simulated Colorimeter");
        strcpy(g_colorimeters[0].info.manufacturer, "System 7.1");
        g_colorimeters[0].info.isConnected = false;
        g_colorimeters[0].info.supportsSpectral = false;
        g_colorimeters[0].info.accuracy = 0.5f; /* ±0.5 Delta E */
        g_colorimeters[0].info.deviceHandle = NULL;
    }

    if (colorimeters && *count >= g_colorimeterCount) {
        for (uint32_t i = 0; i < g_colorimeterCount; i++) {
            colorimeters[i] = g_colorimeters[i].info;
        }
    }

    *count = g_colorimeterCount;
    return cmNoError;
}

CMError CMConnectColorimeter(uint32_t colorimeterIndex) {
    if (colorimeterIndex >= g_colorimeterCount) return cmParameterError;

    g_colorimeters[colorimeterIndex].isConnected = true;
    g_colorimeters[colorimeterIndex].info.isConnected = true;

    return cmNoError;
}

CMError CMDisconnectColorimeter(uint32_t colorimeterIndex) {
    if (colorimeterIndex >= g_colorimeterCount) return cmParameterError;

    g_colorimeters[colorimeterIndex].isConnected = false;
    g_colorimeters[colorimeterIndex].info.isConnected = false;

    return cmNoError;
}

CMError CMeasureColorPatch(uint32_t colorimeterIndex, const CMRGBColor *rgb,
                          CMMeasurementData *measurement) {
    if (colorimeterIndex >= g_colorimeterCount || !rgb || !measurement) return cmParameterError;

    if (!g_colorimeters[colorimeterIndex].isConnected) {
        return cmMethodError;
    }

    /* Simulate colorimeter measurement */
    measurement->inputRGB = *rgb;

    /* Convert RGB to XYZ with some noise to simulate real measurement */
    CMConvertRGBToXYZ(rgb, &measurement->measuredXYZ, NULL);

    /* Add small amount of measurement noise */
    float noise = 0.02f; /* 2% noise */
    measurement->measuredXYZ.X = (int32_t)(measurement->measuredXYZ.X * (1.0f + (rand() / (float)RAND_MAX - 0.5f) * noise));
    measurement->measuredXYZ.Y = (int32_t)(measurement->measuredXYZ.Y * (1.0f + (rand() / (float)RAND_MAX - 0.5f) * noise));
    measurement->measuredXYZ.Z = (int32_t)(measurement->measuredXYZ.Z * (1.0f + (rand() / (float)RAND_MAX - 0.5f) * noise));

    /* Calculate luminance */
    measurement->luminance = measurement->measuredXYZ.Y / 100000.0f * 100.0f; /* cd/m² */

    measurement->patchIndex = 0;
    measurement->isValid = true;

    return cmNoError;
}

/* ================================================================
 * CALIBRATION PERSISTENCE
 * ================================================================ */

CMError CMSaveCalibrationToFile(const CMCalibrationResults *calibration, const char *filename) {
    if (!calibration || !filename) return cmParameterError;

    FILE *file = fopen(filename, "wb");
    if (!file) return cmProfileError;

    /* Write calibration header */
    uint32_t magic = 0x43414C42; /* 'CALB' */
    uint32_t version = 1;

    if (fwrite(&magic, 4, 1, file) != 1 ||
        fwrite(&version, 4, 1, file) != 1) {
        fclose(file);
        return cmProfileError;
    }

    /* Write calibration results */
    if (fwrite(calibration, sizeof(CMCalibrationResults), 1, file) != 1) {
        fclose(file);
        return cmProfileError;
    }

    /* Write measurement data if present */
    if (calibration->measurements && calibration->measurementCount > 0) {
        if (fwrite(calibration->measurements, sizeof(CMMeasurementData),
                  calibration->measurementCount, file) != calibration->measurementCount) {
            fclose(file);
            return cmProfileError;
        }
    }

    fclose(file);
    return cmNoError;
}

CMError CMLoadCalibrationFromFile(const char *filename, CMCalibrationResults *calibration) {
    if (!filename || !calibration) return cmParameterError;

    FILE *file = fopen(filename, "rb");
    if (!file) return cmProfileError;

    /* Read and verify header */
    uint32_t magic, version;
    if (fread(&magic, 4, 1, file) != 1 ||
        fread(&version, 4, 1, file) != 1) {
        fclose(file);
        return cmProfileError;
    }

    if (magic != 0x43414C42 || version != 1) {
        fclose(file);
        return cmFatalProfileErr;
    }

    /* Read calibration results */
    if (fread(calibration, sizeof(CMCalibrationResults), 1, file) != 1) {
        fclose(file);
        return cmProfileError;
    }

    /* Read measurement data if present */
    if (calibration->measurementCount > 0) {
        calibration->measurements = (CMMeasurementData *)malloc(
            calibration->measurementCount * sizeof(CMMeasurementData));
        if (!calibration->measurements) {
            fclose(file);
            return cmProfileError;
        }

        if (fread(calibration->measurements, sizeof(CMMeasurementData),
                 calibration->measurementCount, file) != calibration->measurementCount) {
            free(calibration->measurements);
            calibration->measurements = NULL;
            fclose(file);
            return cmProfileError;
        }
    } else {
        calibration->measurements = NULL;
    }

    fclose(file);
    return cmNoError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static CMError InitializeDisplaySystem(void) {
    if (g_displaySystemInitialized) return cmNoError;

    /* Set default calibration directory */
    if (strlen(g_calibrationDirectory) == 0) {
#ifdef _WIN32
        strcpy(g_calibrationDirectory, "C:\\Windows\\System32\\spool\\drivers\\color");
#elif defined(__APPLE__)
        strcpy(g_calibrationDirectory, "/Library/ColorSync/Profiles/Displays");
#else
        strcpy(g_calibrationDirectory, "/usr/share/color/icc");
#endif
    }

    /* Enumerate displays */
    CMError err = EnumerateDisplays();
    if (err != cmNoError) return err;

    g_displaySystemInitialized = true;
    return cmNoError;
}

static CMError EnumerateDisplays(void) {
    /* Simulate detection of displays */
    g_displayCount = 1; /* At least one display */

#ifdef _WIN32
    g_displayCount = GetSystemMetrics(SM_CMONITORS);
#elif defined(__APPLE__)
    /* Use CoreGraphics to enumerate displays */
    CGDirectDisplayID displays[16];
    uint32_t displayCount;
    if (CGGetActiveDisplayList(16, displays, &displayCount) == kCGErrorSuccess) {
        g_displayCount = displayCount;
    }
#else
    /* X11 display enumeration would go here */
    g_displayCount = 1;
#endif

    if (g_displayCount == 0) g_displayCount = 1;
    if (g_displayCount > 16) g_displayCount = 16; /* Reasonable limit */

    /* Allocate display contexts */
    g_displayContexts = (CMDisplayCalibrationContext *)calloc(g_displayCount,
                                                              sizeof(CMDisplayCalibrationContext));
    if (!g_displayContexts) return cmProfileError;

    /* Initialize display information */
    for (uint32_t i = 0; i < g_displayCount; i++) {
        CMDisplayInfo *info = &g_displayContexts[i].displayInfo;

        snprintf(info->name, sizeof(info->name), "Display %u", i + 1);
        strcpy(info->manufacturer, "Generic");
        strcpy(info->model, "Monitor");
        snprintf(info->serialNumber, sizeof(info->serialNumber), "SN%08u", i + 1);

        info->displayType = cmDisplayLCD;
        info->width = 1920;
        info->height = 1080;
        info->physicalWidth = 510.0f; /* mm */
        info->physicalHeight = 287.0f; /* mm */
        info->bitDepth = 8;
        info->isColorManaged = true;
        info->isHDR = false;
        info->maxLuminance = 250.0f; /* cd/m² */
        info->minLuminance = 0.3f;   /* cd/m² */

        /* Initialize default calibration state */
        g_displayContexts[i].state = cmCalibrationNotStarted;
        g_displayContexts[i].isActive = false;

        /* Set default white point to D65 */
        CMGetIlluminantD65(&g_displayContexts[i].results.achievedWhitePoint);
        g_displayContexts[i].results.achievedGamma = 2.2f;
        g_displayContexts[i].results.achievedLuminance = 120.0f;
    }

    return cmNoError;
}

static CMError CalculateCalibrationCurves(CMDisplayCalibrationContext *context) {
    if (!context || context->measurementCount == 0) return cmParameterError;

    /* Simplified calibration curve calculation */
    /* In a real implementation, this would use sophisticated curve fitting */

    /* Calculate average gamma from measurements */
    float totalGamma = 0.0f;
    uint32_t validMeasurements = 0;

    for (uint32_t i = 0; i < context->measurementCount; i++) {
        const CMMeasurementData *measurement = &context->measurements[i];
        if (!measurement->isValid) continue;

        /* Calculate gamma for this measurement */
        float input = (measurement->inputRGB.red + measurement->inputRGB.green + measurement->inputRGB.blue) / (3.0f * 65535.0f);
        float output = measurement->luminance / context->results.achievedLuminance;

        if (input > 0.0f && output > 0.0f) {
            float gamma = logf(output) / logf(input);
            if (gamma > 0.5f && gamma < 5.0f) { /* Reasonable range */
                totalGamma += gamma;
                validMeasurements++;
            }
        }
    }

    if (validMeasurements > 0) {
        context->results.achievedGamma = totalGamma / validMeasurements;
    } else {
        context->results.achievedGamma = 2.2f; /* Default */
    }

    /* Create gamma curves based on calculated gamma */
    CMError err = CMCreateGammaCurve(cmGammaSimple, context->results.achievedGamma, 256,
                                    &context->results.redCurve);
    if (err != cmNoError) return err;

    context->results.greenCurve = context->results.redCurve;
    context->results.blueCurve = context->results.redCurve;

    /* Calculate color accuracy */
    context->results.deltaE = 2.0f;    /* Simulated average */
    context->results.maxDeltaE = 5.0f; /* Simulated maximum */

    return cmNoError;
}

static CMError ValidateMeasurementData(const CMMeasurementData *measurement) {
    if (!measurement) return cmParameterError;
    if (!measurement->isValid) return cmParameterError;

    /* Check for reasonable XYZ values */
    if (measurement->measuredXYZ.X < 0 || measurement->measuredXYZ.Y < 0 || measurement->measuredXYZ.Z < 0) {
        return cmRangeError;
    }

    /* Check for reasonable luminance */
    if (measurement->luminance < 0.0f || measurement->luminance > 10000.0f) {
        return cmRangeError;
    }

    return cmNoError;
}
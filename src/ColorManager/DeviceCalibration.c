/*
 * DeviceCalibration.c - Device Color Calibration and Profiling Implementation
 *
 * Implementation of device calibration and profiling for scanners, printers,
 * cameras, and other color input/output devices.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#include "../include/ColorManager/DeviceCalibration.h"
#include "../include/ColorManager/ColorSpaces.h"
#include "../include/ColorManager/ICCProfiles.h"
#include "../include/ColorManager/ColorMatching.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

/* ================================================================
 * INTERNAL STRUCTURES
 * ================================================================ */

/* Device registry entry */
typedef struct {
    CMDeviceInfo        info;               /* Device information */
    CMProfileRef        currentProfile;     /* Current device profile */
    void               *calibrationData;    /* Device-specific calibration data */
    bool               isCalibrated;        /* Calibration status */
    uint32_t           lastCalibration;     /* Last calibration timestamp */
} CMDeviceEntry;

/* Batch calibration state */
typedef struct {
    CMBatchCalibrationConfig config;        /* Configuration */
    uint32_t               currentDevice;   /* Current device index */
    float                  progress;        /* Overall progress */
    bool                   isActive;        /* Batch operation active */
    char                   status[128];     /* Current status */
} CMBatchCalibrationState;

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static CMDeviceEntry *g_devices = NULL;
static uint32_t g_deviceCount = 0;
static uint32_t g_deviceCapacity = 0;

static CMBatchCalibrationState g_batchState = {0};
static bool g_deviceSystemInitialized = false;

/* Standard test charts */
static CMTestChart *g_it8Chart = NULL;
static CMTestChart *g_colorCheckerChart = NULL;

/* Forward declarations */
static CMError InitializeDeviceSystem(void);
static CMError RegisterDevice(const CMDeviceInfo *deviceInfo);
static CMDeviceEntry *FindDevice(const char *deviceName);
static CMError CreateStandardTestCharts(void);
static CMError AnalyzeColorPatches(const void *imageData, const CMTestChart *testChart,
                                  CMDeviceMeasurement *measurements, uint32_t *measurementCount);
static CMError CalculateColorCorrectionMatrix(const CMDeviceMeasurement *measurements,
                                             uint32_t measurementCount, CMColorMatrix *matrix);
static CMError GenerateToneCurves(const CMDeviceMeasurement *measurements, uint32_t measurementCount,
                                uint16_t **redCurve, uint16_t **greenCurve, uint16_t **blueCurve,
                                uint32_t curveSize);

/* ================================================================
 * DEVICE ENUMERATION
 * ================================================================ */

CMError CMEnumerateColorDevices(CMDeviceInfo *devices, uint32_t *count, CMDeviceType deviceType) {
    if (!count) return cmParameterError;

    if (!g_deviceSystemInitialized) {
        CMError err = InitializeDeviceSystem();
        if (err != cmNoError) return err;
    }

    uint32_t foundCount = 0;

    /* Count matching devices */
    for (uint32_t i = 0; i < g_deviceCount; i++) {
        if (deviceType == cmDeviceGeneric || g_devices[i].info.deviceType == deviceType) {
            if (devices && foundCount < *count) {
                devices[foundCount] = g_devices[i].info;
            }
            foundCount++;
        }
    }

    *count = foundCount;
    return cmNoError;
}

CMError CMGetDeviceInfo(const char *deviceName, CMDeviceInfo *deviceInfo) {
    if (!deviceName || !deviceInfo) return cmParameterError;

    CMDeviceEntry *device = FindDevice(deviceName);
    if (!device) return cmProfileError;

    *deviceInfo = device->info;
    return cmNoError;
}

bool CMDeviceSupportsColorManagement(const char *deviceName) {
    if (!deviceName) return false;

    CMDeviceEntry *device = FindDevice(deviceName);
    return device ? device->info.isColorManaged : false;
}

/* ================================================================
 * SCANNER CALIBRATION
 * ================================================================ */

CMError CMInitializeScannerCalibration(const char *scannerName, const CMScannerSettings *settings) {
    if (!scannerName || !settings) return cmParameterError;

    CMDeviceEntry *device = FindDevice(scannerName);
    if (!device) return cmProfileError;
    if (device->info.deviceType != cmDeviceScanner) return cmParameterError;

    /* Initialize scanner-specific calibration data */
    device->calibrationData = malloc(sizeof(CMScannerSettings));
    if (!device->calibrationData) return cmProfileError;

    memcpy(device->calibrationData, settings, sizeof(CMScannerSettings));
    return cmNoError;
}

CMError CMScanCalibrationTarget(const char *scannerName, const CMTestChart *testChart,
                               void *scannedImage, uint32_t *imageSize) {
    if (!scannerName || !testChart || !imageSize) return cmParameterError;

    CMDeviceEntry *device = FindDevice(scannerName);
    if (!device) return cmProfileError;

    /* In a real implementation, this would interface with scanner drivers */
    /* For simulation, we'll create a synthetic scanned image */

    uint32_t width = testChart->chartWidth * 100;  /* 100 pixels per patch */
    uint32_t height = testChart->chartHeight * 100;
    uint32_t bytesPerPixel = 3; /* RGB */
    uint32_t requiredSize = width * height * bytesPerPixel;

    if (scannedImage && *imageSize >= requiredSize) {
        uint8_t *imageData = (uint8_t *)scannedImage;

        /* Generate synthetic scan data based on test chart */
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t patchX = x / 100;
                uint32_t patchY = y / 100;
                uint32_t patchIndex = patchY * testChart->chartWidth + patchX;

                if (patchIndex < testChart->patchCount) {
                    CMRGBColor refColor = testChart->referenceColors[patchIndex];

                    /* Add some scanner noise */
                    float noise = 0.05f; /* 5% noise */
                    uint8_t r = (uint8_t)((refColor.red >> 8) * (1.0f + (rand() / (float)RAND_MAX - 0.5f) * noise));
                    uint8_t g = (uint8_t)((refColor.green >> 8) * (1.0f + (rand() / (float)RAND_MAX - 0.5f) * noise));
                    uint8_t b = (uint8_t)((refColor.blue >> 8) * (1.0f + (rand() / (float)RAND_MAX - 0.5f) * noise));

                    uint32_t pixelIndex = (y * width + x) * 3;
                    imageData[pixelIndex + 0] = r;
                    imageData[pixelIndex + 1] = g;
                    imageData[pixelIndex + 2] = b;
                } else {
                    /* White background */
                    uint32_t pixelIndex = (y * width + x) * 3;
                    imageData[pixelIndex + 0] = 255;
                    imageData[pixelIndex + 1] = 255;
                    imageData[pixelIndex + 2] = 255;
                }
            }
        }
    }

    *imageSize = requiredSize;
    return cmNoError;
}

CMError CMAnalyzeScannerCalibration(const void *scannedImage, const CMTestChart *testChart,
                                   CMDeviceCalibrationResults *results) {
    if (!scannedImage || !testChart || !results) return cmParameterError;

    /* Initialize results */
    memset(results, 0, sizeof(CMDeviceCalibrationResults));
    results->deviceType = cmDeviceScanner;
    results->method = cmCalibrationITF;

    /* Allocate measurement array */
    results->measurements = (CMDeviceMeasurement *)calloc(testChart->patchCount, sizeof(CMDeviceMeasurement));
    if (!results->measurements) return cmProfileError;

    /* Analyze color patches */
    CMError err = AnalyzeColorPatches(scannedImage, testChart, results->measurements, &results->measurementCount);
    if (err != cmNoError) {
        free(results->measurements);
        return err;
    }

    /* Calculate color correction matrix */
    err = CalculateColorCorrectionMatrix(results->measurements, results->measurementCount,
                                        &results->correctionMatrix);
    if (err != cmNoError) {
        free(results->measurements);
        return err;
    }

    /* Generate tone curves */
    results->curveSize = 256;
    err = GenerateToneCurves(results->measurements, results->measurementCount,
                           &results->toneCurves[0], &results->toneCurves[1], &results->toneCurves[2],
                           results->curveSize);
    if (err != cmNoError) {
        free(results->measurements);
        return err;
    }

    /* Calculate accuracy statistics */
    float totalDeltaE = 0.0f;
    float maxDeltaE = 0.0f;

    for (uint32_t i = 0; i < results->measurementCount; i++) {
        totalDeltaE += results->measurements[i].deltaE;
        if (results->measurements[i].deltaE > maxDeltaE) {
            maxDeltaE = results->measurements[i].deltaE;
        }
    }

    results->averageDeltaE = totalDeltaE / results->measurementCount;
    results->maxDeltaE = maxDeltaE;

    strcpy(results->notes, "Scanner calibration completed successfully");

    return cmNoError;
}

/* ================================================================
 * PRINTER CALIBRATION
 * ================================================================ */

CMError CMInitializePrinterCalibration(const char *printerName, const CMPrinterSettings *settings) {
    if (!printerName || !settings) return cmParameterError;

    CMDeviceEntry *device = FindDevice(printerName);
    if (!device) return cmProfileError;
    if (device->info.deviceType != cmDevicePrinter) return cmParameterError;

    /* Initialize printer-specific calibration data */
    device->calibrationData = malloc(sizeof(CMPrinterSettings));
    if (!device->calibrationData) return cmProfileError;

    memcpy(device->calibrationData, settings, sizeof(CMPrinterSettings));
    return cmNoError;
}

CMError CMPrintCalibrationTarget(const char *printerName, const CMTestChart *testChart) {
    if (!printerName || !testChart) return cmParameterError;

    CMDeviceEntry *device = FindDevice(printerName);
    if (!device) return cmProfileError;

    /* In a real implementation, this would send the test chart to the printer */
    /* For simulation, we'll just validate the operation */

    printf("Printing calibration target to printer: %s\n", printerName);
    printf("Test chart: %s (%u patches)\n", testChart->name, testChart->patchCount);

    return cmNoError;
}

CMError CMeasurePrintedTarget(const CMTestChart *testChart, uint32_t colorimeterIndex,
                             CMDeviceCalibrationResults *results) {
    if (!testChart || !results) return cmParameterError;

    /* Initialize results for printer */
    memset(results, 0, sizeof(CMDeviceCalibrationResults));
    results->deviceType = cmDevicePrinter;
    results->method = cmCalibrationITF;

    /* Allocate measurement array */
    results->measurements = (CMDeviceMeasurement *)calloc(testChart->patchCount, sizeof(CMDeviceMeasurement));
    if (!results->measurements) return cmProfileError;

    /* Simulate measurement of printed patches */
    for (uint32_t i = 0; i < testChart->patchCount; i++) {
        CMDeviceMeasurement *measurement = &results->measurements[i];
        measurement->patchIndex = i;
        measurement->referenceColor = testChart->referenceColors[i];

        /* Simulate printer color shift */
        measurement->measuredColor.red = (uint16_t)(testChart->referenceColors[i].red * 0.95f);
        measurement->measuredColor.green = (uint16_t)(testChart->referenceColors[i].green * 0.98f);
        measurement->measuredColor.blue = (uint16_t)(testChart->referenceColors[i].blue * 0.92f);

        /* Convert to XYZ */
        CMConvertRGBToXYZ(&measurement->measuredColor, &measurement->measuredXYZ, NULL);

        /* Calculate color error */
        CMXYZColor refXYZ;
        CMConvertRGBToXYZ(&measurement->referenceColor, &refXYZ, NULL);

        CMLABColor refLab, measLab;
        CMXYZColor d65;
        CMGetIlluminantD65(&d65);
        CMConvertXYZToLab(&refXYZ, &refLab, &d65);
        CMConvertXYZToLab(&measurement->measuredXYZ, &measLab, &d65);

        measurement->deltaE = CMCalculateDeltaE76(&refLab, &measLab);
        measurement->isValid = true;
    }

    results->measurementCount = testChart->patchCount;

    /* Calculate statistics */
    float totalDeltaE = 0.0f;
    float maxDeltaE = 0.0f;

    for (uint32_t i = 0; i < results->measurementCount; i++) {
        totalDeltaE += results->measurements[i].deltaE;
        if (results->measurements[i].deltaE > maxDeltaE) {
            maxDeltaE = results->measurements[i].deltaE;
        }
    }

    results->averageDeltaE = totalDeltaE / results->measurementCount;
    results->maxDeltaE = maxDeltaE;

    return cmNoError;
}

/* ================================================================
 * TEST CHART MANAGEMENT
 * ================================================================ */

CMError CMCreateTestChart(const char *name, const char *reference, uint32_t patchCount,
                         CMTestChart **testChart) {
    if (!name || !testChart || patchCount == 0) return cmParameterError;

    CMTestChart *chart = (CMTestChart *)calloc(1, sizeof(CMTestChart));
    if (!chart) return cmProfileError;

    strncpy(chart->name, name, sizeof(chart->name) - 1);
    if (reference) {
        strncpy(chart->reference, reference, sizeof(chart->reference) - 1);
    }

    chart->patchCount = patchCount;
    chart->referenceColors = (CMRGBColor *)calloc(patchCount, sizeof(CMRGBColor));
    chart->patchPositions = (float *)calloc(patchCount * 2, sizeof(float)); /* x, y pairs */

    if (!chart->referenceColors || !chart->patchPositions) {
        CMDisposeTestChart(chart);
        return cmProfileError;
    }

    *testChart = chart;
    return cmNoError;
}

CMError CMGenerateIT8Chart(CMTestChart **testChart) {
    if (!testChart) return cmParameterError;

    CMError err = CMCreateTestChart("IT8.7/2", "IT8.7/2", 288, testChart);
    if (err != cmNoError) return err;

    CMTestChart *chart = *testChart;
    chart->chartWidth = 24;
    chart->chartHeight = 12;

    /* Generate standard IT8 color patches */
    uint32_t patchIndex = 0;

    /* Gray scale patches */
    for (uint32_t i = 0; i < 24; i++) {
        float gray = (float)i / 23.0f;
        uint16_t grayValue = (uint16_t)(gray * 65535.0f);
        chart->referenceColors[patchIndex].red = grayValue;
        chart->referenceColors[patchIndex].green = grayValue;
        chart->referenceColors[patchIndex].blue = grayValue;

        chart->patchPositions[patchIndex * 2] = (float)i / 24.0f;
        chart->patchPositions[patchIndex * 2 + 1] = 0.0f;
        patchIndex++;
    }

    /* Color patches - simplified RGB grid */
    for (uint32_t r = 0; r < 6; r++) {
        for (uint32_t g = 0; g < 6; g++) {
            for (uint32_t b = 0; b < 6; b++) {
                if (patchIndex >= chart->patchCount) break;

                chart->referenceColors[patchIndex].red = (uint16_t)((r * 65535) / 5);
                chart->referenceColors[patchIndex].green = (uint16_t)((g * 65535) / 5);
                chart->referenceColors[patchIndex].blue = (uint16_t)((b * 65535) / 5);

                uint32_t row = (patchIndex - 24) / 24 + 1;
                uint32_t col = (patchIndex - 24) % 24;
                chart->patchPositions[patchIndex * 2] = (float)col / 24.0f;
                chart->patchPositions[patchIndex * 2 + 1] = (float)row / 12.0f;
                patchIndex++;
            }
        }
    }

    return cmNoError;
}

CMError CMGenerateColorCheckerChart(CMTestChart **testChart) {
    if (!testChart) return cmParameterError;

    CMError err = CMCreateTestChart("ColorChecker", "X-Rite ColorChecker", 24, testChart);
    if (err != cmNoError) return err;

    CMTestChart *chart = *testChart;
    chart->chartWidth = 6;
    chart->chartHeight = 4;

    /* Standard ColorChecker colors (approximate sRGB values) */
    const CMRGBColor colorCheckerColors[24] = {
        {29538, 20560, 14906},  /* Dark Skin */
        {52685, 39578, 29538},  /* Light Skin */
        {25700, 33153, 42662},  /* Blue Sky */
        {22359, 27756, 12850},  /* Foliage */
        {35980, 35723, 55255},  /* Blue Flower */
        {23644, 49344, 44461},  /* Bluish Green */
        {61166, 31354, 11565},  /* Orange */
        {24415, 24415, 47802},  /* Purplish Blue */
        {51657, 22616, 25186},  /* Moderate Red */
        {15163, 12079, 26985},  /* Purple */
        {42662, 53199, 19275},  /* Yellow Green */
        {57825, 39321, 9509},   /* Orange Yellow */
        {11822, 20817, 52428},  /* Blue */
        {16962, 39578, 20560},  /* Green */
        {41120, 14649, 13878},  /* Red */
        {65535, 52942, 10537},  /* Yellow */
        {52942, 22873, 44204},  /* Magenta */
        {9252, 33153, 42405},   /* Cyan */
        {60652, 60652, 60652},  /* White (.05 * D) */
        {49601, 49601, 49601},  /* Neutral 8 (.23 * D) */
        {39578, 39578, 39578},  /* Neutral 6.5 (.44 * D) */
        {29538, 29538, 29538},  /* Neutral 5 (.70 * D) */
        {19789, 19789, 19789},  /* Neutral 3.5 (1.05 * D) */
        {8738, 8738, 8738}     /* Black (1.50 * D) */
    };

    for (uint32_t i = 0; i < 24; i++) {
        chart->referenceColors[i] = colorCheckerColors[i];
        chart->patchPositions[i * 2] = (float)(i % 6) / 6.0f;
        chart->patchPositions[i * 2 + 1] = (float)(i / 6) / 4.0f;
    }

    return cmNoError;
}

void CMDisposeTestChart(CMTestChart *testChart) {
    if (!testChart) return;

    if (testChart->referenceColors) {
        free(testChart->referenceColors);
    }
    if (testChart->patchPositions) {
        free(testChart->patchPositions);
    }
    free(testChart);
}

/* ================================================================
 * DEVICE PROFILE MANAGEMENT
 * ================================================================ */

CMError CMInstallDeviceProfile(const char *deviceName, CMProfileRef profile) {
    if (!deviceName || !profile) return cmParameterError;

    CMDeviceEntry *device = FindDevice(deviceName);
    if (!device) return cmProfileError;

    /* Release existing profile */
    if (device->currentProfile) {
        CMCloseProfile(device->currentProfile);
    }

    /* Install new profile */
    device->currentProfile = profile;
    CMCloneProfileRef(profile);
    device->isCalibrated = true;

    return cmNoError;
}

CMError CMGetDeviceProfile(const char *deviceName, CMProfileRef *profile) {
    if (!deviceName || !profile) return cmParameterError;

    CMDeviceEntry *device = FindDevice(deviceName);
    if (!device || !device->currentProfile) return cmNoCurrentProfile;

    *profile = device->currentProfile;
    CMCloneProfileRef(device->currentProfile);

    return cmNoError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static CMError InitializeDeviceSystem(void) {
    if (g_deviceSystemInitialized) return cmNoError;

    /* Initialize device registry */
    g_deviceCapacity = 16;
    g_devices = (CMDeviceEntry *)calloc(g_deviceCapacity, sizeof(CMDeviceEntry));
    if (!g_devices) return cmProfileError;

    /* Register some default devices for demonstration */
    CMDeviceInfo scannerInfo = {0};
    strcpy(scannerInfo.name, "Default Scanner");
    strcpy(scannerInfo.manufacturer, "Generic");
    strcpy(scannerInfo.model, "Scanner");
    scannerInfo.deviceType = cmDeviceScanner;
    scannerInfo.isColorManaged = true;
    scannerInfo.maxResolutionDPI = 2400;
    scannerInfo.bitDepth = 16;
    RegisterDevice(&scannerInfo);

    CMDeviceInfo printerInfo = {0};
    strcpy(printerInfo.name, "Default Printer");
    strcpy(printerInfo.manufacturer, "Generic");
    strcpy(printerInfo.model, "Color Printer");
    printerInfo.deviceType = cmDevicePrinter;
    printerInfo.isColorManaged = true;
    printerInfo.maxResolutionDPI = 1200;
    printerInfo.bitDepth = 8;
    RegisterDevice(&printerInfo);

    /* Create standard test charts */
    CreateStandardTestCharts();

    g_deviceSystemInitialized = true;
    return cmNoError;
}

static CMError RegisterDevice(const CMDeviceInfo *deviceInfo) {
    if (!deviceInfo) return cmParameterError;

    /* Expand array if needed */
    if (g_deviceCount >= g_deviceCapacity) {
        g_deviceCapacity *= 2;
        CMDeviceEntry *newDevices = (CMDeviceEntry *)realloc(g_devices,
            g_deviceCapacity * sizeof(CMDeviceEntry));
        if (!newDevices) return cmProfileError;
        g_devices = newDevices;
    }

    /* Add device */
    CMDeviceEntry *entry = &g_devices[g_deviceCount];
    memset(entry, 0, sizeof(CMDeviceEntry));
    entry->info = *deviceInfo;
    entry->isCalibrated = false;
    entry->lastCalibration = 0;

    g_deviceCount++;
    return cmNoError;
}

static CMDeviceEntry *FindDevice(const char *deviceName) {
    if (!deviceName) return NULL;

    for (uint32_t i = 0; i < g_deviceCount; i++) {
        if (strcmp(g_devices[i].info.name, deviceName) == 0) {
            return &g_devices[i];
        }
    }
    return NULL;
}

static CMError CreateStandardTestCharts(void) {
    /* Create IT8 chart */
    CMError err = CMGenerateIT8Chart(&g_it8Chart);
    if (err != cmNoError) return err;

    /* Create ColorChecker chart */
    err = CMGenerateColorCheckerChart(&g_colorCheckerChart);
    if (err != cmNoError) return err;

    return cmNoError;
}

static CMError AnalyzeColorPatches(const void *imageData, const CMTestChart *testChart,
                                  CMDeviceMeasurement *measurements, uint32_t *measurementCount) {
    if (!imageData || !testChart || !measurements || !measurementCount) return cmParameterError;

    const uint8_t *image = (const uint8_t *)imageData;
    uint32_t patchSize = 100; /* Assume 100x100 pixels per patch */

    for (uint32_t i = 0; i < testChart->patchCount; i++) {
        CMDeviceMeasurement *measurement = &measurements[i];
        measurement->patchIndex = i;
        measurement->referenceColor = testChart->referenceColors[i];

        /* Calculate patch position in image */
        uint32_t patchX = (i % testChart->chartWidth) * patchSize;
        uint32_t patchY = (i / testChart->chartWidth) * patchSize;

        /* Sample center of patch */
        uint32_t centerX = patchX + patchSize / 2;
        uint32_t centerY = patchY + patchSize / 2;
        uint32_t imageWidth = testChart->chartWidth * patchSize;

        uint32_t pixelIndex = (centerY * imageWidth + centerX) * 3;
        uint8_t r = image[pixelIndex + 0];
        uint8_t g = image[pixelIndex + 1];
        uint8_t b = image[pixelIndex + 2];

        measurement->measuredColor.red = (uint16_t)(r << 8);
        measurement->measuredColor.green = (uint16_t)(g << 8);
        measurement->measuredColor.blue = (uint16_t)(b << 8);

        /* Convert to XYZ */
        CMConvertRGBToXYZ(&measurement->measuredColor, &measurement->measuredXYZ, NULL);

        /* Calculate color error */
        CMXYZColor refXYZ;
        CMConvertRGBToXYZ(&measurement->referenceColor, &refXYZ, NULL);

        CMLABColor refLab, measLab;
        CMXYZColor d65;
        CMGetIlluminantD65(&d65);
        CMConvertXYZToLab(&refXYZ, &refLab, &d65);
        CMConvertXYZToLab(&measurement->measuredXYZ, &measLab, &d65);

        measurement->deltaE = CMCalculateDeltaE76(&refLab, &measLab);
        measurement->isValid = true;
    }

    *measurementCount = testChart->patchCount;
    return cmNoError;
}

static CMError CalculateColorCorrectionMatrix(const CMDeviceMeasurement *measurements,
                                             uint32_t measurementCount, CMColorMatrix *matrix) {
    if (!measurements || !matrix || measurementCount == 0) return cmParameterError;

    /* Simplified matrix calculation - in practice this would use least squares fitting */
    /* Initialize with identity matrix */
    memset(matrix, 0, sizeof(CMColorMatrix));
    matrix->matrix[0][0] = 1.0f;
    matrix->matrix[1][1] = 1.0f;
    matrix->matrix[2][2] = 1.0f;

    /* Calculate average correction factors */
    float redCorrection = 0.0f;
    float greenCorrection = 0.0f;
    float blueCorrection = 0.0f;
    uint32_t validCount = 0;

    for (uint32_t i = 0; i < measurementCount; i++) {
        if (!measurements[i].isValid) continue;

        if (measurements[i].measuredColor.red > 0) {
            redCorrection += (float)measurements[i].referenceColor.red / measurements[i].measuredColor.red;
        }
        if (measurements[i].measuredColor.green > 0) {
            greenCorrection += (float)measurements[i].referenceColor.green / measurements[i].measuredColor.green;
        }
        if (measurements[i].measuredColor.blue > 0) {
            blueCorrection += (float)measurements[i].referenceColor.blue / measurements[i].measuredColor.blue;
        }
        validCount++;
    }

    if (validCount > 0) {
        matrix->matrix[0][0] = redCorrection / validCount;
        matrix->matrix[1][1] = greenCorrection / validCount;
        matrix->matrix[2][2] = blueCorrection / validCount;
    }

    return cmNoError;
}

static CMError GenerateToneCurves(const CMDeviceMeasurement *measurements, uint32_t measurementCount,
                                uint16_t **redCurve, uint16_t **greenCurve, uint16_t **blueCurve,
                                uint32_t curveSize) {
    if (!measurements || !redCurve || !greenCurve || !blueCurve || curveSize == 0) return cmParameterError;

    /* Allocate curves */
    *redCurve = (uint16_t *)malloc(curveSize * sizeof(uint16_t));
    *greenCurve = (uint16_t *)malloc(curveSize * sizeof(uint16_t));
    *blueCurve = (uint16_t *)malloc(curveSize * sizeof(uint16_t));

    if (!*redCurve || !*greenCurve || !*blueCurve) {
        if (*redCurve) free(*redCurve);
        if (*greenCurve) free(*greenCurve);
        if (*blueCurve) free(*blueCurve);
        return cmProfileError;
    }

    /* Generate linear curves as default */
    for (uint32_t i = 0; i < curveSize; i++) {
        uint16_t value = (uint16_t)((i * 65535) / (curveSize - 1));
        (*redCurve)[i] = value;
        (*greenCurve)[i] = value;
        (*blueCurve)[i] = value;
    }

    /* In a real implementation, this would fit curves to the measurement data */

    return cmNoError;
}
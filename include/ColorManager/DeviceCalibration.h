/*
 * DeviceCalibration.h - Device Color Calibration and Profiling
 *
 * Device calibration and profiling for scanners, printers, cameras, and other
 * color input/output devices for accurate color reproduction.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#ifndef DEVICECALIBRATION_H
#define DEVICECALIBRATION_H

#include "ColorManager.h"
#include "ColorSpaces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * DEVICE CALIBRATION CONSTANTS
 * ================================================================ */

/* Device types */
typedef enum {
    cmDeviceScanner     = 0,    /* Scanner device */
    cmDevicePrinter     = 1,    /* Printer device */
    cmDeviceCamera      = 2,    /* Camera device */
    cmDeviceProjector   = 3,    /* Projector device */
    cmDeviceGeneric     = 4     /* Generic device */
} CMDeviceType;

/* Calibration methods */
typedef enum {
    cmCalibrationITF    = 0,    /* IT8 test form */
    cmCalibrationCustom = 1,    /* Custom test chart */
    cmCalibrationAuto   = 2,    /* Automatic calibration */
    cmCalibrationManual = 3     /* Manual calibration */
} CMCalibrationMethod;

/* Paper types for printer calibration */
typedef enum {
    cmPaperPlain        = 0,    /* Plain paper */
    cmPaperPhoto        = 1,    /* Photo paper */
    cmPaperMatte        = 2,    /* Matte paper */
    cmPaperGlossy       = 3,    /* Glossy paper */
    cmPaperCanvas       = 4,    /* Canvas */
    cmPaperFineArt      = 5     /* Fine art paper */
} CMPaperType;

/* Ink types */
typedef enum {
    cmInkDye            = 0,    /* Dye-based ink */
    cmInkPigment        = 1,    /* Pigment-based ink */
    cmInkHybrid         = 2,    /* Hybrid ink */
    cmInkArchival       = 3     /* Archival ink */
} CMInkType;

/* ================================================================
 * DEVICE CALIBRATION STRUCTURES
 * ================================================================ */

/* Device information */
typedef struct {
    char            name[128];          /* Device name */
    char            manufacturer[64];   /* Manufacturer */
    char            model[64];          /* Model number */
    char            serialNumber[32];   /* Serial number */
    CMDeviceType    deviceType;         /* Device type */
    char            driverVersion[32];  /* Driver version */
    bool            isColorManaged;     /* Color management support */
    uint32_t        maxResolutionDPI;   /* Maximum resolution */
    uint32_t        bitDepth;           /* Bits per channel */
    char            connectionType[32]; /* USB, Ethernet, etc. */
} CMDeviceInfo;

/* Scanner calibration settings */
typedef struct {
    uint32_t        resolutionDPI;      /* Scan resolution */
    bool            useICCProfile;      /* Use ICC profile */
    float           exposureTime;       /* Exposure time (seconds) */
    float           whitePoint;         /* White point adjustment */
    float           blackPoint;         /* Black point adjustment */
    float           gamma;              /* Gamma correction */
    bool            dustRemoval;        /* Dust removal */
    bool            colorRestoration;   /* Color restoration */
} CMScannerSettings;

/* Printer calibration settings */
typedef struct {
    uint32_t        resolutionDPI;      /* Print resolution */
    CMPaperType     paperType;          /* Paper type */
    CMInkType       inkType;            /* Ink type */
    uint32_t        inkChannels;        /* Number of ink channels */
    float           inkDensity[8];      /* Ink density per channel */
    bool            bidirectional;      /* Bidirectional printing */
    uint32_t        passes;             /* Number of passes */
    float           dryingTime;         /* Drying time between passes */
} CMPrinterSettings;

/* Camera calibration settings */
typedef struct {
    uint32_t        isoSetting;         /* ISO setting */
    float           exposureTime;       /* Exposure time (seconds) */
    float           aperture;           /* F-stop */
    uint16_t        whiteBalance;       /* White balance temperature */
    float           colorMatrix[3][3];  /* Color correction matrix */
    bool            rawProcessing;      /* RAW processing enabled */
    char            colorSpace[32];     /* Color space (sRGB, Adobe RGB, etc.) */
} CMCameraSettings;

/* Test chart definition */
typedef struct {
    char            name[64];           /* Chart name */
    char            reference[32];      /* Reference standard (IT8, etc.) */
    uint32_t        patchCount;        /* Number of color patches */
    CMRGBColor     *referenceColors;   /* Reference color values */
    float          *patchPositions;    /* Patch positions (x, y) */
    uint32_t        chartWidth;        /* Chart width in patches */
    uint32_t        chartHeight;       /* Chart height in patches */
} CMTestChart;

/* Calibration measurement */
typedef struct {
    uint32_t        patchIndex;        /* Patch index */
    CMRGBColor      referenceColor;    /* Reference color */
    CMRGBColor      measuredColor;     /* Measured color */
    CMXYZColor      measuredXYZ;       /* Measured XYZ */
    float           deltaE;            /* Color error */
    bool            isValid;           /* Measurement validity */
} CMDeviceMeasurement;

/* Device calibration results */
typedef struct {
    CMDeviceType        deviceType;         /* Device type */
    CMCalibrationMethod method;             /* Calibration method */
    uint32_t           measurementCount;    /* Number of measurements */
    CMDeviceMeasurement *measurements;      /* Measurement array */
    float              averageDeltaE;       /* Average color error */
    float              maxDeltaE;           /* Maximum color error */
    CMColorMatrix      correctionMatrix;    /* Color correction matrix */
    uint16_t          *toneCurves[3];       /* Tone curves (R, G, B) */
    uint32_t          curveSize;            /* Tone curve size */
    CMProfileRef      deviceProfile;        /* Generated device profile */
    char              notes[256];           /* Calibration notes */
} CMDeviceCalibrationResults;

/* ================================================================
 * DEVICE ENUMERATION
 * ================================================================ */

/* Enumerate color devices */
CMError CMEnumerateColorDevices(CMDeviceInfo *devices, uint32_t *count, CMDeviceType deviceType);

/* Get device information */
CMError CMGetDeviceInfo(const char *deviceName, CMDeviceInfo *deviceInfo);

/* Check device color management support */
bool CMDeviceSupportsColorManagement(const char *deviceName);

/* Get device capabilities */
CMError CMGetDeviceCapabilities(const char *deviceName, uint32_t *capabilities);

/* ================================================================
 * SCANNER CALIBRATION
 * ================================================================ */

/* Initialize scanner calibration */
CMError CMInitializeScannerCalibration(const char *scannerName, const CMScannerSettings *settings);

/* Scan calibration target */
CMError CMScanCalibrationTarget(const char *scannerName, const CMTestChart *testChart,
                               void *scannedImage, uint32_t *imageSize);

/* Analyze scanned calibration target */
CMError CMAnalyzeScannerCalibration(const void *scannedImage, const CMTestChart *testChart,
                                   CMDeviceCalibrationResults *results);

/* Apply scanner calibration */
CMError CMApplyScannerCalibration(const char *scannerName, const CMDeviceCalibrationResults *calibration);

/* Create scanner profile */
CMError CMCreateScannerProfile(const CMDeviceCalibrationResults *calibration, CMProfileRef *profile);

/* ================================================================
 * PRINTER CALIBRATION
 * ================================================================ */

/* Initialize printer calibration */
CMError CMInitializePrinterCalibration(const char *printerName, const CMPrinterSettings *settings);

/* Print calibration target */
CMError CMPrintCalibrationTarget(const char *printerName, const CMTestChart *testChart);

/* Measure printed calibration target */
CMError CMeasurePrintedTarget(const CMTestChart *testChart, uint32_t colorimeterIndex,
                             CMDeviceCalibrationResults *results);

/* Create printer profile */
CMError CMCreatePrinterProfile(const CMDeviceCalibrationResults *calibration,
                              const CMPrinterSettings *settings, CMProfileRef *profile);

/* Apply printer calibration */
CMError CMApplyPrinterCalibration(const char *printerName, const CMDeviceCalibrationResults *calibration);

/* ================================================================
 * CAMERA CALIBRATION
 * ================================================================ */

/* Initialize camera calibration */
CMError CMInitializeCameraCalibration(const char *cameraName, const CMCameraSettings *settings);

/* Capture calibration target */
CMError CMCaptureCameraTarget(const char *cameraName, const CMTestChart *testChart,
                             void *capturedImage, uint32_t *imageSize);

/* Analyze camera calibration */
CMError CMAnalyzeCameraCalibration(const void *capturedImage, const CMTestChart *testChart,
                                  CMDeviceCalibrationResults *results);

/* Create camera profile */
CMError CMCreateCameraProfile(const CMDeviceCalibrationResults *calibration,
                             const CMCameraSettings *settings, CMProfileRef *profile);

/* Apply camera calibration */
CMError CMApplyCameraCalibration(const char *cameraName, const CMDeviceCalibrationResults *calibration);

/* ================================================================
 * TEST CHART MANAGEMENT
 * ================================================================ */

/* Create test chart */
CMError CMCreateTestChart(const char *name, const char *reference, uint32_t patchCount,
                         CMTestChart **testChart);

/* Load test chart from file */
CMError CMLoadTestChart(const char *filename, CMTestChart **testChart);

/* Save test chart to file */
CMError CMSaveTestChart(const CMTestChart *testChart, const char *filename);

/* Dispose test chart */
void CMDisposeTestChart(CMTestChart *testChart);

/* Generate standard test charts */
CMError CMGenerateIT8Chart(CMTestChart **testChart);
CMError CMGenerateColorCheckerChart(CMTestChart **testChart);
CMError CMGenerateCustomChart(uint32_t patchCount, CMTestChart **testChart);

/* ================================================================
 * LINEARIZATION
 * ================================================================ */

/* Device linearization */
CMError CMLinearizeDevice(const char *deviceName, CMDeviceType deviceType);

/* Create linearization curves */
CMError CMCreateLinearizationCurves(const CMDeviceCalibrationResults *calibration,
                                   uint16_t **redCurve, uint16_t **greenCurve, uint16_t **blueCurve,
                                   uint32_t *curveSize);

/* Apply linearization */
CMError CMApplyLinearization(const char *deviceName, const uint16_t *redCurve,
                           const uint16_t *greenCurve, const uint16_t *blueCurve, uint32_t curveSize);

/* Verify linearization */
CMError CMVerifyLinearization(const char *deviceName, float *linearity, float *maxError);

/* ================================================================
 * QUALITY ASSESSMENT
 * ================================================================ */

/* Assess calibration quality */
CMError CMAssessCalibrationQuality(const CMDeviceCalibrationResults *calibration,
                                  float *accuracy, float *precision, float *consistency);

/* Compare device calibrations */
CMError CMCompareDeviceCalibrations(const CMDeviceCalibrationResults *calibration1,
                                   const CMDeviceCalibrationResults *calibration2,
                                   float *similarity);

/* Validate device profile */
CMError CMValidateDeviceProfile(CMProfileRef profile, const CMTestChart *testChart,
                               float *averageError, float *maxError);

/* ================================================================
 * SOFT PROOFING FOR DEVICES
 * ================================================================ */

/* Create device soft proof */
CMError CMCreateDeviceSoftProof(CMProfileRef inputProfile, CMProfileRef deviceProfile,
                               CMProfileRef displayProfile, CMTransformRef *proofTransform);

/* Preview device output */
CMError CMPreviewDeviceOutput(CMTransformRef proofTransform, const void *inputImage,
                             void *previewImage, uint32_t width, uint32_t height);

/* Check device gamut */
CMError CMCheckDeviceGamut(CMProfileRef deviceProfile, const CMColor *colors,
                          uint32_t colorCount, bool *inGamut);

/* ================================================================
 * DEVICE PROFILE MANAGEMENT
 * ================================================================ */

/* Install device profile */
CMError CMInstallDeviceProfile(const char *deviceName, CMProfileRef profile);

/* Get device profile */
CMError CMGetDeviceProfile(const char *deviceName, CMProfileRef *profile);

/* Remove device profile */
CMError CMRemoveDeviceProfile(const char *deviceName);

/* List device profiles */
CMError CMListDeviceProfiles(const char *deviceName, char **profileNames, uint32_t *count);

/* ================================================================
 * BATCH PROCESSING
 * ================================================================ */

/* Batch calibration configuration */
typedef struct {
    char           *deviceNames[16];    /* Device names */
    uint32_t       deviceCount;        /* Number of devices */
    CMTestChart    *testChart;          /* Test chart to use */
    bool           autoMeasure;         /* Automatic measurement */
    bool           generateProfiles;    /* Generate ICC profiles */
    char           outputDirectory[256]; /* Output directory */
} CMBatchCalibrationConfig;

/* Start batch calibration */
CMError CMStartBatchCalibration(const CMBatchCalibrationConfig *config);

/* Get batch calibration progress */
CMError CMGetBatchCalibrationProgress(float *progress, char *currentDevice);

/* Stop batch calibration */
CMError CMStopBatchCalibration(void);

/* ================================================================
 * MAINTENANCE AND VERIFICATION
 * ================================================================ */

/* Schedule device recalibration */
CMError CMScheduleDeviceRecalibration(const char *deviceName, uint32_t intervalDays);

/* Check calibration expiry */
CMError CMCheckCalibrationExpiry(const char *deviceName, bool *isExpired, uint32_t *daysRemaining);

/* Verify device consistency */
CMError CMVerifyDeviceConsistency(const char *deviceName, const CMTestChart *testChart,
                                 float *consistency);

/* Generate calibration report */
CMError CMGenerateCalibrationReport(const CMDeviceCalibrationResults *calibration,
                                   const char *filename);

/* ================================================================
 * SPECTRAL MEASUREMENT SUPPORT
 * ================================================================ */

/* Spectral measurement data */
typedef struct {
    uint32_t        wavelengthStart;    /* Starting wavelength (nm) */
    uint32_t        wavelengthEnd;      /* Ending wavelength (nm) */
    uint32_t        wavelengthStep;     /* Wavelength step (nm) */
    uint32_t        sampleCount;        /* Number of samples */
    float          *spectralData;       /* Spectral reflectance/transmittance */
    CMXYZColor      calculatedXYZ;      /* Calculated XYZ from spectral data */
} CMSpectralMeasurement;

/* Measure spectral data */
CMError CMMeasureSpectralData(uint32_t spectrometerIndex, const CMRGBColor *stimulusColor,
                             CMSpectralMeasurement *spectralData);

/* Convert spectral to XYZ */
CMError CMConvertSpectralToXYZ(const CMSpectralMeasurement *spectralData,
                              const CMXYZColor *illuminant, CMXYZColor *xyz);

/* Calculate metamerism index */
CMError CMCalculateMetamerismIndex(const CMSpectralMeasurement *sample1,
                                  const CMSpectralMeasurement *sample2,
                                  const CMXYZColor *illuminant1,
                                  const CMXYZColor *illuminant2,
                                  float *metamerismIndex);

#ifdef __cplusplus
}
#endif

#endif /* DEVICECALIBRATION_H */
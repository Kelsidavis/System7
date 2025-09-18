/*
 * ColorTransforms.h - Color Space Transformations
 *
 * Advanced color space transformations, lookup tables, and optimization
 * for high-performance color processing in professional workflows.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#ifndef COLORTRANSFORMS_H
#define COLORTRANSFORMS_H

#include "ColorManager.h"
#include "ColorSpaces.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * TRANSFORM CONSTANTS
 * ================================================================ */

/* Transform optimization levels */
typedef enum {
    cmTransformDraft    = 0,    /* Fast, lower precision */
    cmTransformNormal   = 1,    /* Normal precision */
    cmTransformPrecise  = 2     /* High precision, slower */
} CMTransformPrecision;

/* Interpolation methods */
typedef enum {
    cmNearestNeighbor   = 0,    /* Nearest neighbor */
    cmLinearInterp      = 1,    /* Linear interpolation */
    cmTrilinearInterp   = 2,    /* Trilinear interpolation */
    cmTetrahedral       = 3,    /* Tetrahedral interpolation */
    cmCubicInterp       = 4     /* Cubic interpolation */
} CMInterpolationMethod;

/* Cache levels */
typedef enum {
    cmNoCaching         = 0,    /* No caching */
    cmBasicCaching      = 1,    /* Basic LRU cache */
    cmAdvancedCaching   = 2     /* Advanced predictive cache */
} CMCacheLevel;

/* ================================================================
 * TRANSFORM STRUCTURES
 * ================================================================ */

/* Transform configuration */
typedef struct {
    CMTransformPrecision    precision;      /* Transform precision */
    CMInterpolationMethod   interpolation;  /* Interpolation method */
    CMCacheLevel           caching;         /* Cache level */
    uint32_t               cacheSize;       /* Cache size in KB */
    bool                   useGPU;          /* Use GPU acceleration */
    bool                   multithreaded;   /* Use multiple threads */
    uint32_t               threadCount;     /* Number of threads */
} CMTransformConfig;

/* Transform statistics */
typedef struct {
    uint64_t    transformCount;     /* Number of transforms performed */
    uint64_t    cacheHits;          /* Cache hits */
    uint64_t    cacheMisses;        /* Cache misses */
    uint32_t    averageTimeUs;      /* Average transform time (microseconds) */
    uint32_t    totalTimeMs;        /* Total transform time (milliseconds) */
    float       throughput;         /* Colors per second */
} CMTransformStatistics;

/* Lookup table descriptor */
typedef struct {
    uint32_t    inputChannels;      /* Number of input channels */
    uint32_t    outputChannels;     /* Number of output channels */
    uint32_t    gridPoints;         /* Grid points per dimension */
    uint32_t    precision;          /* Bits per value (8 or 16) */
    uint32_t    tableSize;          /* Total table size in bytes */
    void       *tableData;          /* LUT data */
    bool        isOptimized;        /* Optimized for hardware */
} CMLUTDescriptor;

/* ================================================================
 * TRANSFORM SYSTEM MANAGEMENT
 * ================================================================ */

/* Initialize transform system */
CMError CMInitTransformSystem(void);

/* Shutdown transform system */
void CMShutdownTransformSystem(void);

/* Set global transform configuration */
CMError CMSetGlobalTransformConfig(const CMTransformConfig *config);

/* Get global transform configuration */
CMError CMGetGlobalTransformConfig(CMTransformConfig *config);

/* Get transform capabilities */
CMError CMGetTransformCapabilities(uint32_t *capabilities);

/* ================================================================
 * HIGH-LEVEL TRANSFORM OPERATIONS
 * ================================================================ */

/* Transform single color with caching */
CMError CMTransformColorCached(CMTransformRef transform, const CMColor *input, CMColor *output);

/* Transform color array with optimization */
CMError CMTransformColorArray(CMTransformRef transform, const CMColor *input,
                             CMColor *output, uint32_t count);

/* Transform image data */
CMError CMTransformImageData(CMTransformRef transform, const void *inputData,
                            void *outputData, uint32_t width, uint32_t height,
                            uint32_t inputStride, uint32_t outputStride,
                            CMColorSpace inputFormat, CMColorSpace outputFormat);

/* Transform with region of interest */
CMError CMTransformImageRegion(CMTransformRef transform, const void *inputData,
                              void *outputData, uint32_t imageWidth, uint32_t imageHeight,
                              uint32_t roiX, uint32_t roiY, uint32_t roiWidth, uint32_t roiHeight,
                              uint32_t inputStride, uint32_t outputStride,
                              CMColorSpace inputFormat, CMColorSpace outputFormat);

/* ================================================================
 * LOOKUP TABLE OPERATIONS
 * ================================================================ */

/* Create 3D lookup table */
CMError CMCreate3DLUT(CMTransformRef transform, uint32_t gridPoints,
                     uint32_t precision, CMLUTDescriptor **lutDesc);

/* Create 1D lookup table */
CMError CMCreate1DLUT(CMTransformRef transform, uint32_t entries,
                     uint32_t precision, CMLUTDescriptor **lutDesc);

/* Load LUT from file */
CMError CMLoadLUTFromFile(const char *filename, CMLUTDescriptor **lutDesc);

/* Save LUT to file */
CMError CMSaveLUTToFile(const CMLUTDescriptor *lutDesc, const char *filename);

/* Dispose LUT descriptor */
void CMDisposeLUTDescriptor(CMLUTDescriptor *lutDesc);

/* Apply LUT to color */
CMError CMApplyLUTToColor(const CMLUTDescriptor *lutDesc, const CMColor *input, CMColor *output);

/* Apply LUT to color array */
CMError CMApplyLUTToColorArray(const CMLUTDescriptor *lutDesc, const CMColor *input,
                              CMColor *output, uint32_t count);

/* Optimize LUT for hardware */
CMError CMOptimizeLUTForHardware(CMLUTDescriptor *lutDesc);

/* ================================================================
 * MATRIX TRANSFORMS
 * ================================================================ */

/* Create matrix transform */
CMError CMCreateMatrixTransform(const CMColorMatrix *matrix, CMTransformRef *transform);

/* Apply matrix to color */
CMError CMApplyMatrixToColor(const CMColorMatrix *matrix, const CMXYZColor *input, CMXYZColor *output);

/* Apply matrix to color array */
CMError CMApplyMatrixToColorArray(const CMColorMatrix *matrix, const CMXYZColor *input,
                                 CMXYZColor *output, uint32_t count);

/* Concatenate matrices */
CMError CMConcatenateMatrices(const CMColorMatrix *m1, const CMColorMatrix *m2, CMColorMatrix *result);

/* Invert matrix */
CMError CMInvertMatrix(const CMColorMatrix *input, CMColorMatrix *output);

/* ================================================================
 * CURVE TRANSFORMS
 * ================================================================ */

/* Curve descriptor */
typedef struct {
    uint32_t    pointCount;         /* Number of curve points */
    uint16_t   *curveData;          /* Curve data */
    float       gamma;              /* Gamma value (if gamma curve) */
    bool        isGamma;            /* True if gamma curve */
    bool        isLinear;           /* True if linear curve */
} CMCurveDescriptor;

/* Create curve transform */
CMError CMCreateCurveTransform(const CMCurveDescriptor *curves, uint32_t channelCount,
                              CMTransformRef *transform);

/* Apply curve to value */
uint16_t CMApplyCurveToValue(const CMCurveDescriptor *curve, uint16_t input);

/* Apply curves to color */
CMError CMApplyCurvesToColor(const CMCurveDescriptor *curves, uint32_t channelCount,
                            const CMColor *input, CMColor *output);

/* Create gamma curve */
CMError CMCreateGammaCurveDescriptor(float gamma, uint32_t points, CMCurveDescriptor **curveDesc);

/* Dispose curve descriptor */
void CMDisposeCurveDescriptor(CMCurveDescriptor *curveDesc);

/* ================================================================
 * TRANSFORM OPTIMIZATION
 * ================================================================ */

/* Optimize transform for repeated use */
CMError CMOptimizeTransform(CMTransformRef transform);

/* Create optimized transform chain */
CMError CMCreateOptimizedTransformChain(CMTransformRef *transforms, uint32_t count,
                                       CMTransformRef *optimizedTransform);

/* Analyze transform performance */
CMError CMAnalyzeTransformPerformance(CMTransformRef transform, uint32_t testColors,
                                     CMTransformStatistics *stats);

/* Benchmark transform */
CMError CMBenchmarkTransform(CMTransformRef transform, uint32_t iterations,
                           uint32_t colorCount, float *colorsPerSecond);

/* ================================================================
 * MULTITHREADED TRANSFORMS
 * ================================================================ */

/* Transform color array with multiple threads */
CMError CMTransformColorArrayMT(CMTransformRef transform, const CMColor *input,
                               CMColor *output, uint32_t count, uint32_t threadCount);

/* Transform image with multiple threads */
CMError CMTransformImageDataMT(CMTransformRef transform, const void *inputData,
                              void *outputData, uint32_t width, uint32_t height,
                              uint32_t inputStride, uint32_t outputStride,
                              CMColorSpace inputFormat, CMColorSpace outputFormat,
                              uint32_t threadCount);

/* Set thread pool size */
CMError CMSetTransformThreadPoolSize(uint32_t threadCount);

/* Get optimal thread count */
uint32_t CMGetOptimalThreadCount(void);

/* ================================================================
 * GPU ACCELERATION
 * ================================================================ */

/* GPU transform context */
typedef struct CMGPUContext* CMGPUContextRef;

/* Initialize GPU acceleration */
CMError CMInitializeGPUAcceleration(CMGPUContextRef *context);

/* Create GPU transform */
CMError CMCreateGPUTransform(CMGPUContextRef context, CMTransformRef cpuTransform,
                           CMTransformRef *gpuTransform);

/* Upload LUT to GPU */
CMError CMUploadLUTToGPU(CMGPUContextRef context, const CMLUTDescriptor *lutDesc);

/* Transform on GPU */
CMError CMTransformOnGPU(CMTransformRef gpuTransform, const void *inputData,
                        void *outputData, uint32_t pixelCount);

/* Synchronize GPU operations */
CMError CMSynchronizeGPU(CMGPUContextRef context);

/* Cleanup GPU context */
void CMCleanupGPUAcceleration(CMGPUContextRef context);

/* ================================================================
 * ADVANCED INTERPOLATION
 * ================================================================ */

/* Tetrahedral interpolation */
CMError CMTetrahedralInterpolation(const CMLUTDescriptor *lutDesc, const float *input,
                                  float *output);

/* Trilinear interpolation */
CMError CMTrilinearInterpolation(const CMLUTDescriptor *lutDesc, const float *input,
                                float *output);

/* Cubic interpolation */
CMError CMCubicInterpolation(const CMLUTDescriptor *lutDesc, const float *input,
                           float *output);

/* Adaptive interpolation (chooses best method) */
CMError CMAdaptiveInterpolation(const CMLUTDescriptor *lutDesc, const float *input,
                               float *output);

/* ================================================================
 * TRANSFORM VALIDATION
 * ================================================================ */

/* Validate transform accuracy */
CMError CMValidateTransformAccuracy(CMTransformRef transform, const CMColor *testColors,
                                   uint32_t testCount, float *maxDeltaE, float *averageDeltaE);

/* Compare transforms */
CMError CMCompareTransforms(CMTransformRef transform1, CMTransformRef transform2,
                           const CMColor *testColors, uint32_t testCount,
                           float *maxDifference, float *averageDifference);

/* Test transform roundtrip accuracy */
CMError CMTestTransformRoundtrip(CMTransformRef forwardTransform, CMTransformRef reverseTransform,
                                const CMColor *testColors, uint32_t testCount,
                                float *maxError, float *averageError);

/* ================================================================
 * TRANSFORM SERIALIZATION
 * ================================================================ */

/* Serialize transform to data */
CMError CMSerializeTransform(CMTransformRef transform, void **data, uint32_t *size);

/* Deserialize transform from data */
CMError CMDeserializeTransform(const void *data, uint32_t size, CMTransformRef *transform);

/* Save transform to file */
CMError CMSaveTransformToFile(CMTransformRef transform, const char *filename);

/* Load transform from file */
CMError CMLoadTransformFromFile(const char *filename, CMTransformRef *transform);

/* ================================================================
 * SPECIALIZED TRANSFORMS
 * ================================================================ */

/* White point adaptation transform */
CMError CMCreateWhitePointAdaptationTransform(const CMXYZColor *srcWhitePoint,
                                             const CMXYZColor *dstWhitePoint,
                                             CMAdaptationMethod method,
                                             CMTransformRef *transform);

/* Gamut mapping transform */
CMError CMCreateGamutMappingTransform(CMProfileRef srcProfile, CMProfileRef dstProfile,
                                     CMGamutMethod method, CMTransformRef *transform);

/* Tone mapping transform */
CMError CMCreateToneMappingTransform(float inputRange, float outputRange,
                                    float gamma, CMTransformRef *transform);

/* Color space conversion transform */
CMError CMCreateColorSpaceTransform(CMColorSpace srcSpace, CMColorSpace dstSpace,
                                   const CMXYZColor *whitePoint, CMTransformRef *transform);

/* ================================================================
 * CACHING SYSTEM
 * ================================================================ */

/* Cache configuration */
typedef struct {
    uint32_t    maxEntries;         /* Maximum cache entries */
    uint32_t    maxMemoryKB;        /* Maximum memory usage */
    float       evictionThreshold;  /* Eviction threshold (0.0-1.0) */
    bool        persistentCache;    /* Save cache to disk */
    char        cacheDirectory[256]; /* Cache directory path */
} CMCacheConfig;

/* Initialize transform cache */
CMError CMInitializeTransformCache(const CMCacheConfig *config);

/* Clear transform cache */
CMError CMClearTransformCache(void);

/* Get cache statistics */
CMError CMGetCacheStatistics(uint32_t *entries, uint32_t *memoryUsed, uint32_t *hits, uint32_t *misses);

/* Set cache size */
CMError CMSetCacheSize(uint32_t maxEntries, uint32_t maxMemoryKB);

/* ================================================================
 * PERFORMANCE PROFILING
 * ================================================================ */

/* Profile descriptor */
typedef struct {
    char        transformName[64];  /* Transform identifier */
    uint64_t    totalCalls;         /* Total function calls */
    uint64_t    totalTimeUs;        /* Total time in microseconds */
    uint32_t    averageTimeUs;      /* Average time per call */
    uint32_t    minTimeUs;          /* Minimum time */
    uint32_t    maxTimeUs;          /* Maximum time */
    uint64_t    totalColors;        /* Total colors processed */
    float       colorsPerSecond;    /* Processing rate */
} CMProfileDescriptor;

/* Start performance profiling */
CMError CMStartProfiling(void);

/* Stop performance profiling */
CMError CMStopProfiling(void);

/* Get profiling results */
CMError CMGetProfilingResults(CMProfileDescriptor *profiles, uint32_t *count);

/* Reset profiling data */
CMError CMResetProfilingData(void);

#ifdef __cplusplus
}
#endif

#endif /* COLORTRANSFORMS_H */
/*
 * ColorTransforms.c - Color Space Transformations Implementation
 *
 * Implementation of advanced color space transformations, lookup tables,
 * and optimization for high-performance color processing.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color Manager
 */

#include "../include/ColorManager/ColorTransforms.h"
#include "../include/ColorManager/ColorSpaces.h"
#include "../include/ColorManager/ColorMatching.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

/* ================================================================
 * INTERNAL STRUCTURES
 * ================================================================ */

/* Transform cache entry */
typedef struct CMCacheEntry {
    CMColor             input;          /* Input color */
    CMColor             output;         /* Cached output */
    CMTransformRef      transform;      /* Transform used */
    uint64_t           lastUsed;       /* Last access time */
    uint32_t           useCount;       /* Usage count */
    struct CMCacheEntry *next;         /* Next in hash chain */
} CMCacheEntry;

/* Transform cache */
typedef struct {
    CMCacheEntry      **hashTable;     /* Hash table */
    uint32_t           tableSize;      /* Hash table size */
    uint32_t           entryCount;     /* Current entries */
    uint32_t           maxEntries;     /* Maximum entries */
    uint32_t           hits;           /* Cache hits */
    uint32_t           misses;         /* Cache misses */
    bool               enabled;        /* Cache enabled */
} CMTransformCache;

/* GPU context (simplified) */
struct CMGPUContext {
    void               *deviceContext; /* Platform-specific GPU context */
    bool               isInitialized;  /* Initialization status */
    uint32_t           maxTextureSize; /* Maximum texture size */
    bool               supports3DLUTs; /* 3D LUT support */
};

/* ================================================================
 * GLOBAL STATE
 * ================================================================ */

static bool g_transformSystemInitialized = false;
static CMTransformConfig g_globalConfig = {
    cmTransformNormal,      /* precision */
    cmTrilinearInterp,      /* interpolation */
    cmBasicCaching,         /* caching */
    1024,                   /* cacheSize (1MB) */
    false,                  /* useGPU */
    true,                   /* multithreaded */
    0                       /* threadCount (auto-detect) */
};

static CMTransformCache g_transformCache = {0};
static bool g_profilingEnabled = false;
static CMProfileDescriptor *g_profilingData = NULL;
static uint32_t g_profilingDataCount = 0;

/* Forward declarations */
static uint32_t HashColor(const CMColor *color);
static CMCacheEntry *FindCacheEntry(const CMColor *input, CMTransformRef transform);
static void AddCacheEntry(const CMColor *input, const CMColor *output, CMTransformRef transform);
static void EvictOldestCacheEntry(void);
static CMError InitializeTransformCache(void);
static void CleanupTransformCache(void);
static uint64_t GetCurrentTimeUs(void);
static CMError ValidateLUTDescriptor(const CMLUTDescriptor *lutDesc);

/* ================================================================
 * TRANSFORM SYSTEM MANAGEMENT
 * ================================================================ */

CMError CMInitTransformSystem(void) {
    if (g_transformSystemInitialized) {
        return cmNoError;
    }

    /* Initialize transform cache */
    CMError err = InitializeTransformCache();
    if (err != cmNoError) return err;

    /* Auto-detect optimal thread count */
    if (g_globalConfig.threadCount == 0) {
        g_globalConfig.threadCount = 1; /* Default to single thread */
        /* In real implementation, detect CPU cores */
#ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        g_globalConfig.threadCount = sysinfo.dwNumberOfProcessors;
#elif defined(__linux__) || defined(__APPLE__)
        g_globalConfig.threadCount = sysconf(_SC_NPROCESSORS_ONLN);
#endif
        if (g_globalConfig.threadCount < 1) g_globalConfig.threadCount = 1;
        if (g_globalConfig.threadCount > 16) g_globalConfig.threadCount = 16;
    }

    g_transformSystemInitialized = true;
    return cmNoError;
}

void CMShutdownTransformSystem(void) {
    if (!g_transformSystemInitialized) return;

    CleanupTransformCache();

    if (g_profilingData) {
        free(g_profilingData);
        g_profilingData = NULL;
        g_profilingDataCount = 0;
    }

    g_transformSystemInitialized = false;
}

CMError CMSetGlobalTransformConfig(const CMTransformConfig *config) {
    if (!config) return cmParameterError;

    g_globalConfig = *config;

    /* Reconfigure cache if needed */
    if (g_transformCache.enabled) {
        CMSetCacheSize(config->cacheSize * 1024 / sizeof(CMCacheEntry), config->cacheSize);
    }

    return cmNoError;
}

CMError CMGetGlobalTransformConfig(CMTransformConfig *config) {
    if (!config) return cmParameterError;
    *config = g_globalConfig;
    return cmNoError;
}

/* ================================================================
 * HIGH-LEVEL TRANSFORM OPERATIONS
 * ================================================================ */

CMError CMTransformColorCached(CMTransformRef transform, const CMColor *input, CMColor *output) {
    if (!transform || !input || !output) return cmParameterError;

    if (!g_transformSystemInitialized) {
        CMInitTransformSystem();
    }

    /* Check cache first */
    if (g_transformCache.enabled) {
        CMCacheEntry *entry = FindCacheEntry(input, transform);
        if (entry) {
            *output = entry->output;
            entry->lastUsed = GetCurrentTimeUs();
            entry->useCount++;
            g_transformCache.hits++;
            return cmNoError;
        }
        g_transformCache.misses++;
    }

    /* Perform transformation */
    CMError err = CMApplyTransform(transform, input, output);
    if (err != cmNoError) return err;

    /* Add to cache */
    if (g_transformCache.enabled) {
        AddCacheEntry(input, output, transform);
    }

    return cmNoError;
}

CMError CMTransformColorArray(CMTransformRef transform, const CMColor *input,
                             CMColor *output, uint32_t count) {
    if (!transform || !input || !output || count == 0) return cmParameterError;

    uint64_t startTime = GetCurrentTimeUs();

    /* Use multithreading for large arrays */
    if (g_globalConfig.multithreaded && count > 100) {
        return CMTransformColorArrayMT(transform, input, output, count, g_globalConfig.threadCount);
    }

    /* Single-threaded processing */
    for (uint32_t i = 0; i < count; i++) {
        CMError err = CMTransformColorCached(transform, &input[i], &output[i]);
        if (err != cmNoError) return err;
    }

    uint64_t endTime = GetCurrentTimeUs();
    uint32_t timeUs = (uint32_t)(endTime - startTime);

    /* Update statistics */
    if (g_profilingEnabled) {
        /* Add to profiling data */
    }

    return cmNoError;
}

CMError CMTransformImageData(CMTransformRef transform, const void *inputData,
                            void *outputData, uint32_t width, uint32_t height,
                            uint32_t inputStride, uint32_t outputStride,
                            CMColorSpace inputFormat, CMColorSpace outputFormat) {
    if (!transform || !inputData || !outputData || width == 0 || height == 0) {
        return cmParameterError;
    }

    /* Use multithreading for large images */
    if (g_globalConfig.multithreaded && width * height > 10000) {
        return CMTransformImageDataMT(transform, inputData, outputData, width, height,
                                     inputStride, outputStride, inputFormat, outputFormat,
                                     g_globalConfig.threadCount);
    }

    const uint8_t *srcRow = (const uint8_t *)inputData;
    uint8_t *dstRow = (uint8_t *)outputData;

    uint32_t inputBytesPerPixel = (inputFormat == cmRGBSpace) ? 3 : 4;
    uint32_t outputBytesPerPixel = (outputFormat == cmRGBSpace) ? 3 : 4;

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t *srcPixel = srcRow;
        uint8_t *dstPixel = dstRow;

        for (uint32_t x = 0; x < width; x++) {
            /* Convert pixel to CMColor */
            CMColor inputColor = {0};
            if (inputFormat == cmRGBSpace) {
                inputColor.rgb.red = (srcPixel[0] << 8) | srcPixel[0];
                inputColor.rgb.green = (srcPixel[1] << 8) | srcPixel[1];
                inputColor.rgb.blue = (srcPixel[2] << 8) | srcPixel[2];
            }

            /* Transform color */
            CMColor outputColor;
            CMError err = CMTransformColorCached(transform, &inputColor, &outputColor);
            if (err != cmNoError) return err;

            /* Convert back to pixel format */
            if (outputFormat == cmRGBSpace) {
                dstPixel[0] = outputColor.rgb.red >> 8;
                dstPixel[1] = outputColor.rgb.green >> 8;
                dstPixel[2] = outputColor.rgb.blue >> 8;
            }

            srcPixel += inputBytesPerPixel;
            dstPixel += outputBytesPerPixel;
        }

        srcRow += inputStride;
        dstRow += outputStride;
    }

    return cmNoError;
}

/* ================================================================
 * LOOKUP TABLE OPERATIONS
 * ================================================================ */

CMError CMCreate3DLUT(CMTransformRef transform, uint32_t gridPoints,
                     uint32_t precision, CMLUTDescriptor **lutDesc) {
    if (!transform || !lutDesc || gridPoints == 0) return cmParameterError;
    if (precision != 8 && precision != 16) return cmParameterError;

    CMLUTDescriptor *newLUT = (CMLUTDescriptor *)calloc(1, sizeof(CMLUTDescriptor));
    if (!newLUT) return cmProfileError;

    newLUT->inputChannels = 3;
    newLUT->outputChannels = 3;
    newLUT->gridPoints = gridPoints;
    newLUT->precision = precision;

    uint32_t valuesPerEntry = newLUT->outputChannels;
    uint32_t totalEntries = gridPoints * gridPoints * gridPoints;
    uint32_t bytesPerValue = (precision == 16) ? 2 : 1;
    newLUT->tableSize = totalEntries * valuesPerEntry * bytesPerValue;

    newLUT->tableData = malloc(newLUT->tableSize);
    if (!newLUT->tableData) {
        free(newLUT);
        return cmProfileError;
    }

    /* Generate LUT by sampling the transform */
    for (uint32_t r = 0; r < gridPoints; r++) {
        for (uint32_t g = 0; g < gridPoints; g++) {
            for (uint32_t b = 0; b < gridPoints; b++) {
                /* Create input color */
                CMColor inputColor;
                inputColor.rgb.red = (uint16_t)((r * 65535) / (gridPoints - 1));
                inputColor.rgb.green = (uint16_t)((g * 65535) / (gridPoints - 1));
                inputColor.rgb.blue = (uint16_t)((b * 65535) / (gridPoints - 1));

                /* Transform color */
                CMColor outputColor;
                CMError err = CMApplyTransform(transform, &inputColor, &outputColor);
                if (err != cmNoError) {
                    free(newLUT->tableData);
                    free(newLUT);
                    return err;
                }

                /* Store in LUT */
                uint32_t index = ((r * gridPoints + g) * gridPoints + b) * valuesPerEntry;

                if (precision == 16) {
                    uint16_t *lut16 = (uint16_t *)newLUT->tableData;
                    lut16[index + 0] = outputColor.rgb.red;
                    lut16[index + 1] = outputColor.rgb.green;
                    lut16[index + 2] = outputColor.rgb.blue;
                } else {
                    uint8_t *lut8 = (uint8_t *)newLUT->tableData;
                    lut8[index + 0] = outputColor.rgb.red >> 8;
                    lut8[index + 1] = outputColor.rgb.green >> 8;
                    lut8[index + 2] = outputColor.rgb.blue >> 8;
                }
            }
        }
    }

    newLUT->isOptimized = false;
    *lutDesc = newLUT;
    return cmNoError;
}

CMError CMApplyLUTToColor(const CMLUTDescriptor *lutDesc, const CMColor *input, CMColor *output) {
    if (!lutDesc || !input || !output) return cmParameterError;

    CMError err = ValidateLUTDescriptor(lutDesc);
    if (err != cmNoError) return err;

    /* Normalize input to 0-1 range */
    float coords[3] = {
        input->rgb.red / 65535.0f,
        input->rgb.green / 65535.0f,
        input->rgb.blue / 65535.0f
    };

    /* Apply interpolation based on global config */
    float result[3];
    switch (g_globalConfig.interpolation) {
        case cmTrilinearInterp:
            err = CMTrilinearInterpolation(lutDesc, coords, result);
            break;
        case cmTetrahedral:
            err = CMTetrahedralInterpolation(lutDesc, coords, result);
            break;
        case cmNearestNeighbor:
            {
                /* Nearest neighbor - simple but fast */
                uint32_t r = (uint32_t)(coords[0] * (lutDesc->gridPoints - 1) + 0.5f);
                uint32_t g = (uint32_t)(coords[1] * (lutDesc->gridPoints - 1) + 0.5f);
                uint32_t b = (uint32_t)(coords[2] * (lutDesc->gridPoints - 1) + 0.5f);

                r = (r >= lutDesc->gridPoints) ? lutDesc->gridPoints - 1 : r;
                g = (g >= lutDesc->gridPoints) ? lutDesc->gridPoints - 1 : g;
                b = (b >= lutDesc->gridPoints) ? lutDesc->gridPoints - 1 : b;

                uint32_t index = ((r * lutDesc->gridPoints + g) * lutDesc->gridPoints + b) * lutDesc->outputChannels;

                if (lutDesc->precision == 16) {
                    uint16_t *lut16 = (uint16_t *)lutDesc->tableData;
                    result[0] = lut16[index + 0] / 65535.0f;
                    result[1] = lut16[index + 1] / 65535.0f;
                    result[2] = lut16[index + 2] / 65535.0f;
                } else {
                    uint8_t *lut8 = (uint8_t *)lutDesc->tableData;
                    result[0] = lut8[index + 0] / 255.0f;
                    result[1] = lut8[index + 1] / 255.0f;
                    result[2] = lut8[index + 2] / 255.0f;
                }
                err = cmNoError;
            }
            break;
        default:
            return cmMethodError;
    }

    if (err != cmNoError) return err;

    /* Convert result back to 16-bit */
    output->rgb.red = (uint16_t)(result[0] * 65535.0f);
    output->rgb.green = (uint16_t)(result[1] * 65535.0f);
    output->rgb.blue = (uint16_t)(result[2] * 65535.0f);

    return cmNoError;
}

void CMDisposeLUTDescriptor(CMLUTDescriptor *lutDesc) {
    if (!lutDesc) return;

    if (lutDesc->tableData) {
        free(lutDesc->tableData);
    }
    free(lutDesc);
}

/* ================================================================
 * ADVANCED INTERPOLATION
 * ================================================================ */

CMError CMTrilinearInterpolation(const CMLUTDescriptor *lutDesc, const float *input, float *output) {
    if (!lutDesc || !input || !output) return cmParameterError;

    uint32_t gridPoints = lutDesc->gridPoints;
    float r = input[0] * (gridPoints - 1);
    float g = input[1] * (gridPoints - 1);
    float b = input[2] * (gridPoints - 1);

    uint32_t r0 = (uint32_t)floorf(r);
    uint32_t g0 = (uint32_t)floorf(g);
    uint32_t b0 = (uint32_t)floorf(b);

    uint32_t r1 = (r0 + 1 >= gridPoints) ? r0 : r0 + 1;
    uint32_t g1 = (g0 + 1 >= gridPoints) ? g0 : g0 + 1;
    uint32_t b1 = (b0 + 1 >= gridPoints) ? b0 : b0 + 1;

    float dr = r - r0;
    float dg = g - g0;
    float db = b - b0;

    /* Get 8 corner values and interpolate */
    auto getLUTValue = [lutDesc](uint32_t r, uint32_t g, uint32_t b, uint32_t channel) -> float {
        uint32_t index = ((r * lutDesc->gridPoints + g) * lutDesc->gridPoints + b) * lutDesc->outputChannels + channel;
        if (lutDesc->precision == 16) {
            uint16_t *lut16 = (uint16_t *)lutDesc->tableData;
            return lut16[index] / 65535.0f;
        } else {
            uint8_t *lut8 = (uint8_t *)lutDesc->tableData;
            return lut8[index] / 255.0f;
        }
    };

    for (uint32_t channel = 0; channel < lutDesc->outputChannels; channel++) {
        float c000 = getLUTValue(r0, g0, b0, channel);
        float c001 = getLUTValue(r0, g0, b1, channel);
        float c010 = getLUTValue(r0, g1, b0, channel);
        float c011 = getLUTValue(r0, g1, b1, channel);
        float c100 = getLUTValue(r1, g0, b0, channel);
        float c101 = getLUTValue(r1, g0, b1, channel);
        float c110 = getLUTValue(r1, g1, b0, channel);
        float c111 = getLUTValue(r1, g1, b1, channel);

        /* Trilinear interpolation */
        float c00 = c000 * (1.0f - db) + c001 * db;
        float c01 = c010 * (1.0f - db) + c011 * db;
        float c10 = c100 * (1.0f - db) + c101 * db;
        float c11 = c110 * (1.0f - db) + c111 * db;

        float c0 = c00 * (1.0f - dg) + c01 * dg;
        float c1 = c10 * (1.0f - dg) + c11 * dg;

        output[channel] = c0 * (1.0f - dr) + c1 * dr;
    }

    return cmNoError;
}

CMError CMTetrahedralInterpolation(const CMLUTDescriptor *lutDesc, const float *input, float *output) {
    /* Simplified tetrahedral interpolation implementation */
    /* For full implementation, this would use tetrahedral subdivision */
    return CMTrilinearInterpolation(lutDesc, input, output);
}

/* ================================================================
 * MATRIX TRANSFORMS
 * ================================================================ */

CMError CMApplyMatrixToColor(const CMColorMatrix *matrix, const CMXYZColor *input, CMXYZColor *output) {
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

CMError CMApplyMatrixToColorArray(const CMColorMatrix *matrix, const CMXYZColor *input,
                                 CMXYZColor *output, uint32_t count) {
    if (!matrix || !input || !output || count == 0) return cmParameterError;

    for (uint32_t i = 0; i < count; i++) {
        CMError err = CMApplyMatrixToColor(matrix, &input[i], &output[i]);
        if (err != cmNoError) return err;
    }

    return cmNoError;
}

/* ================================================================
 * CACHING SYSTEM
 * ================================================================ */

static CMError InitializeTransformCache(void) {
    if (g_transformCache.enabled) return cmNoError;

    g_transformCache.tableSize = 1024; /* Default hash table size */
    g_transformCache.hashTable = (CMCacheEntry **)calloc(g_transformCache.tableSize, sizeof(CMCacheEntry *));
    if (!g_transformCache.hashTable) return cmProfileError;

    g_transformCache.maxEntries = g_globalConfig.cacheSize * 1024 / sizeof(CMCacheEntry);
    g_transformCache.entryCount = 0;
    g_transformCache.hits = 0;
    g_transformCache.misses = 0;
    g_transformCache.enabled = (g_globalConfig.caching != cmNoCaching);

    return cmNoError;
}

static void CleanupTransformCache(void) {
    if (!g_transformCache.hashTable) return;

    for (uint32_t i = 0; i < g_transformCache.tableSize; i++) {
        CMCacheEntry *entry = g_transformCache.hashTable[i];
        while (entry) {
            CMCacheEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(g_transformCache.hashTable);
    memset(&g_transformCache, 0, sizeof(g_transformCache));
}

static uint32_t HashColor(const CMColor *color) {
    /* Simple hash function for RGB colors */
    uint32_t hash = color->rgb.red;
    hash = hash * 31 + color->rgb.green;
    hash = hash * 31 + color->rgb.blue;
    return hash;
}

static CMCacheEntry *FindCacheEntry(const CMColor *input, CMTransformRef transform) {
    if (!g_transformCache.enabled) return NULL;

    uint32_t hash = HashColor(input) % g_transformCache.tableSize;
    CMCacheEntry *entry = g_transformCache.hashTable[hash];

    while (entry) {
        if (entry->transform == transform &&
            memcmp(&entry->input, input, sizeof(CMColor)) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static void AddCacheEntry(const CMColor *input, const CMColor *output, CMTransformRef transform) {
    if (!g_transformCache.enabled) return;

    /* Check if cache is full */
    if (g_transformCache.entryCount >= g_transformCache.maxEntries) {
        EvictOldestCacheEntry();
    }

    CMCacheEntry *entry = (CMCacheEntry *)malloc(sizeof(CMCacheEntry));
    if (!entry) return;

    entry->input = *input;
    entry->output = *output;
    entry->transform = transform;
    entry->lastUsed = GetCurrentTimeUs();
    entry->useCount = 1;

    uint32_t hash = HashColor(input) % g_transformCache.tableSize;
    entry->next = g_transformCache.hashTable[hash];
    g_transformCache.hashTable[hash] = entry;

    g_transformCache.entryCount++;
}

static void EvictOldestCacheEntry(void) {
    /* Find and remove the oldest entry */
    CMCacheEntry *oldestEntry = NULL;
    CMCacheEntry **oldestPrev = NULL;
    uint64_t oldestTime = UINT64_MAX;

    for (uint32_t i = 0; i < g_transformCache.tableSize; i++) {
        CMCacheEntry **prev = &g_transformCache.hashTable[i];
        CMCacheEntry *entry = g_transformCache.hashTable[i];

        while (entry) {
            if (entry->lastUsed < oldestTime) {
                oldestTime = entry->lastUsed;
                oldestEntry = entry;
                oldestPrev = prev;
            }
            prev = &entry->next;
            entry = entry->next;
        }
    }

    if (oldestEntry && oldestPrev) {
        *oldestPrev = oldestEntry->next;
        free(oldestEntry);
        g_transformCache.entryCount--;
    }
}

/* ================================================================
 * UTILITY FUNCTIONS
 * ================================================================ */

static uint64_t GetCurrentTimeUs(void) {
    /* Platform-specific high-resolution timer */
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000) / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
#endif
}

static CMError ValidateLUTDescriptor(const CMLUTDescriptor *lutDesc) {
    if (!lutDesc) return cmParameterError;
    if (!lutDesc->tableData) return cmProfileError;
    if (lutDesc->gridPoints == 0) return cmParameterError;
    if (lutDesc->precision != 8 && lutDesc->precision != 16) return cmParameterError;
    if (lutDesc->inputChannels == 0 || lutDesc->outputChannels == 0) return cmParameterError;

    return cmNoError;
}

/* ================================================================
 * PUBLIC CACHE FUNCTIONS
 * ================================================================ */

CMError CMClearTransformCache(void) {
    CleanupTransformCache();
    return InitializeTransformCache();
}

CMError CMGetCacheStatistics(uint32_t *entries, uint32_t *memoryUsed, uint32_t *hits, uint32_t *misses) {
    if (entries) *entries = g_transformCache.entryCount;
    if (memoryUsed) *memoryUsed = g_transformCache.entryCount * sizeof(CMCacheEntry) / 1024;
    if (hits) *hits = g_transformCache.hits;
    if (misses) *misses = g_transformCache.misses;
    return cmNoError;
}

CMError CMSetCacheSize(uint32_t maxEntries, uint32_t maxMemoryKB) {
    g_transformCache.maxEntries = maxEntries;

    /* Evict entries if over new limit */
    while (g_transformCache.entryCount > maxEntries) {
        EvictOldestCacheEntry();
    }

    return cmNoError;
}
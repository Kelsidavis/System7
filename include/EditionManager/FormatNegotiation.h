/*
 * FormatNegotiation.h
 *
 * Format Negotiation and Conversion API for Edition Manager
 * Handles data format conversion, negotiation, and compatibility
 */

#ifndef __FORMAT_NEGOTIATION_H__
#define __FORMAT_NEGOTIATION_H__

#include "EditionManager/EditionManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Format Converter Function Type
 */
typedef OSErr (*FormatConverterProc)(const void* inputData,
                                    Size inputSize,
                                    void** outputData,
                                    Size* outputSize,
                                    void* userData);

/*
 * Format Quality Levels
 */
typedef enum {
    kFormatQualityLossless,     /* No data loss */
    kFormatQualityHigh,         /* Minimal data loss */
    kFormatQualityMedium,       /* Moderate data loss */
    kFormatQualityLow,          /* Significant data loss */
    kFormatQualityUnknown       /* Quality not determined */
} FormatQuality;

/*
 * Format Conversion Information
 */
typedef struct {
    FormatType fromFormat;      /* Source format */
    FormatType toFormat;        /* Target format */
    FormatQuality quality;      /* Conversion quality */
    uint32_t conversionCost;    /* Relative conversion cost */
    float fidelity;             /* Data fidelity (0.0 to 1.0) */
    uint32_t flags;             /* Conversion flags */
} FormatConversionInfo;

/*
 * Format Negotiation Result
 */
typedef struct {
    FormatType selectedFormat;  /* Negotiated format */
    float compatibility;        /* Compatibility score */
    bool requiresConversion;    /* Whether conversion is needed */
    FormatType originalFormat;  /* Original publisher format */
    FormatQuality quality;      /* Expected quality after conversion */
} FormatNegotiationResult;

/*
 * Core Format Negotiation Functions
 */

/* Initialize format negotiation system */
OSErr InitializeFormatNegotiation(void);

/* Clean up format negotiation system */
void CleanupFormatNegotiation(void);

/* Register a format converter */
OSErr RegisterFormatConverter(FormatType fromFormat,
                             FormatType toFormat,
                             FormatConverterProc converter,
                             int32_t priority);

/* Unregister a format converter */
OSErr UnregisterFormatConverter(FormatType fromFormat, FormatType toFormat);

/* Convert data between formats */
OSErr ConvertFormat(FormatType fromFormat,
                   FormatType toFormat,
                   const void* inputData,
                   Size inputSize,
                   void** outputData,
                   Size* outputSize);

/*
 * Format Negotiation
 */

/* Negotiate best format between publisher and subscriber */
OSErr NegotiateBestFormat(const FormatType* publisherFormats,
                         int32_t publisherCount,
                         const FormatType* subscriberFormats,
                         int32_t subscriberCount,
                         FormatType* bestFormat,
                         float* compatibility);

/* Get detailed negotiation result */
OSErr NegotiateFormats(const FormatType* publisherFormats,
                      int32_t publisherCount,
                      const FormatType* subscriberFormats,
                      int32_t subscriberCount,
                      FormatNegotiationResult* result);

/* Find optimal format for multiple subscribers */
OSErr NegotiateForMultipleSubscribers(const FormatType* publisherFormats,
                                     int32_t publisherCount,
                                     const FormatType** subscriberFormats,
                                     const int32_t* subscriberCounts,
                                     int32_t subscriberCount,
                                     FormatType* optimalFormat);

/*
 * Format Discovery and Capabilities
 */

/* Get formats that can be converted to target format */
OSErr GetSupportedFormats(FormatType targetFormat,
                         FormatType** supportedFormats,
                         int32_t* formatCount);

/* Get formats that source format can be converted to */
OSErr GetConvertibleFormats(FormatType sourceFormat,
                           FormatType** convertibleFormats,
                           int32_t* formatCount);

/* Check if conversion is possible */
OSErr CanConvertFormat(FormatType fromFormat,
                      FormatType toFormat,
                      bool* canConvert);

/* Get conversion information */
OSErr GetConversionInfo(FormatType fromFormat,
                       FormatType toFormat,
                       FormatConversionInfo* info);

/* Get list of all registered converters */
OSErr GetAllConverters(FormatConversionInfo** converters,
                      int32_t* converterCount);

/*
 * Format Compatibility
 */

/* Set compatibility between two formats */
OSErr SetFormatCompatibility(FormatType format1,
                           FormatType format2,
                           float compatibility,
                           uint32_t conversionCost);

/* Get compatibility between two formats */
OSErr GetFormatCompatibility(FormatType format1,
                           FormatType format2,
                           float* compatibility);

/* Build compatibility matrix for formats */
OSErr BuildCompatibilityMatrix(const FormatType* formats,
                              int32_t formatCount,
                              float** matrix);

/*
 * Advanced Format Conversion
 */

/* Multi-step format conversion */
OSErr ConvertFormatWithPath(FormatType fromFormat,
                           FormatType toFormat,
                           const void* inputData,
                           Size inputSize,
                           void** outputData,
                           Size* outputSize,
                           FormatType* conversionPath,
                           int32_t* pathLength);

/* Batch format conversion */
OSErr ConvertMultipleFormats(const FormatType* fromFormats,
                            const FormatType* toFormats,
                            void** inputData,
                            Size* inputSizes,
                            int32_t formatCount,
                            void*** outputData,
                            Size** outputSizes);

/* Conditional format conversion */
typedef bool (*ConversionFilterProc)(FormatType fromFormat,
                                    FormatType toFormat,
                                    const void* data,
                                    Size dataSize,
                                    void* userData);

OSErr ConvertFormatWithFilter(FormatType fromFormat,
                             FormatType toFormat,
                             const void* inputData,
                             Size inputSize,
                             void** outputData,
                             Size* outputSize,
                             ConversionFilterProc filter,
                             void* userData);

/*
 * Format Validation and Quality
 */

/* Validate format data */
OSErr ValidateFormatData(FormatType format,
                        const void* data,
                        Size dataSize,
                        bool* isValid);

/* Get format data quality metrics */
typedef struct {
    float completeness;         /* Data completeness (0.0 to 1.0) */
    float accuracy;             /* Data accuracy (0.0 to 1.0) */
    float integrity;            /* Data integrity (0.0 to 1.0) */
    uint32_t errorCount;        /* Number of errors detected */
    uint32_t warningCount;      /* Number of warnings */
} FormatQualityMetrics;

OSErr GetFormatQuality(FormatType format,
                      const void* data,
                      Size dataSize,
                      FormatQualityMetrics* metrics);

/* Estimate conversion quality */
OSErr EstimateConversionQuality(FormatType fromFormat,
                               FormatType toFormat,
                               const void* inputData,
                               Size inputSize,
                               FormatQuality* estimatedQuality);

/*
 * Format Metadata and Description
 */

/* Format description structure */
typedef struct {
    FormatType formatType;      /* Format identifier */
    char name[64];              /* Human-readable name */
    char description[256];      /* Format description */
    char fileExtension[16];     /* Common file extension */
    char mimeType[64];          /* MIME type */
    uint32_t capabilities;      /* Format capabilities */
    FormatQuality nativeQuality; /* Native quality level */
} FormatDescription;

/* Register format description */
OSErr RegisterFormatDescription(const FormatDescription* description);

/* Get format description */
OSErr GetFormatDescription(FormatType format, FormatDescription* description);

/* Get all format descriptions */
OSErr GetAllFormatDescriptions(FormatDescription** descriptions,
                              int32_t* descriptionCount);

/*
 * Converter Management
 */

/* Converter priority levels */
enum {
    kConverterPriorityLow = 1,
    kConverterPriorityNormal = 10,
    kConverterPriorityHigh = 20,
    kConverterPrioritySystem = 30
};

/* Converter flags */
enum {
    kConverterFlagLossless = 0x0001,    /* Lossless conversion */
    kConverterFlagFast = 0x0002,        /* Fast conversion */
    kConverterFlagDefault = 0x0004,     /* Default converter for format pair */
    kConverterFlagBidirectional = 0x0008 /* Supports reverse conversion */
};

/* Set converter priority */
OSErr SetConverterPriority(FormatType fromFormat,
                          FormatType toFormat,
                          int32_t priority);

/* Get converter priority */
OSErr GetConverterPriority(FormatType fromFormat,
                          FormatType toFormat,
                          int32_t* priority);

/* Enable/disable converter */
OSErr SetConverterEnabled(FormatType fromFormat,
                         FormatType toFormat,
                         bool enabled);

/* Check if converter is enabled */
OSErr IsConverterEnabled(FormatType fromFormat,
                        FormatType toFormat,
                        bool* enabled);

/*
 * Performance and Optimization
 */

/* Conversion statistics */
typedef struct {
    uint32_t conversionCount;   /* Number of conversions performed */
    uint32_t successCount;      /* Successful conversions */
    uint32_t failureCount;      /* Failed conversions */
    uint32_t totalInputSize;    /* Total input data size */
    uint32_t totalOutputSize;   /* Total output data size */
    uint32_t averageTime;       /* Average conversion time (ms) */
    float compressionRatio;     /* Average compression ratio */
} ConversionStatistics;

/* Get conversion statistics */
OSErr GetConversionStatistics(FormatType fromFormat,
                             FormatType toFormat,
                             ConversionStatistics* stats);

/* Reset conversion statistics */
OSErr ResetConversionStatistics(FormatType fromFormat, FormatType toFormat);

/* Set conversion cache size */
OSErr SetConversionCacheSize(Size cacheSize);

/* Clear conversion cache */
OSErr ClearConversionCache(void);

/*
 * Standard Format Support
 */

/* Standard format types */
#define kFormatTypeText         'TEXT'
#define kFormatTypePICT         'PICT'
#define kFormatTypeSound        'snd '
#define kFormatTypeRTF          'RTF '
#define kFormatTypeHTML         'HTML'
#define kFormatTypeXML          'XML '
#define kFormatTypeJSON         'JSON'
#define kFormatTypePDF          'PDF '
#define kFormatTypeJPEG         'JPEG'
#define kFormatTypePNG          'PNG '
#define kFormatTypeGIF          'GIF '
#define kFormatTypeAIFF         'AIFF'
#define kFormatTypeWAV          'WAV '
#define kFormatTypeMP3          'MP3 '

/* Initialize standard format converters */
OSErr InitializeStandardConverters(void);

/* Register platform-specific converters */
OSErr RegisterPlatformConverters(void);

/*
 * Error Codes
 */
enum {
    formatConversionErr = -490,     /* General conversion error */
    converterNotFoundErr = -491,    /* No converter available */
    formatNotSupportedErr = -492,   /* Format not supported */
    conversionQualityErr = -493,    /* Conversion quality too low */
    converterRegistrationErr = -494  /* Converter registration failed */
};

/*
 * Constants
 */
#define kMaxConversionPath 8            /* Maximum conversion steps */
#define kMaxFormatCompatibilities 256   /* Maximum compatibility entries */
#define kDefaultConversionCost 100      /* Default conversion cost */
#define kDefaultCompatibility 0.5f     /* Default compatibility score */

#ifdef __cplusplus
}
#endif

#endif /* __FORMAT_NEGOTIATION_H__ */
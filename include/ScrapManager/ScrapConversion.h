/*
 * ScrapConversion.h - Data Transformation APIs
 * System 7.1 Portable - Scrap Manager Component
 *
 * Defines data format conversion, transformation, and coercion APIs
 * for seamless clipboard data exchange between different formats.
 */

#ifndef SCRAP_CONVERSION_H
#define SCRAP_CONVERSION_H

#include "ScrapTypes.h"
#include "MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Core Conversion Functions
 */

/* Initialize conversion system */
OSErr InitScrapConversion(void);

/* Cleanup conversion system */
void CleanupScrapConversion(void);

/* Convert data between formats */
OSErr ConvertScrapData(Handle sourceData, ResType sourceType,
                      Handle *destData, ResType destType,
                      uint16_t conversionFlags);

/* Check if conversion is possible */
Boolean CanConvertScrapData(ResType sourceType, ResType destType);

/* Get conversion cost estimate */
OSErr GetConversionCost(ResType sourceType, ResType destType,
                       int32_t sourceSize, int32_t *conversionTime,
                       int32_t *memoryNeeded, int16_t *qualityLoss);

/*
 * Converter Registration and Management
 */

/* Register a data converter */
OSErr RegisterDataConverter(ResType sourceType, ResType destType,
                           ScrapConverterProc converter, void *refCon,
                           uint16_t priority, uint16_t flags);

/* Unregister a data converter */
OSErr UnregisterDataConverter(ResType sourceType, ResType destType,
                             ScrapConverterProc converter);

/* Find best converter for types */
ScrapConverterProc FindBestConverter(ResType sourceType, ResType destType,
                                    void **refCon, uint16_t *priority);

/* Enumerate available converters */
OSErr EnumerateConverters(ResType sourceType, ResType destType,
                         ScrapConverter *converters, int16_t *count,
                         int16_t maxConverters);

/*
 * Conversion Chain Management
 */

/* Build conversion chain between formats */
OSErr BuildConversionChain(ResType sourceType, ResType destType,
                          ResType *chainTypes, int16_t *chainLength,
                          int16_t maxSteps);

/* Execute conversion chain */
OSErr ExecuteConversionChain(Handle sourceData, const ResType *chainTypes,
                            int16_t chainLength, Handle *destData);

/* Optimize conversion chain for quality */
OSErr OptimizeConversionChain(const ResType *inputChain, int16_t inputLength,
                             ResType *optimizedChain, int16_t *optimizedLength);

/* Get chain conversion quality */
int16_t GetChainQuality(const ResType *chainTypes, int16_t chainLength);

/*
 * Text Conversion Functions
 */

/* Convert between text encodings */
OSErr ConvertTextEncoding(Handle sourceText, uint32_t sourceEncoding,
                         Handle *destText, uint32_t destEncoding);

/* Convert plain text to styled text */
OSErr PlainToStyledText(Handle plainText, Handle *styledText,
                       Handle *styleInfo, const TextStyle *defaultStyle);

/* Convert styled text to plain text */
OSErr StyledToPlainText(Handle styledText, Handle styleInfo,
                       Handle *plainText);

/* Convert text to uppercase/lowercase */
OSErr ConvertTextCase(Handle textData, int16_t caseConversion);

/* Normalize text formatting */
OSErr NormalizeTextFormat(Handle textData, ResType textType,
                         Handle *normalizedText);

/* Convert line endings */
OSErr ConvertTextLineEndings(Handle textData, int16_t sourceFormat,
                            int16_t destFormat);

/*
 * Image Conversion Functions
 */

/* Convert between image formats */
OSErr ConvertImageFormat(Handle sourceImage, ResType sourceType,
                        Handle *destImage, ResType destType,
                        const ImageConversionOptions *options);

/* Scale image during conversion */
OSErr ConvertAndScaleImage(Handle sourceImage, ResType sourceType,
                          Handle *destImage, ResType destType,
                          int16_t newWidth, int16_t newHeight,
                          int16_t scalingMode);

/* Convert image color depth */
OSErr ConvertImageColorDepth(Handle imageData, ResType imageType,
                            int16_t newDepth, Handle *convertedImage);

/* Extract image thumbnail */
OSErr ExtractImageThumbnail(Handle imageData, ResType imageType,
                           int16_t thumbWidth, int16_t thumbHeight,
                           Handle *thumbnailData);

/* Convert vector to bitmap */
OSErr VectorToBitmap(Handle vectorData, ResType vectorType,
                    Handle *bitmapData, int16_t width, int16_t height,
                    int16_t resolution);

/* Convert bitmap to vector (if possible) */
OSErr BitmapToVector(Handle bitmapData, ResType bitmapType,
                    Handle *vectorData, ResType vectorType);

/*
 * Sound Conversion Functions
 */

/* Convert between sound formats */
OSErr ConvertSoundFormat(Handle sourceSound, ResType sourceType,
                        Handle *destSound, ResType destType,
                        const SoundConversionOptions *options);

/* Convert sound sample rate */
OSErr ConvertSoundSampleRate(Handle soundData, uint32_t sourceSampleRate,
                            uint32_t destSampleRate, Handle *convertedSound);

/* Convert sound bit depth */
OSErr ConvertSoundBitDepth(Handle soundData, int16_t sourceBitDepth,
                          int16_t destBitDepth, Handle *convertedSound);

/* Convert mono to stereo or vice versa */
OSErr ConvertSoundChannels(Handle soundData, int16_t sourceChannels,
                          int16_t destChannels, Handle *convertedSound);

/* Compress/decompress sound */
OSErr CompressSoundData(Handle soundData, ResType compressionType,
                       Handle *compressedSound);

/*
 * File Format Conversion Functions
 */

/* Convert file references */
OSErr ConvertFileReference(const FSSpec *sourceFile, ResType sourceType,
                          FSSpec *destFile, ResType destType);

/* Convert file paths between formats */
OSErr ConvertFilePath(const char *sourcePath, int16_t sourceFormat,
                     char *destPath, int16_t destFormat, int maxLen);

/* Convert file to data */
OSErr FileToScrapData(const FSSpec *fileSpec, ResType dataType,
                     Handle *fileData);

/* Convert data to file */
OSErr ScrapDataToFile(Handle scrapData, ResType dataType,
                     const FSSpec *fileSpec);

/*
 * Structured Data Conversion
 */

/* Convert between structured data formats */
OSErr ConvertStructuredData(Handle sourceData, ResType sourceType,
                           Handle *destData, ResType destType,
                           const ConversionMapping *fieldMappings,
                           int16_t mappingCount);

/* Convert record-based data */
OSErr ConvertRecordData(Handle sourceData, const RecordFormat *sourceFormat,
                       Handle *destData, const RecordFormat *destFormat);

/* Convert hierarchical data */
OSErr ConvertHierarchicalData(Handle sourceData, ResType sourceType,
                             Handle *destData, ResType destType,
                             Boolean preserveStructure);

/*
 * Automatic Format Detection and Conversion
 */

/* Auto-detect best conversion target */
ResType AutoDetectBestFormat(Handle sourceData, ResType sourceType,
                            const ResType *candidateTypes, int16_t typeCount);

/* Auto-convert to most compatible format */
OSErr AutoConvertToCompatible(Handle sourceData, ResType sourceType,
                             Handle *destData, ResType *destType);

/* Convert with quality preferences */
OSErr ConvertWithQualityPrefs(Handle sourceData, ResType sourceType,
                             Handle *destData, ResType destType,
                             int16_t qualityLevel, Boolean preserveMetadata);

/*
 * Lossy Conversion Management
 */

/* Check if conversion is lossy */
Boolean IsLossyConversion(ResType sourceType, ResType destType);

/* Get quality loss estimate */
int16_t EstimateQualityLoss(ResType sourceType, ResType destType,
                           Handle sourceData);

/* Convert with quality control */
OSErr ConvertWithQualityControl(Handle sourceData, ResType sourceType,
                               Handle *destData, ResType destType,
                               int16_t maxQualityLoss);

/* Preview conversion result */
OSErr PreviewConversion(Handle sourceData, ResType sourceType,
                       ResType destType, Handle *previewData,
                       int16_t *qualityRating);

/*
 * Metadata Preservation
 */

/* Extract metadata from data */
OSErr ExtractDataMetadata(Handle sourceData, ResType dataType,
                         Handle *metadataHandle);

/* Apply metadata to converted data */
OSErr ApplyDataMetadata(Handle destData, ResType dataType,
                       Handle metadataHandle);

/* Convert metadata between formats */
OSErr ConvertMetadata(Handle sourceMetadata, ResType sourceType,
                     Handle *destMetadata, ResType destType);

/*
 * Conversion Options and Configuration
 */

/* Set global conversion preferences */
OSErr SetConversionPreferences(const ConversionPrefs *prefs);

/* Get current conversion preferences */
OSErr GetConversionPreferences(ConversionPrefs *prefs);

/* Set converter-specific options */
OSErr SetConverterOptions(ResType sourceType, ResType destType,
                         const void *options, int32_t optionsSize);

/* Get converter capabilities */
OSErr GetConverterCapabilities(ResType sourceType, ResType destType,
                              ConverterCapabilities *capabilities);

/*
 * Data Structures for Conversion Options
 */

typedef struct {
    int16_t         quality;          /* Conversion quality (0-100) */
    Boolean         preserveMetadata; /* Preserve metadata if possible */
    Boolean         allowLossy;       /* Allow lossy conversions */
    int32_t         maxMemoryUsage;   /* Maximum memory for conversion */
    int32_t         timeoutSeconds;   /* Conversion timeout */
} ConversionPrefs;

typedef struct {
    int16_t         width;            /* Target width (0 = preserve) */
    int16_t         height;           /* Target height (0 = preserve) */
    int16_t         colorDepth;       /* Target color depth */
    int16_t         compressionLevel; /* Compression level (0-100) */
    Boolean         preserveAspect;   /* Preserve aspect ratio */
    int16_t         scalingMode;      /* Scaling algorithm */
} ImageConversionOptions;

typedef struct {
    uint32_t        sampleRate;       /* Target sample rate */
    int16_t         bitDepth;         /* Target bit depth */
    int16_t         channels;         /* Target channel count */
    int16_t         compressionType;  /* Compression type */
    int16_t         quality;          /* Audio quality (0-100) */
} SoundConversionOptions;

typedef struct {
    ResType         sourceField;      /* Source field type */
    ResType         destField;        /* Destination field type */
    int16_t         mappingType;      /* Mapping type */
    void            *mappingData;     /* Mapping-specific data */
} ConversionMapping;

typedef struct {
    ResType         fieldType;        /* Field data type */
    int32_t         fieldOffset;     /* Offset in record */
    int32_t         fieldSize;        /* Field size */
    uint16_t        fieldFlags;      /* Field flags */
} RecordField;

typedef struct {
    int16_t         fieldCount;       /* Number of fields */
    int32_t         recordSize;       /* Total record size */
    RecordField     fields[1];        /* Variable length array */
} RecordFormat;

typedef struct {
    Boolean         canConvert;       /* Converter available */
    Boolean         isLossy;          /* Conversion is lossy */
    int16_t         qualityRating;    /* Quality rating (0-100) */
    int32_t         maxSourceSize;    /* Maximum source size */
    uint16_t        supportedOptions; /* Supported option flags */
} ConverterCapabilities;

/*
 * Conversion Constants
 */

/* Conversion flags */
#define CONVERT_FLAG_LOSSY_OK       0x0001  /* Allow lossy conversion */
#define CONVERT_FLAG_PRESERVE_META  0x0002  /* Preserve metadata */
#define CONVERT_FLAG_FAST_MODE      0x0004  /* Fast/low quality mode */
#define CONVERT_FLAG_HIGH_QUALITY   0x0008  /* High quality mode */
#define CONVERT_FLAG_NO_CHAIN       0x0010  /* Disable chain conversion */
#define CONVERT_FLAG_ASYNC          0x0020  /* Asynchronous conversion */

/* Quality levels */
#define QUALITY_DRAFT               25      /* Draft quality */
#define QUALITY_NORMAL              50      /* Normal quality */
#define QUALITY_HIGH                75      /* High quality */
#define QUALITY_MAXIMUM             100     /* Maximum quality */

/* Case conversion types */
#define CASE_UPPER                  1       /* Convert to uppercase */
#define CASE_LOWER                  2       /* Convert to lowercase */
#define CASE_TITLE                  3       /* Convert to title case */
#define CASE_SENTENCE               4       /* Convert to sentence case */

/* Scaling modes */
#define SCALE_NEAREST_NEIGHBOR      0       /* Nearest neighbor */
#define SCALE_BILINEAR              1       /* Bilinear interpolation */
#define SCALE_BICUBIC               2       /* Bicubic interpolation */
#define SCALE_LANCZOS               3       /* Lanczos resampling */

#ifdef __cplusplus
}
#endif

#endif /* SCRAP_CONVERSION_H */
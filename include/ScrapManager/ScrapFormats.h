/*
 * ScrapFormats.h - Scrap Data Format Definitions and Conversion
 * System 7.1 Portable - Scrap Manager Component
 *
 * Defines data format handling, conversion routines, and format validation
 * for the Mac OS Scrap Manager with modern format support.
 */

#ifndef SCRAP_FORMATS_H
#define SCRAP_FORMATS_H

#include "ScrapTypes.h"
#include "MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Format Detection and Validation
 */

/* Detect data format from content */
ResType DetectScrapFormat(const void *data, int32_t size);

/* Validate format data integrity */
OSErr ValidateFormatData(ResType type, const void *data, int32_t size);

/* Check if format is valid Mac type */
Boolean IsValidMacFormat(ResType type);

/* Check if format requires conversion */
Boolean FormatNeedsConversion(ResType sourceType, ResType destType);

/* Get format description string */
void GetFormatDescription(ResType type, Str255 description);

/*
 * Format Conversion Functions
 */

/* Convert between text formats */
OSErr ConvertTextFormats(ResType sourceType, ResType destType,
                        Handle sourceData, Handle *destData);

/* Convert between image formats */
OSErr ConvertImageFormats(ResType sourceType, ResType destType,
                         Handle sourceData, Handle *destData);

/* Convert between sound formats */
OSErr ConvertSoundFormats(ResType sourceType, ResType destType,
                         Handle sourceData, Handle *destData);

/* Generic format conversion */
OSErr ConvertScrapFormat(ResType sourceType, ResType destType,
                        Handle sourceData, Handle *destData);

/*
 * Text Format Handling
 */

/* Convert Mac text to UTF-8 */
OSErr MacTextToUTF8(Handle macText, Handle *utf8Text);

/* Convert UTF-8 to Mac text */
OSErr UTF8ToMacText(Handle utf8Text, Handle *macText);

/* Convert styled text to RTF */
OSErr StyledTextToRTF(Handle textHandle, Handle styleHandle, Handle *rtfData);

/* Convert RTF to styled text */
OSErr RTFToStyledText(Handle rtfData, Handle *textHandle, Handle *styleHandle);

/* Convert text to HTML */
OSErr TextToHTML(Handle textData, Handle *htmlData, Boolean preserveFormatting);

/* Convert HTML to text */
OSErr HTMLToText(Handle htmlData, Handle *textData, Boolean stripTags);

/* Detect text encoding */
OSErr DetectTextEncoding(const void *data, int32_t size, uint32_t *encoding);

/*
 * Image Format Handling
 */

/* Convert PICT to PNG */
OSErr PICTToPNG(Handle pictData, Handle *pngData);

/* Convert PNG to PICT */
OSErr PNGToPICT(Handle pngData, Handle *pictData);

/* Convert PICT to JPEG */
OSErr PICTToJPEG(Handle pictData, Handle *jpegData, int quality);

/* Convert JPEG to PICT */
OSErr JPEGToPICT(Handle jpegData, Handle *pictData);

/* Convert PICT to TIFF */
OSErr PICTToTIFF(Handle pictData, Handle *tiffData);

/* Convert TIFF to PICT */
OSErr TIFFToPICT(Handle tiffData, Handle *pictData);

/* Get image dimensions */
OSErr GetImageDimensions(ResType type, Handle imageData,
                        int16_t *width, int16_t *height);

/* Validate image data */
OSErr ValidateImageData(ResType type, Handle imageData);

/*
 * Sound Format Handling
 */

/* Convert Mac sound to modern format */
OSErr MacSoundToModern(Handle soundData, ResType destType, Handle *modernData);

/* Convert modern sound to Mac format */
OSErr ModernSoundToMac(Handle modernData, ResType sourceType, Handle *macData);

/* Get sound properties */
OSErr GetSoundProperties(Handle soundData, uint32_t *sampleRate,
                        uint16_t *channels, uint16_t *bitDepth);

/*
 * File Format Handling
 */

/* Convert file reference to URL */
OSErr FileRefToURL(const FSSpec *fileSpec, Handle *urlData);

/* Convert URL to file reference */
OSErr URLToFileRef(Handle urlData, FSSpec *fileSpec);

/* Convert HFS file to modern path */
OSErr HFSToModernPath(const FSSpec *hfsSpec, char *modernPath, int maxLen);

/* Convert modern path to HFS */
OSErr ModernPathToHFS(const char *modernPath, FSSpec *hfsSpec);

/*
 * Modern Format Support
 */

/* Convert Mac format to platform format */
OSErr MacToPlatformData(ResType macType, Handle macData,
                       uint32_t platformFormat, void **platformData,
                       size_t *dataSize);

/* Convert platform format to Mac format */
OSErr PlatformToMacData(uint32_t platformFormat, const void *platformData,
                       size_t dataSize, ResType macType, Handle *macData);

/* Register modern format converter */
OSErr RegisterModernConverter(ResType macType, uint32_t platformFormat,
                             const char *mimeType,
                             ScrapConverterProc toModern,
                             ScrapConverterProc fromModern);

/*
 * Format Registry Functions
 */

/* Register a new format type */
OSErr RegisterScrapFormat(ResType type, ConstStr255Param description,
                         ConstStr255Param fileExtension,
                         uint32_t formatFlags);

/* Unregister a format type */
OSErr UnregisterScrapFormat(ResType type);

/* Get format information */
OSErr GetFormatInfo(ResType type, Str255 description,
                   Str255 fileExtension, uint32_t *formatFlags);

/* Enumerate all registered formats */
OSErr EnumerateFormats(ResType *types, int16_t *count, int16_t maxTypes);

/*
 * Conversion Chain Functions
 */

/* Find conversion path between formats */
OSErr FindConversionPath(ResType sourceType, ResType destType,
                        ResType *pathTypes, int16_t *pathLength,
                        int16_t maxSteps);

/* Execute conversion chain */
OSErr ExecuteConversionChain(Handle sourceData, const ResType *pathTypes,
                            int16_t pathLength, Handle *destData);

/* Get conversion quality estimate */
OSErr GetConversionQuality(ResType sourceType, ResType destType,
                          int16_t *qualityRating);

/*
 * Format-Specific Utilities
 */

/* Text format utilities */
OSErr CountTextLines(Handle textData, int32_t *lineCount);
OSErr GetTextLineRange(Handle textData, int32_t lineIndex,
                      int32_t *startOffset, int32_t *endOffset);
OSErr ConvertLineEndings(Handle textData, int16_t destFormat);

/* Image format utilities */
OSErr ScaleImage(Handle imageData, ResType imageType,
                int16_t newWidth, int16_t newHeight,
                Handle *scaledData);
OSErr RotateImage(Handle imageData, ResType imageType,
                 int16_t degrees, Handle *rotatedData);
OSErr GetImageColorDepth(Handle imageData, ResType imageType,
                        int16_t *colorDepth);

/* URL format utilities */
OSErr ParseURL(Handle urlData, Str255 scheme, Str255 host,
              Str255 path, int16_t *port);
OSErr BuildURL(ConstStr255Param scheme, ConstStr255Param host,
              ConstStr255Param path, int16_t port, Handle *urlData);

/*
 * Format Constants
 */

/* Text format flags */
#define TEXT_FORMAT_PLAIN       0x0001
#define TEXT_FORMAT_STYLED      0x0002
#define TEXT_FORMAT_UNICODE     0x0004
#define TEXT_FORMAT_MARKUP      0x0008

/* Image format flags */
#define IMAGE_FORMAT_BITMAP     0x0001
#define IMAGE_FORMAT_VECTOR     0x0002
#define IMAGE_FORMAT_COMPRESSED 0x0004
#define IMAGE_FORMAT_LOSSY      0x0008

/* Sound format flags */
#define SOUND_FORMAT_COMPRESSED 0x0001
#define SOUND_FORMAT_STEREO     0x0002
#define SOUND_FORMAT_16BIT      0x0004
#define SOUND_FORMAT_LOOPED     0x0008

/* File format flags */
#define FILE_FORMAT_REFERENCE   0x0001
#define FILE_FORMAT_EMBEDDED    0x0002
#define FILE_FORMAT_ALIAS       0x0004
#define FILE_FORMAT_PACKAGE     0x0008

/* Line ending constants */
#define LINE_ENDING_CR          0    /* Classic Mac (CR) */
#define LINE_ENDING_LF          1    /* Unix (LF) */
#define LINE_ENDING_CRLF        2    /* Windows (CRLF) */

/* Image scaling modes */
#define SCALE_MODE_NEAREST      0    /* Nearest neighbor */
#define SCALE_MODE_LINEAR       1    /* Linear interpolation */
#define SCALE_MODE_CUBIC        2    /* Cubic interpolation */

/* Conversion quality ratings */
#define CONVERSION_PERFECT      100  /* Lossless conversion */
#define CONVERSION_EXCELLENT    90   /* Minimal quality loss */
#define CONVERSION_GOOD         70   /* Good quality */
#define CONVERSION_FAIR         50   /* Acceptable quality */
#define CONVERSION_POOR         30   /* Poor quality */
#define CONVERSION_LOSSY        10   /* Significant quality loss */

#ifdef __cplusplus
}
#endif

#endif /* SCRAP_FORMATS_H */
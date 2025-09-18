/*
 * ModernFonts.h - Modern Font Format Support
 *
 * Extended Font Manager support for modern font formats including
 * OpenType, WOFF/WOFF2, system fonts, and web fonts.
 * Maintains Mac OS 7.1 API compatibility while adding modern capabilities.
 */

#ifndef MODERN_FONTS_H
#define MODERN_FONTS_H

#include "FontTypes.h"
#include <Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Modern Font Initialization */
OSErr InitializeModernFontSupport(void);
void CleanupModernFontSupport(void);

/* OpenType Font Support */
OSErr LoadOpenTypeFont(ConstStr255Param filePath, OpenTypeFont **font);
OSErr UnloadOpenTypeFont(OpenTypeFont *font);
OSErr ParseOpenTypeFont(const void *fontData, unsigned long dataSize, OpenTypeFont **font);
OSErr GetOpenTypeFontInfo(OpenTypeFont *font, char **familyName, char **styleName);
OSErr GetOpenTypeGlyphMetrics(OpenTypeFont *font, unsigned short glyphID,
                              short *advanceWidth, short *leftSideBearing);
OSErr GetOpenTypeKerning(OpenTypeFont *font, unsigned short leftGlyph,
                         unsigned short rightGlyph, short *kerning);

/* WOFF Font Support */
OSErr LoadWOFFFont(ConstStr255Param filePath, WOFFFont **font);
OSErr LoadWOFF2Font(ConstStr255Param filePath, WOFFFont **font);
OSErr UnloadWOFFFont(WOFFFont *font);
OSErr DecompressWOFF(const void *woffData, unsigned long woffSize,
                     void **otfData, unsigned long *otfSize);
OSErr DecompressWOFF2(const void *woff2Data, unsigned long woff2Size,
                      void **otfData, unsigned long *otfSize);
OSErr ValidateWOFFFont(const void *woffData, unsigned long dataSize);

/* System Font Support */
OSErr InitializeSystemFontSupport(void);
OSErr ScanSystemFontDirectories(void);
OSErr LoadSystemFont(ConstStr255Param fontName, SystemFont **font);
OSErr UnloadSystemFont(SystemFont *font);
OSErr GetSystemFontPath(ConstStr255Param fontName, char *filePath, unsigned long maxPath);
OSErr RegisterSystemFontPath(ConstStr255Param directoryPath);
OSErr GetAvailableSystemFonts(char ***fontNames, unsigned long *count);

/* Font Collection Support */
OSErr LoadFontCollection(ConstStr255Param filePath, FontCollection **collection);
OSErr UnloadFontCollection(FontCollection *collection);
OSErr GetFontFromCollection(FontCollection *collection, unsigned long index, OpenTypeFont **font);
OSErr GetCollectionFontCount(FontCollection *collection, unsigned long *count);
OSErr GetCollectionFontInfo(FontCollection *collection, unsigned long index,
                           char **familyName, char **styleName);

/* Font Directory Management */
OSErr InitializeFontDirectory(void);
OSErr RefreshFontDirectory(void);
OSErr AddFontToDirectory(ConstStr255Param filePath);
OSErr RemoveFontFromDirectory(ConstStr255Param filePath);
OSErr FindFontInDirectory(ConstStr255Param familyName, ConstStr255Param styleName,
                         FontDirectoryEntry **entry);
OSErr GetFontDirectory(FontDirectory **directory);

/* Modern Font Detection and Validation */
unsigned short DetectFontFormat(const void *fontData, unsigned long dataSize);
OSErr ValidateFontFile(ConstStr255Param filePath);
OSErr GetFontFileInfo(ConstStr255Param filePath, unsigned short *format,
                     char **familyName, char **styleName);
Boolean IsModernFontFormat(unsigned short format);

/* Font Format Conversion */
OSErr ConvertToOpenType(const void *sourceData, unsigned long sourceSize,
                       unsigned short sourceFormat, void **otfData, unsigned long *otfSize);
OSErr ExtractFontMetrics(ModernFont *font, FMetricRec *metrics);
OSErr CreateMacFontRecord(ModernFont *font, FontRec **fontRec);

/* Web Font Support */
OSErr DownloadWebFont(ConstStr255Param url, ConstStr255Param cachePath,
                     WebFontMetadata *metadata);
OSErr LoadWebFont(ConstStr255Param filePath, WebFontMetadata *metadata, ModernFont **font);
OSErr ParseWebFontCSS(ConstStr255Param cssPath, WebFontMetadata **metadataArray,
                     unsigned long *count);
OSErr ValidateWebFont(ConstStr255Param filePath, WebFontMetadata *metadata);

/* Font Rendering Support */
OSErr RenderModernGlyph(ModernFont *font, unsigned short glyphID, unsigned short size,
                       void **bitmap, unsigned long *bitmapSize, short *width, short *height);
OSErr GetModernGlyphOutline(ModernFont *font, unsigned short glyphID, unsigned short size,
                           void **outline, unsigned long *outlineSize);
OSErr ApplyFontHinting(ModernFont *font, Boolean enableHinting);
OSErr SetFontRenderingOptions(ModernFont *font, unsigned short options);

/* Variable Font Support */
OSErr IsVariableFont(ModernFont *font, Boolean *isVariable);
OSErr GetVariableAxes(ModernFont *font, void **axes, unsigned long *axisCount);
OSErr SetVariableInstance(ModernFont *font, const void *coordinates, unsigned long coordCount);
OSErr GetNamedInstances(ModernFont *font, void **instances, unsigned long *instanceCount);

/* Font Subsetting and Optimization */
OSErr CreateFontSubset(ModernFont *font, const unsigned short *glyphIDs,
                      unsigned long glyphCount, ModernFont **subset);
OSErr OptimizeFontForWeb(ModernFont *font, ModernFont **optimized);
OSErr CompressFontToWOFF2(ModernFont *font, void **woff2Data, unsigned long *woff2Size);

/* Platform-Specific Integration */
#ifdef __APPLE__
OSErr LoadCoreTextFont(ConstStr255Param fontName, SystemFont **font);
OSErr GetCoreTextFontDescriptor(SystemFont *font, void **descriptor);
#endif

#ifdef _WIN32
OSErr LoadDirectWriteFont(ConstStr255Param fontName, SystemFont **font);
OSErr GetDirectWriteFontFace(SystemFont *font, void **fontFace);
#endif

#ifdef __linux__
OSErr LoadFontconfigFont(ConstStr255Param fontName, SystemFont **font);
OSErr GetFontconfigPattern(SystemFont *font, void **pattern);
#endif

/* Modern Font Constants */
enum {
    kModernFontRenderingDefault = 0,
    kModernFontRenderingAntialias = 1,
    kModernFontRenderingSubpixel = 2,
    kModernFontRenderingHinted = 4,
    kModernFontRenderingSmoothed = 8
};

enum {
    kFontVariationWeight = 0x77676874,  /* 'wght' */
    kFontVariationWidth = 0x77647468,   /* 'wdth' */
    kFontVariationSlant = 0x736C6E74,   /* 'slnt' */
    kFontVariationOpticalSize = 0x6F70737A  /* 'opsz' */
};

/* Error codes for modern font support */
enum {
    kModernFontNotSupportedErr = -6000,
    kModernFontCorruptErr = -6001,
    kModernFontDecompressionErr = -6002,
    kModernFontSystemErr = -6003,
    kModernFontNetworkErr = -6004,
    kModernFontValidationErr = -6005
};

/* Font Loading Options */
typedef struct FontLoadOptions {
    Boolean loadGlyphData;      /* Load glyph data immediately */
    Boolean enableHinting;      /* Enable font hinting */
    Boolean enableKerning;      /* Enable kerning tables */
    Boolean cacheMetrics;       /* Cache font metrics */
    unsigned short renderingMode; /* Rendering mode flags */
    void *platformOptions;      /* Platform-specific options */
} FontLoadOptions;

/* Modern Font Cache */
typedef struct ModernFontCache {
    ModernFont **fonts;         /* Cached fonts */
    unsigned long count;        /* Number of cached fonts */
    unsigned long capacity;     /* Cache capacity */
    unsigned long totalSize;    /* Total memory usage */
    unsigned long maxSize;      /* Maximum cache size */
    Boolean enableCompression;  /* Compress cached data */
} ModernFontCache;

/* Font Matching Criteria */
typedef struct FontMatchCriteria {
    char *familyName;          /* Font family name */
    char *styleName;           /* Font style name */
    unsigned short weight;      /* Font weight (100-900) */
    unsigned short width;       /* Font width (1-9) */
    unsigned short slope;       /* Font slope */
    unsigned short format;      /* Preferred format */
    Boolean exactMatch;         /* Require exact match */
    Boolean allowSubstitution;  /* Allow font substitution */
} FontMatchCriteria;

/* Advanced Font Operations */
OSErr FindBestFontMatch(FontMatchCriteria *criteria, ModernFont **font);
OSErr GetFontSimilarity(ModernFont *font1, ModernFont *font2, float *similarity);
OSErr CreateFontFallbackChain(ConstStr255Param familyName, ModernFont ***chain,
                             unsigned long *chainLength);
OSErr GetFontCapabilities(ModernFont *font, unsigned long *capabilities);

/* Font Capabilities Flags */
enum {
    kFontCapabilityKerning = 0x01,
    kFontCapabilityLigatures = 0x02,
    kFontCapabilityVariations = 0x04,
    kFontCapabilityColorGlyphs = 0x08,
    kFontCapabilityOpenTypeFeatures = 0x10,
    kFontCapabilityBitmapStrikes = 0x20
};

#ifdef __cplusplus
}
#endif

#endif /* MODERN_FONTS_H */
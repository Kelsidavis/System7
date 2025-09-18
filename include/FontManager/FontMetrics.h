/*
 * FontMetrics.h - Font Measurement and Metrics APIs
 *
 * Comprehensive font measurement functions for character widths,
 * heights, spacing, kerning, and text layout calculations.
 */

#ifndef FONT_METRICS_H
#define FONT_METRICS_H

#include "FontTypes.h"
#include <Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Font Measurement Constants */
enum {
    kFontMetricsVersion = 1,
    kMaxKerningPairs = 1024,
    kMaxStyleVariants = 16
};

/* Measurement Modes */
enum {
    kMeasureExact = 0,         /* Exact pixel measurements */
    kMeasureFractional = 1,    /* Fractional measurements */
    kMeasureRounded = 2,       /* Rounded measurements */
    kMeasureDevice = 3         /* Device-specific measurements */
};

/* Text Direction */
enum {
    kTextDirectionLTR = 0,     /* Left to right */
    kTextDirectionRTL = 1,     /* Right to left */
    kTextDirectionTTB = 2,     /* Top to bottom */
    kTextDirectionBTT = 3      /* Bottom to top */
};

/* Advanced Font Metrics Structure */
typedef struct FontMetrics {
    Fixed ascent;              /* Ascent from baseline */
    Fixed descent;             /* Descent from baseline */
    Fixed leading;             /* Leading between lines */
    Fixed lineHeight;          /* Total line height */
    Fixed widMax;              /* Maximum character width */
    Fixed xHeight;             /* Height of lowercase x */
    Fixed capHeight;           /* Height of capital letters */
    Fixed avgCharWidth;        /* Average character width */
    Fixed maxCharWidth;        /* Maximum character width */
    Fixed minCharWidth;        /* Minimum character width */
    Fixed underlinePosition;   /* Underline position */
    Fixed underlineThickness;  /* Underline thickness */
    Fixed strikeoutPosition;   /* Strikeout position */
    Fixed strikeoutThickness;  /* Strikeout thickness */
    Fixed italicAngle;         /* Italic angle in degrees */
    Fixed superscriptOffset;   /* Superscript offset */
    Fixed subscriptOffset;     /* Subscript offset */
    Fixed superscriptSize;     /* Superscript size factor */
    Fixed subscriptSize;       /* Subscript size factor */
} FontMetrics;

/* Character Metrics Structure */
typedef struct CharMetrics {
    Fixed width;               /* Character width */
    Fixed height;              /* Character height */
    Fixed leftBearing;         /* Left side bearing */
    Fixed rightBearing;        /* Right side bearing */
    Fixed topBearing;          /* Top bearing */
    Fixed bottomBearing;       /* Bottom bearing */
    Rect boundingBox;          /* Character bounding box */
    Fixed advanceWidth;        /* Advance width */
    Fixed advanceHeight;       /* Advance height */
} CharMetrics;

/* Text Metrics Structure */
typedef struct TextMetrics {
    Fixed totalWidth;          /* Total text width */
    Fixed totalHeight;         /* Total text height */
    Fixed lineWidth;           /* Width of current line */
    Fixed lineHeight;          /* Height of current line */
    Rect boundingBox;          /* Text bounding box */
    Rect inkBounds;            /* Ink bounding box */
    short lineCount;           /* Number of lines */
    short charCount;           /* Number of characters */
    Fixed *charWidths;         /* Individual character widths */
    Point *charPositions;      /* Character positions */
} TextMetrics;

/* Basic Font Metrics */
OSErr GetFontMetrics(short familyID, short size, short style, FontMetrics *metrics);
OSErr GetFontAscent(short familyID, short size, short style, Fixed *ascent);
OSErr GetFontDescent(short familyID, short size, short style, Fixed *descent);
OSErr GetFontLeading(short familyID, short size, short style, Fixed *leading);
OSErr GetFontLineHeight(short familyID, short size, short style, Fixed *lineHeight);

/* Character Measurements */
OSErr GetCharWidth(short familyID, short size, short style, char character, Fixed *width);
OSErr GetCharHeight(short familyID, short size, short style, char character, Fixed *height);
OSErr GetCharMetrics(short familyID, short size, short style, char character, CharMetrics *metrics);
OSErr GetCharBounds(short familyID, short size, short style, char character, Rect *bounds);
OSErr GetCharAdvance(short familyID, short size, short style, char character,
                     Fixed *advanceX, Fixed *advanceY);

/* String Measurements */
OSErr MeasureString(short familyID, short size, short style, ConstStr255Param text,
                    Fixed *width, Fixed *height);
OSErr MeasureText(short familyID, short size, short style, const void *text, short length,
                  Fixed *width, Fixed *height);
OSErr MeasureTextAdvanced(short familyID, short size, short style, const void *text,
                         short length, short measureMode, TextMetrics *metrics);

/* Line Breaking and Layout */
OSErr FindLineBreak(short familyID, short size, short style, const void *text,
                   short textLength, Fixed maxWidth, short *breakOffset,
                   Fixed *actualWidth, Boolean *isWhitespace);
OSErr MeasureLineHeight(short familyID, short size, short style, const void *text,
                       short length, Fixed *lineHeight, Fixed *ascent, Fixed *descent);
OSErr CalculateTextLayout(short familyID, short size, short style, const void *text,
                         short length, Fixed maxWidth, TextMetrics *metrics);

/* Kerning Support */
OSErr GetKerningValue(short familyID, short size, short style, char first, char second,
                     Fixed *kerning);
OSErr GetKerningTable(short familyID, short size, short style,
                     KernPair **pairs, short *count);
OSErr ApplyKerning(short familyID, short size, short style, const void *text,
                  short length, Fixed *positions);

/* Advanced Text Measurements */
OSErr MeasureStyledText(const void *styledText, short length, TextMetrics *metrics);
OSErr MeasureMultiStyleText(const void *text, const void *styles, short length,
                           TextMetrics *metrics);
OSErr GetTextBoundingBox(short familyID, short size, short style, const void *text,
                        short length, Rect *bounds, Rect *inkBounds);

/* Character Set Support */
OSErr GetCharacterSupport(short familyID, char character, Boolean *isSupported);
OSErr GetSupportedCharacters(short familyID, char **characters, short *count);
OSErr GetMissingCharacters(short familyID, const void *text, short length,
                          char **missing, short *count);

/* Font Comparison */
OSErr CompareFontMetrics(short familyID1, short size1, short style1,
                        short familyID2, short size2, short style2,
                        Fixed tolerance, Boolean *areEqual);
OSErr GetMetricsDifference(short familyID1, short size1, short style1,
                          short familyID2, short size2, short style2,
                          FontMetrics *difference);

/* Scale-Aware Measurements */
OSErr MeasureWithScale(short familyID, short size, short style, Point numer, Point denom,
                      const void *text, short length, TextMetrics *metrics);
OSErr ScaleMetrics(FontMetrics *metrics, Point numer, Point denom);
OSErr GetScaledCharWidth(short familyID, short size, short style, char character,
                        Point numer, Point denom, Fixed *width);

/* Device-Specific Measurements */
OSErr MeasureForDevice(short familyID, short size, short style, short device,
                      const void *text, short length, TextMetrics *metrics);
OSErr GetDeviceMetrics(short familyID, short size, short style, short device,
                      FontMetrics *metrics);
OSErr ConvertMetricsToDevice(FontMetrics *metrics, short device, FontMetrics *deviceMetrics);

/* Fractional Width Support */
OSErr EnableFractionalWidths(Boolean enable);
Boolean GetFractionalWidthsEnabled(void);
OSErr MeasureWithFractionalWidths(short familyID, short size, short style,
                                 const void *text, short length, Fixed *widths,
                                 short *count);

/* International Text Support */
OSErr MeasureInternationalText(short familyID, short size, short style, short script,
                              const void *text, short length, short direction,
                              TextMetrics *metrics);
OSErr GetScriptMetrics(short familyID, short size, short style, short script,
                      FontMetrics *metrics);
OSErr MeasureBidirectionalText(short familyID, short size, short style,
                              const void *text, short length, const char *levels,
                              TextMetrics *metrics);

/* Caching for Performance */
OSErr InitMetricsCache(short maxEntries);
OSErr FlushMetricsCache(void);
OSErr CacheCharacterMetrics(short familyID, short size, short style, char character);
OSErr GetCachedMetrics(short familyID, short size, short style, char character,
                      CharMetrics **metrics);

/* Measurement Utilities */
Fixed PixelsToPoints(short pixels, short dpi);
short PointsToPixels(Fixed points, short dpi);
OSErr ConvertToFontUnits(Fixed value, short pointSize, short unitsPerEm, Fixed *fontUnits);
OSErr ConvertFromFontUnits(Fixed fontUnits, short pointSize, short unitsPerEm, Fixed *value);

/* Width Table Management */
OSErr BuildWidthTable(short familyID, short size, short style, WidthTable **table);
OSErr GetWidthTableEntry(WidthTable *table, char character, Fixed *width);
OSErr SetWidthTableEntry(WidthTable *table, char character, Fixed width);
OSErr DisposeWidthTable(WidthTable *table);

/* Legacy Compatibility */
OSErr GetOldStyleMetrics(short familyID, short size, short style,
                        short *ascent, short *descent, short *widMax, short *leading);
OSErr ConvertToOldStyleMetrics(const FontMetrics *newMetrics,
                              short *ascent, short *descent, short *widMax, short *leading);

/* Text Justification Support */
OSErr CalculateJustification(short familyID, short size, short style,
                           const void *text, short length, Fixed targetWidth,
                           Fixed *expansion, Fixed *compression, short *spaces);
OSErr DistributeSpacing(const void *text, short length, Fixed totalExpansion,
                       Fixed *charSpacing, short *count);

/* Debugging and Analysis */
OSErr DumpFontMetrics(short familyID, short size, short style);
OSErr AnalyzeTextMetrics(const void *text, short length, short familyID,
                        short size, short style, char **report);
OSErr ValidateMetrics(const FontMetrics *metrics, Boolean *isValid, char **errors);

#ifdef __cplusplus
}
#endif

#endif /* FONT_METRICS_H */
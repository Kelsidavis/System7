/* #include "SystemTypes.h" */
#include "FontManager/FontInternal.h"
#include <string.h>
/*
 * FontMetrics.c - Font Metrics and Text Measurement Implementation
 *
 * Comprehensive font measurement functions for character widths,
 * heights, spacing, kerning, and text layout calculations.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"

#include "FontManager/FontMetrics.h"
#include "FontManager/FontManager.h"
#include "FontManager/BitmapFonts.h"
#include "FontManager/TrueTypeFonts.h"
#include <Memory.h>
#include <Errors.h>
#include <QuickDraw.h>
#include "FontManager/FontLogging.h"


/* Metrics cache for performance */
typedef struct MetricsCacheEntry {
    short familyID;
    short size;
    short style;
    FontMetrics metrics;
    unsigned long lastUsed;
    struct MetricsCacheEntry *next;
} MetricsCacheEntry;

static MetricsCacheEntry *gMetricsCache = NULL;
static short gMetricsCacheSize = 0;
static short gMaxCacheEntries = 32;
static Boolean gFractionalWidthsEnabled = FALSE;

/* Internal helper functions */
static OSErr GetFontMetricsFromBitmap(BitmapFontData *fontData, FontMetrics *metrics);
static OSErr GetFontMetricsFromTrueType(TTFont *font, short pointSize, FontMetrics *metrics);
static OSErr CalculateTextBounds(short familyID, short size, short style, const void *text,
                                 short length, Rect *bounds, Rect *inkBounds);
static MetricsCacheEntry *FindMetricsInCache(short familyID, short size, short style);
static OSErr AddMetricsToCache(short familyID, short size, short style, const FontMetrics *metrics);
static OSErr BreakLineAtWhitespace(const void *text, short length, Fixed maxWidth,
                                  short familyID, short size, short style, short *breakOffset);
static Fixed MeasureCharacterWidth(short familyID, short size, short style, char character);
static unsigned long GetTickCount(void);

/*
 * GetFontMetrics - Get comprehensive font metrics
 */
OSErr GetFontMetrics(short familyID, short size, short style, FontMetrics *metrics)
{
    MetricsCacheEntry *cacheEntry;
    BitmapFontData *bitmapFont;
    TTFont *ttFont;
    OSErr error;

    if (metrics == NULL) {
        return paramErr;
    }

    memset(metrics, 0, sizeof(FontMetrics));

    /* Check cache first */
    cacheEntry = FindMetricsInCache(familyID, size, style);
    if (cacheEntry != NULL) {
        *metrics = cacheEntry->metrics;
        cacheEntry->lastUsed = GetTickCount();
        return noErr;
    }

    /* Try to load TrueType font first */
    error = LoadTrueTypeFont(familyID, &ttFont);
    if (error == noErr && ttFont != NULL) {
        error = GetFontMetricsFromTrueType(ttFont, size, metrics);
        UnloadTrueTypeFont(ttFont);
        if (error == noErr) {
            AddMetricsToCache(familyID, size, style, metrics);
            return noErr;
        }
    }

    /* Try bitmap font */
    error = LoadBitmapFont(familyID, &bitmapFont);
    if (error == noErr && bitmapFont != NULL) {
        error = GetFontMetricsFromBitmap(bitmapFont, metrics);
        UnloadBitmapFont(bitmapFont);
        if (error == noErr) {
            AddMetricsToCache(familyID, size, style, metrics);
            return noErr;
        }
    }

    return fontNotFoundErr;
}

/*
 * GetFontAscent - Get font ascent value
 */
OSErr GetFontAscent(short familyID, short size, short style, Fixed *ascent)
{
    FontMetrics metrics;
    OSErr error;

    if (ascent == NULL) {
        return paramErr;
    }

    error = GetFontMetrics(familyID, size, style, &metrics);
    if (error == noErr) {
        *ascent = metrics.ascent;
    }

    return error;
}

/*
 * GetFontDescent - Get font descent value
 */
OSErr GetFontDescent(short familyID, short size, short style, Fixed *descent)
{
    FontMetrics metrics;
    OSErr error;

    if (descent == NULL) {
        return paramErr;
    }

    error = GetFontMetrics(familyID, size, style, &metrics);
    if (error == noErr) {
        *descent = metrics.descent;
    }

    return error;
}

/*
 * GetFontLeading - Get font leading value
 */
OSErr GetFontLeading(short familyID, short size, short style, Fixed *leading)
{
    FontMetrics metrics;
    OSErr error;

    if (leading == NULL) {
        return paramErr;
    }

    error = GetFontMetrics(familyID, size, style, &metrics);
    if (error == noErr) {
        *leading = metrics.leading;
    }

    return error;
}

/*
 * GetFontLineHeight - Get total line height
 */
OSErr GetFontLineHeight(short familyID, short size, short style, Fixed *lineHeight)
{
    FontMetrics metrics;
    OSErr error;

    if (lineHeight == NULL) {
        return paramErr;
    }

    error = GetFontMetrics(familyID, size, style, &metrics);
    if (error == noErr) {
        *lineHeight = metrics.ascent + metrics.descent + metrics.leading;
    }

    return error;
}

/*
 * GetCharWidth - Get width of a single character
 */
OSErr GetCharWidth(short familyID, short size, short style, char character, Fixed *width)
{
    if (width == NULL) {
        return paramErr;
    }

    *width = MeasureCharacterWidth(familyID, size, style, character);
    return noErr;
}

/*
 * GetCharHeight - Get height of a single character
 */
OSErr GetCharHeight(short familyID, short size, short style, char character, Fixed *height)
{
    FontMetrics metrics;
    OSErr error;

    if (height == NULL) {
        return paramErr;
    }

    error = GetFontMetrics(familyID, size, style, &metrics);
    if (error == noErr) {
        *height = metrics.ascent + metrics.descent;
    }

    return error;
}

/*
 * GetCharMetrics - Get comprehensive character metrics
 */
OSErr GetCharMetrics(short familyID, short size, short style, char character, CharMetrics *metrics)
{
    FontMetrics fontMetrics;
    OSErr error;

    if (metrics == NULL) {
        return paramErr;
    }

    memset(metrics, 0, sizeof(CharMetrics));

    /* Get font metrics */
    error = GetFontMetrics(familyID, size, style, &fontMetrics);
    if (error != noErr) {
        return error;
    }

    /* Get character-specific measurements */
    metrics->width = MeasureCharacterWidth(familyID, size, style, character);
    metrics->height = fontMetrics.ascent + fontMetrics.descent;
    metrics->advanceWidth = metrics->width; /* Simplified */

    /* Set bounding box */
    (metrics)->left = 0;
    (metrics)->top = -(fontMetrics.ascent >> 16);
    (metrics)->right = metrics->width >> 16;
    (metrics)->bottom = fontMetrics.descent >> 16;

    return noErr;
}

/*
 * MeasureString - Measure Pascal string width and height
 */
OSErr MeasureString(short familyID, short size, short style, ConstStr255Param text,
                    Fixed *width, Fixed *height)
{
    if (text == NULL || width == NULL || height == NULL) {
        return paramErr;
    }

    return MeasureText(familyID, size, style, &text[1], text[0], width, height);
}

/*
 * MeasureText - Measure text width and height
 */
OSErr MeasureText(short familyID, short size, short style, const void *text, short length,
                  Fixed *width, Fixed *height)
{
    const char *textPtr;
    short i;
    Fixed totalWidth = 0;
    FontMetrics metrics;
    OSErr error;

    if (text == NULL || width == NULL || height == NULL) {
        return paramErr;
    }

    *width = 0;
    *height = 0;

    if (length <= 0) {
        return noErr;
    }

    /* Get font metrics for height */
    error = GetFontMetrics(familyID, size, style, &metrics);
    if (error != noErr) {
        return error;
    }

    *height = metrics.ascent + metrics.descent + metrics.leading;

    /* Measure character widths */
    textPtr = (const char *)text;
    for (i = 0; i < length; i++) {
        Fixed charWidth = MeasureCharacterWidth(familyID, size, style, textPtr[i]);
        totalWidth += charWidth;
    }

    *width = totalWidth;
    return noErr;
}

/*
 * MeasureTextAdvanced - Advanced text measurement with detailed metrics
 */
OSErr MeasureTextAdvanced(short familyID, short size, short style, const void *text,
                         short length, short measureMode, TextMetrics *metrics)
{
    const char *textPtr;
    short i;
    Fixed totalWidth = 0;
    FontMetrics fontMetrics;
    OSErr error;

    if (text == NULL || metrics == NULL) {
        return paramErr;
    }

    memset(metrics, 0, sizeof(TextMetrics));

    if (length <= 0) {
        return noErr;
    }

    /* Get font metrics */
    error = GetFontMetrics(familyID, size, style, &fontMetrics);
    if (error != noErr) {
        return error;
    }

    metrics->lineHeight = fontMetrics.ascent + fontMetrics.descent + fontMetrics.leading;
    metrics->lineCount = 1;
    metrics->charCount = length;

    /* Allocate arrays for detailed measurements with overflow protection */
    if (measureMode != kMeasureExact) {
        /* Sanity check length to prevent excessive allocations */
        if (length < 0 || length > 0x10000) {  /* Max 64K characters */
            return paramErr;
        }

        /* Check for integer overflow in allocation size */
        if (length > SIZE_MAX / sizeof(Fixed)) {
            return fontOutOfMemoryErr;
        }
        if (length > SIZE_MAX / sizeof(Point)) {
            return fontOutOfMemoryErr;
        }

        metrics->charWidths = (Fixed *)NewPtr(length * sizeof(Fixed));
        metrics->charPositions = (Point *)NewPtr(length * sizeof(Point));

        if (metrics->charWidths == NULL || metrics->charPositions == NULL) {
            if (metrics->charWidths) DisposePtr((Ptr)metrics->charWidths);
            if (metrics->charPositions) DisposePtr((Ptr)metrics->charPositions);
            return fontOutOfMemoryErr;
        }
    }

    /* Measure each character */
    textPtr = (const char *)text;
    for (i = 0; i < length; i++) {
        Fixed charWidth = MeasureCharacterWidth(familyID, size, style, textPtr[i]);

        if (metrics->charWidths != NULL) {
            metrics->charWidths[i] = charWidth;
        }

        if (metrics->charPositions != NULL) {
            metrics->charPositions[i].h = totalWidth >> 16;
            metrics->charPositions[i].v = 0;
        }

        totalWidth += charWidth;
    }

    metrics->totalWidth = totalWidth;
    metrics->lineWidth = totalWidth;
    metrics->totalHeight = metrics->lineHeight;

    /* Set bounding rectangles */
    (metrics)->left = 0;
    (metrics)->top = -(fontMetrics.ascent >> 16);
    (metrics)->right = totalWidth >> 16;
    (metrics)->bottom = fontMetrics.descent >> 16;

    metrics->inkBounds = metrics->boundingBox; /* Simplified */

    return noErr;
}

/*
 * FindLineBreak - Find appropriate line break position
 */
OSErr FindLineBreak(short familyID, short size, short style, const void *text,
                   short textLength, Fixed maxWidth, short *breakOffset,
                   Fixed *actualWidth, Boolean *isWhitespace)
{
    const char *textPtr;
    short i;
    Fixed currentWidth = 0;
    Fixed wordWidth = 0;
    short lastSpaceOffset = -1;
    Fixed lastSpaceWidth = 0;

    if (text == NULL || breakOffset == NULL || actualWidth == NULL || isWhitespace == NULL) {
        return paramErr;
    }

    *breakOffset = textLength;
    *actualWidth = 0;
    *isWhitespace = FALSE;

    if (textLength <= 0) {
        return noErr;
    }

    textPtr = (const char *)text;

    /* Find break point by measuring characters */
    for (i = 0; i < textLength; i++) {
        char character = textPtr[i];
        Fixed charWidth = MeasureCharacterWidth(familyID, size, style, character);

        /* Check for whitespace */
        if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
            lastSpaceOffset = i;
            lastSpaceWidth = currentWidth;
            *isWhitespace = TRUE;
        }

        /* Check if adding this character would exceed max width */
        if (currentWidth + charWidth > maxWidth) {
            /* Use last whitespace break if available */
            if (lastSpaceOffset >= 0) {
                *breakOffset = lastSpaceOffset;
                *actualWidth = lastSpaceWidth;
            } else {
                /* Break at current position */
                *breakOffset = (i > 0) ? i : 1; /* At least one character */
                *actualWidth = currentWidth;
            }
            return noErr;
        }

        currentWidth += charWidth;
    }

    /* Entire text fits */
    *breakOffset = textLength;
    *actualWidth = currentWidth;
    return noErr;
}

/*
 * GetKerningValue - Get kerning between two characters
 */
OSErr GetKerningValue(short familyID, short size, short style, char first, char second,
                     Fixed *kerning)
{
    if (kerning == NULL) {
        return paramErr;
    }

    /* Simplified implementation - no kerning for bitmap fonts */
    *kerning = 0;

    /* For TrueType fonts, would parse kerning table */
    return noErr;
}

/*
 * EnableFractionalWidths - Enable/disable fractional character widths
 */
OSErr EnableFractionalWidths(Boolean enable)
{
    gFractionalWidthsEnabled = enable;
    return noErr;
}

/*
 * GetFractionalWidthsEnabled - Check if fractional widths are enabled
 */
Boolean GetFractionalWidthsEnabled(void)
{
    return gFractionalWidthsEnabled;
}

/*
 * InitMetricsCache - Initialize metrics cache
 */
OSErr InitMetricsCache(short maxEntries)
{
    /* Flush existing cache */
    FlushMetricsCache();

    gMaxCacheEntries = maxEntries;
    return noErr;
}

/*
 * FlushMetricsCache - Clear metrics cache
 */
OSErr FlushMetricsCache(void)
{
    MetricsCacheEntry *entry, *nextEntry;

    entry = gMetricsCache;
    while (entry != NULL) {
        nextEntry = entry->next;
        DisposePtr((Ptr)entry);
        entry = nextEntry;
    }

    gMetricsCache = NULL;
    gMetricsCacheSize = 0;
    return noErr;
}

/*
 * PixelsToPoints - Convert pixels to points
 */
Fixed PixelsToPoints(short pixels, short dpi)
{
    if (dpi <= 0) {
        dpi = 72; /* Default screen resolution */
    }

    return ((long)pixels << 16) * 72L / dpi;
}

/*
 * PointsToPixels - Convert points to pixels
 */
short PointsToPixels(Fixed points, short dpi)
{
    if (dpi <= 0) {
        dpi = 72; /* Default screen resolution */
    }

    return (short)(((long)points * dpi) >> 16) / 72;
}

/* Internal helper function implementations */

static OSErr GetFontMetricsFromBitmap(BitmapFontData *fontData, FontMetrics *metrics)
{
    if (fontData == NULL || metrics == NULL) {
        return paramErr;
    }

    /* Convert bitmap font metrics to Fixed-point values */
    metrics->ascent = (long)(fontData)->ascent << 16;
    metrics->descent = (long)(fontData)->descent << 16;
    metrics->leading = (long)(fontData)->leading << 16;
    metrics->lineHeight = metrics->ascent + metrics->descent + metrics->leading;
    metrics->widMax = (long)(fontData)->widMax << 16;

    /* Calculate additional metrics */
    metrics->xHeight = metrics->ascent / 2; /* Approximate */
    metrics->capHeight = (metrics->ascent * 3) / 4; /* Approximate */
    metrics->avgCharWidth = metrics->widMax / 2; /* Approximate */
    metrics->maxCharWidth = metrics->widMax;
    metrics->minCharWidth = metrics->widMax / 4; /* Approximate */

    /* Underline metrics */
    metrics->underlinePosition = metrics->descent / 4;
    metrics->underlineThickness = 0x00010000; /* 1 point */

    return noErr;
}

static OSErr GetFontMetricsFromTrueType(TTFont *font, short pointSize, FontMetrics *metrics)
{
    Fixed scale;

    if (font == NULL || metrics == NULL) {
        return paramErr;
    }

    if (!font->valid || font->head == NULL || font->hhea == NULL) {
        return fontCorruptErr;
    }

    /* Check for division by zero */
    if (font->head->unitsPerEm == 0) {
        return fontCorruptErr;
    }

    /* Calculate scale factor from font units to points */
    scale = ((long)pointSize << 16) / font->head->unitsPerEm;

    /* Convert TrueType metrics */
    metrics->ascent = ((long)font->hhea->ascender * scale) >> 16;
    metrics->descent = ((long)(-font->hhea->descender) * scale) >> 16;
    metrics->leading = ((long)font->hhea->lineGap * scale) >> 16;
    metrics->lineHeight = metrics->ascent + metrics->descent + metrics->leading;
    metrics->widMax = ((long)font->hhea->advanceWidthMax * scale) >> 16;

    /* Additional metrics from head table */
    metrics->xHeight = ((long)(font->head->yMax * 2 / 3) * scale) >> 16; /* Approximate */
    metrics->capHeight = ((long)(font->head->yMax * 3 / 4) * scale) >> 16; /* Approximate */

    /* Calculate average character width */
    metrics->avgCharWidth = metrics->widMax / 2; /* Simplified */
    metrics->maxCharWidth = metrics->widMax;
    metrics->minCharWidth = metrics->widMax / 4; /* Simplified */

    return noErr;
}

static MetricsCacheEntry *FindMetricsInCache(short familyID, short size, short style)
{
    MetricsCacheEntry *entry = gMetricsCache;

    while (entry != NULL) {
        if (entry->familyID == familyID &&
            entry->size == size &&
            entry->style == style) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static OSErr AddMetricsToCache(short familyID, short size, short style, const FontMetrics *metrics)
{
    MetricsCacheEntry *entry;

    if (metrics == NULL) {
        return paramErr;
    }

    /* Check if cache is full */
    if (gMetricsCacheSize >= gMaxCacheEntries) {
        /* Remove oldest entry */
        MetricsCacheEntry *oldest = gMetricsCache;
        MetricsCacheEntry *prev = NULL;
        MetricsCacheEntry *current = gMetricsCache;
        unsigned long oldestTime = oldest->lastUsed;

        /* Find oldest entry */
        while (current != NULL) {
            if (current->lastUsed < oldestTime) {
                oldest = current;
                oldestTime = current->lastUsed;
            }
            current = current->next;
        }

        /* Remove oldest entry from list */
        current = gMetricsCache;
        while (current != NULL) {
            if (current == oldest) {
                if (prev != NULL) {
                    prev->next = current->next;
                } else {
                    gMetricsCache = current->next;
                }
                DisposePtr((Ptr)current);
                gMetricsCacheSize--;
                break;
            }
            prev = current;
            current = current->next;
        }
    }

    /* Allocate new entry */
    entry = (MetricsCacheEntry *)NewPtr(sizeof(MetricsCacheEntry));
    if (entry == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Initialize entry */
    entry->familyID = familyID;
    entry->size = size;
    entry->style = style;
    entry->metrics = *metrics;
    entry->lastUsed = GetTickCount();
    entry->next = gMetricsCache;

    /* Add to cache */
    gMetricsCache = entry;
    gMetricsCacheSize++;

    return noErr;
}

static Fixed MeasureCharacterWidth(short familyID, short size, short style, char character)
{
    BitmapFontData *bitmapFont;
    TTFont *ttFont;
    OSErr error;
    Fixed width = 0;

    /* Try TrueType font first */
    error = LoadTrueTypeFont(familyID, &ttFont);
    if (error == noErr && ttFont != NULL) {
        unsigned short glyphIndex;
        short advanceWidth, leftSideBearing;

        error = MapCharacterToGlyph(ttFont, (unsigned short)character, &glyphIndex);
        if (error == noErr) {
            error = GetGlyphMetrics(ttFont, glyphIndex, &advanceWidth, &leftSideBearing);
            if (error == noErr && ttFont->head != NULL && ttFont->head->unitsPerEm != 0) {
                Fixed scale = ((long)size << 16) / ttFont->head->unitsPerEm;
                width = ((long)advanceWidth * scale) >> 16;
            }
        }
        UnloadTrueTypeFont(ttFont);

        if (width > 0) {
            return width;
        }
    }

    /* Try bitmap font */
    error = LoadBitmapFont(familyID, &bitmapFont);
    if (error == noErr && bitmapFont != NULL) {
        short charWidth = GetCharacterWidth(bitmapFont, character);
        width = (long)charWidth << 16;
        UnloadBitmapFont(bitmapFont);
    }

    return width;
}

static unsigned long GetTickCount(void)
{
    return TickCount();
}

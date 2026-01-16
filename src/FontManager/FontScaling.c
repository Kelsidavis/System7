/*
 * FontScaling.c - Font Size Scaling Implementation
 *
 * Provides bitmap font scaling for multiple point sizes
 * Uses nearest-neighbor scaling for System 7.1 compatibility
 */

#include "FontManager/FontManager.h"
#include "FontManager/FontTypes.h"
#include "FontManager/FontScaling.h"
/* FontInternal.h not included - this file implements simple wrapper API */
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"
#include "MemoryMgr/MemoryManager.h"
#include "chicago_font.h"
#include <string.h>
#include "FontManager/FontLogging.h"

/* Ensure boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Debug logging */
#define FSC_DEBUG 1

#if FSC_DEBUG
#define FSC_LOG(...) FONT_LOG_DEBUG("FSC: " __VA_ARGS__)
#else
#define FSC_LOG(...)
#endif

/* External dependencies */
extern GrafPtr g_currentPort;

/* Internal Font Manager drawing function */
extern void FM_DrawChicagoCharInternal(short x, short y, char ch, uint32_t color);

/* Standard Mac font sizes (in points) */
static const short g_standardSizes[] = {9, 10, 12, 14, 18, 24};
#define NUM_STANDARD_SIZES 6

/* Font scaling cache entry */
typedef struct ScaledFont {
    short           originalSize;   /* Source font size */
    short           scaledSize;     /* Target font size */
    Style           face;           /* Font style */
    short           scaleFactor;    /* Scaling ratio (fixed-point, /256) */
    struct ScaledFont* next;       /* Next in cache */
} ScaledFont;

/* Global scaled font cache */
static ScaledFont* g_scaledFontCache = NULL;

/* ============================================================================
 * Scaling Factor Calculation
 * ============================================================================ */

/*
 * FM_CalculateScaleFactor - Determine scale factor between sizes
 * Returns scale factor as fixed-point integer (multiplied by 256)
 * For example: scale of 1.0 returns 256, scale of 2.0 returns 512
 */
static short FM_CalculateScaleFactor(short fromSize, short toSize) {
    if (fromSize <= 0) fromSize = 12;  /* Default to Chicago 12 */
    if (toSize <= 0) toSize = 12;

    /* Calculate scale as (toSize / fromSize) * 256 using integer math */
    return (short)((toSize * 256) / fromSize);
}

/*
 * FM_FindNearestStandardSize - Find closest available font size
 */
short FM_FindNearestStandardSize(short requestedSize) {
    short bestSize = 12;  /* Default to Chicago 12 */
    short bestDiff = 1000;

    for (int i = 0; i < NUM_STANDARD_SIZES; i++) {
        short diff = (requestedSize > g_standardSizes[i]) ?
                    (requestedSize - g_standardSizes[i]) :
                    (g_standardSizes[i] - requestedSize);

        if (diff < bestDiff) {
            bestDiff = diff;
            bestSize = g_standardSizes[i];
        }
    }

    FSC_LOG("FindNearestStandardSize: %d -> %d (diff=%d)\n",
            requestedSize, bestSize, bestDiff);

    return bestSize;
}

/* ============================================================================
 * Bitmap Scaling Algorithms
 * ============================================================================ */

/*
 * FM_ScaleCharNearestNeighbor - Scale character using nearest-neighbor
 * This maintains pixel-perfect appearance at integer scale factors
 */
static void FM_ScaleCharNearestNeighbor(short srcX, short srcY, char ch,
                                        short scale, uint32_t color) {
    if (ch < 32 || ch > 126) return;

    ChicagoCharInfo info = chicago_ascii[ch - 32];
    short srcWidth = info.bit_width;
    short srcHeight = CHICAGO_HEIGHT;

    /* Calculate scaled dimensions using fixed-point scale (scale/256) */
    short dstWidth = (short)((srcWidth * scale + 128) / 256);
    short dstHeight = (short)((srcHeight * scale + 128) / 256);

    FSC_LOG("ScaleChar '%c': %dx%d -> %dx%d (scale=%d/256)\n",
            ch, srcWidth, srcHeight, dstWidth, dstHeight, scale);

    /* For now, use simple scaling approximation */
    /* Real implementation would sample bitmap pixels */

    if (scale > 256) {
        /* Scaling up - draw multiple pixels for each source pixel */
        int scaleInt = (scale + 128) / 256;
        for (int rep = 0; rep < scaleInt; rep++) {
            FM_DrawChicagoCharInternal(srcX + rep, srcY, ch, color);
        }
    } else {
        /* scale <= 256: scaling down or no scaling - draw once */
        FM_DrawChicagoCharInternal(srcX, srcY, ch, color);
    }
}

/*
 * FM_ScaleCharBilinear - Scale character using bilinear interpolation
 * Provides smoother scaling for non-integer factors
 */
__attribute__((unused))
static void FM_ScaleCharBilinear(short srcX, short srcY, char ch,
                                 short scale, uint32_t color) {
    /* For System 7.1 compatibility, fall back to nearest-neighbor */
    /* Bilinear would require anti-aliasing not available in 1992 */
    FM_ScaleCharNearestNeighbor(srcX, srcY, ch, scale, color);
}

/* ============================================================================
 * Scaled Width Calculation
 * ============================================================================ */

/*
 * FM_GetScaledCharWidth - Calculate character width at scaled size
 */
short FM_GetScaledCharWidth(SInt16 fontNum, SInt16 fontSize, UInt8 ch) {
    /* Currently only supports Chicago (fontNum is ignored) */
    (void)fontNum;

    if (ch < 32 || ch > 126) return 0;

    ChicagoCharInfo info = chicago_ascii[ch - 32];
    short baseWidth = info.bit_width + 2;  /* Include spacing */
    if (ch == ' ') baseWidth += 3;  /* Extra space width */

    /* Calculate scaling from Chicago 12 using fixed-point math */
    short scale = FM_CalculateScaleFactor(12, fontSize);
    /* Apply scale (divide by 256 to convert from fixed-point, add 128 for rounding) */
    short scaledWidth = (short)((baseWidth * scale + 128) / 256);

    FSC_LOG("GetScaledCharWidth '%c': base=%d, scale=%d, scaled=%d\n",
            ch, baseWidth, scale, scaledWidth);

    return scaledWidth;
}

/*
 * FM_GetScaledStringWidth - Calculate string width at scaled size
 */
short FM_GetScaledStringWidth(ConstStr255Param s, short targetSize) {
    if (!s || s[0] == 0) return 0;

    short totalWidth = 0;
    for (int i = 1; i <= s[0]; i++) {
        totalWidth += FM_GetScaledCharWidth(0, targetSize, s[i]);  /* fontNum=0 for Chicago */
    }

    FSC_LOG("GetScaledStringWidth: \"%.*s\" at %dpt = %d pixels\n",
            s[0], &s[1], targetSize, totalWidth);

    return totalWidth;
}

/* ============================================================================
 * Font Size Synthesis
 * ============================================================================ */

/*
 * FM_SynthesizeSize - Create font at specific size
 */
void FM_SynthesizeSize(short x, short y, char ch, short targetSize, uint32_t color) {
    /* Find best available size */
    short baseSize = FM_FindNearestStandardSize(targetSize);

    /* Calculate scale factor */
    short scale = FM_CalculateScaleFactor(baseSize, targetSize);

    FSC_LOG("SynthesizeSize '%c': target=%dpt, base=%dpt, scale=%d/256\n",
            ch, targetSize, baseSize, scale);

    /* Apply scaling */
    if (baseSize == 12 && targetSize == 12) {
        /* No scaling needed for Chicago 12 */
        FM_DrawChicagoCharInternal(x, y, ch, color);
    } else {
        /* Scale from base size */
        FM_ScaleCharNearestNeighbor(x, y, ch, scale, color);
    }
}

/*
 * FM_DrawScaledString - Draw string at specific size
 */
void FM_DrawScaledString(ConstStr255Param s, short targetSize) {
    if (!s || s[0] == 0) return;

    Point pen = g_currentPort->pnLoc;
    uint32_t color = 0xFF000000; /* Black */

    FSC_LOG("DrawScaledString: \"%.*s\" at %dpt\n", s[0], &s[1], targetSize);

    /* Draw each character scaled */
    for (int i = 1; i <= s[0]; i++) {
        char ch = s[i];

        /* Draw scaled character */
        FM_SynthesizeSize(pen.h, pen.v, ch, targetSize, color);

        /* Advance pen by scaled width */
        pen.h += FM_GetScaledCharWidth(0, targetSize, ch);
    }

    /* Update pen location */
    g_currentPort->pnLoc = pen;
}

/* ============================================================================
 * Font Metrics at Different Sizes
 * ============================================================================ */

/*
 * FM_GetScaledMetrics - Calculate font metrics for scaled size
 */
void FM_GetScaledMetrics(short targetSize, FMetricRec* metrics) {
    if (!metrics) return;

    /* Base Chicago 12 metrics */
    short baseAscent = CHICAGO_ASCENT;
    short baseDescent = CHICAGO_DESCENT;
    short baseLeading = CHICAGO_LEADING;
    short baseWidMax = 16;

    /* Calculate scale factor */
    short scale = FM_CalculateScaleFactor(12, targetSize);

    /* Scale metrics using fixed-point math */
    metrics->ascent = (SInt32)((baseAscent * scale + 128) / 256);
    metrics->descent = (SInt32)((baseDescent * scale + 128) / 256);
    metrics->leading = (SInt32)((baseLeading * scale + 128) / 256);
    metrics->widMax = (SInt32)((baseWidMax * scale + 128) / 256);

    FSC_LOG("GetScaledMetrics %dpt: ascent=%d, descent=%d, leading=%d, widMax=%d\n",
            targetSize, metrics->ascent, metrics->descent,
            metrics->leading, metrics->widMax);
}

/* ============================================================================
 * Size Availability Checking
 * ============================================================================ */

/*
 * FM_IsSizeAvailable - Check if font size is directly available
 */
Boolean FM_IsSizeAvailable(short fontID, short size) {
    /* Currently only Chicago 12 is directly available */
    if (fontID == chicagoFont && size == 12) {
        return TRUE;
    }

    /* Check standard sizes that can be synthesized */
    for (int i = 0; i < NUM_STANDARD_SIZES; i++) {
        if (size == g_standardSizes[i]) {
            FSC_LOG("IsSizeAvailable: %dpt is standard\n", size);
            return TRUE;  /* Can synthesize standard sizes */
        }
    }

    FSC_LOG("IsSizeAvailable: %dpt will be scaled\n", size);
    return FALSE;  /* Will need scaling */
}

/*
 * FM_GetAvailableSizes - Get list of available sizes for font
 */
short FM_GetAvailableSizes(short fontID, short* sizes, short maxSizes) {
    if (!sizes || maxSizes <= 0) return 0;

    short count = 0;

    /* Add all standard sizes */
    for (int i = 0; i < NUM_STANDARD_SIZES && count < maxSizes; i++) {
        sizes[count++] = g_standardSizes[i];
    }

    FSC_LOG("GetAvailableSizes: font %d has %d sizes\n", fontID, count);

    return count;
}

/* ============================================================================
 * Cache Management
 * ============================================================================ */

/*
 * FM_CacheScaledFont - Add scaled font to cache
 */
__attribute__((unused))
static void FM_CacheScaledFont(short originalSize, short scaledSize,
                              Style face, short scaleFactor) {
    ScaledFont* entry = (ScaledFont*)NewPtr(sizeof(ScaledFont));
    if (!entry) return;

    entry->originalSize = originalSize;
    entry->scaledSize = scaledSize;
    entry->face = face;
    entry->scaleFactor = scaleFactor;
    entry->next = g_scaledFontCache;

    g_scaledFontCache = entry;

    FSC_LOG("CacheScaledFont: %dpt->%dpt, scale=%d/256, face=0x%02X\n",
            originalSize, scaledSize, scaleFactor, face);
}

/*
 * FM_FindCachedScale - Look up cached scale factor
 */
__attribute__((unused))
static Boolean FM_FindCachedScale(short originalSize, short scaledSize,
                                 Style face, short* scaleFactor) {
    ScaledFont* entry = g_scaledFontCache;

    while (entry) {
        if (entry->originalSize == originalSize &&
            entry->scaledSize == scaledSize &&
            entry->face == face) {
            *scaleFactor = entry->scaleFactor;
            FSC_LOG("FindCachedScale: found %dpt->%dpt = %d/256\n",
                    originalSize, scaledSize, *scaleFactor);
            return TRUE;
        }
        entry = entry->next;
    }

    return FALSE;
}

/*
 * FM_FlushScaleCache - Clear scaled font cache
 */
void FM_FlushScaleCache(void) {
    ScaledFont* entry = g_scaledFontCache;

    while (entry) {
        ScaledFont* next = entry->next;
        DisposePtr((Ptr)entry);
        entry = next;
    }

    g_scaledFontCache = NULL;
    FSC_LOG("FlushScaleCache: cache cleared\n");
}

/* ============================================================================
 * Integration with Font Manager
 * ============================================================================ */

/*
 * FM_SelectBestSize - Choose best size for requested font
 */
short FM_SelectBestSize(short fontID, short requestedSize, Boolean allowScaling) {
    /* Check if exact size is available */
    if (FM_IsSizeAvailable(fontID, requestedSize)) {
        return requestedSize;
    }

    if (!allowScaling) {
        /* No scaling allowed - return nearest available */
        return FM_FindNearestStandardSize(requestedSize);
    }

    /* Allow scaling - return requested size */
    return requestedSize;
}

/*
 * FM_DrawTextAtSize - Main entry point for sized text drawing
 */
void FM_DrawTextAtSize(const void* textBuf, short firstByte, short byteCount,
                       short targetSize) {
    if (!textBuf || byteCount <= 0) return;

    const char* text = (const char*)textBuf;
    Point pen = g_currentPort->pnLoc;
    uint32_t color = 0xFF000000;

    FSC_LOG("DrawTextAtSize: %d bytes at %dpt\n", byteCount, targetSize);

    /* Draw each character at target size */
    for (short i = 0; i < byteCount; i++) {
        char ch = text[firstByte + i];

        /* Draw scaled character */
        FM_SynthesizeSize(pen.h, pen.v, ch, targetSize, color);

        /* Advance pen */
        pen.h += FM_GetScaledCharWidth(0, targetSize, ch);
    }

    /* Update pen location */
    g_currentPort->pnLoc = pen;
}
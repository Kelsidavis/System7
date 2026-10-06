/*
 * FontScaling.c - Font Size Scaling Implementation
 *
 * Provides bitmap font scaling for multiple point sizes
 * Uses nearest-neighbor scaling for System 7.1 compatibility
 */

#include "FontManager/FontManager.h"
#include "FontManager/FontInternal.h"
#include "FontManager/FontTypes.h"
#include "FontManager/FontScaling.h"
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"
#include "FontManager/FontLogging.h"

/* Ensure boolean constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Keep per-character diagnostics disabled during normal rendering. */
#define FSC_DEBUG 0

#if FSC_DEBUG
#define FSC_LOG(...) FONT_LOG_DEBUG("FSC: " __VA_ARGS__)
#else
#define FSC_LOG(...)
#endif

/* Standard Mac font sizes (in points) */
static const short g_standardSizes[] = {9, 10, 12, 14, 18, 24};
#define NUM_STANDARD_SIZES 6

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
 * Scaled Width Calculation
 * ============================================================================ */

/*
 * FM_GetScaledCharWidth - Calculate character width at scaled size
 */
short FM_GetScaledCharWidth(SInt16 fontNum, SInt16 fontSize, UInt8 ch) {
    /* Currently only supports Chicago (fontNum is ignored) */
    (void)fontNum;

    return FM_GetChicagoCharWidthAtSize(ch, fontSize);
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
void FM_SynthesizeSize(short x, short y, unsigned char ch, short targetSize, uint32_t color) {
    FM_DrawChicagoCharAtSize(x, y, ch, targetSize, color);
}

/*
 * FM_DrawScaledString - Draw string at specific size
 */
void FM_DrawScaledString(ConstStr255Param s, short targetSize) {
    if (!s || !g_currentPort) return;
    short savedSize = g_currentPort->txSize;
    TextSize(targetSize);
    DrawString(s);
    TextSize(savedSize);
}

/* ============================================================================
 * Font Metrics at Different Sizes
 * ============================================================================ */

/*
 * FM_GetScaledMetrics - Calculate font metrics for scaled size
 */
void FM_GetScaledMetrics(short targetSize, FMetricRec* metrics) {
    FM_GetChicagoMetricsAtSize(targetSize, metrics);
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
    (void)fontID;
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
    if (!textBuf || byteCount <= 0 || !g_currentPort) return;
    short savedSize = g_currentPort->txSize;
    TextSize(targetSize);
    DrawText(textBuf, firstByte, byteCount);
    TextSize(savedSize);
}

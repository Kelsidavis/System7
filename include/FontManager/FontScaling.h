/*
 * FontScaling.h - Font Size Scaling API
 *
 * Bitmap font scaling for multiple point sizes
 */

#ifndef FONT_SCALING_H
#define FONT_SCALING_H

#include "SystemTypes.h"
#include "FontTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Font Scaling API - Simple wrapper functions */
/* NOTE: FontInternal.h declares the canonical multi-font API (not yet implemented) */
/* These are interim single-font wrappers that use current graphics port */

/* Size Selection */
short FM_FindNearestStandardSize(short requestedSize);
short FM_SelectBestSize(short fontID, short requestedSize, Boolean allowScaling);
Boolean FM_IsSizeAvailable(short fontID, short size);
short FM_GetAvailableSizes(short fontID, short* sizes, short maxSizes);

/* Character Scaling */
void FM_SynthesizeSize(short x, short y, unsigned char ch, short targetSize, uint32_t color);

/* Width Calculations */
short FM_GetScaledCharWidth(SInt16 fontNum, SInt16 fontSize, UInt8 ch);
short FM_GetScaledStringWidth(ConstStr255Param s, short targetSize);

/* String Drawing */
void FM_DrawScaledString(ConstStr255Param s, short targetSize);
void FM_DrawTextAtSize(const void* textBuf, short firstByte, short byteCount,
                       short targetSize);

/* Metrics */
void FM_GetScaledMetrics(short targetSize, FMetricRec* metrics);

/* Cache Management */
void FM_FlushScaleCache(void);

/* Standard Mac font sizes */
#define FM_SIZE_9PT     9
#define FM_SIZE_10PT   10
#define FM_SIZE_12PT   12   /* Chicago default */
#define FM_SIZE_14PT   14
#define FM_SIZE_18PT   18
#define FM_SIZE_24PT   24

#ifdef __cplusplus
}
#endif

#endif /* FONT_SCALING_H */
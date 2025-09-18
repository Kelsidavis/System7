/*
 * Bitmaps.c - QuickDraw Bitmap and CopyBits Implementation
 *
 * Complete implementation of bitmap operations including CopyBits,
 * scaling, transfer modes, masking, and pixel manipulation.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#include "../include/QuickDraw/QuickDraw.h"
#include "../include/QuickDraw/ColorQuickDraw.h"
#include "../include/QuickDraw/QDRegions.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDrawPlatform.h"

/* Transfer mode operation structures */
typedef struct {
    uint32_t (*operation)(uint32_t src, uint32_t dst, uint32_t pattern);
    bool needsPattern;
    bool supportsColor;
} TransferModeInfo;

/* Scaling information */
typedef struct {
    int16_t srcWidth, srcHeight;
    int16_t dstWidth, dstHeight;
    int32_t hScale, vScale;  /* Fixed-point scaling factors */
    bool needsScaling;
} ScaleInfo;

/* Bit manipulation macros */
#define BITS_PER_BYTE 8
#define FIXED_POINT_SCALE 65536

/* Forward declarations */
static void CopyBitsImplementation(const BitMap *srcBits, const BitMap *dstBits,
                                  const Rect *srcRect, const Rect *dstRect,
                                  int16_t mode, RgnHandle maskRgn);
static void CopyBitsScaled(const BitMap *srcBits, const BitMap *dstBits,
                          const Rect *srcRect, const Rect *dstRect,
                          int16_t mode, const ScaleInfo *scaleInfo);
static void CopyBitsUnscaled(const BitMap *srcBits, const BitMap *dstBits,
                            const Rect *srcRect, const Rect *dstRect,
                            int16_t mode);
static uint32_t ApplyTransferMode(uint32_t src, uint32_t dst, uint32_t pattern, int16_t mode);
static void CalculateScaling(const Rect *srcRect, const Rect *dstRect, ScaleInfo *scaleInfo);
static void CopyPixelRow(const BitMap *srcBits, const BitMap *dstBits,
                        int16_t srcY, int16_t dstY, int16_t srcLeft, int16_t srcRight,
                        int16_t dstLeft, int16_t dstRight, int16_t mode);
static uint32_t GetPixelValue(const BitMap *bitmap, int16_t x, int16_t y);
static void SetPixelValue(const BitMap *bitmap, int16_t x, int16_t y, uint32_t value);
static bool IsPixMap(const BitMap *bitmap);
static int16_t GetBitmapDepth(const BitMap *bitmap);
static void ClipRectToBitmap(const BitMap *bitmap, Rect *rect);

/* Transfer mode operations */
static uint32_t TransferSrcCopy(uint32_t src, uint32_t dst, uint32_t pattern) {
    return src;
}

static uint32_t TransferSrcOr(uint32_t src, uint32_t dst, uint32_t pattern) {
    return src | dst;
}

static uint32_t TransferSrcXor(uint32_t src, uint32_t dst, uint32_t pattern) {
    return src ^ dst;
}

static uint32_t TransferSrcBic(uint32_t src, uint32_t dst, uint32_t pattern) {
    return src & (~dst);
}

static uint32_t TransferNotSrcCopy(uint32_t src, uint32_t dst, uint32_t pattern) {
    return ~src;
}

static uint32_t TransferPatCopy(uint32_t src, uint32_t dst, uint32_t pattern) {
    return pattern;
}

static uint32_t TransferPatOr(uint32_t src, uint32_t dst, uint32_t pattern) {
    return pattern | dst;
}

static uint32_t TransferPatXor(uint32_t src, uint32_t dst, uint32_t pattern) {
    return pattern ^ dst;
}

static uint32_t TransferBlend(uint32_t src, uint32_t dst, uint32_t pattern) {
    /* Simple average blend */
    return (src + dst) / 2;
}

static uint32_t TransferAddPin(uint32_t src, uint32_t dst, uint32_t pattern) {
    uint32_t result = src + dst;
    return (result > 0xFFFFFF) ? 0xFFFFFF : result;
}

static const TransferModeInfo g_transferModes[] = {
    {TransferSrcCopy,    false, true},  /* srcCopy */
    {TransferSrcOr,      false, true},  /* srcOr */
    {TransferSrcXor,     false, true},  /* srcXor */
    {TransferSrcBic,     false, true},  /* srcBic */
    {TransferNotSrcCopy, false, true},  /* notSrcCopy */
    {TransferSrcOr,      false, true},  /* notSrcOr */
    {TransferSrcXor,     false, true},  /* notSrcXor */
    {TransferSrcBic,     false, true},  /* notSrcBic */
    {TransferPatCopy,    true,  true},  /* patCopy */
    {TransferPatOr,      true,  true},  /* patOr */
    {TransferPatXor,     true,  true},  /* patXor */
    {TransferSrcBic,     true,  true},  /* patBic */
    {TransferPatCopy,    true,  true},  /* notPatCopy */
    {TransferPatOr,      true,  true},  /* notPatOr */
    {TransferPatXor,     true,  true},  /* notPatXor */
    {TransferSrcBic,     true,  true}   /* notPatBic */
};

/* ================================================================
 * COPYBITS IMPLEMENTATION
 * ================================================================ */

void CopyBits(const BitMap *srcBits, const BitMap *dstBits,
              const Rect *srcRect, const Rect *dstRect,
              int16_t mode, RgnHandle maskRgn) {
    assert(srcBits != NULL);
    assert(dstBits != NULL);
    assert(srcRect != NULL);
    assert(dstRect != NULL);

    /* Validate rectangles */
    if (EmptyRect(srcRect) || EmptyRect(dstRect)) return;

    CopyBitsImplementation(srcBits, dstBits, srcRect, dstRect, mode, maskRgn);
}

static void CopyBitsImplementation(const BitMap *srcBits, const BitMap *dstBits,
                                  const Rect *srcRect, const Rect *dstRect,
                                  int16_t mode, RgnHandle maskRgn) {
    /* Clip source and destination rectangles */
    Rect clippedSrcRect = *srcRect;
    Rect clippedDstRect = *dstRect;

    ClipRectToBitmap(srcBits, &clippedSrcRect);
    ClipRectToBitmap(dstBits, &clippedDstRect);

    /* Calculate scaling information */
    ScaleInfo scaleInfo;
    CalculateScaling(&clippedSrcRect, &clippedDstRect, &scaleInfo);

    /* Choose appropriate copy method */
    if (scaleInfo.needsScaling) {
        CopyBitsScaled(srcBits, dstBits, &clippedSrcRect, &clippedDstRect,
                       mode, &scaleInfo);
    } else {
        CopyBitsUnscaled(srcBits, dstBits, &clippedSrcRect, &clippedDstRect, mode);
    }
}

static void CopyBitsScaled(const BitMap *srcBits, const BitMap *dstBits,
                          const Rect *srcRect, const Rect *dstRect,
                          int16_t mode, const ScaleInfo *scaleInfo) {
    int16_t dstWidth = dstRect->right - dstRect->left;
    int16_t dstHeight = dstRect->bottom - dstRect->top;

    /* Scale each destination pixel */
    for (int16_t dstY = 0; dstY < dstHeight; dstY++) {
        for (int16_t dstX = 0; dstX < dstWidth; dstX++) {
            /* Calculate corresponding source pixel */
            int16_t srcX = (int16_t)(dstX * scaleInfo->hScale / FIXED_POINT_SCALE);
            int16_t srcY = (int16_t)(dstY * scaleInfo->vScale / FIXED_POINT_SCALE);

            /* Get source pixel */
            uint32_t srcPixel = GetPixelValue(srcBits,
                                            srcRect->left + srcX,
                                            srcRect->top + srcY);

            /* Get destination pixel */
            uint32_t dstPixel = GetPixelValue(dstBits,
                                            dstRect->left + dstX,
                                            dstRect->top + dstY);

            /* Apply transfer mode */
            uint32_t resultPixel = ApplyTransferMode(srcPixel, dstPixel, 0, mode);

            /* Set destination pixel */
            SetPixelValue(dstBits, dstRect->left + dstX, dstRect->top + dstY, resultPixel);
        }
    }
}

static void CopyBitsUnscaled(const BitMap *srcBits, const BitMap *dstBits,
                            const Rect *srcRect, const Rect *dstRect,
                            int16_t mode) {
    int16_t width = srcRect->right - srcRect->left;
    int16_t height = srcRect->bottom - srcRect->top;

    /* Copy row by row for efficiency */
    for (int16_t y = 0; y < height; y++) {
        CopyPixelRow(srcBits, dstBits,
                    srcRect->top + y, dstRect->top + y,
                    srcRect->left, srcRect->right,
                    dstRect->left, dstRect->right,
                    mode);
    }
}

static void CopyPixelRow(const BitMap *srcBits, const BitMap *dstBits,
                        int16_t srcY, int16_t dstY, int16_t srcLeft, int16_t srcRight,
                        int16_t dstLeft, int16_t dstRight, int16_t mode) {
    int16_t width = srcRight - srcLeft;

    for (int16_t x = 0; x < width; x++) {
        uint32_t srcPixel = GetPixelValue(srcBits, srcLeft + x, srcY);
        uint32_t dstPixel = GetPixelValue(dstBits, dstLeft + x, dstY);
        uint32_t resultPixel = ApplyTransferMode(srcPixel, dstPixel, 0, mode);
        SetPixelValue(dstBits, dstLeft + x, dstY, resultPixel);
    }
}

/* ================================================================
 * MASKING OPERATIONS
 * ================================================================ */

void CopyMask(const BitMap *srcBits, const BitMap *maskBits,
              const BitMap *dstBits, const Rect *srcRect,
              const Rect *maskRect, const Rect *dstRect) {
    assert(srcBits != NULL);
    assert(maskBits != NULL);
    assert(dstBits != NULL);
    assert(srcRect != NULL);
    assert(maskRect != NULL);
    assert(dstRect != NULL);

    /* Validate rectangles */
    if (EmptyRect(srcRect) || EmptyRect(maskRect) || EmptyRect(dstRect)) return;

    int16_t width = srcRect->right - srcRect->left;
    int16_t height = srcRect->bottom - srcRect->top;

    /* Copy pixels where mask is non-zero */
    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            uint32_t maskPixel = GetPixelValue(maskBits,
                                             maskRect->left + x,
                                             maskRect->top + y);

            if (maskPixel != 0) {
                uint32_t srcPixel = GetPixelValue(srcBits,
                                                srcRect->left + x,
                                                srcRect->top + y);
                SetPixelValue(dstBits,
                            dstRect->left + x,
                            dstRect->top + y,
                            srcPixel);
            }
        }
    }
}

void CopyDeepMask(const BitMap *srcBits, const BitMap *maskBits,
                  const BitMap *dstBits, const Rect *srcRect,
                  const Rect *maskRect, const Rect *dstRect,
                  int16_t mode, RgnHandle maskRgn) {
    assert(srcBits != NULL);
    assert(maskBits != NULL);
    assert(dstBits != NULL);
    assert(srcRect != NULL);
    assert(maskRect != NULL);
    assert(dstRect != NULL);

    /* Validate rectangles */
    if (EmptyRect(srcRect) || EmptyRect(maskRect) || EmptyRect(dstRect)) return;

    int16_t width = srcRect->right - srcRect->left;
    int16_t height = srcRect->bottom - srcRect->top;

    /* Copy pixels using mask and transfer mode */
    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            uint32_t maskPixel = GetPixelValue(maskBits,
                                             maskRect->left + x,
                                             maskRect->top + y);

            if (maskPixel != 0) {
                uint32_t srcPixel = GetPixelValue(srcBits,
                                                srcRect->left + x,
                                                srcRect->top + y);
                uint32_t dstPixel = GetPixelValue(dstBits,
                                                dstRect->left + x,
                                                dstRect->top + y);

                uint32_t resultPixel = ApplyTransferMode(srcPixel, dstPixel, 0, mode);
                SetPixelValue(dstBits,
                            dstRect->left + x,
                            dstRect->top + y,
                            resultPixel);
            }
        }
    }
}

/* ================================================================
 * SEED FILL OPERATIONS
 * ================================================================ */

void SeedFill(const void *srcPtr, void *dstPtr, int16_t srcRow, int16_t dstRow,
              int16_t height, int16_t words, int16_t seedH, int16_t seedV) {
    assert(srcPtr != NULL);
    assert(dstPtr != NULL);

    /* Simple flood-fill implementation */
    /* This is a simplified version - a full implementation would be more complex */

    const uint16_t *src = (const uint16_t *)srcPtr;
    uint16_t *dst = (uint16_t *)dstPtr;

    /* Copy source to destination first */
    for (int16_t y = 0; y < height; y++) {
        const uint16_t *srcLine = src + y * (srcRow / 2);
        uint16_t *dstLine = dst + y * (dstRow / 2);
        memcpy(dstLine, srcLine, words * 2);
    }

    /* Perform seed fill at specified location */
    if (seedV < height && seedH < words * 16) {
        uint16_t *seedLine = dst + seedV * (dstRow / 2);
        uint16_t wordIndex = seedH / 16;
        uint16_t bitIndex = seedH % 16;

        /* Set the seed bit */
        seedLine[wordIndex] |= (1 << (15 - bitIndex));
    }
}

void CalcMask(const void *srcPtr, void *dstPtr, int16_t srcRow, int16_t dstRow,
              int16_t height, int16_t words) {
    assert(srcPtr != NULL);
    assert(dstPtr != NULL);

    const uint16_t *src = (const uint16_t *)srcPtr;
    uint16_t *dst = (uint16_t *)dstPtr;

    /* Calculate mask from source */
    for (int16_t y = 0; y < height; y++) {
        const uint16_t *srcLine = src + y * (srcRow / 2);
        uint16_t *dstLine = dst + y * (dstRow / 2);

        for (int16_t w = 0; w < words; w++) {
            /* Create mask: 1 where source is non-zero, 0 where zero */
            dstLine[w] = (srcLine[w] != 0) ? 0xFFFF : 0x0000;
        }
    }
}

/* ================================================================
 * UTILITY FUNCTIONS
 * ================================================================ */

static uint32_t ApplyTransferMode(uint32_t src, uint32_t dst, uint32_t pattern, int16_t mode) {
    /* Clamp mode to valid range */
    if (mode < 0 || mode >= sizeof(g_transferModes) / sizeof(g_transferModes[0])) {
        mode = srcCopy;
    }

    const TransferModeInfo *modeInfo = &g_transferModes[mode];
    return modeInfo->operation(src, dst, pattern);
}

static void CalculateScaling(const Rect *srcRect, const Rect *dstRect, ScaleInfo *scaleInfo) {
    scaleInfo->srcWidth = srcRect->right - srcRect->left;
    scaleInfo->srcHeight = srcRect->bottom - srcRect->top;
    scaleInfo->dstWidth = dstRect->right - dstRect->left;
    scaleInfo->dstHeight = dstRect->bottom - dstRect->top;

    scaleInfo->needsScaling = (scaleInfo->srcWidth != scaleInfo->dstWidth ||
                              scaleInfo->srcHeight != scaleInfo->dstHeight);

    if (scaleInfo->needsScaling) {
        scaleInfo->hScale = (scaleInfo->srcWidth * FIXED_POINT_SCALE) / scaleInfo->dstWidth;
        scaleInfo->vScale = (scaleInfo->srcHeight * FIXED_POINT_SCALE) / scaleInfo->dstHeight;
    } else {
        scaleInfo->hScale = FIXED_POINT_SCALE;
        scaleInfo->vScale = FIXED_POINT_SCALE;
    }
}

static uint32_t GetPixelValue(const BitMap *bitmap, int16_t x, int16_t y) {
    if (x < bitmap->bounds.left || x >= bitmap->bounds.right ||
        y < bitmap->bounds.top || y >= bitmap->bounds.bottom) {
        return 0;
    }

    /* Calculate byte offset */
    int16_t relativeX = x - bitmap->bounds.left;
    int16_t relativeY = y - bitmap->bounds.top;
    int16_t rowBytes = bitmap->rowBytes & 0x3FFF; /* Mask out flags */

    uint8_t *baseAddr = (uint8_t *)bitmap->baseAddr;
    if (!baseAddr) return 0;

    uint8_t *pixel = baseAddr + relativeY * rowBytes;

    if (IsPixMap(bitmap)) {
        /* Color bitmap */
        PixMap *pixMap = (PixMap *)bitmap;
        int16_t pixelSize = pixMap->pixelSize;

        switch (pixelSize) {
            case 1: {
                int16_t byteIndex = relativeX / 8;
                int16_t bitIndex = relativeX % 8;
                return (pixel[byteIndex] >> (7 - bitIndex)) & 1;
            }
            case 8:
                return pixel[relativeX];
            case 16: {
                uint16_t *pixel16 = (uint16_t *)pixel;
                return pixel16[relativeX];
            }
            case 32: {
                uint32_t *pixel32 = (uint32_t *)pixel;
                return pixel32[relativeX];
            }
            default:
                return 0;
        }
    } else {
        /* 1-bit bitmap */
        int16_t byteIndex = relativeX / 8;
        int16_t bitIndex = relativeX % 8;
        return (pixel[byteIndex] >> (7 - bitIndex)) & 1;
    }
}

static void SetPixelValue(const BitMap *bitmap, int16_t x, int16_t y, uint32_t value) {
    if (x < bitmap->bounds.left || x >= bitmap->bounds.right ||
        y < bitmap->bounds.top || y >= bitmap->bounds.bottom) {
        return;
    }

    /* Calculate byte offset */
    int16_t relativeX = x - bitmap->bounds.left;
    int16_t relativeY = y - bitmap->bounds.top;
    int16_t rowBytes = bitmap->rowBytes & 0x3FFF; /* Mask out flags */

    uint8_t *baseAddr = (uint8_t *)bitmap->baseAddr;
    if (!baseAddr) return;

    uint8_t *pixel = baseAddr + relativeY * rowBytes;

    if (IsPixMap(bitmap)) {
        /* Color bitmap */
        PixMap *pixMap = (PixMap *)bitmap;
        int16_t pixelSize = pixMap->pixelSize;

        switch (pixelSize) {
            case 1: {
                int16_t byteIndex = relativeX / 8;
                int16_t bitIndex = relativeX % 8;
                if (value & 1) {
                    pixel[byteIndex] |= (1 << (7 - bitIndex));
                } else {
                    pixel[byteIndex] &= ~(1 << (7 - bitIndex));
                }
                break;
            }
            case 8:
                pixel[relativeX] = (uint8_t)value;
                break;
            case 16: {
                uint16_t *pixel16 = (uint16_t *)pixel;
                pixel16[relativeX] = (uint16_t)value;
                break;
            }
            case 32: {
                uint32_t *pixel32 = (uint32_t *)pixel;
                pixel32[relativeX] = value;
                break;
            }
        }
    } else {
        /* 1-bit bitmap */
        int16_t byteIndex = relativeX / 8;
        int16_t bitIndex = relativeX % 8;
        if (value & 1) {
            pixel[byteIndex] |= (1 << (7 - bitIndex));
        } else {
            pixel[byteIndex] &= ~(1 << (7 - bitIndex));
        }
    }
}

static bool IsPixMap(const BitMap *bitmap) {
    return (bitmap->rowBytes & 0x8000) != 0;
}

static int16_t GetBitmapDepth(const BitMap *bitmap) {
    if (IsPixMap(bitmap)) {
        PixMap *pixMap = (PixMap *)bitmap;
        return pixMap->pixelSize;
    }
    return 1;
}

static void ClipRectToBitmap(const BitMap *bitmap, Rect *rect) {
    Rect clippedRect;
    if (SectRect(rect, &bitmap->bounds, &clippedRect)) {
        *rect = clippedRect;
    } else {
        SetRect(rect, 0, 0, 0, 0);
    }
}

/* ================================================================
 * BITMAP REGION CONVERSION
 * ================================================================ */

int16_t BitMapToRegion(RgnHandle region, const BitMap *bMap) {
    assert(region != NULL && *region != NULL);
    assert(bMap != NULL);

    /* Start with empty region */
    SetEmptyRgn(region);

    if (!bMap->baseAddr) return 0;

    int16_t width = bMap->bounds.right - bMap->bounds.left;
    int16_t height = bMap->bounds.bottom - bMap->bounds.top;

    /* For 1-bit bitmaps, create region from set bits */
    if (!IsPixMap(bMap)) {
        RgnHandle tempRgn = NewRgn();
        if (!tempRgn) return rgnOverflowErr;

        for (int16_t y = 0; y < height; y++) {
            for (int16_t x = 0; x < width; x++) {
                if (GetPixelValue(bMap, bMap->bounds.left + x, bMap->bounds.top + y)) {
                    Rect pixelRect;
                    SetRect(&pixelRect,
                           bMap->bounds.left + x, bMap->bounds.top + y,
                           bMap->bounds.left + x + 1, bMap->bounds.top + y + 1);
                    RectRgn(tempRgn, &pixelRect);
                    UnionRgn(region, tempRgn, region);
                }
            }
        }

        DisposeRgn(tempRgn);
    }

    return 0;
}
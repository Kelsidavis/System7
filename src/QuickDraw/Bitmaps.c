/* #include "SystemTypes.h" */
#include "QuickDraw/QuickDrawInternal.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>
/*
 * Bitmaps.c - QuickDraw Bitmap and CopyBits Implementation
 *
 * Complete implementation of bitmap operations including CopyBits,
 * scaling, transfer modes, masking, and pixel manipulation.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "QuickDrawConstants.h"
#include "MemoryMgr/MemoryManager.h"

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDraw/QDRegions.h"
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDraw/QuickDrawPlatform.h"

/* Current QuickDraw port from QuickDrawCore.c */
extern GrafPtr g_currentPort;
extern CGrafPtr g_currentCPort;
extern QDGlobals qd;
extern uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);

extern void serial_puts(const char* str);
extern void serial_putchar(char ch);

static void qd_log_hex_u32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

static void qd_log_memcpy(const char* tag, const void* src, const void* dst, size_t length) {
    serial_puts(tag);
    serial_puts(" src=0x");
    qd_log_hex_u32((uint32_t)(uintptr_t)src);
    serial_puts(" dst=0x");
    qd_log_hex_u32((uint32_t)(uintptr_t)dst);
    serial_puts(" len=0x");
    qd_log_hex_u32((uint32_t)length);
    serial_puts(" dst_end=0x");
    qd_log_hex_u32((uint32_t)((uintptr_t)dst + length));
    serial_putchar('\n');
}


static const UInt32 kColorMask = 0x00FFFFFF;

typedef struct {
    Boolean isPixMap;
    SInt16 pixelSize;
    const PixMap* pixMap;
    const ColorTable* colorTable;
} BitmapDescriptor;

/* Transfer mode operation structures */
typedef struct {
    UInt32 (*operation)(UInt32 src, UInt32 dst, UInt32 pattern);
    Boolean needsPattern;
    Boolean supportsColor;
} TransferModeInfo;

/* Scaling information */
typedef struct {
    SInt16 srcWidth, srcHeight;
    SInt16 dstWidth, dstHeight;
    SInt32 hScale, vScale;  /* Fixed-point scaling factors */
    Boolean needsScaling;
} ScaleInfo;

/* Bit manipulation macros */
#define BITS_PER_BYTE 8
#define FIXED_POINT_SCALE 65536

/* Forward declarations */
static void CopyBitsImplementation(const BitMap *srcBits, const BitMap *dstBits,
                                  const Rect *srcRect, const Rect *dstRect,
                                  SInt16 mode, RgnHandle maskRgn);
static void CopyBitsScaled(const BitMap *srcBits, const BitMap *dstBits,
                          const Rect *srcRect, const Rect *dstRect,
                          SInt16 mode, const ScaleInfo *scaleInfo,
                          RgnHandle maskRgn);
static void CopyBitsUnscaled(const BitMap *srcBits, const BitMap *dstBits,
                            const Rect *srcRect, const Rect *dstRect,
                            SInt16 mode, RgnHandle maskRgn);
static UInt32 ApplyTransferMode(UInt32 src, UInt32 dst, UInt32 pattern, SInt16 mode);
static void CalculateScaling(const Rect *srcRect, const Rect *dstRect, ScaleInfo *scaleInfo);
static UInt32 GetPixelValue(const BitMap *bitmap, SInt16 x, SInt16 y);
static void SetPixelValue(const BitMap *bitmap, SInt16 x, SInt16 y, UInt32 value);
static Boolean ClipAndAlignRects(const BitMap *srcBits, const BitMap *dstBits,
                                 Rect *srcRect, Rect *dstRect);
static void InitBitmapDescriptor(const BitMap *bitmap, BitmapDescriptor *desc);
static UInt32 ReadPixelColor(const BitMap *bitmap, const BitmapDescriptor *desc,
                             SInt16 x, SInt16 y, UInt32 fgColor, UInt32 bgColor);
static void WritePixelColor(const BitMap *bitmap, const BitmapDescriptor *desc,
                            SInt16 x, SInt16 y, UInt32 color, UInt32 fgColor, UInt32 bgColor);
static UInt32 SamplePatternColor(const Pattern *pat, SInt16 x, SInt16 y,
                                 UInt32 fgColor, UInt32 bgColor);
static void GetPortColors(UInt32 *fgColor, UInt32 *bgColor);

/* Transfer mode operations */
static inline UInt32 MaskColor(UInt32 color) {
    return color & kColorMask;
}

static UInt32 TransferSrcCopy(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)dst;
    (void)pattern;
    return MaskColor(src);
}

static UInt32 TransferSrcOr(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)pattern;
    return MaskColor(src | dst);
}

static UInt32 TransferSrcXor(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)pattern;
    return MaskColor(src ^ dst);
}

static UInt32 TransferSrcBic(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)pattern;
    return MaskColor(dst & (~src));
}

static UInt32 TransferNotSrcCopy(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)dst;
    (void)pattern;
    return MaskColor(~src);
}

static UInt32 TransferNotSrcOr(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)pattern;
    return MaskColor(~(src | dst));
}

static UInt32 TransferNotSrcXor(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)pattern;
    return MaskColor(~(src ^ dst));
}

static UInt32 TransferNotSrcBic(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)pattern;
    return MaskColor(~(dst & (~src)));
}

static UInt32 TransferPatCopy(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    (void)dst;
    return MaskColor(pattern);
}

static UInt32 TransferPatOr(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    return MaskColor(pattern | dst);
}

static UInt32 TransferPatXor(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    return MaskColor(pattern ^ dst);
}

static UInt32 TransferPatBic(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    return MaskColor(dst & (~pattern));
}

static UInt32 TransferNotPatCopy(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    (void)dst;
    return MaskColor(~pattern);
}

static UInt32 TransferNotPatOr(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    return MaskColor(~(pattern | dst));
}

static UInt32 TransferNotPatXor(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    return MaskColor(~(pattern ^ dst));
}

static UInt32 TransferNotPatBic(UInt32 src, UInt32 dst, UInt32 pattern) {
    (void)src;
    return MaskColor(~(dst & (~pattern)));
}

static const TransferModeInfo g_transferModes[] = {
    {TransferSrcCopy,    false, true},  /* srcCopy */
    {TransferSrcOr,      false, true},  /* srcOr */
    {TransferSrcXor,     false, true},  /* srcXor */
    {TransferSrcBic,     false, true},  /* srcBic */
    {TransferNotSrcCopy, false, true},  /* notSrcCopy */
    {TransferNotSrcOr,   false, true},  /* notSrcOr */
    {TransferNotSrcXor,  false, true},  /* notSrcXor */
    {TransferNotSrcBic,  false, true},  /* notSrcBic */
    {TransferPatCopy,    true,  true},  /* patCopy */
    {TransferPatOr,      true,  true},  /* patOr */
    {TransferPatXor,     true,  true},  /* patXor */
    {TransferPatBic,     true,  true},  /* patBic */
    {TransferNotPatCopy, true,  true},  /* notPatCopy */
    {TransferNotPatOr,   true,  true},  /* notPatOr */
    {TransferNotPatXor,  true,  true},  /* notPatXor */
    {TransferNotPatBic,  true,  true}   /* notPatBic */
};

static inline UInt8 Expand5To8(UInt16 value) {
    value &= 0x1F;
    return (UInt8)((value << 3) | (value >> 2));
}

static inline UInt16 Compress8To5(UInt8 value) {
    return (UInt16)((value * 31 + 127) / 255);
}

static UInt32 ColorTableLookup(const ColorTable *table, UInt16 index) {
    if (!table) {
        UInt8 gray = (UInt8)(index & 0xFF);
        return pack_color(gray, gray, gray) & kColorMask;
    }

    SInt16 entryCount = table->ctSize + 1;
    for (SInt16 i = 0; i < entryCount; i++) {
        if (table->ctTable[i].value == (SInt16)index) {
            const RGBColor *rgb = &table->ctTable[i].rgb;
            return QDPlatform_RGBToNative(rgb->red, rgb->green, rgb->blue) & kColorMask;
        }
    }

    if (index < (UInt16)entryCount) {
        const RGBColor *rgb = &table->ctTable[index].rgb;
        return QDPlatform_RGBToNative(rgb->red, rgb->green, rgb->blue) & kColorMask;
    }

    const RGBColor *fallback = &table->ctTable[0].rgb;
    return QDPlatform_RGBToNative(fallback->red, fallback->green, fallback->blue) & kColorMask;
}

static void GetPortColors(UInt32 *fgColor, UInt32 *bgColor) {
    if (fgColor) *fgColor = pack_color(0, 0, 0);
    if (bgColor) *bgColor = pack_color(255, 255, 255);

    if (g_currentCPort && (GrafPtr)g_currentCPort == g_currentPort) {
        if (fgColor) {
            const RGBColor *rgb = &g_currentCPort->rgbFgColor;
            *fgColor = QDPlatform_RGBToNative(rgb->red, rgb->green, rgb->blue) & kColorMask;
        }
        if (bgColor) {
            const RGBColor *rgb = &g_currentCPort->rgbBkColor;
            *bgColor = QDPlatform_RGBToNative(rgb->red, rgb->green, rgb->blue) & kColorMask;
        }
        return;
    }

    if (g_currentPort) {
        if (fgColor) {
            *fgColor = QDPlatform_MapQDColor(g_currentPort->fgColor) & kColorMask;
        }
        if (bgColor) {
            *bgColor = QDPlatform_MapQDColor(g_currentPort->bkColor) & kColorMask;
        }
    }
}

static UInt32 SamplePatternColor(const Pattern *pat, SInt16 x, SInt16 y,
                                 UInt32 fgColor, UInt32 bgColor) {
    if (!pat) {
        return fgColor;
    }

    SInt16 patY = y & 7;
    SInt16 patX = x & 7;
    UInt8 patByte = pat->pat[patY];
    Boolean bit = (patByte >> (7 - patX)) & 1;
    return bit ? fgColor : bgColor;
}

static Boolean ClipAndAlignRects(const BitMap *srcBits, const BitMap *dstBits,
                                 Rect *srcRect, Rect *dstRect) {
    SInt16 delta;

    if (srcRect->left < srcBits->bounds.left) {
        delta = srcBits->bounds.left - srcRect->left;
        srcRect->left += delta;
        dstRect->left += delta;
    }
    if (dstRect->left < dstBits->bounds.left) {
        delta = dstBits->bounds.left - dstRect->left;
        srcRect->left += delta;
        dstRect->left += delta;
    }

    if (srcRect->top < srcBits->bounds.top) {
        delta = srcBits->bounds.top - srcRect->top;
        srcRect->top += delta;
        dstRect->top += delta;
    }
    if (dstRect->top < dstBits->bounds.top) {
        delta = dstBits->bounds.top - dstRect->top;
        srcRect->top += delta;
        dstRect->top += delta;
    }

    if (srcRect->right > srcBits->bounds.right) {
        delta = srcRect->right - srcBits->bounds.right;
        srcRect->right -= delta;
        dstRect->right -= delta;
    }
    if (dstRect->right > dstBits->bounds.right) {
        delta = dstRect->right - dstBits->bounds.right;
        srcRect->right -= delta;
        dstRect->right -= delta;
    }

    if (srcRect->bottom > srcBits->bounds.bottom) {
        delta = srcRect->bottom - srcBits->bounds.bottom;
        srcRect->bottom -= delta;
        dstRect->bottom -= delta;
    }
    if (dstRect->bottom > dstBits->bounds.bottom) {
        delta = dstRect->bottom - dstBits->bounds.bottom;
        srcRect->bottom -= delta;
        dstRect->bottom -= delta;
    }

    SInt16 widthSrc = srcRect->right - srcRect->left;
    SInt16 widthDst = dstRect->right - dstRect->left;
    SInt16 heightSrc = srcRect->bottom - srcRect->top;
    SInt16 heightDst = dstRect->bottom - dstRect->top;

    SInt16 finalWidth = (widthSrc < widthDst) ? widthSrc : widthDst;
    SInt16 finalHeight = (heightSrc < heightDst) ? heightSrc : heightDst;

    srcRect->right = srcRect->left + finalWidth;
    dstRect->right = dstRect->left + finalWidth;
    srcRect->bottom = srcRect->top + finalHeight;
    dstRect->bottom = dstRect->top + finalHeight;

    return (finalWidth > 0 && finalHeight > 0);
}

static void InitBitmapDescriptor(const BitMap *bitmap, BitmapDescriptor *desc) {
    memset(desc, 0, sizeof(*desc));

    desc->isPixMap = IsPixMap(bitmap);
    if (desc->isPixMap) {
        const PixMap *pm = (const PixMap *)bitmap;
        desc->pixMap = pm;
        desc->pixelSize = pm->pixelSize;
        if (pm->pmTable) {
            desc->colorTable = (ColorTable *)*((Handle)pm->pmTable);
        }
        return;
    }

    /* Attempt to derive PixMap information from current color port */
    if (g_currentCPort && (GrafPtr)g_currentCPort == g_currentPort &&
        g_currentCPort->portPixMap && *g_currentCPort->portPixMap &&
        bitmap->baseAddr == (*g_currentCPort->portPixMap)->baseAddr) {
        const PixMap *pm = *g_currentCPort->portPixMap;
        desc->isPixMap = true;
        desc->pixMap = pm;
        desc->pixelSize = pm->pixelSize;
        if (pm->pmTable) {
            desc->colorTable = (ColorTable *)*((Handle)pm->pmTable);
        }
        return;
    }

    desc->pixelSize = 1;
}

static UInt32 ReadPixelColor(const BitMap *bitmap, const BitmapDescriptor *desc,
                             SInt16 x, SInt16 y, UInt32 fgColor, UInt32 bgColor) {
    UInt32 raw = GetPixelValue(bitmap, x, y);

    if (!desc->isPixMap) {
        return raw ? fgColor : bgColor;
    }

    switch (desc->pixelSize) {
        case 1:
            if (desc->colorTable) {
                return ColorTableLookup(desc->colorTable, (UInt16)raw);
            }
            return raw ? fgColor : bgColor;
        case 2:
        case 4:
        case 8:
            if (desc->colorTable) {
                return ColorTableLookup(desc->colorTable, (UInt16)raw);
            } else {
                UInt8 gray = (UInt8)raw;
                return pack_color(gray, gray, gray) & kColorMask;
            }
        case 16: {
            UInt16 value = (UInt16)raw;
            UInt8 r = Expand5To8((value >> 10) & 0x1F);
            UInt8 g = Expand5To8((value >> 5) & 0x1F);
            UInt8 b = Expand5To8(value & 0x1F);
            return pack_color(r, g, b) & kColorMask;
        }
        case 24:
        case 32:
        default:
            return raw & kColorMask;
    }
}

static UInt32 ColorDistanceSquaredNative(UInt32 a, UInt32 b) {
    UInt16 ar, ag, ab;
    UInt16 br, bg, bb;
    QDPlatform_NativeToRGB(a, &ar, &ag, &ab);
    QDPlatform_NativeToRGB(b, &br, &bg, &bb);
    SInt32 dr = (SInt32)ar - (SInt32)br;
    SInt32 dg = (SInt32)ag - (SInt32)bg;
    SInt32 db = (SInt32)ab - (SInt32)bb;
    return (UInt32)(dr * dr + dg * dg + db * db);
}

static void WritePixelColor(const BitMap *bitmap, const BitmapDescriptor *desc,
                            SInt16 x, SInt16 y, UInt32 color, UInt32 fgColor, UInt32 bgColor) {
    if (!desc->isPixMap) {
        /* Choose nearest between foreground/background */
        UInt32 distFg = ColorDistanceSquaredNative(color, fgColor);
        UInt32 distBg = ColorDistanceSquaredNative(color, bgColor);
        UInt32 bit = (distFg <= distBg) ? 1 : 0;
        SetPixelValue(bitmap, x, y, bit);
        return;
    }

    switch (desc->pixelSize) {
        case 1: {
            UInt32 bit = (ColorDistanceSquaredNative(color, fgColor) <=
                          ColorDistanceSquaredNative(color, bgColor)) ? 1 : 0;
            SetPixelValue(bitmap, x, y, bit);
            break;
        }
        case 2:
        case 4:
        case 8: {
            if (desc->colorTable) {
                const ColorTable *table = desc->colorTable;
                SInt16 entryCount = table->ctSize + 1;
                UInt32 bestIndex = 0;
                UInt32 bestDistance = UINT32_MAX;
                for (SInt16 i = 0; i < entryCount; i++) {
                    UInt32 entryColor = QDPlatform_RGBToNative(table->ctTable[i].rgb.red,
                                                               table->ctTable[i].rgb.green,
                                                               table->ctTable[i].rgb.blue) & kColorMask;
                    UInt32 distance = ColorDistanceSquaredNative(color, entryColor);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestIndex = (UInt16)table->ctTable[i].value;
                    }
                }
                SetPixelValue(bitmap, x, y, bestIndex);
            } else {
                UInt16 r, g, b;
                QDPlatform_NativeToRGB(color, &r, &g, &b);
                UInt8 gray = (UInt8)(((r + g + b) / 3) >> 8);
                SetPixelValue(bitmap, x, y, gray);
            }
            break;
        }
        case 16: {
            UInt16 r, g, b;
            QDPlatform_NativeToRGB(color, &r, &g, &b);
            UInt16 value = (Compress8To5((UInt8)(r >> 8)) << 10) |
                           (Compress8To5((UInt8)(g >> 8)) << 5) |
                           Compress8To5((UInt8)(b >> 8));
            SetPixelValue(bitmap, x, y, value);
            break;
        }
        case 24:
        case 32:
        default:
            SetPixelValue(bitmap, x, y, color & kColorMask);
            break;
    }
}

/* ================================================================
 * COPYBITS IMPLEMENTATION
 * ================================================================ */

void CopyBits(const BitMap *srcBits, const BitMap *dstBits,
              const Rect *srcRect, const Rect *dstRect,
              SInt16 mode, RgnHandle maskRgn) {
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
                                  SInt16 mode, RgnHandle maskRgn) {
    Rect alignedSrcRect = *srcRect;
    Rect alignedDstRect = *dstRect;

    if (!ClipAndAlignRects(srcBits, dstBits, &alignedSrcRect, &alignedDstRect)) {
        return;
    }

    ScaleInfo scaleInfo;
    CalculateScaling(&alignedSrcRect, &alignedDstRect, &scaleInfo);

    if (scaleInfo.needsScaling) {
        CopyBitsScaled(srcBits, dstBits, &alignedSrcRect, &alignedDstRect,
                       mode, &scaleInfo, maskRgn);
    } else {
        CopyBitsUnscaled(srcBits, dstBits, &alignedSrcRect, &alignedDstRect, mode, maskRgn);
    }
}

static void CopyBitsScaled(const BitMap *srcBits, const BitMap *dstBits,
                          const Rect *srcRect, const Rect *dstRect,
                          SInt16 mode, const ScaleInfo *scaleInfo,
                          RgnHandle maskRgn) {
    SInt16 dstWidth = dstRect->right - dstRect->left;
    SInt16 dstHeight = dstRect->bottom - dstRect->top;

    BitmapDescriptor srcDesc;
    BitmapDescriptor dstDesc;
    InitBitmapDescriptor(srcBits, &srcDesc);
    InitBitmapDescriptor(dstBits, &dstDesc);

    UInt32 fgColor, bgColor;
    GetPortColors(&fgColor, &bgColor);

    Boolean usePattern = (mode >= patCopy && mode <= notPatBic);
    const Pattern *activePattern = NULL;
    if (usePattern) {
        if (g_currentPort) {
            activePattern = &g_currentPort->pnPat;
        } else {
            activePattern = &qd.black;
        }
    }
    Boolean useMask = (maskRgn && *maskRgn);

    for (SInt16 dy = 0; dy < dstHeight; dy++) {
        SInt16 dstY = dstRect->top + dy;
        SInt16 srcY = srcRect->top + (SInt16)(((SInt32)dy * scaleInfo->vScale) >> 16);
        if (srcY >= srcRect->bottom) {
            srcY = srcRect->bottom - 1;
        }

        for (SInt16 dx = 0; dx < dstWidth; dx++) {
            SInt16 dstX = dstRect->left + dx;
            if (useMask) {
                Point pt;
                pt.v = dstY;
                pt.h = dstX;
                if (!PtInRgn(pt, maskRgn)) {
                    continue;
                }
            }

            SInt16 srcX = srcRect->left + (SInt16)(((SInt32)dx * scaleInfo->hScale) >> 16);
            if (srcX >= srcRect->right) {
                srcX = srcRect->right - 1;
            }

            UInt32 patternColor = 0;
            if (activePattern) {
                patternColor = SamplePatternColor(activePattern, dstX, dstY, fgColor, bgColor);
            }

            UInt32 srcColor = ReadPixelColor(srcBits, &srcDesc, srcX, srcY, fgColor, bgColor);
            UInt32 dstColor = ReadPixelColor(dstBits, &dstDesc, dstX, dstY, fgColor, bgColor);
            UInt32 result = ApplyTransferMode(srcColor, dstColor, patternColor, mode);
            WritePixelColor(dstBits, &dstDesc, dstX, dstY, result, fgColor, bgColor);
        }
    }
}

static void CopyBitsUnscaled(const BitMap *srcBits, const BitMap *dstBits,
                            const Rect *srcRect, const Rect *dstRect,
                            SInt16 mode, RgnHandle maskRgn) {
    if (!srcBits || !dstBits || !srcRect || !dstRect) {
        return;
    }

    SInt16 width = srcRect->right - srcRect->left;
    SInt16 height = srcRect->bottom - srcRect->top;
    if (width <= 0 || height <= 0) {
        return;
    }

    BitmapDescriptor srcDesc;
    BitmapDescriptor dstDesc;
    InitBitmapDescriptor(srcBits, &srcDesc);
    InitBitmapDescriptor(dstBits, &dstDesc);

    UInt32 fgColor, bgColor;
    GetPortColors(&fgColor, &bgColor);

    Boolean useMask = (maskRgn && *maskRgn);
    Boolean usePattern = (mode >= patCopy && mode <= notPatBic);
    const Pattern *activePattern = NULL;
    if (usePattern) {
        if (g_currentPort) {
            activePattern = &g_currentPort->pnPat;
        } else {
            activePattern = &qd.black;
        }
    }

    if (srcDesc.isPixMap && dstDesc.isPixMap &&
        srcDesc.pixelSize == 32 && dstDesc.pixelSize == 32 &&
        mode == srcCopy && !useMask) {
        const PixMap *srcPm = srcDesc.pixMap;
        const PixMap *dstPm = dstDesc.pixMap;
        if (srcPm && dstPm && srcBits->baseAddr && dstBits->baseAddr) {
            SInt16 srcRowBytes = GetPixMapRowBytes(srcPm);
            SInt16 dstRowBytes = GetPixMapRowBytes(dstPm);
            UInt8 *srcBase = (UInt8 *)srcBits->baseAddr;
            UInt8 *dstBase = (UInt8 *)dstBits->baseAddr;

            for (SInt16 line = 0; line < height; line++) {
                SInt16 srcOffsetY = (srcRect->top + line - srcBits->bounds.top);
                SInt16 dstOffsetY = (dstRect->top + line - dstBits->bounds.top);
                SInt16 srcOffsetX = srcRect->left - srcBits->bounds.left;
                SInt16 dstOffsetX = dstRect->left - dstBits->bounds.left;

                /* Skip lines with negative offsets to prevent unsigned wraparound */
                if (srcOffsetY < 0 || dstOffsetY < 0 || srcOffsetX < 0 || dstOffsetX < 0) continue;

                UInt32 srcStart = (UInt32)srcOffsetY * (UInt32)srcRowBytes +
                                  (UInt32)srcOffsetX * 4u;
                UInt32 dstStart = (UInt32)dstOffsetY * (UInt32)dstRowBytes +
                                  (UInt32)dstOffsetX * 4u;
                UInt32 copyBytes = (UInt32)width * 4u;

                /* Bounds safety: if pmReserved holds buffer size, clamp copy */
                UInt32 srcLimit = srcPm->pmReserved ? (UInt32)srcPm->pmReserved
                                 : (UInt32)(GetPixMapRowBytes(srcPm)) * (UInt32)(srcBits->bounds.bottom - srcBits->bounds.top);
                UInt32 dstLimit = dstPm->pmReserved ? (UInt32)dstPm->pmReserved
                                 : (UInt32)(GetPixMapRowBytes(dstPm)) * (UInt32)(dstBits->bounds.bottom - dstBits->bounds.top);
                if (srcStart + copyBytes > srcLimit) {
                    if (srcStart >= srcLimit) continue; /* skip line if entirely out */
                    copyBytes = srcLimit - srcStart;
                }
                if (dstStart + copyBytes > dstLimit) {
                    if (dstStart >= dstLimit) continue; /* skip line if entirely out */
                    copyBytes = dstLimit - dstStart;
                }

                UInt8 *srcRow = srcBase + srcStart;
                UInt8 *dstRow = dstBase + dstStart;
                if (copyBytes > 0) {
                    qd_log_memcpy("[CopyBits32] memcpy", srcRow, dstRow, copyBytes);
                    memcpy(dstRow, srcRow, copyBytes);
                }
            }
            return;
        }
    }

    for (SInt16 line = 0; line < height; line++) {
        SInt16 srcY = srcRect->top + line;
        SInt16 dstY = dstRect->top + line;

        for (SInt16 column = 0; column < width; column++) {
            SInt16 srcX = srcRect->left + column;
            SInt16 dstX = dstRect->left + column;

            if (useMask) {
                Point pt;
                pt.v = dstY;
                pt.h = dstX;
                if (!PtInRgn(pt, maskRgn)) {
                    continue;
                }
            }

            UInt32 patternColor = 0;
            if (activePattern) {
                patternColor = SamplePatternColor(activePattern, dstX, dstY, fgColor, bgColor);
            }

            UInt32 srcColor = ReadPixelColor(srcBits, &srcDesc, srcX, srcY, fgColor, bgColor);
            UInt32 dstColor = ReadPixelColor(dstBits, &dstDesc, dstX, dstY, fgColor, bgColor);
            UInt32 result = ApplyTransferMode(srcColor, dstColor, patternColor, mode);
            WritePixelColor(dstBits, &dstDesc, dstX, dstY, result, fgColor, bgColor);
        }
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

    SInt16 width = srcRect->right - srcRect->left;
    SInt16 height = srcRect->bottom - srcRect->top;

    BitmapDescriptor srcDesc;
    BitmapDescriptor dstDesc;
    InitBitmapDescriptor(srcBits, &srcDesc);
    InitBitmapDescriptor(dstBits, &dstDesc);

    UInt32 fgColor, bgColor;
    GetPortColors(&fgColor, &bgColor);

    for (SInt16 y = 0; y < height; y++) {
        for (SInt16 x = 0; x < width; x++) {
            UInt32 maskPixel = GetPixelValue(maskBits,
                                             maskRect->left + x,
                                             maskRect->top + y);

            if (maskPixel != 0) {
                UInt32 srcColor = ReadPixelColor(srcBits, &srcDesc,
                                                 srcRect->left + x,
                                                 srcRect->top + y,
                                                 fgColor, bgColor);
                WritePixelColor(dstBits, &dstDesc,
                                dstRect->left + x,
                                dstRect->top + y,
                                srcColor, fgColor, bgColor);
            }
        }
    }
}

void CopyDeepMask(const BitMap *srcBits, const BitMap *maskBits,
                  const BitMap *dstBits, const Rect *srcRect,
                  const Rect *maskRect, const Rect *dstRect,
                  SInt16 mode, RgnHandle maskRgn) {
    assert(srcBits != NULL);
    assert(maskBits != NULL);
    assert(dstBits != NULL);
    assert(srcRect != NULL);
    assert(maskRect != NULL);
    assert(dstRect != NULL);

    /* Validate rectangles */
    if (EmptyRect(srcRect) || EmptyRect(maskRect) || EmptyRect(dstRect)) return;

    SInt16 width = srcRect->right - srcRect->left;
    SInt16 height = srcRect->bottom - srcRect->top;

    BitmapDescriptor srcDesc;
    BitmapDescriptor dstDesc;
    InitBitmapDescriptor(srcBits, &srcDesc);
    InitBitmapDescriptor(dstBits, &dstDesc);

    UInt32 fgColor, bgColor;
    GetPortColors(&fgColor, &bgColor);

    for (SInt16 y = 0; y < height; y++) {
        for (SInt16 x = 0; x < width; x++) {
            UInt32 maskPixel = GetPixelValue(maskBits,
                                             maskRect->left + x,
                                             maskRect->top + y);

            if (maskPixel != 0) {
                UInt32 srcColor = ReadPixelColor(srcBits, &srcDesc,
                                                 srcRect->left + x,
                                                 srcRect->top + y,
                                                 fgColor, bgColor);
                UInt32 dstColor = ReadPixelColor(dstBits, &dstDesc,
                                                 dstRect->left + x,
                                                 dstRect->top + y,
                                                 fgColor, bgColor);
                UInt32 resultColor = ApplyTransferMode(srcColor, dstColor, 0, mode);
                WritePixelColor(dstBits, &dstDesc,
                                dstRect->left + x,
                                dstRect->top + y,
                                resultColor, fgColor, bgColor);
            }
        }
    }
}

/* ================================================================
 * RECTANGLE SCROLLING
 * ================================================================ */

void ScrollRect(const Rect *r, SInt16 dh, SInt16 dv, RgnHandle updateRgn) {
    if (!r || !g_currentPort) {
        if (updateRgn && *updateRgn) {
            SetEmptyRgn(updateRgn);
        }
        return;
    }

    Rect scrollRect = *r;

    if (EmptyRect(&scrollRect) || (dh == 0 && dv == 0)) {
        if (updateRgn && *updateRgn) {
            SetEmptyRgn(updateRgn);
        }
        return;
    }

    /* Confine scroll region to the current port */
    Rect srcRectLocal;
    if (!SectRect(&scrollRect, &g_currentPort->portRect, &srcRectLocal)) {
        if (updateRgn && *updateRgn) {
            SetEmptyRgn(updateRgn);
        }
        return;
    }

    Rect dstRectLocal = srcRectLocal;
    OffsetRect(&dstRectLocal, dh, dv);

    /* Determine on-screen destination area */
    Rect copyDstLocal;
    if (!SectRect(&dstRectLocal, &g_currentPort->portRect, &copyDstLocal)) {
        if (updateRgn && *updateRgn) {
            RectRgn(updateRgn, &srcRectLocal);
        }
        return;
    }

    /* Map destination back to the source space to get scroll payload */
    Rect copySrcLocal = copyDstLocal;
    OffsetRect(&copySrcLocal, -dh, -dv);

    if (!SectRect(&copySrcLocal, &srcRectLocal, &copySrcLocal)) {
        if (updateRgn && *updateRgn) {
            RectRgn(updateRgn, &srcRectLocal);
        }
        return;
    }

    /* Destination aligned with the clipped source */
    Rect copyDstAligned = copySrcLocal;
    OffsetRect(&copyDstAligned, dh, dv);

    /* Convert rectangles to global coordinates for CopyBits */
    Rect srcRectGlobal = copySrcLocal;
    Rect dstRectGlobal = copyDstAligned;
    OffsetRect(&srcRectGlobal,
               g_currentPort->portBits.bounds.left,
               g_currentPort->portBits.bounds.top);
    OffsetRect(&dstRectGlobal,
               g_currentPort->portBits.bounds.left,
               g_currentPort->portBits.bounds.top);

    RgnHandle maskRgn = NULL;
    if (g_currentPort->clipRgn && *g_currentPort->clipRgn) {
        maskRgn = g_currentPort->clipRgn;
    }

    CopyBits(&g_currentPort->portBits, &g_currentPort->portBits,
             &srcRectGlobal, &dstRectGlobal, srcCopy, maskRgn);

    if (updateRgn && *updateRgn) {
        RectRgn(updateRgn, &srcRectLocal);

        Rect overlapLocal;
        if (SectRect(&srcRectLocal, &dstRectLocal, &overlapLocal)) {
            RgnHandle overlapRgn = NewRgn();
            if (overlapRgn && *overlapRgn) {
                RectRgn(overlapRgn, &overlapLocal);
                DiffRgn(updateRgn, overlapRgn, updateRgn);
                DisposeRgn(overlapRgn);
            } else if (overlapRgn) {
                DisposeRgn(overlapRgn);
            }
        }
    }
}

/* ================================================================
 * SEED FILL OPERATIONS
 * ================================================================ */

/* Helper: Get pixel value at coordinates */
static Boolean GetPixelBit(const UInt8 *bitmap, SInt16 rowBytes, SInt16 x, SInt16 y) {
    const UInt16 *row = (const UInt16 *)(bitmap + y * rowBytes);
    UInt16 wordIndex = x / 16;
    UInt16 bitIndex = x % 16;
    return (row[wordIndex] & (1 << (15 - bitIndex))) != 0;
}

/* Helper: Set pixel value at coordinates */
static void SetPixelBit(UInt8 *bitmap, SInt16 rowBytes, SInt16 x, SInt16 y) {
    UInt16 *row = (UInt16 *)(bitmap + y * rowBytes);
    UInt16 wordIndex = x / 16;
    UInt16 bitIndex = x % 16;
    row[wordIndex] |= (1 << (15 - bitIndex));
}

/* Scanline flood-fill implementation */
void SeedFill(const void *srcPtr, void *dstPtr, SInt16 srcRow, SInt16 dstRow,
              SInt16 height, SInt16 words, SInt16 seedH, SInt16 seedV) {
    assert(srcPtr != NULL);
    assert(dstPtr != NULL);

    /* Validate parameters */
    if (srcRow % 2 != 0 || dstRow % 2 != 0) {
        return;  /* rowBytes must be even for UInt16* access */
    }
    if (height < 0 || words < 0 || height > 10000 || words > 10000) {
        return;  /* Sanity check failed */
    }

    const UInt8 *srcBytes = (const UInt8 *)srcPtr;
    UInt8 *dstBytes = (UInt8 *)dstPtr;
    SInt16 width = words * 16;

    /* Copy source to destination first */
    for (SInt16 y = 0; y < height; y++) {
        const UInt8 *srcLine = srcBytes + y * srcRow;
        UInt8 *dstLine = dstBytes + y * dstRow;
        memcpy(dstLine, srcLine, words * 2);
    }

    /* Validate seed point */
    if (seedV < 0 || seedV >= height || seedH < 0 || seedH >= width) {
        return;
    }

    /* Get the target pixel value at seed point */
    Boolean targetValue = GetPixelBit(srcBytes, srcRow, seedH, seedV);

    /* Allocate scanline stack for flood-fill (limit to reasonable size) */
    typedef struct { SInt16 y, xLeft, xRight, dy; } ScanlineSegment;
    ScanlineSegment *stack = (ScanlineSegment *)NewPtr(sizeof(ScanlineSegment) * 1024);
    if (!stack) return;

    SInt16 stackTop = 0;

    /* Push initial seed scanline */
    stack[stackTop].y = seedV;
    stack[stackTop].xLeft = seedH;
    stack[stackTop].xRight = seedH;
    stack[stackTop].dy = 0;
    stackTop++;

    while (stackTop > 0 && stackTop < 1024) {
        /* Pop scanline from stack */
        stackTop--;
        SInt16 y = stack[stackTop].y;
        SInt16 xLeft = stack[stackTop].xLeft;
        SInt16 xRight = stack[stackTop].xRight;
        SInt16 dy = stack[stackTop].dy;

        /* Move to next scanline */
        y += dy;
        if (y < 0 || y >= height) continue;

        /* Find left and right extent of this scanline */
        SInt16 xStart = xLeft;
        while (xStart >= 0 && GetPixelBit(srcBytes, srcRow, xStart, y) == targetValue) {
            xStart--;
        }
        xStart++;

        SInt16 xEnd = xRight;
        while (xEnd < width && GetPixelBit(srcBytes, srcRow, xEnd, y) == targetValue) {
            xEnd++;
        }
        xEnd--;

        /* Fill this scanline in destination */
        for (SInt16 x = xStart; x <= xEnd; x++) {
            SetPixelBit(dstBytes, dstRow, x, y);
        }

        /* Check adjacent scanlines above and below */
        for (SInt16 direction = -1; direction <= 1; direction += 2) {
            SInt16 nextY = y + direction;
            if (nextY < 0 || nextY >= height) continue;

            /* Scan this segment for fillable regions */
            SInt16 x = xStart;
            while (x <= xEnd) {
                /* Skip non-matching pixels */
                while (x <= xEnd && GetPixelBit(srcBytes, srcRow, x, nextY) != targetValue) {
                    x++;
                }
                if (x > xEnd) break;

                /* Found start of fillable region */
                SInt16 segStart = x;

                /* Find end of fillable region */
                while (x <= xEnd && GetPixelBit(srcBytes, srcRow, x, nextY) == targetValue) {
                    x++;
                }
                SInt16 segEnd = x - 1;

                /* Push this segment to stack */
                if (stackTop < 1024) {
                    stack[stackTop].y = y;
                    stack[stackTop].xLeft = segStart;
                    stack[stackTop].xRight = segEnd;
                    stack[stackTop].dy = direction;
                    stackTop++;
                }
            }
        }
    }

    DisposePtr((Ptr)stack);
}

void CalcMask(const void *srcPtr, void *dstPtr, SInt16 srcRow, SInt16 dstRow,
              SInt16 height, SInt16 words) {
    assert(srcPtr != NULL);
    assert(dstPtr != NULL);

    const UInt16 *src = (const UInt16 *)srcPtr;
    UInt16 *dst = (UInt16 *)dstPtr;

    /* Calculate mask from source */
    for (SInt16 y = 0; y < height; y++) {
        const UInt16 *srcLine = src + y * (srcRow / 2);
        UInt16 *dstLine = dst + y * (dstRow / 2);

        for (SInt16 w = 0; w < words; w++) {
            /* Create mask: 1 where source is non-zero, 0 where zero */
            dstLine[w] = (srcLine[w] != 0) ? 0xFFFF : 0x0000;
        }
    }
}

/* ================================================================
 * UTILITY FUNCTIONS
 * ================================================================ */

static UInt32 ApplyTransferMode(UInt32 src, UInt32 dst, UInt32 pattern, SInt16 mode) {
    /* Clamp mode to valid range */
    if (mode < 0 || mode >= (SInt16)(sizeof(g_transferModes) / sizeof(g_transferModes[0]))) {
        mode = srcCopy;
    }

    const TransferModeInfo *modeInfo = &g_transferModes[mode];
    return modeInfo->operation(MaskColor(src), MaskColor(dst), MaskColor(pattern));
}

static void CalculateScaling(const Rect *srcRect, const Rect *dstRect, ScaleInfo *scaleInfo) {
    scaleInfo->srcWidth = srcRect->right - srcRect->left;
    scaleInfo->srcHeight = srcRect->bottom - srcRect->top;
    scaleInfo->dstWidth = dstRect->right - dstRect->left;
    scaleInfo->dstHeight = dstRect->bottom - dstRect->top;

    scaleInfo->needsScaling = (scaleInfo->srcWidth != scaleInfo->dstWidth ||
                              scaleInfo->srcHeight != scaleInfo->dstHeight);

    if (scaleInfo->needsScaling) {
        /* Guard against division by zero */
        if (scaleInfo->dstWidth == 0 || scaleInfo->dstHeight == 0) {
            /* Invalid destination rect - treat as no scaling */
            scaleInfo->needsScaling = false;
            scaleInfo->hScale = FIXED_POINT_SCALE;
            scaleInfo->vScale = FIXED_POINT_SCALE;
        } else {
            scaleInfo->hScale = (scaleInfo->srcWidth * FIXED_POINT_SCALE) / scaleInfo->dstWidth;
            scaleInfo->vScale = (scaleInfo->srcHeight * FIXED_POINT_SCALE) / scaleInfo->dstHeight;
        }
    } else {
        scaleInfo->hScale = FIXED_POINT_SCALE;
        scaleInfo->vScale = FIXED_POINT_SCALE;
    }
}

static UInt32 GetPixelValue(const BitMap *bitmap, SInt16 x, SInt16 y) {
    if (x < (bitmap)->bounds.left || x >= (bitmap)->bounds.right ||
        y < (bitmap)->bounds.top || y >= (bitmap)->bounds.bottom) {
        return 0;
    }

    /* Calculate byte offset */
    SInt16 relativeX = x - (bitmap)->bounds.left;
    SInt16 relativeY = y - (bitmap)->bounds.top;
    SInt16 rowBytes = bitmap->rowBytes & 0x3FFF; /* Mask out flags */

    const UInt8 *baseAddr = (const UInt8 *)bitmap->baseAddr;
    if (!baseAddr) return 0;

    const UInt8 *pixel = baseAddr + relativeY * rowBytes;

    if (IsPixMap(bitmap)) {
        /* Color bitmap */
        const PixMap *pixMap = (const PixMap *)bitmap;
        SInt16 pixelSize = pixMap->pixelSize;

        switch (pixelSize) {
            case 1: {
                SInt16 byteIndex = relativeX / 8;
                SInt16 bitIndex = relativeX % 8;
                return (pixel[byteIndex] >> (7 - bitIndex)) & 1;
            }
            case 8:
                return pixel[relativeX];
            case 16: {
                const UInt16 *pixel16 = (const UInt16 *)pixel;
                return pixel16[relativeX];
            }
            case 32: {
                const UInt32 *pixel32 = (const UInt32 *)pixel;
                return pixel32[relativeX];
            }
            default:
                return 0;
        }
    } else {
        /* 1-bit bitmap */
        SInt16 byteIndex = relativeX / 8;
        SInt16 bitIndex = relativeX % 8;
        return (pixel[byteIndex] >> (7 - bitIndex)) & 1;
    }
}

static void SetPixelValue(const BitMap *bitmap, SInt16 x, SInt16 y, UInt32 value) {
    if (x < (bitmap)->bounds.left || x >= (bitmap)->bounds.right ||
        y < (bitmap)->bounds.top || y >= (bitmap)->bounds.bottom) {
        return;
    }

    /* Calculate byte offset */
    SInt16 relativeX = x - (bitmap)->bounds.left;
    SInt16 relativeY = y - (bitmap)->bounds.top;
    SInt16 rowBytes = bitmap->rowBytes & 0x3FFF; /* Mask out flags */

    UInt8 *baseAddr = (UInt8 *)bitmap->baseAddr;
    if (!baseAddr) return;

    UInt8 *pixel = baseAddr + relativeY * rowBytes;

    if (IsPixMap(bitmap)) {
        /* Color bitmap */
        const PixMap *pixMap = (const PixMap *)bitmap;
        SInt16 pixelSize = pixMap->pixelSize;

        switch (pixelSize) {
            case 1: {
                SInt16 byteIndex = relativeX / 8;
                SInt16 bitIndex = relativeX % 8;
                if (value & 1) {
                    pixel[byteIndex] |= (1 << (7 - bitIndex));
                } else {
                    pixel[byteIndex] &= ~(1 << (7 - bitIndex));
                }
                break;
            }
            case 8:
                pixel[relativeX] = (UInt8)value;
                break;
            case 16: {
                UInt16 *pixel16 = (UInt16 *)pixel;
                pixel16[relativeX] = (UInt16)value;
                break;
            }
            case 32: {
                UInt32 *pixel32 = (UInt32 *)pixel;
                pixel32[relativeX] = value;
                break;
            }
        }
    } else {
        /* 1-bit bitmap */
        SInt16 byteIndex = relativeX / 8;
        SInt16 bitIndex = relativeX % 8;
        if (value & 1) {
            pixel[byteIndex] |= (1 << (7 - bitIndex));
        } else {
            pixel[byteIndex] &= ~(1 << (7 - bitIndex));
        }
    }
}

/* ================================================================
 * BITMAP REGION CONVERSION
 * ================================================================ */

SInt16 BitMapToRegion(RgnHandle region, const BitMap *bMap) {
    assert(region != NULL && *region != NULL);
    assert(bMap != NULL);

    /* Start with empty region */
    SetEmptyRgn(region);

    if (!bMap->baseAddr) return 0;

    SInt16 width = (bMap)->bounds.right - (bMap)->bounds.left;
    SInt16 height = (bMap)->bounds.bottom - (bMap)->bounds.top;

    /* For 1-bit bitmaps, create region from set bits */
    if (!IsPixMap(bMap)) {
        RgnHandle tempRgn = NewRgn();
        if (!tempRgn) return rgnOverflowErr;

        for (SInt16 y = 0; y < height; y++) {
            for (SInt16 x = 0; x < width; x++) {
                if (GetPixelValue(bMap, (bMap)->bounds.left + x, (bMap)->bounds.top + y)) {
                    Rect pixelRect;
                    SetRect(&pixelRect,
                           (bMap)->bounds.left + x, (bMap)->bounds.top + y,
                           (bMap)->bounds.left + x + 1, (bMap)->bounds.top + y + 1);
                    RectRgn(tempRgn, &pixelRect);
                    UnionRgn(region, tempRgn, region);
                }
            }
        }

        DisposeRgn(tempRgn);
    }

    return 0;
}

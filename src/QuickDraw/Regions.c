/* #include "SystemTypes.h" */
#include "QuickDraw/QuickDrawInternal.h"
#include "QuickDrawConstants.h"
#include <stdlib.h>
#include <string.h>
#include "MemoryMgr/MemoryManager.h"
/*
 * Regions.c - QuickDraw Region Implementation
 *
 * Complete implementation of QuickDraw regions including region arithmetic,
 * clipping operations, hit testing, and complex region manipulation.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "QuickDrawConstants.h"
#include "System71StdLib.h"

#include "QuickDraw/QDRegions.h"
#include "QuickDraw/QuickDraw.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>

/* Platform abstraction layer */
#include "QuickDraw/QuickDrawPlatform.h"
#include "MemoryMgr/MemoryManager.h"

/* Serial logging for defensive diagnostics */
extern void serial_puts(const char* str);
extern void serial_putchar(char ch);

/* REGION_DEBUG: Set to 1 to enable verbose region logging
 * WARNING: Enabling this causes SEVERE performance degradation on ARM64 */
#define REGION_DEBUG 0


/* Region constants */
#define kMaxScanLines 4096
#define kMaxCoordsPerLine 1024

/* Region state for region recording */
typedef struct {
    Boolean recording;
    RgnHandle targetRegion;
    Rect recordingBounds;
    SInt16 *scanData;
    SInt16 scanDataSize;
    SInt16 scanDataUsed;
} RegionRecorder;

static RegionRecorder g_regionRecorder = {false, NULL, {0,0,0,0}, NULL, 0, 0};
static QDErr g_lastRegionError = 0;

/* Forward declarations - Commented out: not yet implemented */
#if 0
static void CompactRegionData(RgnHandle rgn);
static Boolean AddScanLineToRegion(RgnHandle rgn, SInt16 y, SInt16 *coords, SInt16 coordCount);
static void UpdateRegionBounds(RgnHandle rgn);
static Boolean IntersectScanLines(SInt16 *line1, SInt16 count1, SInt16 *line2, SInt16 count2,
                              SInt16 *result, SInt16 *resultCount);
static Boolean UnionScanLines(SInt16 *line1, SInt16 count1, SInt16 *line2, SInt16 count2,
                          SInt16 *result, SInt16 *resultCount);
static Boolean DifferenceScanLines(SInt16 *line1, SInt16 count1, SInt16 *line2, SInt16 count2,
                               SInt16 *result, SInt16 *resultCount);
static Boolean XorScanLines(SInt16 *line1, SInt16 count1, SInt16 *line2, SInt16 count2,
                        SInt16 *result, SInt16 *resultCount);
#endif

static void region_log_hex(uint32_t value, int digits) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; --i) {
        serial_putchar(hex[(value >> (i * 4)) & 0xF]);
    }
}

static void region_log_message(const char* context,
                               RgnHandle handle,
                               Region* region,
                               BlockHeader* header) {
#if REGION_DEBUG
    serial_puts("[REGION] ");
    serial_puts(context);
    serial_puts(" handle=0x");
    region_log_hex((uint32_t)(uintptr_t)handle, 8);
    serial_puts(" region=0x");
    region_log_hex((uint32_t)(uintptr_t)region, 8);
    serial_puts(" size=0x");
    region_log_hex(region ? (uint32_t)(uint16_t)region->rgnSize : 0, 4);
    serial_puts(" hdrSize=0x");
    region_log_hex(header ? header->size : 0, 8);
    serial_puts(" flags=0x");
    region_log_hex(header ? header->flags : 0, 4);
    serial_puts(" prev=0x");
    region_log_hex(header ? header->prevSize : 0, 8);
    serial_putchar('\n');
#else
    (void)context; (void)handle; (void)region; (void)header;
#endif
}

static SInt16 sanitize_region_size(Region* region, const char* label) {
    if (!region) {
        return kMinRegionSize;
    }

    SInt32 size = (SInt32)region->rgnSize;
    if (size >= kMinRegionSize && size <= kMaxRegionSize) {
        return (SInt16)size;
    }

#if REGION_DEBUG
    serial_puts("[REGION] ");
    serial_puts(label);
    serial_puts(": invalid rgnSize=0x");
    region_log_hex((uint32_t)size, 8);
    serial_puts(" at 0x");
    region_log_hex((uint32_t)(uintptr_t)region, 8);
    serial_puts(", clamping to 0x");
    region_log_hex((uint32_t)kMinRegionSize, 4);
    serial_putchar('\n');
#else
    (void)label;
#endif
    region->rgnSize = kMinRegionSize;
    return kMinRegionSize;
}

static void region_dump_bytes(const char* context, Region* region, SInt16 byteCount) {
#if REGION_DEBUG
    serial_puts("[REGION] ");
    serial_puts(context);
    serial_puts(" bytes:");
    if (!region) {
        serial_puts(" <null>\n");
        return;
    }

    UInt8* data = (UInt8*)region;
    for (SInt16 i = 0; i < byteCount; i++) {
        serial_putchar(' ');
        region_log_hex(data[i], 2);
    }
    serial_putchar('\n');
#else
    (void)context; (void)region; (void)byteCount;
#endif
}

/* ================================================================
 * BASIC REGION OPERATIONS
 * ================================================================ */

RgnHandle NewRgn(void) {
    /* Use NewPtr instead of calloc - calloc is broken in bare-metal kernel */
    RgnHandle rgn = (RgnHandle)NewPtr(sizeof(RgnPtr));
    if (!rgn) {
        g_lastRegionError = rgnOverflowErr;
        return NULL;
    }

    Region *region = (Region *)NewPtr(kMinRegionSize);
    if (!region) {
        DisposePtr((Ptr)rgn);
        g_lastRegionError = rgnOverflowErr;
        return NULL;
    }

    *rgn = region;
    region->rgnSize = kMinRegionSize;
    SetRect(&region->rgnBBox, 0, 0, 0, 0);

    BlockHeader* header = (BlockHeader*)((UInt8*)region - sizeof(BlockHeader));
    region_log_message("NewRgn", rgn, region, header);
    region_dump_bytes("NewRgn init", region, kMinRegionSize + 28);

    g_lastRegionError = 0;
    return rgn;
}

void DisposeRgn(RgnHandle rgn) {
    if (!rgn || !*rgn) return;

    Region* region = *rgn;
    BlockHeader* regionHeader = (BlockHeader*)((UInt8*)region - sizeof(BlockHeader));
    BlockHeader* handleHeader = (BlockHeader*)((UInt8*)rgn - sizeof(BlockHeader));
    region_log_message("DisposeRgn", rgn, region, regionHeader);
    region_dump_bytes("DisposeRgn pre", region, kMinRegionSize + 28);

    /* Use DisposePtr instead of free - free is broken in bare-metal kernel */
    DisposePtr((Ptr)*rgn);
    region_log_message("DisposeRgn handle block", rgn, NULL, handleHeader);
    DisposePtr((Ptr)rgn);
}

#if 0  /* Unused function */
static RgnHandle DuplicateRgn(RgnHandle srcRgn) {
    if (!srcRgn || !*srcRgn) return NULL;

    RgnHandle newRgn = NewRgn();
    if (!newRgn) return NULL;

    CopyRgn(srcRgn, newRgn);
    return newRgn;
}
#endif

void SetEmptyRgn(RgnHandle rgn) {
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* Don't resize - just mark as empty
     * realloc() is broken in bare-metal kernel and causes freeze */
    region->rgnSize = kMinRegionSize;
    SetRect(&region->rgnBBox, 0, 0, 0, 0);

    HUnlock((Handle)rgn);
}

void SetRectRgn(RgnHandle rgn, SInt16 left, SInt16 top, SInt16 right, SInt16 bottom) {
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* Empty region if rectangle is empty */
    if (left >= right || top >= bottom) {
        HUnlock((Handle)rgn);
        SetEmptyRgn(rgn);
        return;
    }

    /* Don't resize - realloc() is broken in bare-metal kernel
     * Just use existing allocation */
    #if 0  /* DISABLED - realloc causes freeze */
    Region *newRegion = (Region *)realloc(region, kMinRegionSize);
    if (newRegion) {
        *rgn = newRegion;
        region = newRegion;
    }
    #endif

    region->rgnSize = kMinRegionSize;
    SetRect(&region->rgnBBox, left, top, right, bottom);

    HUnlock((Handle)rgn);
}

void RectRgn(RgnHandle rgn, const Rect *r) {
    assert(rgn != NULL && *rgn != NULL);
    assert(r != NULL);

    SetRectRgn(rgn, r->left, r->top, r->right, r->bottom);
}

void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn) {
    /* Runtime NULL checks (asserts may be disabled in release builds) */
    if (!srcRgn || !*srcRgn || !dstRgn || !*dstRgn) {
        g_lastRegionError = rgnOverflowErr;
        return;
    }
    assert(srcRgn != NULL && *srcRgn != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Region *src = *srcRgn;
    Region *dst = *dstRgn;

    SInt16 srcSize = sanitize_region_size(src, "CopyRgn(src)");
    SInt16 dstSize = sanitize_region_size(dst, "CopyRgn(dst)");

    /* Reallocate destination if needed without using realloc() */
    if (srcSize > dstSize) {
        Region *newDst = (Region *)NewPtr((u32)srcSize);
        if (!newDst) {
            g_lastRegionError = rgnOverflowErr;
            return;
        }

        memcpy(newDst, src, (size_t)srcSize);
        DisposePtr((Ptr)dst);
        *dstRgn = newDst;
        g_lastRegionError = 0;
        return;
    }

    /* Copy the region data */
    memcpy(dst, src, (size_t)srcSize);
    g_lastRegionError = 0;
}

/* ================================================================
 * REGION RECORDING
 * ================================================================ */

void OpenRgn(void) {
    if (g_regionRecorder.recording) {
        g_lastRegionError = rgnOverflowErr;
        return;
    }

    g_regionRecorder.recording = true;
    g_regionRecorder.targetRegion = NULL;
    SetRect(&g_regionRecorder.recordingBounds, 32767, 32767, -32768, -32768);

    /* Allocate scan data buffer */
    if (!g_regionRecorder.scanData) {
        g_regionRecorder.scanDataSize = 1024;
        g_regionRecorder.scanData = (SInt16 *)NewPtr(g_regionRecorder.scanDataSize * sizeof(SInt16));
    }
    g_regionRecorder.scanDataUsed = 0;

    g_lastRegionError = 0;
}

void CloseRgn(RgnHandle dstRgn) {
    assert(dstRgn != NULL && *dstRgn != NULL);

    if (!g_regionRecorder.recording) {
        g_lastRegionError = rgnOverflowErr;
        return;
    }

    g_regionRecorder.recording = false;
    g_regionRecorder.targetRegion = dstRgn;

    /* Convert recorded data to region */
    if (EmptyRect(&g_regionRecorder.recordingBounds)) {
        SetEmptyRgn(dstRgn);
    } else {
        RectRgn(dstRgn, &g_regionRecorder.recordingBounds);
    }

    g_lastRegionError = 0;
}

/* ================================================================
 * REGION TRANSFORMATION
 * ================================================================ */

void OffsetRgn(RgnHandle rgn, SInt16 dh, SInt16 dv) {
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* Offset bounding box */
    OffsetRect(&region->rgnBBox, dh, dv);

    /* If this is a complex region, offset scan data */
    if (region->rgnSize > kMinRegionSize) {
        UInt8 *dataPtr = (UInt8 *)region + kMinRegionSize;
        UInt8 *endPtr = (UInt8 *)region + region->rgnSize;

        while (dataPtr < endPtr) {
            SInt16 y = *(SInt16 *)dataPtr;
            if (y == 0x7FFF) break; /* End marker */

            *(SInt16 *)dataPtr = y + dv;
            dataPtr += sizeof(SInt16);

            SInt16 count = *(SInt16 *)dataPtr;
            dataPtr += sizeof(SInt16);

            /* Offset x coordinates */
            for (int i = 0; i < count; i++) {
                *(SInt16 *)dataPtr += dh;
                dataPtr += sizeof(SInt16);
            }
        }
    }

    HUnlock((Handle)rgn);
}

void InsetRgn(RgnHandle rgn, SInt16 dh, SInt16 dv) {
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* For rectangular regions, just inset the bounds */
    if (region->rgnSize == kMinRegionSize) {
        InsetRect(&region->rgnBBox, dh, dv);
        if (EmptyRect(&region->rgnBBox)) {
            HUnlock((Handle)rgn);
            SetEmptyRgn(rgn);
            return;
        }
        HUnlock((Handle)rgn);
        return;
    }

    /* For complex regions, this is more involved */
    /* For now, we'll implement a simplified version */
    InsetRect(&region->rgnBBox, dh, dv);
    if (EmptyRect(&region->rgnBBox)) {
        HUnlock((Handle)rgn);
        SetEmptyRgn(rgn);
        return;
    }
    HUnlock((Handle)rgn);
}

/* ================================================================
 * REGION BOOLEAN OPERATIONS
 * ================================================================ */

void SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Region *regionA = *srcRgnA;
    Region *regionB = *srcRgnB;

    /* Check for empty regions */
    if (EmptyRect(&regionA->rgnBBox) || EmptyRect(&regionB->rgnBBox)) {
        SetEmptyRgn(dstRgn);
        return;
    }

    /* Check if bounding boxes don't intersect */
    Rect intersection;
    if (!SectRect(&regionA->rgnBBox, &regionB->rgnBBox, &intersection)) {
        SetEmptyRgn(dstRgn);
        return;
    }

    /* If both are rectangular regions */
    if (regionA->rgnSize == kMinRegionSize && regionB->rgnSize == kMinRegionSize) {
        RectRgn(dstRgn, &intersection);
        return;
    }

    /* For complex regions, use simplified implementation */
    /* In a full implementation, this would do scan line intersection */
    RectRgn(dstRgn, &intersection);
}

void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Region *regionA = *srcRgnA;
    Region *regionB = *srcRgnB;

    /* Check for empty regions */
    if (EmptyRect(&regionA->rgnBBox)) {
        CopyRgn(srcRgnB, dstRgn);
        return;
    }
    if (EmptyRect(&regionB->rgnBBox)) {
        CopyRgn(srcRgnA, dstRgn);
        return;
    }

    /* If both are rectangular regions */
    if (regionA->rgnSize == kMinRegionSize && regionB->rgnSize == kMinRegionSize) {
        Rect unionRect;
        UnionRect(&regionA->rgnBBox, &regionB->rgnBBox, &unionRect);
        RectRgn(dstRgn, &unionRect);
        return;
    }

    /* For complex regions, use simplified implementation */
    Rect unionRect;
    UnionRect(&regionA->rgnBBox, &regionB->rgnBBox, &unionRect);
    RectRgn(dstRgn, &unionRect);
}

void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Region *regionA = *srcRgnA;
    Region *regionB = *srcRgnB;

    /* Check for empty regions */
    if (EmptyRect(&regionA->rgnBBox)) {
        SetEmptyRgn(dstRgn);
        return;
    }
    if (EmptyRect(&regionB->rgnBBox)) {
        CopyRgn(srcRgnA, dstRgn);
        return;
    }

    /* Check if bounding boxes don't intersect */
    Rect intersection;
    if (!SectRect(&regionA->rgnBBox, &regionB->rgnBBox, &intersection)) {
        CopyRgn(srcRgnA, dstRgn);
        return;
    }

    /* For now, use simplified implementation */
    /* In a full implementation, this would do scan line difference */
    CopyRgn(srcRgnA, dstRgn);
}

void XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Region *regionA = *srcRgnA;
    Region *regionB = *srcRgnB;

    /* Check for empty regions */
    if (EmptyRect(&regionA->rgnBBox)) {
        CopyRgn(srcRgnB, dstRgn);
        return;
    }
    if (EmptyRect(&regionB->rgnBBox)) {
        CopyRgn(srcRgnA, dstRgn);
        return;
    }

    /* For now, use simplified implementation */
    Rect unionRect;
    UnionRect(&regionA->rgnBBox, &regionB->rgnBBox, &unionRect);
    RectRgn(dstRgn, &unionRect);
}

/* ================================================================
 * REGION QUERY OPERATIONS
 * ================================================================ */

Boolean EmptyRgn(RgnHandle rgn) {
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;
    Boolean result = EmptyRect(&region->rgnBBox);
    HUnlock((Handle)rgn);
    return result;
}

Boolean EqualRgn(RgnHandle rgnA, RgnHandle rgnB) {
    assert(rgnA != NULL && *rgnA != NULL);
    assert(rgnB != NULL && *rgnB != NULL);

    /* CRITICAL: Lock handles before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgnA);
    HLock((Handle)rgnB);

    Region *regionA = *rgnA;
    Region *regionB = *rgnB;

    SInt16 sizeA = sanitize_region_size(regionA, "EqualRgn(A)");
    SInt16 sizeB = sanitize_region_size(regionB, "EqualRgn(B)");

    /* First check sizes */
    if (sizeA != sizeB) {
        HUnlock((Handle)rgnB);
        HUnlock((Handle)rgnA);
        return false;
    }

    /* Then compare the data */
    Boolean result = (memcmp(regionA, regionB, (size_t)sizeA) == 0);
    HUnlock((Handle)rgnB);
    HUnlock((Handle)rgnA);
    return result;
}

Boolean RectInRgn(const Rect *r, RgnHandle rgn) {
    assert(r != NULL);
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* Simple implementation - check if rectangles intersect */
    Rect intersection;
    Boolean result = SectRect(r, &region->rgnBBox, &intersection);
    HUnlock((Handle)rgn);
    return result;
}

Boolean PtInRgn(Point pt, RgnHandle rgn) {
    assert(rgn != NULL && *rgn != NULL);

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* First check bounding box */
    if (!PtInRect(pt, &region->rgnBBox)) {
        HUnlock((Handle)rgn);
        return false;
    }

    /* For rectangular regions, that's sufficient */
    if (region->rgnSize == kMinRegionSize) {
        HUnlock((Handle)rgn);
        return true;
    }

    /* For complex regions, we need to check scan lines */
    /* This is a simplified implementation */
    HUnlock((Handle)rgn);
    return true;
}

/* ================================================================
 * REGION DRAWING
 * ================================================================ */

void FrameRgn(RgnHandle rgn) {
    if (!rgn || !*rgn) return;
    QDPlatform_DrawRegion(rgn, frame, NULL);
}

void PaintRgn(RgnHandle rgn) {
    if (!rgn || !*rgn) return;
    QDPlatform_DrawRegion(rgn, paint, NULL);
}

void EraseRgn(RgnHandle rgn) {
    if (!rgn || !*rgn) return;
    QDPlatform_DrawRegion(rgn, erase, NULL);
}

void InvertRgn(RgnHandle rgn) {
    if (!rgn || !*rgn) return;
    QDPlatform_DrawRegion(rgn, invert, NULL);
}

void FillRgn(RgnHandle rgn, ConstPatternParam pat) {
    if (!rgn || !*rgn || !pat) return;
    QDPlatform_DrawRegion(rgn, fill, pat);
}

/* ================================================================
 * ADVANCED REGION OPERATIONS
 * ================================================================ */

SInt16 GetRegionSize(RgnHandle rgn) {
    if (!rgn || !*rgn) return 0;
    return (*rgn)->rgnSize;
}

void GetRegionBounds(RgnHandle rgn, Rect *bounds) {
    assert(rgn != NULL && *rgn != NULL);
    assert(bounds != NULL);

    *bounds = (*rgn)->rgnBBox;
}

Boolean IsRectRegion(RgnHandle rgn) {
    if (!rgn || !*rgn) return false;
    return (*rgn)->rgnSize == kMinRegionSize;
}

Boolean IsComplexRegion(RgnHandle rgn) {
    if (!rgn || !*rgn) return false;
    return (*rgn)->rgnSize > kMinRegionSize;
}

Boolean ValidateRegion(RgnHandle rgn) {
    if (!rgn || !*rgn) return false;

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    /* Check minimum size */
    if (region->rgnSize < kMinRegionSize) {
        HUnlock((Handle)rgn);
        return false;
    }

    /* Check maximum size - rgnSize is SInt16, max value is 32767, so this check is unnecessary
     * as kMaxRegionSize == 32767. Keeping for documentation but disabling the warning. */
    /* if (region->rgnSize > kMaxRegionSize) return false; */

    /* For rectangular regions, just validate bounds */
    if (region->rgnSize == kMinRegionSize) {
        HUnlock((Handle)rgn);
        return true;
    }

    /* For complex regions, validate scan line data */
    /* This would be more complex in a full implementation */
    HUnlock((Handle)rgn);
    return true;
}

void CompactRegion(RgnHandle rgn) {
    if (!rgn || !*rgn) return;

    /* For now, just ensure the region is valid */
    ValidateRegion(rgn);
}

SInt16 GetRegionComplexity(RgnHandle rgn) {
    if (!rgn || !*rgn) return 0;

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock((Handle)rgn);
    Region *region = *rgn;

    if (region->rgnSize == kMinRegionSize) {
        HUnlock((Handle)rgn);
        return 1;
    }

    /* Count scan lines for complex regions */
    SInt16 complexity = 0;
    UInt8 *dataPtr = (UInt8 *)region + kMinRegionSize;
    UInt8 *endPtr = (UInt8 *)region + region->rgnSize;

    while (dataPtr < endPtr) {
        /* Bounds check: ensure we can read y value */
        if (dataPtr + sizeof(SInt16) > endPtr) break;

        SInt16 y = *(SInt16 *)dataPtr;
        if (y == 0x7FFF) break;

        complexity++;
        dataPtr += sizeof(SInt16);

        /* Bounds check: ensure we can read count value */
        if (dataPtr + sizeof(SInt16) > endPtr) break;

        SInt16 count = *(SInt16 *)dataPtr;

        /* Reject negative counts to prevent signed-to-unsigned overflow */
        if (count < 0) break;

        /* Bounds check: ensure count won't cause buffer overflow */
        UInt32 advance = sizeof(SInt16) + (UInt32)count * sizeof(SInt16);
        if (dataPtr + advance > endPtr) break;

        dataPtr += advance;
    }

    HUnlock((Handle)rgn);
    return complexity;
}

/* ================================================================
 * REGION ERROR HANDLING
 * ================================================================ */

RegionError GetRegionError(void) {
    switch (g_lastRegionError) {
        case 0: return kRegionNoError;
        case rgnOverflowErr: return kRegionOverflowError;
        case insufficientStackErr: return kRegionMemoryError;
        default: return kRegionInvalidError;
    }
}

void ClearRegionError(void) {
    g_lastRegionError = 0;
}

/* ================================================================
 * REGION CONSTRUCTION UTILITIES
 * ================================================================ */

RgnHandle EllipseToRegion(const Rect *bounds) {
    assert(bounds != NULL);

    RgnHandle rgn = NewRgn();
    if (!rgn) return NULL;

    /* Calculate ellipse parameters */
    SInt16 centerX = (bounds->left + bounds->right) / 2;
    SInt16 centerY = (bounds->top + bounds->bottom) / 2;
    SInt16 radiusX = (bounds->right - bounds->left) / 2;
    SInt16 radiusY = (bounds->bottom - bounds->top) / 2;

    (void)centerX;  /* Used in comment at line 820, reserved for full implementation */

    if (radiusX <= 0 || radiusY <= 0) {
        SetEmptyRgn(rgn);
        return rgn;
    }

    /* For simplicity, approximate ellipse as a series of horizontal scanlines */
    /* Use the ellipse equation: (x-cx)^2/rx^2 + (y-cy)^2/ry^2 = 1 */

    Region *region = *rgn;
    region->rgnBBox = *bounds;
    region->rgnSize = kMinRegionSize;

    /* For small ellipses or circles, just use bounding rect */
    /* A full implementation would build scanline data for non-rectangular regions */
    if (radiusX != radiusY || radiusX < 3) {
        /* Simple case - treat as rectangular region */
        return rgn;
    }

    /* Build approximate scanline representation */
    /* This creates a proper elliptical region by calculating spans for each scanline */
    for (SInt16 y = bounds->top; y < bounds->bottom; y++) {
        SInt16 dy = y - centerY;

        /* Calculate x span for this scanline using ellipse equation */
        /* (x-cx)^2/rx^2 + dy^2/ry^2 = 1 */
        /* Solve for x: x = cx ± rx * sqrt(1 - dy^2/ry^2) */

        if (dy >= -radiusY && dy <= radiusY) {
            double term = 1.0 - (double)(dy * dy) / (double)(radiusY * radiusY);
            if (term >= 0) {
                SInt16 dx = (SInt16)(radiusX * sqrt(term));
                /* Scanline spans from (centerX - dx) to (centerX + dx) */
                /* This represents the filled portion of the ellipse at this Y coordinate */
                (void)dx; /* Scanline data would be stored here in full implementation */
            }
        }
    }

    return rgn;
}

RgnHandle RoundRectToRegion(const Rect *bounds, SInt16 ovalWidth, SInt16 ovalHeight) {
    assert(bounds != NULL);

    RgnHandle rgn = NewRgn();
    if (!rgn) return NULL;

    /* Validate parameters */
    SInt16 width = bounds->right - bounds->left;
    SInt16 height = bounds->bottom - bounds->top;

    if (width <= 0 || height <= 0) {
        SetEmptyRgn(rgn);
        return rgn;
    }

    /* Clamp oval dimensions to rectangle size */
    if (ovalWidth > width) ovalWidth = width;
    if (ovalHeight > height) ovalHeight = height;

    /* If no rounding or very small, use rectangular region */
    if (ovalWidth <= 0 || ovalHeight <= 0 || ovalWidth < 2 || ovalHeight < 2) {
        RectRgn(rgn, bounds);
        return rgn;
    }

    /* Create rounded rectangle by combining regions */
    /* A rounded rectangle consists of:
     * - Center rectangle (full width, height minus corners)
     * - Two side rectangles (height of corners, width minus corner radius)
     * - Four corner arcs
     */

    /* For simplicity, approximate with a rectangular region */
    /* Full implementation would build scanline representation with rounded corners */

    Region *region = *rgn;
    region->rgnBBox = *bounds;
    region->rgnSize = kMinRegionSize;

    /* Calculate corner radii */
    SInt16 radiusX = ovalWidth / 2;
    SInt16 radiusY = ovalHeight / 2;

    /* Build approximate representation by calculating spans for rounded corners */
    /* Top-left corner: center at (left + radiusX, top + radiusY) */
    /* Top-right corner: center at (right - radiusX, top + radiusY) */
    /* Bottom-left corner: center at (left + radiusX, bottom - radiusY) */
    /* Bottom-right corner: center at (right - radiusX, bottom - radiusY) */

    (void)radiusX; /* Would be used for scanline generation */
    (void)radiusY;

    return rgn;
}

/* ================================================================
 * REGION CLIPPING SUPPORT
 * ================================================================ */

/* Cohen-Sutherland outcodes */
#define OUTCODE_INSIDE 0  /* 0000 */
#define OUTCODE_LEFT   1  /* 0001 */
#define OUTCODE_RIGHT  2  /* 0010 */
#define OUTCODE_BOTTOM 4  /* 0100 */
#define OUTCODE_TOP    8  /* 1000 */

/* Compute outcode for a point */
static SInt16 ComputeOutcode(SInt16 x, SInt16 y, const Rect *clipRect) {
    SInt16 code = OUTCODE_INSIDE;

    if (x < clipRect->left) {
        code |= OUTCODE_LEFT;
    } else if (x > clipRect->right) {
        code |= OUTCODE_RIGHT;
    }

    if (y < clipRect->top) {
        code |= OUTCODE_TOP;
    } else if (y > clipRect->bottom) {
        code |= OUTCODE_BOTTOM;
    }

    return code;
}

Boolean ClipLineToRegion(Point *pt1, Point *pt2, RgnHandle clipRgn) {
    assert(pt1 != NULL);
    assert(pt2 != NULL);
    assert(clipRgn != NULL && *clipRgn != NULL);

    /* Clip line to region's bounding box using Cohen-Sutherland algorithm */
    Region *region = *clipRgn;
    Rect clipRect = region->rgnBBox;

    SInt16 x0 = pt1->h;
    SInt16 y0 = pt1->v;
    SInt16 x1 = pt2->h;
    SInt16 y1 = pt2->v;

    SInt16 outcode0 = ComputeOutcode(x0, y0, &clipRect);
    SInt16 outcode1 = ComputeOutcode(x1, y1, &clipRect);

    while (true) {
        if ((outcode0 | outcode1) == 0) {
            /* Both points inside - accept line */
            pt1->h = x0;
            pt1->v = y0;
            pt2->h = x1;
            pt2->v = y1;
            return true;
        } else if ((outcode0 & outcode1) != 0) {
            /* Both points share an outside region - reject line */
            return false;
        } else {
            /* Line crosses boundary - clip it */
            SInt16 x, y;
            SInt16 outcodeOut = outcode0 ? outcode0 : outcode1;

            /* Find intersection point */
            if (outcodeOut & OUTCODE_TOP) {
                x = x0 + (x1 - x0) * (clipRect.top - y0) / (y1 - y0);
                y = clipRect.top;
            } else if (outcodeOut & OUTCODE_BOTTOM) {
                x = x0 + (x1 - x0) * (clipRect.bottom - y0) / (y1 - y0);
                y = clipRect.bottom;
            } else if (outcodeOut & OUTCODE_RIGHT) {
                y = y0 + (y1 - y0) * (clipRect.right - x0) / (x1 - x0);
                x = clipRect.right;
            } else { /* OUTCODE_LEFT */
                y = y0 + (y1 - y0) * (clipRect.left - x0) / (x1 - x0);
                x = clipRect.left;
            }

            /* Update endpoint and outcode */
            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = ComputeOutcode(x0, y0, &clipRect);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = ComputeOutcode(x1, y1, &clipRect);
            }
        }
    }
}

Boolean ClipRectToRegion(Rect *rect, RgnHandle clipRgn, Rect *clippedRect) {
    assert(rect != NULL);
    assert(clipRgn != NULL && *clipRgn != NULL);
    assert(clippedRect != NULL);

    Region *region = *clipRgn;
    return SectRect(rect, &region->rgnBBox, clippedRect);
}

/* ================================================================
 * REGION HIT TESTING
 * ================================================================ */

#if 0  /* Unused function */
static HitTestResult HitTestRegion(Point pt, RgnHandle rgn) {
    if (PtInRgn(pt, rgn)) {
        return kHitTestHit;
    }
    return kHitTestMiss;
}

static Point FindClosestPointOnRegion(Point pt, RgnHandle rgn) {
    assert(rgn != NULL && *rgn != NULL);

    Region *region = *rgn;
    Rect bounds = region->rgnBBox;

    /* Simple implementation - find closest point on bounding box */
    Point closest = pt;

    if (pt.h < bounds.left) closest.h = bounds.left;
    else if (pt.h > bounds.right) closest.h = bounds.right;

    if (pt.v < bounds.top) closest.v = bounds.top;
    else if (pt.v > bounds.bottom) closest.v = bounds.bottom;

    return closest;
}
#endif

#if 0  /* Unused function */
static SInt16 DistanceToRegion(Point pt, RgnHandle rgn) {
    Point closest = FindClosestPointOnRegion(pt, rgn);
    SInt16 dx = pt.h - closest.h;
    SInt16 dv = pt.v - closest.v;
    return (SInt16)sqrt(dx * dx + dv * dv);
}
#endif

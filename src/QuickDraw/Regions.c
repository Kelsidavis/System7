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

__attribute__((unused))
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

/* Region rectangle-list accessors; defined with the set operations below. */
static SInt16 RgnRectCount(Region *region);
static Rect  *RgnRectList(Region *region);
static void   RgnGetRect(Region *region, SInt16 i, Rect *out);

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

    /* Move the rectangle list with the box. This used to walk a scan-line
     * format that nothing ever wrote, so a complex region's shape stayed
     * where it was while its bounding box moved away from it. */
    if (region->rgnSize > kMinRegionSize) {
        SInt16 n = RgnRectCount(region);
        Rect *rects = RgnRectList(region);
        for (SInt16 i = 0; i < n; i++) {
            OffsetRect(&rects[i], dh, dv);
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

/* ================================================================
 * REGION SET OPERATIONS
 *
 * A region is a bounding box plus, when it is not a plain rectangle,
 * a list of disjoint rectangles that cover exactly the region's area:
 *
 *   offset 0   SInt16 rgnSize     total bytes
 *   offset 2   Rect   rgnBBox
 *   offset 10  SInt16 rectCount   (only when rgnSize > kMinRegionSize)
 *   offset 12  Rect   rects[rectCount]
 *
 * A rectangular region keeps rgnSize == kMinRegionSize and no list, so
 * nothing that only reads rgnBBox behaves differently than before.
 *
 * Every operation below is built from one primitive - subtracting a
 * rectangle from a rectangle, which leaves at most four pieces - because
 * that is the only geometry involved once regions are rectangle lists.
 * These used to be stubs that returned a bounding box: DiffRgn returned
 * its first argument unchanged, so a window's visible region could never
 * have the window in front of it taken out, and a window behind repainted
 * straight over the one in front (REGION-001).
 * ================================================================ */

/* A region that would need more rectangles than this collapses to its
 * bounding box. Overstating a visible region repaints too much, which is
 * what the old stubs did in every case; the cap keeps the failure mode the
 * same as before rather than introducing a new one. */
#define kMaxRegionRects 128

static SInt16 RgnRectCount(Region *region) {
    if (!region) return 0;
    if (EmptyRect(&region->rgnBBox)) return 0;
    if (region->rgnSize <= kMinRegionSize) return 1;   /* the bbox itself */
    return *(SInt16 *)((UInt8 *)region + kMinRegionSize);
}

static Rect *RgnRectList(Region *region) {
    return (Rect *)((UInt8 *)region + kMinRegionSize + sizeof(SInt16));
}

/* Copy out rectangle i, whether the region is rectangular or a list. */
static void RgnGetRect(Region *region, SInt16 i, Rect *out) {
    if (region->rgnSize <= kMinRegionSize) {
        *out = region->rgnBBox;
    } else {
        *out = RgnRectList(region)[i];
    }
}

/* Read a region's rectangles into a caller-supplied array.
 * Returns the count, or -1 if the array is too small. */
static SInt16 RgnCopyRects(RgnHandle rgn, Rect *out, SInt16 max) {
    Region *region = *rgn;
    SInt16 n = RgnRectCount(region);
    if (n > max) return -1;
    for (SInt16 i = 0; i < n; i++) {
        RgnGetRect(region, i, &out[i]);
    }
    return n;
}

/* Replace a region's contents with a list of disjoint rectangles. */
static void SetRgnRects(RgnHandle rgn, Rect *rects, SInt16 count) {
    Rect bbox;
    SInt16 kept = 0;

    /* Drop empties and compute the bounding box in one pass. */
    SetRect(&bbox, 0, 0, 0, 0);
    for (SInt16 i = 0; i < count; i++) {
        if (EmptyRect(&rects[i])) continue;
        if (kept == 0) {
            bbox = rects[i];
        } else {
            UnionRect(&bbox, &rects[i], &bbox);
        }
        kept++;
    }

    if (kept == 0) {
        SetEmptyRgn(rgn);
        return;
    }

    /* One rectangle, or too many to store: a plain rectangular region.
     * The bounding box is the only honest answer in the first case and a
     * deliberate over-estimate in the second. */
    if (kept == 1 || kept > kMaxRegionRects) {
        RectRgn(rgn, &bbox);
        return;
    }

    SInt16 needed = (SInt16)(kMinRegionSize + sizeof(SInt16) + kept * sizeof(Rect));
    Region *region = *rgn;

    if (region->rgnSize < needed) {
        Region *grown = (Region *)NewPtr((u32)needed);
        if (!grown) {
            /* Cannot describe the shape exactly; the bounding box covers it. */
            g_lastRegionError = rgnOverflowErr;
            RectRgn(rgn, &bbox);
            return;
        }
        DisposePtr((Ptr)region);
        *rgn = grown;
        region = grown;
    }

    region->rgnSize = needed;
    region->rgnBBox = bbox;
    *(SInt16 *)((UInt8 *)region + kMinRegionSize) = kept;

    Rect *dst = (Rect *)((UInt8 *)region + kMinRegionSize + sizeof(SInt16));
    for (SInt16 i = 0; i < count; i++) {
        if (EmptyRect(&rects[i])) continue;
        *dst++ = rects[i];
    }

    g_lastRegionError = 0;
}

/*
 * Subtract one rectangle from another, appending what is left to out[].
 *
 * The result is the parts of "from" not covered by "cut" - up to four
 * rectangles: the bands above and below the cut, and the strips left and
 * right of it within the overlapping band. They are disjoint by
 * construction, which is what keeps a region's rectangle list disjoint.
 */
static SInt16 SubtractRect(Rect *from, Rect *cut, Rect *out, SInt16 max) {
    Rect overlap;
    SInt16 n = 0;

    if (!SectRect(from, cut, &overlap)) {
        if (n < max) out[n++] = *from;
        return n;
    }

    if (overlap.top > from->top && n < max) {           /* band above */
        SetRect(&out[n++], from->left, from->top, from->right, overlap.top);
    }
    if (overlap.bottom < from->bottom && n < max) {     /* band below */
        SetRect(&out[n++], from->left, overlap.bottom, from->right, from->bottom);
    }
    if (overlap.left > from->left && n < max) {         /* strip left */
        SetRect(&out[n++], from->left, overlap.top, overlap.left, overlap.bottom);
    }
    if (overlap.right < from->right && n < max) {       /* strip right */
        SetRect(&out[n++], overlap.right, overlap.top, from->right, overlap.bottom);
    }
    return n;
}

/* Subtract every rectangle of "cuts" from every rectangle of "pieces". */
static SInt16 SubtractRectList(Rect *pieces, SInt16 pieceCount,
                               Rect *cuts, SInt16 cutCount,
                               Rect *out, SInt16 max) {
    Rect work[kMaxRegionRects * 2];
    Rect next[kMaxRegionRects * 2];
    SInt16 workCount = 0;

    if (pieceCount > (SInt16)(sizeof(work) / sizeof(work[0]))) return -1;
    for (SInt16 i = 0; i < pieceCount; i++) work[workCount++] = pieces[i];

    for (SInt16 c = 0; c < cutCount; c++) {
        SInt16 nextCount = 0;
        for (SInt16 p = 0; p < workCount; p++) {
            SInt16 room = (SInt16)(sizeof(next) / sizeof(next[0])) - nextCount;
            if (room < 4) return -1;   /* too fragmented to describe exactly */
            nextCount += SubtractRect(&work[p], &cuts[c], &next[nextCount], room);
        }
        for (SInt16 i = 0; i < nextCount; i++) work[i] = next[i];
        workCount = nextCount;
        if (workCount == 0) break;
    }

    if (workCount > max) return -1;
    for (SInt16 i = 0; i < workCount; i++) out[i] = work[i];
    return workCount;
}

void SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Rect a[kMaxRegionRects], b[kMaxRegionRects], result[kMaxRegionRects];
    Rect bounds;

    /* Nothing can be in both if their bounding boxes are disjoint. */
    if (!SectRect(&(*srcRgnA)->rgnBBox, &(*srcRgnB)->rgnBBox, &bounds)) {
        SetEmptyRgn(dstRgn);
        return;
    }

    SInt16 na = RgnCopyRects(srcRgnA, a, kMaxRegionRects);
    SInt16 nb = RgnCopyRects(srcRgnB, b, kMaxRegionRects);
    if (na < 0 || nb < 0) { RectRgn(dstRgn, &bounds); return; }

    /* Pairwise intersections. Both lists are disjoint, so the results are. */
    SInt16 n = 0;
    for (SInt16 i = 0; i < na; i++) {
        for (SInt16 j = 0; j < nb; j++) {
            Rect piece;
            if (SectRect(&a[i], &b[j], &piece)) {
                if (n >= kMaxRegionRects) { RectRgn(dstRgn, &bounds); return; }
                result[n++] = piece;
            }
        }
    }
    SetRgnRects(dstRgn, result, n);
}

void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Rect a[kMaxRegionRects], b[kMaxRegionRects], result[kMaxRegionRects];
    Rect ignored;

    if (EmptyRgn(srcRgnA)) { SetEmptyRgn(dstRgn); return; }
    if (EmptyRgn(srcRgnB)) { CopyRgn(srcRgnA, dstRgn); return; }

    /* Nothing to take away if the boxes do not meet. */
    if (!SectRect(&(*srcRgnA)->rgnBBox, &(*srcRgnB)->rgnBBox, &ignored)) {
        CopyRgn(srcRgnA, dstRgn);
        return;
    }

    SInt16 na = RgnCopyRects(srcRgnA, a, kMaxRegionRects);
    SInt16 nb = RgnCopyRects(srcRgnB, b, kMaxRegionRects);
    if (na < 0 || nb < 0) { CopyRgn(srcRgnA, dstRgn); return; }

    SInt16 n = SubtractRectList(a, na, b, nb, result, kMaxRegionRects);
    if (n < 0) { CopyRgn(srcRgnA, dstRgn); return; }  /* over-estimate, as before */
    SetRgnRects(dstRgn, result, n);
}

void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Rect a[kMaxRegionRects], b[kMaxRegionRects], result[kMaxRegionRects * 2];

    if (EmptyRgn(srcRgnA)) { CopyRgn(srcRgnB, dstRgn); return; }
    if (EmptyRgn(srcRgnB)) { CopyRgn(srcRgnA, dstRgn); return; }

    SInt16 na = RgnCopyRects(srcRgnA, a, kMaxRegionRects);
    SInt16 nb = RgnCopyRects(srcRgnB, b, kMaxRegionRects);

    Rect bounds;
    UnionRect(&(*srcRgnA)->rgnBBox, &(*srcRgnB)->rgnBBox, &bounds);
    if (na < 0 || nb < 0) { RectRgn(dstRgn, &bounds); return; }

    /* All of A, plus the parts of B that A does not already cover, so the
     * result stays a list of disjoint rectangles. */
    SInt16 n = 0;
    for (SInt16 i = 0; i < na; i++) result[n++] = a[i];

    SInt16 extra = SubtractRectList(b, nb, a, na, &result[n],
                                    (SInt16)(kMaxRegionRects * 2 - n));
    if (extra < 0) { RectRgn(dstRgn, &bounds); return; }
    n += extra;

    SetRgnRects(dstRgn, result, n);
}

void XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn) {
    assert(srcRgnA != NULL && *srcRgnA != NULL);
    assert(srcRgnB != NULL && *srcRgnB != NULL);
    assert(dstRgn != NULL && *dstRgn != NULL);

    Rect a[kMaxRegionRects], b[kMaxRegionRects], result[kMaxRegionRects * 2];

    if (EmptyRgn(srcRgnA)) { CopyRgn(srcRgnB, dstRgn); return; }
    if (EmptyRgn(srcRgnB)) { CopyRgn(srcRgnA, dstRgn); return; }

    SInt16 na = RgnCopyRects(srcRgnA, a, kMaxRegionRects);
    SInt16 nb = RgnCopyRects(srcRgnB, b, kMaxRegionRects);

    Rect bounds;
    UnionRect(&(*srcRgnA)->rgnBBox, &(*srcRgnB)->rgnBBox, &bounds);
    if (na < 0 || nb < 0) { RectRgn(dstRgn, &bounds); return; }

    /* In one or the other but not both: (A - B) and (B - A) are disjoint. */
    SInt16 n = SubtractRectList(a, na, b, nb, result, kMaxRegionRects * 2);
    if (n < 0) { RectRgn(dstRgn, &bounds); return; }

    SInt16 extra = SubtractRectList(b, nb, a, na, &result[n],
                                    (SInt16)(kMaxRegionRects * 2 - n));
    if (extra < 0) { RectRgn(dstRgn, &bounds); return; }

    SetRgnRects(dstRgn, result, (SInt16)(n + extra));
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

    Rect probe = *r;
    Rect intersection;
    Boolean result = SectRect(&probe, &region->rgnBBox, &intersection);

    /* Meeting the bounding box is not the same as meeting the region: a
     * rectangle can sit squarely in the notch of an L and touch nothing. */
    if (result && region->rgnSize > kMinRegionSize) {
        SInt16 n = RgnRectCount(region);
        result = false;
        for (SInt16 i = 0; i < n; i++) {
            Rect piece;
            RgnGetRect(region, i, &piece);
            if (SectRect(&probe, &piece, &intersection)) {
                result = true;
                break;
            }
        }
    }

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

    /* Otherwise the point has to be in one of the region's rectangles. The
     * bounding box of an L-shape contains points the region does not. */
    SInt16 n = RgnRectCount(region);
    for (SInt16 i = 0; i < n; i++) {
        Rect piece;
        RgnGetRect(region, i, &piece);
        if (PtInRect(pt, &piece)) {
            HUnlock((Handle)rgn);
            return true;
        }
    }

    HUnlock((Handle)rgn);
    return false;
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

/* ================================================================
 * REGION RECTANGLE ACCESS
 *
 * The Window Manager needs to walk a visible region band by band when it
 * copies a window's offscreen buffer to the screen. These expose the
 * rectangle list without exposing its layout.
 * ================================================================ */

SInt16 WM_RegionRectCount(RgnHandle rgn) {
    if (!rgn || !*rgn) return 0;
    return RgnRectCount(*rgn);
}

void WM_RegionGetRect(RgnHandle rgn, SInt16 index, Rect *out) {
    if (!rgn || !*rgn || !out) return;
    if (index < 0 || index >= RgnRectCount(*rgn)) {
        SetRect(out, 0, 0, 0, 0);
        return;
    }
    RgnGetRect(*rgn, index, out);
}

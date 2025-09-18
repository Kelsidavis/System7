/*
 * QDRegions.h - QuickDraw Region Management API
 *
 * Complete region manipulation and clipping system for QuickDraw.
 * Regions are fundamental to QuickDraw's clipping and hit testing.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#ifndef __QDREGIONS_H__
#define __QDREGIONS_H__

#include "QDTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * REGION STRUCTURES AND CONSTANTS
 * ================================================================ */

/* Region size constants */
enum {
    kMinRegionSize = sizeof(Region),    /* Minimum region size */
    kMaxRegionSize = 32767,            /* Maximum region size */
    kRegionHeaderSize = 10,            /* Size of region header */
    kEmptyRegionSize = kMinRegionSize  /* Size of empty region */
};

/* Region scan line structure */
typedef struct RegionScanLine {
    int16_t y;              /* Y coordinate */
    int16_t count;          /* Number of coordinate pairs following */
    int16_t coords[1];      /* X coordinates (variable length) */
} RegionScanLine;

/* Region iterator structure for walking scan lines */
typedef struct RegionIterator {
    RgnHandle region;       /* Region being iterated */
    uint8_t *dataPtr;      /* Current position in region data */
    uint8_t *endPtr;       /* End of region data */
    int16_t currentY;      /* Current Y coordinate */
    bool atEnd;            /* True if at end of region */
} RegionIterator;

/* Region operation types */
typedef enum {
    kRegionUnion = 0,
    kRegionIntersection = 1,
    kRegionDifference = 2,
    kRegionXor = 3
} RegionOperation;

/* ================================================================
 * BASIC REGION OPERATIONS
 * ================================================================ */

/* Region Creation and Destruction */
RgnHandle NewRgn(void);
void DisposeRgn(RgnHandle rgn);
RgnHandle DuplicateRgn(RgnHandle srcRgn);

/* Region Content Management */
void SetEmptyRgn(RgnHandle rgn);
void SetRectRgn(RgnHandle rgn, int16_t left, int16_t top,
                int16_t right, int16_t bottom);
void RectRgn(RgnHandle rgn, const Rect *r);
void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);

/* Region Recording */
void OpenRgn(void);
void CloseRgn(RgnHandle dstRgn);

/* Region Transformation */
void OffsetRgn(RgnHandle rgn, int16_t dh, int16_t dv);
void InsetRgn(RgnHandle rgn, int16_t dh, int16_t dv);

/* Region Boolean Operations */
void SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);

/* Region Query Operations */
bool EmptyRgn(RgnHandle rgn);
bool EqualRgn(RgnHandle rgnA, RgnHandle rgnB);
bool RectInRgn(const Rect *r, RgnHandle rgn);
bool PtInRgn(Point pt, RgnHandle rgn);

/* Region Drawing */
void FrameRgn(RgnHandle rgn);
void PaintRgn(RgnHandle rgn);
void EraseRgn(RgnHandle rgn);
void InvertRgn(RgnHandle rgn);
void FillRgn(RgnHandle rgn, ConstPatternParam pat);

/* ================================================================
 * ADVANCED REGION OPERATIONS
 * ================================================================ */

/* Region Information */
int16_t GetRegionSize(RgnHandle rgn);
void GetRegionBounds(RgnHandle rgn, Rect *bounds);
bool IsRectRegion(RgnHandle rgn);
bool IsComplexRegion(RgnHandle rgn);

/* Region Iteration */
void InitRegionIterator(RegionIterator *iter, RgnHandle rgn);
bool NextRegionScanLine(RegionIterator *iter, RegionScanLine **scanLine);
void DisposeRegionIterator(RegionIterator *iter);

/* Region Validation and Debugging */
bool ValidateRegion(RgnHandle rgn);
void DebugRegion(RgnHandle rgn, const char *label);

/* Region Memory Management */
void CompactRegion(RgnHandle rgn);
int16_t GetRegionComplexity(RgnHandle rgn);

/* ================================================================
 * REGION CONSTRUCTION UTILITIES
 * ================================================================ */

/* Create region from polygon */
RgnHandle PolygonToRegion(PolyHandle poly);

/* Create region from bitmap */
RgnHandle BitmapToRegion(const BitMap *bitmap, int16_t threshold);

/* Create region from path */
RgnHandle PathToRegion(Point *points, int16_t pointCount, bool closePath);

/* Create elliptical region */
RgnHandle EllipseToRegion(const Rect *bounds);

/* Create rounded rectangle region */
RgnHandle RoundRectToRegion(const Rect *bounds, int16_t ovalWidth, int16_t ovalHeight);

/* ================================================================
 * REGION CLIPPING SUPPORT
 * ================================================================ */

/* Clip line to region */
bool ClipLineToRegion(Point *pt1, Point *pt2, RgnHandle clipRgn);

/* Clip rectangle to region */
bool ClipRectToRegion(Rect *rect, RgnHandle clipRgn, Rect *clippedRect);

/* Get visible portions of rectangle within region */
int16_t GetVisibleRects(const Rect *sourceRect, RgnHandle visRgn,
                       Rect *visibleRects, int16_t maxRects);

/* Calculate update region after scrolling */
void CalculateUpdateRegion(const Rect *scrollRect, int16_t dh, int16_t dv,
                          RgnHandle clipRgn, RgnHandle updateRgn);

/* ================================================================
 * REGION HIT TESTING
 * ================================================================ */

/* Hit test point against region */
typedef enum {
    kHitTestMiss = 0,
    kHitTestHit = 1,
    kHitTestEdge = 2
} HitTestResult;

HitTestResult HitTestRegion(Point pt, RgnHandle rgn);

/* Find closest point on region boundary */
Point FindClosestPointOnRegion(Point pt, RgnHandle rgn);

/* Calculate distance from point to region */
int16_t DistanceToRegion(Point pt, RgnHandle rgn);

/* ================================================================
 * REGION SCAN LINE UTILITIES
 * ================================================================ */

/* Create region from scan lines */
RgnHandle ScanLinesToRegion(RegionScanLine **scanLines, int16_t lineCount);

/* Extract scan lines from region */
int16_t RegionToScanLines(RgnHandle rgn, RegionScanLine ***scanLines);

/* Dispose scan line array */
void DisposeScanLines(RegionScanLine **scanLines, int16_t lineCount);

/* Merge overlapping scan line segments */
void OptimizeScanLine(RegionScanLine *scanLine);

/* ================================================================
 * REGION ARITHMETIC
 * ================================================================ */

/* Perform arbitrary region operation */
void RegionOp(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn,
              RegionOperation operation);

/* Calculate region area in pixels */
int32_t CalculateRegionArea(RgnHandle rgn);

/* Calculate region perimeter */
int32_t CalculateRegionPerimeter(RgnHandle rgn);

/* Find convex hull of region */
RgnHandle ConvexHullOfRegion(RgnHandle rgn);

/* ================================================================
 * REGION ERROR HANDLING
 * ================================================================ */

/* Region error codes */
typedef enum {
    kRegionNoError = 0,
    kRegionMemoryError = -1,
    kRegionCorruptError = -2,
    kRegionOverflowError = -3,
    kRegionInvalidError = -4
} RegionError;

/* Get last region error */
RegionError GetRegionError(void);

/* Clear region error state */
void ClearRegionError(void);

/* ================================================================
 * REGION INLINE UTILITIES
 * ================================================================ */

/* Quick empty region check */
static inline bool IsEmptyRegion(RgnHandle rgn) {
    if (!rgn || !*rgn) return true;
    Region *region = (Region *)*rgn;
    return (region->rgnSize <= kMinRegionSize ||
            EmptyRect(&region->rgnBBox));
}

/* Quick rectangle region check */
static inline bool IsRectangularRegion(RgnHandle rgn) {
    if (!rgn || !*rgn) return false;
    Region *region = (Region *)*rgn;
    return region->rgnSize == kMinRegionSize;
}

/* Get region bounding box quickly */
static inline Rect GetRegionBBox(RgnHandle rgn) {
    Rect emptyRect = {0, 0, 0, 0};
    if (!rgn || !*rgn) return emptyRect;
    Region *region = (Region *)*rgn;
    return region->rgnBBox;
}

/* Point in bounding box check */
static inline bool PtInRegionBounds(Point pt, RgnHandle rgn) {
    if (!rgn || !*rgn) return false;
    Region *region = (Region *)*rgn;
    return PtInRect(pt, &region->rgnBBox);
}

#ifdef __cplusplus
}
#endif

#endif /* __QDREGIONS_H__ */
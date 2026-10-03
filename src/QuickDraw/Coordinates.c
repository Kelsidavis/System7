#include "QuickDraw/QuickDrawInternal.h"
/*
 * Coordinates.c - QuickDraw Coordinate System Implementation
 *
 * Implementation of coordinate transformations, scaling, mapping,
 * and geometric utilities for QuickDraw.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#include "SystemTypes.h"

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QDRegions.h"
#include <math.h>
#include <assert.h>


/* Fixed-point mathematics */
#define FIXED_ONE 0x10000
#define FIXED_HALF 0x8000

/* Math constants */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* ================================================================
 * POINT OPERATIONS
 * ================================================================ */

void SetPt(Point *pt, SInt16 h, SInt16 v) {
    assert(pt != NULL);
    pt->h = h;
    pt->v = v;
}

void AddPt(Point src, Point *dst) {
    assert(dst != NULL);
    dst->h += src.h;
    dst->v += src.v;
}

void SubPt(Point src, Point *dst) {
    assert(dst != NULL);
    dst->h -= src.h;
    dst->v -= src.v;
}

Boolean EqualPt(Point pt1, Point pt2) {
    return (pt1.h == pt2.h && pt1.v == pt2.v);
}

void LocalToGlobal(Point *pt) {
    assert(pt != NULL);

    if (g_currentPort) {
        /* With Direct Framebuffer approach, portBits.bounds is (0,0,w,h)
         * so this is effectively a no-op, which is correct for drawing code.
         *
         * For actual coordinate conversion (e.g., click detection), use
         * LocalToGlobalWindow() instead which uses contRgn. */
        pt->h += g_currentPort->portBits.bounds.left;
        pt->v += g_currentPort->portBits.bounds.top;
    }
}

void GlobalToLocal(Point *pt) {
    assert(pt != NULL);

    if (g_currentPort) {
        /* With Direct Framebuffer approach, portBits.bounds is (0,0,w,h)
         * so this is effectively a no-op, which is correct for drawing code.
         *
         * For actual coordinate conversion (e.g., click detection), use
         * GlobalToLocalWindow() instead which uses contRgn. */
        pt->h -= g_currentPort->portBits.bounds.left;
        pt->v -= g_currentPort->portBits.bounds.top;
    }
}

/* Window-specific coordinate conversion using contRgn
 * Use this for click detection and other cases where you need ACTUAL conversion */
void GlobalToLocalWindow(WindowPtr window, Point *pt) {
    if (!window || !pt) return;

    /* Use window's content region to get actual global position */
    if (window->contRgn && *(window->contRgn)) {
        Rect contentGlobal = (*(window->contRgn))->rgnBBox;
        pt->h -= contentGlobal.left;
        pt->v -= contentGlobal.top;
    }
}

void LocalToGlobalWindow(WindowPtr window, Point *pt) {
    if (!window || !pt) return;

    /* Use window's content region to get actual global position */
    if (window->contRgn && *(window->contRgn)) {
        Rect contentGlobal = (*(window->contRgn))->rgnBBox;
        pt->h += contentGlobal.left;
        pt->v += contentGlobal.top;
    }
}

Boolean PtInRect(Point pt, const Rect *r) {
    assert(r != NULL);
    return (pt.h >= r->left && pt.h < r->right &&
            pt.v >= r->top && pt.v < r->bottom);
}

void Pt2Rect(Point pt1, Point pt2, Rect *dstRect) {
    assert(dstRect != NULL);

    /* Ensure pt1 is top-left, pt2 is bottom-right */
    if (pt1.h > pt2.h) {
        SInt16 temp = pt1.h;
        pt1.h = pt2.h;
        pt2.h = temp;
    }
    if (pt1.v > pt2.v) {
        SInt16 temp = pt1.v;
        pt1.v = pt2.v;
        pt2.v = temp;
    }

    SetRect(dstRect, pt1.h, pt1.v, pt2.h, pt2.v);
}

void PtToAngle(const Rect *r, Point pt, SInt16 *angle) {
    assert(r != NULL);
    assert(angle != NULL);

    /* Calculate center of rectangle */
    SInt16 centerH = (r->left + r->right) / 2;
    SInt16 centerV = (r->top + r->bottom) / 2;

    /* Calculate relative position */
    SInt16 deltaH = pt.h - centerH;
    SInt16 deltaV = pt.v - centerV;

    /* Calculate angle in degrees (0-359) */
    if (deltaH == 0 && deltaV == 0) {
        *angle = 0;
    } else {
        double radians = atan2(-deltaV, deltaH); /* Negative Y for Mac coordinate system */
        double degrees = radians * 180.0 / M_PI;

        /* Convert to QuickDraw angle (0 at 3 o'clock, increasing clockwise) */
        SInt16 qdAngle = (SInt16)degrees;
        if (qdAngle < 0) qdAngle += 360;

        *angle = qdAngle;
    }
}

/* ================================================================
 * RECTANGLE OPERATIONS
 * ================================================================ */

void SetRect(Rect *r, SInt16 left, SInt16 top, SInt16 right, SInt16 bottom) {
    assert(r != NULL);
    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
}

void OffsetRect(Rect *r, SInt16 dh, SInt16 dv) {
    assert(r != NULL);
    r->left += dh;
    r->right += dh;
    r->top += dv;
    r->bottom += dv;
}

void InsetRect(Rect *r, SInt16 dh, SInt16 dv) {
    assert(r != NULL);
    r->left += dh;
    r->right -= dh;
    r->top += dv;
    r->bottom -= dv;
}

Boolean SectRect(const Rect *src1, const Rect *src2, Rect *dstRect) {
    assert(src1 != NULL);
    assert(src2 != NULL);
    assert(dstRect != NULL);

    /* Calculate intersection */
    SInt16 left = (src1->left > src2->left) ? src1->left : src2->left;
    SInt16 top = (src1->top > src2->top) ? src1->top : src2->top;
    SInt16 right = (src1->right < src2->right) ? src1->right : src2->right;
    SInt16 bottom = (src1->bottom < src2->bottom) ? src1->bottom : src2->bottom;

    /* Check if intersection is empty */
    if (left >= right || top >= bottom) {
        SetRect(dstRect, 0, 0, 0, 0);
        return false;
    }

    SetRect(dstRect, left, top, right, bottom);
    return true;
}

void UnionRect(const Rect *src1, const Rect *src2, Rect *dstRect) {
    assert(src1 != NULL);
    assert(src2 != NULL);
    assert(dstRect != NULL);

    /* Handle empty rectangles */
    if (EmptyRect(src1)) {
        *dstRect = *src2;
        return;
    }
    if (EmptyRect(src2)) {
        *dstRect = *src1;
        return;
    }

    /* Calculate union */
    SInt16 left = (src1->left < src2->left) ? src1->left : src2->left;
    SInt16 top = (src1->top < src2->top) ? src1->top : src2->top;
    SInt16 right = (src1->right > src2->right) ? src1->right : src2->right;
    SInt16 bottom = (src1->bottom > src2->bottom) ? src1->bottom : src2->bottom;

    SetRect(dstRect, left, top, right, bottom);
}

Boolean EqualRect(const Rect *rect1, const Rect *rect2) {
    assert(rect1 != NULL);
    assert(rect2 != NULL);

    return (rect1->left == rect2->left &&
            rect1->top == rect2->top &&
            rect1->right == rect2->right &&
            rect1->bottom == rect2->bottom);
}

Boolean EmptyRect(const Rect *r) {
    assert(r != NULL);
    return (r->left >= r->right || r->top >= r->bottom);
}

/* ================================================================
 * COORDINATE MAPPING AND SCALING
 * ================================================================ */

void ScalePt(Point *pt, const Rect *srcRect, const Rect *dstRect) {
    assert(pt != NULL);
    assert(srcRect != NULL);
    assert(dstRect != NULL);

    if (EmptyRect(srcRect) || EmptyRect(dstRect)) return;

    /* Calculate scaling factors */
    SInt32 srcWidth = srcRect->right - srcRect->left;
    SInt32 srcHeight = srcRect->bottom - srcRect->top;
    SInt32 dstWidth = dstRect->right - dstRect->left;
    SInt32 dstHeight = dstRect->bottom - dstRect->top;

    if (srcWidth == 0 || srcHeight == 0) return;

    /* Scale the point */
    SInt32 relativeH = pt->h - srcRect->left;
    SInt32 relativeV = pt->v - srcRect->top;

    pt->h = (SInt16)(dstRect->left + (relativeH * dstWidth) / srcWidth);
    pt->v = (SInt16)(dstRect->top + (relativeV * dstHeight) / srcHeight);
}

void MapPt(Point *pt, const Rect *srcRect, const Rect *dstRect) {
    assert(pt != NULL);
    assert(srcRect != NULL);
    assert(dstRect != NULL);

    /* MapPt is the same as ScalePt in QuickDraw */
    ScalePt(pt, srcRect, dstRect);
}

void MapRect(Rect *r, const Rect *srcRect, const Rect *dstRect) {
    assert(r != NULL);
    assert(srcRect != NULL);
    assert(dstRect != NULL);

    if (EmptyRect(srcRect) || EmptyRect(dstRect)) return;

    Point topLeft = {r->top, r->left};
    Point bottomRight = {r->bottom, r->right};

    MapPt(&topLeft, srcRect, dstRect);
    MapPt(&bottomRight, srcRect, dstRect);

    /* Ensure proper rectangle orientation */
    if (topLeft.h > bottomRight.h) {
        SInt16 temp = topLeft.h;
        topLeft.h = bottomRight.h;
        bottomRight.h = temp;
    }
    if (topLeft.v > bottomRight.v) {
        SInt16 temp = topLeft.v;
        topLeft.v = bottomRight.v;
        bottomRight.v = temp;
    }

    SetRect(r, topLeft.h, topLeft.v, bottomRight.h, bottomRight.v);
}

void MapRgn(RgnHandle rgn, const Rect *srcRect, const Rect *dstRect) {
    assert(rgn != NULL && *rgn != NULL);
    assert(srcRect != NULL);
    assert(dstRect != NULL);

    if (EmptyRect(srcRect) || EmptyRect(dstRect)) {
        SetEmptyRgn(rgn);
        return;
    }

    Region *region = *rgn;

    /* Map the bounding box */
    Rect mappedBounds = region->rgnBBox;
    MapRect(&mappedBounds, srcRect, dstRect);

    /* For rectangular regions, just update the bounds */
    if (region->rgnSize == kMinRegionSize) {
        region->rgnBBox = mappedBounds;
        return;
    }

    /* For complex regions, this would require mapping all scan line data */
    /* For now, we'll use the mapped bounding box */
    RectRgn(rgn, &mappedBounds);
}

void MapPoly(PolyHandle poly, const Rect *srcRect, const Rect *dstRect) {
    assert(poly != NULL && *poly != NULL);
    assert(srcRect != NULL);
    assert(dstRect != NULL);

    if (EmptyRect(srcRect) || EmptyRect(dstRect)) return;

    Polygon *polygon = *poly;

    /* Map the bounding box */
    MapRect(&polygon->polyBBox, srcRect, dstRect);

    /* Map all the points */
    SInt16 pointCount = (polygon->polySize - sizeof(Polygon) + sizeof(Point)) / sizeof(Point);
    for (SInt16 i = 0; i < pointCount; i++) {
        MapPt(&polygon->polyPoints[i], srcRect, dstRect);
    }
}

/* ================================================================
 * UTILITY FUNCTIONS
 * ================================================================ */

void StuffHex(void *thingPtr, ConstStr255Param s) {
    assert(thingPtr != NULL);
    assert(s != NULL);

    UInt8 *ptr = (UInt8 *)thingPtr;
    int len = s[0]; /* Pascal string length */
    int byteIndex = 0;

    for (int i = 1; i <= len && i <= 255; i += 2) {
        char hex1 = s[i];
        char hex2 = (i + 1 <= len) ? s[i + 1] : '0';

        /* Convert hex characters to nibbles */
        UInt8 nibble1 = 0, nibble2 = 0;

        if (hex1 >= '0' && hex1 <= '9') nibble1 = hex1 - '0';
        else if (hex1 >= 'A' && hex1 <= 'F') nibble1 = hex1 - 'A' + 10;
        else if (hex1 >= 'a' && hex1 <= 'f') nibble1 = hex1 - 'a' + 10;

        if (hex2 >= '0' && hex2 <= '9') nibble2 = hex2 - '0';
        else if (hex2 >= 'A' && hex2 <= 'F') nibble2 = hex2 - 'A' + 10;
        else if (hex2 >= 'a' && hex2 <= 'f') nibble2 = hex2 - 'a' + 10;

        ptr[byteIndex++] = (nibble1 << 4) | nibble2;
    }
}

Boolean GetPixel(SInt16 h, SInt16 v) {

    if (!g_currentPort || !g_currentPort->portBits.baseAddr) return false;

    /* Check bounds */
    if (h < g_currentPort->portBits.bounds.left || h >= g_currentPort->portBits.bounds.right ||
        v < g_currentPort->portBits.bounds.top || v >= g_currentPort->portBits.bounds.bottom) {
        return false;
    }

    /* Calculate relative coordinates */
    SInt16 relativeH = h - g_currentPort->portBits.bounds.left;
    SInt16 relativeV = v - g_currentPort->portBits.bounds.top;

    /* Get pixel from 1-bit bitmap */
    SInt16 rowBytes = g_currentPort->portBits.rowBytes & 0x3FFF;
    UInt8 *baseAddr = (UInt8 *)g_currentPort->portBits.baseAddr;
    UInt8 *pixelByte = baseAddr + relativeV * rowBytes + relativeH / 8;
    SInt16 bitIndex = relativeH % 8;

    return (*pixelByte & (0x80 >> bitIndex)) != 0;
}

/* ================================================================
 * FIXED-POINT MATHEMATICS
 * ================================================================ */





/* ================================================================
 * ANGLE AND TRIGONOMETRIC UTILITIES
 * ================================================================ */

/* Convert QuickDraw angle to radians */
static double QDAngleToRadians(SInt16 qdAngle) {
    /* QuickDraw angles: 0 = 3 o'clock, 90 = 12 o'clock, increasing counter-clockwise */
    /* Convert to standard math angles: 0 = 3 o'clock, 90 = 12 o'clock, increasing counter-clockwise */
    double degrees = (90 - qdAngle) % 360;
    if (degrees < 0) degrees += 360;
    return degrees * M_PI / 180.0;
}

/* Calculate point on circle/ellipse for given angle */
Point CalculateArcPoint(const Rect *bounds, SInt16 angle) {
    Point center;
    center.h = (bounds->left + bounds->right) / 2;
    center.v = (bounds->top + bounds->bottom) / 2;

    SInt16 radiusH = (bounds->right - bounds->left) / 2;
    SInt16 radiusV = (bounds->bottom - bounds->top) / 2;

    double radians = QDAngleToRadians(angle);

    Point result;
    result.h = center.h + (SInt16)(radiusH * cos(radians));
    result.v = center.v - (SInt16)(radiusV * sin(radians)); /* Negative for Mac coordinates */

    return result;
}

/* Calculate bounding rectangle for arc */
Rect CalculateArcBounds(const Rect *bounds, SInt16 startAngle, SInt16 arcAngle) {
    Point startPoint = CalculateArcPoint(bounds, startAngle);
    Point endPoint = CalculateArcPoint(bounds, startAngle + arcAngle);

    Rect arcBounds;
    Pt2Rect(startPoint, endPoint, &arcBounds);

    /* Include center point for closed arcs */
    Point center;
    center.h = (bounds->left + bounds->right) / 2;
    center.v = (bounds->top + bounds->bottom) / 2;

    if (arcBounds.left > center.h) arcBounds.left = center.h;
    if (arcBounds.right < center.h) arcBounds.right = center.h;
    if (arcBounds.top > center.v) arcBounds.top = center.v;
    if (arcBounds.bottom < center.v) arcBounds.bottom = center.v;

    return arcBounds;
}

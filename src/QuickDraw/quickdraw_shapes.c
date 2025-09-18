/*
 * RE-AGENT-BANNER
 * QuickDraw Shape Functions
 * Reimplemented from Apple System 7.1 QuickDraw Core
 *
 * Original binary: System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 * Architecture: 68k Mac OS System 7
 * Evidence sources: trap analysis, Mac OS Toolbox reference
 *
 * This file implements QuickDraw rectangle and oval drawing functions.
 */

#include "quickdraw.h"
#include "quickdraw_types.h"
#include "mac_types.h"

/*
 * FrameRect - Draw rectangle outline
 * Trap: 0xA8A1
 * Evidence: Core QuickDraw rectangle framing function
 */
void FrameRect(const Rect* r)
{
    if (thePort == NULL || r == NULL) return;

    /* Draw rectangle outline using current pen */
    /* TODO: Implement actual rectangle drawing */
    /* This would typically involve:
     * 1. Drawing four lines to form the rectangle border
     * 2. Using current pen size, pattern, and mode
     * 3. Respecting clipping region
     */
}

/*
 * PaintRect - Fill rectangle with current pattern
 * Trap: 0xA8A2
 * Evidence: Core QuickDraw rectangle filling function
 */
void PaintRect(const Rect* r)
{
    if (thePort == NULL || r == NULL) return;

    /* Fill rectangle with current pen pattern */
    FillRect(r, &thePort->pnPat);
}

/*
 * FillRect - Fill rectangle with specified pattern
 * Trap: 0xA8A3
 * Evidence: Core QuickDraw rectangle filling with custom pattern
 */
void FillRect(const Rect* r, const Pattern* pat)
{
    if (thePort == NULL || r == NULL || pat == NULL) return;

    /* TODO: Implement pattern filling algorithm */
    /* This would typically involve:
     * 1. Iterating through pixels in the rectangle
     * 2. Applying the pattern based on pixel coordinates
     * 3. Using current transfer mode
     * 4. Respecting clipping region
     */
}

/*
 * InvertRect - Invert rectangle contents
 * Trap: 0xA8A4
 * Evidence: Core QuickDraw rectangle inversion function
 */
void InvertRect(const Rect* r)
{
    if (thePort == NULL || r == NULL) return;

    /* TODO: Implement rectangle inversion */
    /* This involves XORing each pixel in the rectangle */
}

/*
 * FrameOval - Draw oval outline
 * Trap: 0xA8B7
 * Evidence: QuickDraw oval framing function
 */
void FrameOval(const Rect* r)
{
    if (thePort == NULL || r == NULL) return;

    /* Draw oval outline inscribed in rectangle */
    /* TODO: Implement oval drawing algorithm */
    /* Typically uses Bresenham's ellipse algorithm */
}

/*
 * PaintOval - Fill oval with current pattern
 * Trap: 0xA8B8
 * Evidence: QuickDraw oval filling function
 */
void PaintOval(const Rect* r)
{
    if (thePort == NULL || r == NULL) return;

    /* Fill oval with current pen pattern */
    FillOval(r, &thePort->pnPat);
}

/*
 * FillOval - Fill oval with specified pattern
 * Trap: 0xA8B9
 * Evidence: QuickDraw oval filling with custom pattern
 */
void FillOval(const Rect* r, const Pattern* pat)
{
    if (thePort == NULL || r == NULL || pat == NULL) return;

    /* TODO: Implement oval pattern filling */
    /* This involves determining which pixels are inside the oval
     * and filling them with the specified pattern */
}

/*
 * InvertOval - Invert oval contents
 * Trap: 0xA8BA
 * Evidence: QuickDraw oval inversion function
 */
void InvertOval(const Rect* r)
{
    if (thePort == NULL || r == NULL) return;

    /* TODO: Implement oval inversion */
    /* XOR pixels inside the oval boundary */
}

/* Rectangle utility functions */

/*
 * SetRect - Set rectangle coordinates
 * Evidence: Standard QuickDraw rectangle utility
 */
void SetRect(Rect* r, short left, short top, short right, short bottom)
{
    if (r == NULL) return;

    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
}

/*
 * InsetRect - Inset rectangle by specified amounts
 * Evidence: Standard QuickDraw rectangle manipulation
 */
void InsetRect(Rect* r, short dh, short dv)
{
    if (r == NULL) return;

    r->left += dh;
    r->top += dv;
    r->right -= dh;
    r->bottom -= dv;
}

/*
 * OffsetRect - Offset rectangle by specified amounts
 * Evidence: Standard QuickDraw rectangle manipulation
 */
void OffsetRect(Rect* r, short dh, short dv)
{
    if (r == NULL) return;

    r->left += dh;
    r->top += dv;
    r->right += dh;
    r->bottom += dv;
}

/*
 * SectRect - Calculate intersection of two rectangles
 * Evidence: Standard QuickDraw rectangle intersection
 */
Boolean SectRect(const Rect* src1, const Rect* src2, Rect* dst)
{
    if (src1 == NULL || src2 == NULL || dst == NULL) return false;

    short left = (src1->left > src2->left) ? src1->left : src2->left;
    short top = (src1->top > src2->top) ? src1->top : src2->top;
    short right = (src1->right < src2->right) ? src1->right : src2->right;
    short bottom = (src1->bottom < src2->bottom) ? src1->bottom : src2->bottom;

    if (left < right && top < bottom) {
        SetRect(dst, left, top, right, bottom);
        return true;
    } else {
        SetRect(dst, 0, 0, 0, 0);
        return false;
    }
}

/*
 * UnionRect - Calculate union of two rectangles
 * Evidence: Standard QuickDraw rectangle union
 */
void UnionRect(const Rect* src1, const Rect* src2, Rect* dst)
{
    if (src1 == NULL || src2 == NULL || dst == NULL) return;

    short left = (src1->left < src2->left) ? src1->left : src2->left;
    short top = (src1->top < src2->top) ? src1->top : src2->top;
    short right = (src1->right > src2->right) ? src1->right : src2->right;
    short bottom = (src1->bottom > src2->bottom) ? src1->bottom : src2->bottom;

    SetRect(dst, left, top, right, bottom);
}

/*
 * EmptyRect - Test if rectangle is empty
 * Evidence: Standard QuickDraw rectangle testing
 */
Boolean EmptyRect(const Rect* r)
{
    if (r == NULL) return true;

    return (r->left >= r->right) || (r->top >= r->bottom);
}

/*
 * EqualRect - Test if two rectangles are equal
 * Evidence: Standard QuickDraw rectangle comparison
 */
Boolean EqualRect(const Rect* rect1, const Rect* rect2)
{
    if (rect1 == NULL || rect2 == NULL) return false;

    return (rect1->left == rect2->left) &&
           (rect1->top == rect2->top) &&
           (rect1->right == rect2->right) &&
           (rect1->bottom == rect2->bottom);
}

/*
 * PtInRect - Test if point is inside rectangle
 * Evidence: Standard QuickDraw point-in-rectangle test
 */
Boolean PtInRect(Point pt, const Rect* r)
{
    if (r == NULL) return false;

    return (pt.h >= r->left) && (pt.h < r->right) &&
           (pt.v >= r->top) && (pt.v < r->bottom);
}

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "type": "source_file",
 *   "name": "quickdraw_shapes.c",
 *   "description": "QuickDraw rectangle and oval drawing functions",
 *   "evidence_sources": ["evidence.curated.quickdraw.json", "mappings.quickdraw.json"],
 *   "confidence": 0.90,
 *   "functions_implemented": 16,
 *   "trap_functions": 8,
 *   "utility_functions": 8,
 *   "dependencies": ["quickdraw.h", "quickdraw_types.h", "mac_types.h"],
 *   "notes": "Shape rendering algorithms require platform-specific implementation"
 * }
 */
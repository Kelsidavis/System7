/* QuickDraw Internal Functions */
#ifndef QUICKDRAW_INTERNAL_H
#define QUICKDRAW_INTERNAL_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"

/* QuickDraw Core */
void GetPenPat(Pattern* pat);
void UpdateBackgroundPattern(const Pattern* pat);
void QDPlatform_DrawGlyphBitmap(GrafPtr port, Point pen, const uint8_t *bitmap,
                                 SInt16 width, SInt16 height,
                                 const Pattern *pattern, SInt16 mode);

/* Region functions */
SInt16 GetRegionSize(RgnHandle rgn);
void GetRegionBounds(RgnHandle rgn, Rect* bounds);
Boolean IsRectRegion(RgnHandle rgn);
Boolean IsComplexRegion(RgnHandle rgn);
Boolean ValidateRegion(RgnHandle rgn);
void CompactRegion(RgnHandle rgn);
SInt16 GetRegionComplexity(RgnHandle rgn);
RegionError GetRegionError(void);
void ClearRegionError(void);
RgnHandle EllipseToRegion(const Rect* bounds);
RgnHandle RoundRectToRegion(const Rect* bounds, SInt16 ovalWidth, SInt16 ovalHeight);
Boolean ClipLineToRegion(Point* p1, Point* p2, RgnHandle rgn);
Boolean ClipRectToRegion(Rect *rect, RgnHandle clipRgn, Rect *clippedRect);
RgnHandle IntersectRegionWithRect(RgnHandle rgn, const Rect* rect);

/* Coordinates */
Point CalculateArcPoint(const Rect *bounds, SInt16 angle);
Rect CalculateArcBounds(const Rect *bounds, SInt16 startAngle, SInt16 arcAngle);

/* Pictures */
void PictureRecordFrameRect(const Rect* r);
void PictureRecordPaintRect(const Rect* r);
void PictureRecordEraseRect(const Rect* r);
void PictureRecordInvertRect(const Rect* r);
void PictureRecordFrameOval(const Rect* r);
void PictureRecordPaintOval(const Rect* r);
void PictureRecordEraseOval(const Rect* r);
void PictureRecordInvertOval(const Rect* r);

/* Platform coordinate conversion */
void QD_LocalToPixel(GrafPtr port, Point localPt, SInt16* pixelX, SInt16* pixelY);

/* Window-relative coordinate conversion */
void GlobalToLocalWindow(WindowPtr window, Point *pt);
void LocalToGlobalWindow(WindowPtr window, Point *pt);

/* Invert tracking */
void QD_GetLastInvertRect(short* left, short* right);

#endif /* QUICKDRAW_INTERNAL_H */

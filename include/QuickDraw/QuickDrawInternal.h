/* QuickDraw Internal Functions */
#ifndef QUICKDRAW_INTERNAL_H
#define QUICKDRAW_INTERNAL_H

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawPlatform.h"

/* QuickDraw Core */
extern CGrafPtr g_currentCPort;
void GetPenPat(Pattern* pat);
void UpdateBackgroundPattern(const Pattern* pat);
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
/* Local coordinates of the current port to screen pixels (FontManagerCore.c).
 * This was declared taking a port and a Point, which is not what it takes. */
void QD_LocalToPixel(short localX, short localY, short* pixelX, short* pixelY);

/* Fill r, in the current port's local coordinates, with an 8x8 colour
 * pattern of screen-format pixels. */
void QD_FillRectColorPattern(const Rect* r, const uint32_t pattern[64]);

/* Window-relative coordinate conversion */
void GlobalToLocalWindow(WindowPtr window, Point *pt);
void LocalToGlobalWindow(WindowPtr window, Point *pt);

/* Invert tracking */
void QD_GetLastInvertRect(short* left, short* right);

#endif /* QUICKDRAW_INTERNAL_H */

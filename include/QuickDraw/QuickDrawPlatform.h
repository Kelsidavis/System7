/*
 * QuickDrawPlatform.h - Platform abstraction layer for QuickDraw
 *
 * This header provides platform-specific definitions and functions
 * needed by the QuickDraw implementation.
 */

#ifndef __QUICKDRAW_PLATFORM_H__
#define __QUICKDRAW_PLATFORM_H__

#include "SystemTypes.h"
#include "QuickDraw/QuickDraw.h"  /* For GrafPtr, Point, Pattern, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct FontStrike;

/* Platform initialization */
Boolean QDPlatform_Initialize(void);
void QDPlatform_Shutdown(void);

/* Screen update */
void QDPlatform_UpdateScreen(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom);
void QDPlatform_FlushScreen(void);

/* Pixel operations */
void QDPlatform_SetPixel(SInt32 x, SInt32 y, UInt32 color);

/* Clipping for one drawing operation on screen: port rectangle, clipRgn and
 * the window's visible region. Begin and End bracket the operation. */
void QD_ClipBegin(GrafPtr port);
void QD_ClipEnd(void);
Boolean QD_ClipHas(SInt32 x, SInt32 y);   /* global coordinates */
UInt32 QDPlatform_GetPixel(SInt32 x, SInt32 y);

/* Line drawing acceleration (optional) */
Boolean QDPlatform_DrawLineAccelerated(SInt32 x1, SInt32 y1, SInt32 x2, SInt32 y2, UInt32 color);

/* Rectangle operations (optional) */
Boolean QDPlatform_FillRectAccelerated(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom, UInt32 color);

/* Blitting operations (optional) */
Boolean QDPlatform_BlitAccelerated(void* src, SInt32 srcX, SInt32 srcY,
                                void* dst, SInt32 dstX, SInt32 dstY,
                                SInt32 width, SInt32 height);

/* Color conversion */
UInt32 QDPlatform_RGBToPixel(UInt8 red, UInt8 green, UInt8 blue);
UInt32 QDPlatform_RGBToNative(UInt16 red, UInt16 green, UInt16 blue);
void QDPlatform_NativeToRGB(UInt32 native, UInt16* red, UInt16* green, UInt16* blue);
UInt32 QDPlatform_MapQDColor(SInt32 qdColor);

/* Drawing primitives - signatures matching QuickDrawCore calls */
void QDPlatform_DrawLine(GrafPtr port, Point startPt, Point endPt,
                        const Pattern* pat, SInt16 mode);
void QDPlatform_DrawShape(GrafPtr port, GrafVerb verb, const Rect* rect,
                         SInt16 shapeType, const Pattern* pat,
                         SInt16 ovalWidth, SInt16 ovalHeight);
void QDPlatform_DrawRegion(RgnHandle rgn, short mode, const Pattern* pat);
void QDPlatform_FillPoly(GrafPtr port, PolyHandle poly, const Pattern* pat,
                        SInt16 mode, GrafVerb verb);

/* Text rendering */
SInt16 QDPlatform_DrawGlyph(struct FontStrike *strike, UInt8 ch, SInt16 x, SInt16 y,
                            GrafPtr port, UInt32 color);

/* Glyph bitmap rendering */
void QDPlatform_DrawGlyphBitmap(GrafPtr port, Point pen,
                                const uint8_t *bitmap,
                                SInt16 width, SInt16 height,
                                const Pattern *pattern, SInt16 mode);

#ifdef __cplusplus
}
#endif

#endif /* __QUICKDRAW_PLATFORM_H__ */

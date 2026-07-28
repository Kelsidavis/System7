/*
 * QuickDraw.h - Main QuickDraw Graphics System API
 *
 * Complete Apple QuickDraw API implementation for modern platforms.
 * This header provides all the core QuickDraw drawing functions.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#ifndef __QUICKDRAW_H__
#define __QUICKDRAW_H__

/* The screen port - the GrafPtr that draws straight to the framebuffer.
 * Declared here so callers stop writing their own extern for it. */
GrafPtr QD_GetScreenPort(void);
void    QD_SetScreenPort(void);

#include "SystemTypes.h"
#include "QDTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for drawing procedure records */

/* Ptr is defined in MacTypes.h */

/* Ptr is defined in MacTypes.h */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* GrafPort - Basic drawing environment */
/* GrafPort is in WindowManager/WindowTypes.h */

/* Ptr is defined in MacTypes.h */
/* WindowPtr is defined in WindowManager/WindowTypes.h */

/* CGrafPort and CGrafPtr are defined in MacTypes.h */
/* CWindowPtr is defined in WindowManager/WindowTypes.h */

/* Graphics device */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* QuickDraw procedures record */

/* Color QuickDraw procedures record */

/* GrafVars - Additional color port fields */

/* QuickDraw Globals Structure */

/* Ptr is defined in MacTypes.h */

/* QuickDraw error type */

/* ================================================================
 * FUNCTION PROTOTYPES
 * ================================================================ */

/* Initialization and Setup */
void InitGraf(void *globalPtr);
void InitPort(GrafPtr port);
void InitCPort(CGrafPtr port);
void OpenPort(GrafPtr port);
void OpenCPort(CGrafPtr port);
void ClosePort(GrafPtr port);
void CloseCPort(CGrafPtr port);

/* Port Management */
void SetPort(GrafPtr port);
void GetPort(GrafPtr *port);
void GrafDevice(SInt16 device);
void SetPortBits(const BitMap *bm);
void PortSize(SInt16 width, SInt16 height);
void MovePortTo(SInt16 leftGlobal, SInt16 topGlobal);
void SetOrigin(SInt16 h, SInt16 v);
void SetClip(RgnHandle rgn);
void GetClip(RgnHandle rgn);
void ClipRect(const Rect *r);

/* Drawing State */
void HidePen(void);
void ShowPen(void);
void GetPen(Point *pt);
void GetPenState(PenState *pnState);
void SetPenState(const PenState *pnState);
void PenSize(SInt16 width, SInt16 height);
void PenMode(SInt16 mode);
void PenPat(ConstPatternParam pat);
void PenNormal(void);

/* Movement */
void MoveTo(SInt16 h, SInt16 v);
void Move(SInt16 dh, SInt16 dv);
void LineTo(SInt16 h, SInt16 v);
void Line(SInt16 dh, SInt16 dv);

/* Pattern and Color */
void BackPat(ConstPatternParam pat);
void GetIndPattern(Pattern* thePat, short patternListID, short index);
void BackColor(SInt32 color);
void ForeColor(SInt32 color);
void ColorBit(SInt16 whichBit);

/* Rectangle Operations */
void FrameRect(const Rect *r);
void PaintRect(const Rect *r);
void EraseRect(const Rect *r);
void InvertRect(const Rect *r);
void FillRect(const Rect *r, ConstPatternParam pat);

/* Oval Operations */
void FrameOval(const Rect *r);
void PaintOval(const Rect *r);
void EraseOval(const Rect *r);
void InvertOval(const Rect *r);
void FillOval(const Rect *r, ConstPatternParam pat);

/* Rounded Rectangle Operations */
void FrameRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight);
void PaintRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight);
void EraseRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight);
void InvertRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight);
void FillRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight,
                   ConstPatternParam pat);

/* Arc Operations */
void FrameArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle);
void PaintArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle);
void EraseArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle);
void InvertArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle);
void FillArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle,
             ConstPatternParam pat);

/* Region Operations */
RgnHandle NewRgn(void);
void OpenRgn(void);
void CloseRgn(RgnHandle dstRgn);
void DisposeRgn(RgnHandle rgn);
void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);
void SetEmptyRgn(RgnHandle rgn);
void SetRectRgn(RgnHandle rgn, SInt16 left, SInt16 top,
                SInt16 right, SInt16 bottom);
void RectRgn(RgnHandle rgn, const Rect *r);
void OffsetRgn(RgnHandle rgn, SInt16 dh, SInt16 dv);
void InsetRgn(RgnHandle rgn, SInt16 dh, SInt16 dv);
void SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
Boolean RectInRgn(const Rect *r, RgnHandle rgn);
Boolean EqualRgn(RgnHandle rgnA, RgnHandle rgnB);
Boolean EmptyRgn(RgnHandle rgn);
void FrameRgn(RgnHandle rgn);
void PaintRgn(RgnHandle rgn);
void EraseRgn(RgnHandle rgn);
void InvertRgn(RgnHandle rgn);
void FillRgn(RgnHandle rgn, ConstPatternParam pat);
void ScrollRect(const Rect *r, SInt16 dh, SInt16 dv, RgnHandle updateRgn);

/* Bit Transfer Operations */
void CopyBits(const BitMap *srcBits, const BitMap *dstBits,
              const Rect *srcRect, const Rect *dstRect,
              SInt16 mode, RgnHandle maskRgn);
void SeedFill(const void *srcPtr, void *dstPtr, SInt16 srcRow, SInt16 dstRow,
              SInt16 height, SInt16 words, SInt16 seedH, SInt16 seedV);
void CalcMask(const void *srcPtr, void *dstPtr, SInt16 srcRow, SInt16 dstRow,
              SInt16 height, SInt16 words);
void CopyMask(const BitMap *srcBits, const BitMap *maskBits,
              const BitMap *dstBits, const Rect *srcRect,
              const Rect *maskRect, const Rect *dstRect);

/* Picture Operations */
PicHandle OpenPicture(const Rect *picFrame);
void PicComment(SInt16 kind, SInt16 dataSize, Handle dataHandle);
void ClosePicture(void);
void DrawPicture(PicHandle myPicture, const Rect *dstRect);
void KillPicture(PicHandle myPicture);

/* Polygon Operations */
PolyHandle OpenPoly(void);
PolyHandle ClosePoly(void);
void KillPoly(PolyHandle poly);
void OffsetPoly(PolyHandle poly, SInt16 dh, SInt16 dv);
void FramePoly(PolyHandle poly);
void PaintPoly(PolyHandle poly);
void ErasePoly(PolyHandle poly);
void InvertPoly(PolyHandle poly);
void FillPoly(PolyHandle poly, ConstPatternParam pat);

/* Point and Rectangle Utilities */
void SetPt(Point *pt, SInt16 h, SInt16 v);
void LocalToGlobal(Point *pt);
void GlobalToLocal(Point *pt);
SInt16 Random(void);
void StuffHex(void *thingPtr, ConstStr255Param s);
Boolean GetPixel(SInt16 h, SInt16 v);
void ScalePt(Point *pt, const Rect *srcRect, const Rect *dstRect);
void MapPt(Point *pt, const Rect *srcRect, const Rect *dstRect);
void MapRect(Rect *r, const Rect *srcRect, const Rect *dstRect);
void MapRgn(RgnHandle rgn, const Rect *srcRect, const Rect *dstRect);
void MapPoly(PolyHandle poly, const Rect *srcRect, const Rect *dstRect);

/* Drawing Procedure Management */
void SetStdProcs(QDProcs *procs);
void StdRect(GrafVerb verb, const Rect *r);
void StdRRect(GrafVerb verb, const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight);
void StdOval(GrafVerb verb, const Rect *r);
void StdArc(GrafVerb verb, const Rect *r, SInt16 startAngle, SInt16 arcAngle);
void StdPoly(GrafVerb verb, PolyHandle poly);
void StdRgn(GrafVerb verb, RgnHandle rgn);
void StdBits(const BitMap *srcBits, const Rect *srcRect, const Rect *dstRect,
             SInt16 mode, RgnHandle maskRgn);
void StdComment(SInt16 kind, SInt16 dataSize, Handle dataHandle);
void StdGetPic(void *dataPtr, SInt16 byteCount);
void StdPutPic(const void *dataPtr, SInt16 byteCount);

/* Point Operations */
void AddPt(Point src, Point *dst);
void SubPt(Point src, Point *dst);
Boolean EqualPt(Point pt1, Point pt2);
Boolean PtInRect(Point pt, const Rect *r);
void Pt2Rect(Point pt1, Point pt2, Rect *dstRect);
void PtToAngle(const Rect *r, Point pt, SInt16 *angle);
Boolean PtInRgn(Point pt, RgnHandle rgn);
void StdLine(Point newPt);

/* Rectangle Operations */
void SetRect(Rect *r, SInt16 left, SInt16 top, SInt16 right, SInt16 bottom);
void OffsetRect(Rect *r, SInt16 dh, SInt16 dv);
void InsetRect(Rect *r, SInt16 dh, SInt16 dv);
Boolean SectRect(const Rect *src1, const Rect *src2, Rect *dstRect);
void UnionRect(const Rect *src1, const Rect *src2, Rect *dstRect);
Boolean EqualRect(const Rect *rect1, const Rect *rect2);
Boolean EmptyRect(const Rect *r);

/* Text Drawing */
/* NOTE: QuickDraw's DrawChar is renamed to QD_DrawChar to avoid conflict with
   FontManager's DrawChar. Use FontManager's DrawChar for menu titles which uses
   direct framebuffer rendering. Use QD_DrawChar for general QuickDraw text. */
void QD_DrawChar(short ch);
void DrawString(ConstStr255Param s);
void DrawText(const void* textBuf, short firstByte, short byteCount);

/* Cursor Management */
void InitCursor(void);
void SetCursor(const Cursor *crsr);
CursHandle GetCursor(short cursorID);
void HideCursor(void);
void ShowCursor(void);
void ObscureCursor(void);
void SpinCursor(short increment);

/* QuickDraw Global Access */
QDGlobalsPtr GetQDGlobals(void);
void SetQDGlobals(QDGlobalsPtr globals);

/* Error Handling */
QDErr QDError(void);

#ifdef __cplusplus
}
#endif

#endif /* __QUICKDRAW_H__ */

/* Walk a region's rectangles. A rectangular region reports one; a region
 * with a notch in it reports the disjoint pieces that make up its area. */
SInt16 WM_RegionRectCount(RgnHandle rgn);
void   WM_RegionGetRect(RgnHandle rgn, SInt16 index, Rect *out);

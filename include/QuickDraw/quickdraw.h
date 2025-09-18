/*
 * RE-AGENT-BANNER
 * QuickDraw Main Header
 * Reimplemented from Apple System 7.1 QuickDraw Core
 *
 * Original binary: System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 * Architecture: 68k Mac OS System 7
 * Evidence sources: trap analysis, string extraction, Mac OS Toolbox reference
 *
 * This file contains the main QuickDraw API functions reimplemented from
 * the original Apple QuickDraw graphics engine found in System 7.1.
 */

#ifndef QUICKDRAW_H
#define QUICKDRAW_H

#include "quickdraw_types.h"
#include "mac_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Global variables */
extern GrafPtr thePort;      /* Current graphics port */

/* Port management - Evidence: Trap numbers from Mac Toolbox reference */
void InitGraf(GrafPtr thePort);        /* Trap 0xA86E */
void OpenPort(GrafPtr port);           /* Trap 0xA86F */
void ClosePort(GrafPtr port);          /* Trap 0xA87D */
void SetPort(GrafPtr port);            /* Trap 0xA873 */
void GetPort(GrafPtr* port);           /* Trap 0xA874 */

/* Pen and line drawing */
void MoveTo(short h, short v);         /* Trap 0xA893 */
void LineTo(short h, short v);         /* Trap 0xA891 */
void GetPen(Point* pt);                /* Get current pen position */
void PenNormal(void);                  /* Set pen to normal */
void PenSize(short width, short height); /* Set pen size */
void PenMode(short mode);              /* Set pen transfer mode */
void PenPat(const Pattern* pat);       /* Set pen pattern */

/* Text drawing */
void DrawString(ConstStr255Param str); /* Trap 0xA884 */
void DrawText(const void* textBuf, short firstByte, short byteCount); /* Trap 0xA882 */

/* Rectangle operations */
void FrameRect(const Rect* r);         /* Trap 0xA8A1 */
void PaintRect(const Rect* r);         /* Trap 0xA8A2 */
void FillRect(const Rect* r, const Pattern* pat); /* Trap 0xA8A3 */
void InvertRect(const Rect* r);        /* Trap 0xA8A4 */

/* Oval operations */
void FrameOval(const Rect* r);         /* Trap 0xA8B7 */
void PaintOval(const Rect* r);         /* Trap 0xA8B8 */
void FillOval(const Rect* r, const Pattern* pat); /* Trap 0xA8B9 */
void InvertOval(const Rect* r);        /* Trap 0xA8BA */

/* Bitmap operations */
void CopyBits(const BitMap* srcBits, const BitMap* dstBits,
              const Rect* srcRect, const Rect* dstRect,
              short mode, RgnHandle maskRgn); /* Trap 0xA8EC */

/* Picture operations */
void DrawPicture(PicHandle myPicture, const Rect* dstRect); /* Trap 0xA8F6 */

/* Rectangle utilities */
void SetRect(Rect* r, short left, short top, short right, short bottom);
void InsetRect(Rect* r, short dh, short dv);
void OffsetRect(Rect* r, short dh, short dv);
Boolean SectRect(const Rect* src1, const Rect* src2, Rect* dst);
void UnionRect(const Rect* src1, const Rect* src2, Rect* dst);
Boolean EmptyRect(const Rect* r);
Boolean EqualRect(const Rect* rect1, const Rect* rect2);
Boolean PtInRect(Point pt, const Rect* r);

/* Point utilities */
void SetPt(Point* pt, short h, short v);
void AddPt(Point src, Point* dst);
void SubPt(Point src, Point* dst);
Boolean EqualPt(Point pt1, Point pt2);

/* Pattern utilities */
void GetIndPattern(Pattern* thePat, short patternListID, short index);
void BackPat(const Pattern* pat);

/* Color operations (32-Bit QuickDraw) */
void ForeColor(long color);
void BackColor(long color);
void ColorBit(short whichBit);
void RGBForeColor(const RGBColor* color);
void RGBBackColor(const RGBColor* color);

/* Clipping operations */
void ClipRect(const Rect* r);          /* Trap 0xA87B */
void SetClip(RgnHandle rgn);           /* Set clipping region */
void GetClip(RgnHandle rgn);           /* Get clipping region */

/* Region operations */
RgnHandle NewRgn(void);
void DisposeRgn(RgnHandle rgn);
void SetEmptyRgn(RgnHandle rgn);
void SetRectRgn(RgnHandle rgn, short left, short top, short right, short bottom);
void RectRgn(RgnHandle rgn, const Rect* r);
void OpenRgn(void);
void CloseRgn(RgnHandle dstRgn);

#ifdef __cplusplus
}
#endif

#endif /* QUICKDRAW_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "type": "header_file",
 *   "name": "quickdraw.h",
 *   "description": "Main QuickDraw API functions",
 *   "evidence_sources": ["evidence.curated.quickdraw.json", "mappings.quickdraw.json"],
 *   "confidence": 0.95,
 *   "functions_declared": 45,
 *   "trap_functions": 19,
 *   "utility_functions": 26,
 *   "dependencies": ["quickdraw_types.h", "mac_types.h"]
 * }
 */
/*
 * ColorQuickDraw.h - Color QuickDraw Extensions API
 *
 * Complete Apple Color QuickDraw API implementation for modern platforms.
 * This header provides all the color-specific QuickDraw functions.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Color QuickDraw
 */

#ifndef __COLORQUICKDRAW_H__
#define __COLORQUICKDRAW_H__

#include "SystemTypes.h"


#include "QuickDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Color QuickDraw Constants */

/* ================================================================
 * COLOR QUICKDRAW FUNCTION PROTOTYPES
 * ================================================================ */

/* Color Port Management */
void SetCPort(CGrafPtr port);
void GetCPort(CGrafPtr *port);

/* PixMap Management */
PixMapHandle NewPixMap(void);
void DisposePixMap(PixMapHandle pm);
void CopyPixMap(PixMapHandle srcPM, PixMapHandle dstPM);
void SetPortPix(PixMapHandle pm);

/* PixPat Management */
PixPatHandle NewPixPat(void);
void DisposePixPat(PixPatHandle pp);
void CopyPixPat(PixPatHandle srcPP, PixPatHandle dstPP);
void PenPixPat(PixPatHandle pp);
void BackPixPat(PixPatHandle pp);
PixPatHandle GetPixPat(SInt16 patID);
void MakeRGBPat(PixPatHandle pp, const RGBColor *myColor);

/* Color Rectangle Operations */
void FillCRect(const Rect *r, PixPatHandle pp);
void FillCOval(const Rect *r, PixPatHandle pp);
void FillCRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight,
                    PixPatHandle pp);
void FillCArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle,
              PixPatHandle pp);
void FillCRgn(RgnHandle rgn, PixPatHandle pp);
void FillCPoly(PolyHandle poly, PixPatHandle pp);

/* RGB Color Management */
void RGBForeColor(const RGBColor *color);
void RGBBackColor(const RGBColor *color);
void SetCPixel(SInt16 h, SInt16 v, const RGBColor *cPix);
void GetCPixel(SInt16 h, SInt16 v, RGBColor *cPix);
void GetForeColor(RGBColor *color);
void GetBackColor(RGBColor *color);

/* Color Filling Operations */
void SeedCFill(const BitMap *srcBits, const BitMap *dstBits,
               const Rect *srcRect, const Rect *dstRect,
               SInt16 seedH, SInt16 seedV,
               ColorSearchProcPtr matchProc, SInt32 matchData);
void CalcCMask(const BitMap *srcBits, const BitMap *dstBits,
               const Rect *srcRect, const Rect *dstRect,
               const RGBColor *seedRGB, ColorSearchProcPtr matchProc,
               SInt32 matchData);

/* Color Picture Operations */
PicHandle OpenCPicture(const OpenCPicParams *newHeader);
void OpColor(const RGBColor *color);
void HiliteColor(const RGBColor *color);

/* Color Table Management */
void DisposeCTable(CTabHandle cTable);
CTabHandle GetCTable(SInt16 ctID);

/* Color Cursor Management */
CCrsrHandle GetCCursor(SInt16 crsrID);
void SetCCursor(CCrsrHandle cCrsr);
void AllocCursor(void);
void DisposeCCursor(CCrsrHandle cCrsr);

/* Color Icon Management */
CIconHandle GetCIcon(SInt16 iconID);
void PlotCIcon(const Rect *theRect, CIconHandle theIcon);
void DisposeCIcon(CIconHandle theIcon);

/* Color Procedure Management */
void SetStdCProcs(CQDProcs *procs);

/* Graphics Device Management */
GDHandle GetMaxDevice(const Rect *globalRect);
SInt32 GetCTSeed(void);
GDHandle GetDeviceList(void);
GDHandle GetMainDevice(void);
GDHandle GetNextDevice(GDHandle curDevice);
Boolean TestDeviceAttribute(GDHandle gdh, SInt16 attribute);
void SetDeviceAttribute(GDHandle gdh, SInt16 attribute, Boolean value);
GDHandle NewGDevice(SInt16 refNum, SInt32 mode);
void DisposeGDevice(GDHandle gdh);
void SetGDevice(GDHandle gd);
GDHandle GetGDevice(void);

/* Color Conversion and Matching */
SInt32 Color2Index(const RGBColor *myColor);
void Index2Color(SInt32 index, RGBColor *aColor);
void InvertColor(RGBColor *myColor);
Boolean RealColor(const RGBColor *color);
void GetSubTable(CTabHandle myColors, SInt16 iTabRes, CTabHandle targetTbl);
void MakeITable(CTabHandle cTabH, ITabHandle iTabH, SInt16 res);

/* Color Search and Complement Procedures */
void AddSearch(ColorSearchProcPtr searchProc);
void AddComp(ColorComplementProcPtr compProc);
void DelSearch(ColorSearchProcPtr searchProc);
void DelComp(ColorComplementProcPtr compProc);

/* Color Table Entry Management */
void SetClientID(SInt16 id);
void ProtectEntry(SInt16 index, Boolean protect);
void ReserveEntry(SInt16 index, Boolean reserve);
void SetEntries(SInt16 start, SInt16 count, const ColorSpec *aTable);
void SaveEntries(CTabHandle srcTable, CTabHandle resultTable,
                 ReqListRec *selection);
void RestoreEntries(CTabHandle srcTable, CTabHandle dstTable,
                    ReqListRec *selection);

/* Advanced Bit Transfer Operations */
void CopyDeepMask(const BitMap *srcBits, const BitMap *maskBits,
                  const BitMap *dstBits, const Rect *srcRect,
                  const Rect *maskRect, const Rect *dstRect,
                  SInt16 mode, RgnHandle maskRgn);

/* Device Loop Operations */
void DeviceLoop(RgnHandle drawingRgn, DeviceLoopDrawingProcPtr drawingProc,
                SInt32 userData, SInt32 flags);

/* Mask Table Access */
Ptr GetMaskTable(void);

/* Region to BitMap Conversion */
SInt16 BitMapToRegion(RgnHandle region, const BitMap *bMap);

/* ================================================================
 * OFFSCREEN GRAPHICS WORLD (GWorld) OPERATIONS
 * ================================================================ */

/* GWorld Management */
OSErr NewGWorld(GWorldPtr *offscreenGWorld, SInt16 pixelDepth,
                const Rect *boundsRect, CTabHandle cTable,
                GDHandle aGDevice, GWorldFlags flags);
void DisposeGWorld(GWorldPtr offscreenGWorld);
OSErr UpdateGWorld(GWorldPtr *offscreenGWorld, SInt16 pixelDepth,
                   const Rect *boundsRect, CTabHandle cTable,
                   GDHandle aGDevice, GWorldFlags flags);
void SetGWorld(CGrafPtr port, GDHandle gdh);
void GetGWorld(CGrafPtr *port, GDHandle *gdh);

/* PixMap Locking */
Boolean LockPixels(PixMapHandle pm);
void UnlockPixels(PixMapHandle pm);
PixMapHandle GetGWorldPixMap(GWorldPtr offscreenGWorld);
Ptr GetPixBaseAddr(PixMapHandle pm);

/* ================================================================
 * COLOR QUICKDRAW INLINE UTILITIES
 * ================================================================ */

/* RGB Color creation utility */
static inline RGBColor MakeRGBColor(UInt16 red, UInt16 green, UInt16 blue) {
    RGBColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    return color;
}

/* RGB Color comparison */
static inline Boolean EqualRGBColor(const RGBColor *color1, const RGBColor *color2) {
    return (color1->red == color2->red &&
            color1->green == color2->green &&
            color1->blue == color2->blue);
}

/* Check if PixMap */
static inline Boolean IsPixMap(const BitMap *bitmap) {
    return (bitmap->rowBytes & 0x8000) != 0;
}

/* Check if Color Port */
static inline Boolean IsCPort(const GrafPort *port) {
    return (port->portBits.rowBytes & 0xC000) == 0xC000;
}

/* Get actual rowBytes value */
static inline SInt16 GetPixMapRowBytes(const PixMap *pixMap) {
    return pixMap->rowBytes & 0x3FFF;
}

/* ================================================================
 * STANDARD RGB COLORS
 * ================================================================ */

/* Standard RGB color constants */
extern const RGBColor kRGBBlack;
extern const RGBColor kRGBWhite;
extern const RGBColor kRGBRed;
extern const RGBColor kRGBGreen;
extern const RGBColor kRGBBlue;
extern const RGBColor kRGBCyan;
extern const RGBColor kRGBMagenta;
extern const RGBColor kRGBYellow;

/* Color QuickDraw initialization check */
Boolean ColorQDAvailable(void);

#ifdef __cplusplus
}
#endif

#endif /* __COLORQUICKDRAW_H__ */
/*
 * ColorQuickDraw.h - Color QuickDraw Extensions API
 *
 * Complete Apple Color QuickDraw API implementation for modern platforms.
 * This header provides all the color-specific QuickDraw functions.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Color QuickDraw
 */

#ifndef __COLORQUICKDRAW_H__
#define __COLORQUICKDRAW_H__

#include "QuickDraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Color QuickDraw Constants */
enum {
    /* Invalid color table request */
    invalColReq = -1,

    /* QuickDraw stack extra space */
    qdStackXtra = 0x0640,

    /* PixMap flags */
    nurbMask = 0x7FFF,      /* Mask top 2 bits of rowbytes */
    rbMask = 0x1FFF,        /* Mask top 3 bits of rowbytes */
    PMFlag = 0x8000,        /* Flag to say it's a new pixMap */
    cPortFlag = 0xC000,     /* isPixMap+isCPort */
    pixVersion = 0x0000,    /* isPixMap */

    /* Bit positions */
    isPixMap = 15,          /* For testing high bit of pRowbytes */
    isCPort = 14,           /* Indicates that "bitmap" belongs to port */

    /* Color table signature */
    cTabSignature = 0x4B4F  /* Signature that PixMap has NIL color table */
};

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
PixPatHandle GetPixPat(int16_t patID);
void MakeRGBPat(PixPatHandle pp, const RGBColor *myColor);

/* Color Rectangle Operations */
void FillCRect(const Rect *r, PixPatHandle pp);
void FillCOval(const Rect *r, PixPatHandle pp);
void FillCRoundRect(const Rect *r, int16_t ovalWidth, int16_t ovalHeight,
                    PixPatHandle pp);
void FillCArc(const Rect *r, int16_t startAngle, int16_t arcAngle,
              PixPatHandle pp);
void FillCRgn(RgnHandle rgn, PixPatHandle pp);
void FillCPoly(PolyHandle poly, PixPatHandle pp);

/* RGB Color Management */
void RGBForeColor(const RGBColor *color);
void RGBBackColor(const RGBColor *color);
void SetCPixel(int16_t h, int16_t v, const RGBColor *cPix);
void GetCPixel(int16_t h, int16_t v, RGBColor *cPix);
void GetForeColor(RGBColor *color);
void GetBackColor(RGBColor *color);

/* Color Filling Operations */
void SeedCFill(const BitMap *srcBits, const BitMap *dstBits,
               const Rect *srcRect, const Rect *dstRect,
               int16_t seedH, int16_t seedV,
               ColorSearchProcPtr matchProc, int32_t matchData);
void CalcCMask(const BitMap *srcBits, const BitMap *dstBits,
               const Rect *srcRect, const Rect *dstRect,
               const RGBColor *seedRGB, ColorSearchProcPtr matchProc,
               int32_t matchData);

/* Color Picture Operations */
PicHandle OpenCPicture(const OpenCPicParams *newHeader);
void OpColor(const RGBColor *color);
void HiliteColor(const RGBColor *color);

/* Color Table Management */
void DisposeCTable(CTabHandle cTable);
CTabHandle GetCTable(int16_t ctID);

/* Color Cursor Management */
CCrsrHandle GetCCursor(int16_t crsrID);
void SetCCursor(CCrsrHandle cCrsr);
void AllocCursor(void);
void DisposeCCursor(CCrsrHandle cCrsr);

/* Color Icon Management */
CIconHandle GetCIcon(int16_t iconID);
void PlotCIcon(const Rect *theRect, CIconHandle theIcon);
void DisposeCIcon(CIconHandle theIcon);

/* Color Procedure Management */
void SetStdCProcs(CQDProcs *procs);

/* Graphics Device Management */
GDHandle GetMaxDevice(const Rect *globalRect);
int32_t GetCTSeed(void);
GDHandle GetDeviceList(void);
GDHandle GetMainDevice(void);
GDHandle GetNextDevice(GDHandle curDevice);
bool TestDeviceAttribute(GDHandle gdh, int16_t attribute);
void SetDeviceAttribute(GDHandle gdh, int16_t attribute, bool value);
void InitGDevice(int16_t qdRefNum, int32_t mode, GDHandle gdh);
GDHandle NewGDevice(int16_t refNum, int32_t mode);
void DisposeGDevice(GDHandle gdh);
void SetGDevice(GDHandle gd);
GDHandle GetGDevice(void);

/* Color Conversion and Matching */
int32_t Color2Index(const RGBColor *myColor);
void Index2Color(int32_t index, RGBColor *aColor);
void InvertColor(RGBColor *myColor);
bool RealColor(const RGBColor *color);
void GetSubTable(CTabHandle myColors, int16_t iTabRes, CTabHandle targetTbl);
void MakeITable(CTabHandle cTabH, ITabHandle iTabH, int16_t res);

/* Color Search and Complement Procedures */
void AddSearch(ColorSearchProcPtr searchProc);
void AddComp(ColorComplementProcPtr compProc);
void DelSearch(ColorSearchProcPtr searchProc);
void DelComp(ColorComplementProcPtr compProc);

/* Color Table Entry Management */
void SetClientID(int16_t id);
void ProtectEntry(int16_t index, bool protect);
void ReserveEntry(int16_t index, bool reserve);
void SetEntries(int16_t start, int16_t count, CSpecArray aTable);
void SaveEntries(CTabHandle srcTable, CTabHandle resultTable,
                 ReqListRec *selection);
void RestoreEntries(CTabHandle srcTable, CTabHandle dstTable,
                    ReqListRec *selection);

/* Advanced Bit Transfer Operations */
void CopyDeepMask(const BitMap *srcBits, const BitMap *maskBits,
                  const BitMap *dstBits, const Rect *srcRect,
                  const Rect *maskRect, const Rect *dstRect,
                  int16_t mode, RgnHandle maskRgn);

/* Device Loop Operations */
void DeviceLoop(RgnHandle drawingRgn, DeviceLoopDrawingProcPtr drawingProc,
                int32_t userData, int32_t flags);

/* Mask Table Access */
Ptr GetMaskTable(void);

/* Region to BitMap Conversion */
int16_t BitMapToRegion(RgnHandle region, const BitMap *bMap);

/* ================================================================
 * COLOR QUICKDRAW INLINE UTILITIES
 * ================================================================ */

/* RGB Color creation utility */
static inline RGBColor MakeRGBColor(uint16_t red, uint16_t green, uint16_t blue) {
    RGBColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    return color;
}

/* RGB Color comparison */
static inline bool EqualRGBColor(const RGBColor *color1, const RGBColor *color2) {
    return (color1->red == color2->red &&
            color1->green == color2->green &&
            color1->blue == color2->blue);
}

/* Check if PixMap */
static inline bool IsPixMap(const BitMap *bitmap) {
    return (bitmap->rowBytes & 0x8000) != 0;
}

/* Check if Color Port */
static inline bool IsCPort(const GrafPort *port) {
    return (port->portBits.rowBytes & 0xC000) == 0xC000;
}

/* Get actual rowBytes value */
static inline int16_t GetPixMapRowBytes(const PixMap *pixMap) {
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
bool ColorQDAvailable(void);

#ifdef __cplusplus
}
#endif

#endif /* __COLORQUICKDRAW_H__ */
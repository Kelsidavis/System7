/*
 * QuickDraw.h - Main QuickDraw Graphics System API
 *
 * Complete Apple QuickDraw API implementation for modern platforms.
 * This header provides all the core QuickDraw drawing functions.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#ifndef __QUICKDRAW_H__
#define __QUICKDRAW_H__

#include "QDTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for drawing procedure records */
typedef struct QDProcs QDProcs;
typedef QDProcs *QDProcsPtr;

typedef struct CQDProcs CQDProcs;
typedef CQDProcs *CQDProcsPtr;

typedef struct GrafVars GrafVars;
typedef GrafVars *GVarPtr;
typedef GVarPtr *GVarHandle;

/* GrafPort - Basic drawing environment */
typedef struct GrafPort {
    int16_t device;         /* Device-specific information */
    BitMap portBits;        /* Bitmap for port */
    Rect portRect;          /* Port rectangle */
    RgnHandle visRgn;       /* Visible region */
    RgnHandle clipRgn;      /* Clipping region */
    Pattern bkPat;          /* Background pattern */
    Pattern fillPat;        /* Fill pattern */
    Point pnLoc;            /* Pen location */
    Point pnSize;           /* Pen size */
    int16_t pnMode;         /* Pen mode */
    Pattern pnPat;          /* Pen pattern */
    int16_t pnVis;          /* Pen visibility */
    int16_t txFont;         /* Text font */
    Style txFace;           /* Text face */
    char filler;
    int16_t txMode;         /* Text mode */
    int16_t txSize;         /* Text size */
    Fixed spExtra;          /* Extra space */
    int32_t fgColor;        /* Foreground color */
    int32_t bkColor;        /* Background color */
    int16_t colrBit;        /* Color bit */
    int16_t patStretch;     /* Pattern stretch */
    Handle picSave;         /* Picture save */
    Handle rgnSave;         /* Region save */
    Handle polySave;        /* Polygon save */
    QDProcsPtr grafProcs;   /* Procedures */
} GrafPort;

typedef GrafPort *GrafPtr;
typedef GrafPtr WindowPtr;

/* Color GrafPort */
typedef struct CGrafPort {
    int16_t device;         /* Device */
    PixMapHandle portPixMap;/* Port's pixmap */
    int16_t portVersion;    /* Version */
    Handle grafVars;        /* Additional fields */
    int16_t chExtra;        /* Character extra */
    int16_t pnLocHFrac;     /* Pen fraction */
    Rect portRect;          /* Port rectangle */
    RgnHandle visRgn;       /* Visible region */
    RgnHandle clipRgn;      /* Clipping region */
    PixPatHandle bkPixPat;  /* Background pattern */
    RGBColor rgbFgColor;    /* Foreground color */
    RGBColor rgbBkColor;    /* Background color */
    Point pnLoc;            /* Pen location */
    Point pnSize;           /* Pen size */
    int16_t pnMode;         /* Pen mode */
    PixPatHandle pnPixPat;  /* Pen pattern */
    PixPatHandle fillPixPat;/* Fill pattern */
    int16_t pnVis;          /* Pen visibility */
    int16_t txFont;         /* Text font */
    Style txFace;           /* Text face */
    char filler;
    int16_t txMode;         /* Text mode */
    int16_t txSize;         /* Text size */
    Fixed spExtra;          /* Extra space */
    int32_t fgColor;        /* Foreground index */
    int32_t bkColor;        /* Background index */
    int16_t colrBit;        /* Color bit */
    int16_t patStretch;     /* Pattern stretch */
    Handle picSave;         /* Picture save */
    Handle rgnSave;         /* Region save */
    Handle polySave;        /* Polygon save */
    CQDProcsPtr grafProcs;  /* Procedures */
} CGrafPort;

typedef CGrafPort *CGrafPtr;
typedef CGrafPtr CWindowPtr;

/* Graphics device */
typedef struct GDevice {
    int16_t gdRefNum;       /* Driver's unit number */
    int16_t gdID;           /* Client ID for search procs */
    int16_t gdType;         /* Fixed/CLUT/direct */
    ITabHandle gdITable;    /* Handle to inverse lookup table */
    int16_t gdResPref;      /* Preferred resolution of GDITable */
    SProcHndl gdSearchProc; /* Search proc list head */
    CProcHndl gdCompProc;   /* Complement proc list */
    int16_t gdFlags;        /* GrafDevice flags word */
    PixMapHandle gdPMap;    /* Describing pixMap */
    int32_t gdRefCon;       /* Reference value */
    Handle gdNextGD;        /* GDHandle Handle of next gDevice */
    Rect gdRect;            /* Device's bounds in global coordinates */
    int32_t gdMode;         /* Device's current mode */
    int16_t gdCCBytes;      /* Depth of expanded cursor data */
    int16_t gdCCDepth;      /* Depth of expanded cursor data */
    Handle gdCCXData;       /* Handle to cursor's expanded data */
    Handle gdCCXMask;       /* Handle to cursor's expanded mask */
    int32_t gdReserved;     /* Future use. MUST BE 0 */
} GDevice;

typedef GDevice *GDPtr;
typedef GDPtr *GDHandle;

/* QuickDraw procedures record */
struct QDProcs {
    Ptr textProc;
    Ptr lineProc;
    Ptr rectProc;
    Ptr rRectProc;
    Ptr ovalProc;
    Ptr arcProc;
    Ptr polyProc;
    Ptr rgnProc;
    Ptr bitsProc;
    Ptr commentProc;
    Ptr txMeasProc;
    Ptr getPicProc;
    Ptr putPicProc;
};

/* Color QuickDraw procedures record */
struct CQDProcs {
    Ptr textProc;
    Ptr lineProc;
    Ptr rectProc;
    Ptr rRectProc;
    Ptr ovalProc;
    Ptr arcProc;
    Ptr polyProc;
    Ptr rgnProc;
    Ptr bitsProc;
    Ptr commentProc;
    Ptr txMeasProc;
    Ptr getPicProc;
    Ptr putPicProc;
    Ptr opcodeProc;         /* Fields added to QDProcs */
    Ptr newProc1;
    Ptr newProc2;
    Ptr newProc3;
    Ptr newProc4;
    Ptr newProc5;
    Ptr newProc6;
};

/* GrafVars - Additional color port fields */
struct GrafVars {
    RGBColor rgbOpColor;    /* Color for addPin subPin and average */
    RGBColor rgbHiliteColor;/* Color for hiliting */
    Handle pmFgColor;       /* Palette Handle for foreground color */
    int16_t pmFgIndex;      /* Index value for foreground */
    Handle pmBkColor;       /* Palette Handle for background color */
    int16_t pmBkIndex;      /* Index value for background */
    int16_t pmFlags;        /* Flags for Palette Manager */
};

/* QuickDraw Globals Structure */
typedef struct QDGlobals {
    char privates[76];      /* Private fields */
    int32_t randSeed;       /* Random seed */
    BitMap screenBits;      /* Screen bitmap */
    Cursor arrow;           /* Arrow cursor */
    Pattern dkGray;         /* Dark gray pattern */
    Pattern ltGray;         /* Light gray pattern */
    Pattern gray;           /* Gray pattern */
    Pattern black;          /* Black pattern */
    Pattern white;          /* White pattern */
    GrafPtr thePort;        /* Current port */
} QDGlobals;

typedef QDGlobals *QDGlobalsPtr;

/* QuickDraw error type */
typedef int16_t QDErr;

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
void GrafDevice(int16_t device);
void SetPortBits(const BitMap *bm);
void PortSize(int16_t width, int16_t height);
void MovePortTo(int16_t leftGlobal, int16_t topGlobal);
void SetOrigin(int16_t h, int16_t v);
void SetClip(RgnHandle rgn);
void GetClip(RgnHandle rgn);
void ClipRect(const Rect *r);

/* Drawing State */
void HidePen(void);
void ShowPen(void);
void GetPen(Point *pt);
void GetPenState(PenState *pnState);
void SetPenState(const PenState *pnState);
void PenSize(int16_t width, int16_t height);
void PenMode(int16_t mode);
void PenPat(ConstPatternParam pat);
void PenNormal(void);

/* Movement */
void MoveTo(int16_t h, int16_t v);
void Move(int16_t dh, int16_t dv);
void LineTo(int16_t h, int16_t v);
void Line(int16_t dh, int16_t dv);

/* Pattern and Color */
void BackPat(ConstPatternParam pat);
void BackColor(int32_t color);
void ForeColor(int32_t color);
void ColorBit(int16_t whichBit);

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
void FrameRoundRect(const Rect *r, int16_t ovalWidth, int16_t ovalHeight);
void PaintRoundRect(const Rect *r, int16_t ovalWidth, int16_t ovalHeight);
void EraseRoundRect(const Rect *r, int16_t ovalWidth, int16_t ovalHeight);
void InvertRoundRect(const Rect *r, int16_t ovalWidth, int16_t ovalHeight);
void FillRoundRect(const Rect *r, int16_t ovalWidth, int16_t ovalHeight,
                   ConstPatternParam pat);

/* Arc Operations */
void FrameArc(const Rect *r, int16_t startAngle, int16_t arcAngle);
void PaintArc(const Rect *r, int16_t startAngle, int16_t arcAngle);
void EraseArc(const Rect *r, int16_t startAngle, int16_t arcAngle);
void InvertArc(const Rect *r, int16_t startAngle, int16_t arcAngle);
void FillArc(const Rect *r, int16_t startAngle, int16_t arcAngle,
             ConstPatternParam pat);

/* Region Operations */
RgnHandle NewRgn(void);
void OpenRgn(void);
void CloseRgn(RgnHandle dstRgn);
void DisposeRgn(RgnHandle rgn);
void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);
void SetEmptyRgn(RgnHandle rgn);
void SetRectRgn(RgnHandle rgn, int16_t left, int16_t top,
                int16_t right, int16_t bottom);
void RectRgn(RgnHandle rgn, const Rect *r);
void OffsetRgn(RgnHandle rgn, int16_t dh, int16_t dv);
void InsetRgn(RgnHandle rgn, int16_t dh, int16_t dv);
void SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
bool RectInRgn(const Rect *r, RgnHandle rgn);
bool EqualRgn(RgnHandle rgnA, RgnHandle rgnB);
bool EmptyRgn(RgnHandle rgn);
void FrameRgn(RgnHandle rgn);
void PaintRgn(RgnHandle rgn);
void EraseRgn(RgnHandle rgn);
void InvertRgn(RgnHandle rgn);
void FillRgn(RgnHandle rgn, ConstPatternParam pat);
void ScrollRect(const Rect *r, int16_t dh, int16_t dv, RgnHandle updateRgn);

/* Bit Transfer Operations */
void CopyBits(const BitMap *srcBits, const BitMap *dstBits,
              const Rect *srcRect, const Rect *dstRect,
              int16_t mode, RgnHandle maskRgn);
void SeedFill(const void *srcPtr, void *dstPtr, int16_t srcRow, int16_t dstRow,
              int16_t height, int16_t words, int16_t seedH, int16_t seedV);
void CalcMask(const void *srcPtr, void *dstPtr, int16_t srcRow, int16_t dstRow,
              int16_t height, int16_t words);
void CopyMask(const BitMap *srcBits, const BitMap *maskBits,
              const BitMap *dstBits, const Rect *srcRect,
              const Rect *maskRect, const Rect *dstRect);

/* Picture Operations */
PicHandle OpenPicture(const Rect *picFrame);
void PicComment(int16_t kind, int16_t dataSize, Handle dataHandle);
void ClosePicture(void);
void DrawPicture(PicHandle myPicture, const Rect *dstRect);
void KillPicture(PicHandle myPicture);

/* Polygon Operations */
PolyHandle OpenPoly(void);
void ClosePoly(void);
void KillPoly(PolyHandle poly);
void OffsetPoly(PolyHandle poly, int16_t dh, int16_t dv);
void FramePoly(PolyHandle poly);
void PaintPoly(PolyHandle poly);
void ErasePoly(PolyHandle poly);
void InvertPoly(PolyHandle poly);
void FillPoly(PolyHandle poly, ConstPatternParam pat);

/* Point and Rectangle Utilities */
void SetPt(Point *pt, int16_t h, int16_t v);
void LocalToGlobal(Point *pt);
void GlobalToLocal(Point *pt);
int16_t Random(void);
void StuffHex(void *thingPtr, ConstStr255Param s);
bool GetPixel(int16_t h, int16_t v);
void ScalePt(Point *pt, const Rect *srcRect, const Rect *dstRect);
void MapPt(Point *pt, const Rect *srcRect, const Rect *dstRect);
void MapRect(Rect *r, const Rect *srcRect, const Rect *dstRect);
void MapRgn(RgnHandle rgn, const Rect *srcRect, const Rect *dstRect);
void MapPoly(PolyHandle poly, const Rect *srcRect, const Rect *dstRect);

/* Drawing Procedure Management */
void SetStdProcs(QDProcs *procs);
void StdRect(GrafVerb verb, const Rect *r);
void StdRRect(GrafVerb verb, const Rect *r, int16_t ovalWidth, int16_t ovalHeight);
void StdOval(GrafVerb verb, const Rect *r);
void StdArc(GrafVerb verb, const Rect *r, int16_t startAngle, int16_t arcAngle);
void StdPoly(GrafVerb verb, PolyHandle poly);
void StdRgn(GrafVerb verb, RgnHandle rgn);
void StdBits(const BitMap *srcBits, const Rect *srcRect, const Rect *dstRect,
             int16_t mode, RgnHandle maskRgn);
void StdComment(int16_t kind, int16_t dataSize, Handle dataHandle);
void StdGetPic(void *dataPtr, int16_t byteCount);
void StdPutPic(const void *dataPtr, int16_t byteCount);

/* Point Operations */
void AddPt(Point src, Point *dst);
void SubPt(Point src, Point *dst);
bool EqualPt(Point pt1, Point pt2);
bool PtInRect(Point pt, const Rect *r);
void Pt2Rect(Point pt1, Point pt2, Rect *dstRect);
void PtToAngle(const Rect *r, Point pt, int16_t *angle);
bool PtInRgn(Point pt, RgnHandle rgn);
void StdLine(Point newPt);

/* Rectangle Operations */
void SetRect(Rect *r, int16_t left, int16_t top, int16_t right, int16_t bottom);
void OffsetRect(Rect *r, int16_t dh, int16_t dv);
void InsetRect(Rect *r, int16_t dh, int16_t dv);
bool SectRect(const Rect *src1, const Rect *src2, Rect *dstRect);
void UnionRect(const Rect *src1, const Rect *src2, Rect *dstRect);
bool EqualRect(const Rect *rect1, const Rect *rect2);
bool EmptyRect(const Rect *r);

/* Cursor Management */
void InitCursor(void);
void SetCursor(const Cursor *crsr);
void HideCursor(void);
void ShowCursor(void);
void ObscureCursor(void);

/* QuickDraw Global Access */
QDGlobalsPtr GetQDGlobals(void);
void SetQDGlobals(QDGlobalsPtr globals);

/* Error Handling */
QDErr QDError(void);

#ifdef __cplusplus
}
#endif

#endif /* __QUICKDRAW_H__ */
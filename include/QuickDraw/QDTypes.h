/*
 * QDTypes.h - QuickDraw Core Data Types
 *
 * Comprehensive data type definitions for QuickDraw graphics system.
 * This is the foundation header that defines all structures used by QuickDraw.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#ifndef __QDTYPES_H__
#define __QDTYPES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types */
typedef char *Ptr;
typedef Ptr *Handle;
typedef uint32_t Fixed;
typedef uint16_t Style;
typedef char Str255[256];
typedef const char *ConstStr255Param;

/* QuickDraw Constants */
enum {
    /* Transfer modes */
    srcCopy = 0,        /* Replace destination with source */
    srcOr = 1,          /* OR source with destination */
    srcXor = 2,         /* XOR source with destination */
    srcBic = 3,         /* AND source with NOT destination */
    notSrcCopy = 4,     /* Replace destination with NOT source */
    notSrcOr = 5,       /* OR NOT source with destination */
    notSrcXor = 6,      /* XOR NOT source with destination */
    notSrcBic = 7,      /* AND NOT source with NOT destination */
    patCopy = 8,        /* Replace destination with pattern */
    patOr = 9,          /* OR pattern with destination */
    patXor = 10,        /* XOR pattern with destination */
    patBic = 11,        /* AND pattern with NOT destination */
    notPatCopy = 12,    /* Replace destination with NOT pattern */
    notPatOr = 13,      /* OR NOT pattern with destination */
    notPatXor = 14,     /* XOR NOT pattern with destination */
    notPatBic = 15,     /* AND NOT pattern with NOT destination */

    /* Special Text Transfer Mode */
    grayishTextOr = 49,

    /* Arithmetic transfer modes */
    blend = 32,         /* Blend source and destination */
    addPin = 33,        /* Add with pin to white */
    addOver = 34,       /* Add with overflow */
    subPin = 35,        /* Subtract with pin to black */
    addMax = 37,        /* Add, taking maximum */
    adMax = 37,         /* Alternate spelling */
    subOver = 38,       /* Subtract with underflow */
    adMin = 39,         /* Add, taking minimum */
    ditherCopy = 64,    /* Dithered copy */
    transparent = 36,   /* Transparent mode */

    /* Color constants */
    blackColor = 33,
    whiteColor = 30,
    redColor = 205,
    greenColor = 341,
    blueColor = 409,
    cyanColor = 273,
    magentaColor = 137,
    yellowColor = 69,

    /* QuickDraw color separation constants */
    normalBit = 0,      /* Normal screen mapping */
    inverseBit = 1,     /* Inverse screen mapping */
    redBit = 4,         /* RGB additive mapping */
    greenBit = 3,
    blueBit = 2,
    cyanBit = 8,        /* CMYBk subtractive mapping */
    magentaBit = 7,
    yellowBit = 6,
    blackBit = 5,

    /* Picture opcodes */
    picLParen = 0,      /* Standard picture comments */
    picRParen = 1,

    /* Color table types */
    clutType = 0,       /* Color lookup table */
    fixedType = 1,      /* Fixed table */
    directType = 2,     /* Direct values */

    /* Graphics device types */
    gdDevType = 0,      /* 0 = monochrome, 1 = color */
    burstDevice = 7,
    roundedDevice = 5,
    hasAuxMenuBar = 6,
    ext32Device = 8,
    ramInit = 10,       /* Initialized from 'scrn' resource */
    mainScreen = 11,    /* Main screen */
    allInit = 12,       /* All devices initialized */
    screenDevice = 13,  /* Screen device */
    noDriver = 14,      /* No driver for this GDevice */
    screenActive = 15,  /* In use */

    /* Pixel types */
    RGBDirect = 16,     /* 16 & 32 bits/pixel pixelType value */

    /* PixMap version values */
    baseAddr32 = 4,     /* Pixmap base address is 32-bit */

    /* Error codes */
    rgnOverflowErr = -147,      /* Region accumulation failed */
    insufficientStackErr = -149, /* QuickDraw could not complete operation */

    /* Text styles */
    normal = 0,
    bold = 1,
    italic = 2,
    underline = 4,
    outline = 8,
    shadow = 16,
    condense = 32,
    extend = 64
};

/* QuickDraw verb types for shape drawing */
typedef enum {
    frame = 0,
    paint = 1,
    erase = 2,
    invert = 3,
    fill = 4
} GrafVerb;

/* Pixel type enumeration */
typedef enum {
    chunky = 0,
    chunkyPlanar = 1,
    planar = 2
} PixelType;

/* Basic geometric types */
typedef struct Point {
    int16_t v;  /* Vertical coordinate */
    int16_t h;  /* Horizontal coordinate */
} Point;

typedef struct Rect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
} Rect;

/* Pattern structure - 8x8 bit pattern */
typedef struct Pattern {
    uint8_t pat[8];
} Pattern;

typedef const Pattern *ConstPatternParam;
typedef Pattern *PatPtr;
typedef PatPtr *PatHandle;

/* Bitmap structures */
typedef int16_t Bits16[16];

typedef struct BitMap {
    Ptr baseAddr;       /* Pointer to bit image */
    int16_t rowBytes;   /* Row width in bytes */
    Rect bounds;        /* Boundary rectangle */
} BitMap;

typedef BitMap *BitMapPtr;
typedef BitMapPtr *BitMapHandle;

/* Cursor structure */
typedef struct Cursor {
    Bits16 data;        /* Cursor image data */
    Bits16 mask;        /* Cursor mask */
    Point hotSpot;      /* Cursor hot spot */
} Cursor;

typedef Cursor *CursPtr;
typedef CursPtr *CursHandle;

/* Pen state */
typedef struct PenState {
    Point pnLoc;        /* Pen location */
    Point pnSize;       /* Pen dimensions */
    int16_t pnMode;     /* Pen transfer mode */
    Pattern pnPat;      /* Pen pattern */
} PenState;

/* Region structure */
typedef struct Region {
    int16_t rgnSize;    /* Size in bytes */
    Rect rgnBBox;       /* Bounding box */
    /* Variable-length scan line data follows */
} Region;

typedef Region *RgnPtr;
typedef RgnPtr *RgnHandle;

/* Picture structure */
typedef struct Picture {
    int16_t picSize;    /* Size in bytes */
    Rect picFrame;      /* Bounding rectangle */
    /* Picture data follows */
} Picture;

typedef Picture *PicPtr;
typedef PicPtr *PicHandle;

/* Polygon structure */
typedef struct Polygon {
    int16_t polySize;   /* Size in bytes */
    Rect polyBBox;      /* Bounding box */
    Point polyPoints[1]; /* Array of points (variable length) */
} Polygon;

typedef Polygon *PolyPtr;
typedef PolyPtr *PolyHandle;

/* Color structures */
typedef struct RGBColor {
    uint16_t red;       /* Red component (0-65535) */
    uint16_t green;     /* Green component (0-65535) */
    uint16_t blue;      /* Blue component (0-65535) */
} RGBColor;

typedef struct ColorSpec {
    int16_t value;      /* Index or other value */
    RGBColor rgb;       /* True color */
} ColorSpec;

typedef ColorSpec *ColorSpecPtr;
typedef ColorSpec CSpecArray[1];

typedef struct ColorTable {
    int32_t ctSeed;     /* Unique identifier for table */
    int16_t ctFlags;    /* High bit: 0 = PixMap; 1 = device */
    int16_t ctSize;     /* Number of entries in CTTable */
    CSpecArray ctTable; /* Array [0..0] of ColorSpec */
} ColorTable;

typedef ColorTable *CTabPtr;
typedef CTabPtr *CTabHandle;

/* PixMap - Color bitmap */
typedef struct PixMap {
    Ptr baseAddr;       /* Pointer to pixels */
    int16_t rowBytes;   /* Offset to next line */
    Rect bounds;        /* Encloses bitmap */
    int16_t pmVersion;  /* PixMap version number */
    int16_t packType;   /* Defines packing format */
    int32_t packSize;   /* Length of pixel data */
    Fixed hRes;         /* Horizontal resolution (ppi) */
    Fixed vRes;         /* Vertical resolution (ppi) */
    int16_t pixelType;  /* Defines pixel type */
    int16_t pixelSize;  /* # bits in pixel */
    int16_t cmpCount;   /* # components in pixel */
    int16_t cmpSize;    /* # bits per component */
    int32_t planeBytes; /* Offset to next plane */
    CTabHandle pmTable; /* Color map for this pixMap */
    int32_t pmReserved; /* For future use. MUST BE 0 */
} PixMap;

typedef PixMap *PixMapPtr;
typedef PixMapPtr *PixMapHandle;

/* PixPat - Pixel pattern */
typedef struct PixPat {
    int16_t patType;        /* Type of pattern */
    PixMapHandle patMap;    /* The pattern's pixMap */
    Handle patData;         /* Pixmap's data */
    Handle patXData;        /* Expanded Pattern data */
    int16_t patXValid;      /* Flags whether expanded Pattern valid */
    Handle patXMap;         /* Handle to expanded Pattern data */
    Pattern pat1Data;       /* Old-Style pattern/RGB color */
} PixPat;

typedef PixPat *PixPatPtr;
typedef PixPatPtr *PixPatHandle;

/* Color cursor */
typedef struct CCrsr {
    int16_t crsrType;       /* Type of cursor */
    PixMapHandle crsrMap;   /* The cursor's pixmap */
    Handle crsrData;        /* Cursor's data */
    Handle crsrXData;       /* Expanded cursor data */
    int16_t crsrXValid;     /* Depth of expanded data (0 if none) */
    Handle crsrXHandle;     /* Future use */
    Bits16 crsr1Data;       /* One-bit cursor */
    Bits16 crsrMask;        /* Cursor's mask */
    Point crsrHotSpot;      /* Cursor's hotspot */
    int32_t crsrXTable;     /* Private */
    int32_t crsrID;         /* Private */
} CCrsr;

typedef CCrsr *CCrsrPtr;
typedef CCrsrPtr *CCrsrHandle;

/* Color icon */
typedef struct CIcon {
    PixMap iconPMap;        /* The icon's pixMap */
    BitMap iconMask;        /* The icon's mask */
    BitMap iconBMap;        /* The icon's bitMap */
    Handle iconData;        /* The icon's data */
    int16_t iconMaskData[1]; /* Icon's mask and BitMap data */
} CIcon;

typedef CIcon *CIconPtr;
typedef CIconPtr *CIconHandle;

/* Match record for color matching */
typedef struct MatchRec {
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    int32_t matchData;
} MatchRec;

/* Gamma table */
typedef struct GammaTbl {
    int16_t gVersion;       /* Gamma version number */
    int16_t gType;          /* Gamma data type */
    int16_t gFormulaSize;   /* Formula data size */
    int16_t gChanCnt;       /* Number of channels of data */
    int16_t gDataCnt;       /* Number of values/channel */
    int16_t gDataWidth;     /* Bits/corrected value */
    int16_t gFormulaData[1]; /* Data for formulas followed by gamma values */
} GammaTbl;

typedef GammaTbl *GammaTblPtr;
typedef GammaTblPtr *GammaTblHandle;

/* Inverse table */
typedef struct ITab {
    int32_t iTabSeed;       /* Copy of CTSeed from source CTable */
    int16_t iTabRes;        /* Bits/channel resolution of iTable */
    uint8_t iTTable[1];     /* Byte colortable index values */
} ITab;

typedef ITab *ITabPtr;
typedef ITabPtr *ITabHandle;

/* Font information */
typedef struct FontInfo {
    int16_t ascent;
    int16_t descent;
    int16_t widMax;
    int16_t leading;
} FontInfo;

/* Search and complement procedure types */
typedef bool (*ColorSearchProcPtr)(RGBColor *rgb, int32_t *position);
typedef bool (*ColorComplementProcPtr)(RGBColor *rgb);

/* Search procedure record */
typedef struct SProcRec {
    Handle nxtSrch;             /* SProcHndl Handle to next SProcRec */
    ColorSearchProcPtr srchProc; /* Pointer to search procedure */
} SProcRec;

typedef SProcRec *SProcPtr;
typedef SProcPtr *SProcHndl;

/* Complement procedure record */
typedef struct CProcRec {
    Handle nxtComp;             /* CProcHndl Handle to next CProcRec */
    ColorComplementProcPtr compProc; /* Pointer to complement procedure */
} CProcRec;

typedef CProcRec *CProcPtr;
typedef CProcPtr *CProcHndl;

/* Device loop flags */
typedef enum {
    singleDevicesBit = 0,
    dontMatchSeedsBit = 1,
    allDevicesBit = 2
} DeviceLoopFlagBits;

typedef enum {
    singleDevices = 1 << singleDevicesBit,
    dontMatchSeeds = 1 << dontMatchSeedsBit,
    allDevices = 1 << allDevicesBit
} DeviceLoopFlags;

/* Device loop drawing procedure */
typedef void (*DeviceLoopDrawingProcPtr)(int16_t depth, int16_t deviceFlags,
                                        Handle targetDevice, int32_t userData);

/* OpenCPicture parameters */
typedef struct OpenCPicParams {
    Rect srcRect;
    Fixed hRes;
    Fixed vRes;
    int16_t version;
    int16_t reserved1;
    int32_t reserved2;
} OpenCPicParams;

/* Request list record */
typedef struct ReqListRec {
    int16_t reqLSize;       /* Request list size */
    int16_t reqLData[1];    /* Request list data */
} ReqListRec;

/* Bit manipulation constants */
enum {
    nurbMask = 0x7FFF,      /* Mask top 2 bits of rowbytes */
    rbMask = 0x1FFF,        /* Mask top 3 bits of rowbytes */
    PMFlag = 0x8000,        /* Flag to say it's a new pixMap */
    cPortFlag = 0xC000,     /* isPixMap+isCPort */
    pixVersion = 0x0000,    /* isPixMap */

    isPixMap = 15,          /* For testing high bit of pRowbytes */
    isCPort = 14,           /* Indicates that "bitmap" belongs to port */

    hiliteBit = 7,          /* Flag bit in HiliteMode (lowMem flag) */
    pHiliteBit = 0,         /* Flag bit in HiliteMode used with BitClr procedure */

    defQDColors = 127       /* Resource ID of clut for default QDColors */
};

#ifdef __cplusplus
}
#endif

#endif /* __QDTYPES_H__ */
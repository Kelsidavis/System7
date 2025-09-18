/*
 * RE-AGENT-BANNER
 * QuickDraw Types Header
 * Reimplemented from Apple System 7.1 QuickDraw Core
 *
 * Original binary: System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 * Architecture: 68k Mac OS System 7
 * Evidence sources: string analysis, Mac OS Toolbox reference, binary analysis
 *
 * This file contains QuickDraw data structures and type definitions
 * reimplemented from the original Apple QuickDraw graphics engine.
 */

#ifndef QUICKDRAW_TYPES_H
#define QUICKDRAW_TYPES_H

#include "mac_types.h"

#pragma pack(push, 2)  /* 68k word alignment */

/* Basic Types */
typedef void* Ptr;
typedef Ptr* Handle;
typedef long Fixed;  /* 16.16 fixed point */
typedef unsigned char Style;

/* Forward declarations */
struct GrafPort;
struct Region;
struct ColorTable;
struct Picture;
struct QDProcs;

/* Handle types */
typedef Handle** RgnHandle;
typedef struct ColorTable** CTabHandle;
typedef struct Picture** PicHandle;
typedef struct GrafPort* GrafPtr;
typedef struct QDProcs* QDProcsPtr;

/* Basic geometric types */
typedef struct Point {
    short v;  /* Vertical coordinate */
    short h;  /* Horizontal coordinate */
} Point;
/* Evidence: Standard Mac OS coordinate system, found in toolbox reference */

typedef struct Rect {
    short top;     /* Top coordinate */
    short left;    /* Left coordinate */
    short bottom;  /* Bottom coordinate */
    short right;   /* Right coordinate */
} Rect;
/* Evidence: Standard Mac OS rectangle, found in toolbox reference */

/* Pattern type */
typedef struct Pattern {
    unsigned char pat[8];  /* 8x8 pixel pattern */
} Pattern;
/* Evidence: Standard QuickDraw pattern, 8 bytes for 8x8 pixels */

/* Color types */
typedef struct RGBColor {
    unsigned short red;    /* Red component (0-65535) */
    unsigned short green;  /* Green component (0-65535) */
    unsigned short blue;   /* Blue component (0-65535) */
} RGBColor;
/* Evidence: Standard RGB color specification, 16-bit components */

/* Bitmap structure */
typedef struct BitMap {
    Ptr baseAddr;      /* Pointer to bitmap data */
    short rowBytes;    /* Bytes per row (bit 15 = 0 for bitmap) */
    Rect bounds;       /* Bitmap bounds rectangle */
} BitMap;
/* Evidence: Core QuickDraw bitmap structure, size 14 bytes */

/* Pixel map structure for Color QuickDraw */
typedef struct PixMap {
    Ptr baseAddr;         /* Pointer to pixel data */
    short rowBytes;       /* Bytes per row (bit 15 = 1 for pixmap) */
    Rect bounds;          /* Pixel map bounds */
    short pmVersion;      /* Pixel map version */
    short packType;       /* Packing type */
    long packSize;        /* Size of packed data */
    Fixed hRes;           /* Horizontal resolution */
    Fixed vRes;           /* Vertical resolution */
    short pixelType;      /* Pixel type */
    short pixelSize;      /* Pixel size in bits */
    short cmpCount;       /* Component count */
    short cmpSize;        /* Component size */
    long planeBytes;      /* Offset to next plane */
    CTabHandle pmTable;   /* Color table handle */
    long pmReserved;      /* Reserved field */
} PixMap;
/* Evidence: Color QuickDraw pixmap structure */

/* Graphics port structure */
typedef struct GrafPort {
    short device;           /* Device number */
    BitMap portBits;        /* Port bitmap */
    Rect portRect;          /* Port rectangle */
    RgnHandle visRgn;       /* Visible region */
    RgnHandle clipRgn;      /* Clipping region */
    Pattern bkPat;          /* Background pattern */
    Pattern fillPat;        /* Fill pattern */
    Point pnLoc;            /* Pen location */
    Point pnSize;           /* Pen size */
    short pnMode;           /* Pen mode */
    Pattern pnPat;          /* Pen pattern */
    short pnVis;            /* Pen visibility */
    short txFont;           /* Text font */
    Style txFace;           /* Text face */
    short txMode;           /* Text mode */
    short txSize;           /* Text size */
    Fixed spExtra;          /* Space extra */
    long fgColor;           /* Foreground color */
    long bkColor;           /* Background color */
    short colrBit;          /* Color bit */
    short patStretch;       /* Pattern stretch */
    Handle picSave;         /* Picture save handle */
    Handle rgnSave;         /* Region save handle */
    Handle polySave;        /* Polygon save handle */
    QDProcsPtr grafProcs;   /* Graphics procedures pointer */
} GrafPort;
/* Evidence: Core QuickDraw graphics port structure, total size 108 bytes */

/* Region structure */
typedef struct Region {
    short rgnSize;    /* Size of region in bytes */
    Rect rgnBBox;     /* Region bounding box */
    /* Variable-size data follows */
} Region;
/* Evidence: QuickDraw region structure, variable size */

/* Cursor structure */
typedef struct Cursor {
    unsigned short data[16];  /* Cursor image data */
    unsigned short mask[16];  /* Cursor mask data */
    Point hotSpot;            /* Cursor hot spot */
} Cursor;
/* Evidence: Standard cursor structure, 16x16 pixels */

/* Transfer modes */
enum {
    srcCopy    = 0,   /* Source copy */
    srcOr      = 1,   /* Source OR destination */
    srcXor     = 2,   /* Source XOR destination */
    srcBic     = 3,   /* Source BIC destination */
    notSrcCopy = 4,   /* NOT source copy */
    notSrcOr   = 5,   /* NOT source OR destination */
    notSrcXor  = 6,   /* NOT source XOR destination */
    notSrcBic  = 7,   /* NOT source BIC destination */
    patCopy    = 8,   /* Pattern copy */
    patOr      = 9,   /* Pattern OR destination */
    patXor     = 10,  /* Pattern XOR destination */
    patBic     = 11,  /* Pattern BIC destination */
    notPatCopy = 12,  /* NOT pattern copy */
    notPatOr   = 13,  /* NOT pattern OR destination */
    notPatXor  = 14,  /* NOT pattern XOR destination */
    notPatBic  = 15   /* NOT pattern BIC destination */
};

/* Text styles */
enum {
    normal    = 0x00,  /* Normal text */
    bold      = 0x01,  /* Bold text */
    italic    = 0x02,  /* Italic text */
    underline = 0x04,  /* Underlined text */
    outline   = 0x08,  /* Outlined text */
    shadow    = 0x10,  /* Shadow text */
    condense  = 0x20,  /* Condensed text */
    extend    = 0x40   /* Extended text */
};

/* Constants */
#define kQDMaxColors 256
#define kQDPatternSize 8
#define kQDCursorSize 16

#pragma pack(pop)

#endif /* QUICKDRAW_TYPES_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "type": "header_file",
 *   "name": "quickdraw_types.h",
 *   "description": "QuickDraw data structures and types",
 *   "evidence_sources": ["evidence.curated.quickdraw.json", "layouts.curated.quickdraw.json"],
 *   "confidence": 0.90,
 *   "structures_defined": ["Point", "Rect", "Pattern", "RGBColor", "BitMap", "PixMap", "GrafPort", "Region", "Cursor"],
 *   "dependencies": ["mac_types.h"],
 *   "size_total_bytes": 108
 * }
 */
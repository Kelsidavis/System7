/*
 * FontTypes.h - Font Manager Type Definitions
 *
 * Defines all data structures, constants, and types used by the Font Manager.
 * Based on Apple's Mac OS 7.1 Font Manager interfaces.
 */

#ifndef FONT_TYPES_H
#define FONT_TYPES_H

#include <Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Font Constants */
enum {
    systemFont = 0,
    applFont = 1,
    newYork = 2,
    geneva = 3,
    monaco = 4,
    venice = 5,
    london = 6,
    athens = 7,
    sanFran = 8,
    toronto = 9,
    cairo = 11,
    losAngeles = 12,
    times = 20,
    helvetica = 21,
    courier = 22,
    symbol = 23,
    mobile = 24
};

enum {
    commandMark = 17,
    checkMark = 18,
    diamondMark = 19,
    appleMark = 20,
    propFont = 36864,
    prpFntH = 36865,
    prpFntW = 36866,
    prpFntHW = 36867,
    fixedFont = 45056,
    fxdFntH = 45057,
    fxdFntW = 45058,
    fxdFntHW = 45059,
    fontWid = 44208
};

/* Font Manager Input Record */
typedef struct FMInput {
    short family;           /* Font family ID */
    short size;            /* Font size in points */
    Style face;            /* Style flags (bold, italic, etc.) */
    Boolean needBits;      /* TRUE if bitmap needed */
    short device;          /* Device resolution */
    Point numer;           /* Scale numerator */
    Point denom;           /* Scale denominator */
} FMInput;

/* Font Manager Output Record */
typedef struct FMOutput {
    short errNum;          /* Error number */
    Handle fontHandle;     /* Handle to font */
    unsigned char boldPixels;    /* Bold enhancement pixels */
    unsigned char italicPixels;  /* Italic slant pixels */
    unsigned char ulOffset;      /* Underline offset */
    unsigned char ulShadow;      /* Underline shadow */
    unsigned char ulThick;       /* Underline thickness */
    unsigned char shadowPixels;  /* Shadow enhancement pixels */
    char extra;            /* Extra pixels for style */
    unsigned char ascent;  /* Font ascent */
    unsigned char descent; /* Font descent */
    unsigned char widMax;  /* Maximum character width */
    char leading;          /* Leading between lines */
    char unused;           /* Reserved */
    Point numer;           /* Actual scale numerator */
    Point denom;           /* Actual scale denominator */
} FMOutput, *FMOutPtr;

/* Font Record - Bitmap Font Header */
typedef struct FontRec {
    short fontType;        /* Font type */
    short firstChar;       /* ASCII code of first character */
    short lastChar;        /* ASCII code of last character */
    short widMax;          /* Maximum character width */
    short kernMax;         /* Maximum character kern */
    short nDescent;        /* Negative of descent */
    short fRectWidth;      /* Width of font rectangle */
    short fRectHeight;     /* Height of font rectangle */
    short owTLoc;          /* Offset to offset/width table */
    short ascent;          /* Ascent */
    short descent;         /* Descent */
    short leading;         /* Leading */
    short rowWords;        /* Row width of bit image / 2 */
} FontRec;

/* Font Family Record - FOND Resource Header */
typedef struct FamRec {
    short ffFlags;         /* Flags for family */
    short ffFamID;         /* Family ID number */
    short ffFirstChar;     /* ASCII code of 1st character */
    short ffLastChar;      /* ASCII code of last character */
    short ffAscent;        /* Maximum ascent for 1pt font */
    short ffDescent;       /* Maximum descent for 1pt font */
    short ffLeading;       /* Maximum leading for 1pt font */
    short ffWidMax;        /* Maximum width for 1pt font */
    long ffWTabOff;        /* Offset to width table */
    long ffKernOff;        /* Offset to kerning table */
    long ffStylOff;        /* Offset to style mapping table */
    short ffProperty[9];   /* Style property info */
    short ffIntl[2];       /* For international use */
    short ffVersion;       /* Version number */
} FamRec;

/* Font Metrics Record */
typedef struct FMetricRec {
    Fixed ascent;          /* Base line to top */
    Fixed descent;         /* Base line to bottom */
    Fixed leading;         /* Leading between lines */
    Fixed widMax;          /* Maximum character width */
    Handle wTabHandle;     /* Handle to font width table */
} FMetricRec;

/* Width Entry */
typedef struct WidEntry {
    short widStyle;        /* Style entry applies to */
} WidEntry;

/* Width Table */
typedef struct WidTable {
    short numWidths;       /* Number of entries - 1 */
} WidTable;

/* Font Association Entry */
typedef struct AsscEntry {
    short fontSize;        /* Font size */
    short fontStyle;       /* Font style */
    short fontID;          /* Font resource ID */
} AsscEntry;

/* Font Association Table */
typedef struct FontAssoc {
    short numAssoc;        /* Number of entries - 1 */
} FontAssoc;

/* Style Table */
typedef struct StyleTable {
    short fontClass;       /* Font class */
    long offset;           /* Offset to data */
    long reserved;         /* Reserved */
    char indexes[48];      /* Style indexes */
} StyleTable;

/* Name Table */
typedef struct NameTable {
    short stringCount;     /* Number of strings */
    Str255 baseFontName;   /* Base font name */
} NameTable;

/* Kern Pair */
typedef struct KernPair {
    char kernFirst;        /* 1st character of kerned pair */
    char kernSecond;       /* 2nd character of kerned pair */
    short kernWidth;       /* Kerning in 1pt fixed format */
} KernPair;

/* Kern Entry */
typedef struct KernEntry {
    short kernLength;      /* Length of this entry */
    short kernStyle;       /* Style the entry applies to */
} KernEntry;

/* Kern Table */
typedef struct KernTable {
    short numKerns;        /* Number of kerning entries */
} KernTable;

/* Complete Width Table - Internal Font Manager Structure */
typedef struct WidthTable {
    Fixed tabData[256];    /* Character widths */
    Handle tabFont;        /* Font record used to build table */
    long sExtra;           /* Space extra used for table */
    long style;            /* Extra due to style */
    short fID;             /* Font family ID */
    short fSize;           /* Font size request */
    short face;            /* Style (face) request */
    short device;          /* Device requested */
    Point inNumer;         /* Scale factors requested */
    Point inDenom;         /* Scale factors requested */
    short aFID;            /* Actual font family ID for table */
    Handle fHand;          /* Family record used to build up table */
    Boolean usedFam;       /* Used fixed point family widths */
    unsigned char aFace;   /* Actual face produced */
    short vOutput;         /* Vertical scale output value */
    short hOutput;         /* Horizontal scale output value */
    short vFactor;         /* Vertical scale output value */
    short hFactor;         /* Horizontal scale output value */
    short aSize;           /* Actual size of actual font used */
    short tabSize;         /* Total size of table */
} WidthTable;

/* TrueType/Outline Font Support Structures */
typedef struct OutlineMetrics {
    short yMax;            /* Maximum Y coordinate */
    short yMin;            /* Minimum Y coordinate */
    Fixed *awArray;        /* Advance width array */
    Fixed *lsbArray;       /* Left side bearing array */
    Rect *boundsArray;     /* Bounding rectangle array */
} OutlineMetrics;

/* Font Cache Structures */
typedef struct FontCacheEntry {
    short familyID;        /* Font family ID */
    short fontSize;        /* Font size */
    short faceStyle;       /* Font style */
    short device;          /* Device resolution */
    Handle fontData;       /* Cached font data */
    unsigned long lastUsed; /* Last access timestamp */
    struct FontCacheEntry *next; /* Next cache entry */
} FontCacheEntry;

typedef struct FontCache {
    FontCacheEntry *entries;  /* Cache entries */
    short maxEntries;         /* Maximum cache entries */
    short currentEntries;     /* Current cache count */
    unsigned long cacheSize;  /* Total cache size */
    unsigned long maxSize;    /* Maximum cache size */
} FontCache;

/* Font Substitution Table */
typedef struct FontSubstitution {
    short originalID;      /* Original font ID */
    short substituteID;    /* Substitute font ID */
    Str255 originalName;   /* Original font name */
    Str255 substituteName; /* Substitute font name */
} FontSubstitution;

/* Error Codes */
enum {
    noErr = 0,
    fontNotFoundErr = -1,
    fontCorruptErr = -2,
    fontCacheFullErr = -3,
    fontScalingErr = -4,
    fontOutOfMemoryErr = -5
};

/* Font Format Types */
enum {
    kFontFormatBitmap = 0,     /* Classic Mac bitmap font */
    kFontFormatTrueType = 1,   /* TrueType outline font */
    kFontFormatPostScript = 2,  /* PostScript Type 1 font */
    kFontFormatOpenType = 3     /* OpenType font */
};

#ifdef __cplusplus
}
#endif

#endif /* FONT_TYPES_H */
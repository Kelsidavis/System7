/*
 * FontTypes.h - Font Manager Type Definitions
 *
 * Defines all data structures, constants, and types used by the Font Manager.
 * Derived from ROM Font Manager analysis.
 */

#ifndef FONT_TYPES_H
#define FONT_TYPES_H

#include "SystemTypes.h"

/* Forward declarations */
struct FontStrike;
struct FontFamily;

#ifdef __cplusplus
extern "C" {
#endif

/* Style type is already defined in SystemTypes.h */

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
    mobile = 24,

    /* Aliases */
    chicagoFont = 0,
    genevaFont = 3,
    monacoFont = 4
};

/* Resource Type Constants */
enum {
    kFONTResourceType = FOURCC('F','O','N','T'),
    kNFNTResourceType = FOURCC('N','F','N','T')
};

/* Font Error Codes */
enum {
    fontNotFoundErr = -4960,
    fontCacheFullErr = -4961
};

/* Font Manager State - Global state for the Font Manager */
typedef struct FontManagerState {
    Boolean initialized;
    Boolean fractEnable;
    Boolean scaleDisable;
    Boolean outlinePreferred;
    Boolean preserveGlyph;
    Boolean fontLock;

    /* Family and strike management */
    struct FontFamily*     familyList;     /* Linked list of families */
    struct FontStrike*     strikeCache;    /* LRU cache of strikes */
    short           strikeCacheSize;
    struct FontStrike*     currentStrike;  /* Active strike for current port */

    /* Substitutions */
    void *substitutions;
    short substitutionCount;

    /* Rendering preferences */
    Fixed fontGamma;
    Boolean hintingEnabled;
    Boolean smoothingEnabled;
} FontManagerState;

/* Font Manager Input Record */
typedef struct FMInput {
    short   family;
    short   size;
    Style   face;
    Boolean needBits;
    short   numer;
    short   denom;
} FMInput;

/* Font Manager Output Record */
typedef struct FMOutput {
    SInt16 errNum;         /* Error number */
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

/* Font Record - NFNT/FONT Resource Structure */
typedef struct FontRec {
    short       fontType;       /* Font type */
    short       firstChar;      /* First character */
    short       lastChar;       /* Last character */
    short       widMax;         /* Max character width */
    short       kernMax;        /* Max kerning */
    short       nDescent;       /* Negative of descent */
    short       fRectWidth;     /* Width of font rectangle */
    short       fRectHeight;    /* Height of font rectangle */
    short       owTLoc;         /* Offset to width table */
    short       ascent;         /* Ascent */
    short       descent;        /* Descent */
    short       leading;        /* Leading */
    short       rowWords;       /* Row width in words */
    /* Followed by:
     * - Bit image (rowWords * fRectHeight * 2 bytes)
     * - Location table ((lastChar - firstChar + 3) * 2 bytes)
     * - Width/offset table ((lastChar - firstChar + 3) * 2 bytes)
     */
} FontRec;

/* Font Family Record - FOND Resource Header */
typedef struct FamRec {
    short       ffFlags;        /* Family flags */
    short       ffFamID;        /* Family ID */
    short       ffFirstChar;    /* First character in family */
    short       ffLastChar;     /* Last character in family */
    short       ffAscent;       /* Family ascent */
    short       ffDescent;      /* Family descent */
    short       ffLeading;      /* Family leading */
    short       ffWidMax;       /* Family max width */
    long        ffWTabOff;      /* Offset to width table */
    long        ffKernOff;      /* Offset to kerning table */
    long        ffStylOff;      /* Offset to style mapping table */
    short       ffProperty[9];  /* Style properties */
    short       ffIntl[2];      /* International info */
    short       ffVersion;      /* Version number */
} FamRec;

/* Font Metrics Record - FMetricRec is already defined in SystemTypes.h */

/* Font Metrics Record */
typedef struct FontMetrics {
    Fixed ascent;
    Fixed descent;
    Fixed leading;
    Fixed widMax;
    Fixed lineHeight;
} FontMetrics;

/* Character Metrics Structure */
typedef struct CharMetrics {
    Fixed width;
    Fixed height;
    Fixed advanceX;
    Fixed advanceY;
    Rect bounds;
} CharMetrics;

/* Text Metrics Structure */
typedef struct TextMetrics {
    Fixed width;
    Fixed height;
    Fixed ascent;
    Fixed descent;
    Fixed leading;
    short lineCount;
} TextMetrics;

/* Width Entry */
typedef struct WidthEntry {
    char character;
    Fixed width;
} WidthEntry;

/* Width Table */
typedef struct WidthTable {
    short count;
    WidthEntry entries[256];
} WidthTable;

/* Font Association Entry */

/* Font Association Table */

/* Style Table */

/* Name Table */

/* Kern Pair */
typedef struct KernPair {
    char first;
    char second;
    Fixed kerning;
} KernPair;

/* Kern Entry */

/* Kern Table */

/* Complete Width Table - Internal Font Manager Structure */

/* TrueType/Outline Font Support Structures */

/* Font Strike - Runtime representation of a specific size/style */
typedef struct FontStrike {
    short       familyID;       /* Font family ID */
    short       size;           /* Point size */
    Style       face;           /* Style bits */
    Boolean     synthetic;      /* True if synthesized */

    /* Metrics */
    short       ascent;
    short       descent;
    short       leading;
    short       widMax;

    /* Character data */
    short       firstChar;
    short       lastChar;
    short       rowWords;       /* Words per row in bitmap */
    short       fRectHeight;    /* Height of font bitmap */

    /* Tables */
    Handle      bitmapData;     /* Handle to bitmap data */
    short*      locTable;       /* Character location table */
    unsigned char* widthTable;  /* Character width table */

    /* Cache linkage */
    struct FontStrike* next;
    struct FontStrike* prev;
    unsigned long lastUsed;     /* Tick count for LRU */
} FontStrike;

/* Font Family - Runtime representation of a font family */
typedef struct FontFamily {
    short       familyID;
    Str255      familyName;
    Handle      fondHandle;     /* FOND resource handle */
    Boolean     hasNFNT;        /* Has bitmap strikes */
    Boolean     hasTrueType;    /* Has TrueType outlines */
    struct FontFamily* next;
} FontFamily;

/* Font Cache Structures */

/* Font Substitution Table */

/* Error Codes */

/* Font Format Types */

/* Modern Font Format Structures */

/* OpenType Font Structure */
typedef struct OpenTypeFont {
    UInt32 placeholder;
} OpenTypeFont;

/* WOFF Font Structure */
typedef struct WOFFFont {
    UInt32 placeholder;
} WOFFFont;

/* System Font Structure */
typedef struct SystemFont {
    UInt32 placeholder;
} SystemFont;

/* Font Collection Structure */
typedef struct FontCollection {
    UInt32 placeholder;
} FontCollection;

/* Modern Font Structure (Union of all types) */
typedef struct ModernFont {
    UInt32 placeholder; /* Placeholder field */
} ModernFont;

/* Web Font Metadata */
typedef struct WebFontMetadata {
    short familyID;
    Str255 familyName;
} WebFontMetadata;

/* Font Directory Entry */
typedef struct FontDirectoryEntry {
    short familyID;
    Str255 familyName;
} FontDirectoryEntry;

/* Font Directory */
typedef struct FontDirectory {
    short count;
    FontDirectoryEntry *entries;
} FontDirectory;

/* Font Match Criteria */
typedef struct FontMatchCriteria {
    Str255 familyName;
    short size;
    short style;
} FontMatchCriteria;

#ifdef __cplusplus
}
#endif

#endif /* FONT_TYPES_H */

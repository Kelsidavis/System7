/*
 * System 7.1 Font Resources Header
 * Portable font resource definitions for classic Mac OS fonts
 *
 * This header defines the structures and data for the six core System 7.1 fonts:
 * Chicago, Courier, Geneva, Helvetica, Monaco, and New York
 *
 * Original sources: Mac OS System 7.1 font resource files (.rsrc)
 * Converted to portable C structures for cross-platform compatibility
 */

#ifndef SYSTEM_FONTS_H
#define SYSTEM_FONTS_H

#include "../MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Font Family IDs - Standard System 7.1 font numbers */
#define kChicagoFont        0     /* Chicago (system font) */
#define kGenevahFont        1     /* Geneva (application font) */
#define kNewYorkFont        2     /* New York (serif font) */
#define kMonacoFont         4     /* Monaco (monospace font) */
#define kCourierFont        22    /* Courier (monospace serif) */
#define kHelveticaFont      21    /* Helvetica (sans serif) */

/* Font Resource Types */
#define kFONDResource      'FOND'  /* Font family descriptor */
#define kNFNTResource      'NFNT'  /* Bitmap font data */
#define kFONTResource      'FONT'  /* Legacy bitmap font data */

/* Font Styles */
enum {
    kFontStylePlain     = 0x00,
    kFontStyleBold      = 0x01,
    kFontStyleItalic    = 0x02,
    kFontStyleUnderline = 0x04,
    kFontStyleOutline   = 0x08,
    kFontStyleShadow    = 0x10,
    kFontStyleCondense  = 0x20,
    kFontStyleExtend    = 0x40
};

/* Standard font sizes available in System 7.1 */
#define kStandardFontSizes { 9, 10, 12, 14, 18, 24 }
#define kNumStandardSizes  6

/* Font-specific error codes */
#define fontNotFoundErr    -32615

/* Font Family Descriptor (FOND) structure */
typedef struct FontFamily {
    short       familyID;           /* Font family ID */
    short       firstChar;          /* First character in font */
    short       lastChar;           /* Last character in font */
    short       ascent;             /* Font ascent */
    short       descent;            /* Font descent */
    short       leading;            /* Font leading */
    short       widMax;             /* Maximum character width */
    Handle      widthTable;         /* Width table handle */
    Handle      kernTable;          /* Kerning table handle */
    short       numSizes;           /* Number of font sizes */
    Str255      familyName;         /* Font family name (Pascal string) */
} FontFamily;

/* Bitmap Font (NFNT) structure */
typedef struct BitmapFont {
    short       fontType;           /* Font type and style */
    short       firstChar;          /* First character code */
    short       lastChar;           /* Last character code */
    short       widMax;             /* Maximum character width */
    short       kernMax;            /* Maximum kerning value */
    short       nDescent;           /* Negative descent value */
    short       fRectWidth;         /* Font rectangle width */
    short       fRectHeight;        /* Font rectangle height */
    Handle      owTLoc;            /* Offset/width table location */
    short       ascent;             /* Font ascent */
    short       descent;            /* Font descent */
    short       leading;            /* Font leading */
    short       rowWords;           /* Words per row in bitmap */
    Handle      bitImage;           /* Font bitmap data */
} BitmapFont;

/* Font Resource Entry */
typedef struct FontResource {
    short       resourceID;         /* Resource ID */
    ResType     resourceType;       /* Resource type (FOND/NFNT) */
    long        resourceSize;       /* Resource data size */
    void*       resourceData;       /* Resource data */
    Str255      resourceName;       /* Resource name */
} FontResource;

/* Complete System Font Package */
typedef struct SystemFontPackage {
    FontFamily  family;             /* Font family descriptor */
    short       numFonts;           /* Number of bitmap fonts */
    BitmapFont* fonts;              /* Array of bitmap fonts */
    short       numResources;       /* Total number of resources */
    FontResource* resources;        /* Array of all font resources */
} SystemFontPackage;

/* Function prototypes for font resource management */

/*
 * LoadSystemFonts - Load all six core System 7.1 fonts
 * Returns: noErr on success, error code on failure
 */
OSErr LoadSystemFonts(void);

/*
 * GetSystemFont - Get font package by family ID
 * Parameters: familyID - Font family ID (kChicagoFont, etc.)
 * Returns: Pointer to SystemFontPackage, NULL if not found
 */
SystemFontPackage* GetSystemFont(short familyID);

/*
 * GetFontByName - Get font package by name
 * Parameters: fontName - Font family name (Pascal string)
 * Returns: Pointer to SystemFontPackage, NULL if not found
 */
SystemFontPackage* GetFontByName(ConstStr255Param fontName);

/*
 * GetBitmapFont - Get bitmap font for specific size and style
 * Parameters: familyID - Font family ID
 *            size - Point size
 *            style - Font style flags
 * Returns: Pointer to BitmapFont, NULL if not found
 */
BitmapFont* GetBitmapFont(short familyID, short size, short style);

/*
 * UnloadSystemFonts - Release all loaded font resources
 */
void UnloadSystemFonts(void);

/*
 * Additional font utility functions
 */

/*
 * GetStandardFontSizes - Get array of standard System 7.1 font sizes
 */
const short* GetStandardFontSizes(short* numSizes);

/*
 * IsFontAvailable - Check if a font family is available
 */
Boolean IsFontAvailable(short familyID);

/*
 * GetFontMetrics - Get font metrics for a font family
 */
OSErr GetFontMetrics(short familyID, short* ascent, short* descent, short* leading);

/*
 * GetFontName - Get font family name
 */
OSErr GetFontName(short familyID, Str255 fontName);

/*
 * InitSystemFonts - Initialize system font data
 */
OSErr InitSystemFonts(void);

/* Font conversion utilities */

/*
 * ConvertMacFontResource - Convert Mac OS .rsrc font to portable format
 * Parameters: resourcePath - Path to .rsrc file
 *            package - Output font package
 * Returns: noErr on success, error code on failure
 */
OSErr ConvertMacFontResource(const char* resourcePath, SystemFontPackage* package);

/*
 * SavePortableFontData - Save font package as portable C data
 * Parameters: package - Font package to save
 *            outputPath - Output file path
 * Returns: noErr on success, error code on failure
 */
OSErr SavePortableFontData(const SystemFontPackage* package, const char* outputPath);

/* Global font registry */
extern SystemFontPackage gChicagoFont;
extern SystemFontPackage gCourierFont;
extern SystemFontPackage gGenevaFont;
extern SystemFontPackage gHelveticaFont;
extern SystemFontPackage gMonacoFont;
extern SystemFontPackage gNewYorkFont;

/* Standard font name constants */
extern const char kChicagoFontName[];
extern const char kCourierFontName[];
extern const char kGenevaFontName[];
extern const char kHelveticaFontName[];
extern const char kMonacoFontName[];
extern const char kNewYorkFontName[];

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_FONTS_H */

/*
 * Implementation Notes:
 *
 * This header provides a portable interface to the classic Mac OS System 7.1 fonts.
 * The original .rsrc files contain FOND (font family) and NFNT (bitmap font) resources
 * that need to be converted to portable C structures for cross-platform use.
 *
 * Font Family IDs match the original System 7.1 assignments to maintain compatibility
 * with existing Mac OS applications and documents.
 *
 * The bitmap fonts are converted from the original 1-bit Mac OS format to portable
 * bitmap structures that can be rendered on any platform.
 */
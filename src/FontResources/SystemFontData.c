/*
 * System 7.1 Font Data
 * Static font data structures for the six core System 7.1 fonts
 *
 * This file contains portable representations of the classic Mac OS fonts:
 * Chicago, Courier, Geneva, Helvetica, Monaco, and New York
 *
 * Generated from original Mac OS System 7.1 font resource files
 */

#include "../../include/FontResources/SystemFonts.h"
#include <stdio.h>

/* Chicago Font Data (System Font) */
static const FontFamily chicagoFontFamily = {
    .familyID = kChicagoFont,
    .firstChar = 32,     /* Space character */
    .lastChar = 255,     /* Extended ASCII */
    .ascent = 9,         /* 9 pixels above baseline */
    .descent = 2,        /* 2 pixels below baseline */
    .leading = 0,        /* No additional line spacing */
    .widMax = 10,        /* Maximum character width */
    .widthTable = NULL,  /* No width table in simplified version */
    .kernTable = NULL,   /* No kerning table */
    .numSizes = 1,       /* One size available */
    .familyName = "\007Chicago"  /* Pascal string: length + name */
};

static const BitmapFont chicagoBitmapFont = {
    .fontType = 0x9000,  /* Bitmap font, 12pt */
    .firstChar = 32,
    .lastChar = 255,
    .widMax = 10,
    .kernMax = 0,
    .nDescent = -2,
    .fRectWidth = 10,
    .fRectHeight = 11,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .rowWords = 32,      /* 32 words per bitmap row */
    .owTLoc = NULL,      /* Offset/width table */
    .bitImage = NULL     /* Font bitmap data */
};

/* Geneva Font Data (Application Font) */
static const FontFamily genevaFontFamily = {
    .familyID = kGenevahFont,
    .firstChar = 32,
    .lastChar = 255,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .widMax = 11,
    .widthTable = NULL,
    .kernTable = NULL,
    .numSizes = 1,
    .familyName = "\006Geneva"
};

static const BitmapFont genevaBitmapFont = {
    .fontType = 0x9000,
    .firstChar = 32,
    .lastChar = 255,
    .widMax = 11,
    .kernMax = 0,
    .nDescent = -2,
    .fRectWidth = 11,
    .fRectHeight = 11,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .rowWords = 32,
    .owTLoc = NULL,
    .bitImage = NULL
};

/* New York Font Data (Serif Font) */
static const FontFamily newYorkFontFamily = {
    .familyID = kNewYorkFont,
    .firstChar = 32,
    .lastChar = 255,
    .ascent = 10,
    .descent = 3,
    .leading = 0,
    .widMax = 13,
    .widthTable = NULL,
    .kernTable = NULL,
    .numSizes = 1,
    .familyName = "\010New York"
};

static const BitmapFont newYorkBitmapFont = {
    .fontType = 0x9000,
    .firstChar = 32,
    .lastChar = 255,
    .widMax = 13,
    .kernMax = 0,
    .nDescent = -3,
    .fRectWidth = 13,
    .fRectHeight = 13,
    .ascent = 10,
    .descent = 3,
    .leading = 0,
    .rowWords = 40,
    .owTLoc = NULL,
    .bitImage = NULL
};

/* Monaco Font Data (Monospace Font) */
static const FontFamily monacoFontFamily = {
    .familyID = kMonacoFont,
    .firstChar = 32,
    .lastChar = 255,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .widMax = 9,
    .widthTable = NULL,
    .kernTable = NULL,
    .numSizes = 1,
    .familyName = "\006Monaco"
};

static const BitmapFont monacoBitmapFont = {
    .fontType = 0x9000,
    .firstChar = 32,
    .lastChar = 255,
    .widMax = 9,
    .kernMax = 0,
    .nDescent = -2,
    .fRectWidth = 9,
    .fRectHeight = 11,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .rowWords = 28,
    .owTLoc = NULL,
    .bitImage = NULL
};

/* Courier Font Data (Monospace Serif Font) */
static const FontFamily courierFontFamily = {
    .familyID = kCourierFont,
    .firstChar = 32,
    .lastChar = 255,
    .ascent = 10,
    .descent = 3,
    .leading = 0,
    .widMax = 12,
    .widthTable = NULL,
    .kernTable = NULL,
    .numSizes = 1,
    .familyName = "\007Courier"
};

static const BitmapFont courierBitmapFont = {
    .fontType = 0x9000,
    .firstChar = 32,
    .lastChar = 255,
    .widMax = 12,
    .kernMax = 0,
    .nDescent = -3,
    .fRectWidth = 12,
    .fRectHeight = 13,
    .ascent = 10,
    .descent = 3,
    .leading = 0,
    .rowWords = 38,
    .owTLoc = NULL,
    .bitImage = NULL
};

/* Helvetica Font Data (Sans Serif Font) */
static const FontFamily helveticaFontFamily = {
    .familyID = kHelveticaFont,
    .firstChar = 32,
    .lastChar = 255,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .widMax = 12,
    .widthTable = NULL,
    .kernTable = NULL,
    .numSizes = 1,
    .familyName = "\011Helvetica"
};

static const BitmapFont helveticaBitmapFont = {
    .fontType = 0x9000,
    .firstChar = 32,
    .lastChar = 255,
    .widMax = 12,
    .kernMax = 0,
    .nDescent = -2,
    .fRectWidth = 12,
    .fRectHeight = 11,
    .ascent = 9,
    .descent = 2,
    .leading = 0,
    .rowWords = 38,
    .owTLoc = NULL,
    .bitImage = NULL
};

/* System Font Package Definitions */
SystemFontPackage gChicagoFont = {
    .family = chicagoFontFamily,
    .numFonts = 1,
    .fonts = (BitmapFont*)&chicagoBitmapFont,
    .numResources = 2,
    .resources = NULL
};

SystemFontPackage gCourierFont = {
    .family = courierFontFamily,
    .numFonts = 1,
    .fonts = (BitmapFont*)&courierBitmapFont,
    .numResources = 2,
    .resources = NULL
};

SystemFontPackage gGenevaFont = {
    .family = genevaFontFamily,
    .numFonts = 1,
    .fonts = (BitmapFont*)&genevaBitmapFont,
    .numResources = 2,
    .resources = NULL
};

SystemFontPackage gHelveticaFont = {
    .family = helveticaFontFamily,
    .numFonts = 1,
    .fonts = (BitmapFont*)&helveticaBitmapFont,
    .numResources = 2,
    .resources = NULL
};

SystemFontPackage gMonacoFont = {
    .family = monacoFontFamily,
    .numFonts = 1,
    .fonts = (BitmapFont*)&monacoBitmapFont,
    .numResources = 2,
    .resources = NULL
};

SystemFontPackage gNewYorkFont = {
    .family = newYorkFontFamily,
    .numFonts = 1,
    .fonts = (BitmapFont*)&newYorkBitmapFont,
    .numResources = 2,
    .resources = NULL
};

/* Standard font size table */
static const short standardFontSizes[kNumStandardSizes] = kStandardFontSizes;

/*
 * GetStandardFontSizes - Get array of standard System 7.1 font sizes
 */
const short* GetStandardFontSizes(short* numSizes) {
    if (numSizes) *numSizes = kNumStandardSizes;
    return standardFontSizes;
}

/*
 * IsFontAvailable - Check if a font family is available
 */
Boolean IsFontAvailable(short familyID) {
    return (GetSystemFont(familyID) != NULL);
}

/*
 * GetFontMetrics - Get font metrics for a font family
 */
OSErr GetFontMetrics(short familyID, short* ascent, short* descent, short* leading) {
    SystemFontPackage* package = GetSystemFont(familyID);
    if (!package) return fontNotFoundErr;

    if (ascent) *ascent = package->family.ascent;
    if (descent) *descent = package->family.descent;
    if (leading) *leading = package->family.leading;

    return noErr;
}

/*
 * GetFontName - Get font family name
 */
OSErr GetFontName(short familyID, Str255 fontName) {
    SystemFontPackage* package = GetSystemFont(familyID);
    if (!package || !fontName) return fontNotFoundErr;

    /* Copy Pascal string */
    short nameLen = package->family.familyName[0];
    fontName[0] = nameLen;
    for (int i = 1; i <= nameLen; i++) {
        fontName[i] = package->family.familyName[i];
    }

    return noErr;
}

/*
 * InitSystemFonts - Initialize system font data (simplified version)
 */
OSErr InitSystemFonts(void) {
    /* In this simplified version, fonts are statically defined */
    /* Real implementation would load from resource files */

    printf("System 7.1 Fonts Initialized:\n");
    printf("  - Chicago (System Font)\n");
    printf("  - Geneva (Application Font)\n");
    printf("  - New York (Serif Font)\n");
    printf("  - Monaco (Monospace Font)\n");
    printf("  - Courier (Monospace Serif)\n");
    printf("  - Helvetica (Sans Serif)\n");

    return noErr;
}
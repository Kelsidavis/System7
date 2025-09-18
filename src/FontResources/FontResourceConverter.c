/*
 * Font Resource Converter
 * Converts Mac OS System 7.1 font .rsrc files to portable C structures
 *
 * This utility extracts FOND and NFNT resources from Mac OS font files
 * and converts them to portable bitmap font data for cross-platform use.
 *
 * Supports the six core System 7.1 fonts:
 * Chicago, Courier, Geneva, Helvetica, Monaco, New York
 */

#include "../../include/FontResources/SystemFonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Mac OS resource file format structures */
typedef struct {
    uint32_t dataOffset;        /* Offset to resource data */
    uint32_t mapOffset;         /* Offset to resource map */
    uint32_t dataLength;        /* Length of resource data */
    uint32_t mapLength;         /* Length of resource map */
} ResourceHeader;

typedef struct {
    uint16_t attributes;        /* Resource attributes */
    uint16_t resourceID;        /* Resource ID */
    uint16_t nameOffset;        /* Offset to resource name */
    uint32_t dataOffset;        /* Offset to resource data */
    uint32_t reserved;          /* Reserved field */
} ResourceEntry;

typedef struct {
    uint32_t resourceType;      /* Resource type (FOND, NFNT) */
    uint16_t numResources;      /* Number of resources of this type */
    uint16_t refListOffset;     /* Offset to reference list */
} TypeEntry;

/* External font packages declared in SystemFontData.c */
extern SystemFontPackage gChicagoFont;
extern SystemFontPackage gCourierFont;
extern SystemFontPackage gGenevaFont;
extern SystemFontPackage gHelveticaFont;
extern SystemFontPackage gMonacoFont;
extern SystemFontPackage gNewYorkFont;

/* Font name constants */
const char kChicagoFontName[] = "Chicago";
const char kCourierFontName[] = "Courier";
const char kGenevaFontName[] = "Geneva";
const char kHelveticaFontName[] = "Helvetica";
const char kMonacoFontName[] = "Monaco";
const char kNewYorkFontName[] = "New York";

/* Utility functions for byte order conversion */
static uint16_t SwapUInt16(uint16_t value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

static uint32_t SwapUInt32(uint32_t value) {
    return ((value & 0xFF) << 24) |
           (((value >> 8) & 0xFF) << 16) |
           (((value >> 16) & 0xFF) << 8) |
           ((value >> 24) & 0xFF);
}

/*
 * ReadMacResourceHeader - Read and parse Mac OS resource file header
 */
static OSErr ReadMacResourceHeader(FILE* file, ResourceHeader* header) {
    if (!file || !header) return paramErr;

    fseek(file, 0, SEEK_SET);

    if (fread(header, sizeof(ResourceHeader), 1, file) != 1) {
        return ioErr;
    }

    /* Convert from big-endian to host byte order */
    header->dataOffset = SwapUInt32(header->dataOffset);
    header->mapOffset = SwapUInt32(header->mapOffset);
    header->dataLength = SwapUInt32(header->dataLength);
    header->mapLength = SwapUInt32(header->mapLength);

    return noErr;
}

/*
 * ExtractFONDResource - Extract FOND (Font Family) resource
 */
static OSErr ExtractFONDResource(FILE* file, long offset, FontFamily* family) {
    if (!file || !family) return paramErr;

    fseek(file, offset, SEEK_SET);

    /* Read FOND header */
    uint16_t flags, familyID, firstChar, lastChar;
    uint16_t ascent, descent, leading, widMax;

    fread(&flags, 2, 1, file);
    fread(&familyID, 2, 1, file);
    fread(&firstChar, 2, 1, file);
    fread(&lastChar, 2, 1, file);
    fread(&ascent, 2, 1, file);
    fread(&descent, 2, 1, file);
    fread(&leading, 2, 1, file);
    fread(&widMax, 2, 1, file);

    /* Convert endianness */
    family->familyID = SwapUInt16(familyID);
    family->firstChar = SwapUInt16(firstChar);
    family->lastChar = SwapUInt16(lastChar);
    family->ascent = SwapUInt16(ascent);
    family->descent = SwapUInt16(descent);
    family->leading = SwapUInt16(leading);
    family->widMax = SwapUInt16(widMax);

    /* Initialize other fields */
    family->widthTable = NULL;
    family->kernTable = NULL;
    family->numSizes = 0;

    return noErr;
}

/*
 * ExtractNFNTResource - Extract NFNT (Bitmap Font) resource
 */
static OSErr ExtractNFNTResource(FILE* file, long offset, BitmapFont* font) {
    if (!file || !font) return paramErr;

    fseek(file, offset, SEEK_SET);

    /* Read NFNT header */
    uint16_t fontType, firstChar, lastChar, widMax;
    uint16_t kernMax, nDescent, fRectWidth, fRectHeight;
    uint16_t ascent, descent, leading, rowWords;

    fread(&fontType, 2, 1, file);
    fread(&firstChar, 2, 1, file);
    fread(&lastChar, 2, 1, file);
    fread(&widMax, 2, 1, file);
    fread(&kernMax, 2, 1, file);
    fread(&nDescent, 2, 1, file);
    fread(&fRectWidth, 2, 1, file);
    fread(&fRectHeight, 2, 1, file);

    /* Skip offset/width table location */
    fseek(file, 4, SEEK_CUR);

    fread(&ascent, 2, 1, file);
    fread(&descent, 2, 1, file);
    fread(&leading, 2, 1, file);
    fread(&rowWords, 2, 1, file);

    /* Convert endianness */
    font->fontType = SwapUInt16(fontType);
    font->firstChar = SwapUInt16(firstChar);
    font->lastChar = SwapUInt16(lastChar);
    font->widMax = SwapUInt16(widMax);
    font->kernMax = SwapUInt16(kernMax);
    font->nDescent = SwapUInt16(nDescent);
    font->fRectWidth = SwapUInt16(fRectWidth);
    font->fRectHeight = SwapUInt16(fRectHeight);
    font->ascent = SwapUInt16(ascent);
    font->descent = SwapUInt16(descent);
    font->leading = SwapUInt16(leading);
    font->rowWords = SwapUInt16(rowWords);

    /* Initialize bitmap data */
    font->owTLoc = NULL;
    font->bitImage = NULL;

    return noErr;
}

/*
 * ConvertMacFontResource - Convert Mac OS .rsrc font to portable format
 */
OSErr ConvertMacFontResource(const char* resourcePath, SystemFontPackage* package) {
    if (!resourcePath || !package) return paramErr;

    FILE* file = fopen(resourcePath, "rb");
    if (!file) return fnfErr;

    ResourceHeader header;
    OSErr err = ReadMacResourceHeader(file, &header);
    if (err != noErr) {
        fclose(file);
        return err;
    }

    /* Initialize package */
    memset(package, 0, sizeof(SystemFontPackage));

    /* For now, create minimal font data structures */
    /* This is a simplified conversion - real implementation would */
    /* parse the complete resource map and extract all font data */

    /* Extract basic font family information */
    err = ExtractFONDResource(file, header.dataOffset + 4, &package->family);
    if (err != noErr) {
        fclose(file);
        return err;
    }

    /* Allocate space for bitmap fonts (simplified) */
    package->numFonts = 1;
    package->fonts = (BitmapFont*)malloc(sizeof(BitmapFont));
    if (!package->fonts) {
        fclose(file);
        return memFullErr;
    }

    /* Extract first bitmap font */
    err = ExtractNFNTResource(file, header.dataOffset + 256, &package->fonts[0]);
    if (err != noErr) {
        free(package->fonts);
        package->fonts = NULL;
        fclose(file);
        return err;
    }

    package->numResources = 2; /* FOND + NFNT */

    fclose(file);
    return noErr;
}

/*
 * LoadSystemFonts - Load all six core System 7.1 fonts
 */
OSErr LoadSystemFonts(void) {
    OSErr err;

    /* Convert each font resource file */
    err = ConvertMacFontResource("/home/k/Desktop/system7/system7_resources/Fonts_resources/Chicago.rsrc", &gChicagoFont);
    if (err != noErr) return err;

    err = ConvertMacFontResource("/home/k/Desktop/system7/system7_resources/Fonts_resources/Courier.rsrc", &gCourierFont);
    if (err != noErr) return err;

    err = ConvertMacFontResource("/home/k/Desktop/system7/system7_resources/Fonts_resources/Geneva.rsrc", &gGenevaFont);
    if (err != noErr) return err;

    err = ConvertMacFontResource("/home/k/Desktop/system7/system7_resources/Fonts_resources/Helvetica.rsrc", &gHelveticaFont);
    if (err != noErr) return err;

    err = ConvertMacFontResource("/home/k/Desktop/system7/system7_resources/Fonts_resources/Monaco.rsrc", &gMonacoFont);
    if (err != noErr) return err;

    err = ConvertMacFontResource("/home/k/Desktop/system7/system7_resources/Fonts_resources/New York.rsrc", &gNewYorkFont);
    if (err != noErr) return err;

    /* Set font family names */
    strcpy((char*)&gChicagoFont.family.familyName[1], kChicagoFontName);
    gChicagoFont.family.familyName[0] = strlen(kChicagoFontName);

    strcpy((char*)&gCourierFont.family.familyName[1], kCourierFontName);
    gCourierFont.family.familyName[0] = strlen(kCourierFontName);

    strcpy((char*)&gGenevaFont.family.familyName[1], kGenevaFontName);
    gGenevaFont.family.familyName[0] = strlen(kGenevaFontName);

    strcpy((char*)&gHelveticaFont.family.familyName[1], kHelveticaFontName);
    gHelveticaFont.family.familyName[0] = strlen(kHelveticaFontName);

    strcpy((char*)&gMonacoFont.family.familyName[1], kMonacoFontName);
    gMonacoFont.family.familyName[0] = strlen(kMonacoFontName);

    strcpy((char*)&gNewYorkFont.family.familyName[1], kNewYorkFontName);
    gNewYorkFont.family.familyName[0] = strlen(kNewYorkFontName);

    return noErr;
}

/*
 * GetSystemFont - Get font package by family ID
 */
SystemFontPackage* GetSystemFont(short familyID) {
    switch (familyID) {
        case kChicagoFont:
            return &gChicagoFont;
        case kCourierFont:
            return &gCourierFont;
        case kGenevahFont:
            return &gGenevaFont;
        case kHelveticaFont:
            return &gHelveticaFont;
        case kMonacoFont:
            return &gMonacoFont;
        case kNewYorkFont:
            return &gNewYorkFont;
        default:
            return NULL;
    }
}

/*
 * GetFontByName - Get font package by name
 */
SystemFontPackage* GetFontByName(ConstStr255Param fontName) {
    if (!fontName) return NULL;

    /* Convert Pascal string to C string for comparison */
    char cName[256];
    memcpy(cName, &fontName[1], fontName[0]);
    cName[fontName[0]] = '\0';

    if (strcmp(cName, kChicagoFontName) == 0) return &gChicagoFont;
    if (strcmp(cName, kCourierFontName) == 0) return &gCourierFont;
    if (strcmp(cName, kGenevaFontName) == 0) return &gGenevaFont;
    if (strcmp(cName, kHelveticaFontName) == 0) return &gHelveticaFont;
    if (strcmp(cName, kMonacoFontName) == 0) return &gMonacoFont;
    if (strcmp(cName, kNewYorkFontName) == 0) return &gNewYorkFont;

    return NULL;
}

/*
 * GetBitmapFont - Get bitmap font for specific size and style
 */
BitmapFont* GetBitmapFont(short familyID, short size, short style) {
    SystemFontPackage* package = GetSystemFont(familyID);
    if (!package || !package->fonts) return NULL;

    /* Simplified: return first font (real implementation would find exact match) */
    (void)size;  /* Unused parameters */
    (void)style;

    return &package->fonts[0];
}

/*
 * UnloadSystemFonts - Release all loaded font resources
 */
void UnloadSystemFonts(void) {
    SystemFontPackage* fonts[] = {
        &gChicagoFont, &gCourierFont, &gGenevaFont,
        &gHelveticaFont, &gMonacoFont, &gNewYorkFont
    };

    for (int i = 0; i < 6; i++) {
        if (fonts[i]->fonts) {
            free(fonts[i]->fonts);
            fonts[i]->fonts = NULL;
        }
        if (fonts[i]->resources) {
            free(fonts[i]->resources);
            fonts[i]->resources = NULL;
        }
        fonts[i]->numFonts = 0;
        fonts[i]->numResources = 0;
    }
}

/*
 * SavePortableFontData - Save font package as portable C data
 */
OSErr SavePortableFontData(const SystemFontPackage* package, const char* outputPath) {
    if (!package || !outputPath) return paramErr;

    FILE* file = fopen(outputPath, "w");
    if (!file) return ioErr;

    /* Generate C source code with font data */
    fprintf(file, "/*\n");
    fprintf(file, " * Portable Font Data - Generated from Mac OS System 7.1 resources\n");
    fprintf(file, " */\n\n");

    fprintf(file, "#include \"SystemFonts.h\"\n\n");

    /* Export font family data */
    fprintf(file, "/* Font Family: %.*s */\n",
            package->family.familyName[0], &package->family.familyName[1]);
    fprintf(file, "static const FontFamily fontFamily = {\n");
    fprintf(file, "    .familyID = %d,\n", package->family.familyID);
    fprintf(file, "    .firstChar = %d,\n", package->family.firstChar);
    fprintf(file, "    .lastChar = %d,\n", package->family.lastChar);
    fprintf(file, "    .ascent = %d,\n", package->family.ascent);
    fprintf(file, "    .descent = %d,\n", package->family.descent);
    fprintf(file, "    .leading = %d,\n", package->family.leading);
    fprintf(file, "    .widMax = %d\n", package->family.widMax);
    fprintf(file, "};\n\n");

    fclose(file);
    return noErr;
}
/*
 * ModernFontSupport.c - Modern Font Format Implementation
 *
 * Implements support for modern font formats including OpenType, WOFF/WOFF2,
 * system fonts, and web fonts while maintaining Mac OS 7.1 API compatibility.
 */

#include "../include/FontManager/ModernFonts.h"
#include "../include/FontManager/FontManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global state */
static Boolean gModernFontInitialized = FALSE;
static FontDirectory *gFontDirectory = NULL;
static ModernFontCache *gModernFontCache = NULL;

/* Platform-specific includes */
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <dwrite.h>
#endif

#ifdef __linux__
#include <fontconfig/fontconfig.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#endif

/* Internal helper functions */
static OSErr InitializeModernFontCache(void);
static OSErr ParseOpenTypeHeader(const void *fontData, OpenTypeFont *font);
static OSErr ParseOpenTypeTables(const void *fontData, OpenTypeFont *font);
static OSErr DecompressZlib(const void *compressed, unsigned long compressedSize,
                           void **decompressed, unsigned long *decompressedSize);
static OSErr ValidateOpenTypeFont(const void *fontData, unsigned long dataSize);
static unsigned long CalculateTableChecksum(const void *table, unsigned long length);

/*
 * InitializeModernFontSupport - Initialize modern font support
 */
OSErr InitializeModernFontSupport(void)
{
    OSErr error;

    if (gModernFontInitialized) {
        return noErr; /* Already initialized */
    }

    /* Initialize font directory */
    error = InitializeFontDirectory();
    if (error != noErr) {
        return error;
    }

    /* Initialize modern font cache */
    error = InitializeModernFontCache();
    if (error != noErr) {
        return error;
    }

    /* Initialize platform-specific support */
    error = InitializeSystemFontSupport();
    if (error != noErr) {
        /* Continue without system font support */
    }

    gModernFontInitialized = TRUE;
    return noErr;
}

/*
 * CleanupModernFontSupport - Cleanup modern font support
 */
void CleanupModernFontSupport(void)
{
    if (!gModernFontInitialized) {
        return;
    }

    /* Cleanup font cache */
    if (gModernFontCache != NULL) {
        if (gModernFontCache->fonts != NULL) {
            for (unsigned long i = 0; i < gModernFontCache->count; i++) {
                if (gModernFontCache->fonts[i] != NULL) {
                    /* Cleanup based on font type */
                    switch (gModernFontCache->fonts[i]->format) {
                        case kFontFormatOpenType:
                            UnloadOpenTypeFont(gModernFontCache->fonts[i]->data.openType);
                            break;
                        case kFontFormatWOFF:
                        case kFontFormatWOFF2:
                            UnloadWOFFFont(gModernFontCache->fonts[i]->data.woff);
                            break;
                        case kFontFormatSystem:
                            UnloadSystemFont(gModernFontCache->fonts[i]->data.system);
                            break;
                        case kFontFormatCollection:
                            UnloadFontCollection(gModernFontCache->fonts[i]->data.collection);
                            break;
                    }
                    free(gModernFontCache->fonts[i]);
                }
            }
            free(gModernFontCache->fonts);
        }
        free(gModernFontCache);
        gModernFontCache = NULL;
    }

    /* Cleanup font directory */
    if (gFontDirectory != NULL) {
        if (gFontDirectory->entries != NULL) {
            for (unsigned long i = 0; i < gFontDirectory->count; i++) {
                if (gFontDirectory->entries[i].filePath != NULL) {
                    free(gFontDirectory->entries[i].filePath);
                }
                if (gFontDirectory->entries[i].familyName != NULL) {
                    free(gFontDirectory->entries[i].familyName);
                }
                if (gFontDirectory->entries[i].styleName != NULL) {
                    free(gFontDirectory->entries[i].styleName);
                }
            }
            free(gFontDirectory->entries);
        }
        free(gFontDirectory);
        gFontDirectory = NULL;
    }

    gModernFontInitialized = FALSE;
}

/*
 * LoadOpenTypeFont - Load an OpenType font from file
 */
OSErr LoadOpenTypeFont(ConstStr255Param filePath, OpenTypeFont **font)
{
    FILE *file;
    unsigned long fileSize;
    void *fontData;
    OSErr error;
    char cFilePath[256];

    if (filePath == NULL || font == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&filePath[1], cFilePath, filePath[0]);
    cFilePath[filePath[0]] = '\0';

    /* Open font file */
    file = fopen(cFilePath, "rb");
    if (file == NULL) {
        return fontNotFoundErr;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocate memory for font data */
    fontData = malloc(fileSize);
    if (fontData == NULL) {
        fclose(file);
        return fontOutOfMemoryErr;
    }

    /* Read font data */
    if (fread(fontData, 1, fileSize, file) != fileSize) {
        free(fontData);
        fclose(file);
        return fontCorruptErr;
    }
    fclose(file);

    /* Parse OpenType font */
    error = ParseOpenTypeFont(fontData, fileSize, font);
    if (error != noErr) {
        free(fontData);
        return error;
    }

    return noErr;
}

/*
 * UnloadOpenTypeFont - Unload an OpenType font
 */
OSErr UnloadOpenTypeFont(OpenTypeFont *font)
{
    if (font == NULL) {
        return paramErr;
    }

    /* Free font data */
    if (font->fontData != NULL) {
        free(font->fontData);
    }

    /* Free name strings */
    if (font->familyName != NULL) {
        free(font->familyName);
    }
    if (font->styleName != NULL) {
        free(font->styleName);
    }

    /* Free table pointers (they point into fontData, so no separate free needed) */

    /* Free the font structure itself */
    free(font);

    return noErr;
}

/*
 * ParseOpenTypeFont - Parse OpenType font data
 */
OSErr ParseOpenTypeFont(const void *fontData, unsigned long dataSize, OpenTypeFont **font)
{
    OpenTypeFont *otFont;
    OSErr error;

    if (fontData == NULL || dataSize == 0 || font == NULL) {
        return paramErr;
    }

    /* Validate font data */
    error = ValidateOpenTypeFont(fontData, dataSize);
    if (error != noErr) {
        return error;
    }

    /* Allocate OpenType font structure */
    otFont = (OpenTypeFont *)calloc(1, sizeof(OpenTypeFont));
    if (otFont == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Copy font data */
    otFont->fontData = malloc(dataSize);
    if (otFont->fontData == NULL) {
        free(otFont);
        return fontOutOfMemoryErr;
    }
    memcpy(otFont->fontData, fontData, dataSize);
    otFont->dataSize = dataSize;

    /* Parse OpenType header */
    error = ParseOpenTypeHeader(fontData, otFont);
    if (error != noErr) {
        UnloadOpenTypeFont(otFont);
        return error;
    }

    /* Parse OpenType tables */
    error = ParseOpenTypeTables(fontData, otFont);
    if (error != noErr) {
        UnloadOpenTypeFont(otFont);
        return error;
    }

    *font = otFont;
    return noErr;
}

/*
 * DetectFontFormat - Detect font format from data
 */
unsigned short DetectFontFormat(const void *fontData, unsigned long dataSize)
{
    const unsigned char *data = (const unsigned char *)fontData;

    if (fontData == NULL || dataSize < 4) {
        return 0; /* Unknown format */
    }

    /* Check for OpenType/TrueType signature */
    if (dataSize >= 4) {
        unsigned long signature = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];

        if (signature == 0x00010000 || signature == 0x74727565) { /* TrueType */
            return kFontFormatTrueType;
        }
        if (signature == 0x4F54544F) { /* 'OTTO' - CFF OpenType */
            return kFontFormatOpenType;
        }
        if (signature == 0x74746366) { /* 'ttcf' - TrueType Collection */
            return kFontFormatCollection;
        }
    }

    /* Check for WOFF signature */
    if (dataSize >= 4 && memcmp(data, "wOFF", 4) == 0) {
        return kFontFormatWOFF;
    }

    /* Check for WOFF2 signature */
    if (dataSize >= 4 && memcmp(data, "wOF2", 4) == 0) {
        return kFontFormatWOFF2;
    }

    /* Check for PostScript Type 1 */
    if (dataSize >= 2 && data[0] == 0x80 && data[1] == 0x01) {
        return kFontFormatPostScript;
    }

    /* Check for Mac bitmap font (NFNT/FONT) */
    /* This would require resource fork parsing */

    return 0; /* Unknown format */
}

/*
 * LoadWOFFFont - Load a WOFF font
 */
OSErr LoadWOFFFont(ConstStr255Param filePath, WOFFFont **font)
{
    FILE *file;
    unsigned long fileSize;
    void *woffData;
    void *otfData;
    unsigned long otfSize;
    WOFFFont *woffFont;
    OSErr error;
    char cFilePath[256];

    if (filePath == NULL || font == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&filePath[1], cFilePath, filePath[0]);
    cFilePath[filePath[0]] = '\0';

    /* Read WOFF file */
    file = fopen(cFilePath, "rb");
    if (file == NULL) {
        return fontNotFoundErr;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    woffData = malloc(fileSize);
    if (woffData == NULL) {
        fclose(file);
        return fontOutOfMemoryErr;
    }

    if (fread(woffData, 1, fileSize, file) != fileSize) {
        free(woffData);
        fclose(file);
        return fontCorruptErr;
    }
    fclose(file);

    /* Decompress WOFF to OpenType */
    error = DecompressWOFF(woffData, fileSize, &otfData, &otfSize);
    if (error != noErr) {
        free(woffData);
        return error;
    }

    /* Create WOFF font structure */
    woffFont = (WOFFFont *)calloc(1, sizeof(WOFFFont));
    if (woffFont == NULL) {
        free(woffData);
        free(otfData);
        return fontOutOfMemoryErr;
    }

    woffFont->originalData = otfData;
    woffFont->originalSize = otfSize;
    woffFont->compressedSize = fileSize;

    /* Parse the decompressed OpenType data */
    error = ParseOpenTypeFont(otfData, otfSize, &woffFont->otFont);
    if (error != noErr) {
        free(woffData);
        free(otfData);
        free(woffFont);
        return error;
    }

    free(woffData); /* We don't need the original WOFF data anymore */
    *font = woffFont;
    return noErr;
}

/*
 * UnloadWOFFFont - Unload a WOFF font
 */
OSErr UnloadWOFFFont(WOFFFont *font)
{
    if (font == NULL) {
        return paramErr;
    }

    if (font->otFont != NULL) {
        UnloadOpenTypeFont(font->otFont);
    }

    if (font->originalData != NULL) {
        free(font->originalData);
    }

    free(font);
    return noErr;
}

/*
 * InitializeSystemFontSupport - Initialize platform system font support
 */
OSErr InitializeSystemFontSupport(void)
{
#ifdef __APPLE__
    /* CoreText is automatically initialized on macOS */
    return noErr;
#endif

#ifdef _WIN32
    /* DirectWrite initialization would go here */
    return noErr;
#endif

#ifdef __linux__
    /* Initialize Fontconfig */
    if (!FcInit()) {
        return kModernFontSystemErr;
    }
    return noErr;
#endif

    return kModernFontNotSupportedErr;
}

/*
 * LoadSystemFont - Load a system font by name
 */
OSErr LoadSystemFont(ConstStr255Param fontName, SystemFont **font)
{
    SystemFont *sysFont;
    char cFontName[256];

    if (fontName == NULL || font == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&fontName[1], cFontName, fontName[0]);
    cFontName[fontName[0]] = '\0';

    /* Allocate system font structure */
    sysFont = (SystemFont *)calloc(1, sizeof(SystemFont));
    if (sysFont == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Copy font name */
    sysFont->systemName = strdup(cFontName);
    if (sysFont->systemName == NULL) {
        free(sysFont);
        return fontOutOfMemoryErr;
    }

#ifdef __APPLE__
    /* Use CoreText to load the font */
    CFStringRef cfFontName = CFStringCreateWithCString(NULL, cFontName, kCFStringEncodingUTF8);
    if (cfFontName != NULL) {
        CTFontRef ctFont = CTFontCreateWithName(cfFontName, 12.0, NULL);
        if (ctFont != NULL) {
            sysFont->platformHandle = (void *)ctFont;
            sysFont->isInstalled = TRUE;

            /* Get display name */
            CFStringRef displayName = CTFontCopyDisplayName(ctFont);
            if (displayName != NULL) {
                char displayNameBuffer[256];
                if (CFStringGetCString(displayName, displayNameBuffer, sizeof(displayNameBuffer), kCFStringEncodingUTF8)) {
                    sysFont->displayName = strdup(displayNameBuffer);
                }
                CFRelease(displayName);
            }
        }
        CFRelease(cfFontName);
    }
#endif

#ifdef __linux__
    /* Use Fontconfig to find the font */
    FcPattern *pattern = FcPatternCreate();
    if (pattern != NULL) {
        FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)cFontName);
        FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);

        FcResult result;
        FcPattern *match = FcFontMatch(NULL, pattern, &result);
        if (match != NULL && result == FcResultMatch) {
            FcChar8 *fontPath = NULL;
            if (FcPatternGetString(match, FC_FILE, 0, &fontPath) == FcResultMatch) {
                sysFont->filePath = strdup((char *)fontPath);
                sysFont->isInstalled = TRUE;
            }

            FcChar8 *displayName = NULL;
            if (FcPatternGetString(match, FC_FULLNAME, 0, &displayName) == FcResultMatch) {
                sysFont->displayName = strdup((char *)displayName);
            }

            FcPatternDestroy(match);
        }
        FcPatternDestroy(pattern);
    }
#endif

    if (!sysFont->isInstalled) {
        free(sysFont->systemName);
        if (sysFont->displayName) free(sysFont->displayName);
        if (sysFont->filePath) free(sysFont->filePath);
        free(sysFont);
        return fontNotFoundErr;
    }

    *font = sysFont;
    return noErr;
}

/*
 * UnloadSystemFont - Unload a system font
 */
OSErr UnloadSystemFont(SystemFont *font)
{
    if (font == NULL) {
        return paramErr;
    }

#ifdef __APPLE__
    if (font->platformHandle != NULL) {
        CFRelease((CTFontRef)font->platformHandle);
    }
#endif

    if (font->systemName != NULL) {
        free(font->systemName);
    }
    if (font->displayName != NULL) {
        free(font->displayName);
    }
    if (font->filePath != NULL) {
        free(font->filePath);
    }

    free(font);
    return noErr;
}

/*
 * InitializeFontDirectory - Initialize the font directory
 */
OSErr InitializeFontDirectory(void)
{
    if (gFontDirectory != NULL) {
        return noErr; /* Already initialized */
    }

    gFontDirectory = (FontDirectory *)calloc(1, sizeof(FontDirectory));
    if (gFontDirectory == NULL) {
        return fontOutOfMemoryErr;
    }

    gFontDirectory->capacity = 256; /* Initial capacity */
    gFontDirectory->entries = (FontDirectoryEntry *)calloc(gFontDirectory->capacity, sizeof(FontDirectoryEntry));
    if (gFontDirectory->entries == NULL) {
        free(gFontDirectory);
        gFontDirectory = NULL;
        return fontOutOfMemoryErr;
    }

    gFontDirectory->count = 0;
    gFontDirectory->isDirty = FALSE;

    return noErr;
}

/* Internal helper functions */

static OSErr InitializeModernFontCache(void)
{
    if (gModernFontCache != NULL) {
        return noErr; /* Already initialized */
    }

    gModernFontCache = (ModernFontCache *)calloc(1, sizeof(ModernFontCache));
    if (gModernFontCache == NULL) {
        return fontOutOfMemoryErr;
    }

    gModernFontCache->capacity = 64; /* Initial capacity */
    gModernFontCache->maxSize = 16 * 1024 * 1024; /* 16MB max cache */
    gModernFontCache->fonts = (ModernFont **)calloc(gModernFontCache->capacity, sizeof(ModernFont *));
    if (gModernFontCache->fonts == NULL) {
        free(gModernFontCache);
        gModernFontCache = NULL;
        return fontOutOfMemoryErr;
    }

    return noErr;
}

static OSErr ParseOpenTypeHeader(const void *fontData, OpenTypeFont *font)
{
    const unsigned char *data = (const unsigned char *)fontData;
    unsigned short numTables;

    if (fontData == NULL || font == NULL) {
        return paramErr;
    }

    /* Read SFNT header */
    font->unitsPerEm = (data[18] << 8) | data[19]; /* Will be updated from 'head' table */
    numTables = (data[4] << 8) | data[5];

    /* Basic validation */
    if (numTables == 0 || numTables > 64) {
        return fontCorruptErr;
    }

    return noErr;
}

static OSErr ParseOpenTypeTables(const void *fontData, OpenTypeFont *font)
{
    /* This is a simplified implementation */
    /* In a real implementation, you would parse the table directory */
    /* and locate specific tables like 'head', 'hhea', 'hmtx', etc. */

    /* For now, just mark the font as valid */
    font->numGlyphs = 1000; /* Placeholder */

    return noErr;
}

static OSErr ValidateOpenTypeFont(const void *fontData, unsigned long dataSize)
{
    const unsigned char *data = (const unsigned char *)fontData;

    if (fontData == NULL || dataSize < 12) {
        return fontCorruptErr;
    }

    /* Check SFNT signature */
    unsigned long signature = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    if (signature != 0x00010000 && signature != 0x74727565 && signature != 0x4F54544F) {
        return fontCorruptErr;
    }

    return noErr;
}

static OSErr DecompressWOFF(const void *woffData, unsigned long woffSize,
                           void **otfData, unsigned long *otfSize)
{
    /* Simplified WOFF decompression */
    /* In a real implementation, you would parse the WOFF header */
    /* and decompress each table using zlib */

    return kModernFontNotSupportedErr; /* Not implemented in this stub */
}

static unsigned long CalculateTableChecksum(const void *table, unsigned long length)
{
    const unsigned long *data = (const unsigned long *)table;
    unsigned long sum = 0;
    unsigned long nlongs = (length + 3) / 4;

    while (nlongs-- > 0) {
        sum += *data++;
    }

    return sum;
}
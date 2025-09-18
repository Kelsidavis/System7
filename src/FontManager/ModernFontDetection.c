/*
 * ModernFontDetection.c - Modern Font Format Detection
 *
 * Implements font format detection and validation for modern font formats
 * including OpenType, WOFF/WOFF2, system fonts, and font collections.
 */

#include "../include/FontManager/ModernFonts.h"
#include "../include/FontManager/FontManager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Font format detection functions */

/*
 * IsOpenTypeFont - Check if font is OpenType format
 */
Boolean IsOpenTypeFont(short familyID, short size)
{
    Str255 fontName;
    char cFontName[256];
    char filePath[512];
    FILE *file;
    unsigned char header[4];

    /* Get font name */
    GetFontName(familyID, fontName);
    if (fontName[0] == 0) {
        return FALSE;
    }

    /* Convert to C string */
    BlockMoveData(&fontName[1], cFontName, fontName[0]);
    cFontName[fontName[0]] = '\0';

    /* Try common OpenType file extensions and locations */
    const char *extensions[] = {".otf", ".ttf", NULL};
    const char *directories[] = {
        "/System/Library/Fonts/",
        "/Library/Fonts/",
        "~/Library/Fonts/",
        "/usr/share/fonts/",
        "/usr/local/share/fonts/",
        NULL
    };

    for (int d = 0; directories[d] != NULL; d++) {
        for (int e = 0; extensions[e] != NULL; e++) {
            snprintf(filePath, sizeof(filePath), "%s%s%s", directories[d], cFontName, extensions[e]);

            file = fopen(filePath, "rb");
            if (file != NULL) {
                if (fread(header, 1, 4, file) == 4) {
                    fclose(file);

                    /* Check for OpenType/TrueType signatures */
                    unsigned long signature = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
                    if (signature == 0x00010000 || /* TrueType */
                        signature == 0x74727565 || /* 'true' */
                        signature == 0x4F54544F) { /* 'OTTO' - CFF OpenType */
                        return TRUE;
                    }
                } else {
                    fclose(file);
                }
            }
        }
    }

    return FALSE;
}

/*
 * IsWOFFFont - Check if font is WOFF format
 */
Boolean IsWOFFFont(short familyID, short size)
{
    Str255 fontName;
    char cFontName[256];
    char filePath[512];
    FILE *file;
    unsigned char header[4];

    /* Get font name */
    GetFontName(familyID, fontName);
    if (fontName[0] == 0) {
        return FALSE;
    }

    /* Convert to C string */
    BlockMoveData(&fontName[1], cFontName, fontName[0]);
    cFontName[fontName[0]] = '\0';

    /* Try WOFF file extensions */
    const char *extensions[] = {".woff", NULL};
    const char *directories[] = {
        "/Library/Fonts/",
        "~/Library/Fonts/",
        "./fonts/",
        NULL
    };

    for (int d = 0; directories[d] != NULL; d++) {
        for (int e = 0; extensions[e] != NULL; e++) {
            snprintf(filePath, sizeof(filePath), "%s%s%s", directories[d], cFontName, extensions[e]);

            file = fopen(filePath, "rb");
            if (file != NULL) {
                if (fread(header, 1, 4, file) == 4) {
                    fclose(file);

                    /* Check for WOFF signature */
                    if (memcmp(header, "wOFF", 4) == 0) {
                        return TRUE;
                    }
                } else {
                    fclose(file);
                }
            }
        }
    }

    return FALSE;
}

/*
 * IsWOFF2Font - Check if font is WOFF2 format
 */
Boolean IsWOFF2Font(short familyID, short size)
{
    Str255 fontName;
    char cFontName[256];
    char filePath[512];
    FILE *file;
    unsigned char header[4];

    /* Get font name */
    GetFontName(familyID, fontName);
    if (fontName[0] == 0) {
        return FALSE;
    }

    /* Convert to C string */
    BlockMoveData(&fontName[1], cFontName, fontName[0]);
    cFontName[fontName[0]] = '\0';

    /* Try WOFF2 file extensions */
    const char *extensions[] = {".woff2", NULL};
    const char *directories[] = {
        "/Library/Fonts/",
        "~/Library/Fonts/",
        "./fonts/",
        NULL
    };

    for (int d = 0; directories[d] != NULL; d++) {
        for (int e = 0; extensions[e] != NULL; e++) {
            snprintf(filePath, sizeof(filePath), "%s%s%s", directories[d], cFontName, extensions[e]);

            file = fopen(filePath, "rb");
            if (file != NULL) {
                if (fread(header, 1, 4, file) == 4) {
                    fclose(file);

                    /* Check for WOFF2 signature */
                    if (memcmp(header, "wOF2", 4) == 0) {
                        return TRUE;
                    }
                } else {
                    fclose(file);
                }
            }
        }
    }

    return FALSE;
}

/*
 * IsSystemFont - Check if font is available as system font
 */
Boolean IsSystemFont(short familyID, short size)
{
    Str255 fontName;
    SystemFont *sysFont;
    OSErr error;

    /* Get font name */
    GetFontName(familyID, fontName);
    if (fontName[0] == 0) {
        return FALSE;
    }

    /* Try to load as system font */
    error = LoadSystemFont(fontName, &sysFont);
    if (error == noErr && sysFont != NULL) {
        UnloadSystemFont(sysFont);
        return TRUE;
    }

    return FALSE;
}

/*
 * ValidateFontFile - Validate a font file
 */
OSErr ValidateFontFile(ConstStr255Param filePath)
{
    FILE *file;
    unsigned char header[12];
    char cFilePath[256];
    unsigned short format;

    if (filePath == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&filePath[1], cFilePath, filePath[0]);
    cFilePath[filePath[0]] = '\0';

    /* Open file */
    file = fopen(cFilePath, "rb");
    if (file == NULL) {
        return fontNotFoundErr;
    }

    /* Read header */
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return fontCorruptErr;
    }
    fclose(file);

    /* Detect format */
    format = DetectFontFormat(header, sizeof(header));
    if (format == 0) {
        return kModernFontNotSupportedErr;
    }

    /* Validate based on format */
    switch (format) {
        case kFontFormatOpenType:
        case kFontFormatTrueType:
            return ValidateOpenTypeFont(header, sizeof(header));

        case kFontFormatWOFF:
            /* Check WOFF header structure */
            if (memcmp(header, "wOFF", 4) == 0) {
                return noErr;
            }
            return fontCorruptErr;

        case kFontFormatWOFF2:
            /* Check WOFF2 header structure */
            if (memcmp(header, "wOF2", 4) == 0) {
                return noErr;
            }
            return fontCorruptErr;

        case kFontFormatCollection:
            /* Check TTC header */
            if (memcmp(header, "ttcf", 4) == 0) {
                return noErr;
            }
            return fontCorruptErr;

        default:
            return kModernFontNotSupportedErr;
    }
}

/*
 * GetFontFileInfo - Get information about a font file
 */
OSErr GetFontFileInfo(ConstStr255Param filePath, unsigned short *format,
                     char **familyName, char **styleName)
{
    FILE *file;
    unsigned long fileSize;
    void *fontData;
    char cFilePath[256];
    OSErr error;

    if (filePath == NULL || format == NULL) {
        return paramErr;
    }

    /* Convert Pascal string to C string */
    BlockMoveData(&filePath[1], cFilePath, filePath[0]);
    cFilePath[filePath[0]] = '\0';

    /* Open and read file */
    file = fopen(cFilePath, "rb");
    if (file == NULL) {
        return fontNotFoundErr;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Limit size for format detection */
    unsigned long readSize = (fileSize > 1024) ? 1024 : fileSize;
    fontData = malloc(readSize);
    if (fontData == NULL) {
        fclose(file);
        return fontOutOfMemoryErr;
    }

    if (fread(fontData, 1, readSize, file) != readSize) {
        free(fontData);
        fclose(file);
        return fontCorruptErr;
    }
    fclose(file);

    /* Detect format */
    *format = DetectFontFormat(fontData, readSize);

    /* Extract names based on format */
    if (*format == kFontFormatOpenType || *format == kFontFormatTrueType) {
        /* For full name extraction, we'd need to parse the 'name' table */
        /* This is a simplified implementation */
        if (familyName != NULL) {
            *familyName = strdup("Unknown Family");
        }
        if (styleName != NULL) {
            *styleName = strdup("Regular");
        }
        error = noErr;
    } else if (*format == kFontFormatWOFF || *format == kFontFormatWOFF2) {
        /* Would need WOFF decompression and name table parsing */
        if (familyName != NULL) {
            *familyName = strdup("WOFF Family");
        }
        if (styleName != NULL) {
            *styleName = strdup("Regular");
        }
        error = noErr;
    } else {
        error = kModernFontNotSupportedErr;
    }

    free(fontData);
    return error;
}

/*
 * IsModernFontFormat - Check if format is a modern font format
 */
Boolean IsModernFontFormat(unsigned short format)
{
    switch (format) {
        case kFontFormatOpenType:
        case kFontFormatWOFF:
        case kFontFormatWOFF2:
        case kFontFormatSystem:
        case kFontFormatCollection:
            return TRUE;
        default:
            return FALSE;
    }
}

/*
 * ScanModernFontDirectories - Scan directories for modern fonts
 */
OSErr ScanModernFontDirectories(void)
{
    const char *directories[] = {
#ifdef __APPLE__
        "/System/Library/Fonts/",
        "/Library/Fonts/",
        "~/Library/Fonts/",
#endif
#ifdef __linux__
        "/usr/share/fonts/",
        "/usr/local/share/fonts/",
        "~/.fonts/",
        "~/.local/share/fonts/",
#endif
#ifdef _WIN32
        "C:/Windows/Fonts/",
        "C:/Users/%USERNAME%/AppData/Local/Microsoft/Windows/Fonts/",
#endif
        "./fonts/",
        NULL
    };

    for (int d = 0; directories[d] != NULL; d++) {
        /* This would recursively scan each directory */
        /* and add found fonts to the font directory */
        /* Implementation would depend on platform-specific directory APIs */
    }

    return noErr;
}

/*
 * InstallOpenTypeFont - Install an OpenType font file
 */
OSErr InstallOpenTypeFont(ConstStr255Param filePath, short *familyID)
{
    OpenTypeFont *font;
    OSErr error;

    if (filePath == NULL || familyID == NULL) {
        return paramErr;
    }

    /* Load the OpenType font */
    error = LoadOpenTypeFont(filePath, &font);
    if (error != noErr) {
        return error;
    }

    /* Register the font family */
    if (font->familyName != NULL) {
        Str255 pFamilyName;
        short nameLen = strlen(font->familyName);
        if (nameLen > 255) nameLen = 255;

        pFamilyName[0] = nameLen;
        BlockMoveData(font->familyName, &pFamilyName[1], nameLen);

        /* Find available family ID */
        *familyID = 1024; /* Start with user font IDs */
        while (*familyID < 16384) {
            Str255 existingName;
            GetFontName(*familyID, existingName);
            if (existingName[0] == 0) {
                /* This ID is available */
                break;
            }
            (*familyID)++;
        }

        /* Register the font family (this would be implemented in FontManagerCore) */
        /* RegisterFontFamily(*familyID, pFamilyName); */
    }

    /* Add to font directory */
    error = AddFontToDirectory(filePath);

    /* Keep the font loaded in cache */
    /* Font will be cached in the modern font cache */

    return noErr;
}

/*
 * InstallWOFFFont - Install a WOFF font file
 */
OSErr InstallWOFFFont(ConstStr255Param filePath, short *familyID)
{
    WOFFFont *font;
    OSErr error;

    if (filePath == NULL || familyID == NULL) {
        return paramErr;
    }

    /* Load the WOFF font */
    error = LoadWOFFFont(filePath, &font);
    if (error != noErr) {
        return error;
    }

    /* Use the underlying OpenType font for registration */
    if (font->otFont != NULL && font->otFont->familyName != NULL) {
        Str255 pFamilyName;
        short nameLen = strlen(font->otFont->familyName);
        if (nameLen > 255) nameLen = 255;

        pFamilyName[0] = nameLen;
        BlockMoveData(font->otFont->familyName, &pFamilyName[1], nameLen);

        /* Find available family ID */
        *familyID = 1024;
        while (*familyID < 16384) {
            Str255 existingName;
            GetFontName(*familyID, existingName);
            if (existingName[0] == 0) {
                break;
            }
            (*familyID)++;
        }
    }

    /* Add to font directory */
    error = AddFontToDirectory(filePath);

    return noErr;
}

/*
 * InstallWOFF2Font - Install a WOFF2 font file
 */
OSErr InstallWOFF2Font(ConstStr255Param filePath, short *familyID)
{
    /* Similar to WOFF but with WOFF2 decompression */
    return InstallWOFFFont(filePath, familyID); /* Simplified for now */
}

/*
 * InstallSystemFont - Install a system font by name
 */
OSErr InstallSystemFont(ConstStr255Param fontName, short *familyID)
{
    SystemFont *font;
    OSErr error;

    if (fontName == NULL || familyID == NULL) {
        return paramErr;
    }

    /* Load the system font */
    error = LoadSystemFont(fontName, &font);
    if (error != noErr) {
        return error;
    }

    /* Register with available family ID */
    *familyID = 2048; /* Start with system font IDs */
    while (*familyID < 16384) {
        Str255 existingName;
        GetFontName(*familyID, existingName);
        if (existingName[0] == 0) {
            break;
        }
        (*familyID)++;
    }

    /* If system font has a file path, add to directory */
    if (font->filePath != NULL) {
        Str255 pFilePath;
        short pathLen = strlen(font->filePath);
        if (pathLen > 255) pathLen = 255;

        pFilePath[0] = pathLen;
        BlockMoveData(font->filePath, &pFilePath[1], pathLen);

        AddFontToDirectory(pFilePath);
    }

    return noErr;
}
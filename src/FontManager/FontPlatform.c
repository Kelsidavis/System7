/*
 * FontPlatform.c - Platform Abstraction Layer for Modern Font Systems
 *
 * Provides integration with modern font systems including FreeType,
 * system font directories, and native font rendering APIs.
 */

#include "../include/FontManager/FontManager.h"
#include "../include/FontManager/BitmapFonts.h"
#include "../include/FontManager/TrueTypeFonts.h"
#include <Memory.h>
#include <Errors.h>
#include <Files.h>
#include <string.h>
#include <stdlib.h>

/* Platform-specific includes would go here */
#ifdef __unix__
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

/* Maximum number of system font directories */
#define MAX_FONT_DIRECTORIES 16
#define MAX_PATH_LENGTH 512

/* System font directory paths */
static Str255 gSystemFontDirectories[MAX_FONT_DIRECTORIES];
static short gFontDirectoryCount = 0;

/* Platform font cache */
typedef struct PlatformFontEntry {
    Str255 fontPath;           /* Full path to font file */
    Str255 fontName;           /* Font family name */
    short familyID;            /* Assigned family ID */
    short format;              /* Font format (TrueType, OpenType, etc.) */
    Boolean isInstalled;       /* Font is currently installed */
    struct PlatformFontEntry *next;
} PlatformFontEntry;

static PlatformFontEntry *gPlatformFonts = NULL;
static short gPlatformFontCount = 0;
static short gNextFamilyID = 1000; /* Start platform fonts at ID 1000 */

/* Internal helper functions */
static OSErr ScanFontDirectory(ConstStr255Param directoryPath);
static OSErr RegisterFontFile(ConstStr255Param filePath, ConstStr255Param fontName, short format);
static OSErr DetectFontFormat(ConstStr255Param filePath, short *format);
static OSErr ExtractFontName(ConstStr255Param filePath, Str255 fontName);
static OSErr GetSystemFontDirectories(void);
static PlatformFontEntry *FindPlatformFont(ConstStr255Param fontName);
static OSErr LoadPlatformTrueTypeFont(ConstStr255Param filePath, TTFont **font);

/*
 * InitializePlatformFonts - Initialize platform font system
 */
OSErr InitializePlatformFonts(void)
{
    OSErr error;

    /* Clear existing platform fonts */
    PlatformFontEntry *entry = gPlatformFonts;
    while (entry != NULL) {
        PlatformFontEntry *next = entry->next;
        DisposePtr((Ptr)entry);
        entry = next;
    }
    gPlatformFonts = NULL;
    gPlatformFontCount = 0;

    /* Get system font directories */
    error = GetSystemFontDirectories();
    if (error != noErr) {
        /* Continue anyway - might be running on unsupported platform */
    }

    /* Scan system fonts */
    error = ScanForSystemFonts();
    if (error != noErr) {
        /* Not fatal - continue with basic font support */
    }

    return noErr;
}

/*
 * RegisterSystemFontDirectory - Add a font directory to scan
 */
OSErr RegisterSystemFontDirectory(ConstStr255Param directoryPath)
{
    if (directoryPath == NULL || directoryPath[0] == 0) {
        return paramErr;
    }

    if (gFontDirectoryCount >= MAX_FONT_DIRECTORIES) {
        return fontCacheFullErr;
    }

    /* Copy directory path */
    BlockMoveData(directoryPath, gSystemFontDirectories[gFontDirectoryCount], directoryPath[0] + 1);
    gFontDirectoryCount++;

    /* Scan the new directory */
    return ScanFontDirectory(directoryPath);
}

/*
 * ScanForSystemFonts - Scan all registered font directories
 */
OSErr ScanForSystemFonts(void)
{
    short i;
    OSErr error, lastError = noErr;

    for (i = 0; i < gFontDirectoryCount; i++) {
        error = ScanFontDirectory(gSystemFontDirectories[i]);
        if (error != noErr) {
            lastError = error; /* Remember last error but continue */
        }
    }

    return lastError;
}

/*
 * LoadPlatformFont - Load a font by name from the platform
 */
OSErr LoadPlatformFont(ConstStr255Param fontName, short *familyID)
{
    PlatformFontEntry *entry;
    OSErr error;

    if (fontName == NULL || familyID == NULL) {
        return paramErr;
    }

    *familyID = 0;

    /* Find font in platform cache */
    entry = FindPlatformFont(fontName);
    if (entry == NULL) {
        return fontNotFoundErr;
    }

    /* Return assigned family ID */
    *familyID = entry->familyID;
    return noErr;
}

/*
 * GetInstalledPlatformFonts - Get list of all installed platform fonts
 */
OSErr GetInstalledPlatformFonts(Str255 **fontNames, short *count)
{
    PlatformFontEntry *entry;
    Str255 *names;
    short i = 0;

    if (fontNames == NULL || count == NULL) {
        return paramErr;
    }

    *fontNames = NULL;
    *count = gPlatformFontCount;

    if (gPlatformFontCount == 0) {
        return noErr;
    }

    /* Allocate array for font names */
    names = (Str255 *)NewPtr(gPlatformFontCount * sizeof(Str255));
    if (names == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Copy font names */
    entry = gPlatformFonts;
    while (entry != NULL && i < gPlatformFontCount) {
        BlockMoveData(entry->fontName, names[i], entry->fontName[0] + 1);
        entry = entry->next;
        i++;
    }

    *fontNames = names;
    return noErr;
}

/*
 * LoadPlatformFontFile - Load font directly from file path
 */
OSErr LoadPlatformFontFile(ConstStr255Param filePath, short *familyID)
{
    Str255 fontName;
    short format;
    OSErr error;

    if (filePath == NULL || familyID == NULL) {
        return paramErr;
    }

    *familyID = 0;

    /* Detect font format */
    error = DetectFontFormat(filePath, &format);
    if (error != noErr) {
        return error;
    }

    /* Extract font name */
    error = ExtractFontName(filePath, fontName);
    if (error != noErr) {
        return error;
    }

    /* Register font */
    error = RegisterFontFile(filePath, fontName, format);
    if (error != noErr) {
        return error;
    }

    /* Find registered font and return ID */
    PlatformFontEntry *entry = FindPlatformFont(fontName);
    if (entry != NULL) {
        *familyID = entry->familyID;
    }

    return noErr;
}

/*
 * GetPlatformFontPath - Get file path for a platform font
 */
OSErr GetPlatformFontPath(short familyID, Str255 fontPath)
{
    PlatformFontEntry *entry;

    if (fontPath == NULL) {
        return paramErr;
    }

    fontPath[0] = 0;

    /* Find font entry */
    entry = gPlatformFonts;
    while (entry != NULL) {
        if (entry->familyID == familyID) {
            BlockMoveData(entry->fontPath, fontPath, entry->fontPath[0] + 1);
            return noErr;
        }
        entry = entry->next;
    }

    return fontNotFoundErr;
}

/*
 * RefreshPlatformFonts - Rescan font directories for changes
 */
OSErr RefreshPlatformFonts(void)
{
    /* Clear existing platform fonts */
    PlatformFontEntry *entry = gPlatformFonts;
    while (entry != NULL) {
        PlatformFontEntry *next = entry->next;
        DisposePtr((Ptr)entry);
        entry = next;
    }
    gPlatformFonts = NULL;
    gPlatformFontCount = 0;

    /* Rescan directories */
    return ScanForSystemFonts();
}

/*
 * GetPlatformFontMetrics - Get metrics for platform font using native APIs
 */
OSErr GetPlatformFontMetrics(short familyID, short pointSize, FontMetrics *metrics)
{
    PlatformFontEntry *entry;
    TTFont *font;
    OSErr error;

    if (metrics == NULL) {
        return paramErr;
    }

    /* Find platform font */
    entry = gPlatformFonts;
    while (entry != NULL) {
        if (entry->familyID == familyID) {
            break;
        }
        entry = entry->next;
    }

    if (entry == NULL) {
        return fontNotFoundErr;
    }

    /* Load font and get metrics */
    if (entry->format == kFontFormatTrueType || entry->format == kFontFormatOpenType) {
        error = LoadPlatformTrueTypeFont(entry->fontPath, &font);
        if (error == noErr) {
            error = GetTrueTypeFontMetrics(font, (FMetricRec*)metrics);
            UnloadTrueTypeFont(font);
        }
        return error;
    }

    return fontNotFoundErr;
}

/* Internal helper function implementations */

static OSErr GetSystemFontDirectories(void)
{
    gFontDirectoryCount = 0;

#ifdef __unix__
    /* Common Unix font directories */
    RegisterSystemFontDirectory("\p/usr/share/fonts");
    RegisterSystemFontDirectory("\p/usr/local/share/fonts");
    RegisterSystemFontDirectory("\p/System/Library/Fonts");
    RegisterSystemFontDirectory("\p/Library/Fonts");
    RegisterSystemFontDirectory("\p~/.fonts");

    /* X11 font directories */
    RegisterSystemFontDirectory("\p/usr/X11R6/lib/X11/fonts");
    RegisterSystemFontDirectory("\p/usr/share/X11/fonts");
#endif

#ifdef _WIN32
    /* Windows font directories */
    char windowsPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, windowsPath) == S_OK) {
        Str255 pPath;
        pPath[0] = strlen(windowsPath);
        BlockMoveData(windowsPath, &pPath[1], pPath[0]);
        RegisterSystemFontDirectory(pPath);
    }
#endif

#ifdef __APPLE__
    /* macOS font directories */
    RegisterSystemFontDirectory("\p/System/Library/Fonts");
    RegisterSystemFontDirectory("\p/Library/Fonts");
    RegisterSystemFontDirectory("\p~/Library/Fonts");
#endif

    return noErr;
}

static OSErr ScanFontDirectory(ConstStr255Param directoryPath)
{
    OSErr error = noErr;

    if (directoryPath == NULL || directoryPath[0] == 0) {
        return paramErr;
    }

#ifdef __unix__
    /* Unix directory scanning */
    char cPath[256];
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char fullPath[512];

    /* Convert Pascal string to C string */
    BlockMoveData(&directoryPath[1], cPath, directoryPath[0]);
    cPath[directoryPath[0]] = '\0';

    dir = opendir(cPath);
    if (dir == NULL) {
        return fontNotFoundErr;
    }

    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files and directories */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Build full path */
        snprintf(fullPath, sizeof(fullPath), "%s/%s", cPath, entry->d_name);

        /* Check if it's a file */
        if (stat(fullPath, &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
            /* Check if it's a font file by extension */
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL) {
                if (strcasecmp(ext, ".ttf") == 0 || strcasecmp(ext, ".otf") == 0 ||
                    strcasecmp(ext, ".ttc") == 0) {
                    /* Convert to Pascal string and process */
                    Str255 pPath;
                    short pathLen = strlen(fullPath);
                    if (pathLen > 255) pathLen = 255;
                    pPath[0] = pathLen;
                    BlockMoveData(fullPath, &pPath[1], pathLen);

                    LoadPlatformFontFile(pPath, NULL);
                }
            }
        }
    }

    closedir(dir);
#endif

    return error;
}

static OSErr RegisterFontFile(ConstStr255Param filePath, ConstStr255Param fontName, short format)
{
    PlatformFontEntry *entry;

    if (filePath == NULL || fontName == NULL) {
        return paramErr;
    }

    /* Check if font already registered */
    entry = FindPlatformFont(fontName);
    if (entry != NULL) {
        /* Update existing entry */
        BlockMoveData(filePath, entry->fontPath, filePath[0] + 1);
        entry->format = format;
        return noErr;
    }

    /* Allocate new entry */
    entry = (PlatformFontEntry *)NewPtr(sizeof(PlatformFontEntry));
    if (entry == NULL) {
        return fontOutOfMemoryErr;
    }

    /* Initialize entry */
    BlockMoveData(filePath, entry->fontPath, filePath[0] + 1);
    BlockMoveData(fontName, entry->fontName, fontName[0] + 1);
    entry->familyID = gNextFamilyID++;
    entry->format = format;
    entry->isInstalled = TRUE;
    entry->next = gPlatformFonts;

    /* Add to list */
    gPlatformFonts = entry;
    gPlatformFontCount++;

    return noErr;
}

static OSErr DetectFontFormat(ConstStr255Param filePath, short *format)
{
    char cPath[256];
    char *ext;

    if (filePath == NULL || format == NULL) {
        return paramErr;
    }

    /* Convert to C string */
    BlockMoveData(&filePath[1], cPath, filePath[0]);
    cPath[filePath[0]] = '\0';

    /* Detect by file extension */
    ext = strrchr(cPath, '.');
    if (ext == NULL) {
        return fontCorruptErr;
    }

    if (strcasecmp(ext, ".ttf") == 0 || strcasecmp(ext, ".ttc") == 0) {
        *format = kFontFormatTrueType;
    } else if (strcasecmp(ext, ".otf") == 0) {
        *format = kFontFormatOpenType;
    } else if (strcasecmp(ext, ".pfa") == 0 || strcasecmp(ext, ".pfb") == 0) {
        *format = kFontFormatPostScript;
    } else {
        *format = kFontFormatTrueType; /* Default assumption */
    }

    return noErr;
}

static OSErr ExtractFontName(ConstStr255Param filePath, Str255 fontName)
{
    char cPath[256];
    char *basename;
    char *ext;
    short nameLen;

    if (filePath == NULL || fontName == NULL) {
        return paramErr;
    }

    /* Convert to C string */
    BlockMoveData(&filePath[1], cPath, filePath[0]);
    cPath[filePath[0]] = '\0';

    /* Extract basename */
    basename = strrchr(cPath, '/');
    if (basename == NULL) {
        basename = cPath;
    } else {
        basename++; /* Skip the '/' */
    }

    /* Remove extension */
    ext = strrchr(basename, '.');
    if (ext != NULL) {
        *ext = '\0';
    }

    /* Convert back to Pascal string */
    nameLen = strlen(basename);
    if (nameLen > 255) nameLen = 255;
    fontName[0] = nameLen;
    BlockMoveData(basename, &fontName[1], nameLen);

    return noErr;
}

static PlatformFontEntry *FindPlatformFont(ConstStr255Param fontName)
{
    PlatformFontEntry *entry;

    if (fontName == NULL) {
        return NULL;
    }

    entry = gPlatformFonts;
    while (entry != NULL) {
        if (EqualString(fontName, entry->fontName, FALSE, TRUE)) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static OSErr LoadPlatformTrueTypeFont(ConstStr255Param filePath, TTFont **font)
{
    FSSpec fontFile;
    short refNum;
    Handle fontData;
    long fileSize;
    OSErr error;

    if (filePath == NULL || font == NULL) {
        return paramErr;
    }

    /* Convert path to FSSpec */
    error = FSMakeFSSpec(0, 0, filePath, &fontFile);
    if (error != noErr) {
        return error;
    }

    /* Open font file */
    error = FSpOpenDF(&fontFile, fsRdPerm, &refNum);
    if (error != noErr) {
        return error;
    }

    /* Get file size */
    error = GetEOF(refNum, &fileSize);
    if (error != noErr) {
        FSClose(refNum);
        return error;
    }

    /* Allocate handle for font data */
    fontData = NewHandle(fileSize);
    if (fontData == NULL) {
        FSClose(refNum);
        return fontOutOfMemoryErr;
    }

    /* Read font data */
    HLock(fontData);
    error = FSRead(refNum, &fileSize, *fontData);
    HUnlock(fontData);
    FSClose(refNum);

    if (error != noErr) {
        DisposeHandle(fontData);
        return error;
    }

    /* Parse TrueType font */
    error = LoadTrueTypeFontFromResource(fontData, font);
    if (error != noErr) {
        DisposeHandle(fontData);
        return error;
    }

    return noErr;
}
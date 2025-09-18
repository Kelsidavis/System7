/*
 * FontManagerCore.c - Core Font Manager Implementation
 *
 * Implements the main Font Manager APIs compatible with Mac OS 7.1
 * Provides font loading, family management, and core font operations.
 */

#include "../include/FontManager/FontManager.h"
#include "../include/FontManager/BitmapFonts.h"
#include "../include/FontManager/TrueTypeFonts.h"
#include "../include/FontManager/FontMetrics.h"
#include <Memory.h>
#include <Resources.h>
#include <Errors.h>
#include <string.h>
#include <stdlib.h>

/* Font Manager Global State */
static FontManagerState gFontManagerState = {
    FALSE,          /* initialized */
    FALSE,          /* fractEnable */
    FALSE,          /* scaleDisable */
    FALSE,          /* outlinePreferred */
    FALSE,          /* preserveGlyph */
    FALSE,          /* fontLock */
    NULL,           /* cache */
    NULL,           /* substitutions */
    0,              /* substitutionCount */
    0x00010000,     /* fontGamma (1.0 in Fixed) */
    TRUE,           /* hintingEnabled */
    TRUE            /* smoothingEnabled */
};

/* Font Family Table */
static struct {
    short familyID;
    Str255 familyName;
    Boolean isInstalled;
} gFontFamilyTable[256];
static short gFontFamilyCount = 0;

/* Last error */
static OSErr gLastError = noErr;
static void (*gErrorCallback)(OSErr, const char*) = NULL;

/* Internal helper functions */
static OSErr InitializeFontFamilyTable(void);
static OSErr LoadSystemFonts(void);
static OSErr FindFontResource(short familyID, short size, short style, Handle *fontHandle, short *resourceID);
static OSErr ParseFontName(ConstStr255Param name, short *familyID);
static OSErr RegisterFontFamily(short familyID, ConstStr255Param name);
static void SetLastError(OSErr error, const char *message);

/*
 * InitFonts - Initialize the Font Manager
 *
 * This is the main initialization routine that sets up all Font Manager
 * data structures and loads system fonts.
 */
void InitFonts(void)
{
    OSErr error;

    if (gFontManagerState.initialized) {
        return; /* Already initialized */
    }

    /* Initialize font family table */
    error = InitializeFontFamilyTable();
    if (error != noErr) {
        SetLastError(error, "Failed to initialize font family table");
        return;
    }

    /* Initialize font cache */
    error = InitFontCache(64, 1024 * 1024); /* 64 entries, 1MB cache */
    if (error != noErr) {
        SetLastError(error, "Failed to initialize font cache");
        return;
    }

    /* Load system fonts */
    error = LoadSystemFonts();
    if (error != noErr) {
        SetLastError(error, "Failed to load system fonts");
        return;
    }

    /* Initialize platform font support */
    error = InitializePlatformFonts();
    if (error != noErr) {
        SetLastError(error, "Failed to initialize platform fonts");
        /* Don't fail completely - continue with basic support */
    }

    gFontManagerState.initialized = TRUE;
    SetLastError(noErr, "Font Manager initialized successfully");
}

/*
 * FlushFonts - Flush font caches and reload fonts
 */
OSErr FlushFonts(void)
{
    OSErr error;

    if (!gFontManagerState.initialized) {
        return fontNotFoundErr;
    }

    /* Flush font cache */
    error = FlushFontCache();
    if (error != noErr) {
        SetLastError(error, "Failed to flush font cache");
        return error;
    }

    /* Reload system fonts */
    error = LoadSystemFonts();
    if (error != noErr) {
        SetLastError(error, "Failed to reload system fonts");
        return error;
    }

    SetLastError(noErr, "Fonts flushed successfully");
    return noErr;
}

/*
 * GetFontName - Get the name of a font family
 */
void GetFontName(short familyID, Str255 name)
{
    short i;

    if (!gFontManagerState.initialized) {
        name[0] = 0;
        return;
    }

    /* Search font family table */
    for (i = 0; i < gFontFamilyCount; i++) {
        if (gFontFamilyTable[i].familyID == familyID) {
            BlockMoveData(gFontFamilyTable[i].familyName, name,
                         gFontFamilyTable[i].familyName[0] + 1);
            return;
        }
    }

    /* Font not found - return empty string */
    name[0] = 0;
    SetLastError(fontNotFoundErr, "Font family not found");
}

/*
 * GetFNum - Get the family ID for a font name
 */
void GetFNum(ConstStr255Param name, short *familyID)
{
    OSErr error;

    if (!gFontManagerState.initialized || name == NULL || familyID == NULL) {
        if (familyID) *familyID = 0;
        return;
    }

    error = ParseFontName(name, familyID);
    if (error != noErr) {
        *familyID = 0;
        SetLastError(error, "Font name not found");
    }
}

/*
 * RealFont - Check if a font exists at the specified size
 */
Boolean RealFont(short fontNum, short size)
{
    Handle fontHandle;
    short resourceID;
    OSErr error;

    if (!gFontManagerState.initialized) {
        return FALSE;
    }

    /* Try to find the font resource */
    error = FindFontResource(fontNum, size, 0, &fontHandle, &resourceID);
    if (error == noErr && fontHandle != NULL) {
        /* Font exists */
        return TRUE;
    }

    /* Check if TrueType font exists */
    TTFont *ttFont;
    error = LoadTrueTypeFont(fontNum, &ttFont);
    if (error == noErr && ttFont != NULL) {
        UnloadTrueTypeFont(ttFont);
        return TRUE;
    }

    return FALSE;
}

/*
 * FMSwapFont - Main font swapping routine
 *
 * This is the core font loading function that handles font selection,
 * scaling, and style application.
 */
FMOutPtr FMSwapFont(const FMInput *inRec)
{
    static FMOutput output;
    Handle fontHandle;
    short resourceID;
    OSErr error;
    BitmapFontData *bitmapFont = NULL;
    TTFont *trueTypeFont = NULL;
    FontMetrics metrics;
    WidthTable *widthTable = NULL;

    if (!gFontManagerState.initialized || inRec == NULL) {
        output.errNum = fontNotFoundErr;
        return &output;
    }

    /* Clear output structure */
    memset(&output, 0, sizeof(FMOutput));

    /* Try to load TrueType font first if outline preferred */
    if (gFontManagerState.outlinePreferred || IsOutline(inRec->numer, inRec->denom)) {
        error = LoadTrueTypeFont(inRec->family, &trueTypeFont);
        if (error == noErr && trueTypeFont != NULL) {
            /* Get TrueType font metrics */
            error = GetTrueTypeFontMetrics(trueTypeFont, &metrics);
            if (error == noErr) {
                output.ascent = (unsigned char)(metrics.ascent >> 16);
                output.descent = (unsigned char)(metrics.descent >> 16);
                output.widMax = (unsigned char)(metrics.widMax >> 16);
                output.leading = (char)(metrics.leading >> 16);
                output.fontHandle = (Handle)trueTypeFont;
                output.errNum = noErr;
                return &output;
            }
            UnloadTrueTypeFont(trueTypeFont);
        }
    }

    /* Try to load bitmap font */
    error = FindFontResource(inRec->family, inRec->size, inRec->face, &fontHandle, &resourceID);
    if (error == noErr && fontHandle != NULL) {
        error = LoadBitmapFontFromResource(fontHandle, &bitmapFont);
        if (error == noErr && bitmapFont != NULL) {
            /* Get bitmap font metrics */
            FMetricRec fmetrics;
            error = GetBitmapFontMetrics(bitmapFont, &fmetrics);
            if (error == noErr) {
                output.ascent = (unsigned char)(fmetrics.ascent >> 16);
                output.descent = (unsigned char)(fmetrics.descent >> 16);
                output.widMax = (unsigned char)(fmetrics.widMax >> 16);
                output.leading = (char)(fmetrics.leading >> 16);
                output.fontHandle = (Handle)bitmapFont;

                /* Apply style effects */
                if (inRec->face & bold) {
                    output.boldPixels = 1;
                }
                if (inRec->face & italic) {
                    output.italicPixels = 1;
                }
                if (inRec->face & underline) {
                    output.ulOffset = output.descent / 2;
                    output.ulThick = 1;
                }
                if (inRec->face & shadow) {
                    output.shadowPixels = 1;
                }

                output.errNum = noErr;
                return &output;
            }
            UnloadBitmapFont(bitmapFont);
        }
    }

    /* Font not found - try system font as fallback */
    if (inRec->family != systemFont) {
        FMInput sysInput = *inRec;
        sysInput.family = systemFont;
        return FMSwapFont(&sysInput);
    }

    /* Complete failure */
    output.errNum = fontNotFoundErr;
    SetLastError(fontNotFoundErr, "Font not found");
    return &output;
}

/*
 * FontMetrics - Set current font metrics
 */
void FontMetrics(const FMetricRec *theMetrics)
{
    if (!gFontManagerState.initialized || theMetrics == NULL) {
        return;
    }

    /* Store metrics for current font context */
    /* This is typically used by QuickDraw to cache font metrics */
    /* Implementation would store in current grafport or global state */
}

/*
 * SetFScaleDisable - Enable/disable font scaling
 */
void SetFScaleDisable(Boolean fscaleDisable)
{
    gFontManagerState.scaleDisable = fscaleDisable;
}

/*
 * SetFractEnable - Enable/disable fractional character widths
 */
void SetFractEnable(Boolean fractEnable)
{
    gFontManagerState.fractEnable = fractEnable;
    EnableFractionalWidths(fractEnable);
}

/*
 * IsOutline - Check if font should be rendered as outline
 */
Boolean IsOutline(Point numer, Point denom)
{
    /* Check if scaling would benefit from outline rendering */
    if (numer.h != denom.h || numer.v != denom.v) {
        return gFontManagerState.outlinePreferred;
    }

    return gFontManagerState.outlinePreferred;
}

/*
 * SetOutlinePreferred - Set outline font preference
 */
void SetOutlinePreferred(Boolean outlinePreferred)
{
    gFontManagerState.outlinePreferred = outlinePreferred;
}

/*
 * GetOutlinePreferred - Get outline font preference
 */
Boolean GetOutlinePreferred(void)
{
    return gFontManagerState.outlinePreferred;
}

/*
 * SetPreserveGlyph - Set glyph preservation mode
 */
void SetPreserveGlyph(Boolean preserveGlyph)
{
    gFontManagerState.preserveGlyph = preserveGlyph;
}

/*
 * GetPreserveGlyph - Get glyph preservation mode
 */
Boolean GetPreserveGlyph(void)
{
    return gFontManagerState.preserveGlyph;
}

/*
 * SetFontLock - Enable/disable font locking
 */
void SetFontLock(Boolean lockFlag)
{
    gFontManagerState.fontLock = lockFlag;
}

/*
 * GetDefFontSize - Get default font size
 */
short GetDefFontSize(void)
{
    return 12; /* Default 12-point font */
}

/*
 * GetSysFont - Get system font ID
 */
short GetSysFont(void)
{
    return systemFont;
}

/*
 * GetAppFont - Get application font ID
 */
short GetAppFont(void)
{
    return applFont;
}

/*
 * C-style font name functions for compatibility
 */
void getfnum(char *theName, short *familyID)
{
    Str255 pName;

    if (theName == NULL || familyID == NULL) {
        return;
    }

    /* Convert C string to Pascal string */
    pName[0] = strlen(theName);
    if (pName[0] > 255) pName[0] = 255;
    BlockMoveData(theName, &pName[1], pName[0]);

    GetFNum(pName, familyID);
}

void getfontname(short familyID, char *theName)
{
    Str255 pName;

    if (theName == NULL) {
        return;
    }

    GetFontName(familyID, pName);

    /* Convert Pascal string to C string */
    if (pName[0] > 0) {
        BlockMoveData(&pName[1], theName, pName[0]);
        theName[pName[0]] = '\0';
    } else {
        theName[0] = '\0';
    }
}

/*
 * GetFontManagerState - Get current Font Manager state
 */
FontManagerState *GetFontManagerState(void)
{
    return &gFontManagerState;
}

/*
 * Error handling functions
 */
OSErr GetLastFontError(void)
{
    return gLastError;
}

void SetFontErrorCallback(void (*callback)(OSErr error, const char *message))
{
    gErrorCallback = callback;
}

/* Internal helper functions */

static OSErr InitializeFontFamilyTable(void)
{
    gFontFamilyCount = 0;

    /* Register built-in system fonts */
    RegisterFontFamily(systemFont, "\pChicago");
    RegisterFontFamily(applFont, "\pGeneva");
    RegisterFontFamily(newYork, "\pNew York");
    RegisterFontFamily(geneva, "\pGeneva");
    RegisterFontFamily(monaco, "\pMonaco");
    RegisterFontFamily(venice, "\pVenice");
    RegisterFontFamily(london, "\pLondon");
    RegisterFontFamily(athens, "\pAthens");
    RegisterFontFamily(sanFran, "\pSan Francisco");
    RegisterFontFamily(toronto, "\pToronto");
    RegisterFontFamily(cairo, "\pCairo");
    RegisterFontFamily(losAngeles, "\pLos Angeles");
    RegisterFontFamily(times, "\pTimes");
    RegisterFontFamily(helvetica, "\pHelvetica");
    RegisterFontFamily(courier, "\pCourier");
    RegisterFontFamily(symbol, "\pSymbol");
    RegisterFontFamily(mobile, "\pMobile");

    return noErr;
}

static OSErr LoadSystemFonts(void)
{
    /* This would typically scan system font directories and load font resources */
    /* For now, we assume fonts are available through the Resource Manager */
    return noErr;
}

static OSErr FindFontResource(short familyID, short size, short style, Handle *fontHandle, short *resourceID)
{
    Handle resource;
    short resID;

    if (fontHandle == NULL || resourceID == NULL) {
        return paramErr;
    }

    *fontHandle = NULL;
    *resourceID = 0;

    /* Try NFNT resource first */
    resID = (familyID * 128) + size; /* Simplified resource ID calculation */
    resource = GetResource(kNFNTResourceType, resID);
    if (resource != NULL) {
        *fontHandle = resource;
        *resourceID = resID;
        return noErr;
    }

    /* Try FONT resource */
    resource = GetResource(kFONTResourceType, resID);
    if (resource != NULL) {
        *fontHandle = resource;
        *resourceID = resID;
        return noErr;
    }

    return fontNotFoundErr;
}

static OSErr ParseFontName(ConstStr255Param name, short *familyID)
{
    short i;

    if (name == NULL || familyID == NULL) {
        return paramErr;
    }

    /* Search font family table */
    for (i = 0; i < gFontFamilyCount; i++) {
        if (EqualString(name, gFontFamilyTable[i].familyName, FALSE, TRUE)) {
            *familyID = gFontFamilyTable[i].familyID;
            return noErr;
        }
    }

    return fontNotFoundErr;
}

static OSErr RegisterFontFamily(short familyID, ConstStr255Param name)
{
    if (gFontFamilyCount >= 256) {
        return fontCacheFullErr;
    }

    gFontFamilyTable[gFontFamilyCount].familyID = familyID;
    BlockMoveData(name, gFontFamilyTable[gFontFamilyCount].familyName, name[0] + 1);
    gFontFamilyTable[gFontFamilyCount].isInstalled = TRUE;
    gFontFamilyCount++;

    return noErr;
}

static void SetLastError(OSErr error, const char *message)
{
    gLastError = error;

    if (gErrorCallback != NULL && message != NULL) {
        gErrorCallback(error, message);
    }
}
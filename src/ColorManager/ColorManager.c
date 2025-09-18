/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * ColorManager.c - Color Manager Implementation
 * System 7.1 color management functionality
 */

#include "ColorManager/ColorTypes.h"
#include <stdlib.h>
#include <string.h>

/* Global Color Manager State */
static RGBColor gForeColor = {0x0000, 0x0000, 0x0000};  /* Black foreground */
static RGBColor gBackColor = {0xFFFF, 0xFFFF, 0xFFFF};  /* White background */
static CTabHandle gDeviceColors = NULL;                  /* Current device color table */
static int16_t gLastError = noErr;                       /* Last error code */

/* Internal state tracking */
static PaletteHandle gCurrentPalette = NULL;     /* Current active palette */
static GDHandle gMainDevice = NULL;              /* Main graphics device */
static Boolean gColorManagerInitialized = false;

/* Core Color Drawing Functions */

/* RGBForeColor - Set foreground RGB color */
void RGBForeColor(const RGBColor* rgb) {
    if (rgb == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Copy RGB values to global foreground color */
    gForeColor.red = rgb->red;
    gForeColor.green = rgb->green;
    gForeColor.blue = rgb->blue;

    gLastError = noErr;
}

/* RGBBackColor - Set background RGB color */
void RGBBackColor(const RGBColor* rgb) {
    if (rgb == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Copy RGB values to global background color */
    gBackColor.red = rgb->red;
    gBackColor.green = rgb->green;
    gBackColor.blue = rgb->blue;

    gLastError = noErr;
}

/* GetForeColor - Get current foreground color */
void GetForeColor(RGBColor* rgb) {
    if (rgb == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Return current foreground color */
    rgb->red = gForeColor.red;
    rgb->green = gForeColor.green;
    rgb->blue = gForeColor.blue;

    gLastError = noErr;
}

/* GetBackColor - Get current background color */
void GetBackColor(RGBColor* rgb) {
    if (rgb == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Return current background color */
    rgb->red = gBackColor.red;
    rgb->green = gBackColor.green;
    rgb->blue = gBackColor.blue;

    gLastError = noErr;
}

/* GetCPixel - Get color of pixel at coordinates */
void GetCPixel(int16_t h, int16_t v, RGBColor* cPix) {
    if (cPix == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Simplified implementation - return foreground color */
    /* Real implementation would read from current port's pixel map */
    cPix->red = gForeColor.red;
    cPix->green = gForeColor.green;
    cPix->blue = gForeColor.blue;

    gLastError = noErr;
}

/* SetCPixel - Set color of pixel at coordinates */
void SetCPixel(int16_t h, int16_t v, const RGBColor* cPix) {
    if (cPix == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Simplified implementation - would draw to current port */
    /* Real implementation would write to current port's pixel map */

    gLastError = noErr;
}

/* Color Table Management */

/* GetCTable - Get color table resource by ID */
CTabHandle GetCTable(int16_t ctID) {
    /* Allocate ColorTable handle */
    CTabHandle ctabH = (CTabHandle)NewHandle(sizeof(ColorTable*));
    if (ctabH == NULL) {
        gLastError = memFullErr;
        return NULL;
    }

    /* Allocate ColorTable structure with basic size */
    size_t tableSize = sizeof(ColorTable) + (256 * sizeof(ColorSpec));
    *ctabH = (ColorTable*)NewPtr(tableSize);
    if (*ctabH == NULL) {
        DisposeHandle((Handle)ctabH);
        gLastError = memFullErr;
        return NULL;
    }

    /* Initialize basic color table */
    (*ctabH)->ctSeed = 0;
    (*ctabH)->ctFlags = 0;
    (*ctabH)->ctSize = 255; /* 256 entries - 1 */

    /* Initialize basic color entries */
    for (int i = 0; i <= 255; i++) {
        (*ctabH)->ctTable[i].value = i;
        (*ctabH)->ctTable[i].rgb.red = (i * 257);   /* Scale 8-bit to 16-bit */
        (*ctabH)->ctTable[i].rgb.green = (i * 257);
        (*ctabH)->ctTable[i].rgb.blue = (i * 257);
    }

    gLastError = noErr;
    return ctabH;
}

/* DisposeCTable - Dispose of color table handle */
void DisposeCTable(CTabHandle cTabH) {
    if (cTabH == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Free ColorTable data and handle */
    if (*cTabH != NULL) {
        DisposePtr((Ptr)*cTabH);
    }
    DisposeHandle((Handle)cTabH);

    gLastError = noErr;
}

/* CopyColorTable - Create a copy of a color table */
CTabHandle CopyColorTable(CTabHandle srcTab) {
    if (srcTab == NULL || *srcTab == NULL) {
        gLastError = paramErr;
        return NULL;
    }

    /* Calculate table size */
    size_t tableSize = sizeof(ColorTable) + (((*srcTab)->ctSize + 1) * sizeof(ColorSpec));

    /* Allocate new handle */
    CTabHandle newTab = (CTabHandle)NewHandle(sizeof(ColorTable*));
    if (newTab == NULL) {
        gLastError = memFullErr;
        return NULL;
    }

    /* Allocate ColorTable structure */
    *newTab = (ColorTable*)NewPtr(tableSize);
    if (*newTab == NULL) {
        DisposeHandle((Handle)newTab);
        gLastError = memFullErr;
        return NULL;
    }

    /* Copy color table data */
    BlockMove((Ptr)*srcTab, (Ptr)*newTab, tableSize);

    gLastError = noErr;
    return newTab;
}

/* Color Conversion Functions */

/* Index2Color - Convert color table index to RGB */
void Index2Color(int32_t index, RGBColor* aColor) {
    if (aColor == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Use current device color table if available */
    if (gDeviceColors != NULL && *gDeviceColors != NULL) {
        int16_t ctSize = (*gDeviceColors)->ctSize;
        if (index >= 0 && index <= ctSize) {
            *aColor = (*gDeviceColors)->ctTable[index].rgb;
            gLastError = noErr;
            return;
        }
    }

    /* Default grayscale conversion */
    uint16_t gray = (index & 0xFF) * 257; /* Scale 8-bit to 16-bit */
    aColor->red = gray;
    aColor->green = gray;
    aColor->blue = gray;

    gLastError = noErr;
}

/* Color2Index - Convert RGB color to index */
int32_t Color2Index(const RGBColor* myColor) {
    if (myColor == NULL) {
        gLastError = paramErr;
        return 0;
    }

    /* Simplified color matching - convert to grayscale index */
    /* Real implementation would use inverse color table */
    uint32_t gray = (myColor->red + myColor->green + myColor->blue) / 3;
    int32_t index = gray / 257; /* Scale 16-bit to 8-bit */

    gLastError = noErr;
    return index;
}

/* InvertColor - Invert RGB color values */
void InvertColor(RGBColor* myColor) {
    if (myColor == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Invert each RGB component */
    myColor->red = 0xFFFF - myColor->red;
    myColor->green = 0xFFFF - myColor->green;
    myColor->blue = 0xFFFF - myColor->blue;

    gLastError = noErr;
}

/* RealColor - Test if color is available */
Boolean RealColor(const RGBColor* rgb) {
    if (rgb == NULL) {
        gLastError = paramErr;
        return false;
    }

    /* Simplified implementation - assume all colors are available */
    /* Real implementation would check against current device color table */
    gLastError = noErr;
    return true;
}

/* Inverse Table Functions */

/* MakeITable - Create inverse color table */
OSErr MakeITable(CTabHandle cTabH, ITabHandle iTabH, int16_t res) {
    if (cTabH == NULL || iTabH == NULL) {
        gLastError = paramErr;
        return paramErr;
    }

    /* Allocate inverse table if needed */
    if (*iTabH == NULL) {
        int32_t tableSize = sizeof(ITab) + (res * res * res); /* Cubic resolution */
        *iTabH = (ITab*)NewPtr(tableSize);
        if (*iTabH == NULL) {
            gLastError = memFullErr;
            return memFullErr;
        }
    }

    /* Initialize inverse table header */
    (*iTabH)->iTabSeed = (*cTabH)->ctSeed;
    (*iTabH)->iTabRes = res;

    /* Build inverse table mapping */
    for (int32_t i = 0; i < res * res * res; i++) {
        /* Simplified mapping - use modulo for demonstration */
        (*iTabH)->iTTable[i] = i % ((*cTabH)->ctSize + 1);
    }

    gLastError = noErr;
    return noErr;
}

/* Palette Management Functions */

/* NewPalette - Create new palette */
PaletteHandle NewPalette(int16_t entries, CTabHandle srcColors,
                        int16_t srcUsage, int16_t srcTolerance) {
    /* Allocate palette handle */
    PaletteHandle palH = (PaletteHandle)NewHandle(sizeof(Palette*));
    if (palH == NULL) {
        gLastError = memFullErr;
        return NULL;
    }

    /* Allocate palette structure */
    size_t palSize = sizeof(Palette) + (entries * sizeof(PaletteEntry));
    *palH = (Palette*)NewPtr(palSize);
    if (*palH == NULL) {
        DisposeHandle((Handle)palH);
        gLastError = memFullErr;
        return NULL;
    }

    /* Initialize palette header */
    (*palH)->pmEntries = entries;
    (*palH)->pmDataFields = 0;

    /* Copy colors from source color table if provided */
    if (srcColors != NULL && *srcColors != NULL) {
        int16_t copyCount = (entries <= (*srcColors)->ctSize + 1) ? entries : (*srcColors)->ctSize + 1;
        for (int i = 0; i < copyCount; i++) {
            (*palH)->pmInfo[i].rgb = (*srcColors)->ctTable[i].rgb;
            (*palH)->pmInfo[i].usage = srcUsage;
            (*palH)->pmInfo[i].tolerance = srcTolerance;
        }
    }

    gLastError = noErr;
    return palH;
}

/* DisposePalette - Dispose of palette handle */
void DisposePalette(PaletteHandle srcPalette) {
    if (srcPalette == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Clear current palette if this is it */
    if (gCurrentPalette == srcPalette) {
        gCurrentPalette = NULL;
    }

    /* Free palette data and handle */
    if (*srcPalette != NULL) {
        DisposePtr((Ptr)*srcPalette);
    }
    DisposeHandle((Handle)srcPalette);

    gLastError = noErr;
}

/* ActivatePalette - Activate palette for window */
void ActivatePalette(WindowPtr srcWindow) {
    /* Simplified implementation - just mark as current */
    /* Real implementation would update window's color environment */

    gLastError = noErr;
}

/* SetPalette - Set palette for window */
void SetPalette(WindowPtr dstWindow, PaletteHandle srcPalette, Boolean cUpdates) {
    if (srcPalette == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Set as current palette */
    gCurrentPalette = srcPalette;

    /* Update color environment if requested */
    if (cUpdates) {
        /* Real implementation would update window's color environment */
    }

    gLastError = noErr;
}

/* GetPalette - Get palette for window */
PaletteHandle GetPalette(WindowPtr srcWindow) {
    /* Return current palette */
    /* Real implementation would get window-specific palette */

    gLastError = noErr;
    return gCurrentPalette;
}

/* Extended Color Table Functions */

void GetSubTable(CTabHandle myColors, int16_t iTabRes, CTabHandle targetTbl) {
    /* Basic implementation */
    gLastError = noErr;
}

void ProtectEntry(int16_t index, Boolean protect) {
    /* Basic implementation */
    gLastError = noErr;
}

void ReserveEntry(int16_t index, Boolean reserve) {
    /* Basic implementation */
    gLastError = noErr;
}

void SetEntries(int16_t start, int16_t count, CTabHandle srcTable) {
    /* Basic implementation */
    gLastError = noErr;
}

void RestoreEntries(CTabHandle srcTable, CTabHandle dstTable, ReqListRec* selection) {
    /* Basic implementation */
    gLastError = noErr;
}

void SaveEntries(CTabHandle srcTable, CTabHandle resultTable, ReqListRec* selection) {
    /* Basic implementation */
    gLastError = noErr;
}

/* Palette Color Functions */

void PmForeColor(int16_t dstEntry) {
    /* Basic implementation */
    gLastError = noErr;
}

void PmBackColor(int16_t dstEntry) {
    /* Basic implementation */
    gLastError = noErr;
}

void AnimateEntry(WindowPtr dstWindow, int16_t dstEntry, const RGBColor* srcRGB) {
    /* Basic implementation */
    gLastError = noErr;
}

void AnimatePalette(WindowPtr dstWindow, CTabHandle srcCTab, int16_t srcIndex,
                   int16_t dstEntry, int16_t dstLength) {
    /* Basic implementation */
    gLastError = noErr;
}

void GetEntryColor(PaletteHandle srcPalette, int16_t srcEntry, RGBColor* dstRGB) {
    if (srcPalette == NULL || *srcPalette == NULL || dstRGB == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Get color from palette entry */
    if (srcEntry >= 0 && srcEntry < (*srcPalette)->pmEntries) {
        *dstRGB = (*srcPalette)->pmInfo[srcEntry].rgb;
        gLastError = noErr;
    } else {
        gLastError = paramErr;
    }
}

void SetEntryColor(PaletteHandle dstPalette, int16_t dstEntry, const RGBColor* srcRGB) {
    if (dstPalette == NULL || *dstPalette == NULL || srcRGB == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Set color in palette entry */
    if (dstEntry >= 0 && dstEntry < (*dstPalette)->pmEntries) {
        (*dstPalette)->pmInfo[dstEntry].rgb = *srcRGB;
        gLastError = noErr;
    } else {
        gLastError = paramErr;
    }
}

void GetEntryUsage(PaletteHandle srcPalette, int16_t srcEntry,
                  int16_t* dstUsage, int16_t* dstTolerance) {
    if (srcPalette == NULL || *srcPalette == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Get usage and tolerance from palette entry */
    if (srcEntry >= 0 && srcEntry < (*srcPalette)->pmEntries) {
        if (dstUsage) *dstUsage = (*srcPalette)->pmInfo[srcEntry].usage;
        if (dstTolerance) *dstTolerance = (*srcPalette)->pmInfo[srcEntry].tolerance;
        gLastError = noErr;
    } else {
        gLastError = paramErr;
    }
}

void SetEntryUsage(PaletteHandle dstPalette, int16_t dstEntry,
                  int16_t srcUsage, int16_t srcTolerance) {
    if (dstPalette == NULL || *dstPalette == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Set usage and tolerance in palette entry */
    if (dstEntry >= 0 && dstEntry < (*dstPalette)->pmEntries) {
        (*dstPalette)->pmInfo[dstEntry].usage = srcUsage;
        (*dstPalette)->pmInfo[dstEntry].tolerance = srcTolerance;
        gLastError = noErr;
    } else {
        gLastError = paramErr;
    }
}

/* CopyPalette - Copy palette entries */
void CopyPalette(PaletteHandle srcPalette, PaletteHandle dstPalette,
                int16_t srcEntry, int16_t dstEntry, int16_t dstLength) {
    if (srcPalette == NULL || *srcPalette == NULL ||
        dstPalette == NULL || *dstPalette == NULL) {
        gLastError = paramErr;
        return;
    }

    /* Copy palette entries */
    int16_t srcMax = (*srcPalette)->pmEntries;
    int16_t dstMax = (*dstPalette)->pmEntries;

    for (int16_t i = 0; i < dstLength; i++) {
        if (srcEntry + i >= srcMax || dstEntry + i >= dstMax) break;

        (*dstPalette)->pmInfo[dstEntry + i] = (*srcPalette)->pmInfo[srcEntry + i];
    }

    gLastError = noErr;
}

/* Error Handling */

int16_t QDError(void) {
    /* Return last QuickDraw error */
    return gLastError;
}

/* Color Manager Initialization */

OSErr ColorManager_Init(void) {
    if (gColorManagerInitialized) {
        return noErr;
    }

    /* Initialize default color table */
    gDeviceColors = GetCTable(0);
    if (gDeviceColors == NULL) {
        return memFullErr;
    }

    /* Initialize default colors */
    gForeColor.red = 0x0000;
    gForeColor.green = 0x0000;
    gForeColor.blue = 0x0000;

    gBackColor.red = 0xFFFF;
    gBackColor.green = 0xFFFF;
    gBackColor.blue = 0xFFFF;

    gColorManagerInitialized = true;
    gLastError = noErr;
    return noErr;
}

void ColorManager_Cleanup(void) {
    if (!gColorManagerInitialized) {
        return;
    }

    /* Dispose of device color table */
    if (gDeviceColors != NULL) {
        DisposeCTable(gDeviceColors);
        gDeviceColors = NULL;
    }

    /* Dispose of current palette if any */
    if (gCurrentPalette != NULL) {
        DisposePalette(gCurrentPalette);
        gCurrentPalette = NULL;
    }

    gColorManagerInitialized = false;
}

/* Global State Access */

RGBColor* ColorManager_GetForeColorPtr(void) {
    return &gForeColor;
}

RGBColor* ColorManager_GetBackColorPtr(void) {
    return &gBackColor;
}

CTabHandle ColorManager_GetDeviceColors(void) {
    return gDeviceColors;
}
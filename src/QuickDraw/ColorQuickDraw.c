/* #include "SystemTypes.h" */
#include "QuickDraw/QuickDrawInternal.h"
#include <stdlib.h>
#include <string.h>
/*
 * ColorQuickDraw.c - Color QuickDraw Implementation
 *
 * Implementation of Color QuickDraw extensions including 32-bit color support,
 * color tables, pixel depth handling, and color graphics ports.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Color QuickDraw
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "QuickDrawConstants.h"

#include "QuickDraw/ColorQuickDraw.h"
#include "QuickDraw/QDRegions.h"
#include "PatternMgr/pattern_manager.h"
#include <math.h>
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDraw/QuickDrawPlatform.h"

/* QuickDraw globals from QuickDrawCore.c */
extern QDGlobals qd;

/* Color QuickDraw globals */
static Boolean g_colorQDAvailable = false;
CGrafPtr g_currentCPort = NULL;  /* Exported for QDPlatform_DrawGlyph */
static GDHandle g_currentDevice = NULL;
static GDHandle g_deviceList = NULL;
static GDHandle g_mainDevice = NULL;
static SInt32 g_colorSeed = 1;

/* Standard RGB colors */
const RGBColor kRGBBlack = {0, 0, 0};
const RGBColor kRGBWhite = {65535, 65535, 65535};
const RGBColor kRGBRed = {65535, 0, 0};
const RGBColor kRGBGreen = {0, 65535, 0};
const RGBColor kRGBBlue = {0, 0, 65535};
const RGBColor kRGBCyan = {0, 65535, 65535};
const RGBColor kRGBMagenta = {65535, 0, 65535};
const RGBColor kRGBYellow = {65535, 65535, 0};

/* Forward declarations */
static PixMapHandle CreatePixMap(SInt16 width, SInt16 height, SInt16 depth);
static CTabHandle CreateColorTable(SInt16 numColors);
static void InitializeDevice(GDHandle device, SInt16 depth);
static RGBColor IndexToRGB(SInt32 index);
static SInt32 RGBToIndex(const RGBColor *color);
static void UpdateColorTable(CTabHandle cTable);

/* ================================================================
 * COLOR QUICKDRAW INITIALIZATION
 * ================================================================ */

Boolean ColorQDAvailable(void) {
    return g_colorQDAvailable;
}

static void InitializeColorQuickDraw(void) {
    if (g_colorQDAvailable) return;

    /* Create main device */
    g_mainDevice = NewGDevice(0, 0);
    if (!g_mainDevice) return;

    g_deviceList = g_mainDevice;
    g_currentDevice = g_mainDevice;

    /* Initialize main device with 8-bit color */
    InitializeDevice(g_mainDevice, 8);

    g_colorQDAvailable = true;
}

/* ================================================================
 * COLOR PORT MANAGEMENT
 * ================================================================ */

void InitCPort(CGrafPtr port) {
    assert(port != NULL);

    if (!g_colorQDAvailable) {
        InitializeColorQuickDraw();
    }

    /* Initialize basic port structure */
    port->device = 0;
    port->portVersion = 0xC000; /* High bits set for color port */

    /* Create port pixmap */
    port->portPixMap = NewPixMap();
    if (!port->portPixMap) return;

    /* Initialize port rectangle to actual screen bounds */
    port->portRect = qd.screenBits.bounds;

    /* Create regions */
    port->visRgn = NewRgn();
    port->clipRgn = NewRgn();

    if (!port->visRgn || !port->clipRgn) return;

    /* Set default visible region to port rect */
    RectRgn(port->visRgn, &port->portRect);

    /* Set default clip region to very large rect */
    Rect bigRect;
    SetRect(&bigRect, -32768, -32768, 32767, 32767);
    RectRgn(port->clipRgn, &bigRect);

    /* Initialize color settings */
    port->rgbFgColor = kRGBBlack;
    port->rgbBkColor = kRGBWhite;

    /* Create pixel patterns */
    port->bkPixPat = NewPixPat();
    port->pnPixPat = NewPixPat();
    port->fillPixPat = NewPixPat();

    /* Initialize pen */
    port->pnLoc.h = 0;
    port->pnLoc.v = 0;
    port->pnSize.h = 1;
    port->pnSize.v = 1;
    port->pnMode = patCopy;
    port->pnVis = 0;
    port->pnLocHFrac = 0;

    /* Initialize text settings */
    port->txFont = 0;
    port->txFace = normal;
    port->txMode = srcOr;
    port->txSize = 0;
    port->spExtra = 0;
    port->chExtra = 0;

    /* Initialize color indices */
    port->fgColor = blackColor;
    port->bkColor = whiteColor;

    /* Create graphics variables */
    port->grafVars = (Handle)calloc(1, sizeof(GrafVars));

    /* Clear other fields */
    port->colrBit = 0;
    port->patStretch = 0;
    port->picSave = NULL;
    port->rgnSave = NULL;
    port->polySave = NULL;
    port->grafProcs = NULL;
}

void OpenCPort(CGrafPtr port) {
    InitCPort(port);
    SetCPort(port);
}

void CloseCPort(CGrafPtr port) {
    if (port == NULL) return;

    /* Dispose of pixmap */
    if (port->portPixMap) {
        DisposePixMap(port->portPixMap);
    }

    /* Dispose of regions */
    if (port->visRgn) DisposeRgn(port->visRgn);
    if (port->clipRgn) DisposeRgn(port->clipRgn);

    /* Dispose of pixel patterns */
    if (port->bkPixPat) DisposePixPat(port->bkPixPat);
    if (port->pnPixPat) DisposePixPat(port->pnPixPat);
    if (port->fillPixPat) DisposePixPat(port->fillPixPat);

    /* Dispose of graphics variables */
    if (port->grafVars) {
        free(port->grafVars);
    }

    /* Clear saved handles */
    port->picSave = NULL;
    port->rgnSave = NULL;
    port->polySave = NULL;

    /* If this was the current port, clear it */
    if (g_currentCPort == port) {
        g_currentCPort = NULL;
    }
}

void SetCPort(CGrafPtr port) {
    g_currentCPort = port;
    /* Also set as regular port for compatibility */
    SetPort((GrafPtr)port);
}

void GetCPort(CGrafPtr *port) {
    assert(port != NULL);
    *port = g_currentCPort;
}

/* ================================================================
 * PIXMAP MANAGEMENT
 * ================================================================ */

PixMapHandle NewPixMap(void) {
    PixMapHandle pm = (PixMapHandle)calloc(1, sizeof(PixMapPtr));
    if (!pm) return NULL;

    PixMap *pixMap = (PixMap *)calloc(1, sizeof(PixMap));
    if (!pixMap) {
        free(pm);
        return NULL;
    }

    *pm = pixMap;

    /* Initialize with default values */
    pixMap->baseAddr = NULL;
    pixMap->rowBytes = 0x8000; /* Set high bit to indicate PixMap */
    SetRect(&pixMap->bounds, 0, 0, 0, 0);
    pixMap->pmVersion = 0;
    pixMap->packType = 0;
    pixMap->packSize = 0;
    pixMap->hRes = 0x00480000; /* 72 DPI */
    pixMap->vRes = 0x00480000; /* 72 DPI */
    pixMap->pixelType = 0;
    pixMap->pixelSize = 8;     /* Default to 8-bit */
    pixMap->cmpCount = 1;
    pixMap->cmpSize = 8;
    pixMap->planeBytes = 0;
    pixMap->pmTable = NULL;
    pixMap->pmReserved = 0;

    return pm;
}

void DisposePixMap(PixMapHandle pm) {
    if (!pm || !*pm) return;

    PixMap *pixMap = *pm;

    /* Dispose of color table */
    if (pixMap->pmTable) {
        DisposeCTable((CTabHandle)pixMap->pmTable);
    }

    free(pixMap);
    free(pm);
}

void CopyPixMap(PixMapHandle srcPM, PixMapHandle dstPM) {
    assert(srcPM != NULL && *srcPM != NULL);
    assert(dstPM != NULL && *dstPM != NULL);

    PixMap *src = *srcPM;
    PixMap *dst = *dstPM;

    /* Copy pixmap structure */
    *dst = *src;

    /* Duplicate color table if present */
    if (src->pmTable) {
        ColorTable* srcTable = (ColorTable*)*((Handle)src->pmTable);
        dst->pmTable = (Handle)GetCTable(srcTable->ctSize + 1);
        if (dst->pmTable) {
            ColorTable* dstTable = (ColorTable*)*((Handle)dst->pmTable);
            memcpy(dstTable, srcTable,
                   sizeof(ColorTable) + srcTable->ctSize * sizeof(ColorSpec));
        }
    }
}

void SetPortPix(PixMapHandle pm) {
    assert(g_currentCPort != NULL);
    assert(pm != NULL);

    if (g_currentCPort->portPixMap) {
        DisposePixMap(g_currentCPort->portPixMap);
    }

    g_currentCPort->portPixMap = pm;
}

/* ================================================================
 * PIXPAT MANAGEMENT
 * ================================================================ */

PixPatHandle NewPixPat(void) {
    PixPatHandle pp = (PixPatHandle)calloc(1, sizeof(PixPatPtr));
    if (!pp) return NULL;

    PixPat *pixPat = (PixPat *)calloc(1, sizeof(PixPat));
    if (!pixPat) {
        free(pp);
        return NULL;
    }

    *pp = pixPat;

    /* Initialize with default values */
    pixPat->patType = 0; /* Bitmap pattern */
    pixPat->patMap = NewPixMap();
    pixPat->patData = NULL;
    pixPat->patXData = NULL;
    pixPat->patXValid = 0;
    pixPat->patXMap = NULL;

    /* Initialize 1-bit pattern to black */
    memset(&pixPat->pat1Data, 0xFF, sizeof(Pattern));

    return pp;
}

void DisposePixPat(PixPatHandle pp) {
    if (!pp || !*pp) return;

    PixPat *pixPat = *pp;

    /* Dispose of pixmap */
    if (pixPat->patMap) {
        DisposePixMap(pixPat->patMap);
    }

    /* Dispose of data handles */
    if (pixPat->patData) free(pixPat->patData);
    if (pixPat->patXData) free(pixPat->patXData);
    if (pixPat->patXMap) free(pixPat->patXMap);

    free(pixPat);
    free(pp);
}

void CopyPixPat(PixPatHandle srcPP, PixPatHandle dstPP) {
    assert(srcPP != NULL && *srcPP != NULL);
    assert(dstPP != NULL && *dstPP != NULL);

    PixPat *src = *srcPP;
    PixPat *dst = *dstPP;

    /* Dispose of existing data */
    if (dst->patMap) DisposePixMap(dst->patMap);
    if (dst->patData) free(dst->patData);
    if (dst->patXData) free(dst->patXData);
    if (dst->patXMap) free(dst->patXMap);

    /* Copy structure */
    *dst = *src;

    /* Duplicate pixmap */
    if (src->patMap) {
        dst->patMap = NewPixMap();
        if (dst->patMap) {
            CopyPixMap(src->patMap, dst->patMap);
        }
    }

    /* Note: Data handles would need deep copying in full implementation */
}

PixPatHandle GetPixPat(SInt16 patID) {
    /* Try to load actual ppat resource first */
    Handle h = PM_LoadPPAT(patID);
    if (h) {
        /* We have a real ppat resource - return it as a PixPatHandle */
        return (PixPatHandle)h;
    }

    /* Fallback: create a simple pattern */
    PixPatHandle pp = NewPixPat();
    if (!pp) return NULL;

    /* Create a simple checkerboard pattern as fallback */
    MakeRGBPat(pp, (patID % 2) ? &kRGBBlack : &kRGBWhite);

    return pp;
}

void MakeRGBPat(PixPatHandle pp, const RGBColor *myColor) {
    assert(pp != NULL && *pp != NULL);
    assert(myColor != NULL);

    PixPat *pixPat = *pp;

    /* Set pattern type to RGB */
    pixPat->patType = 1; /* RGB pattern */

    /* Store RGB color in pat1Data (simplified) */
    UInt8 gray = (UInt8)((myColor->red + myColor->green + myColor->blue) / (3 * 256));
    memset(&pixPat->pat1Data, gray > 128 ? 0x00 : 0xFF, sizeof(Pattern));

    /* Invalidate expanded data */
    pixPat->patXValid = 0;
}

void PenPixPat(PixPatHandle pp) {
    assert(g_currentCPort != NULL);
    assert(pp != NULL);

    if (g_currentCPort->pnPixPat) {
        DisposePixPat(g_currentCPort->pnPixPat);
    }

    g_currentCPort->pnPixPat = pp;
}

void BackPixPat(PixPatHandle pp) {
    assert(g_currentCPort != NULL);
    assert(pp != NULL);

    if (g_currentCPort->bkPixPat) {
        DisposePixPat(g_currentCPort->bkPixPat);
    }

    g_currentCPort->bkPixPat = pp;
}

/* ================================================================
 * RGB COLOR MANAGEMENT
 * ================================================================ */

void RGBForeColor(const RGBColor *color) {
    if (!color) return;
    if (!g_currentCPort) return;  /* No color port set yet */

    g_currentCPort->rgbFgColor = *color;
    g_currentCPort->fgColor = RGBToIndex(color);
}

void RGBBackColor(const RGBColor *color) {
    if (!color) return;
    if (!g_currentCPort) return;  /* No color port set yet */

    g_currentCPort->rgbBkColor = *color;
    g_currentCPort->bkColor = RGBToIndex(color);
}

void GetForeColor(RGBColor *color) {
    if (!color) return;
    if (!g_currentCPort) {
        *color = kRGBBlack;  /* Default to black */
        return;
    }

    *color = g_currentCPort->rgbFgColor;
}

void GetBackColor(RGBColor *color) {
    if (!color) return;
    if (!g_currentCPort) {
        *color = kRGBWhite;  /* Default to white */
        return;
    }

    *color = g_currentCPort->rgbBkColor;
}

void SetCPixel(SInt16 h, SInt16 v, const RGBColor *cPix) {
    assert(g_currentCPort != NULL);
    assert(cPix != NULL);

    /* Convert RGBColor to native pixel color */
    UInt32 color = QDPlatform_RGBToNative(cPix->red, cPix->green, cPix->blue);

    /* Set pixel in current color port */
    QDPlatform_SetPixel(h, v, color);
}

void GetCPixel(SInt16 h, SInt16 v, RGBColor *cPix) {
    assert(g_currentCPort != NULL);
    assert(cPix != NULL);

    /* Get pixel from current color port */
    UInt32 color = QDPlatform_GetPixel(h, v);

    /* Convert native pixel color to RGBColor */
    QDPlatform_NativeToRGB(color, &cPix->red, &cPix->green, &cPix->blue);
}

/* ================================================================
 * GRAPHICS DEVICE MANAGEMENT
 * ================================================================ */

GDHandle NewGDevice(SInt16 refNum, SInt32 mode) {
    GDHandle gdh = (GDHandle)calloc(1, sizeof(GDPtr));
    if (!gdh) return NULL;

    GDevice *device = (GDevice *)calloc(1, sizeof(GDevice));
    if (!device) {
        free(gdh);
        return NULL;
    }

    *gdh = device;

    /* Initialize device */
    device->gdRefNum = refNum;
    device->gdID = 0;
    device->gdType = 1; /* Color device */
    device->gdITable = NULL;
    device->gdResPref = 4; /* 4-bit resolution */
    device->gdSearchProc = NULL;
    device->gdCompProc = NULL;
    device->gdFlags = (1 << screenActive) | (1 << mainScreen);
    device->gdPMap = NewPixMap();
    device->gdRefCon = 0;
    device->gdNextGD = NULL;
    /* Use actual screen bounds instead of hardcoding 640x480 */
    device->gdRect = qd.screenBits.bounds;
    device->gdMode = mode;
    device->gdCCBytes = 0;
    device->gdCCDepth = 0;
    device->gdCCXData = NULL;
    device->gdCCXMask = NULL;
    device->gdReserved = 0;

    return gdh;
}

void DisposeGDevice(GDHandle gdh) {
    if (!gdh || !*gdh) return;

    GDevice *device = *gdh;

    /* Dispose of pixmap */
    if (device->gdPMap) {
        DisposePixMap(device->gdPMap);
    }

    /* Remove from device list */
    if (g_deviceList == gdh) {
        g_deviceList = (GDHandle)device->gdNextGD;
    } else {
        GDHandle curr = g_deviceList;
        while (curr && *curr) {
            if ((*curr)->gdNextGD == (Handle)gdh) {
                (*curr)->gdNextGD = device->gdNextGD;
                break;
            }
            curr = (GDHandle)(*curr)->gdNextGD;
        }
    }

    free(device);
    free(gdh);
}

void SetGDevice(GDHandle gd) {
    g_currentDevice = gd;
}

GDHandle GetGDevice(void) {
    return g_currentDevice;
}

GDHandle GetDeviceList(void) {
    return g_deviceList;
}

GDHandle GetMainDevice(void) {
    return g_mainDevice;
}

GDHandle GetNextDevice(GDHandle curDevice) {
    if (!curDevice || !*curDevice) return NULL;
    return (GDHandle)(*curDevice)->gdNextGD;
}

Boolean TestDeviceAttribute(GDHandle gdh, SInt16 attribute) {
    if (!gdh || !*gdh) return false;
    return ((*gdh)->gdFlags & (1 << attribute)) != 0;
}

void SetDeviceAttribute(GDHandle gdh, SInt16 attribute, Boolean value) {
    if (!gdh || !*gdh) return;

    if (value) {
        (*gdh)->gdFlags |= (1 << attribute);
    } else {
        (*gdh)->gdFlags &= ~(1 << attribute);
    }
}

/* ================================================================
 * COLOR TABLE MANAGEMENT
 * ================================================================ */

CTabHandle GetCTable(SInt16 ctID) {
    CTabHandle cTable = (CTabHandle)calloc(1, sizeof(CTabPtr));
    if (!cTable) return NULL;

    /* Calculate size needed */
    size_t tableSize = sizeof(ColorTable) + (ctID - 1) * sizeof(ColorSpec);
    ColorTable *table = (ColorTable *)calloc(1, tableSize);
    if (!table) {
        free(cTable);
        return NULL;
    }

    *cTable = table;

    /* Initialize color table */
    table->ctSeed = ++g_colorSeed;
    table->ctFlags = 0;
    table->ctSize = ctID - 1;

    /* Create standard color table */
    for (int i = 0; i < ctID; i++) {
        table->ctTable[i].value = i;
        if (i == 0) {
            table->ctTable[i].rgb = kRGBWhite;
        } else if (i == ctID - 1) {
            table->ctTable[i].rgb = kRGBBlack;
        } else {
            /* Grayscale progression */
            UInt16 gray = (UInt16)(65535 * (ctID - 1 - i) / (ctID - 1));
            table->ctTable[i].rgb.red = gray;
            table->ctTable[i].rgb.green = gray;
            table->ctTable[i].rgb.blue = gray;
        }
    }

    return cTable;
}

void DisposeCTable(CTabHandle cTable) {
    if (!cTable || !*cTable) return;
    free(*cTable);
    free(cTable);
}

SInt32 GetCTSeed(void) {
    return g_colorSeed;
}

/* ================================================================
 * COLOR CONVERSION
 * ================================================================ */

SInt32 Color2Index(const RGBColor *myColor) {
    return RGBToIndex(myColor);
}

void Index2Color(SInt32 index, RGBColor *aColor) {
    assert(aColor != NULL);
    *aColor = IndexToRGB(index);
}

void InvertColor(RGBColor *myColor) {
    assert(myColor != NULL);
    myColor->red = 65535 - myColor->red;
    myColor->green = 65535 - myColor->green;
    myColor->blue = 65535 - myColor->blue;
}

Boolean RealColor(const RGBColor *color) {
    assert(color != NULL);
    /* In a full implementation, this would check if the color
       can be displayed exactly on the current device */
    return true;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static void InitializeDevice(GDHandle device, SInt16 depth) {
    if (!device || !*device) return;

    GDevice *gd = *device;

    /* Set up pixmap for device */
    if (gd->gdPMap) {
        PixMap *pm = *gd->gdPMap;
        pm->pixelSize = depth;
        pm->cmpCount = (depth <= 8) ? 1 : 3;
        pm->cmpSize = (depth <= 8) ? depth : 8;
        pm->bounds = gd->gdRect;
        pm->rowBytes = 0x8000 | ((gd->gdRect.right - gd->gdRect.left) * depth / 8);

        /* Create color table */
        pm->pmTable = (Handle)GetCTable(1 << depth);
    }
}

static RGBColor IndexToRGB(SInt32 index) {
    /* Simple grayscale mapping */
    UInt16 gray = (UInt16)((index & 0xFF) * 257);
    RGBColor color = {gray, gray, gray};
    return color;
}

static SInt32 RGBToIndex(const RGBColor *color) {
    /* Simple grayscale conversion */
    return (color->red + color->green + color->blue) / (3 * 257);
}

/* Color filling operations - simplified implementations */
void FillCRect(const Rect *r, PixPatHandle pp) {
    assert(g_currentCPort != NULL);
    assert(r != NULL);
    assert(pp != NULL);
    FillRect(r, &(*pp)->pat1Data);
}

void FillCOval(const Rect *r, PixPatHandle pp) {
    assert(g_currentCPort != NULL);
    assert(r != NULL);
    assert(pp != NULL);
    FillOval(r, &(*pp)->pat1Data);
}

void FillCRgn(RgnHandle rgn, PixPatHandle pp) {
    assert(g_currentCPort != NULL);
    assert(rgn != NULL);
    assert(pp != NULL);
    FillRgn(rgn, &(*pp)->pat1Data);
}

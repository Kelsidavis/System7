/* Desktop pattern state consumed by QuickDraw. */

#include "PatternMgr/pattern_manager.h"
#include "PatternMgr/pattern_resources.h"
#include "PatternMgr/pram_prefs.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QuickDrawInternal.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowManagerInternal.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Global Pattern Manager state */
static struct {
    bool initialized;
    bool usePixPat;
    Pattern backPat;
    Handle  backPixPat;    /* Opaque PixPat; format handled inside QuickDraw */
    RGBColor backColor;
    uint32_t colorPattern[64];  /* Decoded PPAT8 pixels */
    bool hasColorPattern;
} gPM;

/* External QuickDraw globals */

void PM_Init(void) {
    if (gPM.initialized) return;
    memset(&gPM, 0, sizeof(gPM));

    /* Default classic platinum gray */
    gPM.backColor.red   = 0xC000;
    gPM.backColor.green = 0xC000;
    gPM.backColor.blue  = 0xC000;

    /* A simple 50% stipple pattern */
    static const uint8_t dither[8] = {0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55};
    memcpy(&gPM.backPat.pat, dither, 8);

    gPM.initialized = true;
}

void PM_SetBackPat(const Pattern *pat) {
    if (!pat) return;
    gPM.usePixPat = false;
    gPM.backPat = *pat;
    /* The colour pattern no longer applies. Left set, the desktop went on
     * being erased with it whatever 1-bit pattern was chosen. */
    gPM.hasColorPattern = false;

    /* Store the pattern but DON'T call BackPat() - we only want it on the desktop, not in windows */
    /* The DeskHook will use this pattern directly when drawing the desktop background */
    /* Set white pattern for EraseRect so windows have white background */
    Pattern whitePat;
    memset(&whitePat, 0x00, sizeof(whitePat));  /* 0x00 = white, 0xFF = black */
    UpdateBackgroundPattern(&whitePat);
}

void PM_SetBackPixPat(Handle pixPatHandle) {
    if (!pixPatHandle) return;

    gPM.usePixPat = true;

    /* Release old handle if we have one */
    if (gPM.backPixPat) {
        DisposeHandle(gPM.backPixPat);
    }

    gPM.backPixPat = pixPatHandle; /* Caller transfers ownership to PM */

    HLock(pixPatHandle);
    const uint8_t* data = (const uint8_t*)*pixPatHandle;
    Size sz = GetHandleSize(pixPatHandle);   /* was a fixed 156, from when
                                              * handle sizes were not kept:
                                              * Apple's ppat 16, 182 bytes,
                                              * lost its colour table */

    /* Windows erase white; the pattern is the desktop's alone */
    Pattern whitePat;
    memset(&whitePat, 0x00, sizeof(whitePat));
    UpdateBackgroundPattern(&whitePat);

    if (DecodePPAT8(data, (size_t)sz, gPM.colorPattern)) {
        gPM.hasColorPattern = true;
    } else {
        /* What QuickDraw shows where colour cannot be: the pattern's own
         * black-and-white version, pat1Data, carried in every 'ppat' */
        gPM.hasColorPattern = false;
        if (sz >= 28 && data[0] == 0 && data[1] == 1) {
            memcpy(&gPM.backPat, data + 20, sizeof(Pattern));
        }
    }

    HUnlock(pixPatHandle);
}

void PM_SetBackColor(const RGBColor *rgb) {
    if (!rgb) return;
    gPM.backColor.red = rgb->red;
    gPM.backColor.green = rgb->green;
    gPM.backColor.blue = rgb->blue;
    RGBBackColor(rgb);
}

void PM_GetBackPat(Pattern *pat) {
    if (pat) memcpy(pat, &gPM.backPat, sizeof(Pattern));
}

void PM_GetBackColor(RGBColor *rgb) {
    if (rgb) {
        rgb->red = gPM.backColor.red;
        rgb->green = gPM.backColor.green;
        rgb->blue = gPM.backColor.blue;
    }
}

bool PM_IsPixPatActive(void) {
    return gPM.usePixPat;
}

DesktopPref PM_GetSavedDesktopPref(void) {
    DesktopPref p;
    memset(&p, 0, sizeof(DesktopPref));
    if (!PRAM_LoadDesktopPref(&p)) {
        /* Fallback defaults */
        p.usePixPat = false;
        p.patID = 16; /* kDesktopPatternID from SystemTypes.h */
        p.ppatID = 0;
        p.backColor.red = p.backColor.green = p.backColor.blue = 0xC000;
    }
    return p;
}

void PM_SaveDesktopPref(const DesktopPref *p) {
    if (!p) return;
    PRAM_SaveDesktopPref(p);
}

bool PM_ApplyDesktopPref(const DesktopPref *p) {
    serial_puts("PM_ApplyDesktopPref called\n");

    if (!p) return false;

    /* Set background color */
    PM_SetBackColor(&p->backColor);

    if (p->usePixPat) {
        serial_puts("PM: Using ppat pattern\n");
        Handle h = PM_LoadPPAT(p->ppatID);
        if (!h) {
            serial_puts("PM: Failed to load ppat!\n");
            return false;
        }
        serial_puts("PM: Loaded ppat, calling SetBackPixPat\n");
        PM_SetBackPixPat(h);
        serial_puts("PM: SetBackPixPat done\n");
    } else {
        serial_puts("PM: Using PAT pattern\n");
        Pattern pat;
        if (!PM_LoadPAT(p->patID, &pat)) {
            /* Fall back to default gray pattern */
            memcpy(&pat, &qd.gray, sizeof(Pattern));
        }
        PM_SetBackPat(&pat);
    }

    PM_RedrawDesktop();
    return true;
}

/*
 * PM_RedrawDesktop - paint the desktop again in the current pattern.
 *
 * Through the desk hook, which paints only where no window is. This used to
 * InvalRect the screen rectangle in whatever port was current - a window's,
 * typically - which never reached the desktop, so a new pattern appeared
 * only where something later happened to uncover it.
 */
void PM_RedrawDesktop(void)
{
    if (!g_deskHook) return;
    RgnHandle all = NewRgn();
    if (!all) return;
    Rect screen = qd.screenBits.bounds;
    screen.top = 20;   /* below the menu bar */
    RectRgn(all, &screen);
    g_deskHook(all);
    DisposeRgn(all);
}

bool PM_LoadPAT(int16_t id, Pattern *out) {
    if (!out) return false;
    return LoadPATResource(id, out);
}

Handle PM_LoadPPAT(int16_t id) {
    return LoadPPATResource(id);
}

/* Decode colour pattern ppatID into 64 screen-format pixels, without making
 * it the desktop's. False if it cannot be loaded or decoded. */
bool PM_LoadColorPattern(int16_t ppatID, uint32_t out[64]) {
    Handle h = PM_LoadPPAT(ppatID);
    if (!h || !*h) return false;
    HLock(h);
    bool ok = DecodePPAT8((const uint8_t*)*h, (size_t)GetHandleSize(h), out);
    HUnlock(h);
    DisposeHandle(h);
    return ok;
}

/* Get color pattern data if available */
bool PM_GetColorPattern(uint32_t** patternData) {
    if (gPM.hasColorPattern && patternData) {
        *patternData = gPM.colorPattern;
        return true;
    }
    return false;
}

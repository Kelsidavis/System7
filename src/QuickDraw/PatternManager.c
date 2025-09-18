/*
 * PatternManager.c - QuickDraw Pattern Management with System 7 Resources
 *
 * Manages standard System 7 patterns using embedded resource data.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "QuickDraw/QuickDraw.h"
#include "Resources/ResourceData.h"
#include "MemoryManager/MemoryManager.h"
#include <string.h>

/* Standard pattern handles */
static Pattern* gDesktopPattern = NULL;
static Pattern* gGray25Pattern = NULL;
static Pattern* gGray50Pattern = NULL;
static Pattern* gGray75Pattern = NULL;
static Pattern* gScrollPattern = NULL;

/* System patterns */
static Pattern gWhitePattern = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static Pattern gBlackPattern = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
static Pattern gGrayPattern =  {{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}};
static Pattern gLtGrayPattern = {{0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22}};
static Pattern gDkGrayPattern = {{0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD}};

/* Pattern list for system patterns */
static PatternList gSysPatList;
static Boolean gPatternsInitialized = false;

/* Initialize standard patterns from resources */
OSErr InitStandardPatterns(void) {
    OSErr err;

    if (gPatternsInitialized) {
        return noErr;
    }

    /* Initialize resource data system */
    err = InitResourceData();
    if (err != noErr) {
        return err;
    }

    /* Load standard patterns */
    gDesktopPattern = LoadResourcePattern(kDesktopPatternID);
    if (!gDesktopPattern) {
        return memFullErr;
    }

    gGray25Pattern = LoadResourcePattern(kGray25PatternID);
    if (!gGray25Pattern) {
        return memFullErr;
    }

    gGray50Pattern = LoadResourcePattern(kGray50PatternID);
    if (!gGray50Pattern) {
        return memFullErr;
    }

    gGray75Pattern = LoadResourcePattern(kGray75PatternID);
    if (!gGray75Pattern) {
        return memFullErr;
    }

    gScrollPattern = LoadResourcePattern(kScrollPatternID);
    if (!gScrollPattern) {
        return memFullErr;
    }

    /* Initialize system pattern list */
    gSysPatList.count = 10;
    gSysPatList.patterns[0] = gWhitePattern;
    gSysPatList.patterns[1] = gLtGrayPattern;
    gSysPatList.patterns[2] = *gGray25Pattern;
    gSysPatList.patterns[3] = gGrayPattern;
    gSysPatList.patterns[4] = *gGray50Pattern;
    gSysPatList.patterns[5] = gDkGrayPattern;
    gSysPatList.patterns[6] = *gGray75Pattern;
    gSysPatList.patterns[7] = gBlackPattern;
    gSysPatList.patterns[8] = *gDesktopPattern;
    gSysPatList.patterns[9] = *gScrollPattern;

    gPatternsInitialized = true;
    return noErr;
}

/* Get system patterns */
void GetIndPattern(Pattern* thePat, short patternListID, short index) {
    if (!gPatternsInitialized) {
        InitStandardPatterns();
    }

    if (patternListID == 0 && index > 0 && index <= gSysPatList.count) {
        *thePat = gSysPatList.patterns[index - 1];
    } else {
        /* Default to black pattern */
        *thePat = gBlackPattern;
    }
}

/* Get standard patterns by type */
void GetDesktopPattern(Pattern* thePat) {
    if (!gPatternsInitialized) {
        InitStandardPatterns();
    }
    if (gDesktopPattern) {
        *thePat = *gDesktopPattern;
    }
}

void GetScrollBarPattern(Pattern* thePat) {
    if (!gPatternsInitialized) {
        InitStandardPatterns();
    }
    if (gScrollPattern) {
        *thePat = *gScrollPattern;
    }
}

/* Pattern utilities for drawing */
void BackPat(const Pattern* pat) {
    GrafPtr port;
    GetPort(&port);
    if (port && pat) {
        port->bkPat = *pat;
    }
}

void PenPat(const Pattern* pat) {
    GrafPtr port;
    GetPort(&port);
    if (port && pat) {
        port->pnPat = *pat;
    }
}

void FillPat(const Pattern* pat) {
    GrafPtr port;
    GetPort(&port);
    if (port && pat) {
        port->fillPat = *pat;
    }
}

/* Pattern fill operations */
void FillRect(const Rect* r, const Pattern* pat) {
    if (!r || !pat) return;

    GrafPtr port;
    GetPort(&port);
    if (!port) return;

    /* Save current pattern */
    Pattern oldPat = port->fillPat;

    /* Set new pattern */
    port->fillPat = *pat;

    /* Perform fill operation */
    /* TODO: Call QuickDraw fill implementation */

    /* Restore pattern */
    port->fillPat = oldPat;
}

void FillOval(const Rect* r, const Pattern* pat) {
    if (!r || !pat) return;

    GrafPtr port;
    GetPort(&port);
    if (!port) return;

    /* Save current pattern */
    Pattern oldPat = port->fillPat;

    /* Set new pattern */
    port->fillPat = *pat;

    /* Perform fill operation */
    /* TODO: Call QuickDraw oval fill implementation */

    /* Restore pattern */
    port->fillPat = oldPat;
}

void FillRoundRect(const Rect* r, short ovalWidth, short ovalHeight, const Pattern* pat) {
    if (!r || !pat) return;

    GrafPtr port;
    GetPort(&port);
    if (!port) return;

    /* Save current pattern */
    Pattern oldPat = port->fillPat;

    /* Set new pattern */
    port->fillPat = *pat;

    /* Perform fill operation */
    /* TODO: Call QuickDraw round rect fill implementation */

    /* Restore pattern */
    port->fillPat = oldPat;
}

void FillArc(const Rect* r, short startAngle, short arcAngle, const Pattern* pat) {
    if (!r || !pat) return;

    GrafPtr port;
    GetPort(&port);
    if (!port) return;

    /* Save current pattern */
    Pattern oldPat = port->fillPat;

    /* Set new pattern */
    port->fillPat = *pat;

    /* Perform fill operation */
    /* TODO: Call QuickDraw arc fill implementation */

    /* Restore pattern */
    port->fillPat = oldPat;
}

void FillRgn(RgnHandle rgn, const Pattern* pat) {
    if (!rgn || !pat) return;

    GrafPtr port;
    GetPort(&port);
    if (!port) return;

    /* Save current pattern */
    Pattern oldPat = port->fillPat;

    /* Set new pattern */
    port->fillPat = *pat;

    /* Perform fill operation */
    /* TODO: Call QuickDraw region fill implementation */

    /* Restore pattern */
    port->fillPat = oldPat;
}

void FillPoly(PolyHandle poly, const Pattern* pat) {
    if (!poly || !pat) return;

    GrafPtr port;
    GetPort(&port);
    if (!port) return;

    /* Save current pattern */
    Pattern oldPat = port->fillPat;

    /* Set new pattern */
    port->fillPat = *pat;

    /* Perform fill operation */
    /* TODO: Call QuickDraw polygon fill implementation */

    /* Restore pattern */
    port->fillPat = oldPat;
}

/* Desktop pattern operations */
void SetDeskPattern(const Pattern* pat) {
    if (pat) {
        /* TODO: Set desktop background pattern */
        if (gDesktopPattern) {
            *gDesktopPattern = *pat;
        }
    }
}

void GetDeskPattern(Pattern* pat) {
    if (!gPatternsInitialized) {
        InitStandardPatterns();
    }
    if (pat && gDesktopPattern) {
        *pat = *gDesktopPattern;
    }
}

/* Use standard gray patterns */
void UseGrayPattern(short level) {
    Pattern pat;

    if (!gPatternsInitialized) {
        InitStandardPatterns();
    }

    switch (level) {
        case 0:
            pat = gWhitePattern;
            break;
        case 1:
            pat = gLtGrayPattern;
            break;
        case 2:
            pat = *gGray25Pattern;
            break;
        case 3:
            pat = gGrayPattern;
            break;
        case 4:
            pat = *gGray50Pattern;
            break;
        case 5:
            pat = gDkGrayPattern;
            break;
        case 6:
            pat = *gGray75Pattern;
            break;
        case 7:
            pat = gBlackPattern;
            break;
        default:
            pat = gGrayPattern;
            break;
    }

    PenPat(&pat);
}

/* Create custom pattern */
PatHandle NewPattern(void) {
    PatHandle pat = (PatHandle)NewHandle(sizeof(Pattern));
    if (pat) {
        /* Initialize to white */
        memset(**pat, 0, sizeof(Pattern));
    }
    return pat;
}

/* Dispose custom pattern */
void DisposePattern(PatHandle pat) {
    if (pat) {
        DisposeHandle((Handle)pat);
    }
}

/* Copy pattern */
void CopyPattern(const Pattern* srcPat, Pattern* dstPat) {
    if (srcPat && dstPat) {
        *dstPat = *srcPat;
    }
}

/* Pattern comparison */
Boolean EqualPattern(const Pattern* pat1, const Pattern* pat2) {
    if (!pat1 || !pat2) return false;
    return memcmp(pat1, pat2, sizeof(Pattern)) == 0;
}

/* Clean up pattern resources */
void CleanupPatterns(void) {
    /* Note: Patterns are cached by ResourceData module */
    /* We don't need to dispose them here */
    gPatternsInitialized = false;
}
/* #include "SystemTypes.h" */
#include "QuickDraw/QuickDrawInternal.h"
#include "QuickDrawConstants.h"
#include <string.h>
// #include "CompatibilityFix.h" // Removed
/*
 * QuickDrawCore.c - Core QuickDraw Graphics Implementation
 *
 * Implementation of fundamental QuickDraw graphics operations including
 * port management, basic drawing primitives, and coordinate systems.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#include "SystemTypes.h"
#include "QuickDrawConstants.h"
#include "System71StdLib.h"
#include "QuickDraw/QDLogging.h"

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QDRegions.h"
#include "MemoryMgr/MemoryManager.h"
#include <math.h>
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDraw/QuickDrawPlatform.h"

/* Picture recording hooks - from Pictures.c */
extern void PictureRecordFrameRect(const Rect *r);
extern void PictureRecordPaintRect(const Rect *r);
extern void PictureRecordEraseRect(const Rect *r);
extern void PictureRecordInvertRect(const Rect *r);
extern void PictureRecordFrameOval(const Rect *r);
extern void PictureRecordPaintOval(const Rect *r);
extern void PictureRecordEraseOval(const Rect *r);
extern void PictureRecordInvertOval(const Rect *r);

/* QuickDraw Globals */
extern QDGlobals qd;  /* Global QD from main.c */
static QDGlobalsPtr g_currentQD = &qd;
static Boolean g_qdInitialized = false;
GrafPtr g_currentPort = NULL;  /* Shared with Coordinates.c */
static QDErr g_lastError = 0;

/* Polygon recording state */
#define MAX_POLY_POINTS 1024
static Boolean g_polyRecording = false;
static Point g_polyPoints[MAX_POLY_POINTS];
static SInt16 g_polyPointCount = 0;
static Rect g_polyBBox;

/* Standard patterns
 *
 * CRITICAL PATTERN QUIRK - READ THIS TO AVOID CONFUSION:
 * ========================================================
 * QuickDraw Patterns are 8x8 1-bit bitmaps, NOT color values!
 * - Bit value 0 = WHITE pixel (background)
 * - Bit value 1 = BLACK pixel (foreground)
 *
 * This is COUNTER-INTUITIVE for modern developers because:
 * - 0x00 bytes = ALL ZEROS = WHITE (not black!)
 * - 0xFF bytes = ALL ONES  = BLACK (not white!)
 *
 * When filling with a solid color:
 * - Use Pattern with all 0x00 bytes for WHITE
 * - Use Pattern with all 0xFF bytes for BLACK
 *
 * Example from folder_window.c that caused confusion:
 *   Pattern whitePat;
 *   for (int i = 0; i < 8; i++) whitePat.pat[i] = 0x00;  // WHITE, not black!
 *   FillRect(&rect, &whitePat);
 *
 * This matches classic Macintosh QuickDraw behavior where patterns
 * are monochrome bitmaps expanded to screen depth during rendering.
 *
 * DEBUGGING HISTORY - TRANSPARENT WINDOW BUG (2025):
 * ===================================================
 * Symptom: Window backgrounds appeared transparent, only icons visible
 * Root cause: Used 0xFF pattern bytes thinking it meant white
 *
 * Debugging sequence that led to the fix:
 * 1. Initially removed EraseRect → windows became transparent
 * 2. Disabled GWorld double-buffering → still transparent
 * 3. Changed to FillRect with 0xFF pattern → windows became BLACK
 * 4. Discovered Pattern definition shows 0x00=white, 0xFF=black
 * 5. Changed pattern to 0x00 → FIXED, windows now white!
 *
 * Key insight: EraseRect uses port's background pattern (bkPat), which
 * may be set to the desktop pattern instead of white. Always use FillRect
 * with explicit white pattern (0x00) for guaranteed white backgrounds.
 */
static const Pattern g_standardPatterns[] = {
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}, /* white */
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}, /* black */
    {{0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22}}, /* gray */
    {{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}}, /* ltGray */
    {{0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD}}  /* dkGray */
};

/* Forward declarations */
static void DrawPrimitive(GrafVerb verb, const Rect *shape, int shapeType,
                         ConstPatternParam pat, SInt16 ovalWidth, SInt16 ovalHeight);
static void ClipToPort(GrafPtr port, Rect *rect);
static Boolean PrepareDrawing(GrafPtr port);
static void ApplyPenToRect(GrafPtr port, Rect *rect);

/* ================================================================
 * INITIALIZATION AND SETUP
 * ================================================================ */

void InitGraf(void *globalPtr) {
    assert(globalPtr != NULL);

    /* Initialize QuickDraw globals */
    memset(&qd, 0, sizeof(QDGlobals));

    /* Set up random seed */
    qd.randSeed = 1;

    /* Initialize standard patterns */
    memcpy(&qd.white, &g_standardPatterns[0], sizeof(Pattern));
    memcpy(&qd.black, &g_standardPatterns[1], sizeof(Pattern));
    memcpy(&qd.gray, &g_standardPatterns[2], sizeof(Pattern));
    memcpy(&qd.ltGray, &g_standardPatterns[3], sizeof(Pattern));
    memcpy(&qd.dkGray, &g_standardPatterns[4], sizeof(Pattern));

    /* Initialize arrow cursor */
    static const UInt16 arrowData[16] = {
        0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7C00, 0x7E00, 0x7F00,
        0x7F80, 0x7C00, 0x6C00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000
    };
    static const UInt16 arrowMask[16] = {
        0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00, 0xFF80,
        0xFFC0, 0xFFC0, 0xFE00, 0xEF00, 0xCF00, 0x8780, 0x0780, 0x0380
    };

    memcpy(qd.arrow.data, arrowData, sizeof(arrowData));
    memcpy(qd.arrow.mask, arrowMask, sizeof(arrowMask));
    qd.arrow.hotSpot.h = 1;
    qd.arrow.hotSpot.v = 1;

    /* Initialize platform layer first to get framebuffer info */
    QDPlatform_Initialize();

    /* Set up screen bitmap with actual framebuffer */
    extern void* framebuffer;
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    extern uint32_t fb_pitch;

    qd.screenBits.baseAddr = (Ptr)framebuffer;
    qd.screenBits.rowBytes = fb_pitch;
    SetRect(&qd.screenBits.bounds, 0, 0, fb_width, fb_height);

    g_qdInitialized = true;
    g_lastError = 0;

    /* Create and initialize a default screen port */
    static GrafPort screenPort;

    QD_LOG_TRACE("InitGraf creating screen port\n");

    InitPort(&screenPort);

    /* Set the screen port to use the framebuffer */
    screenPort.portBits = qd.screenBits;
    screenPort.portRect = qd.screenBits.bounds;

    /* Make it the current port */
    qd.thePort = &screenPort;
    SetPort(&screenPort);

    QD_LOG_TRACE("InitGraf port ready\n");
}

void InitPort(GrafPtr port) {
    assert(port != NULL);
    assert(g_qdInitialized);

    /* Initialize basic port fields */
    port->device = 0;

    /* Set up default port bitmap */
    port->portBits = qd.screenBits;

    /* Set up port rectangle to match screen */
    port->portRect = qd.screenBits.bounds;

    /* Create regions */
    port->visRgn = NewRgn();
    port->clipRgn = NewRgn();

    if (!port->visRgn || !port->clipRgn) {
        g_lastError = memFullErr; /* Using memFullErr instead of insufficientStackErr */
        return;
    }

    /* Set default visible region to port rect */
    RectRgn(port->visRgn, &port->portRect);

    /* Set default clip region to very large rect */
    Rect bigRect;
    SetRect(&bigRect, -32768, -32768, 32767, 32767);
    RectRgn(port->clipRgn, &bigRect);

    /* Set up default patterns */
    port->bkPat = qd.white;
    port->fillPat = qd.black;
    port->pnPat = qd.black;

    /* Initialize pen */
    port->pnLoc.h = 0;
    port->pnLoc.v = 0;
    port->pnSize.h = 1;
    port->pnSize.v = 1;
    port->pnMode = patCopy;
    port->pnVis = 0;  /* Pen visible */

    /* Initialize text settings */
    port->txFont = 0;     /* System font */
    port->txFace = normal;
    port->txMode = srcOr;
    port->txSize = 0;     /* Default size */
    port->spExtra = 0;

    /* Initialize colors */
    port->fgColor = blackColor;
    port->bkColor = whiteColor;

    /* Clear other fields */
    port->colrBit = 0;
    port->patStretch = 0;
    port->picSave = NULL;
    port->rgnSave = NULL;
    port->polySave = NULL;
    port->grafProcs = NULL;
}

void OpenPort(GrafPtr port) {
    InitPort(port);
    SetPort(port);
}

void ClosePort(GrafPtr port) {
    if (port == NULL) return;

    /* Dispose of regions */
    if (port->visRgn) DisposeRgn(port->visRgn);
    if (port->clipRgn) DisposeRgn(port->clipRgn);

    /* Clear saved handles */
    port->picSave = NULL;
    port->rgnSave = NULL;
    port->polySave = NULL;

    /* If this was the current port, clear it */
    if (g_currentPort == port) {
        g_currentPort = NULL;
        qd.thePort = NULL;  /* Direct access instead of g_currentQD->thePort */
    }
}

/* ================================================================
 * PORT MANAGEMENT
 * ================================================================ */

void SetPort(GrafPtr port) {
    assert(g_qdInitialized);

    g_currentPort = port;
    qd.thePort = port;  /* Direct access instead of g_currentQD->thePort */
}

void GetPort(GrafPtr *port) {
    assert(g_qdInitialized);
    assert(port != NULL);
    *port = g_currentPort;
}

void GrafDevice(SInt16 device) {
    if (g_currentPort) {
        g_currentPort->device = device;
    }
}

void SetPortBits(const BitMap *bm) {
    assert(g_currentPort != NULL);
    assert(bm != NULL);
    g_currentPort->portBits = *bm;
}

void PortSize(SInt16 width, SInt16 height) {
    assert(g_currentPort != NULL);
    g_currentPort->portRect.right = g_currentPort->portRect.left + width;
    g_currentPort->portRect.bottom = g_currentPort->portRect.top + height;
}

void MovePortTo(SInt16 leftGlobal, SInt16 topGlobal) {
    assert(g_currentPort != NULL);

    SInt16 width = g_currentPort->portRect.right - g_currentPort->portRect.left;
    SInt16 height = g_currentPort->portRect.bottom - g_currentPort->portRect.top;

    g_currentPort->portRect.left = leftGlobal;
    g_currentPort->portRect.top = topGlobal;
    g_currentPort->portRect.right = leftGlobal + width;
    g_currentPort->portRect.bottom = topGlobal + height;
}

void SetOrigin(SInt16 h, SInt16 v) {
    assert(g_currentPort != NULL);

    /* Adjust port bounds by the origin offset */
    SInt16 dh = g_currentPort->portBits.bounds.left - h;
    SInt16 dv = g_currentPort->portBits.bounds.top - v;

    OffsetRect(&g_currentPort->portBits.bounds, dh, dv);
}

/* ================================================================
 * CLIPPING
 * ================================================================ */

void SetClip(RgnHandle rgn) {
    assert(g_currentPort != NULL);
    assert(rgn != NULL);
    CopyRgn(rgn, g_currentPort->clipRgn);
}

void GetClip(RgnHandle rgn) {
    assert(g_currentPort != NULL);
    assert(rgn != NULL);
    CopyRgn(g_currentPort->clipRgn, rgn);
}

void ClipRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    RectRgn(g_currentPort->clipRgn, r);
}

/* ================================================================
 * DRAWING STATE
 * ================================================================ */

void HidePen(void) {
    if (g_currentPort) {
        g_currentPort->pnVis++;
    }
}

void ShowPen(void) {
    if (g_currentPort) {
        g_currentPort->pnVis--;
    }
}

void PenNormal(void) {
    assert(g_currentPort != NULL);
    g_currentPort->pnSize.h = 1;
    g_currentPort->pnSize.v = 1;
    g_currentPort->pnMode = patCopy;
    g_currentPort->pnPat = qd.black;
}

void PenSize(SInt16 width, SInt16 height) {
    assert(g_currentPort != NULL);
    g_currentPort->pnSize.h = width;
    g_currentPort->pnSize.v = height;
}

void PenMode(SInt16 mode) {
    assert(g_currentPort != NULL);
    g_currentPort->pnMode = mode;
}

void PenPat(ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(pat != NULL);
    g_currentPort->pnPat = *pat;
}

void GetPenPat(Pattern* pat) {
    assert(g_currentPort != NULL);
    assert(pat != NULL);
    *pat = g_currentPort->pnPat;
}

void GetPen(Point *pt) {
    assert(g_currentPort != NULL);
    assert(pt != NULL);
    *pt = g_currentPort->pnLoc;
}

void GetPenState(PenState *pnState) {
    assert(g_currentPort != NULL);
    assert(pnState != NULL);

    pnState->pnLoc = g_currentPort->pnLoc;
    pnState->pnSize = g_currentPort->pnSize;
    pnState->pnMode = g_currentPort->pnMode;
    pnState->pnPat = g_currentPort->pnPat;
}

void SetPenState(const PenState *pnState) {
    assert(g_currentPort != NULL);
    assert(pnState != NULL);

    g_currentPort->pnLoc = pnState->pnLoc;
    g_currentPort->pnSize = pnState->pnSize;
    g_currentPort->pnMode = pnState->pnMode;
    g_currentPort->pnPat = pnState->pnPat;
}

/* ================================================================
 * MOVEMENT
 * ================================================================ */

void MoveTo(SInt16 h, SInt16 v) {
    if (g_currentPort != NULL) {
        g_currentPort->pnLoc.h = h;
        g_currentPort->pnLoc.v = v;

        /* Record point if polygon recording is active */
        if (g_polyRecording && g_polyPointCount < MAX_POLY_POINTS) {
            g_polyPoints[g_polyPointCount].h = h;
            g_polyPoints[g_polyPointCount].v = v;
            g_polyPointCount++;

            /* Update bounding box */
            if (g_polyPointCount == 1) {
                g_polyBBox.left = g_polyBBox.right = h;
                g_polyBBox.top = g_polyBBox.bottom = v;
            } else {
                if (h < g_polyBBox.left) g_polyBBox.left = h;
                if (h > g_polyBBox.right) g_polyBBox.right = h;
                if (v < g_polyBBox.top) g_polyBBox.top = v;
                if (v > g_polyBBox.bottom) g_polyBBox.bottom = v;
            }
        }
    }
}

void Move(SInt16 dh, SInt16 dv) {
    assert(g_currentPort != NULL);
    g_currentPort->pnLoc.h += dh;
    g_currentPort->pnLoc.v += dv;
}

void LineTo(SInt16 h, SInt16 v) {
    assert(g_currentPort != NULL);

    /* Treat pen locations in LOCAL port coordinates */
    Point startLocal = g_currentPort->pnLoc;
    Point endLocal;
    endLocal.h = h;
    endLocal.v = v;

    /* Quick reject if the line's bounding box misses the port bounds */
    Rect lineBounds;
    SInt16 left = (startLocal.h < endLocal.h) ? startLocal.h : endLocal.h;
    SInt16 right = (startLocal.h > endLocal.h) ? startLocal.h : endLocal.h;
    SInt16 top = (startLocal.v < endLocal.v) ? startLocal.v : endLocal.v;
    SInt16 bottom = (startLocal.v > endLocal.v) ? startLocal.v : endLocal.v;
    /* Ensure non-zero width/height so SectRect works */
    SetRect(&lineBounds, left, top, right + 1, bottom + 1);

    Rect clippedBounds;
    if (!SectRect(&lineBounds, &g_currentPort->portRect, &clippedBounds)) {
        g_currentPort->pnLoc = endLocal;
        return;
    }

    if (g_currentPort->clipRgn && *g_currentPort->clipRgn) {
        Rect clipBounds = (*g_currentPort->clipRgn)->rgnBBox;
        /* clipRgn stored in GLOBAL coords; convert to LOCAL for comparison */
        OffsetRect(&clipBounds,
                   -g_currentPort->portBits.bounds.left,
                   -g_currentPort->portBits.bounds.top);
        if (!SectRect(&lineBounds, &clipBounds, &clippedBounds)) {
            g_currentPort->pnLoc = endLocal;
            return;
        }
    }

    /* Record point if polygon recording is active */
    if (g_polyRecording && g_polyPointCount < MAX_POLY_POINTS) {
        g_polyPoints[g_polyPointCount].h = h;
        g_polyPoints[g_polyPointCount].v = v;
        g_polyPointCount++;

        /* Update bounding box */
        if (g_polyPointCount == 1) {
            g_polyBBox.left = g_polyBBox.right = h;
            g_polyBBox.top = g_polyBBox.bottom = v;
        } else {
            if (h < g_polyBBox.left) g_polyBBox.left = h;
            if (h > g_polyBBox.right) g_polyBBox.right = h;
            if (v < g_polyBBox.top) g_polyBBox.top = v;
            if (v > g_polyBBox.bottom) g_polyBBox.bottom = v;
        }
    }

    /* Draw the line if pen should be visible and not recording polygon */
    if (!g_polyRecording && g_currentPort->pnVis <= 0 &&
        g_currentPort->pnSize.h > 0 && g_currentPort->pnSize.v > 0) {
        Point startGlobal = startLocal;
        Point endGlobal = endLocal;
        LocalToGlobal(&startGlobal);
        LocalToGlobal(&endGlobal);

        QDPlatform_DrawLine(g_currentPort, startGlobal, endGlobal,
                            &g_currentPort->pnPat, g_currentPort->pnMode);
    }

    /* Update pen location in LOCAL coordinates */
    g_currentPort->pnLoc = endLocal;
}

void Line(SInt16 dh, SInt16 dv) {
    assert(g_currentPort != NULL);
    LineTo(g_currentPort->pnLoc.h + dh, g_currentPort->pnLoc.v + dv);
}

/* ================================================================
 * PATTERN AND COLOR
 * ================================================================ */

void BackPat(ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(pat != NULL);
    g_currentPort->bkPat = *pat;
}

/* Additional function for Pattern Manager integration */
void UpdateBackgroundPattern(const Pattern* pat) {
    if (!pat) return;
    if (g_currentPort) {
        g_currentPort->bkPat = *pat;
    }
}

void BackColor(SInt32 color) {
    assert(g_currentPort != NULL);
    g_currentPort->bkColor = color;
}

void ForeColor(SInt32 color) {
    assert(g_currentPort != NULL);
    g_currentPort->fgColor = color;
}

void ColorBit(SInt16 whichBit) {
    assert(g_currentPort != NULL);
    g_currentPort->colrBit = whichBit;
}

/* ================================================================
 * RECTANGLE OPERATIONS
 * ================================================================ */

void FrameRect(const Rect *r) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    PictureRecordFrameRect(r);
    DrawPrimitive(frame, r, 0, &g_currentPort->pnPat, 0, 0);
}

void PaintRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    PictureRecordPaintRect(r);
    DrawPrimitive(paint, r, 0, &g_currentPort->pnPat, 0, 0);
}

void EraseRect(const Rect *r) {
    QD_LOG_TRACE("EraseRect entry rect=(%d,%d,%d,%d)\n",
                r ? r->left : -1, r ? r->top : -1, r ? r->right : -1, r ? r->bottom : -1);

    assert(g_currentPort != NULL);
    assert(r != NULL);

    if (EmptyRect(r)) {
        QD_LOG_TRACE("EraseRect empty rect early exit\n");
        return;
    }

    QD_LOG_TRACE("EraseRect dispatch DrawPrimitive\n");
    PictureRecordEraseRect(r);
    DrawPrimitive(erase, r, 0, &g_currentPort->bkPat, 0, 0);
    QD_LOG_TRACE("EraseRect DrawPrimitive returned\n");
}

void InvertRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    PictureRecordInvertRect(r);
    DrawPrimitive(invert, r, 0, NULL, 0, 0);
}

void FillRect(const Rect *r, ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    assert(pat != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(fill, r, 0, pat, 0, 0);
}

/* ================================================================
 * OVAL OPERATIONS
 * ================================================================ */

void FrameOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    PictureRecordFrameOval(r);
    DrawPrimitive(frame, r, 1, &g_currentPort->pnPat, 0, 0);
}

void PaintOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    PictureRecordPaintOval(r);
    DrawPrimitive(paint, r, 1, &g_currentPort->pnPat, 0, 0);
}

void EraseOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    PictureRecordEraseOval(r);
    DrawPrimitive(erase, r, 1, &g_currentPort->bkPat, 0, 0);
}

void InvertOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    PictureRecordInvertOval(r);
    DrawPrimitive(invert, r, 1, NULL, 0, 0);
}

void FillOval(const Rect *r, ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    assert(pat != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(fill, r, 1, pat, 0, 0);
}

/* ================================================================
 * ROUNDED RECTANGLE OPERATIONS
 * ================================================================ */

void FrameRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(frame, r, 2, &g_currentPort->pnPat, ovalWidth, ovalHeight);
}

void PaintRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(paint, r, 2, &g_currentPort->pnPat, ovalWidth, ovalHeight);
}

void EraseRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(erase, r, 2, &g_currentPort->bkPat, ovalWidth, ovalHeight);
}

void InvertRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(invert, r, 2, NULL, ovalWidth, ovalHeight);
}

void FillRoundRect(const Rect *r, SInt16 ovalWidth, SInt16 ovalHeight,
                   ConstPatternParam pat) {
    if (!g_currentPort || !r || !pat || EmptyRect(r)) return;
    DrawPrimitive(fill, r, 2, pat, ovalWidth, ovalHeight);
}

/* ================================================================
 * ARC OPERATIONS
 * ================================================================ */

void FrameArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(frame, r, 3, &g_currentPort->pnPat, startAngle, arcAngle);
}

void PaintArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(paint, r, 3, &g_currentPort->pnPat, startAngle, arcAngle);
}

void EraseArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(erase, r, 3, &g_currentPort->bkPat, startAngle, arcAngle);
}

void InvertArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle) {
    if (!g_currentPort || !r || EmptyRect(r)) return;
    DrawPrimitive(invert, r, 3, NULL, startAngle, arcAngle);
}

void FillArc(const Rect *r, SInt16 startAngle, SInt16 arcAngle,
             ConstPatternParam pat) {
    if (!g_currentPort || !r || !pat || EmptyRect(r)) return;
    DrawPrimitive(fill, r, 3, pat, startAngle, arcAngle);
}

/* ================================================================
 * POLYGON OPERATIONS
 * ================================================================ */

PolyHandle OpenPoly(void) {
    if (!g_currentPort) return NULL;

    /* Initialize polygon recording */
    g_polyRecording = true;
    g_polyPointCount = 0;
    SetRect(&g_polyBBox, 0, 0, 0, 0);

    /* Store current pen location as first point if it's valid */
    Point penLoc = g_currentPort->pnLoc;
    if (g_polyPointCount < MAX_POLY_POINTS) {
        g_polyPoints[g_polyPointCount++] = penLoc;
        g_polyBBox.left = g_polyBBox.right = penLoc.h;
        g_polyBBox.top = g_polyBBox.bottom = penLoc.v;
    }

    return NULL;  /* Will return actual handle in ClosePoly */
}

PolyHandle ClosePoly(void) {
    if (!g_polyRecording) return NULL;

    g_polyRecording = false;

    if (g_polyPointCount == 0) return NULL;

    /* Calculate size needed for Polygon structure */
    SInt16 polySize = sizeof(SInt16) + sizeof(Rect) +
                      (g_polyPointCount * sizeof(Point));

    /* Allocate handle for polygon */
    PolyHandle poly = (PolyHandle)NewHandle(polySize);
    if (!poly) {
        g_lastError = memFullErr;
        return NULL;
    }

    /* Fill in polygon data */
    Polygon *polyPtr = *poly;
    polyPtr->polySize = polySize;
    polyPtr->polyBBox = g_polyBBox;

    /* Copy points */
    for (SInt16 i = 0; i < g_polyPointCount; i++) {
        polyPtr->polyPoints[i] = g_polyPoints[i];
    }

    return poly;
}

void KillPoly(PolyHandle poly) {
    if (poly) {
        DisposeHandle((Handle)poly);
    }
}

void OffsetPoly(PolyHandle poly, SInt16 dh, SInt16 dv) {
    if (!poly || !*poly) return;

    Polygon *polyPtr = *poly;

    /* Offset bounding box */
    OffsetRect(&polyPtr->polyBBox, dh, dv);

    /* Calculate number of points from polySize */
    SInt16 numPoints = (polyPtr->polySize - sizeof(SInt16) - sizeof(Rect)) / sizeof(Point);

    /* Offset all points */
    for (SInt16 i = 0; i < numPoints; i++) {
        polyPtr->polyPoints[i].h += dh;
        polyPtr->polyPoints[i].v += dv;
    }
}

void FramePoly(PolyHandle poly) {
    if (!g_currentPort || !poly || !*poly) return;

    Polygon *polyPtr = *poly;
    SInt16 numPoints = (polyPtr->polySize - sizeof(SInt16) - sizeof(Rect)) / sizeof(Point);

    if (numPoints < 2) return;

    /* Draw lines between consecutive points */
    for (SInt16 i = 0; i < numPoints - 1; i++) {
        MoveTo(polyPtr->polyPoints[i].h, polyPtr->polyPoints[i].v);
        LineTo(polyPtr->polyPoints[i + 1].h, polyPtr->polyPoints[i + 1].v);
    }
}

void PaintPoly(PolyHandle poly) {
    if (!g_currentPort || !poly || !*poly) return;

    /* Use platform layer to fill polygon */
    QDPlatform_FillPoly(g_currentPort, poly, &g_currentPort->pnPat,
                        g_currentPort->pnMode, paint);
}

void ErasePoly(PolyHandle poly) {
    if (!g_currentPort || !poly || !*poly) return;

    /* Use platform layer to fill polygon with background */
    QDPlatform_FillPoly(g_currentPort, poly, &g_currentPort->bkPat,
                        patCopy, erase);
}

void InvertPoly(PolyHandle poly) {
    if (!g_currentPort || !poly || !*poly) return;

    /* Use platform layer to invert polygon */
    QDPlatform_FillPoly(g_currentPort, poly, NULL, patCopy, invert);
}

void FillPoly(PolyHandle poly, ConstPatternParam pat) {
    if (!g_currentPort || !poly || !*poly || !pat) return;

    /* Use platform layer to fill polygon with pattern */
    QDPlatform_FillPoly(g_currentPort, poly, pat, patCopy, fill);
}

/* ================================================================
 * UTILITY FUNCTIONS
 * ================================================================ */

SInt16 Random(void) {
    /* Linear congruential generator */
    qd.randSeed = (qd.randSeed * 16807) % 2147483647;
    return (SInt16)((qd.randSeed & 0xFFFF) - 32768);
}

QDGlobalsPtr GetQDGlobals(void) {
    return g_currentQD;
}

void SetQDGlobals(QDGlobalsPtr globals) {
    assert(globals != NULL);
    g_currentQD = globals;
}

QDErr QDError(void) {
    return g_lastError;
}

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

static void DrawPrimitive(GrafVerb verb, const Rect *shape, int shapeType,
                         ConstPatternParam pat, SInt16 ovalWidth, SInt16 ovalHeight) {
    QD_LOG_TRACE("DrawPrimitive entry verb=%d\n", verb);

    if (!PrepareDrawing(g_currentPort)) {
        QD_LOG_WARN("DrawPrimitive PrepareDrawing failed\n");
        return;
    }

    Rect drawRect = *shape;
    QD_LOG_TRACE("DrawPrimitive original rect=(%d,%d,%d,%d)\n",
                drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);

    /* Apply pen size for frame operations */
    if (verb == frame) {
        ApplyPenToRect(g_currentPort, &drawRect);
    }

    /* Clip to port and visible region */
    ClipToPort(g_currentPort, &drawRect);
    QD_LOG_TRACE("DrawPrimitive clipped rect=(%d,%d,%d,%d)\n",
                drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);

    /* CRITICAL: Convert LOCAL coordinates to GLOBAL
     * For basic GrafPort: use portBits.bounds
     * For CGrafPort/GWorld: use portPixMap bounds (which is already local 0,0)
     * Platform layer needs correct coords based on port type */
    Rect globalRect = drawRect;

    /* Check if this is a color port (CGrafPtr/GWorld) */
    extern CGrafPtr g_currentCPort;  /* from ColorQuickDraw.c */
    Boolean isColorPort = (g_currentCPort != NULL && (GrafPtr)g_currentCPort == g_currentPort);

    if (isColorPort) {
        /* Color port (GWorld) - portPixMap bounds are already local (0,0,w,h)
         * Don't add offset - coords stay local for offscreen drawing */
        CGrafPtr cport = (CGrafPtr)g_currentPort;
        if (cport->portPixMap && *cport->portPixMap) {
            PixMapPtr pm = *cport->portPixMap;
            /* For GWorld, bounds are local - no offset needed */
            QD_LOG_TRACE("DrawPrimitive COLOR PORT: rect stays local (%d,%d,%d,%d)\n",
                        globalRect.left, globalRect.top, globalRect.right, globalRect.bottom);
        }
    } else {
        /* Basic GrafPort - convert local to global screen coords */
        globalRect.left += g_currentPort->portBits.bounds.left;
        globalRect.top += g_currentPort->portBits.bounds.top;
        globalRect.right += g_currentPort->portBits.bounds.left;
        globalRect.bottom += g_currentPort->portBits.bounds.top;

        QD_LOG_TRACE("DrawPrimitive BASIC PORT: global rect=(%d,%d,%d,%d) offset by portBits(%d,%d)\n",
                    globalRect.left, globalRect.top, globalRect.right, globalRect.bottom,
                    g_currentPort->portBits.bounds.left, g_currentPort->portBits.bounds.top);
    }

    /* Call platform layer to do actual drawing */
    QD_LOG_TRACE("DrawPrimitive call QDPlatform_DrawShape\n");
    /* Clamp oval dimensions after clipping */
    if (shapeType == 2) {
        SInt16 width = globalRect.right - globalRect.left;
        SInt16 height = globalRect.bottom - globalRect.top;
        if (ovalWidth > width) ovalWidth = width;
        if (ovalHeight > height) ovalHeight = height;
    }

    QDPlatform_DrawShape(g_currentPort, verb, &globalRect, shapeType, pat,
                         ovalWidth, ovalHeight);
    QD_LOG_TRACE("DrawPrimitive QDPlatform_DrawShape returned\n");
}

static void ClipToPort(GrafPtr port, Rect *rect) {
    /* Intersect with port rectangle */
    Rect clippedRect;
    if (!SectRect(rect, &port->portRect, &clippedRect)) {
        SetRect(rect, 0, 0, 0, 0); /* Empty result */
        return;
    }
    *rect = clippedRect;

    /* CRITICAL: Also clip to clipRgn if set (e.g., content region to prevent overdrawing chrome) */
    if (port->clipRgn && *port->clipRgn) {
        Rect clipBounds = (*port->clipRgn)->rgnBBox;
        /* clipRgn is in GLOBAL coords, convert to LOCAL */
        OffsetRect(&clipBounds, -port->portBits.bounds.left, -port->portBits.bounds.top);
        if (!SectRect(rect, &clipBounds, &clippedRect)) {
            SetRect(rect, 0, 0, 0, 0); /* Empty result */
            return;
        }
        *rect = clippedRect;
    }
}

static Boolean PrepareDrawing(GrafPtr port) {
    if (!port || !g_qdInitialized) {
        g_lastError = memFullErr; /* Using memFullErr instead of insufficientStackErr */
        return false;
    }

    g_lastError = 0;
    return true;
}

static void ApplyPenToRect(GrafPtr port, Rect *rect) {
    /* Adjust rectangle for pen size */
    if (port->pnSize.h > 1) {
        rect->right += port->pnSize.h - 1;
    }
    if (port->pnSize.v > 1) {
        rect->bottom += port->pnSize.v - 1;
    }
}

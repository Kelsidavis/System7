/*
 * QuickDrawCore.c - Core QuickDraw Graphics Implementation
 *
 * Implementation of fundamental QuickDraw graphics operations including
 * port management, basic drawing primitives, and coordinate systems.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#include "../include/QuickDraw/QuickDraw.h"
#include "../include/QuickDraw/QDRegions.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDrawPlatform.h"

/* QuickDraw Globals */
static QDGlobals g_qdGlobals;
static QDGlobalsPtr g_currentQD = &g_qdGlobals;
static bool g_qdInitialized = false;
static GrafPtr g_currentPort = NULL;
static QDErr g_lastError = 0;

/* Standard patterns */
static const Pattern g_standardPatterns[] = {
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}, /* white */
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}, /* black */
    {{0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22}}, /* gray */
    {{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}}, /* ltGray */
    {{0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD}}  /* dkGray */
};

/* Forward declarations */
static void DrawPrimitive(GrafVerb verb, const void *shape, int shapeType,
                         ConstPatternParam pat);
static void ClipToPort(GrafPtr port, Rect *rect);
static bool PrepareDrawing(GrafPtr port);
static void ApplyPenToRect(GrafPtr port, Rect *rect);

/* ================================================================
 * INITIALIZATION AND SETUP
 * ================================================================ */

void InitGraf(void *globalPtr) {
    assert(globalPtr != NULL);

    /* Initialize QuickDraw globals */
    memset(&g_qdGlobals, 0, sizeof(QDGlobals));

    /* Set up random seed */
    g_qdGlobals.randSeed = 1;

    /* Initialize standard patterns */
    memcpy(&g_qdGlobals.white, &g_standardPatterns[0], sizeof(Pattern));
    memcpy(&g_qdGlobals.black, &g_standardPatterns[1], sizeof(Pattern));
    memcpy(&g_qdGlobals.gray, &g_standardPatterns[2], sizeof(Pattern));
    memcpy(&g_qdGlobals.ltGray, &g_standardPatterns[3], sizeof(Pattern));
    memcpy(&g_qdGlobals.dkGray, &g_standardPatterns[4], sizeof(Pattern));

    /* Initialize arrow cursor */
    static const uint16_t arrowData[16] = {
        0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7C00, 0x7E00, 0x7F00,
        0x7F80, 0x7C00, 0x6C00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000
    };
    static const uint16_t arrowMask[16] = {
        0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00, 0xFF80,
        0xFFC0, 0xFFC0, 0xFE00, 0xEF00, 0xCF00, 0x8780, 0x0780, 0x0380
    };

    memcpy(g_qdGlobals.arrow.data, arrowData, sizeof(arrowData));
    memcpy(g_qdGlobals.arrow.mask, arrowMask, sizeof(arrowMask));
    g_qdGlobals.arrow.hotSpot.h = 1;
    g_qdGlobals.arrow.hotSpot.v = 1;

    /* Set up screen bitmap (will be configured by platform layer) */
    g_qdGlobals.screenBits.baseAddr = NULL;
    g_qdGlobals.screenBits.rowBytes = 0;
    SetRect(&g_qdGlobals.screenBits.bounds, 0, 0, 640, 480); /* Default size */

    /* Initialize platform layer */
    QDPlatform_Initialize();

    g_qdInitialized = true;
    g_lastError = 0;
}

void InitPort(GrafPtr port) {
    assert(port != NULL);
    assert(g_qdInitialized);

    /* Initialize basic port fields */
    port->device = 0;

    /* Set up default port bitmap */
    port->portBits = g_qdGlobals.screenBits;

    /* Set up port rectangle to match screen */
    port->portRect = g_qdGlobals.screenBits.bounds;

    /* Create regions */
    port->visRgn = NewRgn();
    port->clipRgn = NewRgn();

    if (!port->visRgn || !port->clipRgn) {
        g_lastError = insufficientStackErr;
        return;
    }

    /* Set default visible region to port rect */
    RectRgn(port->visRgn, &port->portRect);

    /* Set default clip region to very large rect */
    Rect bigRect;
    SetRect(&bigRect, -32768, -32768, 32767, 32767);
    RectRgn(port->clipRgn, &bigRect);

    /* Set up default patterns */
    port->bkPat = g_qdGlobals.white;
    port->fillPat = g_qdGlobals.black;
    port->pnPat = g_qdGlobals.black;

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
        g_currentQD->thePort = NULL;
    }
}

/* ================================================================
 * PORT MANAGEMENT
 * ================================================================ */

void SetPort(GrafPtr port) {
    assert(g_qdInitialized);
    g_currentPort = port;
    g_currentQD->thePort = port;
}

void GetPort(GrafPtr *port) {
    assert(g_qdInitialized);
    assert(port != NULL);
    *port = g_currentPort;
}

void GrafDevice(int16_t device) {
    if (g_currentPort) {
        g_currentPort->device = device;
    }
}

void SetPortBits(const BitMap *bm) {
    assert(g_currentPort != NULL);
    assert(bm != NULL);
    g_currentPort->portBits = *bm;
}

void PortSize(int16_t width, int16_t height) {
    assert(g_currentPort != NULL);
    g_currentPort->portRect.right = g_currentPort->portRect.left + width;
    g_currentPort->portRect.bottom = g_currentPort->portRect.top + height;
}

void MovePortTo(int16_t leftGlobal, int16_t topGlobal) {
    assert(g_currentPort != NULL);

    int16_t width = g_currentPort->portRect.right - g_currentPort->portRect.left;
    int16_t height = g_currentPort->portRect.bottom - g_currentPort->portRect.top;

    g_currentPort->portRect.left = leftGlobal;
    g_currentPort->portRect.top = topGlobal;
    g_currentPort->portRect.right = leftGlobal + width;
    g_currentPort->portRect.bottom = topGlobal + height;
}

void SetOrigin(int16_t h, int16_t v) {
    assert(g_currentPort != NULL);

    /* Adjust port bounds by the origin offset */
    int16_t dh = g_currentPort->portBits.bounds.left - h;
    int16_t dv = g_currentPort->portBits.bounds.top - v;

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
    g_currentPort->pnPat = g_qdGlobals.black;
}

void PenSize(int16_t width, int16_t height) {
    assert(g_currentPort != NULL);
    g_currentPort->pnSize.h = width;
    g_currentPort->pnSize.v = height;
}

void PenMode(int16_t mode) {
    assert(g_currentPort != NULL);
    g_currentPort->pnMode = mode;
}

void PenPat(ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(pat != NULL);
    g_currentPort->pnPat = *pat;
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

void MoveTo(int16_t h, int16_t v) {
    assert(g_currentPort != NULL);
    g_currentPort->pnLoc.h = h;
    g_currentPort->pnLoc.v = v;
}

void Move(int16_t dh, int16_t dv) {
    assert(g_currentPort != NULL);
    g_currentPort->pnLoc.h += dh;
    g_currentPort->pnLoc.v += dv;
}

void LineTo(int16_t h, int16_t v) {
    assert(g_currentPort != NULL);

    Point startPt = g_currentPort->pnLoc;
    Point endPt = {v, h};

    /* Draw the line if pen is visible */
    if (g_currentPort->pnVis <= 0 &&
        (g_currentPort->pnSize.h > 0 && g_currentPort->pnSize.v > 0)) {
        QDPlatform_DrawLine(g_currentPort, startPt, endPt,
                           &g_currentPort->pnPat, g_currentPort->pnMode);
    }

    /* Update pen location */
    g_currentPort->pnLoc = endPt;
}

void Line(int16_t dh, int16_t dv) {
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

void BackColor(int32_t color) {
    assert(g_currentPort != NULL);
    g_currentPort->bkColor = color;
}

void ForeColor(int32_t color) {
    assert(g_currentPort != NULL);
    g_currentPort->fgColor = color;
}

void ColorBit(int16_t whichBit) {
    assert(g_currentPort != NULL);
    g_currentPort->colrBit = whichBit;
}

/* ================================================================
 * RECTANGLE OPERATIONS
 * ================================================================ */

void FrameRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(frame, r, 0, &g_currentPort->pnPat);
}

void PaintRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(paint, r, 0, &g_currentPort->pnPat);
}

void EraseRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(erase, r, 0, &g_currentPort->bkPat);
}

void InvertRect(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(invert, r, 0, NULL);
}

void FillRect(const Rect *r, ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    assert(pat != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(fill, r, 0, pat);
}

/* ================================================================
 * OVAL OPERATIONS
 * ================================================================ */

void FrameOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(frame, r, 1, &g_currentPort->pnPat);
}

void PaintOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(paint, r, 1, &g_currentPort->pnPat);
}

void EraseOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(erase, r, 1, &g_currentPort->bkPat);
}

void InvertOval(const Rect *r) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(invert, r, 1, NULL);
}

void FillOval(const Rect *r, ConstPatternParam pat) {
    assert(g_currentPort != NULL);
    assert(r != NULL);
    assert(pat != NULL);
    if (EmptyRect(r)) return;

    DrawPrimitive(fill, r, 1, pat);
}

/* ================================================================
 * UTILITY FUNCTIONS
 * ================================================================ */

int16_t Random(void) {
    /* Linear congruential generator */
    g_qdGlobals.randSeed = (g_qdGlobals.randSeed * 16807) % 2147483647;
    return (int16_t)((g_qdGlobals.randSeed & 0xFFFF) - 32768);
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

static void DrawPrimitive(GrafVerb verb, const void *shape, int shapeType,
                         ConstPatternParam pat) {
    if (!PrepareDrawing(g_currentPort)) return;

    const Rect *rect = (const Rect *)shape;
    Rect drawRect = *rect;

    /* Apply pen size for frame operations */
    if (verb == frame) {
        ApplyPenToRect(g_currentPort, &drawRect);
    }

    /* Clip to port and visible region */
    ClipToPort(g_currentPort, &drawRect);

    /* Call platform layer to do actual drawing */
    QDPlatform_DrawShape(g_currentPort, verb, &drawRect, shapeType, pat);
}

static void ClipToPort(GrafPtr port, Rect *rect) {
    /* Intersect with port rectangle */
    Rect clippedRect;
    if (!SectRect(rect, &port->portRect, &clippedRect)) {
        SetRect(rect, 0, 0, 0, 0); /* Empty result */
        return;
    }
    *rect = clippedRect;
}

static bool PrepareDrawing(GrafPtr port) {
    if (!port || !g_qdInitialized) {
        g_lastError = insufficientStackErr;
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
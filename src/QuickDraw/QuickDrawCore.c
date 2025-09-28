/* #include "SystemTypes.h" */
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
 * Based on Apple Macintosh System Software 7.1 QuickDraw
 */

#include "SystemTypes.h"
#include "QuickDrawConstants.h"
#include "System71StdLib.h"

#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/QDRegions.h"
#include <math.h>
#include <assert.h>

/* Platform abstraction layer */
#include "QuickDrawPlatform.h"


/* QuickDraw Globals */
extern QDGlobals qd;  /* Global QD from main.c */
static QDGlobalsPtr g_currentQD = &qd;
static Boolean g_qdInitialized = false;
GrafPtr g_currentPort = NULL;  /* Shared with Coordinates.c */
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

    extern void serial_puts(const char* str);
    serial_puts("InitGraf: Creating screen port\n");

    InitPort(&screenPort);

    /* Set the screen port to use the framebuffer */
    screenPort.portBits = qd.screenBits;
    screenPort.portRect = qd.screenBits.bounds;

    /* Make it the current port */
    qd.thePort = &screenPort;
    SetPort(&screenPort);

    serial_puts("InitGraf: qd.thePort and g_currentPort set\n");
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
    }
}

void Move(SInt16 dh, SInt16 dv) {
    assert(g_currentPort != NULL);
    g_currentPort->pnLoc.h += dh;
    g_currentPort->pnLoc.v += dv;
}

void LineTo(SInt16 h, SInt16 v) {
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
    extern void serial_printf(const char* fmt, ...);
    serial_printf("EraseRect: ENTRY, rect=(%d,%d,%d,%d)\n",
                  r ? r->left : -1, r ? r->top : -1, r ? r->right : -1, r ? r->bottom : -1);

    assert(g_currentPort != NULL);
    assert(r != NULL);

    if (EmptyRect(r)) {
        serial_printf("EraseRect: EmptyRect returned TRUE, returning early\n");
        return;
    }

    serial_printf("EraseRect: About to call DrawPrimitive\n");
    DrawPrimitive(erase, r, 0, &g_currentPort->bkPat);
    serial_printf("EraseRect: DrawPrimitive returned\n");
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

static void DrawPrimitive(GrafVerb verb, const void *shape, int shapeType,
                         ConstPatternParam pat) {
    extern void serial_printf(const char* fmt, ...);
    serial_printf("DrawPrimitive: verb=%d ENTRY\n", verb);

    if (!PrepareDrawing(g_currentPort)) {
        serial_printf("DrawPrimitive: PrepareDrawing FAILED\n");
        return;
    }

    const Rect *rect = (const Rect *)shape;
    Rect drawRect = *rect;
    serial_printf("DrawPrimitive: original rect=(%d,%d,%d,%d)\n",
                  drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);

    /* Apply pen size for frame operations */
    if (verb == frame) {
        ApplyPenToRect(g_currentPort, &drawRect);
    }

    /* Clip to port and visible region */
    ClipToPort(g_currentPort, &drawRect);
    serial_printf("DrawPrimitive: clipped rect=(%d,%d,%d,%d)\n",
                  drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);

    /* Call platform layer to do actual drawing */
    serial_printf("DrawPrimitive: calling QDPlatform_DrawShape\n");
    QDPlatform_DrawShape(g_currentPort, verb, &drawRect, shapeType, pat);
    serial_printf("DrawPrimitive: QDPlatform_DrawShape returned\n");
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

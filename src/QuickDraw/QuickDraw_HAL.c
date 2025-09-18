/*
 * QuickDraw Hardware Abstraction Layer
 * Bridges classic Mac OS QuickDraw to modern graphics systems
 * Integrates with Memory, Window, and Menu Managers
 * Implements critical region operations for proper clipping
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#endif

#include "QuickDraw/quickdraw.h"
#include "QuickDraw/quickdraw_types.h"
#include "MemoryMgr/memory_manager.h"
#include "WindowManager/window_manager.h"

/* QuickDraw globals */
GrafPtr thePort = NULL;  /* Current graphics port */

static struct {
    BitMap      screenBits;         /* Screen bitmap */
    Pattern     white;              /* White pattern */
    Pattern     black;              /* Black pattern */
    Pattern     gray;               /* 50% gray pattern */
    Pattern     ltGray;             /* Light gray pattern */
    Pattern     dkGray;             /* Dark gray pattern */
    Cursor      arrow;              /* Arrow cursor */
    RgnHandle   theRgn;            /* Scratch region */
    pthread_mutex_t qdLock;        /* Thread safety */
    bool        initialized;

    /* Platform-specific */
#ifdef __linux__
    Display*    display;
    cairo_t*    cairo_context;
    cairo_surface_t* cairo_surface;
#endif
#ifdef __APPLE__
    CGContextRef cg_context;
#endif
} gQD = {0};

/* Forward declarations */
static void QD_HAL_InitPatterns(void);
static void QD_HAL_InitScreenBits(void);
static void QD_HAL_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
static void QD_HAL_FillRectangle(const Rect* r, const Pattern* pat);

/* Initialize QuickDraw HAL */
void QuickDraw_HAL_InitGraf(GrafPtr port)
{
    if (gQD.initialized) {
        return;
    }

    /* Initialize mutex */
    pthread_mutex_init(&gQD.qdLock, NULL);

    /* Initialize patterns */
    QD_HAL_InitPatterns();

    /* Initialize screen bitmap */
    QD_HAL_InitScreenBits();

    /* Create scratch region using Memory Manager */
    gQD.theRgn = NewRgn();

    /* Initialize arrow cursor */
    memset(&gQD.arrow, 0, sizeof(Cursor));
    gQD.arrow.hotSpot.h = 0;
    gQD.arrow.hotSpot.v = 0;

#ifdef __linux__
    /* Initialize X11/Cairo */
    gQD.display = XOpenDisplay(NULL);
    if (gQD.display) {
        int screen = DefaultScreen(gQD.display);
        Window rootWindow = RootWindow(gQD.display, screen);

        /* Create Cairo surface for X11 */
        Visual* visual = DefaultVisual(gQD.display, screen);
        gQD.cairo_surface = cairo_xlib_surface_create(gQD.display, rootWindow,
                                                      visual, 1024, 768);
        gQD.cairo_context = cairo_create(gQD.cairo_surface);
    }
#endif

#ifdef __APPLE__
    /* Initialize Core Graphics context */
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    gQD.cg_context = CGBitmapContextCreate(NULL, 1024, 768, 8, 0,
                                           colorSpace, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(colorSpace);
#endif

    /* Set the initial port */
    if (port) {
        thePort = port;
    }

    gQD.initialized = true;
}

/* Open a graphics port */
void QuickDraw_HAL_OpenPort(GrafPtr port)
{
    if (!port) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Initialize port structure */
    port->portBits = gQD.screenBits;
    SetRect(&port->portRect, 0, 0, 1024, 768);

    /* Create port regions using Memory Manager */
    port->visRgn = NewRgn();
    port->clipRgn = NewRgn();

    /* Set default regions to port rectangle */
    RectRgn(port->visRgn, &port->portRect);
    RectRgn(port->clipRgn, &port->portRect);

    /* Initialize pen */
    port->pnLoc.h = 0;
    port->pnLoc.v = 0;
    port->pnSize.h = 1;
    port->pnSize.v = 1;
    port->pnMode = patCopy;
    port->pnPat = gQD.black;

    /* Initialize text */
    port->txFont = 0;      /* System font */
    port->txFace = 0;      /* Plain */
    port->txMode = srcOr;
    port->txSize = 12;

    /* Initialize colors */
    port->fgColor = blackColor;
    port->bkColor = whiteColor;

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Close a graphics port */
void QuickDraw_HAL_ClosePort(GrafPtr port)
{
    if (!port) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Dispose regions using Memory Manager */
    if (port->visRgn) {
        DisposeRgn(port->visRgn);
    }
    if (port->clipRgn) {
        DisposeRgn(port->clipRgn);
    }

    /* Clear port if it's current */
    if (thePort == port) {
        thePort = NULL;
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Set current port */
void QuickDraw_HAL_SetPort(GrafPtr port)
{
    pthread_mutex_lock(&gQD.qdLock);
    thePort = port;
    pthread_mutex_unlock(&gQD.qdLock);
}

/* Get current port */
void QuickDraw_HAL_GetPort(GrafPtr* port)
{
    if (port) {
        *port = thePort;
    }
}

/* ===== CRITICAL REGION OPERATIONS FOR CLIPPING ===== */

/* Create new region */
RgnHandle QuickDraw_HAL_NewRgn(void)
{
    /* Allocate region handle using Memory Manager */
    RgnHandle rgn = (RgnHandle)NewHandle(sizeof(Region));
    if (rgn) {
        (*rgn)->rgnSize = sizeof(Region);
        (*rgn)->rgnBBox.left = 0;
        (*rgn)->rgnBBox.top = 0;
        (*rgn)->rgnBBox.right = 0;
        (*rgn)->rgnBBox.bottom = 0;
    }
    return rgn;
}

/* Dispose region */
void QuickDraw_HAL_DisposeRgn(RgnHandle rgn)
{
    if (rgn) {
        DisposeHandle((Handle)rgn);
    }
}

/* Set region to empty */
void QuickDraw_HAL_SetEmptyRgn(RgnHandle rgn)
{
    if (!rgn) return;

    pthread_mutex_lock(&gQD.qdLock);
    (*rgn)->rgnSize = sizeof(Region);
    (*rgn)->rgnBBox.left = 0;
    (*rgn)->rgnBBox.top = 0;
    (*rgn)->rgnBBox.right = 0;
    (*rgn)->rgnBBox.bottom = 0;
    pthread_mutex_unlock(&gQD.qdLock);
}

/* Set region to rectangle */
void QuickDraw_HAL_RectRgn(RgnHandle rgn, const Rect* r)
{
    if (!rgn || !r) return;

    pthread_mutex_lock(&gQD.qdLock);
    (*rgn)->rgnSize = sizeof(Region);
    (*rgn)->rgnBBox = *r;
    pthread_mutex_unlock(&gQD.qdLock);
}

/* Copy region */
void QuickDraw_HAL_CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn)
{
    if (!srcRgn || !dstRgn) return;

    pthread_mutex_lock(&gQD.qdLock);

    Size rgnSize = (*srcRgn)->rgnSize;
    SetHandleSize((Handle)dstRgn, rgnSize);

    if (*dstRgn) {
        BlockMoveData(*srcRgn, *dstRgn, rgnSize);
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* CRITICAL: Region intersection for menu/window clipping */
void QuickDraw_HAL_SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn)
{
    if (!srcRgnA || !srcRgnB || !dstRgn) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Calculate intersection of bounding boxes */
    Rect intersect;
    intersect.left = MAX((*srcRgnA)->rgnBBox.left, (*srcRgnB)->rgnBBox.left);
    intersect.top = MAX((*srcRgnA)->rgnBBox.top, (*srcRgnB)->rgnBBox.top);
    intersect.right = MIN((*srcRgnA)->rgnBBox.right, (*srcRgnB)->rgnBBox.right);
    intersect.bottom = MIN((*srcRgnA)->rgnBBox.bottom, (*srcRgnB)->rgnBBox.bottom);

    /* Check if intersection is valid */
    if (intersect.left < intersect.right && intersect.top < intersect.bottom) {
        (*dstRgn)->rgnBBox = intersect;
        (*dstRgn)->rgnSize = sizeof(Region);
    } else {
        /* Empty intersection */
        QuickDraw_HAL_SetEmptyRgn(dstRgn);
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* CRITICAL: Region union for update regions */
void QuickDraw_HAL_UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn)
{
    if (!srcRgnA || !srcRgnB || !dstRgn) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Calculate union of bounding boxes */
    Rect unionRect;

    /* Handle empty regions */
    Boolean aEmpty = EmptyRect(&(*srcRgnA)->rgnBBox);
    Boolean bEmpty = EmptyRect(&(*srcRgnB)->rgnBBox);

    if (aEmpty && bEmpty) {
        QuickDraw_HAL_SetEmptyRgn(dstRgn);
    } else if (aEmpty) {
        CopyRgn(srcRgnB, dstRgn);
    } else if (bEmpty) {
        CopyRgn(srcRgnA, dstRgn);
    } else {
        unionRect.left = MIN((*srcRgnA)->rgnBBox.left, (*srcRgnB)->rgnBBox.left);
        unionRect.top = MIN((*srcRgnA)->rgnBBox.top, (*srcRgnB)->rgnBBox.top);
        unionRect.right = MAX((*srcRgnA)->rgnBBox.right, (*srcRgnB)->rgnBBox.right);
        unionRect.bottom = MAX((*srcRgnA)->rgnBBox.bottom, (*srcRgnB)->rgnBBox.bottom);

        (*dstRgn)->rgnBBox = unionRect;
        (*dstRgn)->rgnSize = sizeof(Region);
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* CRITICAL: Region difference for visible region calculation */
void QuickDraw_HAL_DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn)
{
    if (!srcRgnA || !srcRgnB || !dstRgn) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* For simple rectangular regions, approximate difference */
    /* Full implementation would handle complex regions */

    /* If B doesn't intersect A, result is A */
    Rect intersect;
    if (!SectRect(&(*srcRgnA)->rgnBBox, &(*srcRgnB)->rgnBBox, &intersect)) {
        CopyRgn(srcRgnA, dstRgn);
    } else {
        /* For now, use A's bounding box (simplified) */
        /* A full implementation would compute actual difference */
        (*dstRgn)->rgnBBox = (*srcRgnA)->rgnBBox;
        (*dstRgn)->rgnSize = sizeof(Region);
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* CRITICAL: Region XOR for selection feedback */
void QuickDraw_HAL_XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn)
{
    if (!srcRgnA || !srcRgnB || !dstRgn) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* XOR is union minus intersection */
    /* For rectangular regions, we can approximate */

    /* Calculate union */
    Rect unionRect;
    unionRect.left = MIN((*srcRgnA)->rgnBBox.left, (*srcRgnB)->rgnBBox.left);
    unionRect.top = MIN((*srcRgnA)->rgnBBox.top, (*srcRgnB)->rgnBBox.top);
    unionRect.right = MAX((*srcRgnA)->rgnBBox.right, (*srcRgnB)->rgnBBox.right);
    unionRect.bottom = MAX((*srcRgnA)->rgnBBox.bottom, (*srcRgnB)->rgnBBox.bottom);

    /* For now, use union (simplified) */
    (*dstRgn)->rgnBBox = unionRect;
    (*dstRgn)->rgnSize = sizeof(Region);

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Test if point in region */
Boolean QuickDraw_HAL_PtInRgn(Point pt, RgnHandle rgn)
{
    if (!rgn) return false;

    return PtInRect(pt, &(*rgn)->rgnBBox);
}

/* Test if region empty */
Boolean QuickDraw_HAL_EmptyRgn(RgnHandle rgn)
{
    if (!rgn) return true;

    return EmptyRect(&(*rgn)->rgnBBox);
}

/* ===== Drawing Operations ===== */

/* Move pen to position */
void QuickDraw_HAL_MoveTo(int16_t h, int16_t v)
{
    if (!thePort) return;

    pthread_mutex_lock(&gQD.qdLock);
    thePort->pnLoc.h = h;
    thePort->pnLoc.v = v;
    pthread_mutex_unlock(&gQD.qdLock);
}

/* Draw line to position */
void QuickDraw_HAL_LineTo(int16_t h, int16_t v)
{
    if (!thePort) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Check clipping */
    Point startPt = thePort->pnLoc;
    Point endPt = {h, v};

    /* Simple clip test against clip region */
    if (PtInRgn(startPt, thePort->clipRgn) || PtInRgn(endPt, thePort->clipRgn)) {
        QD_HAL_DrawLine(startPt.h, startPt.v, endPt.h, endPt.v);
    }

    /* Update pen location */
    thePort->pnLoc = endPt;

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Frame rectangle */
void QuickDraw_HAL_FrameRect(const Rect* r)
{
    if (!thePort || !r) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Check clipping against clip region */
    Rect clipped;
    if (SectRect(r, &(*thePort->clipRgn)->rgnBBox, &clipped)) {
        /* Draw four lines */
        QD_HAL_DrawLine(clipped.left, clipped.top, clipped.right-1, clipped.top);
        QD_HAL_DrawLine(clipped.right-1, clipped.top, clipped.right-1, clipped.bottom-1);
        QD_HAL_DrawLine(clipped.right-1, clipped.bottom-1, clipped.left, clipped.bottom-1);
        QD_HAL_DrawLine(clipped.left, clipped.bottom-1, clipped.left, clipped.top);
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Paint rectangle */
void QuickDraw_HAL_PaintRect(const Rect* r)
{
    if (!thePort || !r) return;

    FillRect(r, &thePort->pnPat);
}

/* Fill rectangle with pattern */
void QuickDraw_HAL_FillRect(const Rect* r, const Pattern* pat)
{
    if (!thePort || !r || !pat) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Check clipping */
    Rect clipped;
    if (SectRect(r, &(*thePort->clipRgn)->rgnBBox, &clipped)) {
        QD_HAL_FillRectangle(&clipped, pat);
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Erase rectangle */
void QuickDraw_HAL_EraseRect(const Rect* r)
{
    if (!thePort || !r) return;

    FillRect(r, &gQD.white);
}

/* Invert rectangle */
void QuickDraw_HAL_InvertRect(const Rect* r)
{
    if (!thePort || !r) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Check clipping */
    Rect clipped;
    if (SectRect(r, &(*thePort->clipRgn)->rgnBBox, &clipped)) {
#ifdef __linux__
        if (gQD.cairo_context) {
            cairo_set_operator(gQD.cairo_context, CAIRO_OPERATOR_XOR);
            cairo_set_source_rgb(gQD.cairo_context, 1.0, 1.0, 1.0);
            cairo_rectangle(gQD.cairo_context, clipped.left, clipped.top,
                          clipped.right - clipped.left,
                          clipped.bottom - clipped.top);
            cairo_fill(gQD.cairo_context);
            cairo_set_operator(gQD.cairo_context, CAIRO_OPERATOR_OVER);
        }
#endif
#ifdef __APPLE__
        if (gQD.cg_context) {
            CGContextSetBlendMode(gQD.cg_context, kCGBlendModeExclusion);
            CGContextSetRGBFillColor(gQD.cg_context, 1.0, 1.0, 1.0, 1.0);
            CGContextFillRect(gQD.cg_context,
                            CGRectMake(clipped.left, clipped.top,
                                     clipped.right - clipped.left,
                                     clipped.bottom - clipped.top));
            CGContextSetBlendMode(gQD.cg_context, kCGBlendModeNormal);
        }
#endif
    }

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Draw string */
void QuickDraw_HAL_DrawString(ConstStr255Param s)
{
    if (!thePort || !s || s[0] == 0) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Check if pen location is in clip region */
    if (PtInRgn(thePort->pnLoc, thePort->clipRgn)) {
#ifdef __linux__
        if (gQD.cairo_context) {
            char cStr[256];
            memcpy(cStr, s + 1, s[0]);
            cStr[s[0]] = '\0';

            cairo_move_to(gQD.cairo_context, thePort->pnLoc.h, thePort->pnLoc.v);
            cairo_show_text(gQD.cairo_context, cStr);
        }
#endif
#ifdef __APPLE__
        if (gQD.cg_context) {
            char cStr[256];
            memcpy(cStr, s + 1, s[0]);
            cStr[s[0]] = '\0';

            CGContextShowTextAtPoint(gQD.cg_context,
                                    thePort->pnLoc.h, thePort->pnLoc.v,
                                    cStr, s[0]);
        }
#endif
    }

    /* Advance pen position */
    thePort->pnLoc.h += StringWidth(s);

    pthread_mutex_unlock(&gQD.qdLock);
}

/* Get string width */
int16_t QuickDraw_HAL_StringWidth(ConstStr255Param s)
{
    if (!s) return 0;

    /* Simplified: assume 7 pixels per character */
    return s[0] * 7;
}

/* CRITICAL: CopyBits for efficient blitting */
void QuickDraw_HAL_CopyBits(const BitMap* srcBits, const BitMap* dstBits,
                            const Rect* srcRect, const Rect* dstRect,
                            int16_t mode, RgnHandle maskRgn)
{
    if (!srcBits || !dstBits || !srcRect || !dstRect) return;

    pthread_mutex_lock(&gQD.qdLock);

    /* Calculate actual copy rectangles with clipping */
    Rect actualSrc = *srcRect;
    Rect actualDst = *dstRect;

    /* Clip destination against port's clip region */
    if (thePort && thePort->clipRgn) {
        Rect clipped;
        if (!SectRect(&actualDst, &(*thePort->clipRgn)->rgnBBox, &clipped)) {
            pthread_mutex_unlock(&gQD.qdLock);
            return;  /* Completely clipped */
        }

        /* Adjust source rectangle proportionally */
        float xScale = (float)(srcRect->right - srcRect->left) /
                      (float)(dstRect->right - dstRect->left);
        float yScale = (float)(srcRect->bottom - srcRect->top) /
                      (float)(dstRect->bottom - dstRect->top);

        actualSrc.left = srcRect->left + (clipped.left - dstRect->left) * xScale;
        actualSrc.top = srcRect->top + (clipped.top - dstRect->top) * yScale;
        actualSrc.right = srcRect->left + (clipped.right - dstRect->left) * xScale;
        actualSrc.bottom = srcRect->top + (clipped.bottom - dstRect->top) * yScale;

        actualDst = clipped;
    }

    /* Apply mask region if provided */
    if (maskRgn) {
        Rect masked;
        if (!SectRect(&actualDst, &(*maskRgn)->rgnBBox, &masked)) {
            pthread_mutex_unlock(&gQD.qdLock);
            return;  /* Completely masked */
        }
        /* Adjust rectangles for mask */
        /* Full implementation would handle complex mask regions */
    }

    /* Perform the actual copy */
    /* Platform-specific implementation would do the actual blitting */
#ifdef __linux__
    if (gQD.cairo_context) {
        /* Cairo-based implementation */
        /* This would copy pixel data from source to destination */
    }
#endif
#ifdef __APPLE__
    if (gQD.cg_context) {
        /* Core Graphics implementation */
        /* This would use CGContextDrawImage or similar */
    }
#endif

    pthread_mutex_unlock(&gQD.qdLock);
}

/* ===== Utility Functions ===== */

/* Set rectangle */
void QuickDraw_HAL_SetRect(Rect* r, int16_t left, int16_t top,
                          int16_t right, int16_t bottom)
{
    if (!r) return;

    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
}

/* Offset rectangle */
void QuickDraw_HAL_OffsetRect(Rect* r, int16_t dh, int16_t dv)
{
    if (!r) return;

    r->left += dh;
    r->top += dv;
    r->right += dh;
    r->bottom += dv;
}

/* Test if rectangles intersect */
Boolean QuickDraw_HAL_SectRect(const Rect* src1, const Rect* src2, Rect* dstRect)
{
    if (!src1 || !src2) return false;

    int16_t left = MAX(src1->left, src2->left);
    int16_t top = MAX(src1->top, src2->top);
    int16_t right = MIN(src1->right, src2->right);
    int16_t bottom = MIN(src1->bottom, src2->bottom);

    if (left < right && top < bottom) {
        if (dstRect) {
            dstRect->left = left;
            dstRect->top = top;
            dstRect->right = right;
            dstRect->bottom = bottom;
        }
        return true;
    }

    return false;
}

/* Test if rectangle empty */
Boolean QuickDraw_HAL_EmptyRect(const Rect* r)
{
    if (!r) return true;

    return (r->left >= r->right) || (r->top >= r->bottom);
}

/* Test if point in rectangle */
Boolean QuickDraw_HAL_PtInRect(Point pt, const Rect* r)
{
    if (!r) return false;

    return (pt.h >= r->left && pt.h < r->right &&
            pt.v >= r->top && pt.v < r->bottom);
}

/* ===== Internal Helper Functions ===== */

/* Initialize patterns */
static void QD_HAL_InitPatterns(void)
{
    /* White pattern */
    memset(&gQD.white, 0x00, sizeof(Pattern));

    /* Black pattern */
    memset(&gQD.black, 0xFF, sizeof(Pattern));

    /* Gray pattern (50% gray) */
    gQD.gray.pat[0] = 0xAA;
    gQD.gray.pat[1] = 0x55;
    gQD.gray.pat[2] = 0xAA;
    gQD.gray.pat[3] = 0x55;
    gQD.gray.pat[4] = 0xAA;
    gQD.gray.pat[5] = 0x55;
    gQD.gray.pat[6] = 0xAA;
    gQD.gray.pat[7] = 0x55;

    /* Light gray pattern */
    gQD.ltGray.pat[0] = 0x88;
    gQD.ltGray.pat[1] = 0x22;
    gQD.ltGray.pat[2] = 0x88;
    gQD.ltGray.pat[3] = 0x22;
    gQD.ltGray.pat[4] = 0x88;
    gQD.ltGray.pat[5] = 0x22;
    gQD.ltGray.pat[6] = 0x88;
    gQD.ltGray.pat[7] = 0x22;

    /* Dark gray pattern */
    gQD.dkGray.pat[0] = 0xDD;
    gQD.dkGray.pat[1] = 0x77;
    gQD.dkGray.pat[2] = 0xDD;
    gQD.dkGray.pat[3] = 0x77;
    gQD.dkGray.pat[4] = 0xDD;
    gQD.dkGray.pat[5] = 0x77;
    gQD.dkGray.pat[6] = 0xDD;
    gQD.dkGray.pat[7] = 0x77;
}

/* Initialize screen bitmap */
static void QD_HAL_InitScreenBits(void)
{
    gQD.screenBits.baseAddr = NULL;  /* Would point to screen memory */
    gQD.screenBits.rowBytes = 128;   /* 1024 pixels / 8 bits */
    SetRect(&gQD.screenBits.bounds, 0, 0, 1024, 768);
}

/* Platform-specific line drawing */
static void QD_HAL_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
#ifdef __linux__
    if (gQD.cairo_context) {
        cairo_move_to(gQD.cairo_context, x1, y1);
        cairo_line_to(gQD.cairo_context, x2, y2);
        cairo_stroke(gQD.cairo_context);
    }
#endif
#ifdef __APPLE__
    if (gQD.cg_context) {
        CGContextMoveToPoint(gQD.cg_context, x1, y1);
        CGContextAddLineToPoint(gQD.cg_context, x2, y2);
        CGContextStrokePath(gQD.cg_context);
    }
#endif
}

/* Platform-specific rectangle filling */
static void QD_HAL_FillRectangle(const Rect* r, const Pattern* pat)
{
#ifdef __linux__
    if (gQD.cairo_context) {
        /* Set pattern based on pattern data */
        if (memcmp(pat, &gQD.white, sizeof(Pattern)) == 0) {
            cairo_set_source_rgb(gQD.cairo_context, 1.0, 1.0, 1.0);
        } else if (memcmp(pat, &gQD.black, sizeof(Pattern)) == 0) {
            cairo_set_source_rgb(gQD.cairo_context, 0.0, 0.0, 0.0);
        } else if (memcmp(pat, &gQD.gray, sizeof(Pattern)) == 0) {
            cairo_set_source_rgb(gQD.cairo_context, 0.5, 0.5, 0.5);
        } else {
            cairo_set_source_rgb(gQD.cairo_context, 0.75, 0.75, 0.75);
        }

        cairo_rectangle(gQD.cairo_context, r->left, r->top,
                       r->right - r->left, r->bottom - r->top);
        cairo_fill(gQD.cairo_context);
    }
#endif
#ifdef __APPLE__
    if (gQD.cg_context) {
        /* Set pattern based on pattern data */
        if (memcmp(pat, &gQD.white, sizeof(Pattern)) == 0) {
            CGContextSetRGBFillColor(gQD.cg_context, 1.0, 1.0, 1.0, 1.0);
        } else if (memcmp(pat, &gQD.black, sizeof(Pattern)) == 0) {
            CGContextSetRGBFillColor(gQD.cg_context, 0.0, 0.0, 0.0, 1.0);
        } else if (memcmp(pat, &gQD.gray, sizeof(Pattern)) == 0) {
            CGContextSetRGBFillColor(gQD.cg_context, 0.5, 0.5, 0.5, 1.0);
        } else {
            CGContextSetRGBFillColor(gQD.cg_context, 0.75, 0.75, 0.75, 1.0);
        }

        CGContextFillRect(gQD.cg_context,
                         CGRectMake(r->left, r->top,
                                   r->right - r->left, r->bottom - r->top));
    }
#endif
}

/* Terminate QuickDraw */
void QuickDraw_HAL_Terminate(void)
{
    if (!gQD.initialized) return;

    /* Dispose scratch region */
    if (gQD.theRgn) {
        DisposeRgn(gQD.theRgn);
    }

#ifdef __linux__
    if (gQD.cairo_context) {
        cairo_destroy(gQD.cairo_context);
    }
    if (gQD.cairo_surface) {
        cairo_surface_destroy(gQD.cairo_surface);
    }
    if (gQD.display) {
        XCloseDisplay(gQD.display);
    }
#endif

#ifdef __APPLE__
    if (gQD.cg_context) {
        CGContextRelease(gQD.cg_context);
    }
#endif

    pthread_mutex_destroy(&gQD.qdLock);
    gQD.initialized = false;
}

/* Initialize on first use */
__attribute__((constructor))
static void QuickDraw_HAL_Init(void)
{
    /* QuickDraw will be initialized when InitGraf is called */
}

/* Cleanup on exit */
__attribute__((destructor))
static void QuickDraw_HAL_Cleanup(void)
{
    QuickDraw_HAL_Terminate();
}
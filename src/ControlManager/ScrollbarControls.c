/**
 * @file ScrollbarControls.c
 * @brief System 7-style Scrollbar Controls Implementation
 *
 * Implements vertical and horizontal scrollbar controls with classic Mac semantics:
 * - Arrow buttons (up/down or left/right) with repeat-on-hold
 * - Page areas (click-to-scroll by visible span) with repeat-on-hold
 * - Draggable thumb with proportional sizing
 * - Integration with List Manager via LAttachScrollbars()
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "SystemTypes.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "QuickDraw/QuickDrawPlatform.h"
#include "EventManager/EventManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "System71StdLib.h"

/* External QuickDraw functions */
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);
extern void GetClip(RgnHandle rgn);
extern void SetClip(RgnHandle rgn);
extern RgnHandle NewRgn(void);
extern void DisposeRgn(RgnHandle rgn);
extern void ClipRect(const Rect* r);
extern void FrameRect(const Rect* r);
extern void PaintRect(const Rect* r);
extern void EraseRect(const Rect* r);
extern void InvertRect(const Rect* r);
extern void MoveTo(short h, short v);
extern void LineTo(short h, short v);
extern void FillRect(const Rect* r, const Pattern* pat);
extern void PenPat(const Pattern* pat);
extern void PenMode(short mode);
extern Boolean PtInRect(Point pt, const Rect* r);
extern struct QDGlobals qd;

/* Logging helpers */
#define CTRL_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleControl, kLogLevelDebug, "[CTRL] " fmt, ##__VA_ARGS__)
#define CTRL_LOG_TRACE(fmt, ...) serial_logf(kLogModuleControl, kLogLevelTrace, "[CTRL] " fmt, ##__VA_ARGS__)

/* Scrollbar constants */
#define SCROLLBAR_WIDTH     16
#define MIN_THUMB_SIZE      10
#define ARROW_INITIAL_DELAY 8   /* Ticks before first repeat */
#define ARROW_REPEAT_RATE   3   /* Ticks between repeats */
#define PAGE_INITIAL_DELAY  8
#define PAGE_REPEAT_RATE    4

/* Scrollbar procID (must match registration) */
#define scrollBarProc       16

/* Internal scrollbar data structure */
typedef struct ScrollBarData {
    Boolean vertical;           /* True for vertical, false for horizontal */
    short visibleSpan;         /* Visible rows/cols (for thumb sizing) */

    /* Cached geometry */
    Rect upArrow;              /* Up or left arrow rect */
    Rect downArrow;            /* Down or right arrow rect */
    Rect trackRect;            /* Total track area */
    Rect thumbRect;            /* Current thumb rect */
    Rect pageUpRect;           /* Page up/left area */
    Rect pageDownRect;         /* Page down/right area */

    /* Tracking state */
    short pressedPart;         /* Currently pressed part (0 = none) */
    UInt32 lastActionTime;     /* Tick of last action */
    Boolean initialDelay;      /* True if waiting for initial delay */
} ScrollBarData;

/* Forward declarations */
static void CalcScrollbarRegions(ControlHandle c);
static void CalcThumbRect(ControlHandle c);
static void CalcThumbRectForValue(ControlHandle c, short value, Rect* thumbRect);
static void DrawScrollbarArrow(GrafPtr port, const Rect* r, short direction, Boolean hilite);
static void DrawScrollbarThumb(GrafPtr port, ControlHandle c, Boolean hilite);
static void DrawScrollbarTrack(GrafPtr port, const Rect* r);
static short HitTestScrollbar(ControlHandle c, Point pt);
static short CalcThumbValue(ControlHandle c, Point pt);

/* QuickDraw state restoration macro */
#define RESTORE_QD(savePort, saveClip) do { \
    if (saveClip) { SetClip(saveClip); DisposeRgn(saveClip); } \
    SetPort(savePort); \
} while(0)

static inline UInt32 ScrollGray(UInt8 level) {
    return QDPlatform_RGBToPixel(level, level, level);
}

static inline void FillSolidRectInPort(GrafPtr port, const Rect* r, UInt32 color) {
    if (!port || !r) return;
    Rect global = *r;
    global.left   += port->portBits.bounds.left;
    global.right  += port->portBits.bounds.left;
    global.top    += port->portBits.bounds.top;
    global.bottom += port->portBits.bounds.top;
    QDPlatform_FillRectAccelerated(global.left, global.top, global.right, global.bottom, color);
}

/**
 * NewVScrollBar - Create vertical scrollbar control
 */
ControlHandle NewVScrollBar(WindowPtr w, const Rect* bounds, short min, short max, short value)
{
    ControlHandle c;
    ScrollBarData* data;

    if (!w || !bounds) return NULL;

    c = NewControl(w, bounds, (ConstStr255Param)"", true, value, min, max, scrollBarProc, 0);
    if (!c) return NULL;

    /* Scrollbar data initialized in initCntl message */
    if ((*c)->contrlData) {
        data = (ScrollBarData*)(*(*c)->contrlData);
        data->vertical = true;
        data->visibleSpan = 1;
        CalcScrollbarRegions(c);
        CalcThumbRect(c);
    }

    return c;
}

/**
 * NewHScrollBar - Create horizontal scrollbar control
 */
ControlHandle NewHScrollBar(WindowPtr w, const Rect* bounds, short min, short max, short value)
{
    ControlHandle c;
    ScrollBarData* data;

    if (!w || !bounds) return NULL;

    c = NewControl(w, bounds, (ConstStr255Param)"", true, value, min, max, scrollBarProc, 0);
    if (!c) return NULL;

    if ((*c)->contrlData) {
        data = (ScrollBarData*)(*(*c)->contrlData);
        data->vertical = false;
        data->visibleSpan = 1;
        CalcScrollbarRegions(c);
        CalcThumbRect(c);
    }

    return c;
}

/**
 * UpdateScrollThumb - Update thumb for new range/value/visibleSpan
 * Called by List Manager when content changes
 */
void UpdateScrollThumb(ControlHandle c, short value, short min, short max, short visibleSpan)
{
    ScrollBarData* data;

    if (!c || !(*c)->contrlData) return;

    data = (ScrollBarData*)(*(*c)->contrlData);

    (*c)->contrlMin = min;
    (*c)->contrlMax = max;
    (*c)->contrlValue = value;
    data->visibleSpan = visibleSpan > 0 ? visibleSpan : 1;

    CalcThumbRect(c);

    /* Redraw if visible */
    if ((*c)->contrlVis) {
        Draw1Control(c);
    }
}

/**
 * ScrollBarCDEF - Scrollbar control definition function
 */
SInt32 ScrollBarCDEF(SInt16 varCode, ControlHandle theControl, SInt16 message, SInt32 param)
{
    ScrollBarData* data;
    Point pt;
    Rect bounds;

    (void)varCode; /* Unused */

    if (!theControl) return 0;

    switch (message) {
    case initCntl:
        /* Allocate scrollbar data */
        (*theControl)->contrlData = NewHandle(sizeof(ScrollBarData));
        if ((*theControl)->contrlData) {
            data = (ScrollBarData*)(*(*theControl)->contrlData);
            bounds = (*theControl)->contrlRect;

            /* Determine orientation from aspect ratio */
            data->vertical = (bounds.bottom - bounds.top) > (bounds.right - bounds.left);
            data->visibleSpan = 1;
            data->pressedPart = 0;
            data->lastActionTime = 0;
            data->initialDelay = true;

            CalcScrollbarRegions(theControl);
            CalcThumbRect(theControl);
        }
        break;

    case dispCntl:
        if ((*theControl)->contrlData) {
            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        DrawScrollBar(theControl);
        break;

    case testCntl:
        pt.h = (short)((param >> 16) & 0xFFFF);
        pt.v = (short)(param & 0xFFFF);
        return HitTestScrollbar(theControl, pt);

    case posCntl:
    case calcCRgns:
    case calcCntlRgn:
        CalcScrollbarRegions(theControl);
        CalcThumbRect(theControl);
        break;
    }

    return 0;
}

/**
 * DrawScrollBar - Draw complete scrollbar
 */
void DrawScrollBar(ControlHandle scrollBar)
{
    GrafPtr savePort;
    RgnHandle saveClip;
    ScrollBarData* data;
    Boolean hiliteUp, hiliteDown, hiliteThumb;
    Boolean disabled;

    if (!scrollBar || !(*scrollBar)->contrlData) return;

    data = (ScrollBarData*)(*(*scrollBar)->contrlData);

    /* Save QD state */
    GetPort(&savePort);
    SetPort((GrafPtr)(*scrollBar)->contrlOwner);
    GrafPtr ownerPort = (GrafPtr)(*scrollBar)->contrlOwner;
    saveClip = NewRgn();
    if (saveClip) {
        GetClip(saveClip);
        ClipRect(&(*scrollBar)->contrlRect);
    }

    SInt32 savedFG = ((GrafPtr)(*scrollBar)->contrlOwner)->fgColor;
    SInt32 savedBG = ((GrafPtr)(*scrollBar)->contrlOwner)->bkColor;

    ForeColor(blackColor);
    BackColor(whiteColor);

    /* Check if disabled */
    disabled = ((*scrollBar)->contrlMax <= (*scrollBar)->contrlMin) ||
               ((*scrollBar)->contrlHilite == (UInt8)inactiveHilite);

    /* Determine hilite states */
    hiliteUp = (data->pressedPart == inUpButton);
    hiliteDown = (data->pressedPart == inDownButton);
    hiliteThumb = (data->pressedPart == inThumb);

    /* Draw track */
    DrawScrollbarTrack(ownerPort, &data->trackRect);

    /* Draw arrows */
    DrawScrollbarArrow(ownerPort, &data->upArrow, data->vertical ? 0 : 3, hiliteUp);
    DrawScrollbarArrow(ownerPort, &data->downArrow, data->vertical ? 1 : 2, hiliteDown);

    /* Draw thumb even when disabled so control remains visible */
    DrawScrollbarThumb(ownerPort, scrollBar, hiliteThumb && !disabled);

    /* Gray out if inactive */
    if ((*scrollBar)->contrlHilite == (UInt8)inactiveHilite) {
        PenMode(patBic);
        PenPat(&qd.gray);
        PaintRect(&(*scrollBar)->contrlRect);
        PenMode(patCopy);
        PenPat(&qd.black);
    }

    ForeColor(savedFG);
    BackColor(savedBG);

    RESTORE_QD(savePort, saveClip);
}

/**
 * ScrollbarHilite - Highlight a scrollbar part (internal helper)
 */
static void ScrollbarHilite(ControlHandle c, short part)
{
    ScrollBarData* data;

    if (!c || !(*c)->contrlData) return;

    data = (ScrollBarData*)(*(*c)->contrlData);
    data->pressedPart = part;

    if ((*c)->contrlVis) {
        Draw1Control(c);
    }
}

/* Note: TestControl is provided by ControlManager core - no need to redefine */

/**
 * TrackScrollbar - Track mouse in scrollbar until button release
 * Returns part code and sets *outDelta to value change
 * Note: Use this instead of TrackControl for scrollbars when you need delta values
 */
short TrackScrollbar(ControlHandle c, Point startLocal, short startPart,
                     short modifiers, short* outDelta)
{
    ScrollBarData* data;
    short startValue, newValue, delta;
    Point pt;
    Boolean stillInPart;
    UInt32 now;
    short trackPart;

    (void)modifiers; /* Unused */

    if (!c || !(*c)->contrlData || !outDelta) return 0;

    data = (ScrollBarData*)(*(*c)->contrlData);
    startValue = (*c)->contrlValue;  /* Save initial value for final delta */
    delta = 0;
    trackPart = startPart;

    /* Highlight pressed part */
    ScrollbarHilite(c, startPart);

    if (startPart == inThumb) {
        /* Thumb drag tracking */
        const UInt32 MAX_THUMB_ITERATIONS = 100000;  /* Safety timeout: ~1666 seconds at 60Hz */
        UInt32 loopCount = 0;

        while (StillDown() && loopCount < MAX_THUMB_ITERATIONS) {
            loopCount++;
            GetMouse(&pt);
            newValue = CalcThumbValue(c, pt);
            if (newValue != (*c)->contrlValue) {
                /* OPTIMIZATION: Invalidate only the region affected by thumb movement
                 * instead of redrawing the entire scrollbar control */
                short oldValue = (*c)->contrlValue;
                Rect oldThumbRect, newThumbRect, updateRect;

                /* Calculate old thumb position */
                CalcThumbRectForValue(c, oldValue, &oldThumbRect);

                /* Calculate new thumb position */
                CalcThumbRectForValue(c, newValue, &newThumbRect);

                /* Get union of old and new thumb rectangles */
                extern void UnionRect(const Rect* src1, const Rect* src2, Rect* dstRect);
                UnionRect(&oldThumbRect, &newThumbRect, &updateRect);

                /* Update the control value without full redraw */
                (*c)->contrlValue = newValue;

                /* Invalidate only the affected region for efficient redraw */
                if ((*c)->contrlOwner) {
                    extern void InvalRect(const Rect* badRect);
                    InvalRect(&updateRect);
                }

                /* Redraw just the affected area */
                Draw1Control(c);
            }
        }

        if (loopCount >= MAX_THUMB_ITERATIONS) {
            /* Safety timeout reached - log warning */
            CTRL_LOG_DEBUG("ScrollbarControls: Thumb drag loop timeout after %u iterations\n", loopCount);
        }
    } else if (startPart == inUpButton || startPart == inDownButton ||
               startPart == inPageUp || startPart == inPageDown) {
        /* Arrow or page tracking with repeat */
        Boolean isPage;
        UInt32 initialDelay, repeatRate;

        /* Determine timing based on part type */
        isPage = (startPart == inPageUp || startPart == inPageDown);
        initialDelay = ARROW_INITIAL_DELAY;  /* Same value for both (8 ticks) */
        repeatRate = isPage ? PAGE_REPEAT_RATE : ARROW_REPEAT_RATE;

        data->initialDelay = true;
        data->lastActionTime = TickCount();

        /* Perform initial action */
        if (startPart == inUpButton) {
            delta = -1;
        } else if (startPart == inDownButton) {
            delta = 1;
        } else if (startPart == inPageUp) {
            delta = -data->visibleSpan;
        } else if (startPart == inPageDown) {
            delta = data->visibleSpan;
        }

        newValue = (*c)->contrlValue + delta;
        if (newValue < (*c)->contrlMin) newValue = (*c)->contrlMin;
        if (newValue > (*c)->contrlMax) newValue = (*c)->contrlMax;
        SetControlValue(c, newValue);

        /* Track with repeat using part-specific timing */
        const UInt32 MAX_REPEAT_ITERATIONS = 100000;  /* Safety timeout */
        UInt32 loopCount = 0;

        while (StillDown() && loopCount < MAX_REPEAT_ITERATIONS) {
            loopCount++;
            GetMouse(&pt);
            stillInPart = (HitTestScrollbar(c, pt) == startPart);

            if (stillInPart) {
                now = TickCount();
                if (data->initialDelay) {
                    if (now - data->lastActionTime >= initialDelay) {
                        data->initialDelay = false;
                        data->lastActionTime = now;
                    }
                } else {
                    if (now - data->lastActionTime >= repeatRate) {
                        /* Repeat action */
                        if (startPart == inUpButton) {
                            delta = -1;
                        } else if (startPart == inDownButton) {
                            delta = 1;
                        } else if (startPart == inPageUp) {
                            delta = -data->visibleSpan;
                        } else {
                            delta = data->visibleSpan;
                        }

                        newValue = (*c)->contrlValue + delta;
                        if (newValue < (*c)->contrlMin) newValue = (*c)->contrlMin;
                        if (newValue > (*c)->contrlMax) newValue = (*c)->contrlMax;
                        SetControlValue(c, newValue);

                        data->lastActionTime = now;
                    }
                }

                /* Keep highlighted */
                if (data->pressedPart != startPart) {
                    ScrollbarHilite(c, startPart);
                }
            } else {
                /* Mouse left part - unhighlight */
                if (data->pressedPart == startPart) {
                    ScrollbarHilite(c, 0);
                }
            }
        }

        if (loopCount >= MAX_REPEAT_ITERATIONS) {
            /* Safety timeout reached - log warning */
            CTRL_LOG_DEBUG("ScrollbarControls: Repeat tracking loop timeout after %u iterations\n", loopCount);
        }
    }

    /* Unhighlight */
    ScrollbarHilite(c, 0);

    /* Compute final delta from start to end */
    *outDelta = (*c)->contrlValue - startValue;

    CTRL_LOG_TRACE("TrackScrollbar: part=%d delta=%d\n", trackPart, *outDelta);

    return trackPart;
}

/* ================================================================
 * INTERNAL IMPLEMENTATION
 * ================================================================ */

/**
 * CalcScrollbarRegions - Compute arrow and track rects
 */
static void CalcScrollbarRegions(ControlHandle c)
{
    ScrollBarData* data;
    Rect bounds;

    if (!c || !(*c)->contrlData) return;

    data = (ScrollBarData*)(*(*c)->contrlData);
    bounds = (*c)->contrlRect;

    if (data->vertical) {
        /* Vertical: arrows at top/bottom */
        data->upArrow = bounds;
        data->upArrow.bottom = data->upArrow.top + SCROLLBAR_WIDTH;

        data->downArrow = bounds;
        data->downArrow.top = data->downArrow.bottom - SCROLLBAR_WIDTH;

        data->trackRect = bounds;
        data->trackRect.top = data->upArrow.bottom;
        data->trackRect.bottom = data->downArrow.top;
    } else {
        /* Horizontal: arrows at left/right */
        data->upArrow = bounds;
        data->upArrow.right = data->upArrow.left + SCROLLBAR_WIDTH;

        data->downArrow = bounds;
        data->downArrow.left = data->downArrow.right - SCROLLBAR_WIDTH;

        data->trackRect = bounds;
        data->trackRect.left = data->upArrow.right;
        data->trackRect.right = data->downArrow.left;
    }
}

/**
 * CalcThumbRect - Compute thumb rect based on value/range/visibleSpan
 */
static void CalcThumbRect(ControlHandle c)
{
    ScrollBarData* data;
    short range, trackLen, thumbLen, thumbPos;
    short value;

    if (!c || !(*c)->contrlData) return;

    data = (ScrollBarData*)(*(*c)->contrlData);
    range = (*c)->contrlMax - (*c)->contrlMin;
    value = (*c)->contrlValue - (*c)->contrlMin;

    if (data->vertical) {
        trackLen = data->trackRect.bottom - data->trackRect.top;
    } else {
        trackLen = data->trackRect.right - data->trackRect.left;
    }

    if (range <= 0 || trackLen <= 0) {
        /* Disabled or invalid - thumb fills track */
        data->thumbRect = data->trackRect;
        data->pageUpRect = data->trackRect;
        data->pageUpRect.bottom = data->pageUpRect.top;
        data->pageDownRect = data->trackRect;
        data->pageDownRect.top = data->pageDownRect.bottom;
        return;
    }

    /* Proportional thumb: size = (visible / total) * trackLen */
    thumbLen = (data->visibleSpan * trackLen) / (range + data->visibleSpan);
    if (thumbLen < MIN_THUMB_SIZE) thumbLen = MIN_THUMB_SIZE;
    if (thumbLen > trackLen) thumbLen = trackLen;

    /* Thumb position */
    if (range > 0) {
        thumbPos = (value * (trackLen - thumbLen)) / range;
    } else {
        thumbPos = 0;
    }

    /* Build thumb rect */
    if (data->vertical) {
        data->thumbRect = data->trackRect;
        data->thumbRect.top += thumbPos;
        data->thumbRect.bottom = data->thumbRect.top + thumbLen;

        /* Page areas */
        data->pageUpRect = data->trackRect;
        data->pageUpRect.bottom = data->thumbRect.top;

        data->pageDownRect = data->trackRect;
        data->pageDownRect.top = data->thumbRect.bottom;
    } else {
        data->thumbRect = data->trackRect;
        data->thumbRect.left += thumbPos;
        data->thumbRect.right = data->thumbRect.left + thumbLen;

        data->pageUpRect = data->trackRect;
        data->pageUpRect.right = data->thumbRect.left;

        data->pageDownRect = data->trackRect;
        data->pageDownRect.left = data->thumbRect.right;
    }
}

/**
 * CalcThumbRectForValue - Compute thumb rect for a specific value
 * This is a helper function for optimized scrollbar dragging
 */
static void CalcThumbRectForValue(ControlHandle c, short value, Rect* thumbRect)
{
    ScrollBarData* data;
    short range, trackLen, thumbLen, thumbPos;
    short relValue;

    if (!c || !(*c)->contrlData || !thumbRect) return;

    data = (ScrollBarData*)(*(*c)->contrlData);
    range = (*c)->contrlMax - (*c)->contrlMin;
    relValue = value - (*c)->contrlMin;

    if (data->vertical) {
        trackLen = data->trackRect.bottom - data->trackRect.top;
    } else {
        trackLen = data->trackRect.right - data->trackRect.left;
    }

    if (range <= 0 || trackLen <= 0) {
        /* Disabled or invalid - thumb fills track */
        *thumbRect = data->trackRect;
        return;
    }

    /* Proportional thumb: size = (visible / total) * trackLen */
    thumbLen = (data->visibleSpan * trackLen) / (range + data->visibleSpan);
    if (thumbLen < MIN_THUMB_SIZE) thumbLen = MIN_THUMB_SIZE;
    if (thumbLen > trackLen) thumbLen = trackLen;

    /* Thumb position */
    if (range > 0) {
        thumbPos = (relValue * (trackLen - thumbLen)) / range;
    } else {
        thumbPos = 0;
    }

    /* Build thumb rect */
    if (data->vertical) {
        *thumbRect = data->trackRect;
        thumbRect->top += thumbPos;
        thumbRect->bottom = thumbRect->top + thumbLen;
    } else {
        *thumbRect = data->trackRect;
        thumbRect->left += thumbPos;
        thumbRect->right = thumbRect->left + thumbLen;
    }
}

/**
 * DrawScrollbarArrow - Draw arrow button
 * direction: 0=up, 1=down, 2=right, 3=left
 */
static void DrawScrollbarArrow(GrafPtr port, const Rect* r, short direction, Boolean hilite)
{
    Rect arrowFrame;
    short cx = 0, cy = 0;
    short x1, y1, x2, y2, x3, y3;

    arrowFrame = *r;

    UInt32 baseColor = hilite ? ScrollGray(0xB8) : ScrollGray(0xD0);
    FillSolidRectInPort(port, &arrowFrame, baseColor);

    /* 3D highlight */
    PenPat(&qd.white);
    MoveTo(arrowFrame.left, arrowFrame.bottom - 1);
    LineTo(arrowFrame.left, arrowFrame.top);
    LineTo(arrowFrame.right - 1, arrowFrame.top);
    PenPat(&qd.dkGray);
    LineTo(arrowFrame.right - 1, arrowFrame.bottom - 1);
    LineTo(arrowFrame.left, arrowFrame.bottom - 1);
    PenPat(&qd.black);
    FrameRect(&arrowFrame);

    /* Draw arrow triangle */
    cx = (arrowFrame.left + arrowFrame.right) / 2;
    cy = (arrowFrame.top + arrowFrame.bottom) / 2;

    PenPat(&qd.black);

    switch (direction) {
    case 0: /* Up */
        x1 = cx; y1 = cy - 3;
        x2 = cx - 3; y2 = cy + 3;
        x3 = cx + 3; y3 = cy + 3;
        break;
    case 1: /* Down */
        x1 = cx; y1 = cy + 3;
        x2 = cx - 3; y2 = cy - 3;
        x3 = cx + 3; y3 = cy - 3;
        break;
    case 2: /* Right */
        x1 = cx + 3; y1 = cy;
        x2 = cx - 3; y2 = cy - 3;
        x3 = cx - 3; y3 = cy + 3;
        break;
    default: /* Left */
        x1 = cx - 3; y1 = cy;
        x2 = cx + 3; y2 = cy - 3;
        x3 = cx + 3; y3 = cy + 3;
        break;
    }

    /* Paint triangle */
    {
        PolyHandle arrowPoly = OpenPoly();
        if (arrowPoly) {
            MoveTo(x1, y1);
            LineTo(x2, y2);
            LineTo(x3, y3);
            LineTo(x1, y1);
            ClosePoly();
            PaintPoly(arrowPoly);
            KillPoly(arrowPoly);
        } else {
            MoveTo(x1, y1);
            LineTo(x2, y2);
            LineTo(x3, y3);
            LineTo(x1, y1);
        }
    }
}

/**
 * DrawScrollbarThumb - Draw thumb indicator
 */
static void DrawScrollbarThumb(GrafPtr port, ControlHandle c, Boolean hilite)
{
    ScrollBarData* data;
    Rect thumb;
    short cx = 0;
    short cy = 0;

    if (!c || !(*c)->contrlData) return;

    data = (ScrollBarData*)(*(*c)->contrlData);
    thumb = data->thumbRect;

    /* Don't draw if too small */
    if (thumb.right <= thumb.left + 2 || thumb.bottom <= thumb.top + 2) {
        return;
    }

    UInt32 baseColor = hilite ? ScrollGray(0x90) : ScrollGray(0xBC);
    FillSolidRectInPort(port, &thumb, baseColor);

    if (!hilite) {
        PenPat(&qd.white);
        MoveTo(thumb.left, thumb.bottom - 1);
        LineTo(thumb.left, thumb.top);
        LineTo(thumb.right - 1, thumb.top);

        PenPat(&qd.dkGray);
        LineTo(thumb.right - 1, thumb.bottom - 1);
        LineTo(thumb.left, thumb.bottom - 1);

        /* Draw grip */
        cx = (thumb.left + thumb.right) / 2;
        cy = (thumb.top + thumb.bottom) / 2;

        PenPat(&qd.dkGray);
        if (data->vertical && (thumb.bottom - thumb.top) >= 12) {
            MoveTo(cx - 3, cy - 1);
            LineTo(cx + 4, cy - 1);
            MoveTo(cx - 3, cy + 1);
            LineTo(cx + 4, cy + 1);
        } else if (!data->vertical && (thumb.right - thumb.left) >= 12) {
            MoveTo(cx - 1, cy - 3);
            LineTo(cx - 1, cy + 4);
            MoveTo(cx + 1, cy - 3);
            LineTo(cx + 1, cy + 4);
        }
    } else {
        cx = (thumb.left + thumb.right) / 2;
        cy = (thumb.top + thumb.bottom) / 2;
    }

    PenPat(&qd.black);
    FrameRect(&thumb);
}

/**
 * DrawScrollbarTrack - Draw track background
 */
static void DrawScrollbarTrack(GrafPtr port, const Rect* r)
{
    FillSolidRectInPort(port, r, ScrollGray(0xE0));

    /* Draw simple bevel */
    PenPat(&qd.white);
    MoveTo(r->left, r->bottom - 1);
    LineTo(r->left, r->top);
    LineTo(r->right - 1, r->top);

    PenPat(&qd.dkGray);
    LineTo(r->right - 1, r->bottom - 1);
    LineTo(r->left, r->bottom - 1);

    PenPat(&qd.black);
    FrameRect(r);
}

/**
 * HitTestScrollbar - Return part code for point
 */
static short HitTestScrollbar(ControlHandle c, Point pt)
{
    ScrollBarData* data;

    if (!c || !(*c)->contrlData) return 0;

    /* Disabled scrollbars return inNothing (0) */
    if ((*c)->contrlMax <= (*c)->contrlMin ||
        (*c)->contrlHilite == (UInt8)inactiveHilite) {
        return 0;
    }

    data = (ScrollBarData*)(*(*c)->contrlData);

    if (PtInRect(pt, &data->upArrow)) return inUpButton;
    if (PtInRect(pt, &data->downArrow)) return inDownButton;
    if (PtInRect(pt, &data->thumbRect)) return inThumb;
    if (PtInRect(pt, &data->pageUpRect)) return inPageUp;
    if (PtInRect(pt, &data->pageDownRect)) return inPageDown;

    return 0;
}

/**
 * CalcThumbValue - Calculate value from thumb drag point
 */
static short CalcThumbValue(ControlHandle c, Point pt)
{
    ScrollBarData* data;
    short range, trackLen, thumbLen;
    short mousePos, trackStart;
    SInt32 newValue;

    if (!c || !(*c)->contrlData) return (*c)->contrlValue;

    data = (ScrollBarData*)(*(*c)->contrlData);
    range = (*c)->contrlMax - (*c)->contrlMin;

    if (range <= 0) return (*c)->contrlValue;

    if (data->vertical) {
        trackLen = data->trackRect.bottom - data->trackRect.top;
        thumbLen = data->thumbRect.bottom - data->thumbRect.top;
        mousePos = pt.v;
        trackStart = data->trackRect.top;
    } else {
        trackLen = data->trackRect.right - data->trackRect.left;
        thumbLen = data->thumbRect.right - data->thumbRect.left;
        mousePos = pt.h;
        trackStart = data->trackRect.left;
    }

    if (trackLen <= thumbLen) return (*c)->contrlValue;

    /* Map mouse position to value */
    newValue = ((mousePos - trackStart) * (SInt32)range) / (trackLen - thumbLen);
    newValue += (*c)->contrlMin;

    /* Clamp */
    if (newValue < (*c)->contrlMin) newValue = (*c)->contrlMin;
    if (newValue > (*c)->contrlMax) newValue = (*c)->contrlMax;

    return (short)newValue;
}

/**
 * RegisterScrollBarControlType - Register scrollbar CDEF
 */
void RegisterScrollBarControlType(void)
{
    RegisterControlType(scrollBarProc, ScrollBarCDEF);
    CTRL_LOG_DEBUG("Scrollbar control type registered (procID=%d)\n", scrollBarProc);
}

/**
 * IsScrollBarControl - Check if control is a scrollbar
 */
Boolean IsScrollBarControl(ControlHandle c)
{
    if (!c) return false;
    return (GetControlVariant(c) & 0xFFF0) == scrollBarProc;
}

/**
 * SetScrollBarPageSize - Set page size (visibleSpan)
 */
void SetScrollBarPageSize(ControlHandle c, SInt16 pageSize)
{
    ScrollBarData* data;

    if (!c || !(*c)->contrlData || pageSize < 1) return;

    data = (ScrollBarData*)(*(*c)->contrlData);
    data->visibleSpan = pageSize;

    CalcThumbRect(c);
    if ((*c)->contrlVis) Draw1Control(c);
}

/**
 * GetScrollBarPageSize - Get page size
 */
SInt16 GetScrollBarPageSize(ControlHandle c)
{
    ScrollBarData* data;

    if (!c || !(*c)->contrlData) return 0;

    data = (ScrollBarData*)(*(*c)->contrlData);
    return data->visibleSpan;
}

/**
 * SetScrollBarLiveTracking - Enable/disable live tracking (ignored for now)
 */
void SetScrollBarLiveTracking(ControlHandle c, Boolean liveTracking)
{
    (void)c;
    (void)liveTracking;
    /* Live tracking always enabled in this implementation */
}

/**
 * GetScrollBarLiveTracking - Get live tracking state
 */
Boolean GetScrollBarLiveTracking(ControlHandle c)
{
    (void)c;
    return true; /* Always enabled */
}

/* Usage example (for documentation):
 *
 * // Create scrollbars
 * ControlHandle vScroll = NewVScrollBar(win, &vRect, 0, maxRows - 1, 0);
 * ControlHandle hScroll = NewHScrollBar(win, &hRect, 0, maxCols - 1, 0);
 *
 * // Attach to list
 * LAttachScrollbars(list, vScroll, hScroll);
 *
 * // On mouse down in scrollbar
 * short part = TestControl(vScroll, localPt);
 * if (part) {
 *     short delta = 0;
 *     TrackScrollbar(vScroll, localPt, part, 0, &delta);
 *     if (delta) {
 *         LScroll(list, delta, 0);
 *     }
 * }
 */

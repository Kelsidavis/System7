/**
 * @file ScrollbarControls.c
 * @brief Scrollbar control implementation with thumb tracking
 *
 * This file implements scrollbar controls including vertical and horizontal
 * scrollbars, thumb tracking, arrow buttons, and page areas. Scrollbars are
 * essential for navigating content in lists, text areas, and other scrollable views.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "ScrollbarControls.h"
#include "ControlManager.h"
#include "ControlDrawing.h"
#include "ControlTracking.h"
#include "../QuickDraw/QuickDraw.h"
#include "../EventManager/EventManager.h"
#include "../MacTypes.h"
#include <string.h>
#include <stdlib.h>

/* Scrollbar constants */
#define SCROLLBAR_WIDTH     16
#define MIN_THUMB_SIZE      16
#define ARROW_REPEAT_DELAY  30   /* Ticks before arrow repeat starts */
#define ARROW_REPEAT_RATE   5    /* Ticks between arrow repeats */
#define PAGE_REPEAT_DELAY   20   /* Ticks before page repeat starts */
#define PAGE_REPEAT_RATE    10   /* Ticks between page repeats */

/* Scrollbar data structure */
typedef struct ScrollBarData {
    bool isVertical;             /* Vertical vs horizontal orientation */
    int16_t pageSize;            /* Page size for proportional thumb */

    /* Scrollbar regions */
    Rect upArrowRect;            /* Up/left arrow rectangle */
    Rect downArrowRect;          /* Down/right arrow rectangle */
    Rect thumbRect;              /* Thumb rectangle */
    Rect pageUpRect;             /* Page up/left rectangle */
    Rect pageDownRect;           /* Page down/right rectangle */
    Rect trackRect;              /* Complete track rectangle */

    /* Thumb calculation */
    int16_t thumbSize;           /* Calculated thumb size */
    int16_t thumbPos;            /* Calculated thumb position */
    int16_t trackLen;            /* Available track length */

    /* Tracking state */
    int16_t trackingPart;        /* Currently tracking part */
    Point trackingOffset;        /* Mouse offset from thumb center */
    uint32_t lastActionTime;     /* Time of last action */
    uint32_t actionDelay;        /* Current action delay */

    /* Visual state */
    int16_t hiliteUp;            /* Up arrow highlight state */
    int16_t hiliteDown;          /* Down arrow highlight state */
    int16_t hiliteThumb;         /* Thumb highlight state */

    /* Platform support */
    bool liveTracking;           /* Enable live tracking */
    bool showThumbOutline;       /* Show thumb outline when tracking */

} ScrollBarData;

/* Internal function prototypes */
static void CalculateScrollBarRegions(ControlHandle scrollBar);
static void UpdateScrollBarThumb(ControlHandle scrollBar);
static void DrawScrollBarArrow(const Rect *arrowRect, int16_t direction, bool pushed);
static void DrawScrollBarThumb(ControlHandle scrollBar);
static void DrawScrollBarTrack(ControlHandle scrollBar);
static int16_t CalcScrollBarPart(ControlHandle scrollBar, Point pt);
static int16_t CalcNewThumbValue(ControlHandle scrollBar, Point pt);
static void HandleScrollBarTracking(ControlHandle scrollBar, int16_t part, Point startPt);
static void DrawThumbOutline(ControlHandle scrollBar, const Rect *thumbRect);
static void ScrollBarAutoTrack(ControlHandle scrollBar);

/**
 * Register scrollbar control type
 */
void RegisterScrollBarControlType(void) {
    RegisterControlType(scrollBarProc, ScrollBarCDEF);
}

/**
 * Scroll bar control definition procedure
 */
int32_t ScrollBarCDEF(int16_t varCode, ControlHandle theControl,
                      int16_t message, int32_t param) {
    ScrollBarData *scrollData;
    Point pt;

    if (!theControl) {
        return 0;
    }

    switch (message) {
    case initCntl:
        /* Allocate scroll bar data */
        (*theControl)->contrlData = NewHandleClear(sizeof(ScrollBarData));
        if ((*theControl)->contrlData) {
            scrollData = (ScrollBarData *)*(*theControl)->contrlData;

            /* Determine orientation from control dimensions */
            Rect bounds = (*theControl)->contrlRect;
            scrollData->isVertical = (bounds.bottom - bounds.top) >
                                    (bounds.right - bounds.left);

            scrollData->pageSize = 10;  /* Default page size */
            scrollData->liveTracking = true;
            scrollData->showThumbOutline = false;

            /* Calculate scrollbar regions */
            CalculateScrollBarRegions(theControl);
            UpdateScrollBarThumb(theControl);
        }
        break;

    case dispCntl:
        /* Dispose scroll bar data */
        if ((*theControl)->contrlData) {
            DisposeHandle((*theControl)->contrlData);
            (*theControl)->contrlData = NULL;
        }
        break;

    case drawCntl:
        /* Draw scroll bar */
        DrawScrollBar(theControl);
        break;

    case testCntl:
        /* Test which part of scroll bar was hit */
        pt = *(Point *)&param;
        return CalcScrollBarPart(theControl, pt);

    case calcCRgns:
    case calcCntlRgn:
        /* Recalculate scroll bar regions */
        CalculateScrollBarRegions(theControl);
        UpdateScrollBarThumb(theControl);
        break;

    case calcThumbRgn:
        /* Calculate thumb region */
        if ((*theControl)->contrlData) {
            scrollData = (ScrollBarData *)*(*theControl)->contrlData;
            /* Return thumb rectangle encoded as parameter */
            return *(int32_t *)&scrollData->thumbRect;
        }
        break;

    case posCntl:
        /* Update thumb position for new value */
        UpdateScrollBarThumb(theControl);
        break;

    case thumbCntl:
        /* Handle thumb tracking */
        if ((*theControl)->contrlData) {
            pt = *(Point *)&param;
            HandleScrollBarTracking(theControl, inThumb, pt);
        }
        break;

    case autoTrack:
        /* Handle automatic tracking for arrows and pages */
        ScrollBarAutoTrack(theControl);
        break;

    case drawThumbOutline:
        /* Draw thumb outline (private message) */
        if ((*theControl)->contrlData) {
            scrollData = (ScrollBarData *)*(*theControl)->contrlData;
            DrawThumbOutline(theControl, &scrollData->thumbRect);
        }
        break;
    }

    return 0;
}

/**
 * Draw complete scrollbar
 */
void DrawScrollBar(ControlHandle scrollBar) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;

    /* Draw scrollbar track */
    DrawScrollBarTrack(scrollBar);

    /* Draw up/left arrow */
    DrawScrollBarArrow(&scrollData->upArrowRect,
                      scrollData->isVertical ? 0 : 3,
                      scrollData->hiliteUp != 0);

    /* Draw down/right arrow */
    DrawScrollBarArrow(&scrollData->downArrowRect,
                      scrollData->isVertical ? 1 : 2,
                      scrollData->hiliteDown != 0);

    /* Draw thumb */
    DrawScrollBarThumb(scrollBar);

    /* Gray out if inactive */
    if ((*scrollBar)->contrlHilite == inactiveHilite) {
        Rect bounds = (*scrollBar)->contrlRect;
        PenPat(&gray);
        PenMode(patBic);
        PaintRect(&bounds);
        PenMode(patCopy);
        PenPat(&black);
    }
}

/**
 * Calculate scrollbar value from point
 */
int16_t CalcScrollBarPart(ControlHandle scrollBar, Point pt) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return 0;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;

    /* Test each scrollbar part */
    if (PtInRect(pt, &scrollData->upArrowRect)) {
        return inUpButton;
    }
    if (PtInRect(pt, &scrollData->downArrowRect)) {
        return inDownButton;
    }
    if (PtInRect(pt, &scrollData->thumbRect)) {
        return inThumb;
    }
    if (PtInRect(pt, &scrollData->pageUpRect)) {
        return inPageUp;
    }
    if (PtInRect(pt, &scrollData->pageDownRect)) {
        return inPageDown;
    }

    return 0;
}

/**
 * Set scrollbar page size
 */
void SetScrollBarPageSize(ControlHandle scrollBar, int16_t pageSize) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData || pageSize < 1) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    if (scrollData->pageSize != pageSize) {
        scrollData->pageSize = pageSize;
        UpdateScrollBarThumb(scrollBar);

        /* Redraw if visible */
        if ((*scrollBar)->contrlVis) {
            Draw1Control(scrollBar);
        }
    }
}

/**
 * Get scrollbar page size
 */
int16_t GetScrollBarPageSize(ControlHandle scrollBar) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return 0;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    return scrollData->pageSize;
}

/**
 * Set scrollbar live tracking
 */
void SetScrollBarLiveTracking(ControlHandle scrollBar, bool liveTracking) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    scrollData->liveTracking = liveTracking;
}

/**
 * Get scrollbar live tracking
 */
bool GetScrollBarLiveTracking(ControlHandle scrollBar) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return false;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    return scrollData->liveTracking;
}

/* Internal Implementation */

/**
 * Calculate scrollbar regions
 */
static void CalculateScrollBarRegions(ControlHandle scrollBar) {
    ScrollBarData *scrollData;
    Rect bounds;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    bounds = (*scrollBar)->contrlRect;

    if (scrollData->isVertical) {
        /* Vertical scrollbar */
        scrollData->upArrowRect = bounds;
        scrollData->upArrowRect.bottom = scrollData->upArrowRect.top + SCROLLBAR_WIDTH;

        scrollData->downArrowRect = bounds;
        scrollData->downArrowRect.top = scrollData->downArrowRect.bottom - SCROLLBAR_WIDTH;

        scrollData->trackRect = bounds;
        scrollData->trackRect.top = scrollData->upArrowRect.bottom;
        scrollData->trackRect.bottom = scrollData->downArrowRect.top;

        scrollData->trackLen = scrollData->trackRect.bottom - scrollData->trackRect.top;
    } else {
        /* Horizontal scrollbar */
        scrollData->upArrowRect = bounds;
        scrollData->upArrowRect.right = scrollData->upArrowRect.left + SCROLLBAR_WIDTH;

        scrollData->downArrowRect = bounds;
        scrollData->downArrowRect.left = scrollData->downArrowRect.right - SCROLLBAR_WIDTH;

        scrollData->trackRect = bounds;
        scrollData->trackRect.left = scrollData->upArrowRect.right;
        scrollData->trackRect.right = scrollData->downArrowRect.left;

        scrollData->trackLen = scrollData->trackRect.right - scrollData->trackRect.left;
    }
}

/**
 * Update scrollbar thumb position and size
 */
static void UpdateScrollBarThumb(ControlHandle scrollBar) {
    ScrollBarData *scrollData;
    int32_t range, value;
    int16_t thumbLen, thumbPos;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    range = (*scrollBar)->contrlMax - (*scrollBar)->contrlMin;
    value = (*scrollBar)->contrlValue - (*scrollBar)->contrlMin;

    if (range <= 0) {
        /* No range - thumb fills track */
        thumbLen = scrollData->trackLen;
        thumbPos = 0;
    } else {
        /* Calculate proportional thumb size */
        thumbLen = (scrollData->pageSize * scrollData->trackLen) /
                   (range + scrollData->pageSize);

        /* Enforce minimum thumb size */
        if (thumbLen < MIN_THUMB_SIZE) {
            thumbLen = MIN_THUMB_SIZE;
        }
        if (thumbLen > scrollData->trackLen) {
            thumbLen = scrollData->trackLen;
        }

        /* Calculate thumb position */
        if (range > 0) {
            thumbPos = (value * (scrollData->trackLen - thumbLen)) / range;
        } else {
            thumbPos = 0;
        }
    }

    scrollData->thumbSize = thumbLen;
    scrollData->thumbPos = thumbPos;

    /* Calculate thumb rectangle */
    if (scrollData->isVertical) {
        scrollData->thumbRect = scrollData->trackRect;
        scrollData->thumbRect.top = scrollData->trackRect.top + thumbPos;
        scrollData->thumbRect.bottom = scrollData->thumbRect.top + thumbLen;

        /* Calculate page areas */
        scrollData->pageUpRect = scrollData->trackRect;
        scrollData->pageUpRect.bottom = scrollData->thumbRect.top;

        scrollData->pageDownRect = scrollData->trackRect;
        scrollData->pageDownRect.top = scrollData->thumbRect.bottom;
    } else {
        scrollData->thumbRect = scrollData->trackRect;
        scrollData->thumbRect.left = scrollData->trackRect.left + thumbPos;
        scrollData->thumbRect.right = scrollData->thumbRect.left + thumbLen;

        /* Calculate page areas */
        scrollData->pageUpRect = scrollData->trackRect;
        scrollData->pageUpRect.right = scrollData->thumbRect.left;

        scrollData->pageDownRect = scrollData->trackRect;
        scrollData->pageDownRect.left = scrollData->thumbRect.right;
    }
}

/**
 * Draw scrollbar arrow
 */
static void DrawScrollBarArrow(const Rect *arrowRect, int16_t direction, bool pushed) {
    Rect frameRect = *arrowRect;
    Point arrowPts[3];
    int16_t centerH, centerV;

    /* Draw arrow button frame */
    if (pushed) {
        /* Pushed state */
        PenPat(&black);
        FrameRect(&frameRect);
        InsetRect(&frameRect, 1, 1);
        PenPat(&dkGray);
        PaintRect(&frameRect);
    } else {
        /* Normal state */
        PenPat(&ltGray);
        PaintRect(&frameRect);
        PenPat(&white);
        MoveTo(frameRect.left, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.top);
        LineTo(frameRect.right - 1, frameRect.top);
        PenPat(&black);
        LineTo(frameRect.right - 1, frameRect.bottom - 1);
        LineTo(frameRect.left, frameRect.bottom - 1);
    }

    /* Calculate arrow center */
    centerH = (frameRect.left + frameRect.right) / 2;
    centerV = (frameRect.top + frameRect.bottom) / 2;

    /* Draw arrow triangle */
    PenPat(&black);
    switch (direction) {
    case 0: /* Up arrow */
        arrowPts[0].h = centerH;
        arrowPts[0].v = centerV - 3;
        arrowPts[1].h = centerH - 3;
        arrowPts[1].v = centerV + 3;
        arrowPts[2].h = centerH + 3;
        arrowPts[2].v = centerV + 3;
        break;

    case 1: /* Down arrow */
        arrowPts[0].h = centerH;
        arrowPts[0].v = centerV + 3;
        arrowPts[1].h = centerH - 3;
        arrowPts[1].v = centerV - 3;
        arrowPts[2].h = centerH + 3;
        arrowPts[2].v = centerV - 3;
        break;

    case 2: /* Right arrow */
        arrowPts[0].h = centerH + 3;
        arrowPts[0].v = centerV;
        arrowPts[1].h = centerH - 3;
        arrowPts[1].v = centerV - 3;
        arrowPts[2].h = centerH - 3;
        arrowPts[2].v = centerV + 3;
        break;

    case 3: /* Left arrow */
        arrowPts[0].h = centerH - 3;
        arrowPts[0].v = centerV;
        arrowPts[1].h = centerH + 3;
        arrowPts[1].v = centerV - 3;
        arrowPts[2].h = centerH + 3;
        arrowPts[2].v = centerV + 3;
        break;
    }

    /* Paint arrow triangle */
    OpenPoly();
    MoveTo(arrowPts[0].h, arrowPts[0].v);
    LineTo(arrowPts[1].h, arrowPts[1].v);
    LineTo(arrowPts[2].h, arrowPts[2].v);
    LineTo(arrowPts[0].h, arrowPts[0].v);
    PolyHandle arrowPoly = ClosePoly();

    if (arrowPoly) {
        PaintPoly(arrowPoly);
        KillPoly(arrowPoly);
    }
}

/**
 * Draw scrollbar thumb
 */
static void DrawScrollBarThumb(ControlHandle scrollBar) {
    ScrollBarData *scrollData;
    Rect thumbRect;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    thumbRect = scrollData->thumbRect;

    /* Don't draw thumb if too small */
    if ((thumbRect.right - thumbRect.left) <= 2 ||
        (thumbRect.bottom - thumbRect.top) <= 2) {
        return;
    }

    /* Draw thumb frame */
    if (scrollData->hiliteThumb) {
        /* Highlighted thumb */
        PenPat(&black);
        PaintRect(&thumbRect);
    } else {
        /* Normal thumb */
        PenPat(&ltGray);
        PaintRect(&thumbRect);

        /* Highlight edges */
        PenPat(&white);
        MoveTo(thumbRect.left, thumbRect.bottom - 1);
        LineTo(thumbRect.left, thumbRect.top);
        LineTo(thumbRect.right - 1, thumbRect.top);

        /* Shadow edges */
        PenPat(&dkGray);
        LineTo(thumbRect.right - 1, thumbRect.bottom - 1);
        LineTo(thumbRect.left, thumbRect.bottom - 1);
    }

    /* Draw thumb grip lines */
    if (!scrollData->hiliteThumb) {
        int16_t centerH = (thumbRect.left + thumbRect.right) / 2;
        int16_t centerV = (thumbRect.top + thumbRect.bottom) / 2;

        PenPat(&dkGray);
        if (scrollData->isVertical) {
            /* Horizontal grip lines */
            MoveTo(centerH - 3, centerV - 1);
            LineTo(centerH + 3, centerV - 1);
            MoveTo(centerH - 3, centerV + 1);
            LineTo(centerH + 3, centerV + 1);
        } else {
            /* Vertical grip lines */
            MoveTo(centerH - 1, centerV - 3);
            LineTo(centerH - 1, centerV + 3);
            MoveTo(centerH + 1, centerV - 3);
            LineTo(centerH + 1, centerV + 3);
        }
    }

    PenPat(&black);
}

/**
 * Draw scrollbar track
 */
static void DrawScrollBarTrack(ControlHandle scrollBar) {
    ScrollBarData *scrollData;
    Rect trackRect;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    trackRect = scrollData->trackRect;

    /* Fill track background */
    PenPat(&white);
    PaintRect(&trackRect);

    /* Draw track border */
    PenPat(&black);
    FrameRect(&trackRect);
}

/**
 * Calculate new thumb value from mouse position
 */
static int16_t CalcNewThumbValue(ControlHandle scrollBar, Point pt) {
    ScrollBarData *scrollData;
    int32_t range, newValue;
    int16_t mousePos, trackStart;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return 0;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    range = (*scrollBar)->contrlMax - (*scrollBar)->contrlMin;

    if (range <= 0) {
        return (*scrollBar)->contrlValue;
    }

    /* Calculate mouse position in track */
    if (scrollData->isVertical) {
        mousePos = pt.v - scrollData->trackingOffset.v;
        trackStart = scrollData->trackRect.top;
    } else {
        mousePos = pt.h - scrollData->trackingOffset.h;
        trackStart = scrollData->trackRect.left;
    }

    /* Convert to value */
    newValue = ((mousePos - trackStart) * range) /
               (scrollData->trackLen - scrollData->thumbSize);

    newValue += (*scrollBar)->contrlMin;

    /* Clamp to valid range */
    if (newValue < (*scrollBar)->contrlMin) {
        newValue = (*scrollBar)->contrlMin;
    }
    if (newValue > (*scrollBar)->contrlMax) {
        newValue = (*scrollBar)->contrlMax;
    }

    return (int16_t)newValue;
}

/**
 * Handle scrollbar tracking
 */
static void HandleScrollBarTracking(ControlHandle scrollBar, int16_t part, Point startPt) {
    ScrollBarData *scrollData;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;
    scrollData->trackingPart = part;

    if (part == inThumb) {
        /* Calculate mouse offset from thumb center */
        Rect thumbRect = scrollData->thumbRect;
        scrollData->trackingOffset.h = startPt.h - (thumbRect.left + thumbRect.right) / 2;
        scrollData->trackingOffset.v = startPt.v - (thumbRect.top + thumbRect.bottom) / 2;
    }
}

/**
 * Draw thumb outline during tracking
 */
static void DrawThumbOutline(ControlHandle scrollBar, const Rect *thumbRect) {
    if (!scrollBar || !thumbRect) {
        return;
    }

    /* Draw dotted outline */
    PenPat(&gray);
    PenMode(patXor);
    FrameRect(thumbRect);
    PenMode(patCopy);
    PenPat(&black);
}

/**
 * Handle automatic scrollbar tracking
 */
static void ScrollBarAutoTrack(ControlHandle scrollBar) {
    ScrollBarData *scrollData;
    int16_t newValue;

    if (!scrollBar || !(*scrollBar)->contrlData) {
        return;
    }

    scrollData = (ScrollBarData *)*(*scrollBar)->contrlData;

    switch (scrollData->trackingPart) {
    case inUpButton:
        newValue = (*scrollBar)->contrlValue - 1;
        break;

    case inDownButton:
        newValue = (*scrollBar)->contrlValue + 1;
        break;

    case inPageUp:
        newValue = (*scrollBar)->contrlValue - scrollData->pageSize;
        break;

    case inPageDown:
        newValue = (*scrollBar)->contrlValue + scrollData->pageSize;
        break;

    default:
        return;
    }

    /* Update value */
    SetControlValue(scrollBar, newValue);

    /* Call action procedure if set */
    if ((*scrollBar)->contrlAction) {
        (*(*scrollBar)->contrlAction)(scrollBar, scrollData->trackingPart);
    }
}

/**
 * Check if control is a scrollbar
 */
bool IsScrollBarControl(ControlHandle control) {
    if (!control) {
        return false;
    }

    int16_t procID = GetControlVariant(control) & 0xFFF0;
    return (procID == scrollBarProc);
}
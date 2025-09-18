/**
 * @file ControlTracking.c
 * @brief Control mouse tracking and user interaction implementation
 *
 * This file implements mouse tracking, hit testing, and user interaction
 * for all control types. It provides the foundation for responsive
 * control behavior and user feedback.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "ControlTracking.h"
#include "ControlManager.h"
#include "../EventManager/EventManager.h"
#include "../QuickDraw/QuickDraw.h"
#include "../MacTypes.h"
#include <string.h>

/* Tracking state */
typedef struct TrackingState {
    ControlHandle activeControl;
    int16_t activePart;
    Point startPoint;
    uint32_t trackStart;
    bool isTracking;
} TrackingState;

static TrackingState gTracking = {0};

/**
 * Test point in control
 */
int16_t TestControl(ControlHandle theControl, Point thePt) {
    if (!theControl) {
        return 0;
    }

    /* Check if control is visible and active */
    if (!(*theControl)->contrlVis || (*theControl)->contrlHilite == inactiveHilite) {
        return 0;
    }

    /* Check if point is in control bounds */
    if (!PtInRect(thePt, &(*theControl)->contrlRect)) {
        return 0;
    }

    /* Let CDEF determine part code */
    return (int16_t)_CallControlDefProc(theControl, testCntl, *(int32_t *)&thePt);
}

/**
 * Track control interaction
 */
int16_t TrackControl(ControlHandle theControl, Point thePoint,
                     ControlActionProcPtr actionProc) {
    int16_t partCode, currentPart;
    Point currentPt;
    uint32_t actionTime = 0;

    if (!theControl) {
        return 0;
    }

    /* Test initial hit */
    partCode = TestControl(theControl, thePoint);
    if (partCode == 0) {
        return 0;
    }

    /* Set up tracking state */
    gTracking.activeControl = theControl;
    gTracking.activePart = partCode;
    gTracking.startPoint = thePoint;
    gTracking.trackStart = TickCount();
    gTracking.isTracking = true;

    /* Highlight initial part */
    HiliteControl(theControl, partCode);

    /* Call action procedure if provided */
    if (actionProc) {
        (*actionProc)(theControl, partCode);
        actionTime = TickCount();
    }

    /* Track until mouse up */
    while (StillDown()) {
        GetMouse(&currentPt);
        currentPart = TestControl(theControl, currentPt);

        /* Update highlighting */
        if (currentPart != gTracking.activePart) {
            if (currentPart == partCode) {
                HiliteControl(theControl, partCode);
                gTracking.activePart = partCode;
            } else {
                HiliteControl(theControl, 0);
                gTracking.activePart = 0;
            }
        }

        /* Call action procedure periodically for auto-repeat */
        if (actionProc && gTracking.activePart != 0) {
            uint32_t now = TickCount();
            if (now - actionTime >= 6) { /* ~100ms repeat rate */
                (*actionProc)(theControl, gTracking.activePart);
                actionTime = now;
            }
        }

        /* Special handling for scroll bar thumb tracking */
        if (partCode == inThumb) {
            _CallControlDefProc(theControl, thumbCntl, *(int32_t *)&currentPt);
        }
    }

    /* Clear highlighting */
    HiliteControl(theControl, 0);

    /* Clear tracking state */
    partCode = gTracking.activePart;
    gTracking.activeControl = NULL;
    gTracking.activePart = 0;
    gTracking.isTracking = false;

    return partCode;
}

/**
 * Find control at point in window
 */
int16_t FindControl(Point thePoint, WindowPtr theWindow,
                    ControlHandle *theControl) {
    ControlHandle control;
    int16_t partCode;

    if (!theWindow || !theControl) {
        return 0;
    }

    *theControl = NULL;

    /* Search controls from front to back */
    control = _GetFirstControl(theWindow);
    while (control) {
        partCode = TestControl(control, thePoint);
        if (partCode != 0) {
            *theControl = control;
            return partCode;
        }
        control = (*control)->nextControl;
    }

    return 0;
}

/**
 * Drag a control
 */
void DragControl(ControlHandle theControl, Point startPt,
                 const Rect *limitRect, const Rect *slopRect, int16_t axis) {
    Point currentPt, lastPt;
    Rect bounds, limit;
    int16_t dh, dv;

    if (!theControl) {
        return;
    }

    /* Set up limit rectangle */
    if (limitRect) {
        limit = *limitRect;
    } else {
        /* Use window bounds */
        GetWindowBounds((*theControl)->contrlOwner, &limit);
    }

    /* Track mouse */
    lastPt = startPt;
    while (StillDown()) {
        GetMouse(&currentPt);

        /* Apply axis constraints */
        if (axis == hAxisOnly) {
            currentPt.v = startPt.v;
        } else if (axis == vAxisOnly) {
            currentPt.h = startPt.h;
        }

        /* Calculate offset */
        dh = currentPt.h - lastPt.h;
        dv = currentPt.v - lastPt.v;

        if (dh != 0 || dv != 0) {
            /* Check limits */
            bounds = (*theControl)->contrlRect;
            OffsetRect(&bounds, dh, dv);

            if (bounds.left >= limit.left && bounds.right <= limit.right &&
                bounds.top >= limit.top && bounds.bottom <= limit.bottom) {
                /* Move control */
                MoveControl(theControl, bounds.left, bounds.top);
                lastPt = currentPt;
            }
        }
    }
}

/**
 * Get current tracking control
 */
ControlHandle GetTrackingControl(void) {
    return gTracking.isTracking ? gTracking.activeControl : NULL;
}

/**
 * Get current tracking part
 */
int16_t GetTrackingPart(void) {
    return gTracking.isTracking ? gTracking.activePart : 0;
}

/**
 * Check if control is currently being tracked
 */
bool IsControlTracking(ControlHandle control) {
    return (gTracking.isTracking && gTracking.activeControl == control);
}
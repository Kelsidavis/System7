// #include "CompatibilityFix.h" // Removed
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

#include "SystemTypes.h"
/* ControlTracking.h local */
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlInternal.h"
#include "ControlManager/ControlTypes.h"
#include "EventManager/EventManager.h"
#include "QuickDraw/QuickDraw.h"
#include "DialogManager/DialogManager.h"
#include "SystemTypes.h"


/* Tracking state */
typedef struct TrackingState {
    ControlHandle activeControl;
    SInt16 activePart;
    Point startPoint;
    UInt32 trackStart;
    Boolean isTracking;
} TrackingState;

static TrackingState gTracking = {0};

/**
 * Test point in control
 */
SInt16 TestControl(ControlHandle theControl, Point thePt) {
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
    return (SInt16)_CallControlDefProc(theControl, testCntl, *(SInt32 *)&thePt);
}

/**
 * Track control interaction
 */
SInt16 TrackControl(ControlHandle theControl, Point thePoint,
                     ControlActionProcPtr actionProc) {
    SInt16 partCode, currentPart;
    Point currentPt;
    UInt32 actionTime = 0;

    if (!theControl) {
        return 0;
    }

    /* Route scrollbars to TrackScrollbar for delta-based tracking */
    if (IsScrollBarControl(theControl)) {
        SInt16 delta = 0;
        partCode = TestControl(theControl, thePoint);
        if (partCode == 0) {
            return 0;
        }
        TrackScrollbar(theControl, thePoint, partCode, 0, &delta);
        /* Call action proc if provided (for compatibility) */
        if (actionProc && delta != 0) {
            (*actionProc)(theControl, partCode);
        }
        return partCode;
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

    /* Call action procedure if provided (with debounce for mouse path) */
    if (actionProc && !DM_DebounceAction(2)) {
        (*actionProc)(theControl, partCode);
        actionTime = TickCount();
    }

    /* Track until mouse up - with safety timeout */
    int trackLoopCount = 0;
    const int MAX_TRACK_ITERATIONS = 5000;  /* Safety limit (~5 seconds at 1ms per iteration) */
    extern void serial_puts(const char* str);

    while (StillDown() && trackLoopCount < MAX_TRACK_ITERATIONS) {
        trackLoopCount++;
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
            UInt32 now = TickCount();
            if (now - actionTime >= 6) { /* ~100ms repeat rate */
                (*actionProc)(theControl, gTracking.activePart);
                actionTime = now;
            }
        }

        /* Special handling for scroll bar thumb tracking */
        if (partCode == inThumb) {
            _CallControlDefProc(theControl, thumbCntl, *(SInt32 *)&currentPt);
        }
    }

    /* Log if we hit the timeout */
    if (trackLoopCount >= MAX_TRACK_ITERATIONS) {
        serial_puts("[CTRL-TRACK] WARNING: TrackControl timeout - exceeded max iterations\n");
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
SInt16 FindControl(Point thePoint, WindowPtr theWindow,
                    ControlHandle *theControl) {
    ControlHandle control;
    SInt16 partCode;

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
                 const Rect *limitRect, const Rect *slopRect, SInt16 axis) {
    Point currentPt, lastPt;
    Rect bounds, limit;
    SInt16 dh, dv;

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

    /* Track mouse with infinite loop guard */
    const UInt32 MAX_DRAG_ITERATIONS = 100000;  /* Prevent infinite loop hang */
    UInt32 loopCount = 0;

    lastPt = startPt;
    while (StillDown() && loopCount < MAX_DRAG_ITERATIONS) {
        loopCount++;
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

    if (loopCount >= MAX_DRAG_ITERATIONS) {
        CTRL_LOG_WARN("DragControl: Hit iteration limit, possible UI hang prevented\n");
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
SInt16 GetTrackingPart(void) {
    return gTracking.isTracking ? gTracking.activePart : 0;
}

/**
 * Check if control is currently being tracked
 */
Boolean IsControlTracking(ControlHandle control) {
    return (gTracking.isTracking && gTracking.activeControl == control);
}

/**
 * @file ControlManagerCore.c
 * @brief Core Control Manager implementation for System 7.1 Portable
 *
 * This file implements the main Control Manager functionality for managing
 * controls in windows, handling control creation, disposal, display, and
 * interaction. This is the final essential component for complete Mac application
 * UI toolkit functionality.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#include "ControlManager.h"
#include "ControlTypes.h"
#include "ControlDrawing.h"
#include "ControlTracking.h"
#include "ControlResources.h"
#include "../QuickDraw/QuickDraw.h"
#include "../WindowManager/WindowManager.h"
#include "../EventManager/EventManager.h"
#include "../ResourceManager/ResourceManager.h"
#include "../MacTypes.h"
#include <string.h>
#include <stdlib.h>

/* Control Manager Globals */
typedef struct {
    ControlHandle trackingControl;     /* Control being tracked */
    int16_t trackingPart;              /* Part being tracked */
    ControlActionProcPtr trackingProc; /* Tracking action procedure */
    uint32_t lastActionTime;           /* Last action procedure call */
    uint32_t actionInterval;           /* Action procedure interval */
    AuxCtlHandle auxCtlList;           /* Auxiliary control records */
    bool initialized;                   /* Control Manager initialized */

    /* Control type registry */
    struct ControlTypeEntry {
        int16_t procID;
        ControlDefProcPtr defProc;
        struct ControlTypeEntry *next;
    } *controlTypes;

    /* Platform abstraction */
    struct {
        bool useNativeControls;         /* Use platform native controls */
        bool enableAccessibility;       /* Enable accessibility support */
        bool enableHighDPI;             /* Enable high-DPI rendering */
        bool enableTouch;               /* Enable touch input support */
        bool enableAnimation;           /* Enable control animations */
    } platformSettings;

} ControlManagerGlobals;

static ControlManagerGlobals gControlMgr = {0};

/* Internal utility functions */
static void LinkControl(WindowPtr window, ControlHandle control);
static void UnlinkControl(ControlHandle control);
static void CalcControlBounds(ControlHandle control);
static void NotifyControlChange(ControlHandle control, int16_t changeType);
static AuxCtlHandle NewAuxCtlRec(ControlHandle control);
static void DisposeAuxCtlRec(AuxCtlHandle auxRec);
static void InitializePlatformSettings(void);
static OSErr ValidateControlParameters(WindowPtr theWindow, const Rect *boundsRect,
                                      int16_t value, int16_t min, int16_t max);

/**
 * Initialize the Control Manager
 */
void _InitControlManager(void) {
    if (!gControlMgr.initialized) {
        memset(&gControlMgr, 0, sizeof(gControlMgr));

        /* Initialize platform settings */
        InitializePlatformSettings();

        /* Register standard control types */
        RegisterStandardControlTypes();

        /* Set default action interval (60 ticks = 1 second) */
        gControlMgr.actionInterval = 60;

        gControlMgr.initialized = true;
    }
}

/**
 * Cleanup the Control Manager
 */
void _CleanupControlManager(void) {
    if (gControlMgr.initialized) {
        /* Dispose all auxiliary control records */
        while (gControlMgr.auxCtlList) {
            AuxCtlHandle next = (AuxCtlHandle)(*gControlMgr.auxCtlList)->acNext;
            DisposeAuxCtlRec(gControlMgr.auxCtlList);
            gControlMgr.auxCtlList = next;
        }

        /* Cleanup control type registry */
        while (gControlMgr.controlTypes) {
            struct ControlTypeEntry *next = gControlMgr.controlTypes->next;
            free(gControlMgr.controlTypes);
            gControlMgr.controlTypes = next;
        }

        gControlMgr.initialized = false;
    }
}

/**
 * Create a new control
 */
ControlHandle NewControl(WindowPtr theWindow, const Rect *boundsRect,
                         ConstStr255Param title, Boolean visible,
                         int16_t value, int16_t min, int16_t max,
                         int16_t procID, int32_t refCon) {
    ControlHandle control;
    ControlPtr ctlPtr;
    OSErr err;

    /* Initialize Control Manager if needed */
    if (!gControlMgr.initialized) {
        _InitControlManager();
    }

    /* Validate parameters */
    err = ValidateControlParameters(theWindow, boundsRect, value, min, max);
    if (err != noErr) {
        return NULL;
    }

    /* Allocate control handle */
    control = (ControlHandle)NewHandle(sizeof(ControlRecord));
    if (!control) {
        return NULL;
    }

    /* Lock and initialize control record */
    HLock((Handle)control);
    ctlPtr = *control;
    memset(ctlPtr, 0, sizeof(ControlRecord));

    /* Set control fields */
    ctlPtr->contrlOwner = theWindow;
    ctlPtr->contrlRect = *boundsRect;
    ctlPtr->contrlVis = visible ? 1 : 0;
    ctlPtr->contrlHilite = noHilite;
    ctlPtr->contrlValue = value;
    ctlPtr->contrlMin = min;
    ctlPtr->contrlMax = max;
    ctlPtr->contrlRfCon = refCon;

    /* Copy title */
    if (title && title[0] > 0) {
        memcpy(ctlPtr->contrlTitle, title, title[0] + 1);
    } else {
        ctlPtr->contrlTitle[0] = 0;
    }

    /* Get control definition procedure */
    ctlPtr->contrlDefProc = _GetControlDefProc(procID);
    if (!ctlPtr->contrlDefProc) {
        HUnlock((Handle)control);
        DisposeHandle((Handle)control);
        return NULL;
    }

    /* Initialize control via CDEF */
    _CallControlDefProc(control, initCntl, 0);

    /* Link control to window */
    LinkControl(theWindow, control);

    HUnlock((Handle)control);

    /* Draw control if visible */
    if (visible) {
        Draw1Control(control);
    }

    return control;
}

/**
 * Get a control from a resource
 */
ControlHandle GetNewControl(int16_t controlID, WindowPtr owner) {
    Handle cntlRes;
    ControlHandle control = NULL;

    if (!owner) {
        return NULL;
    }

    /* Load CNTL resource */
    cntlRes = GetResource('CNTL', controlID);
    if (cntlRes) {
        control = LoadControlFromResource(cntlRes, owner);
        ReleaseResource(cntlRes);
    }

    return control;
}

/**
 * Dispose of a control
 */
void DisposeControl(ControlHandle theControl) {
    AuxCtlHandle auxRec;

    if (!theControl) {
        return;
    }

    /* Stop tracking if this is the tracked control */
    if (gControlMgr.trackingControl == theControl) {
        gControlMgr.trackingControl = NULL;
        gControlMgr.trackingPart = 0;
        gControlMgr.trackingProc = NULL;
    }

    /* Call disposal routine in CDEF */
    _CallControlDefProc(theControl, dispCntl, 0);

    /* Unlink from window */
    UnlinkControl(theControl);

    /* Dispose auxiliary record if any */
    if (GetAuxiliaryControlRecord(theControl, &auxRec)) {
        DisposeAuxCtlRec(auxRec);
    }

    /* Dispose control data if any */
    if ((*theControl)->contrlData) {
        DisposeHandle((*theControl)->contrlData);
    }

    /* Dispose control definition procedure handle */
    if ((*theControl)->contrlDefProc) {
        DisposeHandle((*theControl)->contrlDefProc);
    }

    /* Dispose control handle */
    DisposeHandle((Handle)theControl);
}

/**
 * Dispose all controls in a window
 */
void KillControls(WindowPtr theWindow) {
    ControlHandle control, next;

    if (!theWindow) {
        return;
    }

    /* Get first control */
    control = _GetFirstControl(theWindow);

    /* Dispose all controls */
    while (control) {
        next = (*control)->nextControl;
        DisposeControl(control);
        control = next;
    }

    /* Clear window's control list */
    _SetFirstControl(theWindow, NULL);
}

/**
 * Show a control
 */
void ShowControl(ControlHandle theControl) {
    if (!theControl || (*theControl)->contrlVis) {
        return;
    }

    (*theControl)->contrlVis = 1;
    Draw1Control(theControl);

    /* Notify of visibility change */
    NotifyControlChange(theControl, kControlVisibilityChanged);
}

/**
 * Hide a control
 */
void HideControl(ControlHandle theControl) {
    Rect bounds;

    if (!theControl || !(*theControl)->contrlVis) {
        return;
    }

    (*theControl)->contrlVis = 0;

    /* Invalidate control area */
    bounds = (*theControl)->contrlRect;
    InvalRect(&bounds);

    /* Notify of visibility change */
    NotifyControlChange(theControl, kControlVisibilityChanged);
}

/**
 * Draw all controls in a window
 */
void DrawControls(WindowPtr theWindow) {
    ControlHandle control;
    GrafPtr savePort;

    if (!theWindow) {
        return;
    }

    /* Save and set port */
    GetPort(&savePort);
    SetPort(theWindow);

    /* Draw each visible control */
    control = _GetFirstControl(theWindow);
    while (control) {
        if ((*control)->contrlVis) {
            Draw1Control(control);
        }
        control = (*control)->nextControl;
    }

    /* Restore port */
    SetPort(savePort);
}

/**
 * Draw a single control
 */
void Draw1Control(ControlHandle theControl) {
    GrafPtr savePort;

    if (!theControl || !(*theControl)->contrlVis) {
        return;
    }

    /* Save and set port */
    GetPort(&savePort);
    SetPort((*theControl)->contrlOwner);

    /* Draw control via CDEF */
    _CallControlDefProc(theControl, drawCntl, 0);

    /* Restore port */
    SetPort(savePort);
}

/**
 * Update controls in a region
 */
void UpdateControls(WindowPtr theWindow, RgnHandle updateRgn) {
    ControlHandle control;
    GrafPtr savePort;
    Rect controlRect;

    if (!theWindow) {
        return;
    }

    /* Save and set port */
    GetPort(&savePort);
    SetPort(theWindow);

    /* Update each visible control in the region */
    control = _GetFirstControl(theWindow);
    while (control) {
        if ((*control)->contrlVis) {
            controlRect = (*control)->contrlRect;
            if (RectInRgn(&controlRect, updateRgn)) {
                Draw1Control(control);
            }
        }
        control = (*control)->nextControl;
    }

    /* Restore port */
    SetPort(savePort);
}

/**
 * Highlight a control
 */
void HiliteControl(ControlHandle theControl, int16_t hiliteState) {
    if (!theControl) {
        return;
    }

    if ((*theControl)->contrlHilite != hiliteState) {
        (*theControl)->contrlHilite = hiliteState;
        if ((*theControl)->contrlVis) {
            Draw1Control(theControl);
        }

        /* Notify of highlight change */
        NotifyControlChange(theControl, kControlHighlightChanged);
    }
}

/**
 * Move a control
 */
void MoveControl(ControlHandle theControl, int16_t h, int16_t v) {
    Rect oldRect, newRect;
    int16_t dh, dv;

    if (!theControl) {
        return;
    }

    /* Calculate offset */
    oldRect = (*theControl)->contrlRect;
    dh = h - oldRect.left;
    dv = v - oldRect.top;

    if (dh == 0 && dv == 0) {
        return;
    }

    /* Invalidate old position */
    if ((*theControl)->contrlVis) {
        InvalRect(&oldRect);
    }

    /* Update control rectangle */
    OffsetRect(&(*theControl)->contrlRect, dh, dv);

    /* Notify CDEF of position change */
    _CallControlDefProc(theControl, posCntl, 0);

    /* Invalidate new position */
    if ((*theControl)->contrlVis) {
        newRect = (*theControl)->contrlRect;
        InvalRect(&newRect);
    }

    /* Notify of position change */
    NotifyControlChange(theControl, kControlPositionChanged);
}

/**
 * Resize a control
 */
void SizeControl(ControlHandle theControl, int16_t w, int16_t h) {
    Rect oldRect, newRect;

    if (!theControl || w <= 0 || h <= 0) {
        return;
    }

    oldRect = (*theControl)->contrlRect;

    /* Update control size */
    (*theControl)->contrlRect.right = (*theControl)->contrlRect.left + w;
    (*theControl)->contrlRect.bottom = (*theControl)->contrlRect.top + h;

    newRect = (*theControl)->contrlRect;

    /* Notify CDEF of size change */
    _CallControlDefProc(theControl, calcCRgns, 0);

    /* Invalidate affected area */
    if ((*theControl)->contrlVis) {
        UnionRect(&oldRect, &newRect, &oldRect);
        InvalRect(&oldRect);
    }

    /* Notify of size change */
    NotifyControlChange(theControl, kControlSizeChanged);
}

/**
 * Set control value
 */
void SetControlValue(ControlHandle theControl, int16_t theValue) {
    if (!theControl) {
        return;
    }

    /* Clamp to min/max */
    if (theValue < (*theControl)->contrlMin) {
        theValue = (*theControl)->contrlMin;
    } else if (theValue > (*theControl)->contrlMax) {
        theValue = (*theControl)->contrlMax;
    }

    if ((*theControl)->contrlValue != theValue) {
        (*theControl)->contrlValue = theValue;

        /* Notify CDEF of value change */
        _CallControlDefProc(theControl, posCntl, 0);

        /* Redraw if visible */
        if ((*theControl)->contrlVis) {
            Draw1Control(theControl);
        }

        /* Notify of value change */
        NotifyControlChange(theControl, kControlValueChanged);
    }
}

/**
 * Get control value
 */
int16_t GetControlValue(ControlHandle theControl) {
    if (!theControl) {
        return 0;
    }
    return (*theControl)->contrlValue;
}

/**
 * Set control minimum
 */
void SetControlMinimum(ControlHandle theControl, int16_t minValue) {
    if (!theControl) {
        return;
    }

    (*theControl)->contrlMin = minValue;

    /* Adjust value if necessary */
    if ((*theControl)->contrlValue < minValue) {
        SetControlValue(theControl, minValue);
    }

    /* Notify of range change */
    NotifyControlChange(theControl, kControlRangeChanged);
}

/**
 * Get control minimum
 */
int16_t GetControlMinimum(ControlHandle theControl) {
    if (!theControl) {
        return 0;
    }
    return (*theControl)->contrlMin;
}

/**
 * Set control maximum
 */
void SetControlMaximum(ControlHandle theControl, int16_t maxValue) {
    if (!theControl) {
        return;
    }

    (*theControl)->contrlMax = maxValue;

    /* Adjust value if necessary */
    if ((*theControl)->contrlValue > maxValue) {
        SetControlValue(theControl, maxValue);
    }

    /* Notify of range change */
    NotifyControlChange(theControl, kControlRangeChanged);
}

/**
 * Get control maximum
 */
int16_t GetControlMaximum(ControlHandle theControl) {
    if (!theControl) {
        return 0;
    }
    return (*theControl)->contrlMax;
}

/**
 * Set control title
 */
void SetControlTitle(ControlHandle theControl, ConstStr255Param title) {
    if (!theControl) {
        return;
    }

    /* Copy new title */
    if (title && title[0] > 0) {
        memcpy((*theControl)->contrlTitle, title, title[0] + 1);
    } else {
        (*theControl)->contrlTitle[0] = 0;
    }

    /* Redraw if visible */
    if ((*theControl)->contrlVis) {
        Draw1Control(theControl);
    }

    /* Notify of title change */
    NotifyControlChange(theControl, kControlTitleChanged);
}

/**
 * Get control title
 */
void GetControlTitle(ControlHandle theControl, Str255 title) {
    if (!theControl || !title) {
        return;
    }

    memcpy(title, (*theControl)->contrlTitle, (*theControl)->contrlTitle[0] + 1);
}

/**
 * Set control reference
 */
void SetControlReference(ControlHandle theControl, int32_t data) {
    if (!theControl) {
        return;
    }
    (*theControl)->contrlRfCon = data;
}

/**
 * Get control reference
 */
int32_t GetControlReference(ControlHandle theControl) {
    if (!theControl) {
        return 0;
    }
    return (*theControl)->contrlRfCon;
}

/**
 * Set control action procedure
 */
void SetControlAction(ControlHandle theControl, ControlActionProcPtr actionProc) {
    if (!theControl) {
        return;
    }
    (*theControl)->contrlAction = actionProc;
}

/**
 * Get control action procedure
 */
ControlActionProcPtr GetControlAction(ControlHandle theControl) {
    if (!theControl) {
        return NULL;
    }
    return (*theControl)->contrlAction;
}

/**
 * Get control variant
 */
int16_t GetControlVariant(ControlHandle theControl) {
    if (!theControl || !(*theControl)->contrlDefProc) {
        return 0;
    }

    /* Extract variant from CDEF handle */
    /* High byte of first word contains variant */
    return (*(int16_t *)*(*theControl)->contrlDefProc) >> 8;
}

/**
 * Get auxiliary control record
 */
Boolean GetAuxiliaryControlRecord(ControlHandle theControl, AuxCtlHandle *acHndl) {
    AuxCtlHandle auxRec;

    if (!theControl || !acHndl) {
        return false;
    }

    *acHndl = NULL;

    /* Search auxiliary list */
    auxRec = gControlMgr.auxCtlList;
    while (auxRec) {
        if ((*auxRec)->acOwner == theControl) {
            *acHndl = auxRec;
            return true;
        }
        auxRec = (AuxCtlHandle)(*auxRec)->acNext;
    }

    return false;
}

/**
 * Set control color table
 */
void SetControlColor(ControlHandle theControl, CCTabHandle newColorTable) {
    AuxCtlHandle auxRec;

    if (!theControl) {
        return;
    }

    /* Get or create auxiliary record */
    if (!GetAuxiliaryControlRecord(theControl, &auxRec)) {
        auxRec = NewAuxCtlRec(theControl);
        if (!auxRec) {
            return;
        }
    }

    /* Replace color table */
    if ((*auxRec)->acCTable) {
        DisposeHandle((Handle)(*auxRec)->acCTable);
    }
    (*auxRec)->acCTable = newColorTable;

    /* Redraw control */
    if ((*theControl)->contrlVis) {
        Draw1Control(theControl);
    }
}

/* Internal Functions */

/**
 * Initialize platform settings
 */
static void InitializePlatformSettings(void) {
    /* Set default platform settings */
    gControlMgr.platformSettings.useNativeControls = false;
    gControlMgr.platformSettings.enableAccessibility = true;
    gControlMgr.platformSettings.enableHighDPI = true;
    gControlMgr.platformSettings.enableTouch = false;
    gControlMgr.platformSettings.enableAnimation = false;

    /* Platform-specific initialization could go here */
}

/**
 * Validate control parameters
 */
static OSErr ValidateControlParameters(WindowPtr theWindow, const Rect *boundsRect,
                                      int16_t value, int16_t min, int16_t max) {
    if (!theWindow) {
        return paramErr;
    }

    if (!boundsRect) {
        return paramErr;
    }

    if (boundsRect->left >= boundsRect->right ||
        boundsRect->top >= boundsRect->bottom) {
        return paramErr;
    }

    if (min > max) {
        return paramErr;
    }

    return noErr;
}

/**
 * Link control to window's control list
 */
static void LinkControl(WindowPtr window, ControlHandle control) {
    ControlHandle firstControl;

    if (!window || !control) {
        return;
    }

    /* Get current first control */
    firstControl = _GetFirstControl(window);

    /* Link new control at head of list */
    (*control)->nextControl = firstControl;
    _SetFirstControl(window, control);
}

/**
 * Unlink control from window's control list
 */
static void UnlinkControl(ControlHandle control) {
    WindowPtr window;
    ControlHandle current, prev;

    if (!control) {
        return;
    }

    window = (*control)->contrlOwner;
    if (!window) {
        return;
    }

    /* Find and unlink control */
    prev = NULL;
    current = _GetFirstControl(window);

    while (current) {
        if (current == control) {
            if (prev) {
                (*prev)->nextControl = (*current)->nextControl;
            } else {
                _SetFirstControl(window, (*current)->nextControl);
            }
            (*control)->nextControl = NULL;
            break;
        }
        prev = current;
        current = (*current)->nextControl;
    }
}

/**
 * Notify of control change
 */
static void NotifyControlChange(ControlHandle control, int16_t changeType) {
    /* Platform-specific notification could go here */
    /* For accessibility, native control updates, etc. */
}

/**
 * Create new auxiliary control record
 */
static AuxCtlHandle NewAuxCtlRec(ControlHandle control) {
    AuxCtlHandle auxRec;

    if (!control) {
        return NULL;
    }

    /* Allocate auxiliary record */
    auxRec = (AuxCtlHandle)NewHandleClear(sizeof(AuxCtlRec));
    if (!auxRec) {
        return NULL;
    }

    /* Initialize */
    (*auxRec)->acOwner = control;
    (*auxRec)->acNext = (Handle)gControlMgr.auxCtlList;
    gControlMgr.auxCtlList = auxRec;

    return auxRec;
}

/**
 * Dispose auxiliary control record
 */
static void DisposeAuxCtlRec(AuxCtlHandle auxRec) {
    AuxCtlHandle current, prev;

    if (!auxRec) {
        return;
    }

    /* Unlink from list */
    prev = NULL;
    current = gControlMgr.auxCtlList;

    while (current) {
        if (current == auxRec) {
            if (prev) {
                (*prev)->acNext = (*current)->acNext;
            } else {
                gControlMgr.auxCtlList = (AuxCtlHandle)(*current)->acNext;
            }
            break;
        }
        prev = current;
        current = (AuxCtlHandle)(*current)->acNext;
    }

    /* Dispose color table if any */
    if ((*auxRec)->acCTable) {
        DisposeHandle((Handle)(*auxRec)->acCTable);
    }

    /* Dispose record */
    DisposeHandle((Handle)auxRec);
}

/**
 * Call control definition procedure
 */
int16_t _CallControlDefProc(ControlHandle control, int16_t message, int32_t param) {
    ControlDefProcPtr defProc;
    int16_t variant;

    if (!control || !(*control)->contrlDefProc) {
        return 0;
    }

    /* Get CDEF procedure and variant */
    defProc = (ControlDefProcPtr)*(*control)->contrlDefProc;
    variant = GetControlVariant(control);

    /* Call CDEF */
    if (defProc) {
        return (int16_t)(*defProc)(variant, control, message, param);
    }

    return 0;
}

/**
 * Get control definition procedure for procID
 */
Handle _GetControlDefProc(int16_t procID) {
    ControlDefProcPtr defProc;
    Handle cdefHandle;
    struct ControlTypeEntry *entry;

    /* Search registered control types */
    entry = gControlMgr.controlTypes;
    while (entry) {
        if ((entry->procID & 0xFFF0) == (procID & 0xFFF0)) {
            defProc = entry->defProc;
            break;
        }
        entry = entry->next;
    }

    if (!entry) {
        return NULL;
    }

    /* Create handle for CDEF */
    cdefHandle = NewHandle(sizeof(ControlDefProcPtr) + 2);
    if (!cdefHandle) {
        return NULL;
    }

    /* Store variant in high byte of first word */
    *(int16_t *)*cdefHandle = (procID & 0x0F) << 8;
    /* Store procedure pointer */
    *(ControlDefProcPtr *)((char *)*cdefHandle + 2) = defProc;

    return cdefHandle;
}

/**
 * Register a control type
 */
void RegisterControlType(int16_t procID, ControlDefProcPtr defProc) {
    struct ControlTypeEntry *entry;

    /* Check if already registered */
    entry = gControlMgr.controlTypes;
    while (entry) {
        if (entry->procID == procID) {
            entry->defProc = defProc;
            return;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = (struct ControlTypeEntry *)malloc(sizeof(struct ControlTypeEntry));
    if (entry) {
        entry->procID = procID;
        entry->defProc = defProc;
        entry->next = gControlMgr.controlTypes;
        gControlMgr.controlTypes = entry;
    }
}

/**
 * Get control definition procedure by procID
 */
ControlDefProcPtr GetControlDefProc(int16_t procID) {
    struct ControlTypeEntry *entry = gControlMgr.controlTypes;

    while (entry) {
        if ((entry->procID & 0xFFF0) == (procID & 0xFFF0)) {
            return entry->defProc;
        }
        entry = entry->next;
    }

    return NULL;
}
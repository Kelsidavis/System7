/* #include "SystemTypes.h" */
#include <stdlib.h>
#include <string.h>
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

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
/* ControlDrawing.h not needed */
/* ControlTracking.h local */
/* ControlResources.h local */
#include "QuickDraw/QuickDraw.h"

/* External function declarations */
extern ControlHandle LoadControlFromResource(Handle cntlRes, WindowPtr owner);
#include "WindowManager/WindowManager.h"
#include "EventManager/EventManager.h"
#include "DialogManager/DialogManager.h"
#include "ResourceMgr/resource_types.h"
#include "ResourceMgr/ResourceMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "SystemTypes.h"
#include "System71StdLib.h"

/* Logging helpers */
#define CTRL_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleControl, kLogLevelDebug, "[CTRL] " fmt, ##__VA_ARGS__)
#define CTRL_LOG_WARN(fmt, ...)  serial_logf(kLogModuleControl, kLogLevelWarn,  "[CTRL] " fmt, ##__VA_ARGS__)
#define CTRL_LOG_ERROR(fmt, ...) serial_logf(kLogModuleControl, kLogLevelError, "[CTRL] " fmt, ##__VA_ARGS__)


/* Control Manager Globals */
typedef struct {
    ControlHandle trackingControl;     /* Control being tracked */
    SInt16 trackingPart;              /* Part being tracked */
    ControlActionProcPtr trackingProc; /* Tracking action procedure */
    UInt32 lastActionTime;           /* Last action procedure call */
    UInt32 actionInterval;           /* Action procedure interval */
    AuxCtlHandle auxCtlList;           /* Auxiliary control records */
    Boolean initialized;                   /* Control Manager initialized */

    /* Control type registry */
    struct ControlTypeEntry {
        SInt16 procID;
        ControlDefProcPtr defProc;
        struct ControlTypeEntry *next;
    } *controlTypes;

    /* Platform abstraction */
    struct {
        Boolean useNativeControls;         /* Use platform native controls */
        Boolean enableAccessibility;       /* Enable accessibility support */
        Boolean enableHighDPI;             /* Enable high-DPI rendering */
        Boolean enableTouch;               /* Enable touch input support */
        Boolean enableAnimation;           /* Enable control animations */
    } platformSettings;

} ControlManagerGlobals;

static ControlManagerGlobals gControlMgr = {0};

/* Internal utility functions */
static void LinkControl(WindowPtr window, ControlHandle control);
static void UnlinkControl(ControlHandle control);
static void NotifyControlChange(ControlHandle control, SInt16 changeType);
static AuxCtlHandle NewAuxCtlRec(ControlHandle control);
static void DisposeAuxCtlRec(AuxCtlHandle auxRec);
static void InitializePlatformSettings(void);
static OSErr ValidateControlParameters(WindowPtr theWindow, const Rect *boundsRect,
                                      SInt16 value, SInt16 min, SInt16 max);

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
        RegisterScrollBarControlType();

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
            DisposePtr((Ptr)gControlMgr.controlTypes);
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
                         SInt16 value, SInt16 min, SInt16 max,
                         SInt16 procID, SInt32 refCon) {
    ControlHandle control;
    ControlPtr ctlPtr;
    OSErr err;

    CTRL_LOG_DEBUG("NewControl ENTRY: procID=%d\n", procID);

    /* Initialize Control Manager if needed */
    if (!gControlMgr.initialized) {
        CTRL_LOG_DEBUG("Calling _InitControlManager\n");
        _InitControlManager();
    }
    CTRL_LOG_DEBUG("After init check\n");

    /* Validate parameters */
    err = ValidateControlParameters(theWindow, boundsRect, value, min, max);
    if (err != noErr) {
        CTRL_LOG_WARN("ValidateControlParameters FAILED\n");
        return NULL;
    }
    CTRL_LOG_DEBUG("Parameters validated\n");

    /* Allocate control handle */
    control = (ControlHandle)NewHandle(sizeof(ControlRecord));
    if (!control) {
        CTRL_LOG_ERROR("NewHandle FAILED\n");
        return NULL;
    }
    CTRL_LOG_DEBUG("Handle allocated\n");

    /* Lock and initialize control record */
    HLock((Handle)control);
    ctlPtr = *control;
    memset(ctlPtr, 0, sizeof(ControlRecord));
    CTRL_LOG_DEBUG("Control record initialized\n");

    /* Set control fields */
    ctlPtr->contrlOwner = theWindow;
    ctlPtr->contrlRect = *boundsRect;
    ctlPtr->contrlVis = visible ? 1 : 0;
    ctlPtr->contrlHilite = noHilite;
    ctlPtr->contrlValue = value;
    ctlPtr->contrlMin = min;
    ctlPtr->contrlMax = max;
    ctlPtr->contrlRfCon = refCon;

    /* Copy title - unsigned char already limited to 255 max */
    if (title && title[0] > 0) {
        unsigned char len = title[0];
        ctlPtr->contrlTitle[0] = len;
        memcpy(&ctlPtr->contrlTitle[1], &title[1], len);
    } else {
        ctlPtr->contrlTitle[0] = 0;
    }

    /* Get control definition procedure */
    CTRL_LOG_DEBUG("Getting CDEF for procID=%d\n", procID);
    ctlPtr->contrlDefProc = _GetControlDefProc(procID);
    if (!ctlPtr->contrlDefProc) {
        CTRL_LOG_ERROR("CDEF not found!\n");
        HUnlock((Handle)control);
        DisposeHandle((Handle)control);
        return NULL;
    }
    CTRL_LOG_DEBUG("CDEF found\n");

    /* Initialize control via CDEF */
    CTRL_LOG_DEBUG("Calling CDEF initCntl\n");
    _CallControlDefProc(control, initCntl, 0);
    CTRL_LOG_DEBUG("CDEF initCntl complete\n");

    /* Link control to window */
    CTRL_LOG_DEBUG("Linking control to window\n");
    LinkControl(theWindow, control);

    HUnlock((Handle)control);
    CTRL_LOG_DEBUG("Control unlocked\n");

    /* Draw control if visible */
    if (visible) {
        CTRL_LOG_DEBUG("Drawing control\n");
        Draw1Control(control);
    }

    CTRL_LOG_DEBUG("NewControl EXIT: control=0x%08x\n", (unsigned int)P2UL(control));
    return control;
}

/**
 * Get a control from a resource
 */
ControlHandle GetNewControl(SInt16 controlID, WindowPtr owner) {
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

    /* Clear keyboard focus if this control has it (erase focus ring, prevent dangling pointer) */
    DM_OnDisposeControl(theControl);

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
        (*theControl)->contrlData = NULL;  /* Prevent double-free */
    }

    /* Dispose control definition procedure handle */
    if ((*theControl)->contrlDefProc) {
        DisposeHandle((*theControl)->contrlDefProc);
        (*theControl)->contrlDefProc = NULL;  /* Prevent double-free */
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
    SetPort((GrafPtr)theWindow);

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
    ControlHandle focusedControl;

    if (!theControl || !(*theControl)->contrlVis) {
        return;
    }

    /* Save and set port */
    GetPort(&savePort);
    SetPort((GrafPtr)(*theControl)->contrlOwner);

    /* Draw control via CDEF */
    _CallControlDefProc(theControl, drawCntl, 0);

    /* Restore focus ring if this control has focus (XOR reapply after redraw) */
    focusedControl = DM_GetKeyboardFocus((*theControl)->contrlOwner);
    if (focusedControl == theControl) {
        ToggleFocusRing(theControl); /* Reapply after control drew itself */
    }

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
    SetPort((GrafPtr)theWindow);

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
void HiliteControl(ControlHandle theControl, SInt16 hiliteState) {
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
void MoveControl(ControlHandle theControl, SInt16 h, SInt16 v) {
    Rect oldRect, newRect;
    SInt16 dh, dv;

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
void SizeControl(ControlHandle theControl, SInt16 w, SInt16 h) {
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
void SetControlValue(ControlHandle theControl, SInt16 theValue) {
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
SInt16 GetControlValue(ControlHandle theControl) {
    if (!theControl) {
        return 0;
    }
    return (*theControl)->contrlValue;
}

/**
 * Set control minimum
 */
void SetControlMinimum(ControlHandle theControl, SInt16 minValue) {
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
SInt16 GetControlMinimum(ControlHandle theControl) {
    if (!theControl) {
        return 0;
    }
    return (*theControl)->contrlMin;
}

/**
 * Set control maximum
 */
void SetControlMaximum(ControlHandle theControl, SInt16 maxValue) {
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
SInt16 GetControlMaximum(ControlHandle theControl) {
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

    /* Copy new title - unsigned char already limited to 255 max */
    if (title && title[0] > 0) {
        unsigned char len = title[0];
        (*theControl)->contrlTitle[0] = len;
        memcpy(&(*theControl)->contrlTitle[1], &title[1], len);
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

    /* Copy title - unsigned char already limited to 255 max */
    unsigned char len = (*theControl)->contrlTitle[0];
    title[0] = len;
    if (len > 0) {
        memcpy(&title[1], &(*theControl)->contrlTitle[1], len);
    }
}

/**
 * Set control reference
 */
void SetControlReference(ControlHandle theControl, SInt32 data) {
    if (!theControl) {
        return;
    }
    (*theControl)->contrlRfCon = data;
}

/**
 * Get control reference
 */
SInt32 GetControlReference(ControlHandle theControl) {
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
SInt16 GetControlVariant(ControlHandle theControl) {
    if (!theControl || !(*theControl)->contrlDefProc) {
        return 0;
    }

    /* Extract variant from CDEF handle */
    /* High byte of first word contains variant */
    return (*(SInt16 *)*(*theControl)->contrlDefProc) >> 8;
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
    gControlMgr.platformSettings.enableAnimation = true;  /* Enable for visual feedback */

    /* Platform-specific initialization could go here */
}

/**
 * Validate control parameters
 */
static OSErr ValidateControlParameters(WindowPtr theWindow, const Rect *boundsRect,
                                      SInt16 value, SInt16 min, SInt16 max) {
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
static void NotifyControlChange(ControlHandle control, SInt16 changeType) {
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
SInt16 _CallControlDefProc(ControlHandle control, SInt16 message, SInt32 param) {
    ControlDefProcPtr defProc;
    SInt16 variant;
    SInt16 result;

    if (!control) {
        return 0;
    }

    /* CRITICAL: Lock control before dereferencing to prevent heap compaction issues
     * The CDEF may allocate memory, so we must lock the control handle before calling it */
    HLock((Handle)control);

    if (!(*control)->contrlDefProc) {
        HUnlock((Handle)control);
        return 0;
    }

    /* Get CDEF procedure and variant */
    /* Variant is stored in first 2 bytes, function pointer at offset +2 */
    variant = *(SInt16 *)*(*control)->contrlDefProc >> 8;
    defProc = *(ControlDefProcPtr *)((char *)*(*control)->contrlDefProc + 2);

    /* Call CDEF - it can now safely dereference the locked control handle */
    if (defProc) {
        result = (SInt16)(*defProc)(variant, control, message, param);
        HUnlock((Handle)control);
        return result;
    }

    HUnlock((Handle)control);
    return 0;
}

/**
 * Get control definition procedure for procID
 */
Handle _GetControlDefProc(SInt16 procID) {
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

    /* Lock handle before dereferencing */
    HLock(cdefHandle);

    /* Store variant in high byte of first word */
    *(SInt16 *)*cdefHandle = (procID & 0x0F) << 8;
    /* Store procedure pointer */
    *(ControlDefProcPtr *)((char *)*cdefHandle + 2) = defProc;

    HUnlock(cdefHandle);
    return cdefHandle;
}

/**
 * Register a control type
 */
void RegisterControlType(SInt16 procID, ControlDefProcPtr defProc) {
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
    entry = (struct ControlTypeEntry *)NewPtr(sizeof(struct ControlTypeEntry));
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
ControlDefProcPtr GetControlDefProc(SInt16 procID) {
    struct ControlTypeEntry *entry = gControlMgr.controlTypes;

    while (entry) {
        if ((entry->procID & 0xFFF0) == (procID & 0xFFF0)) {
            return entry->defProc;
        }
        entry = entry->next;
    }

    return NULL;
}

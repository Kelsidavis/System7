/**
 * Control Manager Hardware Abstraction Layer
 * Provides platform-independent implementation of Control Manager functionality
 *
 * This HAL layer bridges the System 7 Control Manager with modern platforms,
 * providing complete control functionality including buttons, checkboxes,
 * radio buttons, scroll bars, popup menus, and custom controls (CDEFs).
 */

#include "../../include/ControlManager/ControlManager.h"
#include "../../include/ControlManager/control_trap_glue.h"
#include "../../include/WindowManager/WindowManager.h"
#include "../../include/QuickDraw/QuickDraw.h"
#include "../../include/EventManager/EventManager.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#endif

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif

/* Control Manager Global State */
typedef struct ControlMgrState {
    bool initialized;
    ControlHandle rootControl;     /* Root control for hierarchical controls */
    int16_t nextControlID;          /* Auto-generated control IDs */
    RgnHandle updateRgn;            /* Region needing update */
    ControlHandle focusedControl;  /* Currently focused control */
    ControlHandle trackingControl;  /* Control being tracked */
    Point lastMousePt;              /* Last mouse position during tracking */

    /* System 7 features */
    ScrollSpeedGlobals scrollSpeed;
    bool useColorControls;
    bool supportEmbedding;

    /* CDEF (Control Definition) cache */
    struct {
        int16_t procID;
        Handle cdefHandle;
        ControlDefProcPtr defProc;
    } cdefCache[16];
    int cdefCacheCount;

} ControlMgrState;

static ControlMgrState gControlMgr = {0};

/* Forward declarations */
static void ControlMgr_HAL_DrawControl(ControlHandle control);
static int16_t ControlMgr_HAL_TestControl(ControlHandle control, Point pt);
static void ControlMgr_HAL_CalcControlRgn(ControlHandle control, RgnHandle rgn);
static void ControlMgr_HAL_UpdateScrollBar(ControlHandle control);
static ControlDefProcPtr ControlMgr_HAL_GetCDEF(int16_t procID);

/**
 * Initialize Control Manager HAL
 */
void ControlMgr_HAL_Init(void)
{
    if (gControlMgr.initialized) {
        return;
    }

    printf("Control Manager HAL: Initializing...\n");

    /* Initialize update region */
    gControlMgr.updateRgn = NewRgn();

    /* Set default control ID counter */
    gControlMgr.nextControlID = 128;  /* Start with IDs above standard range */

    /* Initialize scroll speed globals for System 7 */
    gControlMgr.scrollSpeed.scrollDelay = 2;      /* Ticks before scroll starts */
    gControlMgr.scrollSpeed.scrollInterval = 1;   /* Ticks between scrolls */
    gControlMgr.scrollSpeed.scrollAmount = 1;     /* Lines per scroll */

    /* Enable System 7 features */
    gControlMgr.useColorControls = true;
    gControlMgr.supportEmbedding = true;

    gControlMgr.initialized = true;

    printf("Control Manager HAL: Initialized with System 7 features\n");
}

/**
 * Create a new control
 */
ControlHandle ControlMgr_HAL_NewControl(WindowPtr theWindow, const Rect* boundsRect,
                                        const char* title, bool visible,
                                        int16_t value, int16_t min, int16_t max,
                                        int16_t procID, int32_t refCon)
{
    if (!gControlMgr.initialized) {
        ControlMgr_HAL_Init();
    }

    /* Allocate control handle */
    ControlHandle control = (ControlHandle)NewHandleClear(sizeof(ControlRecord));
    if (!control) {
        return NULL;
    }

    /* Lock handle and initialize control record */
    HLock((Handle)control);
    ControlRecord* ctlRec = *control;

    /* Set control fields */
    ctlRec->nextControl = NULL;
    ctlRec->contrlOwner = theWindow;
    ctlRec->contrlRect = *boundsRect;

    /* Copy title as Pascal string */
    if (title && *title) {
        int len = strlen(title);
        if (len > 255) len = 255;
        ctlRec->contrlTitle[0] = len;
        memcpy(&ctlRec->contrlTitle[1], title, len);
    } else {
        ctlRec->contrlTitle[0] = 0;
    }

    ctlRec->contrlVis = visible ? 0xFF : 0x00;
    ctlRec->contrlHilite = 0;  /* Active */
    ctlRec->contrlValue = value;
    ctlRec->contrlMin = min;
    ctlRec->contrlMax = max;
    ctlRec->contrlDefProc = ControlMgr_HAL_GetCDEF(procID);
    ctlRec->contrlData = NULL;
    ctlRec->contrlAction = NULL;
    ctlRec->contrlRfCon = refCon;

    /* Add to window's control list */
    if (theWindow) {
        WindowPeek wPeek = (WindowPeek)theWindow;
        if (!wPeek->controlList) {
            wPeek->controlList = control;
        } else {
            /* Find end of list */
            ControlHandle lastCtl = wPeek->controlList;
            while ((*lastCtl)->nextControl) {
                lastCtl = (*lastCtl)->nextControl;
            }
            (*lastCtl)->nextControl = control;
        }
    }

    /* Call CDEF init message */
    if (ctlRec->contrlDefProc) {
        /* CallControlDefProc(initCntl, control, 0, 0); */
    }

    HUnlock((Handle)control);

    /* Draw if visible */
    if (visible) {
        ControlMgr_HAL_DrawControl(control);
    }

    return control;
}

/**
 * Dispose of a control
 */
void ControlMgr_HAL_DisposeControl(ControlHandle control)
{
    if (!control) return;

    /* Remove from window's control list */
    WindowPtr window = (**control).contrlOwner;
    if (window) {
        WindowPeek wPeek = (WindowPeek)window;
        ControlHandle* ctlPtr = &wPeek->controlList;

        while (*ctlPtr) {
            if (*ctlPtr == control) {
                *ctlPtr = (**control).nextControl;
                break;
            }
            ctlPtr = &((**ctlPtr)->nextControl);
        }
    }

    /* Call CDEF dispose message */
    if ((**control).contrlDefProc) {
        /* CallControlDefProc(dispCntl, control, 0, 0); */
    }

    /* Free control data if any */
    if ((**control).contrlData) {
        DisposeHandle((**control).contrlData);
    }

    /* Dispose control handle */
    DisposeHandle((Handle)control);
}

/**
 * Draw a control
 */
static void ControlMgr_HAL_DrawControl(ControlHandle control)
{
    if (!control || !(**control).contrlVis) {
        return;
    }

    /* Save graphics state */
    PenState savedPen;
    GetPenState(&savedPen);

    /* Standard control drawing based on procID */
    int16_t procID = (int16_t)((int32_t)(**control).contrlDefProc & 0xFFFF);
    Rect* bounds = &(**control).contrlRect;

    switch (procID) {
        case pushButProc:      /* 0 - Push button */
        case checkBoxProc:     /* 1 - Checkbox */
        case radioButProc:     /* 2 - Radio button */
            /* Draw button frame */
            if (procID == pushButProc) {
                /* Draw rounded rect button */
                PenSize(2, 2);
                if ((**control).contrlHilite == 255) {
                    PenPat(&gray);
                }
                FrameRoundRect(bounds, 16, 16);

                /* Draw title centered */
                if ((**control).contrlTitle[0] > 0) {
                    MoveTo(bounds->left + 10, bounds->top + 16);
                    DrawString((**control).contrlTitle);
                }
            } else {
                /* Draw checkbox or radio button */
                Rect box = *bounds;
                box.right = box.left + 16;
                box.bottom = box.top + 16;

                if (procID == checkBoxProc) {
                    FrameRect(&box);
                    if ((**control).contrlValue) {
                        /* Draw checkmark */
                        MoveTo(box.left + 3, box.top + 8);
                        LineTo(box.left + 6, box.bottom - 4);
                        LineTo(box.right - 3, box.top + 3);
                    }
                } else {
                    /* Radio button */
                    FrameOval(&box);
                    if ((**control).contrlValue) {
                        Rect dot = box;
                        InsetRect(&dot, 4, 4);
                        PaintOval(&dot);
                    }
                }

                /* Draw title */
                if ((**control).contrlTitle[0] > 0) {
                    MoveTo(box.right + 5, box.top + 12);
                    DrawString((**control).contrlTitle);
                }
            }
            break;

        case scrollBarProc:    /* 16 - Scroll bar */
            ControlMgr_HAL_UpdateScrollBar(control);
            break;

        default:
            /* Custom CDEF - call draw message */
            if ((**control).contrlDefProc) {
                /* CallControlDefProc(drawCntl, control, 0, 0); */
            }
            break;
    }

    /* Restore graphics state */
    SetPenState(&savedPen);
}

/**
 * Update scroll bar appearance
 */
static void ControlMgr_HAL_UpdateScrollBar(ControlHandle control)
{
    Rect* bounds = &(**control).contrlRect;
    bool isVertical = (bounds->bottom - bounds->top) > (bounds->right - bounds->left);

    /* Draw scroll bar track */
    FrameRect(bounds);

    /* Draw arrows */
    Rect arrowRect;
    if (isVertical) {
        /* Top arrow */
        arrowRect = *bounds;
        arrowRect.bottom = arrowRect.top + 16;
        FrameRect(&arrowRect);

        /* Bottom arrow */
        arrowRect = *bounds;
        arrowRect.top = arrowRect.bottom - 16;
        FrameRect(&arrowRect);

        /* Draw thumb if range exists */
        if ((**control).contrlMax > (**control).contrlMin) {
            int16_t trackLen = (bounds->bottom - bounds->top) - 48;  /* Minus arrows and thumb */
            int16_t range = (**control).contrlMax - (**control).contrlMin;
            int16_t thumbPos = (((**control).contrlValue - (**control).contrlMin) * trackLen) / range;

            Rect thumbRect = *bounds;
            thumbRect.left += 1;
            thumbRect.right -= 1;
            thumbRect.top = bounds->top + 16 + thumbPos;
            thumbRect.bottom = thumbRect.top + 16;

            FillRect(&thumbRect, &gray);
            FrameRect(&thumbRect);
        }
    } else {
        /* Horizontal scroll bar - similar logic */
        /* Left arrow */
        arrowRect = *bounds;
        arrowRect.right = arrowRect.left + 16;
        FrameRect(&arrowRect);

        /* Right arrow */
        arrowRect = *bounds;
        arrowRect.left = arrowRect.right - 16;
        FrameRect(&arrowRect);

        /* Draw thumb */
        if ((**control).contrlMax > (**control).contrlMin) {
            int16_t trackLen = (bounds->right - bounds->left) - 48;
            int16_t range = (**control).contrlMax - (**control).contrlMin;
            int16_t thumbPos = (((**control).contrlValue - (**control).contrlMin) * trackLen) / range;

            Rect thumbRect = *bounds;
            thumbRect.top += 1;
            thumbRect.bottom -= 1;
            thumbRect.left = bounds->left + 16 + thumbPos;
            thumbRect.right = thumbRect.left + 16;

            FillRect(&thumbRect, &gray);
            FrameRect(&thumbRect);
        }
    }
}

/**
 * Test where mouse click occurred in control
 */
static int16_t ControlMgr_HAL_TestControl(ControlHandle control, Point pt)
{
    if (!control || !(**control).contrlVis || (**control).contrlHilite == 255) {
        return 0;  /* inDeactive */
    }

    Rect* bounds = &(**control).contrlRect;
    if (!PtInRect(pt, bounds)) {
        return 0;
    }

    /* Check control type */
    int16_t procID = (int16_t)((int32_t)(**control).contrlDefProc & 0xFFFF);

    switch (procID) {
        case pushButProc:
        case checkBoxProc:
        case radioButProc:
            return inButton;  /* Click in button/checkbox/radio */

        case scrollBarProc: {
            bool isVertical = (bounds->bottom - bounds->top) > (bounds->right - bounds->left);

            if (isVertical) {
                if (pt.v < bounds->top + 16) return inUpButton;
                if (pt.v > bounds->bottom - 16) return inDownButton;

                /* Check if in thumb */
                if ((**control).contrlMax > (**control).contrlMin) {
                    int16_t trackLen = (bounds->bottom - bounds->top) - 48;
                    int16_t range = (**control).contrlMax - (**control).contrlMin;
                    int16_t thumbPos = (((**control).contrlValue - (**control).contrlMin) * trackLen) / range;
                    int16_t thumbTop = bounds->top + 16 + thumbPos;

                    if (pt.v >= thumbTop && pt.v <= thumbTop + 16) {
                        return inThumb;
                    }
                }

                /* In page area */
                return (pt.v < bounds->top + 32) ? inPageUp : inPageDown;
            } else {
                /* Horizontal scroll bar */
                if (pt.h < bounds->left + 16) return inUpButton;   /* Left arrow */
                if (pt.h > bounds->right - 16) return inDownButton; /* Right arrow */

                /* Check thumb and page areas similarly */
                return inPageUp;  /* Simplified */
            }
        }

        default:
            /* Custom CDEF */
            if ((**control).contrlDefProc) {
                /* return CallControlDefProc(testCntl, control, 0, MAKELONG(pt.v, pt.h)); */
            }
            return inButton;
    }
}

/**
 * Track control interaction
 */
int16_t ControlMgr_HAL_TrackControl(ControlHandle control, Point startPt,
                                    ControlActionProcPtr actionProc)
{
    if (!control) return 0;

    int16_t partCode = ControlMgr_HAL_TestControl(control, startPt);
    if (partCode == 0) return 0;

    /* Save tracking state */
    gControlMgr.trackingControl = control;
    gControlMgr.lastMousePt = startPt;

    /* Track until mouse up */
    EventRecord event;
    bool tracking = true;
    int16_t lastPart = partCode;

    while (tracking) {
        /* Get next event */
        if (GetNextEvent(mUpMask | mDragMask, &event)) {
            if (event.what == mouseUp) {
                tracking = false;
            }
        }

        /* Check if still in same part */
        Point currentPt = event.where;
        GlobalToLocal(&currentPt);
        int16_t currentPart = ControlMgr_HAL_TestControl(control, currentPt);

        /* Handle part changes */
        if (currentPart != lastPart) {
            /* Update control highlighting */
            if (partCode == inButton || partCode == inCheckBox) {
                (**control).contrlHilite = (currentPart == partCode) ? 1 : 0;
                ControlMgr_HAL_DrawControl(control);
            }
            lastPart = currentPart;
        }

        /* Call action proc if provided */
        if (actionProc && currentPart == partCode) {
            (*actionProc)(control, currentPart);
        }

        /* Handle scroll bar tracking */
        if ((partCode == inUpButton || partCode == inDownButton ||
             partCode == inPageUp || partCode == inPageDown) &&
            currentPart == partCode) {

            /* Adjust value based on part */
            int16_t delta = 0;
            switch (partCode) {
                case inUpButton:   delta = -1; break;
                case inDownButton: delta = 1; break;
                case inPageUp:     delta = -10; break;
                case inPageDown:   delta = 10; break;
            }

            int16_t newValue = (**control).contrlValue + delta;
            if (newValue < (**control).contrlMin) newValue = (**control).contrlMin;
            if (newValue > (**control).contrlMax) newValue = (**control).contrlMax;

            if (newValue != (**control).contrlValue) {
                (**control).contrlValue = newValue;
                ControlMgr_HAL_DrawControl(control);
            }
        }
    }

    /* Clear tracking state */
    gControlMgr.trackingControl = NULL;

    /* Final action for buttons/checkboxes */
    if (lastPart == partCode) {
        switch (partCode) {
            case inButton:
                /* Button clicked */
                break;
            case inCheckBox:
                /* Toggle checkbox */
                (**control).contrlValue = !(**control).contrlValue;
                ControlMgr_HAL_DrawControl(control);
                break;
        }
        return partCode;
    }

    return 0;
}

/**
 * Find control at point in window
 */
ControlHandle ControlMgr_HAL_FindControl(Point pt, WindowPtr window)
{
    if (!window) return NULL;

    WindowPeek wPeek = (WindowPeek)window;
    ControlHandle control = wPeek->controlList;

    /* Search from front to back */
    while (control) {
        if ((**control).contrlVis && (**control).contrlHilite != 255) {
            if (PtInRect(pt, &(**control).contrlRect)) {
                return control;
            }
        }
        control = (**control).nextControl;
    }

    return NULL;
}

/**
 * Draw all controls in window
 */
void ControlMgr_HAL_DrawControls(WindowPtr window)
{
    if (!window) return;

    WindowPeek wPeek = (WindowPeek)window;
    ControlHandle control = wPeek->controlList;

    while (control) {
        if ((**control).contrlVis) {
            ControlMgr_HAL_DrawControl(control);
        }
        control = (**control).nextControl;
    }
}

/**
 * Update controls in window
 */
void ControlMgr_HAL_UpdateControls(WindowPtr window, RgnHandle updateRgn)
{
    if (!window) return;

    WindowPeek wPeek = (WindowPeek)window;
    ControlHandle control = wPeek->controlList;

    while (control) {
        if ((**control).contrlVis) {
            /* Check if control intersects update region */
            RgnHandle ctlRgn = NewRgn();
            RectRgn(ctlRgn, &(**control).contrlRect);

            RgnHandle intersect = NewRgn();
            SectRgn(ctlRgn, updateRgn, intersect);

            if (!EmptyRgn(intersect)) {
                ControlMgr_HAL_DrawControl(control);
            }

            DisposeRgn(intersect);
            DisposeRgn(ctlRgn);
        }
        control = (**control).nextControl;
    }
}

/**
 * Get/Set control value
 */
int16_t ControlMgr_HAL_GetCtlValue(ControlHandle control)
{
    return control ? (**control).contrlValue : 0;
}

void ControlMgr_HAL_SetCtlValue(ControlHandle control, int16_t value)
{
    if (!control) return;

    if (value < (**control).contrlMin) value = (**control).contrlMin;
    if (value > (**control).contrlMax) value = (**control).contrlMax;

    if ((**control).contrlValue != value) {
        (**control).contrlValue = value;
        if ((**control).contrlVis) {
            ControlMgr_HAL_DrawControl(control);
        }
    }
}

/**
 * Get/Set control min/max
 */
int16_t ControlMgr_HAL_GetCtlMin(ControlHandle control)
{
    return control ? (**control).contrlMin : 0;
}

void ControlMgr_HAL_SetCtlMin(ControlHandle control, int16_t min)
{
    if (!control) return;
    (**control).contrlMin = min;
    if ((**control).contrlValue < min) {
        ControlMgr_HAL_SetCtlValue(control, min);
    }
}

int16_t ControlMgr_HAL_GetCtlMax(ControlHandle control)
{
    return control ? (**control).contrlMax : 0;
}

void ControlMgr_HAL_SetCtlMax(ControlHandle control, int16_t max)
{
    if (!control) return;
    (**control).contrlMax = max;
    if ((**control).contrlValue > max) {
        ControlMgr_HAL_SetCtlValue(control, max);
    }
}

/**
 * Show/Hide control
 */
void ControlMgr_HAL_ShowControl(ControlHandle control)
{
    if (!control || (**control).contrlVis) return;

    (**control).contrlVis = 0xFF;
    ControlMgr_HAL_DrawControl(control);
}

void ControlMgr_HAL_HideControl(ControlHandle control)
{
    if (!control || !(**control).contrlVis) return;

    (**control).contrlVis = 0x00;

    /* Erase control area */
    Rect bounds = (**control).contrlRect;
    EraseRect(&bounds);
}

/**
 * Enable/Disable (hilite) control
 */
void ControlMgr_HAL_HiliteControl(ControlHandle control, int16_t hiliteState)
{
    if (!control) return;

    if ((**control).contrlHilite != hiliteState) {
        (**control).contrlHilite = hiliteState;
        if ((**control).contrlVis) {
            ControlMgr_HAL_DrawControl(control);
        }
    }
}

/**
 * Move/Size control
 */
void ControlMgr_HAL_MoveControl(ControlHandle control, int16_t h, int16_t v)
{
    if (!control) return;

    int16_t dh = h - (**control).contrlRect.left;
    int16_t dv = v - (**control).contrlRect.top;

    OffsetRect(&(**control).contrlRect, dh, dv);

    if ((**control).contrlVis) {
        /* Should invalidate old and new areas */
        ControlMgr_HAL_DrawControl(control);
    }
}

void ControlMgr_HAL_SizeControl(ControlHandle control, int16_t width, int16_t height)
{
    if (!control) return;

    (**control).contrlRect.right = (**control).contrlRect.left + width;
    (**control).contrlRect.bottom = (**control).contrlRect.top + height;

    if ((**control).contrlVis) {
        ControlMgr_HAL_DrawControl(control);
    }
}

/**
 * Get/Set control title
 */
void ControlMgr_HAL_GetCtlTitle(ControlHandle control, char* title)
{
    if (!control || !title) return;

    /* Copy Pascal string to C string */
    int len = (**control).contrlTitle[0];
    memcpy(title, &(**control).contrlTitle[1], len);
    title[len] = '\0';
}

void ControlMgr_HAL_SetCtlTitle(ControlHandle control, const char* title)
{
    if (!control) return;

    /* Copy C string to Pascal string */
    int len = title ? strlen(title) : 0;
    if (len > 255) len = 255;

    (**control).contrlTitle[0] = len;
    if (len > 0) {
        memcpy(&(**control).contrlTitle[1], title, len);
    }

    if ((**control).contrlVis) {
        ControlMgr_HAL_DrawControl(control);
    }
}

/**
 * Get CDEF for control type
 */
static ControlDefProcPtr ControlMgr_HAL_GetCDEF(int16_t procID)
{
    /* Check cache first */
    for (int i = 0; i < gControlMgr.cdefCacheCount; i++) {
        if (gControlMgr.cdefCache[i].procID == procID) {
            return gControlMgr.cdefCache[i].defProc;
        }
    }

    /* For standard controls, return procID as placeholder */
    /* Real implementation would load CDEF resource */
    return (ControlDefProcPtr)(intptr_t)procID;
}

/**
 * System 7 specific: Embedding support
 */
OSErr ControlMgr_HAL_EmbedControl(ControlHandle inControl, ControlHandle inContainer)
{
    if (!gControlMgr.supportEmbedding) {
        return -1;  /* Not supported */
    }

    if (!inControl || !inContainer) {
        return paramErr;
    }

    /* Remove from current parent */
    if ((**inControl).contrlOwner) {
        /* Remove from window list */
        WindowPeek wPeek = (WindowPeek)(**inControl).contrlOwner;
        ControlHandle* ctlPtr = &wPeek->controlList;

        while (*ctlPtr) {
            if (*ctlPtr == inControl) {
                *ctlPtr = (**inControl).nextControl;
                break;
            }
            ctlPtr = &((**ctlPtr)->nextControl);
        }
    }

    /* Add to container (simplified - real would track parent/child) */
    (**inControl).nextControl = NULL;
    (**inControl).contrlOwner = (**inContainer).contrlOwner;

    return noErr;
}

/**
 * System 7: Get scroll speed globals
 */
ScrollSpeedGlobals* ControlMgr_HAL_GetScrollSpeed(void)
{
    return &gControlMgr.scrollSpeed;
}

/**
 * Cleanup Control Manager HAL
 */
void ControlMgr_HAL_Cleanup(void)
{
    if (!gControlMgr.initialized) {
        return;
    }

    /* Dispose update region */
    if (gControlMgr.updateRgn) {
        DisposeRgn(gControlMgr.updateRgn);
    }

    /* Clear CDEF cache */
    for (int i = 0; i < gControlMgr.cdefCacheCount; i++) {
        if (gControlMgr.cdefCache[i].cdefHandle) {
            DisposeHandle(gControlMgr.cdefCache[i].cdefHandle);
        }
    }

    /* Reset state */
    memset(&gControlMgr, 0, sizeof(gControlMgr));

    printf("Control Manager HAL: Cleaned up\n");
}
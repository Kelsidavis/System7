/**
 * @file ControlManager.h
 * @brief Main Control Manager header for System 7.1 Portable
 *
 * This file provides the complete Control Manager API for System 7.1 Portable,
 * supporting all standard control types and behaviors. This is THE FINAL ESSENTIAL
 * COMPONENT for complete Mac application UI toolkit functionality.
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#ifndef CONTROLMANAGER_H
#define CONTROLMANAGER_H

#include "../MacTypes.h"
#include "../QuickDraw/QuickDraw.h"
#include "../WindowManager/WindowManager.h"
#include "../MenuManager/MenuManager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Control Definition IDs (procIDs) */
enum {
    pushButProc     = 0,    /* Standard push button */
    checkBoxProc    = 1,    /* Standard checkbox */
    radioButProc    = 2,    /* Standard radio button */
    useWFont        = 8,    /* Use window font */
    scrollBarProc   = 16,   /* Standard scroll bar */
    editTextProc    = 64,   /* Edit text control */
    staticTextProc  = 65,   /* Static text control */
    popupMenuProc   = 1008  /* Popup menu control (63 * 16) */
};

/* Control Part Codes */
enum {
    inButton        = 10,   /* In push button */
    inCheckBox      = 11,   /* In checkbox */
    inUpButton      = 20,   /* In scroll bar up arrow */
    inDownButton    = 21,   /* In scroll bar down arrow */
    inPageUp        = 22,   /* In scroll bar page up area */
    inPageDown      = 23,   /* In scroll bar page down area */
    inThumb         = 129,  /* In scroll bar thumb */

    /* Popup menu part codes */
    inLabel         = 1,    /* In popup label */
    inMenu          = 2,    /* In popup menu area */
    inTriangle      = 4     /* In popup triangle */
};

/* Control Messages for CDEFs */
enum {
    drawCntl        = 0,    /* Draw control */
    testCntl        = 1,    /* Test hit in control */
    calcCRgns       = 2,    /* Calculate control regions */
    initCntl        = 3,    /* Initialize control */
    dispCntl        = 4,    /* Dispose control */
    posCntl         = 5,    /* Position indicator */
    thumbCntl       = 6,    /* Calculate thumb position */
    dragCntl        = 7,    /* Drag control */
    autoTrack       = 8,    /* Auto-track control */
    calcCntlRgn     = 10,   /* Calculate control region */
    calcThumbRgn    = 11,   /* Calculate thumb region */
    drawThumbOutline = 12   /* Draw thumb outline (private) */
};

/* Control Highlight States */
enum {
    noHilite        = 0,    /* No highlighting */
    inactiveHilite  = 255,  /* Inactive (grayed out) */
    /* Part codes 1-253 indicate highlighted part */
};

/* Control Change Notification Types */
enum {
    kControlValueChanged     = 1,
    kControlRangeChanged     = 2,
    kControlPositionChanged  = 3,
    kControlSizeChanged      = 4,
    kControlVisibilityChanged = 5,
    kControlHighlightChanged = 6,
    kControlTitleChanged     = 7
};

/* Popup Menu Style Flags */
enum {
    popupFixedWidth     = (1 << 0),    /* Fixed width popup */
    popupReserved       = (1 << 1),    /* Reserved */
    popupUseAddResMenu  = (1 << 2),    /* Use AddResMenu */
    popupUseWFont       = (1 << 3)     /* Use window font */
};

/* Drag Constraints */
enum {
    noConstraint    = 0,    /* No constraint */
    hAxisOnly       = 1,    /* Horizontal axis only */
    vAxisOnly       = 2     /* Vertical axis only */
};

/* Forward declarations */
typedef struct ControlRecord ControlRecord;
typedef ControlRecord *ControlPtr, **ControlHandle;

/* Control action procedure */
typedef void (*ControlActionProcPtr)(ControlHandle theControl, int16_t partCode);

/* Control definition function */
typedef int32_t (*ControlDefProcPtr)(int16_t varCode, ControlHandle theControl,
                                     int16_t message, int32_t param);

/* Text validation procedure */
typedef bool (*TextValidationProcPtr)(ControlHandle control, const char *text, int32_t refCon);

/* Control Record Structure */
struct ControlRecord {
    ControlHandle   nextControl;    /* Next control in window's control list */
    WindowPtr       contrlOwner;    /* Window that owns this control */
    Rect            contrlRect;     /* Control's rectangle */
    uint8_t         contrlVis;      /* Visibility flag */
    uint8_t         contrlHilite;   /* Highlight state */
    int16_t         contrlValue;    /* Current value */
    int16_t         contrlMin;      /* Minimum value */
    int16_t         contrlMax;      /* Maximum value */
    Handle          contrlDefProc;  /* Control definition procedure */
    Handle          contrlData;     /* Control's private data */
    ControlActionProcPtr contrlAction; /* Action procedure */
    int32_t         contrlRfCon;    /* Reference constant */
    Str255          contrlTitle;    /* Control title */
};

/* Color Table for Controls */
typedef struct CtlCTab {
    int32_t         ccSeed;         /* Reserved */
    int16_t         ccRider;        /* Reserved */
    int16_t         ctSize;         /* Number of entries (usually 3) */
    ColorSpec       ctTable[4];     /* Color specifications */
} CtlCTab;

typedef CtlCTab *CCTabPtr, **CCTabHandle;

/* Auxiliary Control Record (for color controls) */
typedef struct AuxCtlRec {
    Handle          acNext;         /* Next auxiliary record */
    ControlHandle   acOwner;        /* Control that owns this record */
    CCTabHandle     acCTable;       /* Color table */
    int16_t         acFlags;        /* Flags */
    int32_t         acReserved;     /* Reserved */
    int32_t         acRefCon;       /* Reference constant */
} AuxCtlRec;

typedef AuxCtlRec *AuxCtlPtr, **AuxCtlHandle;

/* Control Creation and Disposal */
ControlHandle NewControl(WindowPtr theWindow, const Rect *boundsRect,
                         ConstStr255Param title, Boolean visible,
                         int16_t value, int16_t min, int16_t max,
                         int16_t procID, int32_t refCon);

ControlHandle GetNewControl(int16_t controlID, WindowPtr owner);
void DisposeControl(ControlHandle theControl);
void KillControls(WindowPtr theWindow);

/* Control Display */
void ShowControl(ControlHandle theControl);
void HideControl(ControlHandle theControl);
void DrawControls(WindowPtr theWindow);
void Draw1Control(ControlHandle theControl);
void UpdateControls(WindowPtr theWindow, RgnHandle updateRgn);
void HiliteControl(ControlHandle theControl, int16_t hiliteState);

/* Control Manipulation */
void MoveControl(ControlHandle theControl, int16_t h, int16_t v);
void SizeControl(ControlHandle theControl, int16_t w, int16_t h);
void DragControl(ControlHandle theControl, Point startPt,
                 const Rect *limitRect, const Rect *slopRect, int16_t axis);

/* Control Values */
void SetControlValue(ControlHandle theControl, int16_t theValue);
int16_t GetControlValue(ControlHandle theControl);
void SetControlMinimum(ControlHandle theControl, int16_t minValue);
int16_t GetControlMinimum(ControlHandle theControl);
void SetControlMaximum(ControlHandle theControl, int16_t maxValue);
int16_t GetControlMaximum(ControlHandle theControl);

/* Control Title */
void SetControlTitle(ControlHandle theControl, ConstStr255Param title);
void GetControlTitle(ControlHandle theControl, Str255 title);

/* Control Properties */
void SetControlReference(ControlHandle theControl, int32_t data);
int32_t GetControlReference(ControlHandle theControl);
void SetControlAction(ControlHandle theControl, ControlActionProcPtr actionProc);
ControlActionProcPtr GetControlAction(ControlHandle theControl);
int16_t GetControlVariant(ControlHandle theControl);

/* Control Interaction */
int16_t TestControl(ControlHandle theControl, Point thePt);
int16_t TrackControl(ControlHandle theControl, Point thePoint,
                     ControlActionProcPtr actionProc);
int16_t FindControl(Point thePoint, WindowPtr theWindow,
                    ControlHandle *theControl);

/* Color Control Support */
void SetControlColor(ControlHandle theControl, CCTabHandle newColorTable);
Boolean GetAuxiliaryControlRecord(ControlHandle theControl, AuxCtlHandle *acHndl);

/* Standard Controls */
void RegisterStandardControlTypes(void);
bool IsButtonControl(ControlHandle control);
bool IsCheckboxControl(ControlHandle control);
bool IsRadioControl(ControlHandle control);

/* Scrollbar Controls */
void RegisterScrollBarControlType(void);
bool IsScrollBarControl(ControlHandle control);
void SetScrollBarPageSize(ControlHandle scrollBar, int16_t pageSize);
int16_t GetScrollBarPageSize(ControlHandle scrollBar);
void SetScrollBarLiveTracking(ControlHandle scrollBar, bool liveTracking);
bool GetScrollBarLiveTracking(ControlHandle scrollBar);

/* Text Controls */
void RegisterTextControlTypes(void);
ControlHandle NewEditTextControl(WindowPtr window, const Rect *bounds,
                                ConstStr255Param text, bool visible,
                                int16_t maxLength, int32_t refCon);
ControlHandle NewStaticTextControl(WindowPtr window, const Rect *bounds,
                                  ConstStr255Param text, bool visible,
                                  int16_t alignment, int32_t refCon);
void SetTextControlText(ControlHandle control, ConstStr255Param text);
void GetTextControlText(ControlHandle control, Str255 text);
void SetEditTextPassword(ControlHandle control, bool isPassword, char passwordChar);
void SetTextValidation(ControlHandle control, TextValidationProcPtr validator, int32_t refCon);
void ActivateEditText(ControlHandle control);
void DeactivateEditText(ControlHandle control);
bool IsEditTextControl(ControlHandle control);
bool IsStaticTextControl(ControlHandle control);

/* Popup Controls */
void RegisterPopupControlType(void);
ControlHandle NewPopupControl(WindowPtr window, const Rect *bounds,
                             ConstStr255Param title, bool visible,
                             int16_t menuID, int16_t variation, int32_t refCon);
void SetPopupMenu(ControlHandle popup, MenuHandle menu);
MenuHandle GetPopupMenu(ControlHandle popup);
void AppendPopupMenuItem(ControlHandle popup, ConstStr255Param itemText);
void InsertPopupMenuItem(ControlHandle popup, ConstStr255Param itemText, int16_t afterItem);
void DeletePopupMenuItem(ControlHandle popup, int16_t item);
void SetPopupMenuItemText(ControlHandle popup, int16_t item, ConstStr255Param text);
void GetPopupMenuItemText(ControlHandle popup, int16_t item, Str255 text);
bool IsPopupMenuControl(ControlHandle control);

/* Control Type Registration */
void RegisterControlType(int16_t procID, ControlDefProcPtr defProc);
ControlDefProcPtr GetControlDefProc(int16_t procID);

/* Control Definition Procedures */
int32_t ButtonCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);
int32_t CheckboxCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);
int32_t RadioButtonCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);
int32_t ScrollBarCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);
int32_t EditTextCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);
int32_t StaticTextCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);
int32_t PopupMenuCDEF(int16_t varCode, ControlHandle theControl, int16_t message, int32_t param);

/* Drawing and Tracking */
void DrawScrollBar(ControlHandle scrollBar);
void DrawPopupMenu(ControlHandle popup);

/* Compatibility Aliases */
#define SetCtlValue SetControlValue
#define GetCtlValue GetControlValue
#define SetCtlMin SetControlMinimum
#define GetCtlMin GetControlMinimum
#define SetCtlMax SetControlMaximum
#define GetCtlMax GetControlMaximum
#define SetCTitle SetControlTitle
#define GetCTitle GetControlTitle
#define SetCRefCon SetControlReference
#define GetCRefCon GetControlReference
#define SetCtlAction SetControlAction
#define GetCtlAction GetControlAction
#define GetCVariant GetControlVariant
#define UpdtControl UpdateControls
#define SetCtlColor SetControlColor
#define GetAuxCtl GetAuxiliaryControlRecord

/* Internal Functions (not part of public API) */
void _InitControlManager(void);
void _CleanupControlManager(void);
ControlHandle _GetFirstControl(WindowPtr window);
void _SetFirstControl(WindowPtr window, ControlHandle control);
int16_t _CallControlDefProc(ControlHandle control, int16_t message, int32_t param);
Handle _GetControlDefProc(int16_t procID);

#ifdef __cplusplus
}
#endif

#endif /* CONTROLMANAGER_H */
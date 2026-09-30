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

#include "SystemTypes.h"
#include "MacTypes.h"

/* Forward declarations */

#ifdef __cplusplus
extern "C" {
#endif

/* Control Definition IDs (procIDs) */

/* Control Part Codes */

/* Control Messages for CDEFs */

/* Control Highlight States */

/* Control Change Notification Types */

/* Popup Menu Style Flags */

/* Drag Constraints */

/* Forward declarations */
/* Handle is defined in MacTypes.h */

/* Control action procedure */

/* Control definition function */

/* Text validation procedure */

/* ControlRecord is defined in MacTypes.h */

/* Color Table for Controls */

/* Handle is defined in MacTypes.h */

/* AuxCtlRec is defined in MacTypes.h */

/* Handle is defined in MacTypes.h */

/* Control Creation and Disposal */
ControlHandle NewControl(WindowPtr theWindow, const Rect *boundsRect,
                         ConstStr255Param title, Boolean visible,
                         SInt16 value, SInt16 min, SInt16 max,
                         SInt16 procID, SInt32 refCon);

ControlHandle GetNewControl(SInt16 controlID, WindowPtr owner);
void DisposeControl(ControlHandle theControl);
void KillControls(WindowPtr theWindow);

/* Control Display */
void ShowControl(ControlHandle theControl);
void HideControl(ControlHandle theControl);
void DrawControls(WindowPtr theWindow);
void Draw1Control(ControlHandle theControl);
void UpdateControls(WindowPtr theWindow, RgnHandle updateRgn);
void HiliteControl(ControlHandle theControl, SInt16 hiliteState);

/* Control Manipulation */
void MoveControl(ControlHandle theControl, SInt16 h, SInt16 v);
void SizeControl(ControlHandle theControl, SInt16 w, SInt16 h);
void DragControl(ControlHandle theControl, Point startPt,
                 const Rect *limitRect, const Rect *slopRect, SInt16 axis);

/* Control Values */
void SetControlValue(ControlHandle theControl, SInt16 theValue);
SInt16 GetControlValue(ControlHandle theControl);
void SetControlMinimum(ControlHandle theControl, SInt16 minValue);
SInt16 GetControlMinimum(ControlHandle theControl);
void SetControlMaximum(ControlHandle theControl, SInt16 maxValue);
SInt16 GetControlMaximum(ControlHandle theControl);

/* Control Title */
void SetControlTitle(ControlHandle theControl, ConstStr255Param title);
void GetControlTitle(ControlHandle theControl, Str255 title);

/* Control Properties */
void SetControlReference(ControlHandle theControl, SInt32 data);
SInt32 GetControlReference(ControlHandle theControl);
void SetControlAction(ControlHandle theControl, ControlActionProcPtr actionProc);
ControlActionProcPtr GetControlAction(ControlHandle theControl);
SInt16 GetControlVariant(ControlHandle theControl);
ControlDefProcPtr GetControlDefFunction(ControlHandle theControl);

/* Control Interaction */
SInt16 TestControl(ControlHandle theControl, Point thePt);
SInt16 TrackControl(ControlHandle theControl, Point thePoint,
                     ControlActionProcPtr actionProc);
SInt16 FindControl(Point thePoint, WindowPtr theWindow,
                    ControlHandle *theControl);

/* Color Control Support */
void SetControlColor(ControlHandle theControl, CCTabHandle newColorTable);
Boolean GetAuxiliaryControlRecord(ControlHandle theControl, AuxCtlHandle *acHndl);

/* Standard Controls */
void RegisterStandardControlTypes(void);
Boolean IsButtonControl(ControlHandle control);
Boolean IsCheckboxControl(ControlHandle control);
Boolean IsRadioControl(ControlHandle control);
Boolean IsDefaultButton(ControlHandle button);
Boolean IsCancelButton(ControlHandle button);

/* Scrollbar Controls */
void RegisterScrollBarControlType(void);
Boolean IsScrollBarControl(ControlHandle control);
void SetScrollBarPageSize(ControlHandle scrollBar, SInt16 pageSize);
SInt16 GetScrollBarPageSize(ControlHandle scrollBar);
void SetScrollBarLiveTracking(ControlHandle scrollBar, Boolean liveTracking);
Boolean GetScrollBarLiveTracking(ControlHandle scrollBar);

/* Scrollbar Creation Helpers */
ControlHandle NewVScrollBar(WindowPtr w, const Rect* bounds, SInt16 min, SInt16 max, SInt16 value);
ControlHandle NewHScrollBar(WindowPtr w, const Rect* bounds, SInt16 min, SInt16 max, SInt16 value);

/* Scrollbar Integration (for List Manager, etc.) */
void UpdateScrollThumb(ControlHandle scrollBar, SInt16 value, SInt16 min, SInt16 max, SInt16 visibleSpan);
/* Note: For horizontal scrollbars, part codes (inUpButton/inDownButton) map to left/right visually; use outDelta instead of interpreting part names literally. */
SInt16 TrackScrollbar(ControlHandle scrollBar, Point startLocal, SInt16 startPart,
                      SInt16 modifiers, SInt16* outDelta);
SInt16 TrackScrollbarAction(ControlHandle scrollBar, Point startLocal, SInt16 startPart,
                            ControlActionProcPtr action, SInt16* outDelta);

/* Text Controls */
ControlHandle NewEditTextControl(WindowPtr window, const Rect *bounds,
                                ConstStr255Param text, Boolean visible,
                                SInt16 maxLength, SInt32 refCon);
ControlHandle NewStaticTextControl(WindowPtr window, const Rect *bounds,
                                  ConstStr255Param text, Boolean visible,
                                  SInt16 alignment, SInt32 refCon);

/* Popup Controls */
ControlHandle NewPopupControl(WindowPtr window, const Rect *bounds,
                             ConstStr255Param title, Boolean visible,
                             SInt16 menuID, SInt16 variation, SInt32 refCon);

/* Control Type Registration */
void RegisterControlType(SInt16 procID, ControlDefProcPtr defProc);
ControlDefProcPtr GetControlDefProc(SInt16 procID);

/* Control Definition Procedures */
SInt32 ButtonCDEF(SInt16 varCode, ControlHandle theControl, SInt16 message, SInt32 param);
SInt32 CheckboxCDEF(SInt16 varCode, ControlHandle theControl, SInt16 message, SInt32 param);
SInt32 RadioButtonCDEF(SInt16 varCode, ControlHandle theControl, SInt16 message, SInt32 param);
SInt32 ScrollBarCDEF(SInt16 varCode, ControlHandle theControl, SInt16 message, SInt32 param);

/* Drawing and Tracking */
void DrawScrollBar(ControlHandle scrollBar);

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
SInt16 _CallControlDefProc(ControlHandle control, SInt16 message, SInt32 param);
Handle _GetControlDefProc(SInt16 procID);

/* Smoke Test Functions (CTRL_SMOKE_TEST) */
void InitControlSmokeTest(void);
void CreateControlSmokeWindow(void);
Boolean HandleControlSmokeClick(WindowPtr window, Point where);
Boolean HandleControlSmokeKey(WindowPtr window, EventRecord* evt);

#ifdef __cplusplus
}
#endif

#endif /* CONTROLMANAGER_H */
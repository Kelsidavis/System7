/*
 * DialogManager.h - Macintosh System 7.1 Dialog Manager API
 *
 * This header provides the complete Dialog Manager interface for Mac System 7.1,
 * maintaining exact behavioral compatibility while providing modern platform
 * integration capabilities.
 *
 * The Dialog Manager is essential for:
 * - Modal and modeless dialog handling
 * - Alert dialogs and system notifications
 * - File dialogs (Standard File Package)
 * - Dialog item management and interaction
 * - Resource-based dialog templates (DLOG/DITL)
 * - Keyboard navigation and accessibility
 */

#ifndef DIALOG_MANAGER_H
#define DIALOG_MANAGER_H

#include "SystemTypes.h"
#include "DialogManager/DialogItems.h"

/* Forward declarations */


#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for Mac types */
/* Rect and Point types are defined in MacTypes.h */
/* Str255 is defined in MacTypes.h */
/* OSErr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Window and Event Manager dependencies */
/* Ptr is defined in MacTypes.h */
/* EventRecord defined in EventTypes.h */

/* TextEdit dependencies */
/* Handle is defined in MacTypes.h */

/* Dialog Manager core types */
/* Ptr is defined in MacTypes.h */

/* Dialog item types and constants */

/* Standard dialog button IDs */

/* Alert icon types */

/* Dialog item list manipulation methods */

/* Stage list type for alerts */

/* Callback procedure types */

/* Dialog record - internal structure exactly matching Mac OS */
/* DialogRecord is defined in SystemTypes.h */

/* Dialog template structure for DLOG resources */

/* Ptr is defined in MacTypes.h */

/* Alert template structure for ALRT resources */

/* Ptr is defined in MacTypes.h */

/* Dialog item structure for DITL resources */

/* Dialog item list (DITL) structure */

/* Handle is defined in MacTypes.h */

/* Modal dialog window classes */

/* Extended dialog features flags */

/*
 * CORE DIALOG MANAGER API
 * These functions provide exact Mac System 7.1 Dialog Manager compatibility
 */

/* Dialog Manager initialization and cleanup */
void InitDialogs(ResumeProcPtr resumeProc);
void ErrorSound(SoundProcPtr soundProc);

/* Dialog creation and disposal */
DialogPtr NewDialog(void* wStorage, const Rect* boundsRect, const unsigned char* title,
                    Boolean visible, SInt16 procID, WindowPtr behind, Boolean goAwayFlag,
                    SInt32 refCon, Handle itmLstHndl);
DialogPtr GetNewDialog(SInt16 dialogID, void* dStorage, WindowPtr behind);
DialogPtr NewColorDialog(void* dStorage, const Rect* boundsRect, const unsigned char* title,
                        Boolean visible, SInt16 procID, WindowPtr behind, Boolean goAwayFlag,
                        SInt32 refCon, Handle items);
void CloseDialog(DialogPtr theDialog);
void DisposDialog(DialogPtr theDialog);
void DisposeDialog(DialogPtr theDialog);

/* Dialog drawing and updating */
void DrawDialog(DialogPtr theDialog);
void UpdateDialog(DialogPtr theDialog, RgnHandle updateRgn);
void UpdtDialog(DialogPtr theDialog, RgnHandle updateRgn);

/* Modal dialog processing */
void ModalDialog(ModalFilterProcPtr filterProc, SInt16* itemHit);
Boolean IsDialogEvent(const EventRecord* theEvent);
Boolean DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog, SInt16* itemHit);

/* Alert dialogs */
SInt16 Alert(SInt16 alertID, ModalFilterProcPtr filterProc);
SInt16 StopAlert(SInt16 alertID, ModalFilterProcPtr filterProc);
SInt16 NoteAlert(SInt16 alertID, ModalFilterProcPtr filterProc);
SInt16 CautionAlert(SInt16 alertID, ModalFilterProcPtr filterProc);

/* Alert stage management */
SInt16 GetAlertStage(void);
void ResetAlertStage(void);
void ResetAlrtStage(void);

/* Dialog edit operations */
void DialogCut(DialogPtr theDialog);
void DialogCopy(DialogPtr theDialog);
void DialogPaste(DialogPtr theDialog);
void DialogDelete(DialogPtr theDialog);
unsigned char* DM_ParamTextSlot(SInt16 index);   /* ParamText's string for ^index */
void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3);

/* Dialog settings */
void SetDialogFont(SInt16 fontNum);
void SetDAFont(SInt16 fontNum);

/* Standard filter procedure and extended features */
Boolean StdFilterProc(DialogPtr theDialog, EventRecord* event, SInt16* itemHit);
OSErr SetDialogDefaultItem(DialogPtr theDialog, SInt16 newItem);
OSErr SetDialogCancelItem(DialogPtr theDialog, SInt16 newItem);
SInt16 GetDialogDefaultItem(DialogPtr theDialog);
SInt16 GetDialogCancelItem(DialogPtr theDialog);

/* Window modal class support */
SInt16 GetFrontWindowModalClass(SInt16* modalClass);
SInt16 GetWindowModalClass(WindowPtr theWindow, SInt16* modalClass);


/*
 * BACKWARDS COMPATIBILITY ALIASES
 * These maintain compatibility with existing Mac code
 */
#define GetAlrtStage    GetAlertStage
#define DlgCut          DialogCut
#define DlgCopy         DialogCopy
#define DlgPaste        DialogPaste
#define DlgDelete       DialogDelete
#define NewCDialog      NewColorDialog

/*
 * EXTENDED API FOR MODERN PLATFORMS
 * These functions provide additional capabilities for modern integration
 */

/* Platform integration */
void DialogManager_SetNativeDialogEnabled(Boolean enabled);
Boolean DialogManager_GetNativeDialogEnabled(void);
void DialogManager_SetAccessibilityEnabled(Boolean enabled);
Boolean DialogManager_GetAccessibilityEnabled(void);

/* High-DPI support */
void DialogManager_SetScaleFactor(float scale);
float DialogManager_GetScaleFactor(void);

/* File dialog integration */
OSErr DialogManager_ShowOpenFileDialog(const char* title, const char* defaultPath,
                                      const char* fileTypes, char* selectedPath,
                                      size_t pathSize);
OSErr DialogManager_ShowSaveFileDialog(const char* title, const char* defaultPath,
                                      const char* defaultName, char* selectedPath,
                                      size_t pathSize);

/* Color and theme support */


/*
 * INTERNAL UTILITY FUNCTIONS
 * These are used internally but may be useful for advanced applications
 */
Handle GetDialogItemList(DialogPtr theDialog);
void SetDialogItemList(DialogPtr theDialog, Handle itemList);
Boolean GetDialogTracksCursor(DialogPtr theDialog);

/* Dialog state queries */
Boolean IsModalDialog(DialogPtr theDialog);
Boolean IsDialogVisible(DialogPtr theDialog);
WindowPtr GetDialogWindow(DialogPtr theDialog);
DialogPtr GetWindowDialog(WindowPtr theWindow);

/* Keyboard navigation and control activation */
ControlHandle DM_FindDefaultButton(WindowPtr dialog);
Boolean DM_IsControlOf(WindowPtr w, Handle h);   /* is h one of w's controls */
ControlHandle DM_FindCancelButton(WindowPtr dialog);
void DM_ActivatePushButton(ControlHandle button);
void DM_SetKeyboardFocus(WindowPtr window, ControlHandle newFocus);
void DM_FocusNextControl(WindowPtr window, Boolean backwards);
ControlHandle DM_GetKeyboardFocus(WindowPtr window);
Boolean DM_HandleReturnKey(WindowPtr dialog, SInt16* itemHit);
Boolean DM_HandleEscapeKey(WindowPtr dialog, SInt16* itemHit);
Boolean DM_HandleSpaceKey(WindowPtr dialog, ControlHandle focusedControl);
Boolean DM_HandleTabKey(WindowPtr dialog, Boolean shiftPressed);
Boolean DM_HandleDialogKey(WindowPtr dialog, EventRecord* evt, SInt16* itemHit);

/* Debounce helper (for both keyboard and mouse paths) */
Boolean DM_DebounceAction(SInt16 kind); /* kind: 1=keyboard, 2=mouse */

/* Focus cleanup for window/control disposal */
void DM_ClearFocusForWindow(WindowPtr window);
void DM_OnDisposeControl(ControlHandle control);

/* Focus ring drawing (for Draw1Control hook) */
void ToggleFocusRing(ControlHandle control);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_MANAGER_H */

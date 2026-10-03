/* Core Dialog Manager API. */

#ifndef DIALOG_MANAGER_CORE_H
#define DIALOG_MANAGER_CORE_H

#include "SystemTypes.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/AlertDialogs.h"

/* Forward declarations */
typedef struct DialogMgrGlobals DialogMgrGlobals;


#ifdef __cplusplus
extern "C" {
#endif

/* Dialog and event types are defined by the shared system headers. */
#ifndef DIALOG_TYPES_DEFINED
#define DIALOG_TYPES_DEFINED

#endif /* DIALOG_TYPES_DEFINED */

/*
 * NewDialog - Create a new dialog
 * Assembly signature: newdialog proc EXPORT with c2pstr/p2cstr conversions
 */
DialogPtr NewDialog(void* wStorage, const Rect* boundsRect,
                   const unsigned char* title, Boolean visible, SInt16 procID,
                   WindowPtr behind, Boolean goAwayFlag, SInt32 refCon,
                   Handle itmLstHndl);

/*
 * NewColorDialog (NewCDialog) - Create a new color dialog
 * Assembly signature: newcolordialog/newcdialog proc EXPORT
 */
DialogPtr NewColorDialog(void* wStorage, const Rect* boundsRect,
                        const unsigned char* title, Boolean visible, SInt16 procID,
                        WindowPtr behind, Boolean goAwayFlag, SInt32 refCon,
                        Handle itmLstHndl);

/*
 * StdFilterProc - Standard filter procedure for dialog events
 * Assembly signature: STDFILTERPROC proc EXPORT - trampoline to actual filter
 */
Boolean StdFilterProc(DialogPtr dlg, EventRecord* evt, SInt16* itemHit);

/* Utility functions for string conversion. */
void C2PStr(char* str);     /* Convert C string to Pascal string in place */
void P2CStr(unsigned char* str);  /* Convert Pascal string to C string in place */

/* Dialog Manager initialization */
void InitDialogs(ResumeProcPtr resumeProc);
void ErrorSound(SoundProcPtr soundProc);

/* Global state access */
DialogMgrGlobals* GetDialogManagerGlobals(void);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_MANAGER_CORE_H */

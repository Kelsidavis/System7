/* #include "SystemTypes.h" */
#include "DialogManager/DialogInternal.h"
#include <stdlib.h>
#include <string.h>
/* #include <stdio.h> */
/*
 * DialogManagerCore.c - Core Dialog Manager Implementation
 *
 * This module provides the core Dialog Manager functionality with exact
 * Mac System 7.1 behavioral compatibility, including dialog creation,
 * disposal, and basic management operations.
 */

// #include "CompatibilityFix.h" // Removed
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/ModalDialogs.h"
#include "DialogManager/DialogItems.h"
#include "DialogManager/DialogResources.h"
#include "DialogManager/DialogEvents.h"
#include "DialogManager/AlertDialogs.h"
#include "DialogManager/DialogManagerStateExt.h"
#include "DialogManager/dialog_manager_private.h"  /* For DialogMgrGlobals */
#include "MemoryMgr/MemoryManager.h"
#include <assert.h>
#include "DialogManager/DialogLogging.h"


/* Global Dialog Manager state */
static DialogManagerState gDialogManagerState = {0};
static Boolean gDialogManagerInitialized = false;

/* External dependencies that need to be linked */
/* NewWindow is already declared in WindowManager.h */
extern void DisposeWindow(WindowPtr window);
extern void ShowWindow(WindowPtr window);
extern void HideWindow(WindowPtr window);
extern void DrawWindow(WindowPtr window);
extern void InvalRect(const Rect* rect);
extern void BeginUpdate(WindowPtr window);
extern void EndUpdate(WindowPtr window);
extern void EraseRect(const Rect* r);
extern void GetPort(GrafPtr* port);
extern void SetPort(GrafPtr port);

/* Private function prototypes */
static DialogPtr CreateDialogStructure(void* storage, Boolean isColor);
static void InitializeDialogRecord(DialogPtr dialog, const Rect* bounds,
                                   const unsigned char* title, Boolean visible,
                                   SInt16 procID, WindowPtr behind, Boolean goAway,
                                   SInt32 refCon, Handle itemList);
static void DisposeDialogStructure(DialogPtr dialog, Boolean closeOnly);
static OSErr ValidateDialogPtr(DialogPtr dialog);
static void SetupDialogDefaults(DialogPtr dialog);
static void PlaySystemBeep(SInt16 soundType);

/*
 * InitDialogs - Initialize the Dialog Manager
 *
 * This function initializes the Dialog Manager and all its subsystems.
 * It must be called before any other Dialog Manager functions.
 */
void InitDialogs(ResumeProcPtr resumeProc)
{
    if (gDialogManagerInitialized) {
        return; /* Already initialized */
    }

    /* Initialize global state */
    memset(&gDialogManagerState, 0, sizeof(gDialogManagerState));
    gDialogManagerState.globals.resumeProc = resumeProc;
    gDialogManagerState.globals.soundProc = NULL;
    gDialogManagerState.globals.alertStage = 0;
    gDialogManagerState.globals.dialogFont = 0; /* System font */
    gDialogManagerState.globals.spareFlags = 0;
    gDialogManagerState.globals.frontModal = NULL;
    gDialogManagerState.globals.defaultItem = 1; /* OK button by default */
    gDialogManagerState.globals.cancelItem = 2; /* Cancel button by default */
    gDialogManagerState.globals.tracksCursor = false;

    gDialogManagerState.initialized = true;
    gDialogManagerState.modalLevel = 0;
    gDialogManagerState.systemModal = false;
    gDialogManagerState.useNativeDialogs = false;
    gDialogManagerState.useAccessibility = true;
    gDialogManagerState.scaleFactor = 1.0f;
    gDialogManagerState.platformContext = NULL;

    /* Clear parameter text */
    for (int i = 0; i < 4; i++) {
        gDialogManagerState.globals.paramText[i][0] = 0;
    }

    /* Clear modal stack */
    for (int i = 0; i < 16; i++) {
        gDialogManagerState.modalStack[i] = NULL;
    }

    /* Initialize subsystems */
    InitModalDialogs();
    InitDialogItems();
    InitDialogResources();
    InitDialogEvents();
    InitAlertDialogs();

    gDialogManagerInitialized = true;

    // DIALOG_LOG_DEBUG("Dialog Manager initialized successfully\n");
}

/*
 * ErrorSound - Set the error sound procedure
 */
void ErrorSound(SoundProcPtr soundProc)
{
    if (!gDialogManagerInitialized) {
        return;
    }

    gDialogManagerState.globals.soundProc = soundProc;
}

/*
 * NewDialog - Create a new dialog
 *
 * This is the core dialog creation function that creates a dialog
 * from the specified parameters and item list.
 */
DialogPtr NewDialog(void* wStorage, const Rect* boundsRect, const unsigned char* title,
                    Boolean visible, SInt16 procID, WindowPtr behind, Boolean goAwayFlag,
                    SInt32 refCon, Handle itmLstHndl)
{
    DialogPtr dialog;
    WindowPtr window;

    if (!gDialogManagerInitialized) {
        // printf("Error: Dialog Manager not initialized\n");
        return NULL;
    }

    if (!boundsRect || !itmLstHndl) {
        // printf("Error: Invalid parameters to NewDialog\n");
        return NULL;
    }

    /* Allocate dialog structure */
    dialog = CreateDialogStructure(wStorage, false);
    if (!dialog) {
        // printf("Error: Failed to create dialog structure\n");
        return NULL;
    }

    /* Create the underlying window */
    window = NewWindow(wStorage, boundsRect, title, false, /* Start hidden */
                       procID, behind, goAwayFlag, refCon);
    if (!window) {
        // printf("Error: Failed to create dialog window\n");
        if (!wStorage) {
            DisposePtr((Ptr)dialog);
        }
        return NULL;
    }

    /* Initialize dialog record */
    InitializeDialogRecord(dialog, boundsRect, title, visible, procID,
                          behind, goAwayFlag, refCon, itmLstHndl);

    /* Cast to DialogRecord for access to extended fields */
    DialogRecord* dialogRec = (DialogRecord*)dialog;

    /* Copy window data into dialog record */
    memcpy(&dialogRec->window, window, sizeof(struct WindowRecord));

    /* Set up dialog-specific data */
    dialogRec->items = itmLstHndl;
    dialogRec->textH = NULL; /* Will be created when needed */
    dialogRec->editField = -1; /* No active edit field */
    dialogRec->editOpen = 0;
    dialogRec->aDefItem = 1; /* Default button is item 1 */

    /* Set up dialog defaults */
    SetupDialogDefaults(dialog);

    /* Show the dialog if requested */
    if (visible) {
        ShowWindow((WindowPtr)dialog);
    }

    // printf("Created new dialog at %p\n", (void*)dialog);
    return dialog;
}

/*
 * GetNewDialog - Create dialog from DLOG resource
 */
DialogPtr GetNewDialog(SInt16 dialogID, void* dStorage, WindowPtr behind)
{
    DialogTemplate* template = NULL;
    Handle itemList = NULL;
    DialogPtr dialog = NULL;
    OSErr err;

    if (!gDialogManagerInitialized) {
        // printf("Error: Dialog Manager not initialized\n");
        return NULL;
    }

    /* Load dialog template from resource */
    err = LoadDialogTemplate(dialogID, &template);
    if (err != 0 || !template) {
        // printf("Error: Failed to load DLOG resource %d (error %d)\n", dialogID, err);
        return NULL;
    }

    /* Load dialog item list from resource */
    err = LoadDialogItemList(template->itemsID, &itemList);
    if (err != 0 || !itemList) {
        // printf("Error: Failed to load DITL resource %d (error %d)\n", template->itemsID, err);
        DisposeDialogTemplate(template);
        return NULL;
    }

    /* Create the dialog */
    dialog = NewDialog(dStorage, &template->boundsRect, template->title,
                       template->visible, template->procID, behind,
                       template->goAwayFlag, template->refCon, itemList);

    /* Clean up template (dialog now owns the item list) */
    DisposeDialogTemplate(template);

    if (!dialog) {
        DisposeDialogItemList(itemList);
        // printf("Error: Failed to create dialog from template\n");
        return NULL;
    }

    // printf("Created dialog from DLOG resource %d\n", dialogID);
    return dialog;
}

/*
 * NewColorDialog - Create a new color dialog
 */
DialogPtr NewColorDialog(void* dStorage, const Rect* boundsRect, const unsigned char* title,
                        Boolean visible, SInt16 procID, WindowPtr behind, Boolean goAwayFlag,
                        SInt32 refCon, Handle items)
{
    DialogPtr dialog;

    /* For now, color dialogs are the same as regular dialogs */
    /* In a full implementation, this would enable color support */
    dialog = NewDialog(dStorage, boundsRect, title, visible, procID,
                       behind, goAwayFlag, refCon, items);

    if (dialog) {
        // printf("Created new color dialog at %p\n", (void*)dialog);
    }

    return dialog;
}

/*
 * CloseDialog - Close a dialog without disposing it
 */
void CloseDialog(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    /* Hide the dialog window */
    HideWindow((WindowPtr)theDialog);

    /* If this was a modal dialog, end modal processing */
    if (IsModalDialog(theDialog)) {
        EndModalDialog(theDialog);
    }
}

/*
 * DisposDialog - Dispose of a dialog and free its memory
 */
void DisposDialog(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    /* Clear keyboard focus before disposal (defensive, also done in DisposeWindow) */
    DM_ClearFocusForWindow((WindowPtr)theDialog);

    /* End modal processing if active */
    if (IsModalDialog(theDialog)) {
        EndModalDialog(theDialog);
    }

    /* Dispose of the dialog structure */
    DisposeDialogStructure(theDialog, false);
}

/*
 * DisposeDialog - Synonym for DisposDialog (System 7.1 compatibility)
 */
void DisposeDialog(DialogPtr theDialog)
{
    DisposDialog(theDialog);
}

/*
 * DrawDialog - Draw the entire dialog
 */
void DrawDialog(DialogPtr theDialog)
{
    GrafPtr savePort;

    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    /* Save and set port */
    GetPort(&savePort);
    SetPort((GrafPtr)theDialog);

    /* Draw the window frame */
    DrawWindow((WindowPtr)theDialog);

    /* Draw all dialog items */
    SInt16 itemCount = CountDITL(theDialog);
    for (SInt16 i = 1; i <= itemCount; i++) {
        DrawDialogItem(theDialog, i);
    }

    /* Restore port */
    SetPort(savePort);

    // printf("Drew dialog at %p with %d items\n", (void*)theDialog, itemCount);
}

/*
 * UpdateDialog - Update dialog in response to update event
 */
void UpdateDialog(DialogPtr theDialog, RgnHandle updateRgn)
{
    GrafPtr savePort;
    SInt16 itemCount, i;

    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    /* Save and set port */
    GetPort(&savePort);
    SetPort((GrafPtr)theDialog);

    /* Begin update - sets clip to update region */
    BeginUpdate((WindowPtr)theDialog);

    /* Erase content region */
    EraseRect(&((GrafPtr)theDialog)->portRect);

    /* Draw all visible items in order */
    itemCount = CountDITL(theDialog);
    for (i = 1; i <= itemCount; i++) {
        DrawDialogItem(theDialog, i);
    }

    /* End update */
    EndUpdate((WindowPtr)theDialog);

    /* Restore port */
    SetPort(savePort);

    // DIALOG_LOG_DEBUG("Dialog: Updated dialog %p\n", (void*)theDialog);
}

/*
 * UpdtDialog - Synonym for UpdateDialog
 */
void UpdtDialog(DialogPtr theDialog, RgnHandle updateRgn)
{
    UpdateDialog(theDialog, updateRgn);
}

/*
 * SetDialogFont - Set font for dialogs
 */
void SetDialogFont(SInt16 fontNum)
{
    if (!gDialogManagerInitialized) {
        return;
    }

    gDialogManagerState.globals.dialogFont = fontNum;
}

/*
 * SetDAFont - Synonym for SetDialogFont
 */
void SetDAFont(SInt16 fontNum)
{
    SetDialogFont(fontNum);
}

/*
 * ParamText - Set parameter text for alert substitution
 */
void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3)
{
    if (!gDialogManagerInitialized) {
        return;
    }

    /* Copy parameter strings (Pascal strings) with bounds checking */
    if (param0) {
        unsigned char len = param0[0];
        if (len > 255) len = 255;
        gDialogManagerState.globals.paramText[0][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[0][1], &param0[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[0][0] = 0;
    }

    if (param1) {
        unsigned char len = param1[0];
        if (len > 255) len = 255;
        gDialogManagerState.globals.paramText[1][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[1][1], &param1[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[1][0] = 0;
    }

    if (param2) {
        unsigned char len = param2[0];
        if (len > 255) len = 255;
        gDialogManagerState.globals.paramText[2][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[2][1], &param2[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[2][0] = 0;
    }

    if (param3) {
        unsigned char len = param3[0];
        if (len > 255) len = 255;
        gDialogManagerState.globals.paramText[3][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[3][1], &param3[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[3][0] = 0;
    }

    // printf("Set parameter text: param0='%.*s', param1='%.*s', param2='%.*s', param3='%.*s'\n",
    //        gDialogManagerState.globals.paramText[0][0], &gDialogManagerState.globals.paramText[0][1],
    //        gDialogManagerState.globals.paramText[1][0], &gDialogManagerState.globals.paramText[1][1],
    //        gDialogManagerState.globals.paramText[2][0], &gDialogManagerState.globals.paramText[2][1],
    //        gDialogManagerState.globals.paramText[3][0], &gDialogManagerState.globals.paramText[3][1]);
}

/*
 * Extended API implementations
 */

void DialogManager_SetNativeDialogEnabled(Boolean enabled)
{
    if (gDialogManagerInitialized) {
        gDialogManagerState.useNativeDialogs = enabled;
    }
}

Boolean DialogManager_GetNativeDialogEnabled(void)
{
    return gDialogManagerInitialized ? gDialogManagerState.useNativeDialogs : false;
}

void DialogManager_SetAccessibilityEnabled(Boolean enabled)
{
    if (gDialogManagerInitialized) {
        gDialogManagerState.useAccessibility = enabled;
    }
}

Boolean DialogManager_GetAccessibilityEnabled(void)
{
    return gDialogManagerInitialized ? gDialogManagerState.useAccessibility : false;
}

void DialogManager_SetScaleFactor(float scale)
{
    if (gDialogManagerInitialized && scale > 0.0f) {
        gDialogManagerState.scaleFactor = scale;
    }
}

float DialogManager_GetScaleFactor(void)
{
    return gDialogManagerInitialized ? gDialogManagerState.scaleFactor : 1.0f;
}

/*
 * Internal utility functions
 */

static DialogPtr CreateDialogStructure(void* storage, Boolean isColor)
{
    DialogPtr dialog;

    if (storage) {
        /* Use provided storage */
        dialog = (DialogPtr)storage;
    } else {
        /* Allocate new storage */
        dialog = (DialogPtr)NewPtr(sizeof(DialogRecord));
        if (!dialog) {
            return NULL;
        }
    }

    /* Initialize the structure */
    memset(dialog, 0, sizeof(DialogRecord));

    return dialog;
}

static void InitializeDialogRecord(DialogPtr dialog, const Rect* bounds,
                                   const unsigned char* title, Boolean visible,
                                   SInt16 procID, WindowPtr behind, Boolean goAway,
                                   SInt32 refCon, Handle itemList)
{
    /* This function would initialize the dialog record with the given parameters */
    /* For now, we'll just set the basic fields that we've defined */
    DialogRecord* dialogRec = (DialogRecord*)dialog;

    dialogRec->items = itemList;
    dialogRec->textH = NULL;
    dialogRec->editField = -1;
    dialogRec->editOpen = 0;
    dialogRec->aDefItem = 1;
}

static void DisposeDialogStructure(DialogPtr dialog, Boolean closeOnly)
{
    if (!dialog) {
        return;
    }

    /* Dispose of dialog items if not just closing */
    DialogRecord* dialogRec = (DialogRecord*)dialog;
    if (!closeOnly && dialogRec->items) {
        DisposeDialogItemList(dialogRec->items);
        dialogRec->items = NULL;
    }

    /* Dispose of TextEdit record if allocated */
    if (!closeOnly && dialogRec->textH) {
        /* TEDispose(dialogRec->textH); - would need TextEdit integration */
        dialogRec->textH = NULL;
    }

    /* Dispose of all TEHandles created for edit-text items
     * NOTE: teHandles array is global in DialogManagerState, so we dispose all
     * TEHandles when disposing any dialog. GetOrCreateDialogTEHandle will recreate
     * them on demand if other dialogs are still active. */
    if (!closeOnly) {
        DialogManagerState* dmState = GetDialogManagerState();
        if (dmState) {
            DialogManagerState_Extended* extState = GET_EXTENDED_DLG_STATE(dmState);
            if (extState) {
                extern void TEDispose(TEHandle hTE);

                /* Dispose all TEHandles to prevent memory leaks */
                for (int i = 0; i < 256; i++) {
                    if (extState->teHandles[i] != NULL) {
                        TEDispose((TEHandle)extState->teHandles[i]);
                        extState->teHandles[i] = NULL;
                    }
                }
            }
        }
    }

    /* Dispose of the underlying window */
    if (!closeOnly) {
        DisposeWindow((WindowPtr)dialog);
    }

    /* Free the dialog structure if it was allocated */
    if (!closeOnly) {
        DisposePtr((Ptr)dialog);
    }
}

static OSErr ValidateDialogPtr(DialogPtr dialog)
{
    if (!dialog) {
        return -1700; /* dialogErr_InvalidDialog */
    }

    /* Additional validation could be added here */
    /* For now, just check that it's not NULL */

    return 0; /* noErr */
}

static void SetupDialogDefaults(DialogPtr dialog)
{
    if (!dialog) {
        return;
    }

    /* Set up default dialog behavior */
    DialogRecord* dialogRec = (DialogRecord*)dialog;
    dialogRec->aDefItem = 1; /* Default button is typically item 1 */
    dialogRec->editField = -1; /* No edit field active initially */
    dialogRec->editOpen = 0;
}

static void PlaySystemBeep(SInt16 soundType)
{
    /* Play appropriate system sound */
    if (gDialogManagerState.globals.soundProc) {
        /* Cast void* to function pointer and call it */
        typedef void (*SoundProc)(SInt16);
        SoundProc proc = (SoundProc)gDialogManagerState.globals.soundProc;
        proc(soundType);
    } else {
        /* Default system beep */
        // printf("BEEP (sound type %d)\n", soundType);
    }
}

/*
 * Internal utility functions for other modules
 */

Handle GetDialogItemList(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return NULL;
    }

    return ((DialogRecord*)theDialog)->items;
}

void SetDialogItemList(DialogPtr theDialog, Handle itemList)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    ((DialogRecord*)theDialog)->items = itemList;
}

SInt16 GetDialogDefaultItem(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return 0;
    }

    return ((DialogRecord*)theDialog)->aDefItem;
}

SInt16 GetDialogCancelItem(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return 0;
    }
    return gDialogManagerState.globals.cancelItem;
}

OSErr SetDialogDefaultItem(DialogPtr theDialog, SInt16 newItem)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return -50; /* paramErr */
    }

    ((DialogRecord*)theDialog)->aDefItem = newItem;
    return noErr;
}

OSErr SetDialogCancelItem(DialogPtr theDialog, SInt16 newItem)
{
    /* Store in global state for now */
    /* A full implementation would track this per dialog */
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return -50; /* paramErr */
    }

    gDialogManagerState.globals.cancelItem = newItem;
    return noErr;
}

Boolean GetDialogTracksCursor(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return false;
    }

    return gDialogManagerState.globals.tracksCursor;
}

static Boolean IsModalDialog_Core(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return false;
    }

    /* Check if this dialog is in the modal stack */
    for (int i = 0; i < gDialogManagerState.modalLevel; i++) {
        if (gDialogManagerState.modalStack[i] == (WindowPtr)theDialog) {
            return true;
        }
    }

    return false;
}

Boolean IsDialogVisible(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return false;
    }

    /* In a full implementation, this would check the window's visibility */
    /* For now, just return true */
    return true;
}

WindowPtr GetDialogWindow(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return NULL;
    }

    return (WindowPtr)theDialog;
}

DialogPtr GetWindowDialog(WindowPtr theWindow)
{
    /* In a full implementation, this would check if the window is actually a dialog */
    /* For now, just cast it */
    return (DialogPtr)theWindow;
}

/*
 * Get global Dialog Manager state (for use by other modules)
 */
DialogManagerState* GetDialogManagerState(void)
{
    return gDialogManagerInitialized ? &gDialogManagerState : NULL;
}

/* Stub for GetDialogManagerGlobals - returns stub globals */
static DialogMgrGlobals gStubDialogMgrGlobals = {0};

DialogMgrGlobals* GetDialogManagerGlobals(void) {
    return &gStubDialogMgrGlobals;
}

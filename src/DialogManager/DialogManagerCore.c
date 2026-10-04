#include "DialogManager/DialogInternal.h"
#include <stdlib.h>
#include <string.h>
/*
 * DialogManagerCore.c - Core Dialog Manager Implementation
 *
 * This module provides the core Dialog Manager functionality with exact
 * Mac System 7.1 behavioral compatibility, including dialog creation,
 * disposal, and basic management operations.
 */

#include "SystemTypes.h"
#include <stddef.h>
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
#include "WindowManager/WindowManager.h"
#include "WindowManager/WindowKinds.h"
#include "QuickDraw/QuickDraw.h"
#include "MemoryMgr/MemoryManager.h"
#include <assert.h>
#include "DialogManager/DialogLogging.h"

/* Global Dialog Manager state */
/*
 * The Dialog Manager's state, sized for the extended view of it
 * (DialogManagerStateExt.h) that the edit-text and keyboard code reach
 * through GET_EXTENDED_DLG_STATE. It used to be allocated at the base
 * size, so the extended fields - 256 TextEdit handle slots and their
 * owner - lay past its end: disposing of any dialog zeroed a kilobyte of
 * whatever followed it, the modal and alert state among it, and storing an
 * edit field wrote into them.
 */
static DialogManagerState_Extended gDialogManagerStateStorage;
#define gDialogManagerState (gDialogManagerStateStorage.base)
_Static_assert(offsetof(DialogManagerState_Extended, base) == 0,
               "the extended Dialog Manager state must begin with the base state");
_Static_assert(offsetof(DialogManagerState_Extended, teHandles) == sizeof(DialogManagerState),
               "TextEdit state must immediately follow the base state");
static Boolean gDialogManagerInitialized = false;

/* Private function prototypes */
static DialogPtr CreateDialogStructure(void* storage, Boolean isColor);
static void InitializeDialogRecord(DialogPtr dialog, const Rect* bounds,
                                   const unsigned char* title, Boolean visible,
                                   SInt16 procID, WindowPtr behind, Boolean goAway,
                                   SInt32 refCon, Handle itemList);
static void DisposeDialogStructure(DialogPtr dialog, Boolean closeOnly);
static OSErr ValidateDialogPtr(DialogPtr dialog);
static void SetupDialogDefaults(DialogPtr dialog);

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
    memset(&gDialogManagerStateStorage, 0, sizeof(gDialogManagerStateStorage));
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
    InitDialogEvents();
    InitAlertDialogs();

    gDialogManagerInitialized = true;

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
        return NULL;
    }

    if (!boundsRect || !itmLstHndl) {
        return NULL;
    }

    /* Allocate dialog structure */
    dialog = CreateDialogStructure(wStorage, false);
    if (!dialog) {
        return NULL;
    }

    /*
     * Create the underlying window inside the dialog record.
     *
     * A DialogRecord begins with a WindowRecord because a dialog IS a window
     * - every Toolbox call that takes a DialogPtr and hands it to the Window
     * Manager depends on that. This used to build a separate window and then
     * memcpy it into the dialog, leaving two records for one window: the
     * Window Manager's list held one and every DialogPtr in the system
     * pointed at the other. The copy never appeared in the window list, so a
     * dialog could not become the front window - the Open and Save dialogs
     * drew and then had the document window painted straight over them - the
     * real record was leaked, and disposal freed the dialog twice, once
     * through DisposeWindow and once directly. Giving NewWindow the dialog's
     * own storage makes them one object again.
     */
    window = NewWindow(&((DialogRecord*)dialog)->window, boundsRect, title,
                       false, /* Start hidden until items are set up */
                       procID, behind, goAwayFlag, refCon);
    if (!window) {
        if (!wStorage) {
            DisposePtr((Ptr)dialog);
        }
        return NULL;
    }

    window->windowKind = dialogKind;

    /* Initialize dialog record */
    InitializeDialogRecord(dialog, boundsRect, title, visible, procID,
                          behind, goAwayFlag, refCon, itmLstHndl);

    /* Cast to DialogRecord for access to extended fields */
    DialogRecord* dialogRec = (DialogRecord*)dialog;

    /* Set up dialog-specific data */
    dialogRec->items = itmLstHndl;
    dialogRec->textH = NULL; /* Will be created when needed */
    dialogRec->editField = -1; /* No active edit field */
    dialogRec->editOpen = 0;
    dialogRec->aDefItem = 1; /* Default button is item 1 */

    /* Set up dialog defaults */
    SetupDialogDefaults(dialog);

    /*
     * Record it as the current dialog.
     *
     * FrontDialog() looks at globals.frontModal and then falls back to
     * currentDialog - but nothing anywhere assigned currentDialog, so the
     * fallback was dead and only dialogs put up through BeginModalDialog were
     * ever findable. IsDialogEvent asks FrontWindowIsDialog(), which asks
     * FrontDialog(), so for a dialog created here and driven by its own event
     * loop every mouse click was rejected before it reached DialogSelect: the
     * Empty Trash confirmation drew correctly and then ignored its buttons
     * entirely.
     */
    gDialogManagerState.currentDialog = dialog;

    /* Give the first edit-text item the keyboard focus. System 7 opens a
     * dialog with its first field already active and its text selected, so
     * you can type straight into a rename or Find box; here nothing set focus
     * until the field was clicked, and even then the keys went to a stub. */
    InitDialogEditTextFocus(dialog);

    /* Show the dialog if requested */
    if (visible) {
        ShowWindow((WindowPtr)dialog);
    }

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
        return NULL;
    }

    /* Load dialog template from resource */
    err = LoadDialogTemplate(dialogID, &template);
    if (err != 0 || !template) {
        return NULL;
    }

    /* Load dialog item list from resource */
    err = LoadDialogItemList(template->itemsID, &itemList);
    if (err != 0 || !itemList) {
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
        return NULL;
    }

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

    /* Remove dialog item cache to prevent memory leak and stale cache reuse */
    RemoveDialogItemCache(theDialog);

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

}

/*
 * UpdateDialog - Update dialog in response to update event
 */
void UpdateDialog(DialogPtr theDialog, RgnHandle updateRgn)
{
    (void)updateRgn;
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
/* ParamText's strings, the one copy: what ^0 to ^3 stand for. */
unsigned char* DM_ParamTextSlot(SInt16 index)
{
    if (index < 0 || index > 3) return NULL;
    return gDialogManagerState.globals.paramText[index];
}

void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3)
{
    if (!gDialogManagerInitialized) {
        return;
    }

    /* Copy parameter strings (Pascal strings) - unsigned char already limited to 255 max */
    if (param0) {
        unsigned char len = param0[0];
        gDialogManagerState.globals.paramText[0][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[0][1], &param0[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[0][0] = 0;
    }

    if (param1) {
        unsigned char len = param1[0];
        gDialogManagerState.globals.paramText[1][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[1][1], &param1[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[1][0] = 0;
    }

    if (param2) {
        unsigned char len = param2[0];
        gDialogManagerState.globals.paramText[2][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[2][1], &param2[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[2][0] = 0;
    }

    if (param3) {
        unsigned char len = param3[0];
        gDialogManagerState.globals.paramText[3][0] = len;
        if (len > 0) {
            memcpy(&gDialogManagerState.globals.paramText[3][1], &param3[1], len);
        }
    } else {
        gDialogManagerState.globals.paramText[3][0] = 0;
    }

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
    (void)isColor;
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
    (void)bounds;
    (void)title;
    (void)visible;
    (void)procID;
    (void)behind;
    (void)goAway;
    (void)refCon;
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

    /* Stop reporting a dialog that is going away as the current one */
    if (gDialogManagerState.currentDialog == dialog) {
        gDialogManagerState.currentDialog = NULL;
    }

    /* Dispose of dialog items if not just closing */
    DialogRecord* dialogRec = (DialogRecord*)dialog;
    if (!closeOnly && dialogRec->items) {
        DisposeDialogItemList(dialogRec->items);
        dialogRec->items = NULL;
    }

    /*
     * Let go of the dialog's edit fields.
     *
     * They live in one global array indexed by item number, so leaving them
     * behind meant the next window to redraw could find a TextEdit record for
     * an item number it happened to share and draw that dialog's field into
     * its own port. Cancelling SimpleText's Find box left the top of the
     * document underneath it full of text that was never in the document.
     */
    DialogEditText_ReleaseAll(dialog);

    if (!closeOnly && dialogRec->textH) {
        TEDispose(dialogRec->textH);
        dialogRec->textH = NULL;
    }

    /* Dispose of the underlying window. The window record and the dialog
     * record are the same allocation, so this frees the dialog too - there
     * used to be a second DisposePtr here, which freed it a second time. */
    if (!closeOnly) {
        DisposeWindow((WindowPtr)dialog);
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

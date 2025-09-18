/*
 * DialogManagerCore.c - Core Dialog Manager Implementation
 *
 * This module provides the core Dialog Manager functionality with exact
 * Mac System 7.1 behavioral compatibility, including dialog creation,
 * disposal, and basic management operations.
 */

#include "../../include/DialogManager/DialogManager.h"
#include "../../include/DialogManager/DialogTypes.h"
#include "../../include/DialogManager/ModalDialogs.h"
#include "../../include/DialogManager/DialogItems.h"
#include "../../include/DialogManager/DialogResources.h"
#include "../../include/DialogManager/DialogEvents.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Global Dialog Manager state */
static DialogManagerState gDialogManagerState = {0};
static bool gDialogManagerInitialized = false;

/* External dependencies that need to be linked */
extern WindowPtr NewWindow(void* wStorage, const Rect* boundsRect,
                          const unsigned char* title, bool visible,
                          int16_t procID, WindowPtr behind, bool goAwayFlag,
                          int32_t refCon);
extern void DisposeWindow(WindowPtr window);
extern void ShowWindow(WindowPtr window);
extern void HideWindow(WindowPtr window);
extern void DrawWindow(WindowPtr window);
extern void InvalRect(const Rect* rect);

/* Private function prototypes */
static DialogPtr CreateDialogStructure(void* storage, bool isColor);
static void InitializeDialogRecord(DialogPtr dialog, const Rect* bounds,
                                   const unsigned char* title, bool visible,
                                   int16_t procID, WindowPtr behind, bool goAway,
                                   int32_t refCon, Handle itemList);
static void DisposeDialogStructure(DialogPtr dialog, bool closeOnly);
static OSErr ValidateDialogPtr(DialogPtr dialog);
static void SetupDialogDefaults(DialogPtr dialog);
static void PlaySystemBeep(int16_t soundType);

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

    gDialogManagerInitialized = true;

    printf("Dialog Manager initialized successfully\n");
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
                    bool visible, int16_t procID, WindowPtr behind, bool goAwayFlag,
                    int32_t refCon, Handle itmLstHndl)
{
    DialogPtr dialog;
    WindowPtr window;

    if (!gDialogManagerInitialized) {
        printf("Error: Dialog Manager not initialized\n");
        return NULL;
    }

    if (!boundsRect || !itmLstHndl) {
        printf("Error: Invalid parameters to NewDialog\n");
        return NULL;
    }

    /* Allocate dialog structure */
    dialog = CreateDialogStructure(wStorage, false);
    if (!dialog) {
        printf("Error: Failed to create dialog structure\n");
        return NULL;
    }

    /* Create the underlying window */
    window = NewWindow(wStorage, boundsRect, title, false, /* Start hidden */
                       procID, behind, goAwayFlag, refCon);
    if (!window) {
        printf("Error: Failed to create dialog window\n");
        if (!wStorage) {
            free(dialog);
        }
        return NULL;
    }

    /* Initialize dialog record */
    InitializeDialogRecord(dialog, boundsRect, title, visible, procID,
                          behind, goAwayFlag, refCon, itmLstHndl);

    /* Copy window data into dialog record */
    memcpy(&dialog->window, window, sizeof(struct WindowRecord));

    /* Set up dialog-specific data */
    dialog->items = itmLstHndl;
    dialog->textH = NULL; /* Will be created when needed */
    dialog->editField = -1; /* No active edit field */
    dialog->editOpen = 0;
    dialog->aDefItem = 1; /* Default button is item 1 */

    /* Set up dialog defaults */
    SetupDialogDefaults(dialog);

    /* Show the dialog if requested */
    if (visible) {
        ShowWindow((WindowPtr)dialog);
    }

    printf("Created new dialog at %p\n", (void*)dialog);
    return dialog;
}

/*
 * GetNewDialog - Create dialog from DLOG resource
 */
DialogPtr GetNewDialog(int16_t dialogID, void* dStorage, WindowPtr behind)
{
    DialogTemplate* template = NULL;
    Handle itemList = NULL;
    DialogPtr dialog = NULL;
    OSErr err;

    if (!gDialogManagerInitialized) {
        printf("Error: Dialog Manager not initialized\n");
        return NULL;
    }

    /* Load dialog template from resource */
    err = LoadDialogTemplate(dialogID, &template);
    if (err != 0 || !template) {
        printf("Error: Failed to load DLOG resource %d (error %d)\n", dialogID, err);
        return NULL;
    }

    /* Load dialog item list from resource */
    err = LoadDialogItemList(template->itemsID, &itemList);
    if (err != 0 || !itemList) {
        printf("Error: Failed to load DITL resource %d (error %d)\n", template->itemsID, err);
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
        printf("Error: Failed to create dialog from template\n");
        return NULL;
    }

    printf("Created dialog from DLOG resource %d\n", dialogID);
    return dialog;
}

/*
 * NewColorDialog - Create a new color dialog
 */
DialogPtr NewColorDialog(void* dStorage, const Rect* boundsRect, const unsigned char* title,
                        bool visible, int16_t procID, WindowPtr behind, bool goAwayFlag,
                        int32_t refCon, Handle items)
{
    DialogPtr dialog;

    /* For now, color dialogs are the same as regular dialogs */
    /* In a full implementation, this would enable color support */
    dialog = NewDialog(dStorage, boundsRect, title, visible, procID,
                       behind, goAwayFlag, refCon, items);

    if (dialog) {
        printf("Created new color dialog at %p\n", (void*)dialog);
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

    printf("Closed dialog at %p\n", (void*)theDialog);
}

/*
 * DisposDialog - Dispose of a dialog and free its memory
 */
void DisposDialog(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    printf("Disposing dialog at %p\n", (void*)theDialog);

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
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    /* Draw the window frame */
    DrawWindow((WindowPtr)theDialog);

    /* Draw all dialog items */
    int16_t itemCount = CountDITL(theDialog);
    for (int16_t i = 1; i <= itemCount; i++) {
        DrawDialogItem(theDialog, i);
    }

    printf("Drew dialog at %p with %d items\n", (void*)theDialog, itemCount);
}

/*
 * UpdateDialog - Update dialog in response to update event
 */
void UpdateDialog(DialogPtr theDialog, RgnHandle updateRgn)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    /* For now, just redraw the entire dialog */
    /* A full implementation would only redraw the update region */
    DrawDialog(theDialog);
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
void SetDialogFont(int16_t fontNum)
{
    if (!gDialogManagerInitialized) {
        return;
    }

    gDialogManagerState.globals.dialogFont = fontNum;
}

/*
 * SetDAFont - Synonym for SetDialogFont
 */
void SetDAFont(int16_t fontNum)
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

    /* Copy parameter strings (Pascal strings) */
    if (param0) {
        memcpy(gDialogManagerState.globals.paramText[0], param0, param0[0] + 1);
    } else {
        gDialogManagerState.globals.paramText[0][0] = 0;
    }

    if (param1) {
        memcpy(gDialogManagerState.globals.paramText[1], param1, param1[0] + 1);
    } else {
        gDialogManagerState.globals.paramText[1][0] = 0;
    }

    if (param2) {
        memcpy(gDialogManagerState.globals.paramText[2], param2, param2[0] + 1);
    } else {
        gDialogManagerState.globals.paramText[2][0] = 0;
    }

    if (param3) {
        memcpy(gDialogManagerState.globals.paramText[3], param3, param3[0] + 1);
    } else {
        gDialogManagerState.globals.paramText[3][0] = 0;
    }

    printf("Set parameter text: param0='%.*s', param1='%.*s', param2='%.*s', param3='%.*s'\n",
           gDialogManagerState.globals.paramText[0][0], &gDialogManagerState.globals.paramText[0][1],
           gDialogManagerState.globals.paramText[1][0], &gDialogManagerState.globals.paramText[1][1],
           gDialogManagerState.globals.paramText[2][0], &gDialogManagerState.globals.paramText[2][1],
           gDialogManagerState.globals.paramText[3][0], &gDialogManagerState.globals.paramText[3][1]);
}

/*
 * Extended API implementations
 */

void DialogManager_SetNativeDialogEnabled(bool enabled)
{
    if (gDialogManagerInitialized) {
        gDialogManagerState.useNativeDialogs = enabled;
    }
}

bool DialogManager_GetNativeDialogEnabled(void)
{
    return gDialogManagerInitialized ? gDialogManagerState.useNativeDialogs : false;
}

void DialogManager_SetAccessibilityEnabled(bool enabled)
{
    if (gDialogManagerInitialized) {
        gDialogManagerState.useAccessibility = enabled;
    }
}

bool DialogManager_GetAccessibilityEnabled(void)
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

static DialogPtr CreateDialogStructure(void* storage, bool isColor)
{
    DialogPtr dialog;

    if (storage) {
        /* Use provided storage */
        dialog = (DialogPtr)storage;
    } else {
        /* Allocate new storage */
        dialog = (DialogPtr)malloc(sizeof(DialogRecord));
        if (!dialog) {
            return NULL;
        }
    }

    /* Initialize the structure */
    memset(dialog, 0, sizeof(DialogRecord));

    return dialog;
}

static void InitializeDialogRecord(DialogPtr dialog, const Rect* bounds,
                                   const unsigned char* title, bool visible,
                                   int16_t procID, WindowPtr behind, bool goAway,
                                   int32_t refCon, Handle itemList)
{
    /* This function would initialize the dialog record with the given parameters */
    /* For now, we'll just set the basic fields that we've defined */

    dialog->items = itemList;
    dialog->textH = NULL;
    dialog->editField = -1;
    dialog->editOpen = 0;
    dialog->aDefItem = 1;
}

static void DisposeDialogStructure(DialogPtr dialog, bool closeOnly)
{
    if (!dialog) {
        return;
    }

    /* Dispose of dialog items if not just closing */
    if (!closeOnly && dialog->items) {
        DisposeDialogItemList(dialog->items);
        dialog->items = NULL;
    }

    /* Dispose of TextEdit record if allocated */
    if (!closeOnly && dialog->textH) {
        /* TEDispose(dialog->textH); - would need TextEdit integration */
        dialog->textH = NULL;
    }

    /* Dispose of the underlying window */
    if (!closeOnly) {
        DisposeWindow((WindowPtr)dialog);
    }

    /* Free the dialog structure if it was allocated */
    if (!closeOnly) {
        free(dialog);
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
    dialog->aDefItem = 1; /* Default button is typically item 1 */
    dialog->editField = -1; /* No edit field active initially */
    dialog->editOpen = 0;
}

static void PlaySystemBeep(int16_t soundType)
{
    /* Play appropriate system sound */
    if (gDialogManagerState.globals.soundProc) {
        gDialogManagerState.globals.soundProc(soundType);
    } else {
        /* Default system beep */
        printf("BEEP (sound type %d)\n", soundType);
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

    return theDialog->items;
}

void SetDialogItemList(DialogPtr theDialog, Handle itemList)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return;
    }

    theDialog->items = itemList;
}

int16_t GetDialogDefaultItem(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return 0;
    }

    return theDialog->aDefItem;
}

int16_t GetDialogCancelItem(DialogPtr theDialog)
{
    /* For now, assume cancel is always item 2 */
    /* A full implementation would track this per dialog */
    return 2;
}

bool GetDialogTracksCursor(DialogPtr theDialog)
{
    if (!theDialog || ValidateDialogPtr(theDialog) != 0) {
        return false;
    }

    return gDialogManagerState.globals.tracksCursor;
}

bool IsModalDialog(DialogPtr theDialog)
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

bool IsDialogVisible(DialogPtr theDialog)
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
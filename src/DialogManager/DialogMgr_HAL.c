/*
 * Dialog Manager Hardware Abstraction Layer
 * Bridges classic Mac OS Dialog Manager to modern platforms
 * Integrates with Window, Control, QuickDraw, and Event Managers
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "DialogManager/dialog_manager_core.h"
#include "DialogManager/dialog_manager_dispatch.h"
#include "DialogManager/dialog_manager_private.h"
#include "MemoryMgr/memory_manager.h"
#include "ResourceMgr/resource_manager.h"
#include "WindowManager/window_manager.h"
#include "ControlManager/control_manager.h"
#include "QuickDraw/QuickDraw.h"
#include "EventManager/event_manager.h"
#include "TextEdit/TextEdit.h"

/* Dialog Manager globals */
static struct {
    DialogPtr       frontDialog;        /* Front-most dialog */
    DialogPtr       modalDialog;        /* Current modal dialog */
    int16_t         alertStage;         /* Alert stage (0-3) */
    Handle          alertTemplate;      /* Current alert template */
    TEHandle        dialogTE;           /* TextEdit handle for dialogs */
    ModalFilterUPP  defaultFilter;      /* Default modal filter */
    SoundUPP        alertSound;         /* Alert sound procedure */
    pthread_mutex_t dialogLock;         /* Thread safety */
    bool            initialized;

    /* Dialog item tracking */
    int16_t         lastHit;           /* Last item hit */
    int16_t         defaultButton;     /* Default button ID */
    int16_t         cancelButton;      /* Cancel button ID */

    /* Platform-specific */
    void*           platformData;
} gDialogMgr = {0};

/* Alert stage table - evidence from StartAlert.a */
static const int16_t kAlertStages[4][4] = {
    {1, 0, 0, 0},  /* Stop - beep once */
    {0, 0, 0, 0},  /* Note - no beep */
    {2, 0, 0, 0},  /* Caution - beep twice */
    {3, 0, 0, 0}   /* Alert - beep three times */
};

/* Dialog item types - evidence from DIALOGS.a */
#define ctrlItem        0x00    /* Control */
#define btnCtrl         0x04    /* Button control */
#define chkCtrl         0x05    /* Checkbox control */
#define radCtrl         0x06    /* Radio button control */
#define resCtrl         0x07    /* Resource control */
#define statText        0x08    /* Static text */
#define editText        0x10    /* Editable text */
#define iconItem        0x20    /* Icon */
#define picItem         0x40    /* Picture */
#define userItem        0x80    /* User-defined item */
#define disabled        0x80    /* Item disabled flag */

/* Dialog item structure */
typedef struct DialogItem {
    Handle      handle;         /* Item handle */
    Rect        bounds;         /* Item rectangle */
    uint8_t     type;          /* Item type */
    uint8_t     length;        /* Data length */
    union {
        Str255      text;       /* Text for static/edit */
        int16_t     resID;      /* Resource ID for icons/pics */
        ControlHandle control;  /* Control handle */
        UserItemUPP userProc;   /* User item procedure */
    } data;
} DialogItem, *DialogItemPtr;

/* Forward declarations */
static OSErr DialogMgr_HAL_CreateDialogItems(DialogPtr dialog, Handle itemList);
static void DialogMgr_HAL_DrawDialogItems(DialogPtr dialog);
static int16_t DialogMgr_HAL_TrackDialogItem(DialogPtr dialog, Point where);
static void DialogMgr_HAL_UpdateDefaultButton(DialogPtr dialog);

/* Initialize Dialog Manager HAL */
OSErr DialogMgr_HAL_Initialize(void)
{
    if (gDialogMgr.initialized) {
        return noErr;
    }

    /* Initialize mutex */
    pthread_mutex_init(&gDialogMgr.dialogLock, NULL);

    /* Create default TextEdit for dialogs */
    gDialogMgr.dialogTE = TENew(&(Rect){0, 0, 100, 100}, &(Rect){0, 0, 100, 100});

    /* Initialize alert stage */
    gDialogMgr.alertStage = 0;

    /* Set default buttons */
    gDialogMgr.defaultButton = 1;
    gDialogMgr.cancelButton = 2;

    gDialogMgr.initialized = true;
    return noErr;
}

/* Terminate Dialog Manager HAL */
void DialogMgr_HAL_Terminate(void)
{
    if (!gDialogMgr.initialized) {
        return;
    }

    /* Dispose TextEdit */
    if (gDialogMgr.dialogTE) {
        TEDispose(gDialogMgr.dialogTE);
    }

    /* Close any open dialogs */
    while (gDialogMgr.frontDialog) {
        CloseDialog(gDialogMgr.frontDialog);
    }

    pthread_mutex_destroy(&gDialogMgr.dialogLock);
    gDialogMgr.initialized = false;
}

/* Create new dialog - integrates with Window Manager */
DialogPtr DialogMgr_HAL_NewDialog(void* storage, const Rect* bounds,
                                  ConstStr255Param title, Boolean visible,
                                  int16_t procID, WindowPtr behind,
                                  Boolean goAwayFlag, int32_t refCon,
                                  Handle items)
{
    DialogPtr dialog;
    DialogPeek dPeek;

    pthread_mutex_lock(&gDialogMgr.dialogLock);

    /* Create window using Window Manager */
    dialog = (DialogPtr)NewWindow(storage, bounds, title, false,
                                  procID, behind, goAwayFlag, refCon);
    if (!dialog) {
        pthread_mutex_unlock(&gDialogMgr.dialogLock);
        return NULL;
    }

    /* Set window kind to dialog */
    dPeek = (DialogPeek)dialog;
    ((WindowPeek)dialog)->windowKind = dialogKind;

    /* Initialize dialog-specific fields */
    dPeek->items = items;
    dPeek->textH = gDialogMgr.dialogTE;
    dPeek->editField = -1;  /* No edit field selected */
    dPeek->aDefItem = gDialogMgr.defaultButton;

    /* Create dialog items */
    if (items) {
        DialogMgr_HAL_CreateDialogItems(dialog, items);
    }

    /* Add to dialog list */
    if (!gDialogMgr.frontDialog) {
        gDialogMgr.frontDialog = dialog;
    }

    /* Make visible if requested */
    if (visible) {
        ShowWindow((WindowPtr)dialog);
    }

    pthread_mutex_unlock(&gDialogMgr.dialogLock);
    return dialog;
}

/* Get dialog from resource - integrates with Resource Manager */
DialogPtr DialogMgr_HAL_GetNewDialog(int16_t dialogID, void* storage,
                                     WindowPtr behind)
{
    Handle dlogResource;
    Handle ditlResource;
    DialogPtr dialog = NULL;

    /* Load DLOG resource using Resource Manager */
    dlogResource = GetResource('DLOG', dialogID);
    if (!dlogResource || !*dlogResource) {
        return NULL;
    }

    /* Parse DLOG resource structure */
    struct DLOGResource {
        Rect    boundsRect;
        int16_t procID;
        int16_t visible;
        int16_t goAwayFlag;
        int32_t refCon;
        int16_t itemsID;
        Str255  title;
    } *dlogData;

    HLock(dlogResource);
    dlogData = (struct DLOGResource*)*dlogResource;

    /* Load DITL (Dialog Item List) resource */
    ditlResource = GetResource('DITL', dlogData->itemsID);

    /* Create dialog from resource data */
    dialog = DialogMgr_HAL_NewDialog(storage,
                                     &dlogData->boundsRect,
                                     dlogData->title,
                                     dlogData->visible != 0,
                                     dlogData->procID,
                                     behind,
                                     dlogData->goAwayFlag != 0,
                                     dlogData->refCon,
                                     ditlResource);

    HUnlock(dlogResource);
    ReleaseResource(dlogResource);

    return dialog;
}

/* Close dialog */
void DialogMgr_HAL_CloseDialog(DialogPtr dialog)
{
    if (!dialog) return;

    pthread_mutex_lock(&gDialogMgr.dialogLock);

    /* Remove from dialog list */
    if (gDialogMgr.frontDialog == dialog) {
        gDialogMgr.frontDialog = NULL;
    }
    if (gDialogMgr.modalDialog == dialog) {
        gDialogMgr.modalDialog = NULL;
    }

    /* Dispose dialog items */
    DialogPeek dPeek = (DialogPeek)dialog;
    if (dPeek->items) {
        /* Dispose each item's data */
        int16_t itemCount = CountDITL(dialog);
        for (int16_t i = 1; i <= itemCount; i++) {
            int16_t itemType;
            Handle itemHandle;
            Rect itemRect;

            GetDItem(dialog, i, &itemType, &itemHandle, &itemRect);

            if (itemType & ctrlItem) {
                /* Dispose control */
                if (itemHandle) {
                    DisposeControl((ControlHandle)itemHandle);
                }
            }
        }

        /* Dispose item list */
        DisposeHandle(dPeek->items);
    }

    /* Close window using Window Manager */
    CloseWindow((WindowPtr)dialog);

    pthread_mutex_unlock(&gDialogMgr.dialogLock);
}

/* Dispose dialog */
void DialogMgr_HAL_DisposeDialog(DialogPtr dialog)
{
    /* Same as CloseDialog for now */
    DialogMgr_HAL_CloseDialog(dialog);
}

/* Modal dialog - integrates with Event Manager */
void DialogMgr_HAL_ModalDialog(ModalFilterUPP modalFilter, int16_t* itemHit)
{
    EventRecord event;
    Boolean done = false;

    if (!gDialogMgr.modalDialog || !itemHit) {
        return;
    }

    pthread_mutex_lock(&gDialogMgr.dialogLock);

    /* Event loop for modal dialog */
    while (!done) {
        /* Get next event using Event Manager */
        if (GetNextEvent(everyEvent, &event)) {
            /* Use filter proc if provided */
            if (modalFilter) {
                if ((*modalFilter)(gDialogMgr.modalDialog, &event, itemHit)) {
                    done = true;
                    continue;
                }
            }

            /* Handle event */
            switch (event.what) {
                case mouseDown:
                    {
                        Point where = event.where;
                        GlobalToLocal(&where);
                        *itemHit = DialogMgr_HAL_TrackDialogItem(gDialogMgr.modalDialog, where);
                        if (*itemHit > 0) {
                            done = true;
                        }
                    }
                    break;

                case keyDown:
                case autoKey:
                    {
                        char key = event.message & charCodeMask;
                        if (key == '\r' || key == 0x03) {  /* Return or Enter */
                            *itemHit = gDialogMgr.defaultButton;
                            done = true;
                        } else if (key == 0x1B) {  /* Escape */
                            *itemHit = gDialogMgr.cancelButton;
                            done = true;
                        }
                    }
                    break;

                case updateEvt:
                    if ((WindowPtr)event.message == (WindowPtr)gDialogMgr.modalDialog) {
                        BeginUpdate((WindowPtr)gDialogMgr.modalDialog);
                        DialogMgr_HAL_DrawDialogItems(gDialogMgr.modalDialog);
                        EndUpdate((WindowPtr)gDialogMgr.modalDialog);
                    }
                    break;
            }
        } else {
            /* Null event - could call SystemTask */
        }
    }

    pthread_mutex_unlock(&gDialogMgr.dialogLock);
}

/* Alert - integrates with all managers */
int16_t DialogMgr_HAL_Alert(int16_t alertID, ModalFilterUPP modalFilter)
{
    Handle alertResource;
    int16_t itemHit = -1;

    /* Load ALRT resource */
    alertResource = GetResource('ALRT', alertID);
    if (!alertResource || !*alertResource) {
        return -1;
    }

    /* Parse ALRT resource */
    struct ALRTResource {
        Rect    boundsRect;
        int16_t itemsID;
        struct {
            unsigned boldItm4 : 1;
            unsigned boxDrwn4 : 1;
            unsigned sound4 : 2;
            unsigned boldItm3 : 1;
            unsigned boxDrwn3 : 1;
            unsigned sound3 : 2;
            unsigned boldItm2 : 1;
            unsigned boxDrwn2 : 1;
            unsigned sound2 : 2;
            unsigned boldItm1 : 1;
            unsigned boxDrwn1 : 1;
            unsigned sound1 : 2;
        } stages;
    } *alertData;

    HLock(alertResource);
    alertData = (struct ALRTResource*)*alertResource;

    /* Get current alert stage */
    int16_t stage = gDialogMgr.alertStage;
    if (stage > 3) stage = 3;

    /* Play alert sound based on stage */
    int16_t soundCount = 0;
    if (stage == 0) soundCount = (alertData->stages.sound1);
    else if (stage == 1) soundCount = (alertData->stages.sound2);
    else if (stage == 2) soundCount = (alertData->stages.sound3);
    else soundCount = (alertData->stages.sound4);

    for (int16_t i = 0; i < soundCount; i++) {
        SysBeep(30);  /* System beep */
    }

    /* Load DITL resource */
    Handle ditlResource = GetResource('DITL', alertData->itemsID);

    /* Create alert dialog */
    DialogPtr alertDialog = DialogMgr_HAL_NewDialog(NULL,
                                                    &alertData->boundsRect,
                                                    "\p",  /* No title */
                                                    true,  /* Visible */
                                                    dBoxProc,  /* Alert window type */
                                                    (WindowPtr)-1,  /* Front */
                                                    false,  /* No go-away */
                                                    0,
                                                    ditlResource);

    if (alertDialog) {
        /* Set as modal dialog */
        gDialogMgr.modalDialog = alertDialog;

        /* Update default button based on stage */
        if (stage == 0 && (alertData->stages.boldItm1)) {
            gDialogMgr.defaultButton = 1;
        }

        /* Draw alert */
        ShowWindow((WindowPtr)alertDialog);
        DialogMgr_HAL_UpdateDefaultButton(alertDialog);

        /* Run modal loop */
        DialogMgr_HAL_ModalDialog(modalFilter, &itemHit);

        /* Clean up */
        DialogMgr_HAL_DisposeDialog(alertDialog);
    }

    /* Increment alert stage for next time */
    gDialogMgr.alertStage++;

    HUnlock(alertResource);
    ReleaseResource(alertResource);

    return itemHit;
}

/* Stop alert */
int16_t DialogMgr_HAL_StopAlert(int16_t alertID, ModalFilterUPP modalFilter)
{
    gDialogMgr.alertStage = 0;  /* Stop stage */
    return DialogMgr_HAL_Alert(alertID, modalFilter);
}

/* Note alert */
int16_t DialogMgr_HAL_NoteAlert(int16_t alertID, ModalFilterUPP modalFilter)
{
    gDialogMgr.alertStage = 1;  /* Note stage */
    return DialogMgr_HAL_Alert(alertID, modalFilter);
}

/* Caution alert */
int16_t DialogMgr_HAL_CautionAlert(int16_t alertID, ModalFilterUPP modalFilter)
{
    gDialogMgr.alertStage = 2;  /* Caution stage */
    return DialogMgr_HAL_Alert(alertID, modalFilter);
}

/* Get dialog item */
void DialogMgr_HAL_GetDItem(DialogPtr dialog, int16_t itemNo,
                           int16_t* itemType, Handle* itemHandle, Rect* itemBox)
{
    if (!dialog || itemNo <= 0) return;

    DialogPeek dPeek = (DialogPeek)dialog;
    if (!dPeek->items) return;

    /* Parse item list to find item */
    /* DITL format: count, then items */
    /* Each item: handle, bounds, type, data */

    /* Simplified - would parse actual DITL structure */
    if (itemType) *itemType = statText;
    if (itemHandle) *itemHandle = NULL;
    if (itemBox) SetRect(itemBox, 10, 10 + (itemNo-1)*20, 200, 30 + (itemNo-1)*20);
}

/* Set dialog item */
void DialogMgr_HAL_SetDItem(DialogPtr dialog, int16_t itemNo,
                           int16_t itemType, Handle itemHandle, const Rect* itemBox)
{
    if (!dialog || itemNo <= 0) return;

    /* Would update item in dialog's item list */
}

/* Set dialog item text */
void DialogMgr_HAL_SetIText(Handle itemHandle, ConstStr255Param text)
{
    if (!itemHandle || !text) return;

    /* Copy text to item handle */
    Size textLen = text[0] + 1;
    SetHandleSize(itemHandle, textLen);
    if (*itemHandle) {
        BlockMoveData(text, *itemHandle, textLen);
    }
}

/* Get dialog item text */
void DialogMgr_HAL_GetIText(Handle itemHandle, Str255 text)
{
    if (!itemHandle || !text) return;

    Size handleSize = GetHandleSize(itemHandle);
    if (handleSize > 0 && *itemHandle) {
        BlockMoveData(*itemHandle, text, MIN(handleSize, 256));
    } else {
        text[0] = 0;
    }
}

/* Select dialog item text */
void DialogMgr_HAL_SelIText(DialogPtr dialog, int16_t itemNo,
                           int16_t strtSel, int16_t endSel)
{
    if (!dialog) return;

    DialogPeek dPeek = (DialogPeek)dialog;

    /* Set edit field */
    dPeek->editField = itemNo;

    /* Select text in TextEdit record */
    if (dPeek->textH) {
        TESetSelect(strtSel, endSel, dPeek->textH);
    }
}

/* Draw dialog */
void DialogMgr_HAL_DrawDialog(DialogPtr dialog)
{
    if (!dialog) return;

    /* Draw all dialog items */
    DialogMgr_HAL_DrawDialogItems(dialog);

    /* Update default button outline */
    DialogMgr_HAL_UpdateDefaultButton(dialog);
}

/* Update dialog */
void DialogMgr_HAL_UpdateDialog(DialogPtr dialog, RgnHandle updateRgn)
{
    if (!dialog) return;

    /* Set update region and draw */
    BeginUpdate((WindowPtr)dialog);

    if (updateRgn) {
        /* Clip to update region */
        SetClip(updateRgn);
    }

    DialogMgr_HAL_DrawDialog(dialog);

    EndUpdate((WindowPtr)dialog);
}

/* Dialog select - handle dialog events */
Boolean DialogMgr_HAL_DialogSelect(const EventRecord* theEvent,
                                  DialogPtr* theDialog, int16_t* itemHit)
{
    WindowPtr window;
    int16_t partCode;

    if (!theEvent || !theDialog || !itemHit) return false;

    switch (theEvent->what) {
        case mouseDown:
            /* Find which window */
            partCode = FindWindow(theEvent->where, &window);

            /* Check if it's a dialog */
            if (window && ((WindowPeek)window)->windowKind == dialogKind) {
                *theDialog = (DialogPtr)window;

                if (partCode == inContent) {
                    Point where = theEvent->where;
                    SetPort(window);
                    GlobalToLocal(&where);
                    *itemHit = DialogMgr_HAL_TrackDialogItem(*theDialog, where);
                    return (*itemHit > 0);
                }
            }
            break;

        case keyDown:
        case autoKey:
            /* Check front window is dialog */
            window = FrontWindow();
            if (window && ((WindowPeek)window)->windowKind == dialogKind) {
                *theDialog = (DialogPtr)window;
                /* Handle key in dialog */
                return true;
            }
            break;

        case updateEvt:
            window = (WindowPtr)theEvent->message;
            if (window && ((WindowPeek)window)->windowKind == dialogKind) {
                *theDialog = (DialogPtr)window;
                DialogMgr_HAL_UpdateDialog(*theDialog, NULL);
                return true;
            }
            break;
    }

    return false;
}

/* Is dialog event */
Boolean DialogMgr_HAL_IsDialogEvent(const EventRecord* theEvent)
{
    DialogPtr dialog;
    int16_t itemHit;

    return DialogMgr_HAL_DialogSelect(theEvent, &dialog, &itemHit);
}

/* Count dialog items */
int16_t DialogMgr_HAL_CountDITL(DialogPtr dialog)
{
    if (!dialog) return 0;

    DialogPeek dPeek = (DialogPeek)dialog;
    if (!dPeek->items || !*dPeek->items) return 0;

    /* First word of DITL is count minus 1 */
    int16_t count = *((int16_t*)*dPeek->items) + 1;
    return count;
}

/* === Internal Helper Functions === */

/* Create dialog items from DITL */
static OSErr DialogMgr_HAL_CreateDialogItems(DialogPtr dialog, Handle itemList)
{
    if (!dialog || !itemList || !*itemList) return paramErr;

    /* Parse DITL format */
    int16_t itemCount = *((int16_t*)*itemList) + 1;
    uint8_t* itemPtr = (uint8_t*)*itemList + sizeof(int16_t);

    for (int16_t i = 0; i < itemCount; i++) {
        /* Parse item structure */
        /* Format: placeholder(4), bounds(8), type(1), data(variable) */

        /* Skip placeholder */
        itemPtr += 4;

        /* Get bounds */
        Rect* bounds = (Rect*)itemPtr;
        itemPtr += sizeof(Rect);

        /* Get type */
        uint8_t itemType = *itemPtr++;

        /* Handle based on type */
        if (itemType & ctrlItem) {
            /* Create control using Control Manager */
            int16_t ctrlType = itemType & 0x7F;
            ControlHandle control = NULL;

            switch (ctrlType) {
                case btnCtrl:
                    /* Button control */
                    control = NewControl((WindowPtr)dialog, bounds,
                                        "\pOK", true, 0, 0, 1,
                                        pushButProc, 0);
                    break;

                case chkCtrl:
                    /* Checkbox control */
                    control = NewControl((WindowPtr)dialog, bounds,
                                        "\p", true, 0, 0, 1,
                                        checkBoxProc, 0);
                    break;

                case radCtrl:
                    /* Radio button control */
                    control = NewControl((WindowPtr)dialog, bounds,
                                        "\p", true, 0, 0, 1,
                                        radioButProc, 0);
                    break;
            }

            /* Skip control data */
            uint8_t dataLen = *itemPtr++;
            itemPtr += dataLen;

        } else if (itemType == statText || itemType == editText) {
            /* Text item */
            uint8_t textLen = *itemPtr++;
            /* Skip text data */
            itemPtr += textLen;

        } else if (itemType == iconItem || itemType == picItem) {
            /* Resource item */
            /* Skip resource ID */
            itemPtr += 2;
        }

        /* Align to word boundary */
        if ((itemPtr - (uint8_t*)*itemList) & 1) {
            itemPtr++;
        }
    }

    return noErr;
}

/* Draw all dialog items */
static void DialogMgr_HAL_DrawDialogItems(DialogPtr dialog)
{
    if (!dialog) return;

    int16_t itemCount = CountDITL(dialog);

    for (int16_t i = 1; i <= itemCount; i++) {
        int16_t itemType;
        Handle itemHandle;
        Rect itemBox;

        GetDItem(dialog, i, &itemType, &itemHandle, &itemBox);

        /* Draw based on type */
        if (itemType & ctrlItem) {
            /* Draw control */
            if (itemHandle) {
                Draw1Control((ControlHandle)itemHandle);
            }
        } else if (itemType == statText) {
            /* Draw static text */
            if (itemHandle && *itemHandle) {
                MoveTo(itemBox.left, itemBox.bottom - 4);
                DrawString((ConstStr255Param)*itemHandle);
            }
        } else if (itemType == editText) {
            /* Draw edit text with frame */
            FrameRect(&itemBox);
            if (itemHandle && *itemHandle) {
                MoveTo(itemBox.left + 2, itemBox.bottom - 4);
                DrawString((ConstStr255Param)*itemHandle);
            }
        }
    }
}

/* Track click in dialog item */
static int16_t DialogMgr_HAL_TrackDialogItem(DialogPtr dialog, Point where)
{
    if (!dialog) return 0;

    int16_t itemCount = CountDITL(dialog);

    for (int16_t i = 1; i <= itemCount; i++) {
        int16_t itemType;
        Handle itemHandle;
        Rect itemBox;

        GetDItem(dialog, i, &itemType, &itemHandle, &itemBox);

        if (PtInRect(where, &itemBox)) {
            /* Found item */
            if (itemType & ctrlItem) {
                /* Track control */
                if (itemHandle) {
                    int16_t partCode = TrackControl((ControlHandle)itemHandle,
                                                   where, NULL);
                    if (partCode) {
                        return i;
                    }
                }
            } else if (!(itemType & disabled)) {
                /* Non-control item */
                return i;
            }
        }
    }

    return 0;  /* No item hit */
}

/* Update default button outline */
static void DialogMgr_HAL_UpdateDefaultButton(DialogPtr dialog)
{
    if (!dialog) return;

    DialogPeek dPeek = (DialogPeek)dialog;
    if (dPeek->aDefItem <= 0) return;

    /* Get default button */
    int16_t itemType;
    Handle itemHandle;
    Rect itemBox;

    GetDItem(dialog, dPeek->aDefItem, &itemType, &itemHandle, &itemBox);

    if (itemType & btnCtrl) {
        /* Draw default button outline */
        PenSize(3, 3);
        InsetRect(&itemBox, -4, -4);
        FrameRoundRect(&itemBox, 16, 16);
        PenNormal();
    }
}

/* Dispatch implementation */
OSErr DialogMgr_HAL_Dispatch(int16_t selector, DialogDispatchParams* params)
{
    OSErr result = noErr;

    if (!params) return paramErr;

    switch (selector) {
        case kGetStdFilterProc:
            /* Return standard filter procedure */
            params->getStdFilterProc.filterProc = NULL;  /* Would return actual UPP */
            break;

        case kSetDialogDefaultItem:
            if (params->setDialogDefaultItem.dialog) {
                ((DialogPeek)params->setDialogDefaultItem.dialog)->aDefItem =
                    params->setDialogDefaultItem.newItem;
            }
            break;

        case kSetDialogCancelItem:
            gDialogMgr.cancelButton = params->setDialogCancelItem.newItem;
            break;

        case kSetDialogTracksCursor:
            /* Set cursor tracking flag */
            break;

        default:
            result = paramErr;
            break;
    }

    return result;
}

/* Initialize on first use */
__attribute__((constructor))
static void DialogMgr_HAL_Init(void)
{
    DialogMgr_HAL_Initialize();
}

/* Cleanup on exit */
__attribute__((destructor))
static void DialogMgr_HAL_Cleanup(void)
{
    DialogMgr_HAL_Terminate();
}
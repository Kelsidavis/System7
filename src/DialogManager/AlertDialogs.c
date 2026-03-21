#include "MemoryMgr/MemoryManager.h"
/*
#include "DialogManager/DialogInternal.h"
 * AlertDialogs.c - Alert Dialog Implementation
 *
 * This module provides the alert dialog functionality faithful to
 * Mac System 7.1, including Alert, StopAlert, NoteAlert, and CautionAlert.
 */

#include <stdlib.h>
#include <string.h>

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "DialogManager/AlertDialogs.h"
#include "SoundManager/SoundEffects.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/ModalDialogs.h"
#include "DialogManager/DialogResources.h"
#include "DialogManager/DialogItems.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlTypes.h"
#include "DialogManager/DialogLogging.h"

/* External dependencies */
extern void SysBeep(SInt16 duration);
extern UInt32 TickCount(void);
extern void ShowWindow(WindowPtr window);
/* NewHandleClear, DisposeHandle, HLock, HUnlock now provided by MemoryManager.h */
extern ControlHandle _GetFirstControl(WindowPtr window);
extern void CenterDialogOnScreen(DialogPtr dlg);
extern void InvalRect(const Rect* r);

/* Global alert state */
static struct {
    Boolean initialized;
    SInt16 alertStage;
    unsigned char paramText[4][256];
    SInt16 alertSounds[4];  /* Stop, Note, Caution, (reserved) */
    SInt16 alertIcons[4];
    Boolean useNativeAlerts;
    SInt16 alertPosition;
} gAlertState = {0};

/* Built-in fallback alert specifications (System 7-style layout) */
typedef struct {
    Rect bounds;      /* dialog frame in global coords */
    SInt16 defItem;   /* 1-based default button */
    SInt16 cancelItem;/* 1-based cancel button (0=none) */
    SInt16 icon;      /* 0=none, 1=stop, 2=note, 3=caution */
    SInt16 ditlId;    /* pseudo id for fallback DITL */
} BuiltInAlertSpec;

static const BuiltInAlertSpec kFallbackStop    = {{160, 180, 320, 460}, 3, 0, 1, 9001};
static const BuiltInAlertSpec kFallbackNote    = {{160, 180, 320, 460}, 3, 0, 2, 9002};
static const BuiltInAlertSpec kFallbackCaution = {{160, 180, 320, 460}, 3, 0, 3, 9003};
static const BuiltInAlertSpec kFallbackGeneric = {{160, 180, 320, 460}, 3, 4, 0, 9004};

/* Private function prototypes */
static SInt16 RunAlertDialog(SInt16 alertID, ModalFilterProcPtr filterProc, SInt16 alertType);
static DialogPtr CreateAlertDialogFromTemplate(const AlertTemplate* alertTemplate);
static void PlayAlertSoundForStage(SInt16 alertType, SInt16 stage);
static void __attribute__((unused)) PositionAlertDialog(DialogPtr alertDialog);
/* Forward declarations */
static void __attribute__((unused)) DrawAlertIcon(DialogPtr alertDialog, SInt16 iconType);
static OSErr BuildFallbackDLOG(const BuiltInAlertSpec* spec, DialogTemplate** outDLOG);
static OSErr BuildFallbackDITL(SInt16 pseudoId, SInt16 iconKind, Handle* outDITL);
static Boolean LoadAlertWithFallback(SInt16 alertID, SInt16 alertType,
                                     DialogTemplate** outDLOG, Handle* outDITL,
                                     SInt16* outDefItem, SInt16* outCancelItem,
                                     SInt16* outIconKind);

/*
 * InitAlertDialogs - Initialize alert dialog subsystem
 */
void InitAlertDialogs(void)
{
    int i;

    if (gAlertState.initialized) {
        return;
    }

    memset(&gAlertState, 0, sizeof(gAlertState));
    gAlertState.initialized = true;
    gAlertState.alertStage = 0;
    gAlertState.useNativeAlerts = false;
    gAlertState.alertPosition = 0; /* Center on main screen */

    /* Clear parameter text */
    for (i = 0; i < 4; i++) {
        gAlertState.paramText[i][0] = 0;
    }

    /* Set default alert sounds (0 = system beep) */
    gAlertState.alertSounds[0] = 0; /* Stop */
    gAlertState.alertSounds[1] = 0; /* Note */
    gAlertState.alertSounds[2] = 0; /* Caution */
    gAlertState.alertSounds[3] = 0; /* Reserved */

    /* Set default alert icons */
    gAlertState.alertIcons[0] = 0; /* Stop icon */
    gAlertState.alertIcons[1] = 1; /* Note icon */
    gAlertState.alertIcons[2] = 2; /* Caution icon */
    gAlertState.alertIcons[3] = 0; /* Reserved */

    // printf("Alert dialog subsystem initialized\n");
}

/*
 * Alert - Display a generic alert dialog
 */
SInt16 Alert(SInt16 alertID, ModalFilterProcPtr filterProc)
{
    return RunAlertDialog(alertID, filterProc, 0);
}

/*
 * StopAlert - Display a stop alert dialog
 */
SInt16 StopAlert(SInt16 alertID, ModalFilterProcPtr filterProc)
{
    return RunAlertDialog(alertID, filterProc, 0);
}

/*
 * NoteAlert - Display a note alert dialog
 */
SInt16 NoteAlert(SInt16 alertID, ModalFilterProcPtr filterProc)
{
    return RunAlertDialog(alertID, filterProc, 1);
}

/*
 * CautionAlert - Display a caution alert dialog
 */
SInt16 CautionAlert(SInt16 alertID, ModalFilterProcPtr filterProc)
{
    return RunAlertDialog(alertID, filterProc, 2);
}

/*
 * GetAlertStage - Get current alert stage
 */
SInt16 GetAlertStage(void)
{
    if (!gAlertState.initialized) {
        return 0;
    }
    return gAlertState.alertStage;
}

/*
 * ResetAlertStage - Reset alert stage to 0
 */
void ResetAlertStage(void)
{
    if (!gAlertState.initialized) {
        return;
    }
    gAlertState.alertStage = 0;
    // printf("Alert stage reset to 0\n");
}

/*
 * SetAlertStage - Set current alert stage
 */
void SetAlertStage(SInt16 stage)
{
    if (!gAlertState.initialized) {
        return;
    }
    if (stage >= 0 && stage <= 3) {
        gAlertState.alertStage = stage;
    // printf("Alert stage set to %d\n", stage);
    }
}

/*
 * GetParamText - Get current parameter text
 */
void GetParamText(SInt16 paramIndex, unsigned char* text)
{
    if (!gAlertState.initialized || !text || paramIndex < 0 || paramIndex > 3) {
        if (text) text[0] = 0;
        return;
    }

    memcpy(text, gAlertState.paramText[paramIndex],
           gAlertState.paramText[paramIndex][0] + 1);
}

/*
 * ClearParamText - Clear all parameter text
 */
void ClearParamText(void)
{
    int i;

    if (!gAlertState.initialized) {
        return;
    }

    for (i = 0; i < 4; i++) {
        gAlertState.paramText[i][0] = 0;
    }
}

/*
 * SetAlertSound - Set sound for alert type
 */
void SetAlertSound(SInt16 alertType, SInt16 soundID)
{
    if (!gAlertState.initialized || alertType < 0 || alertType > 3) {
        return;
    }
    gAlertState.alertSounds[alertType] = soundID;
}

/*
 * GetAlertSound - Get sound for alert type
 */
SInt16 GetAlertSound(SInt16 alertType)
{
    if (!gAlertState.initialized || alertType < 0 || alertType > 3) {
        return 0;
    }
    return gAlertState.alertSounds[alertType];
}

/*
 * SetAlertPosition - Set position for alert dialogs
 */
void SetAlertPosition(SInt16 position)
{
    if (!gAlertState.initialized) {
        return;
    }
    gAlertState.alertPosition = position;
}

/*
 * SetAlertIcon - Set custom icon for alert type
 */
void SetAlertIcon(SInt16 alertType, SInt16 iconID)
{
    if (!gAlertState.initialized || alertType < 0 || alertType > 3) {
        return;
    }
    gAlertState.alertIcons[alertType] = iconID;
}

/*
 * GetAlertIcon - Get icon for alert type
 */
SInt16 GetAlertIcon(SInt16 alertType)
{
    if (!gAlertState.initialized || alertType < 0 || alertType > 3) {
        return 0;
    }
    return gAlertState.alertIcons[alertType];
}

/*
 * SetUseNativeAlerts - Enable/disable native alert dialogs
 */
void SetUseNativeAlerts(Boolean useNative)
{
    if (!gAlertState.initialized) {
        return;
    }
    gAlertState.useNativeAlerts = useNative;
}

/*
 * GetUseNativeAlerts - Check if native alerts are enabled
 */
Boolean GetUseNativeAlerts(void)
{
    return gAlertState.initialized ? gAlertState.useNativeAlerts : false;
}

/*
 * CreateAlertFromTemplate - Create alert from template
 */
DialogPtr CreateAlertFromTemplate(SInt16 alertID)
{
    AlertTemplate* alertTemplate = NULL;
    DialogPtr alertDialog = NULL;
    OSErr err;

    if (!gAlertState.initialized) {
    // printf("Error: Alert subsystem not initialized\n");
        return NULL;
    }

    /* Load alert template */
    err = LoadAlertTemplate(alertID, &alertTemplate);
    if (err != noErr || !alertTemplate) {
    // printf("Error: Failed to load ALRT resource %d (error %d)\n", alertID, err);
        return NULL;
    }

    /* Create dialog from template */
    alertDialog = CreateAlertDialogFromTemplate(alertTemplate);

    /* Dispose template */
    DisposeAlertTemplate(alertTemplate);

    return alertDialog;
}

/*
 * RunAlert - Run an alert dialog
 */
SInt16 RunAlert(DialogPtr alertDialog, ModalFilterProcPtr filterProc)
{
    SInt16 itemHit = 0;

    if (!alertDialog) {
        return 1; /* Default to OK */
    }

    /* Make dialog modal */
    BeginModalDialog(alertDialog);

    /* Prime initial keyboard focus (prefer default button) */
    DM_FocusNextControl((WindowPtr)alertDialog, false);

    /* Run modal dialog loop */
    ModalDialog(filterProc, &itemHit);

    /* End modal processing */
    EndModalDialog(alertDialog);

    return itemHit;
}

/*
 * PlayAlertSound - Play sound for alert
 */
void PlayAlertSound(SInt16 alertType, SInt16 stage)
{
    if (!gAlertState.initialized) {
        return;
    }

    PlayAlertSoundForStage(alertType, stage);
}

/*
 * CleanupAlertDialogs - Cleanup alert subsystem
 */
void CleanupAlertDialogs(void)
{
    if (!gAlertState.initialized) {
        return;
    }

    gAlertState.initialized = false;
    // printf("Alert dialog subsystem cleaned up\n");
}

/*
 * Private implementation functions
 */

static OSErr BuildFallbackDLOG(const BuiltInAlertSpec* spec, DialogTemplate** outDLOG)
{
    DialogTemplate* t;

    if (!spec || !outDLOG) {
        return -50; /* paramErr */
    }

    t = (DialogTemplate*)NewPtr(sizeof(DialogTemplate));
    if (!t) {
        return -108; /* memFullErr */
    }

    memset(t, 0, sizeof(DialogTemplate));
    t->boundsRect = spec->bounds;  /* already global; DM will position/center */
    t->procID     = 1;             /* modal */
    t->visible    = false;
    t->goAwayFlag = false;
    t->refCon     = spec->icon;    /* Store icon kind in refCon for drawing */
    t->itemsID    = spec->ditlId;
    t->title[0]   = 5;             /* Pascal string length */
    memcpy(&t->title[1], "Alert", 5);

    *outDLOG = t;
    return noErr;
}

static OSErr BuildFallbackDITL(SInt16 pseudoId, SInt16 iconKind, Handle* outDITL)
{
    SInt16 n;
    unsigned char* p;
    Handle h;
    SInt16 i;
    static const unsigned char okText[] = "\002OK";
    static const unsigned char cancelText[] = "\006Cancel";
    static const unsigned char msgText[] = "\034Alert message will appear here.";

    if (!outDITL) {
        return -50; /* paramErr */
    }

    /* Determine item count: icon(1), text(2), OK(3), Cancel(4 if generic) */
    n = (pseudoId == 9004) ? 4 : 3;

    /* Build minimal DITL in binary format that ParseDITL can read */
    /* Format: count-1 (2 bytes), then for each item: placeholder(4), rect(8), type(1), data */
    h = NewHandleClear(1024);  /* Generous size for small DITL */
    if (!h) {
        return -108; /* memFullErr */
    }

    HLock(h);
    p = (unsigned char*)*h;

    /* Write count-1 */
    *p++ = 0;
    *p++ = (unsigned char)(n - 1);

    /* Item 1: Icon user item at left */
    /* Placeholder (4 bytes) */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    /* Bounds: top, left, bottom, right (each 2 bytes) */
    *p++ = 0; *p++ = 20;  /* top */
    *p++ = 0; *p++ = 20;  /* left */
    *p++ = 0; *p++ = 52;  /* bottom */
    *p++ = 0; *p++ = 52;  /* right */
    /* Type: userItem (0) + itemDisable (128) */
    *p++ = 0;  /* userItem */
    /* Data length */
    *p++ = 0;  /* No data for user item */

    /* Item 2: Static text */
    /* Placeholder */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    /* Bounds */
    *p++ = 0; *p++ = 20;  /* top */
    *p++ = 0; *p++ = 60;  /* left */
    *p++ = 0; *p++ = 96;  /* bottom */
    *p++ = 1; *p++ = 4;   /* right (260) */
    /* Type: statText (8) */
    *p++ = 8;
    /* Data: Pascal string */
    *p++ = msgText[0];  /* length */
    for (i = 1; i <= msgText[0]; i++) {
        *p++ = msgText[i];
    }

    /* Item 3: OK button */
    /* Placeholder */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    /* Bounds: bottom-right, 80×20 button */
    *p++ = 0; *p++ = 96;   /* top */
    *p++ = 0; *p++ = 180;  /* left (260-80) */
    *p++ = 0; *p++ = 116;  /* bottom */
    *p++ = 0; *p++ = 240;  /* right (260-20) */
    /* Type: ctrlItem + btnCtrl (4+0) */
    *p++ = 4;
    /* Data: Pascal string */
    *p++ = okText[0];
    for (i = 1; i <= okText[0]; i++) {
        *p++ = okText[i];
    }

    if (n == 4) {
        /* Item 4: Cancel button */
        /* Placeholder */
        *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
        /* Bounds */
        *p++ = 0; *p++ = 96;   /* top */
        *p++ = 0; *p++ = 100;  /* left (260-160) */
        *p++ = 0; *p++ = 116;  /* bottom */
        *p++ = 0; *p++ = 160;  /* right (260-100) */
        /* Type: ctrlItem + btnCtrl */
        *p++ = 4;
        /* Data: Pascal string */
        *p++ = cancelText[0];
        for (i = 1; i <= cancelText[0]; i++) {
            *p++ = cancelText[i];
        }
    }

    HUnlock(h);
    *outDITL = h;
    return noErr;
}

static Boolean LoadAlertWithFallback(SInt16 alertID, SInt16 alertType,
                                     DialogTemplate** outDLOG, Handle* outDITL,
                                     SInt16* outDefItem, SInt16* outCancelItem,
                                     SInt16* outIconKind)
{
    const BuiltInAlertSpec* spec;
    OSErr err;

    if (!outDLOG || !outDITL || !outDefItem || !outCancelItem || !outIconKind) {
        return false;
    }

    /* Map alert ID to fallback spec */
    /* Standard alert IDs: 128=generic, 129=stop, 130=note, 131=caution */
    if (alertID == 129) {
        spec = &kFallbackStop;
    } else if (alertID == 130) {
        spec = &kFallbackNote;
    } else if (alertID == 131) {
        spec = &kFallbackCaution;
    } else {
        spec = &kFallbackGeneric;
    }

    *outDefItem    = spec->defItem;
    *outCancelItem = spec->cancelItem;
    *outIconKind   = spec->icon;

    /* Build fallback DLOG and DITL */
    err = BuildFallbackDLOG(spec, outDLOG);
    if (err != noErr) {
    // printf("Failed to build fallback DLOG: error %d\n", err);
        return false;
    }

    err = BuildFallbackDITL(spec->ditlId, spec->icon, outDITL);
    if (err != noErr) {
    // printf("Failed to build fallback DITL: error %d\n", err);
        if (*outDLOG) {
            DisposePtr((Ptr)*outDLOG);
            *outDLOG = NULL;
        }
        return false;
    }

    // printf("Using fallback alert template for ID %d (icon=%d, def=%d, cancel=%d)\n", alertID, *outIconKind, *outDefItem, *outCancelItem);

    return true;
}

/*
 * Alert_RealizeButtons - Realize DITL button items into actual Control records
 *
 * Walks the DITL and creates ControlHandles for any ctrlItem+btnCtrl entries
 * that don't have a handle yet. This makes buttons findable by keyboard handlers.
 */
static void Alert_RealizeButtons(DialogPtr d)
{
    SInt16 count, i;
    SInt16 itemType;
    Handle itemHandle;
    Rect r;
    ControlHandle c;
    Str255 title;

    if (!d) {
        return;
    }

    count = CountDITL(d);
    for (i = 1; i <= count; i++) {
        itemType = 0;
        itemHandle = NULL;

        GetDialogItem(d, i, &itemType, &itemHandle, &r);
        // DIALOG_LOG_DEBUG("[ALERT] GetDialogItem returned: item=%d, type=%d, rect=(%d,%d,%d,%d)\n", i, itemType, r.top, r.left, r.bottom, r.right);

        /* Classic Mac encoding: low 7 bits carry the base type */
        if (((itemType & 0x7F) == (ctrlItem + btnCtrl)) && itemHandle == NULL) {
            /* Use default button title (OK) - could be enhanced to parse from DITL */
            title[0] = 2;
            title[1] = 'O';
            title[2] = 'K';

            /* Create the standard push button control */
            // DIALOG_LOG_DEBUG("[ALERT] About to create button control for item %d\n", i);
            c = NewControl(
                (WindowPtr)d,
                &r,
                title,
                true,     /* visible */
                0,        /* value */
                0,        /* min */
                1,        /* max */
                pushButProc,
                0         /* refCon */
            );

            if (c) {
                /* Store handle back into the dialog item */
                SetDialogItem(d, i, itemType, (Handle)c, &r);
                // DIALOG_LOG_DEBUG("[ALERT] Realized button item=%d, rect=(%d,%d,%d,%d)\n", i, r.left, r.top, r.right, r.bottom);

                /* Verify control was linked to window */
                {
                    ControlHandle first = _GetFirstControl((WindowPtr)d);
                    if (first) {
                        // DIALOG_LOG_DEBUG("[ALERT] _GetFirstControl after NewControl: found control\n");
                    } else {
                        // DIALOG_LOG_DEBUG("[ALERT] _GetFirstControl after NewControl: NULL\n");
                    }
                }
            } else {
                // DIALOG_LOG_DEBUG("[ALERT] Failed to realize button item=%d\n", i);
            }
        }
    }
}

static SInt16 RunAlertDialog(SInt16 alertID, ModalFilterProcPtr filterProc, SInt16 alertType)
{
    DialogPtr alertDialog = NULL;
    DialogTemplate* dlogTemplate = NULL;
    Handle ditlHandle = NULL;
    SInt16 itemHit = 1; /* Default to OK button */
    SInt16 defItem = 1, cancelItem = 0, iconKind = 0;
    static const unsigned char alertTitle[] = "\005Alert";

    if (!gAlertState.initialized) {
    // printf("Error: Alert subsystem not initialized\n");
        return 1;
    }

    /* Play alert sound based on current stage */
    PlayAlertSoundForStage(alertType, gAlertState.alertStage);

    /* Load alert with fallback */
    if (!LoadAlertWithFallback(alertID, alertType, &dlogTemplate, &ditlHandle,
                               &defItem, &cancelItem, &iconKind)) {
    // printf("Error: Failed to load/create alert %d\n", alertID);
        SysBeep(30);
        return 1;
    }

    /* Create the dialog window */
    alertDialog = NewDialog(NULL, &dlogTemplate->boundsRect, alertTitle,
                           false, /* Start invisible */
                           1,     /* Modal dialog proc */
                           (WindowPtr)-1, /* Behind all windows */
                           false, /* No close box */
                           iconKind,      /* Store icon kind in refCon */
                           ditlHandle);

    if (!alertDialog) {
    // printf("Error: Failed to create alert dialog\n");
        if (dlogTemplate) DisposePtr((Ptr)dlogTemplate);
        if (ditlHandle) DisposeHandle(ditlHandle);
        return 1;
    }

    /* Set default and cancel items */
    if (defItem) SetDialogDefaultItem(alertDialog, defItem);
    if (cancelItem) SetDialogCancelItem(alertDialog, cancelItem);

    /* Center and show */
    CenterDialogOnScreen(alertDialog);
    ShowWindow((WindowPtr)alertDialog);

    /* Force initial update */
    InvalRect(&((GrafPtr)alertDialog)->portRect);

    /* Realize button controls so keyboard can find them */
    Alert_RealizeButtons(alertDialog);

    /* Make dialog modal and run modal loop */
    BeginModalDialog(alertDialog);

    /* Prime initial keyboard focus to default button */
    if (defItem > 0) {
        SInt16 itemType;
        Handle itemHandle;
        Rect itemRect;
        GetDialogItem(alertDialog, defItem, &itemType, &itemHandle, &itemRect);
        if (itemHandle) {
            DM_SetKeyboardFocus((WindowPtr)alertDialog, (ControlHandle)itemHandle);
        } else {
            /* Fallback to first focusable control */
            DM_FocusNextControl((WindowPtr)alertDialog, false);
        }
    } else {
        /* No default item, focus first focusable control */
        DM_FocusNextControl((WindowPtr)alertDialog, false);
    }

    ModalDialog(filterProc, &itemHit);
    EndModalDialog(alertDialog);

    /* Dispose of the alert dialog */
    DisposeDialog(alertDialog);
    if (dlogTemplate) DisposePtr((Ptr)dlogTemplate);

    /* Advance alert stage for repeated alerts */
    if (gAlertState.alertStage < 3) {
        gAlertState.alertStage++;
    }

    // printf("Alert %d completed, item hit: %d, new stage: %d\n", alertID, itemHit, gAlertState.alertStage);

    return itemHit;
}

static DialogPtr CreateAlertDialogFromTemplate(const AlertTemplate* alertTemplate)
{
    DialogPtr alertDialog = NULL;
    Handle itemList = NULL;
    OSErr err;
    static const unsigned char alertTitle[] = "\005Alert";  /* Pascal string: length byte + "Alert" */

    if (!alertTemplate) {
        return NULL;
    }

    /* Load item list for the alert */
    err = LoadDialogItemList(alertTemplate->itemsID, &itemList);
    if (err != noErr || !itemList) {
    // printf("Error: Failed to load DITL resource %d for alert (error %d)\n", alertTemplate->itemsID, err);
        return NULL;
    }

    /* Create the dialog window */
    /* Alert dialogs are always modal (procID = 1) */
    alertDialog = NewDialog(NULL, &alertTemplate->boundsRect, alertTitle,
                           false, /* Start invisible */
                           1,     /* Modal dialog proc */
                           (WindowPtr)-1, /* Behind all windows */
                           false, /* No close box */
                           0,     /* refCon */
                           itemList);

    if (!alertDialog) {
        DisposeDialogItemList(itemList);
        return NULL;
    }

    return alertDialog;
}

static void PlayAlertSoundForStage(SInt16 alertType, SInt16 stage)
{
    SoundEffectId effect = kSoundEffectBeep;

    if (alertType >= 0 && alertType <= 3) {
        SInt16 soundID = gAlertState.alertSounds[alertType];
        if (soundID == 0) {
            switch (alertType) {
                case 0: effect = kSoundEffectAlertStop; break;
                case 1: effect = kSoundEffectAlertNote; break;
                case 2: effect = kSoundEffectAlertCaution; break;
                default: effect = kSoundEffectBeep; break;
            }
        } else {
            /* Future: map custom sound IDs */
        }
    }

    SoundEffects_Play(effect);
}

static void __attribute__((unused)) PositionAlertDialog(DialogPtr alertDialog)
{
    if (!alertDialog) {
        return;
    }

    /* In System 7, alerts are typically centered on the main screen */
    /* The position can be specified in the ALRT resource:
     * 0x0000 = use bounds as-is
     * 0x300A = center on main screen
     * 0x700A = center on deepest screen
     * 0xB00A = center over frontmost window
     */

    /* For now, we'll just use the bounds from the template */
    /* A full implementation would adjust position based on positioning code */

    // printf("Positioning alert dialog at %p\n", (void*)alertDialog);
}

void SubstituteAlertParameters(unsigned char* text)
{
    if (!text || !gAlertState.initialized) {
        return;
    }

    unsigned char result[256];
    unsigned char textLen = text[0];
    unsigned char resultLen = 0;

    for (unsigned char i = 1; i <= textLen && resultLen < 255; i++) {
        if (text[i] == '^' && i < textLen) {
            /* Check next character */
            unsigned char nextChar = text[i + 1];
            if (nextChar >= '0' && nextChar <= '3') {
                /* Substitute parameter */
                SInt16 paramIndex = nextChar - '0';
                unsigned char paramLen = gAlertState.paramText[paramIndex][0];

                /* Copy parameter text */
                for (unsigned char j = 0; j < paramLen && resultLen < 255; j++) {
                    result[resultLen + 1] = gAlertState.paramText[paramIndex][j + 1];
                    resultLen++;
                }

                /* Skip the caret and digit */
                i++;
                continue;
            }
        }

        /* Copy normal character */
        result[resultLen + 1] = text[i];
        resultLen++;
    }

    /* Update original text with substituted result */
    result[0] = resultLen;
    memcpy(text, result, resultLen + 1);
}

static void __attribute__((unused)) DrawAlertIcon(DialogPtr alertDialog, SInt16 iconType)
{
    if (!alertDialog) {
        return;
    }

    /* In System 7, alerts have standard icons:
     * Stop (0) = stop sign icon (octagon with hand)
     * Note (1) = information icon (speech bubble or note)
     * Caution (2) = exclamation point icon (triangle with !)
     */

    /* Draw icon in standard location (left side of alert, typically 20x20 at 20,20) */
    extern void SetPort(GrafPtr port);
    extern void GetPort(GrafPtr *port);
    extern void PaintRect(const Rect *r);
    extern void FrameRect(const Rect *r);
    extern void FrameOval(const Rect *r);
    extern void MoveTo(SInt16 h, SInt16 v);
    extern void LineTo(SInt16 h, SInt16 v);

    GrafPtr savePort;
    GetPort(&savePort);
    SetPort((GrafPtr)alertDialog);

    Rect iconRect;
    iconRect.left = 20;
    iconRect.top = 20;
    iconRect.right = 52;  /* 32x32 icon */
    iconRect.bottom = 52;

    switch (iconType) {
        case 0:  /* Stop Alert - draw octagonal stop sign */
        {
            /* Draw octagon approximation using rectangle and corners */
            Rect innerRect = iconRect;
            FrameOval(&innerRect);  /* Circle for stop sign */

            /* Draw X or hand symbol inside */
            SInt16 centerH = (iconRect.left + iconRect.right) / 2;
            SInt16 centerV = (iconRect.top + iconRect.bottom) / 2;
            SInt16 offset = 8;

            /* Draw X */
            MoveTo(centerH - offset, centerV - offset);
            LineTo(centerH + offset, centerV + offset);
            MoveTo(centerH + offset, centerV - offset);
            LineTo(centerH - offset, centerV + offset);
            break;
        }

        case 1:  /* Note Alert - draw note/info icon */
        {
            /* Draw circle */
            FrameOval(&iconRect);

            /* Draw lowercase 'i' in center */
            SInt16 centerH = (iconRect.left + iconRect.right) / 2;
            SInt16 topV = iconRect.top + 12;
            SInt16 bottomV = iconRect.bottom - 8;

            /* Dot of 'i' */
            Rect dotRect;
            dotRect.left = centerH - 2;
            dotRect.right = centerH + 2;
            dotRect.top = topV;
            dotRect.bottom = topV + 4;
            PaintRect(&dotRect);

            /* Stem of 'i' */
            MoveTo(centerH, topV + 6);
            LineTo(centerH, bottomV);
            break;
        }

        case 2:  /* Caution Alert - draw triangle with exclamation */
        {
            /* Draw triangle */
            SInt16 topH = (iconRect.left + iconRect.right) / 2;
            SInt16 topV = iconRect.top + 4;
            SInt16 leftH = iconRect.left + 4;
            SInt16 rightH = iconRect.right - 4;
            SInt16 bottomV = iconRect.bottom - 4;

            MoveTo(topH, topV);
            LineTo(leftH, bottomV);
            LineTo(rightH, bottomV);
            LineTo(topH, topV);

            /* Draw exclamation point */
            SInt16 centerH = (iconRect.left + iconRect.right) / 2;
            MoveTo(centerH, topV + 8);
            LineTo(centerH, bottomV - 12);

            /* Dot at bottom */
            Rect dotRect;
            dotRect.left = centerH - 2;
            dotRect.right = centerH + 2;
            dotRect.top = bottomV - 8;
            dotRect.bottom = bottomV - 4;
            PaintRect(&dotRect);
            break;
        }

        default:
            /* Unknown icon type - draw placeholder rectangle */
            FrameRect(&iconRect);
            break;
    }

    SetPort(savePort);
}

/* Stub implementations for additional alert functions */

void SetAlertAccessibility(Boolean enabled)
{
    // printf("SetAlertAccessibility: %d\n", enabled);
}

void AnnounceAlert(const char* title, const char* message)
{
    // printf("AnnounceAlert: %s - %s\n", title ? title : "", message ? message : "");
}

SInt16 ShowNativeAlert(const char* title, const char* message,
                       SInt16 buttons, SInt16 alertType)
{
    // printf("ShowNativeAlert: %s - %s (buttons: %d, type: %d)\n", title ? title : "", message ? message : "", buttons, alertType);
    return 1; /* OK button */
}

void SetAlertTheme(const DialogTheme* theme)
{
    // printf("SetAlertTheme\n");
}

void GetAlertTheme(DialogTheme* theme)
{
    // printf("GetAlertTheme\n");
}

SInt16 ShowAlert(const char* title, const char* message,
                 SInt16 buttons, SInt16 alertType)
{
    // printf("ShowAlert: %s - %s (buttons: %d, type: %d)\n", title ? title : "", message ? message : "", buttons, alertType);
    return 1; /* OK button */
}

SInt16 ShowAlertWithParams(const char* title, const char* message,
                           SInt16 buttons, SInt16 alertType,
                           const char* param0, const char* param1,
                           const char* param2, const char* param3)
{
    // printf("ShowAlertWithParams: %s - %s\n", title ? title : "", message ? message : "");
    return 1; /* OK button */
}

void ProcessAlertStages(SInt16 alertType, SInt16 stage)
{
    // printf("ProcessAlertStages: type=%d, stage=%d\n", alertType, stage);
}

void SubstituteParamText(char* text, size_t textSize)
{
    if (!text || textSize == 0 || !gAlertState.initialized) {
        return;
    }

    char result[512];
    size_t srcIdx = 0;
    size_t dstIdx = 0;
    size_t srcLen = strlen(text);

    while (srcIdx < srcLen && dstIdx < sizeof(result) - 1) {
        if (text[srcIdx] == '^' && srcIdx + 1 < srcLen) {
            char nextChar = text[srcIdx + 1];
            if (nextChar >= '0' && nextChar <= '3') {
                /* Substitute parameter */
                SInt16 paramIndex = nextChar - '0';
                unsigned char paramLen = gAlertState.paramText[paramIndex][0];

                /* Convert Pascal string to C string and copy */
                for (unsigned char i = 0; i < paramLen && dstIdx < sizeof(result) - 1; i++) {
                    result[dstIdx++] = gAlertState.paramText[paramIndex][i + 1];
                }

                /* Skip the caret and digit */
                srcIdx += 2;
                continue;
            }
        }

        /* Copy normal character */
        result[dstIdx++] = text[srcIdx++];
    }

    /* Null terminate and copy back to original buffer */
    result[dstIdx] = '\0';

    size_t copyLen = (dstIdx < textSize - 1) ? dstIdx : textSize - 1;
    memcpy(text, result, copyLen);
    text[copyLen] = '\0';
}

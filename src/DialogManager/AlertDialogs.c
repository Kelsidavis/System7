#include "DialogManager/DITLBuilder.h"
#include "MemoryMgr/MemoryManager.h"
/*
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
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "DialogManager/DialogTypes.h"
#include "DialogManager/ModalDialogs.h"
#include "DialogManager/DialogResources.h"
#include "DialogManager/DialogItems.h"
#include "ControlManager/ControlManager.h"
#include "ControlManager/ControlInternal.h"
#include "ControlManager/ControlTypes.h"
#include "DialogManager/DialogLogging.h"
#include "TimeManager/TimeBase.h"
#include "Resources/ResourceData.h"

/* External dependencies */
extern void SysBeep(SInt16 duration);
/* NewHandleClear, DisposeHandle, HLock, HUnlock now provided by MemoryManager.h */
extern void CenterDialogOnScreen(DialogPtr dlg);

/* Global alert state */
static struct {
    Boolean initialized;
    SInt16 alertStage;
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

/* Items in the standard order: OK is item 1, Cancel (when there is one)
 * item 2, as Inside Macintosh's alerts have them and callers test for. */
static const BuiltInAlertSpec kFallbackStop    = {{160, 180, 320, 460}, 1, 0, 1, 9001};
static const BuiltInAlertSpec kFallbackNote    = {{160, 180, 320, 460}, 1, 0, 2, 9002};
static const BuiltInAlertSpec kFallbackCaution = {{160, 180, 320, 460}, 1, 0, 3, 9003};
static const BuiltInAlertSpec kFallbackGeneric = {{160, 180, 320, 460}, 1, 2, 0, 9004};

/* Private function prototypes */
static SInt16 RunAlertDialog(SInt16 alertID, ModalFilterProcPtr filterProc, SInt16 alertType);
static DialogPtr CreateAlertDialogFromTemplate(const AlertTemplate* alertTemplate);
static void PlayAlertSoundForStage(SInt16 alertType, SInt16 stage);
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
        DM_ParamTextSlot(i)[0] = 0;
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

}

/*
 * Alert - Display a generic alert dialog
 */
SInt16 Alert(SInt16 alertID, ModalFilterProcPtr filterProc)
{
    return RunAlertDialog(alertID, filterProc, -1);   /* Alert has no icon */
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

    const unsigned char* p = DM_ParamTextSlot(paramIndex);
    memcpy(text, p, p[0] + 1);
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
        DM_ParamTextSlot(i)[0] = 0;
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
        return NULL;
    }

    /* Load alert template */
    err = LoadAlertTemplate(alertID, &alertTemplate);
    if (err != noErr || !alertTemplate) {
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
    (void)iconKind;
    if (!outDITL) {
        return -50; /* paramErr */
    }

    /*
     * OK, Cancel for the generic alert, the message, and the icon well.
     * The message is ^0, so ParamText supplies it; this said "Alert message
     * will appear here." whatever the caller had set. OK was item 3, so a
     * caller testing for item 1 - Special > Restart does - took OK for
     * Cancel.
     *
     * Built through DITLBuilder, which pads items whose data length is odd.
     */
    DITLBuilder b;
    if (!DITL_Begin(&b, 1024)) {
        return -108; /* memFullErr */
    }

    DITL_AddButtonPascal(&b, 96, 180, 116, 240, PSTR("OK"));
    if (pseudoId == 9004) {
        DITL_AddButtonPascal(&b, 96, 100, 116, 160, PSTR("Cancel"));
    }
    DITL_AddTextPascal(&b, 20, 60, 90, 270, PSTR("^0"));
    DITL_AddItem(&b, userItem, &(Rect){20, 20, 52, 52}, NULL);   /* icon well */

    Handle h = DITL_Finish(&b);
    if (!h) {
        return -108; /* memFullErr */
    }
    *outDITL = h;
    return noErr;
}

/* The alert's icon, drawn into the icon well (a user item) from the 32x32
 * bitmaps in system7_resources.h. The kind is kept in the dialog's refCon:
 * 1 stop, 2 note, 3 caution, 0 none. The well used to have no procedure and
 * drew as an empty square. */
static pascal void Alert_DrawIconWell(DialogPtr d, SInt16 itemNo)
{
    SInt16 type;
    Handle h;
    Rect r;
    GetDialogItem(d, itemNo, &type, &h, &r);
    const unsigned char* bits = Alert_IconBitmap((SInt16)GetWRefCon((WindowPtr)d));
    if (!bits) return;
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            if (bits[row * 4 + col / 8] & (0x80 >> (col % 8))) {
                Rect px = { (short)(r.top + row), (short)(r.left + col),
                            (short)(r.top + row + 1), (short)(r.left + col + 1) };
                PaintRect(&px);
            }
        }
    }
}

/* Whether the alert being built has an icon well as its last item */
static Boolean gAlertIconWell = false;

/* A user item for the icon where Inside Macintosh puts it, (10,20,42,52)
 * (Toolbox Essentials, 6-31), at the end of an application's item list:
 * its alerts were drawn with none, the DITL leaving the room empty. */
static void Alert_AppendIconWell(Handle items)
{
    Size size = GetHandleSize(items);
    if (size < 2) return;
    SetHandleSize(items, size + 14);
    if (MemError() != noErr) return;
    UInt8* p = (UInt8*)*items;
    UInt16 last = (UInt16)((p[0] << 8) | p[1]);
    last++;
    p[0] = (UInt8)(last >> 8);
    p[1] = (UInt8)last;
    static const UInt8 well[14] = { 0, 0, 0, 0,  0, 10, 0, 20, 0, 42, 0, 52,
                                    userItem | itemDisable, 0 };
    memcpy(p + size, well, sizeof well);
    gAlertIconWell = true;
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

    gAlertIconWell = false;

    /* The application's own ALRT and DITL, when it has them (Inside
     * Macintosh: Toolbox Essentials, 6-156). Every alert used to be built
     * from the fallback below whatever its resources said. */
    {
        AlertTemplate* alrt = NULL;
        Handle items = NULL;
        if (LoadAlertTemplate(alertID, &alrt) == noErr && alrt &&
            LoadDialogItemList(alrt->itemsID, &items) == noErr && items) {
            DialogTemplate* t = (DialogTemplate*)NewPtrClear(sizeof(DialogTemplate));
            if (t) {
                t->boundsRect = alrt->boundsRect;
                t->procID = 1;      /* dBoxProc */
                t->itemsID = alrt->itemsID;
                /* Stage 1's boldItm bit picks item 2 as the default */
                SInt16 def = (alrt->stages & 0x0008) ? 2 : 1;
                *outDefItem = def;
                *outCancelItem = (def == 1) ? 2 : 1;
                *outIconKind = (alertType < 0) ? 0 : alertType + 1;
                if (*outIconKind) Alert_AppendIconWell(items);
                *outDLOG = t;
                *outDITL = items;
                DisposeAlertTemplate(alrt);
                return true;
            }
            DisposeHandle(items);
        }
        if (alrt) DisposeAlertTemplate(alrt);
    }

    gAlertIconWell = true;

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
    *outIconKind   = (alertType < 0) ? 0 : alertType + 1;   /* the call, not the ID, says which */

    /* Build fallback DLOG and DITL */
    err = BuildFallbackDLOG(spec, outDLOG);
    if (err != noErr) {
        return false;
    }

    err = BuildFallbackDITL(spec->ditlId, spec->icon, outDITL);
    if (err != noErr) {
        if (*outDLOG) {
            DisposePtr((Ptr)*outDLOG);
            *outDLOG = NULL;
        }
        return false;
    }

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

        /* Classic Mac encoding: low 7 bits carry the base type */
        if (((itemType & 0x7F) == (ctrlItem + btnCtrl)) && itemHandle == NULL) {
            /* Use default button title (OK) - could be enhanced to parse from DITL */
            title[0] = 2;
            title[1] = 'O';
            title[2] = 'K';

            /* Create the standard push button control */
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

                /* Verify control was linked to window */
                (void)_GetFirstControl((WindowPtr)d);
            } else {
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
    ConstStr255Param alertTitle = PSTR("Alert");

    if (!gAlertState.initialized) {
        return 1;
    }

    /* Play alert sound based on current stage */
    PlayAlertSoundForStage(alertType, gAlertState.alertStage);

    /* Load alert with fallback */
    if (!LoadAlertWithFallback(alertID, alertType, &dlogTemplate, &ditlHandle,
                               &defItem, &cancelItem, &iconKind)) {
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
        if (dlogTemplate) DisposePtr((Ptr)dlogTemplate);
        if (ditlHandle) DisposeHandle(ditlHandle);
        return 1;
    }

    /* The icon well draws the alert's icon */
    if (gAlertIconWell && iconKind) {
        SInt16 well = CountDITL(alertDialog);
        SInt16 type;
        Handle h;
        Rect r;
        GetDialogItem(alertDialog, well, &type, &h, &r);
        if ((type & itemTypeMask) == userItem) {
            SetDialogItem(alertDialog, well, type, (Handle)Alert_DrawIconWell, &r);
        }
    }

    /* Set default and cancel items */
    if (defItem) SetDialogDefaultItem(alertDialog, defItem);
    if (cancelItem) SetDialogCancelItem(alertDialog, cancelItem);

    /* Center and show */
    CenterDialogOnScreen(alertDialog);
    ShowWindow((WindowPtr)alertDialog);

    /* Force initial update - in the alert, not whatever port is current,
     * which blanked that much of an application's window behind it */
    InvalWindowRect((WindowPtr)alertDialog, &((GrafPtr)alertDialog)->portRect);

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
        if (DM_IsControlOf((WindowPtr)alertDialog, itemHandle)) {
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

    return itemHit;
}

static DialogPtr CreateAlertDialogFromTemplate(const AlertTemplate* alertTemplate)
{
    DialogPtr alertDialog = NULL;
    Handle itemList = NULL;
    OSErr err;
    ConstStr255Param alertTitle = PSTR("Alert");

    if (!alertTemplate) {
        return NULL;
    }

    /* Load item list for the alert */
    err = LoadDialogItemList(alertTemplate->itemsID, &itemList);
    if (err != noErr || !itemList) {
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
    (void)stage;
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
                const unsigned char* param = DM_ParamTextSlot(paramIndex);
                unsigned char paramLen = param[0];

                /* Copy parameter text */
                for (unsigned char j = 0; j < paramLen && resultLen < 255; j++) {
                    result[resultLen + 1] = param[j + 1];
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

/* Stub implementations for additional alert functions */

void SetAlertAccessibility(Boolean enabled)
{
    (void)enabled;
}

void AnnounceAlert(const char* title, const char* message)
{
    (void)title;
    (void)message;
}

SInt16 ShowNativeAlert(const char* title, const char* message,
                       SInt16 buttons, SInt16 alertType)
{
    (void)title;
    (void)message;
    (void)buttons;
    (void)alertType;
    return 1; /* OK button */
}

void SetAlertTheme(const DialogTheme* theme)
{
    (void)theme;
}

void GetAlertTheme(DialogTheme* theme)
{
    (void)theme;
}

SInt16 ShowAlert(const char* title, const char* message,
                 SInt16 buttons, SInt16 alertType)
{
    (void)title;
    (void)message;
    (void)buttons;
    (void)alertType;
    return 1; /* OK button */
}

SInt16 ShowAlertWithParams(const char* title, const char* message,
                           SInt16 buttons, SInt16 alertType,
                           const char* param0, const char* param1,
                           const char* param2, const char* param3)
{
    (void)title;
    (void)message;
    (void)buttons;
    (void)alertType;
    (void)param0;
    (void)param1;
    (void)param2;
    (void)param3;
    return 1; /* OK button */
}

void ProcessAlertStages(SInt16 alertType, SInt16 stage)
{
    (void)alertType;
    (void)stage;
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
                const unsigned char* param = DM_ParamTextSlot(paramIndex);
                unsigned char paramLen = param[0];

                /* Convert Pascal string to C string and copy */
                for (unsigned char i = 0; i < paramLen && dstIdx < sizeof(result) - 1; i++) {
                    result[dstIdx++] = param[i + 1];
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
